"""
Command Executor Service.
Orchestrates the full command execution pipeline:
1. Receive user input (transcribed text)
2. Parse intent with LLM
3. Save to database
4. Execute on device (virtual or physical)
5. Generate TTS response
6. Emit real-time events
"""
import logging
from collections.abc import Awaitable, Callable
from typing import Optional, Dict, Any

from sqlalchemy.ext.asyncio import AsyncSession

from repositories.command_repository import CommandRepository
from services.intent_parser import intent_parser
from services.iflytek_tts import tts_client
from services.device_controller import device_controller

logger = logging.getLogger(__name__)

EventCallback = Callable[[str, Dict[str, Any]], Awaitable[None] | None]


class CommandExecutorService:
    """
    Orchestrates the end-to-end command execution pipeline.
    """

    def __init__(self):
        logger.info("Command Executor Service initialized")

    async def execute_command(
        self,
        user_input_text: str,
        session: AsyncSession,
        on_event: Optional[EventCallback] = None,
    ) -> Dict[str, Any]:
        """
        Execute a complete command pipeline from user input to device control.

        Pipeline Steps:
            1. Parse intent with LLM (user_input → control_json)
            2. Save command to database (status=pending)
            3. Execute command on device (virtual or physical)
            4. Generate TTS audio response
            5. Update command status (executed/failed)
            6. Emit events for real-time frontend updates

        Args:
            user_input_text: User's voice command (transcribed)
            session: Database session
            on_event: Optional callback for emitting real-time events
                      Signature: (event_type: str, data: Dict[str, Any]) -> None

        Returns:
            Dictionary containing:
                - command_id: UUID
                - control_json: Dict
                - response_text: str
                - tts_audio_base64: Optional[str]
                - status: str (executed/failed)
                - error: Optional[str]

        Raises:
            ValueError: If LLM parsing fails
        """
        command_id = None
        repository = CommandRepository(session)

        try:
            # ========== Step 1: Parse Intent with LLM ==========
            logger.info(f"[Step 1/5] Parsing user input with LLM: '{user_input_text}'")
            await self._emit_event(on_event, "llm_processing", {"text": user_input_text})

            parse_result = await intent_parser.parse_command(user_input_text)

            if not parse_result:
                raise ValueError("LLM returned empty response")

            control_json = parse_result["control_json"]
            response_text = parse_result["response_text"]

            logger.info(f"[Step 1/5] ✓ LLM parsing successful: {control_json}")
            await self._emit_event(
                on_event,
                "command_generated",
                {
                    "control_json": control_json,
                    "response_text": response_text,
                },
            )

            # ========== Step 2: Save to Database ==========
            logger.info("[Step 2/5] Saving command to database")

            command = await repository.create_command(
                user_input_text=user_input_text,
                llm_response_text=response_text,
                control_json=control_json,
            )
            command_id = command.id

            logger.info(f"[Step 2/5] ✓ Command saved: ID={command_id}")
            await self._emit_event(on_event, "command_saved", {"command_id": str(command_id)})

            # ========== Step 3: Execute on Device ==========
            logger.info("[Step 3/5] Executing command on device")
            await self._emit_event(on_event, "device_executing", {"command_id": str(command_id)})

            device_success = await device_controller.execute_command(control_json)

            if not device_success:
                raise RuntimeError("Device command execution failed")

            logger.info("[Step 3/5] ✓ Device command executed successfully")
            await self._emit_event(on_event, "device_executed", {"command_id": str(command_id)})

            # ========== Step 4: Generate TTS Audio ==========
            logger.info("[Step 4/5] Generating TTS audio response")
            await self._emit_event(on_event, "tts_generating", {"text": response_text})

            tts_audio = await tts_client.synthesize(
                text=response_text,
                vcn="xiaoyan",  # Default voice
                speed=50,
                volume=50,
            )

            # Convert to base64 for JSON transport
            import base64
            tts_audio_base64 = None
            if tts_audio:
                tts_audio_base64 = base64.b64encode(tts_audio).decode("utf-8")
                logger.info(f"[Step 4/5] ✓ TTS audio generated: {len(tts_audio)} bytes")
                await self._emit_event(
                    on_event,
                    "tts_ready",
                    {
                        "audio_base64": tts_audio_base64,
                        "size_bytes": len(tts_audio),
                    },
                )
            else:
                logger.warning("[Step 4/5] ⚠ TTS generation returned empty audio")

            # ========== Step 5: Update Command Status ==========
            logger.info("[Step 5/5] Updating command status to 'executed'")

            await repository.update_status(command_id, "executed")

            logger.info("[Step 5/5] ✓ Command execution pipeline complete")
            await self._emit_event(
                on_event,
                "execution_complete",
                {
                    "command_id": str(command_id),
                    "status": "executed",
                },
            )

            # Return full result
            return {
                "command_id": str(command_id),
                "control_json": control_json,
                "response_text": response_text,
                "tts_audio_base64": tts_audio_base64,
                "status": "executed",
                "error": None,
            }

        except Exception as e:
            logger.error(f"Command execution failed: {e}")

            # Update command status if it was created
            if command_id:
                try:
                    await repository.update_status(
                        command_id,
                        "failed",
                        execution_error=str(e),
                    )
                except Exception as db_error:
                    logger.error(f"Failed to update command status: {db_error}")

            # Emit error event
            await self._emit_event(
                on_event,
                "execution_error",
                {
                    "command_id": str(command_id) if command_id else None,
                    "error": str(e),
                },
            )

            # Re-raise for caller to handle
            raise

    async def get_latest_device_status(self) -> Dict[str, Any]:
        """
        Get current device status.

        Returns:
            Device status dictionary
        """
        return await device_controller.get_device_status()

    async def stop_device(self) -> bool:
        """
        Stop the aromatherapy device.

        Returns:
            True if stopped successfully
        """
        logger.info("Stopping device via command executor")
        return await device_controller.stop()

    async def _emit_event(
        self,
        on_event: Optional[EventCallback],
        event_type: str,
        data: Dict[str, Any],
    ) -> None:
        """
        Emit event if callback is provided.

        Args:
            on_event: Event callback function
            event_type: Event type name
            data: Event data
        """
        if on_event:
            try:
                callback_result = on_event(event_type, data)
                if isinstance(callback_result, Awaitable):
                    await callback_result
            except Exception as e:
                logger.error(f"Error emitting event '{event_type}': {e}")


# Global command executor instance
command_executor = CommandExecutorService()

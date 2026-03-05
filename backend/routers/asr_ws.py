"""
ASR WebSocket Router.
Provides WebSocket endpoint for real-time audio transcription.
"""
import asyncio
import logging
from typing import Optional
from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from fastapi.websockets import WebSocketState

from services.iflytek_asr import asr_client

logger = logging.getLogger(__name__)

router = APIRouter()


@router.websocket("/ws/asr")
async def asr_websocket(websocket: WebSocket):
    """
    WebSocket endpoint for real-time ASR (Automatic Speech Recognition).

    Protocol:
        1. Client connects to /ws/asr
        2. Client sends binary audio frames (PCM 16kHz 16bit mono)
        3. Server forwards to iFlytek ASR via WebSocket
        4. Server streams back transcription events:
           - {"type": "partial", "text": "..."} - Partial transcription
           - {"type": "final", "text": "..."} - Final transcription
           - {"type": "error", "message": "..."} - Error occurred

    Audio Format:
        - Sample Rate: 16kHz
        - Bit Depth: 16bit
        - Channels: Mono
        - Encoding: PCM (raw audio)
    """
    await websocket.accept()
    logger.info("ASR WebSocket client connected")

    audio_queue: asyncio.Queue = asyncio.Queue()
    transcription_complete = asyncio.Event()
    final_transcript = []

    async def audio_stream_generator():
        """Generator that yields audio chunks from the queue."""
        while True:
            chunk = await audio_queue.get()
            if chunk is None:  # Sentinel value to end stream
                break
            yield chunk

    async def receive_audio():
        """Receive audio from client and queue it."""
        try:
            while True:
                # Receive binary audio data from client
                data = await websocket.receive()

                if "bytes" in data:
                    audio_chunk = data["bytes"]
                    await audio_queue.put(audio_chunk)
                    logger.debug(f"Received audio chunk: {len(audio_chunk)} bytes")

                elif "text" in data:
                    # Handle control messages
                    import json
                    message = json.loads(data["text"])

                    if message.get("type") == "end":
                        # Client signals end of audio
                        logger.info("Client signaled end of audio")
                        await audio_queue.put(None)  # Sentinel value
                        break

        except WebSocketDisconnect:
            logger.info("Client disconnected during audio reception")
            await audio_queue.put(None)  # Signal end
        except Exception as e:
            logger.error(f"Error receiving audio: {e}")
            await audio_queue.put(None)

    def on_partial_result(text: str):
        """Callback for partial transcription results."""
        logger.info(f"Partial transcription: {text}")
        asyncio.create_task(send_result("partial", text))

    def on_final_result(text: str):
        """Callback for final transcription result."""
        logger.info(f"Final transcription: {text}")
        final_transcript.append(text)
        asyncio.create_task(send_result("final", text))
        transcription_complete.set()

    async def send_result(result_type: str, text: str):
        """Send transcription result to client."""
        try:
            if websocket.client_state == WebSocketState.CONNECTED:
                await websocket.send_json({
                    "type": result_type,
                    "text": text,
                })
        except Exception as e:
            logger.error(f"Error sending result: {e}")

    try:
        # Start receiving audio task
        receive_task = asyncio.create_task(receive_audio())

        # Start ASR transcription
        try:
            transcript = await asr_client.transcribe_stream(
                audio_stream=audio_stream_generator(),
                on_partial=on_partial_result,
                on_final=on_final_result,
            )

            # Send final result if not already sent
            if not final_transcript:
                await send_result("final", transcript)

        except ConnectionError as e:
            logger.error(f"ASR connection error: {e}")
            await send_result("error", f"ASR service connection failed: {e}")

        except ValueError as e:
            logger.error(f"ASR transcription error: {e}")
            await send_result("error", f"Transcription failed: {e}")

        except Exception as e:
            logger.error(f"Unexpected ASR error: {e}")
            await send_result("error", f"Transcription error: {e}")

        # Wait for receive task to complete
        await receive_task

    except WebSocketDisconnect:
        logger.info("ASR WebSocket client disconnected")

    except Exception as e:
        logger.error(f"ASR WebSocket error: {e}")
        try:
            if websocket.client_state == WebSocketState.CONNECTED:
                await send_result("error", str(e))
        except:
            pass

    finally:
        # Cleanup
        if websocket.client_state == WebSocketState.CONNECTED:
            await websocket.close()
        logger.info("ASR WebSocket connection closed")

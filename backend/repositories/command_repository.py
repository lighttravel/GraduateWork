"""
Repository pattern for aromatherapy commands.
Encapsulates all database operations for the AromatherapyCommand model.
"""
from typing import List, Optional
from uuid import UUID
from sqlalchemy import select, desc
from sqlalchemy.ext.asyncio import AsyncSession
from models import AromatherapyCommand, ControlJson


class CommandRepository:
    """
    Data access layer for aromatherapy commands.
    Implements Repository Pattern for consistent data operations.
    """

    def __init__(self, session: AsyncSession):
        self.session = session

    async def create_command(
        self,
        user_input_text: str,
        control_json: dict,
        llm_response_text: Optional[str] = None,
    ) -> AromatherapyCommand:
        """
        Create a new aromatherapy command.

        Args:
            user_input_text: User's voice input (transcribed)
            control_json: Structured control parameters
            llm_response_text: Optional LLM natural language response

        Returns:
            Created command object
        """
        command = AromatherapyCommand(
            user_input_text=user_input_text,
            llm_response_text=llm_response_text,
            control_json=control_json,
            status="pending",
        )
        self.session.add(command)
        await self.session.commit()
        await self.session.refresh(command)
        return command

    async def get_command(self, command_id: UUID) -> Optional[AromatherapyCommand]:
        """
        Get a command by ID.

        Args:
            command_id: UUID of the command

        Returns:
            Command object or None if not found
        """
        stmt = select(AromatherapyCommand).where(AromatherapyCommand.id == command_id)
        result = await self.session.execute(stmt)
        return result.scalar_one_or_none()

    async def list_commands(
        self,
        limit: int = 100,
        offset: int = 0,
        status: Optional[str] = None,
    ) -> List[AromatherapyCommand]:
        """
        List commands with optional filtering and pagination.

        Args:
            limit: Maximum number of commands to return
            offset: Number of commands to skip
            status: Optional filter by status (pending/executed/failed)

        Returns:
            List of commands
        """
        stmt = select(AromatherapyCommand).order_by(desc(AromatherapyCommand.created_at))

        if status:
            stmt = stmt.where(AromatherapyCommand.status == status)

        stmt = stmt.limit(limit).offset(offset)
        result = await self.session.execute(stmt)
        return list(result.scalars().all())

    async def update_status(
        self,
        command_id: UUID,
        status: str,
        execution_error: Optional[str] = None,
    ) -> Optional[AromatherapyCommand]:
        """
        Update command execution status.

        Args:
            command_id: UUID of the command
            status: New status (pending/executed/failed)
            execution_error: Optional error message if failed

        Returns:
            Updated command object or None if not found
        """
        command = await self.get_command(command_id)
        if not command:
            return None

        command.status = status
        if execution_error:
            command.execution_error = execution_error

        await self.session.commit()
        await self.session.refresh(command)
        return command

    async def update_tts_url(
        self,
        command_id: UUID,
        tts_audio_url: str,
    ) -> Optional[AromatherapyCommand]:
        """
        Update TTS audio URL for a command.

        Args:
            command_id: UUID of the command
            tts_audio_url: URL to TTS audio file

        Returns:
            Updated command object or None if not found
        """
        command = await self.get_command(command_id)
        if not command:
            return None

        command.tts_audio_url = tts_audio_url
        await self.session.commit()
        await self.session.refresh(command)
        return command

    async def update_feedback(
        self,
        command_id: UUID,
        user_feedback: int,
    ) -> Optional[AromatherapyCommand]:
        """
        Update user feedback rating for a command.

        Args:
            command_id: UUID of the command
            user_feedback: Rating 1-5 stars

        Returns:
            Updated command object or None if not found
        """
        if not (1 <= user_feedback <= 5):
            raise ValueError("Feedback must be between 1 and 5")

        command = await self.get_command(command_id)
        if not command:
            return None

        command.user_feedback = user_feedback
        await self.session.commit()
        await self.session.refresh(command)
        return command

    async def get_latest_command(self) -> Optional[AromatherapyCommand]:
        """
        Get the most recently created command.
        Useful for retrieving current device state.

        Returns:
            Latest command or None if no commands exist
        """
        stmt = (
            select(AromatherapyCommand)
            .order_by(desc(AromatherapyCommand.created_at))
            .limit(1)
        )
        result = await self.session.execute(stmt)
        return result.scalar_one_or_none()

    async def count_commands(self, status: Optional[str] = None) -> int:
        """
        Count total commands, optionally filtered by status.

        Args:
            status: Optional filter by status

        Returns:
            Total count
        """
        from sqlalchemy import func

        stmt = select(func.count(AromatherapyCommand.id))
        if status:
            stmt = stmt.where(AromatherapyCommand.status == status)

        result = await self.session.execute(stmt)
        return result.scalar_one()

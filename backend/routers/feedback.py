"""
Feedback Router.
Provides endpoint to update user rating for an executed command.
"""
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession

from database import get_db_session
from models import FeedbackUpdate
from repositories.command_repository import CommandRepository

router = APIRouter()


@router.patch("/commands/{command_id}/feedback")
async def update_command_feedback(
    command_id: UUID,
    payload: FeedbackUpdate,
    session: AsyncSession = Depends(get_db_session),
):
    """
    Update user feedback (1-5) for a command.

    Returns:
        Updated feedback payload for UI confirmation.
    """
    repository = CommandRepository(session)

    try:
        updated = await repository.update_feedback(
            command_id=command_id,
            user_feedback=payload.user_feedback,
        )
    except ValueError as exc:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=str(exc),
        ) from exc

    if not updated:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Command not found: {command_id}",
        )

    return {
        "command_id": str(updated.id),
        "user_feedback": updated.user_feedback,
        "updated_at": updated.updated_at.isoformat(),
    }

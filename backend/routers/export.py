"""
Export Router.
Provides training-data export endpoint in CSV or JSONL format.
"""
import csv
import io
import json
from datetime import datetime
from typing import Literal

from fastapi import APIRouter, Depends, Query, Response
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from database import get_db_session
from models import AromatherapyCommand

router = APIRouter()


def _build_export_rows(commands: list[AromatherapyCommand]) -> list[dict]:
    rows: list[dict] = []
    for command in commands:
        rows.append(
            {
                "command_id": str(command.id),
                "user_input_text": command.user_input_text,
                "control_json": command.control_json,
                "user_feedback": command.user_feedback,
                "created_at": command.created_at.isoformat() if command.created_at else None,
            }
        )
    return rows


def _rows_to_csv(rows: list[dict]) -> str:
    fieldnames = ["command_id", "user_input_text", "control_json", "user_feedback", "created_at"]
    stream = io.StringIO()
    writer = csv.DictWriter(stream, fieldnames=fieldnames)
    writer.writeheader()
    for row in rows:
        writer.writerow(
            {
                **row,
                "control_json": json.dumps(row["control_json"], ensure_ascii=False),
            }
        )
    return stream.getvalue()


def _rows_to_jsonl(rows: list[dict]) -> str:
    lines = [json.dumps(row, ensure_ascii=False) for row in rows]
    return "\n".join(lines)


@router.get("/export/training-data")
async def export_training_data(
    format: Literal["csv", "jsonl"] = Query(default="csv"),
    session: AsyncSession = Depends(get_db_session),
):
    """
    Export commands that contain user feedback for model fine-tuning.
    """
    stmt = (
        select(AromatherapyCommand)
        .where(AromatherapyCommand.user_feedback.is_not(None))
        .order_by(AromatherapyCommand.created_at.desc())
    )
    result = await session.execute(stmt)
    commands = list(result.scalars().all())
    rows = _build_export_rows(commands)

    timestamp = datetime.utcnow().strftime("%Y%m%d_%H%M%S")

    if format == "jsonl":
        payload = _rows_to_jsonl(rows)
        filename = f"training_data_{timestamp}.jsonl"
        media_type = "application/x-ndjson"
    else:
        payload = _rows_to_csv(rows)
        filename = f"training_data_{timestamp}.csv"
        media_type = "text/csv; charset=utf-8"

    return Response(
        content=payload,
        media_type=media_type,
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
    )

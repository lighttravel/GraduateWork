"""
Global exception handlers for FastAPI.
Provides a consistent JSON error response format across the API.
"""
import asyncio
import logging
from typing import Any

from fastapi import FastAPI, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse
from pydantic import ValidationError
from sqlalchemy.exc import SQLAlchemyError

logger = logging.getLogger(__name__)


def _error_payload(error: str, detail: Any) -> dict[str, Any]:
    return {
        "error": error,
        "detail": detail,
    }


def register_error_handlers(app: FastAPI) -> None:
    """
    Register global error handlers on the FastAPI app.
    """

    @app.exception_handler(RequestValidationError)
    async def request_validation_handler(
        request: Request,  # noqa: ARG001
        exc: RequestValidationError,
    ) -> JSONResponse:
        logger.warning("Request validation error: %s", exc)
        return JSONResponse(
            status_code=400,
            content=_error_payload("validation_error", exc.errors()),
        )

    @app.exception_handler(ValidationError)
    async def pydantic_validation_handler(
        request: Request,  # noqa: ARG001
        exc: ValidationError,
    ) -> JSONResponse:
        logger.warning("Pydantic validation error: %s", exc)
        return JSONResponse(
            status_code=400,
            content=_error_payload("validation_error", exc.errors()),
        )

    @app.exception_handler(SQLAlchemyError)
    async def database_error_handler(
        request: Request,  # noqa: ARG001
        exc: SQLAlchemyError,
    ) -> JSONResponse:
        logger.exception("Database error: %s", exc)
        return JSONResponse(
            status_code=500,
            content=_error_payload("database_error", "Database operation failed."),
        )

    @app.exception_handler(asyncio.TimeoutError)
    async def timeout_error_handler(
        request: Request,  # noqa: ARG001
        exc: asyncio.TimeoutError,
    ) -> JSONResponse:
        logger.error("Timeout error: %s", exc)
        return JSONResponse(
            status_code=504,
            content=_error_payload("timeout_error", "Request timed out."),
        )

    @app.exception_handler(Exception)
    async def unexpected_error_handler(
        request: Request,  # noqa: ARG001
        exc: Exception,
    ) -> JSONResponse:
        logger.exception("Unhandled application error: %s", exc)
        return JSONResponse(
            status_code=500,
            content=_error_payload("internal_server_error", "Unexpected server error."),
        )

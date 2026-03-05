"""
FastAPI Application Entry Point
AI-Powered Voice-Controlled Aromatherapy System Backend
"""
from fastapi import FastAPI, Depends
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from contextlib import asynccontextmanager
import logging
import sys

from config import settings, validate_settings
from database import init_db, close_db

# Configure logging
logging.basicConfig(
    level=getattr(logging, settings.log_level.upper()),
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],
)
logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """
    Application lifespan manager.
    Handles startup and shutdown events.
    """
    # Startup
    logger.info("Starting aromatherapy system backend...")
    try:
        validate_settings()
        logger.info("Configuration validated successfully")
        await init_db()
        logger.info("Database initialized successfully")
    except Exception as e:
        logger.error(f"Startup failed: {e}")
        raise

    yield

    # Shutdown
    logger.info("Shutting down aromatherapy system backend...")
    await close_db()


# Create FastAPI app
app = FastAPI(
    title="AI Aromatherapy Control System",
    description="Voice-controlled aromatherapy system with LLM-powered intent parsing",
    version="1.0.0",
    lifespan=lifespan,
)

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# Health check endpoint
@app.get("/health", tags=["Health"])
async def health_check():
    """
    Health check endpoint.
    Returns service status and configuration info.
    """
    return JSONResponse(
        status_code=200,
        content={
            "status": "healthy",
            "environment": settings.app_env,
            "services": {
                "database": "connected",
                "iflytek_asr": "configured",
                "iflytek_tts": "configured",
                "llm": "configured",
            },
        },
    )


@app.get("/", tags=["Root"])
async def root():
    """Root endpoint with API information."""
    return {
        "message": "AI-Powered Aromatherapy Control System API",
        "version": "1.0.0",
        "docs": "/docs",
        "health": "/health",
    }


# Import and register routers
from routers import asr_ws, tts, command_ws

app.include_router(asr_ws.router, prefix="/api", tags=["ASR"])
app.include_router(tts.router, prefix="/api", tags=["TTS"])
app.include_router(command_ws.router, prefix="/api", tags=["Commands"])

# Routers to be added in later phases:
# from routers import feedback, export
# app.include_router(feedback.router, prefix="/api", tags=["Feedback"])
# app.include_router(export.router, prefix="/api", tags=["Export"])


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "main:app",
        host=settings.app_host,
        port=settings.app_port,
        reload=settings.app_env == "development",
    )

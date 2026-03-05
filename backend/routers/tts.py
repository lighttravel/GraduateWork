"""
TTS (Text-to-Speech) Router.
Provides HTTP endpoint for text-to-speech audio generation.
"""
import base64
import logging
from typing import Optional
from fastapi import APIRouter, HTTPException
from fastapi.responses import Response
from pydantic import BaseModel, Field

from services.iflytek_tts import tts_client

logger = logging.getLogger(__name__)

router = APIRouter()


class TTSRequest(BaseModel):
    """Request model for TTS synthesis."""
    text: str = Field(..., min_length=1, max_length=8000, description="Text to synthesize")
    voice: str = Field(default="xiaoyan", description="Voice name (xiaoyan=female, aisjiuxu=male)")
    speed: int = Field(default=50, ge=0, le=100, description="Speech speed (0-100)")
    volume: int = Field(default=50, ge=0, le=100, description="Volume (0-100)")
    return_base64: bool = Field(default=False, description="Return base64-encoded audio instead of binary")


class TTSResponse(BaseModel):
    """Response model for TTS synthesis (base64 mode)."""
    audio_base64: str = Field(..., description="Base64-encoded MP3 audio")
    size_bytes: int = Field(..., description="Audio size in bytes")


@router.post("/tts", response_class=Response)
async def synthesize_speech(request: TTSRequest):
    """
    Generate speech audio from text using iFlytek TTS.

    Args:
        request: TTS request with text and voice parameters

    Returns:
        - If return_base64=False (default): Binary MP3 audio (application/mpeg)
        - If return_base64=True: JSON with base64-encoded audio

    Raises:
        HTTPException 400: Invalid input
        HTTPException 500: TTS synthesis failed
        HTTPException 503: TTS service unavailable

    Example:
        ```
        POST /api/tts
        {
            "text": "设置柠檬香型，中等强度，持续30分钟",
            "voice": "xiaoyan",
            "speed": 50,
            "volume": 50
        }
        ```
    """
    try:
        logger.info(f"TTS request: text='{request.text[:50]}...', voice={request.voice}")

        # Synthesize audio
        audio_data = await tts_client.synthesize(
            text=request.text,
            vcn=request.voice,
            speed=request.speed,
            volume=request.volume,
        )

        if not audio_data:
            raise HTTPException(
                status_code=500,
                detail="TTS synthesis returned empty audio"
            )

        # Return based on format preference
        if request.return_base64:
            # Return as JSON with base64-encoded audio
            return TTSResponse(
                audio_base64=base64.b64encode(audio_data).decode("utf-8"),
                size_bytes=len(audio_data),
            )
        else:
            # Return binary MP3 audio
            return Response(
                content=audio_data,
                media_type="audio/mpeg",
                headers={
                    "Content-Length": str(len(audio_data)),
                    "Content-Disposition": "inline; filename=speech.mp3",
                },
            )

    except ValueError as e:
        logger.error(f"TTS validation error: {e}")
        raise HTTPException(status_code=400, detail=str(e))

    except ConnectionError as e:
        logger.error(f"TTS connection error: {e}")
        raise HTTPException(
            status_code=503,
            detail="TTS service temporarily unavailable. Please try again later."
        )

    except Exception as e:
        logger.error(f"TTS synthesis error: {e}")
        raise HTTPException(
            status_code=500,
            detail=f"TTS synthesis failed: {str(e)}"
        )


@router.get("/tts/voices")
async def list_voices():
    """
    List available TTS voices.

    Returns:
        Dictionary of available voices with descriptions
    """
    return {
        "voices": [
            {
                "name": "xiaoyan",
                "language": "zh_CN",
                "gender": "female",
                "description": "Standard female voice (Mandarin)",
            },
            {
                "name": "aisjiuxu",
                "language": "zh_CN",
                "gender": "male",
                "description": "Standard male voice (Mandarin)",
            },
            {
                "name": "aisxping",
                "language": "zh_CN",
                "gender": "female",
                "description": "Affectionate female voice (Mandarin)",
            },
            {
                "name": "aisjinger",
                "language": "zh_CN",
                "gender": "female",
                "description": "Gentle female voice (Mandarin)",
            },
            {
                "name": "aisbabyxu",
                "language": "zh_CN",
                "gender": "male",
                "description": "Child voice (Mandarin)",
            },
        ],
        "default": "xiaoyan",
    }

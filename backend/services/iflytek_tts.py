"""
iFlytek TTS (Text-to-Speech) WebSocket Client.
Converts text to natural-sounding speech using iFlytek WebSocket API.
"""
import asyncio
import base64
import hashlib
import hmac
import json
import logging
from datetime import datetime
from io import BytesIO
from time import mktime
from typing import Optional
from urllib.parse import urlencode
from wsgiref.handlers import format_date_time

import websockets
from websockets.client import WebSocketClientProtocol

from config import settings

logger = logging.getLogger(__name__)


class IFlytekTTSClient:
    """
    WebSocket client for iFlytek TTS service.
    Handles authentication, text submission, and audio synthesis.
    """

    # iFlytek TTS WebSocket URL
    TTS_HOST = "tts-api.xfyun.cn"
    TTS_PATH = "/v2/tts"
    TTS_URL = f"wss://{TTS_HOST}{TTS_PATH}"

    def __init__(self):
        """Initialize iFlytek TTS client with credentials."""
        self.appid = settings.iflytek_tts_appid
        self.api_key = settings.iflytek_tts_api_key
        self.api_secret = settings.iflytek_tts_api_secret
        self.ws: Optional[WebSocketClientProtocol] = None
        logger.info("iFlytek TTS Client initialized")

    def _generate_auth_url(self) -> str:
        """
        Generate authenticated WebSocket URL using HMAC-SHA256 signature.

        Returns:
            Authenticated WebSocket URL with signature
        """
        # Generate RFC1123 format timestamp
        now = datetime.now()
        date = format_date_time(mktime(now.timetuple()))

        # Build signature string
        signature_origin = f"host: {self.TTS_HOST}\n"
        signature_origin += f"date: {date}\n"
        signature_origin += f"GET {self.TTS_PATH} HTTP/1.1"

        # Calculate HMAC-SHA256 signature
        signature_sha = hmac.new(
            self.api_secret.encode("utf-8"),
            signature_origin.encode("utf-8"),
            digestmod=hashlib.sha256,
        ).digest()

        # Base64 encode signature
        signature_sha_base64 = base64.b64encode(signature_sha).decode("utf-8")

        # Build authorization header
        authorization_origin = (
            f'api_key="{self.api_key}", '
            f'algorithm="hmac-sha256", '
            f'headers="host date request-line", '
            f'signature="{signature_sha_base64}"'
        )
        authorization = base64.b64encode(authorization_origin.encode("utf-8")).decode("utf-8")

        # Build query parameters
        params = {
            "authorization": authorization,
            "date": date,
            "host": self.TTS_HOST,
        }

        # Construct final URL
        auth_url = f"{self.TTS_URL}?{urlencode(params)}"
        logger.debug("Generated authenticated TTS WebSocket URL")
        return auth_url

    def _build_request(self, text: str, vcn: str = "xiaoyan", speed: int = 50, volume: int = 50) -> str:
        """
        Build TTS request frame.

        Args:
            text: Text to synthesize
            vcn: Voice name (xiaoyan=female, aisjiuxu=male, etc.)
            speed: Speech speed (0-100, default 50)
            volume: Volume (0-100, default 50)

        Returns:
            JSON string containing TTS request
        """
        frame = {
            "common": {"app_id": self.appid},
            "business": {
                "aue": "lame",  # Audio encoding: MP3
                "sfl": 1,  # Audio quality (1=16kHz)
                "auf": "audio/L16;rate=16000",  # Audio format
                "vcn": vcn,  # Voice name
                "speed": speed,  # Speech speed
                "volume": volume,  # Volume
                "pitch": 50,  # Pitch (0-100, default 50)
                "bgs": 0,  # Background sound (0=none)
                "tte": "UTF8",  # Text encoding
            },
            "data": {
                "status": 2,  # 2 = complete text in one frame
                "text": base64.b64encode(text.encode("utf-8")).decode("utf-8"),
            },
        }
        return json.dumps(frame)

    async def synthesize(
        self,
        text: str,
        vcn: str = "xiaoyan",
        speed: int = 50,
        volume: int = 50,
    ) -> Optional[bytes]:
        """
        Synthesize text to speech audio.

        Args:
            text: Text to convert to speech (max ~8000 Chinese characters)
            vcn: Voice name (xiaoyan, aisjiuxu, etc.)
            speed: Speech speed (0-100)
            volume: Volume (0-100)

        Returns:
            MP3 audio bytes, or None if synthesis fails

        Raises:
            ConnectionError: If WebSocket connection fails
            ValueError: If TTS synthesis fails
        """
        if not text:
            raise ValueError("Text cannot be empty")

        if len(text) > 8000:
            logger.warning(f"Text length ({len(text)}) exceeds recommended limit (8000)")

        auth_url = self._generate_auth_url()
        audio_chunks = []

        try:
            async with websockets.connect(auth_url) as ws:
                self.ws = ws
                logger.info("TTS WebSocket connection established")

                # Send TTS request
                request = self._build_request(text, vcn, speed, volume)
                await ws.send(request)
                logger.info(f"Sent TTS request: '{text[:50]}...' (voice={vcn})")

                # Receive audio chunks
                async for message in ws:
                    try:
                        result = json.loads(message)
                        code = result.get("code")

                        if code != 0:
                            error_msg = result.get("message", "Unknown error")
                            logger.error(f"iFlytek TTS error {code}: {error_msg}")
                            raise ValueError(f"TTS error: {error_msg}")

                        # Extract audio data
                        data = result.get("data", {})
                        audio_b64 = data.get("audio")
                        status = data.get("status", 0)

                        if audio_b64:
                            # Decode base64 audio
                            audio_bytes = base64.b64decode(audio_b64)
                            audio_chunks.append(audio_bytes)
                            logger.debug(f"Received audio chunk: {len(audio_bytes)} bytes")

                        # Check if synthesis is complete
                        if status == 2:
                            logger.info("TTS synthesis complete")
                            break

                    except json.JSONDecodeError as e:
                        logger.error(f"Invalid JSON from TTS: {e}")
                        continue

        except websockets.exceptions.WebSocketException as e:
            logger.error(f"TTS WebSocket error: {e}")
            raise ConnectionError(f"iFlytek TTS connection failed: {e}")
        except Exception as e:
            logger.error(f"TTS synthesis error: {e}")
            raise
        finally:
            self.ws = None

        # Combine audio chunks
        if not audio_chunks:
            logger.warning("No audio data received from TTS")
            return None

        audio_data = b"".join(audio_chunks)
        logger.info(f"TTS synthesis successful: {len(audio_data)} bytes")
        return audio_data


# Global TTS client instance
tts_client = IFlytekTTSClient()

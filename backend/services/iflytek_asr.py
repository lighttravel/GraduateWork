"""
iFlytek ASR (Automatic Speech Recognition) WebSocket Client.
Handles real-time audio transcription using iFlytek WebSocket API.
"""
import asyncio
import base64
import hashlib
import hmac
import json
import logging
from datetime import datetime
from time import mktime
from typing import Optional, Callable, AsyncIterator
from urllib.parse import urlencode, urlparse
from wsgiref.handlers import format_date_time

import websockets
from websockets.client import WebSocketClientProtocol

from config import settings

logger = logging.getLogger(__name__)


class IFlytekASRClient:
    """
    WebSocket client for iFlytek ASR service.
    Handles authentication, audio streaming, and transcription result parsing.
    """

    # iFlytek ASR WebSocket URL
    ASR_HOST = "ws-api.xfyun.cn"
    ASR_PATH = "/v2/iat"
    ASR_URL = f"wss://{ASR_HOST}{ASR_PATH}"

    # Audio parameters
    AUDIO_FORMAT = "audio/L16;rate=16000"  # 16kHz PCM
    ENCODING = "raw"

    def __init__(self):
        """Initialize iFlytek ASR client with credentials."""
        self.appid = settings.iflytek_asr_appid
        self.api_key = settings.iflytek_asr_api_key
        self.api_secret = settings.iflytek_asr_api_secret
        self.ws: Optional[WebSocketClientProtocol] = None
        logger.info("iFlytek ASR Client initialized")

    def _generate_auth_url(self) -> str:
        """
        Generate authenticated WebSocket URL using HMAC-SHA256 signature.

        Returns:
            Authenticated WebSocket URL with signature

        Reference:
            https://www.xfyun.cn/doc/asr/rtasr/API.html#接口鉴权
        """
        # Generate RFC1123 format timestamp
        now = datetime.now()
        date = format_date_time(mktime(now.timetuple()))

        # Build signature string
        signature_origin = f"host: {self.ASR_HOST}\n"
        signature_origin += f"date: {date}\n"
        signature_origin += f"GET {self.ASR_PATH} HTTP/1.1"

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
            "host": self.ASR_HOST,
        }

        # Construct final URL
        auth_url = f"{self.ASR_URL}?{urlencode(params)}"
        logger.debug("Generated authenticated WebSocket URL")
        return auth_url

    def _build_frame(self, audio_data: bytes, status: int = 1) -> str:
        """
        Build audio frame in iFlytek format.

        Args:
            audio_data: Raw audio bytes (PCM 16kHz 16bit)
            status: Frame status (0=first, 1=continue, 2=last)

        Returns:
            JSON string containing audio frame
        """
        frame = {
            "common": {"app_id": self.appid},
            "business": {
                "language": "zh_cn",  # Chinese
                "domain": "iat",  # General transcription
                "accent": "mandarin",  # Mandarin Chinese
                "vad_eos": 2000,  # End of speech detection (2s silence)
                "dwa": "wpgs",  # Enable punctuation
            },
            "data": {
                "status": status,
                "format": self.AUDIO_FORMAT,
                "encoding": self.ENCODING,
                "audio": base64.b64encode(audio_data).decode("utf-8"),
            },
        }
        return json.dumps(frame)

    @staticmethod
    def _merge_transcript_text(current_text: str, incoming_text: str) -> str:
        """
        Merge ASR text fragments while avoiding duplicated accumulation.

        Some ASR streams return cumulative text chunks ("你" -> "你好" -> "你好。"),
        while others return incremental deltas. This merger handles both.
        """
        if not incoming_text:
            return current_text

        if not current_text:
            return incoming_text

        # Cumulative chunk replaces previous text.
        if incoming_text.startswith(current_text):
            return incoming_text

        # Ignore stale/shorter rollback fragments.
        if current_text.startswith(incoming_text):
            return current_text

        # Merge by longest suffix-prefix overlap.
        max_overlap = min(len(current_text), len(incoming_text))
        overlap = 0
        for size in range(max_overlap, 0, -1):
            if current_text.endswith(incoming_text[:size]):
                overlap = size
                break

        return f"{current_text}{incoming_text[overlap:]}"

    @staticmethod
    def _extract_text_from_ws(ws_list: list[dict]) -> str:
        """
        Extract recognized text from iFlytek `ws` result segments.

        iFlytek may return multiple candidates in `cw`. Only the top candidate
        should be used, otherwise candidates get concatenated and duplicate text appears.
        """
        text_parts: list[str] = []

        for ws_item in ws_list:
            cw_list = ws_item.get("cw", [])
            if not cw_list:
                continue

            best_word = ""
            for candidate in cw_list:
                candidate_word = candidate.get("w", "")
                if candidate_word:
                    best_word = candidate_word
                    break

            if best_word:
                text_parts.append(best_word)

        return "".join(text_parts)

    async def transcribe_stream(
        self,
        audio_stream: AsyncIterator[bytes],
        on_partial: Optional[Callable[[str], None]] = None,
        on_final: Optional[Callable[[str], None]] = None,
    ) -> str:
        """
        Transcribe audio stream in real-time.

        Args:
            audio_stream: Async iterator yielding audio chunks (PCM 16kHz 16bit)
            on_partial: Callback for partial transcription results
            on_final: Callback for final transcription result

        Returns:
            Complete transcription text

        Raises:
            ConnectionError: If WebSocket connection fails
            ValueError: If transcription fails
        """
        auth_url = self._generate_auth_url()
        transcript_state = {"text": ""}

        try:
            async with websockets.connect(auth_url) as ws:
                self.ws = ws
                logger.info("WebSocket connection established")

                # Start receiving task
                receive_task = asyncio.create_task(
                    self._receive_results(ws, transcript_state, on_partial, on_final)
                )

                # Send audio frames
                frame_count = 0
                async for audio_chunk in audio_stream:
                    if not audio_chunk:
                        break

                    status = 0 if frame_count == 0 else 1
                    frame = self._build_frame(audio_chunk, status)
                    await ws.send(frame)
                    frame_count += 1
                    logger.debug(f"Sent audio frame {frame_count}")

                # Send final frame (empty audio, status=2)
                final_frame = self._build_frame(b"", status=2)
                await ws.send(final_frame)
                logger.info(f"Sent final frame (total frames: {frame_count})")

                # Wait for all results
                await receive_task

        except websockets.exceptions.WebSocketException as e:
            logger.error(f"WebSocket error: {e}")
            raise ConnectionError(f"iFlytek ASR connection failed: {e}")
        except Exception as e:
            logger.error(f"Transcription error: {e}")
            raise
        finally:
            self.ws = None

        final_text = transcript_state["text"]
        logger.info(f"Transcription complete: '{final_text}'")
        return final_text

    async def _receive_results(
        self,
        ws: WebSocketClientProtocol,
        transcript_state: dict[str, str],
        on_partial: Optional[Callable[[str], None]],
        on_final: Optional[Callable[[str], None]],
    ) -> None:
        """
        Receive and process transcription results from WebSocket.

        Args:
            ws: WebSocket connection
            transcript_state: Mutable transcript state container
            on_partial: Callback for partial results
            on_final: Callback for final result
        """
        try:
            async for message in ws:
                try:
                    result = json.loads(message)
                    code = result.get("code")

                    if code != 0:
                        error_msg = result.get("message", "Unknown error")
                        logger.error(f"iFlytek ASR error {code}: {error_msg}")
                        raise ValueError(f"ASR error: {error_msg}")

                    # Extract transcription text
                    data = result.get("data", {})
                    status = data.get("status", 0)
                    ws_result = data.get("result", {})

                    if not ws_result:
                        continue

                    # Parse transcription segments
                    ws_list = ws_result.get("ws", [])
                    partial_text = self._extract_text_from_ws(ws_list)

                    if partial_text:
                        merged_text = self._merge_transcript_text(
                            transcript_state["text"],
                            partial_text,
                        )
                        transcript_state["text"] = merged_text
                        logger.debug(
                            "Partial result (status=%s): raw='%s', merged='%s'",
                            status,
                            partial_text,
                            merged_text,
                        )

                        # Trigger callbacks
                        if status == 2:  # Final result
                            if on_final:
                                on_final(transcript_state["text"])
                        else:  # Partial result
                            if on_partial:
                                on_partial(transcript_state["text"])

                    # Check if transcription is complete
                    if status == 2:
                        logger.info("Transcription complete (status=2)")
                        break

                except json.JSONDecodeError as e:
                    logger.error(f"Invalid JSON from ASR: {e}")
                    continue

        except asyncio.CancelledError:
            logger.info("Result receiving task cancelled")
        except Exception as e:
            logger.error(f"Error receiving results: {e}")
            raise


# Global ASR client instance
asr_client = IFlytekASRClient()

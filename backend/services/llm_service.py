"""
LLM Service for GLM-4.7 Integration.
Provides a wrapper around the Anthropic-compatible API for consistent error handling and retry logic.
"""
import logging
from typing import Optional, Dict, Any
from anthropic import Anthropic, APIError, APIConnectionError, RateLimitError
from config import settings

logger = logging.getLogger(__name__)


class LLMService:
    """
    Service for interacting with GLM-4.7 LLM via Anthropic-compatible API.
    Handles API calls, error handling, and retry logic.
    """

    def __init__(self):
        """Initialize Anthropic client with custom base URL for GLM-4.7."""
        self.client = Anthropic(
            api_key=settings.anthropic_auth_token,
            base_url=settings.anthropic_base_url,
        )
        self.model = "glm-4"  # GLM-4.7 model identifier
        self.max_retries = 3
        logger.info(f"LLM Service initialized with model: {self.model}")

    async def generate_response(
        self,
        system_prompt: str,
        user_message: str,
        max_tokens: int = 1024,
        temperature: float = 0.7,
    ) -> Optional[str]:
        """
        Generate a response from the LLM.

        Args:
            system_prompt: System instructions for the LLM
            user_message: User input message
            max_tokens: Maximum tokens in response
            temperature: Sampling temperature (0.0-1.0)

        Returns:
            LLM response text, or None if error occurs

        Raises:
            APIError: If API request fails after retries
        """
        retry_count = 0

        while retry_count < self.max_retries:
            try:
                logger.info(f"Sending request to LLM (attempt {retry_count + 1}/{self.max_retries})")

                message = self.client.messages.create(
                    model=self.model,
                    max_tokens=max_tokens,
                    temperature=temperature,
                    system=system_prompt,
                    messages=[
                        {"role": "user", "content": user_message}
                    ],
                )

                # Extract text content from response
                if message.content and len(message.content) > 0:
                    response_text = message.content[0].text
                    logger.info(f"LLM response received ({len(response_text)} chars)")
                    return response_text
                else:
                    logger.warning("LLM returned empty response")
                    return None

            except RateLimitError as e:
                retry_count += 1
                wait_time = 2 ** retry_count  # Exponential backoff
                logger.warning(
                    f"Rate limit exceeded. Retrying in {wait_time}s... "
                    f"(attempt {retry_count}/{self.max_retries})"
                )
                if retry_count < self.max_retries:
                    import asyncio
                    await asyncio.sleep(wait_time)
                else:
                    logger.error("Max retries exceeded due to rate limiting")
                    raise

            except APIConnectionError as e:
                retry_count += 1
                logger.warning(
                    f"API connection error: {e}. "
                    f"Retrying... (attempt {retry_count}/{self.max_retries})"
                )
                if retry_count < self.max_retries:
                    import asyncio
                    await asyncio.sleep(2)
                else:
                    logger.error("Max retries exceeded due to connection errors")
                    raise

            except APIError as e:
                logger.error(f"LLM API error: {e}")
                raise

            except Exception as e:
                logger.error(f"Unexpected error in LLM service: {e}")
                raise

        return None


# Global LLM service instance
llm_service = LLMService()

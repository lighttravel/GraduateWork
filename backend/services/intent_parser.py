"""
Intent Parser Service.
Converts natural language user input into structured aromatherapy control commands using LLM.
"""
import json
import logging
from typing import Optional, Dict, Any
from pathlib import Path
from pydantic import ValidationError

from services.llm_service import llm_service
from models import ControlJson

logger = logging.getLogger(__name__)

# Load system prompt
PROMPT_FILE = Path(__file__).parent.parent / "prompts" / "aromatherapy_system_prompt.txt"
with open(PROMPT_FILE, "r", encoding="utf-8") as f:
    SYSTEM_PROMPT = f.read()


class IntentParserService:
    """
    Service for parsing natural language commands into structured control parameters.
    Uses LLM with specialized system prompt to generate valid JSON.
    """

    def __init__(self):
        self.llm = llm_service
        logger.info("Intent Parser Service initialized")

    async def parse_command(self, user_input: str) -> Optional[Dict[str, Any]]:
        """
        Parse user's natural language input into structured control JSON.

        Args:
            user_input: User's voice command (transcribed text)

        Returns:
            Dictionary containing:
                - control_json: Validated control parameters
                - response_text: Natural language confirmation from LLM
            Returns None if parsing fails

        Raises:
            ValueError: If LLM output is invalid JSON or fails validation
        """
        try:
            logger.info(f"Parsing user input: '{user_input}'")

            # Call LLM with system prompt and user input
            llm_response = await self.llm.generate_response(
                system_prompt=SYSTEM_PROMPT,
                user_message=user_input,
                max_tokens=512,
                temperature=0.3,  # Lower temperature for more deterministic output
            )

            if not llm_response:
                logger.error("LLM returned empty response")
                return None

            # Extract JSON from response (handle potential markdown code blocks)
            json_text = self._extract_json(llm_response)

            # Parse JSON
            try:
                parsed = json.loads(json_text)
            except json.JSONDecodeError as e:
                logger.error(f"Invalid JSON from LLM: {e}")
                logger.debug(f"LLM raw response: {llm_response}")
                raise ValueError(f"LLM returned invalid JSON: {e}")

            # Extract response_text (if present)
            response_text = parsed.pop("response_text", None)

            # Validate control parameters with Pydantic
            try:
                control_json = ControlJson(**parsed)
            except ValidationError as e:
                logger.error(f"JSON validation failed: {e}")
                logger.debug(f"Parsed JSON: {parsed}")
                raise ValueError(f"Invalid control parameters: {e}")

            logger.info(
                f"Successfully parsed command: scent={control_json.scent_type}, "
                f"intensity={control_json.intensity}, duration={control_json.duration_minutes}min"
            )

            return {
                "control_json": control_json.model_dump(),
                "response_text": response_text or self._generate_default_response(control_json),
            }

        except Exception as e:
            logger.error(f"Error parsing command: {e}")
            raise

    def _extract_json(self, text: str) -> str:
        """
        Extract JSON from LLM response.
        Handles cases where JSON is wrapped in markdown code blocks.

        Args:
            text: Raw LLM response

        Returns:
            Cleaned JSON string
        """
        text = text.strip()

        # Remove markdown code blocks if present
        if text.startswith("```json"):
            text = text[7:]  # Remove ```json
        if text.startswith("```"):
            text = text[3:]  # Remove ```

        if text.endswith("```"):
            text = text[:-3]  # Remove trailing ```

        return text.strip()

    def _generate_default_response(self, control_json: ControlJson) -> str:
        """
        Generate a default natural language response if LLM doesn't provide one.

        Args:
            control_json: Validated control parameters

        Returns:
            Human-readable confirmation message
        """
        scent_names = {
            "lemon": "fresh lemon",
            "lavender": "calming lavender",
            "woody": "warm woody",
            "floral": "delicate floral",
            "mixed": "custom blend",
        }

        rhythm_desc = {
            "gradual": "steady release",
            "pulse": "pulsing rhythm",
            "intermittent": "natural variation",
        }

        scent = scent_names.get(control_json.scent_type, control_json.scent_type)
        rhythm = rhythm_desc.get(control_json.release_rhythm, control_json.release_rhythm)

        if control_json.scent_type == "mixed" and control_json.mixing_ratios:
            ratios_str = ", ".join(
                f"{int(ratio * 100)}% {scent}"
                for scent, ratio in control_json.mixing_ratios.model_dump().items()
                if ratio and ratio > 0
            )
            return (
                f"Setting aromatherapy blend ({ratios_str}) "
                f"at intensity {control_json.intensity} with {rhythm} "
                f"for {control_json.duration_minutes} minutes."
            )
        else:
            return (
                f"Setting {scent} scent at intensity {control_json.intensity} "
                f"with {rhythm} for {control_json.duration_minutes} minutes."
            )


# Global intent parser instance
intent_parser = IntentParserService()

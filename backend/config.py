"""
Configuration management using Pydantic Settings.
Loads and validates environment variables from .env file.
"""
from pathlib import Path
from typing import List
from pydantic import Field, field_validator
from pydantic_settings import BaseSettings, SettingsConfigDict

BACKEND_DIR = Path(__file__).resolve().parent


class Settings(BaseSettings):
    """Application settings loaded from environment variables."""

    model_config = SettingsConfigDict(
        env_file=BACKEND_DIR / ".env",
        env_file_encoding="utf-8",
        case_sensitive=False,
        extra="ignore",
    )

    # Database
    database_url: str = Field(..., validation_alias="DATABASE_URL")

    # iFlytek ASR (Automatic Speech Recognition)
    iflytek_asr_appid: str = Field(..., validation_alias="IFLYTEK_ASR_APPID")
    iflytek_asr_api_secret: str = Field(..., validation_alias="IFLYTEK_ASR_API_SECRET")
    iflytek_asr_api_key: str = Field(..., validation_alias="IFLYTEK_ASR_API_KEY")

    # iFlytek TTS (Text-to-Speech)
    iflytek_tts_appid: str = Field(..., validation_alias="IFLYTEK_TTS_APPID")
    iflytek_tts_api_secret: str = Field(..., validation_alias="IFLYTEK_TTS_API_SECRET")
    iflytek_tts_api_key: str = Field(..., validation_alias="IFLYTEK_TTS_API_KEY")

    # GLM-4.7 LLM (via Anthropic-compatible API)
    anthropic_auth_token: str = Field(..., validation_alias="ANTHROPIC_AUTH_TOKEN")
    anthropic_base_url: str = Field(..., validation_alias="ANTHROPIC_BASE_URL")
    anthropic_model: str = Field(default="glm-4.7", validation_alias="ANTHROPIC_MODEL")

    # Application
    app_env: str = Field(default="development", validation_alias="APP_ENV")
    app_host: str = Field(default="0.0.0.0", validation_alias="APP_HOST")
    app_port: int = Field(default=8000, validation_alias="APP_PORT")
    cors_origins: str = Field(
        default="http://localhost:5173,http://localhost:3000",
        validation_alias="CORS_ORIGINS",
    )

    # Logging
    log_level: str = Field(default="INFO", validation_alias="LOG_LEVEL")

    @field_validator("cors_origins", mode="before")
    @classmethod
    def validate_cors_origins(cls, value: str | List[str]) -> str:
        """Validate CORS origins shape while keeping env format compatibility."""
        if isinstance(value, list):
            return ",".join([origin.strip() for origin in value if origin.strip()])
        if isinstance(value, str):
            return value
        raise ValueError("CORS_ORIGINS must be a comma-separated string or list.")

    @property
    def cors_origins_list(self) -> List[str]:
        """Parsed CORS origins for FastAPI middleware."""
        return [origin.strip() for origin in self.cors_origins.split(",") if origin.strip()]


# Global settings instance
settings = Settings()


def validate_settings() -> None:
    """
    Validate that all required settings are present.
    Call this at application startup to fail fast if configuration is invalid.
    """
    required_fields = [
        "database_url",
        "iflytek_asr_appid",
        "iflytek_asr_api_secret",
        "iflytek_asr_api_key",
        "iflytek_tts_appid",
        "iflytek_tts_api_secret",
        "iflytek_tts_api_key",
        "anthropic_auth_token",
        "anthropic_base_url",
    ]

    missing = []
    for field in required_fields:
        if not getattr(settings, field, None):
            missing.append(field.upper())

    if missing:
        raise ValueError(
            f"Missing required environment variables: {', '.join(missing)}\n"
            f"Please check your .env file and ensure all required variables are set."
        )

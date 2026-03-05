"""
Configuration management using Pydantic Settings.
Loads and validates environment variables from .env file.
"""
from pydantic_settings import BaseSettings
from pydantic import Field, validator
from typing import List


class Settings(BaseSettings):
    """Application settings loaded from environment variables."""

    # Database
    database_url: str = Field(..., env="DATABASE_URL")

    # iFlytek ASR (Automatic Speech Recognition)
    iflytek_asr_appid: str = Field(..., env="IFLYTEK_ASR_APPID")
    iflytek_asr_api_secret: str = Field(..., env="IFLYTEK_ASR_API_SECRET")
    iflytek_asr_api_key: str = Field(..., env="IFLYTEK_ASR_API_KEY")

    # iFlytek TTS (Text-to-Speech)
    iflytek_tts_appid: str = Field(..., env="IFLYTEK_TTS_APPID")
    iflytek_tts_api_secret: str = Field(..., env="IFLYTEK_TTS_API_SECRET")
    iflytek_tts_api_key: str = Field(..., env="IFLYTEK_TTS_API_KEY")

    # GLM-4.7 LLM (via Anthropic-compatible API)
    anthropic_auth_token: str = Field(..., env="ANTHROPIC_AUTH_TOKEN")
    anthropic_base_url: str = Field(..., env="ANTHROPIC_BASE_URL")

    # Application
    app_env: str = Field(default="development", env="APP_ENV")
    app_host: str = Field(default="0.0.0.0", env="APP_HOST")
    app_port: int = Field(default=8000, env="APP_PORT")
    cors_origins: str = Field(
        default="http://localhost:5173,http://localhost:3000",
        env="CORS_ORIGINS"
    )

    # Logging
    log_level: str = Field(default="INFO", env="LOG_LEVEL")

    @validator("cors_origins")
    def parse_cors_origins(cls, v: str) -> List[str]:
        """Parse comma-separated CORS origins into a list."""
        return [origin.strip() for origin in v.split(",")]

    class Config:
        env_file = ".env"
        env_file_encoding = "utf-8"
        case_sensitive = False


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

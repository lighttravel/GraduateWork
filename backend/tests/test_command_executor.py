import importlib
from types import SimpleNamespace
from uuid import uuid4

import pytest

from services.command_executor import CommandExecutorService

command_executor_module = importlib.import_module("services.command_executor")


@pytest.mark.asyncio
async def test_execute_command_success_pipeline(monkeypatch):
    saved_status_updates: list[tuple[str, str, str | None]] = []

    class FakeRepository:
        def __init__(self, _session):
            pass

        async def create_command(self, user_input_text, control_json, llm_response_text=None):
            return SimpleNamespace(id=uuid4(), user_input_text=user_input_text, control_json=control_json)

        async def update_status(self, command_id, status, execution_error=None):
            saved_status_updates.append((str(command_id), status, execution_error))
            return SimpleNamespace(id=command_id, status=status)

    async def fake_parse_command(_text):
        return {
            "control_json": {
                "scent_type": "floral",
                "intensity": 4,
                "duration_minutes": 12,
                "release_rhythm": "gradual",
            },
            "response_text": "Floral mode enabled.",
        }

    async def fake_execute_device(_control_json):
        return True

    async def fake_tts(*_args, **_kwargs):
        return b"audio-bytes"

    monkeypatch.setattr(command_executor_module, "CommandRepository", FakeRepository)
    monkeypatch.setattr(command_executor_module.intent_parser, "parse_command", fake_parse_command)
    monkeypatch.setattr(command_executor_module.device_controller, "execute_command", fake_execute_device)
    monkeypatch.setattr(command_executor_module.tts_client, "synthesize", fake_tts)

    events: list[str] = []

    def on_event(event_type, _event_data):
        events.append(event_type)

    service = CommandExecutorService()
    result = await service.execute_command("set floral", session=object(), on_event=on_event)

    assert result["status"] == "executed"
    assert result["control_json"]["scent_type"] == "floral"
    assert result["tts_audio_base64"] is not None
    assert "execution_complete" in events
    assert any(status == "executed" for _, status, _ in saved_status_updates)


@pytest.mark.asyncio
async def test_execute_command_marks_failed_when_device_fails(monkeypatch):
    saved_status_updates: list[tuple[str, str, str | None]] = []

    class FakeRepository:
        def __init__(self, _session):
            pass

        async def create_command(self, *_args, **_kwargs):
            return SimpleNamespace(id=uuid4())

        async def update_status(self, command_id, status, execution_error=None):
            saved_status_updates.append((str(command_id), status, execution_error))
            return SimpleNamespace(id=command_id, status=status)

    async def fake_parse_command(_text):
        return {
            "control_json": {
                "scent_type": "lemon",
                "intensity": 5,
                "duration_minutes": 20,
                "release_rhythm": "pulse",
            },
            "response_text": "Lemon profile set.",
        }

    async def fake_execute_device(_control_json):
        return False

    async def fake_tts(*_args, **_kwargs):
        return b"unused"

    monkeypatch.setattr(command_executor_module, "CommandRepository", FakeRepository)
    monkeypatch.setattr(command_executor_module.intent_parser, "parse_command", fake_parse_command)
    monkeypatch.setattr(command_executor_module.device_controller, "execute_command", fake_execute_device)
    monkeypatch.setattr(command_executor_module.tts_client, "synthesize", fake_tts)

    events: list[str] = []

    def on_event(event_type, _event_data):
        events.append(event_type)

    service = CommandExecutorService()

    with pytest.raises(RuntimeError, match="Device command execution failed"):
        await service.execute_command("set lemon", session=object(), on_event=on_event)

    assert "execution_error" in events
    assert any(status == "failed" for _, status, _ in saved_status_updates)

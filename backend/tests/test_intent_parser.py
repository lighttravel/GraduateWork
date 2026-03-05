import pytest

from services.intent_parser import IntentParserService


@pytest.mark.asyncio
async def test_parse_command_valid_json_with_response_text(monkeypatch):
    parser = IntentParserService()

    async def fake_generate_response(**_kwargs):
        return """
        {
          "scent_type": "lemon",
          "intensity": 7,
          "duration_minutes": 20,
          "release_rhythm": "gradual",
          "response_text": "Applied lemon profile."
        }
        """

    monkeypatch.setattr(parser.llm, "generate_response", fake_generate_response)

    result = await parser.parse_command("make it fresh")

    assert result is not None
    assert result["control_json"]["scent_type"] == "lemon"
    assert result["control_json"]["intensity"] == 7
    assert result["response_text"] == "Applied lemon profile."


@pytest.mark.asyncio
async def test_parse_command_supports_markdown_json_block(monkeypatch):
    parser = IntentParserService()

    async def fake_generate_response(**_kwargs):
        return """```json
{
  "scent_type": "woody",
  "intensity": 5,
  "duration_minutes": 15,
  "release_rhythm": "pulse"
}
```"""

    monkeypatch.setattr(parser.llm, "generate_response", fake_generate_response)
    result = await parser.parse_command("warmer tone")

    assert result is not None
    assert result["control_json"]["scent_type"] == "woody"
    assert "response_text" in result


@pytest.mark.asyncio
async def test_parse_command_raises_on_invalid_json(monkeypatch):
    parser = IntentParserService()

    async def fake_generate_response(**_kwargs):
        return "{ invalid json }"

    monkeypatch.setattr(parser.llm, "generate_response", fake_generate_response)

    with pytest.raises(ValueError, match="invalid JSON"):
        await parser.parse_command("broken json")


@pytest.mark.asyncio
async def test_parse_command_raises_on_schema_validation(monkeypatch):
    parser = IntentParserService()

    async def fake_generate_response(**_kwargs):
        return """
        {
          "scent_type": "lemon",
          "intensity": 99,
          "duration_minutes": 30,
          "release_rhythm": "intermittent"
        }
        """

    monkeypatch.setattr(parser.llm, "generate_response", fake_generate_response)

    with pytest.raises(ValueError, match="Invalid control parameters"):
        await parser.parse_command("create blend")

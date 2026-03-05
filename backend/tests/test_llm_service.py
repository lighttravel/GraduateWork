from types import SimpleNamespace

from services.llm_service import LLMService


def test_extract_response_text_skips_thinking_block():
    message = SimpleNamespace(
        content=[
            SimpleNamespace(type="thinking", text=None),
            SimpleNamespace(type="text", text='{"response_text":"ok"}'),
        ]
    )

    assert LLMService._extract_response_text(message) == '{"response_text":"ok"}'


def test_extract_response_text_returns_none_when_no_text():
    message = SimpleNamespace(
        content=[
            SimpleNamespace(type="thinking", text=None),
            SimpleNamespace(type="tool_use", text=""),
        ]
    )

    assert LLMService._extract_response_text(message) is None

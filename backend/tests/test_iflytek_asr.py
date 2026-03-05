from services.iflytek_asr import IFlytekASRClient


def test_extract_text_from_ws_uses_top_candidate_only():
    ws_list = [
        {"cw": [{"w": "你"}, {"w": "你好"}]},
        {"cw": [{"w": "好"}, {"w": "号"}]},
    ]

    text = IFlytekASRClient._extract_text_from_ws(ws_list)

    assert text == "你好"


def test_merge_transcript_text_handles_cumulative_chunks():
    merged = IFlytekASRClient._merge_transcript_text("", "你")
    merged = IFlytekASRClient._merge_transcript_text(merged, "你好")
    merged = IFlytekASRClient._merge_transcript_text(merged, "你好。")

    assert merged == "你好。"


def test_merge_transcript_text_handles_incremental_overlap():
    merged = IFlytekASRClient._merge_transcript_text("今天", "天很好")

    assert merged == "今天很好"

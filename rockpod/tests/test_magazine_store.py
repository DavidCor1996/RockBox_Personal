from services.magazine_store import parse_magazine_download_progress


def test_parse_magazine_download_progress_reports_completion():
    progress = parse_magazine_download_progress(
        ["ROCKPOD_MAGAZINE_PROGRESS=42%", "ROCKPOD_MAGAZINE_OUTPUT=/tmp/example.pdf"]
    )
    assert progress["phase"] == "Completed"
    assert progress["progress"] == "100%"
    assert progress["detail"] == "example.pdf"

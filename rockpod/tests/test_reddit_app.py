from pathlib import Path

from PIL import Image

import services.reddit_app as reddit_app
from services.reddit_app import RedditAppService, _subreddit_slug

ROOT = Path(__file__).resolve().parents[2]


def test_reddit_subreddit_url_validation():
    assert _subreddit_slug("https://www.reddit.com/r/ipod/") == "ipod"


def test_reddit_sync_screen_exposes_both_video_formats():
    panel = (ROOT / "rockpod/ui/reddit_panel.py").read_text(encoding="utf-8")
    assert 'QPushButton("Sync as MPEG")' in panel
    assert 'QPushButton("Sync as H.264")' in panel


def test_reddit_photo_sync(config, mock_device):
    service = RedditAppService(config, ROOT)
    source = Path(config.get("cache_dir")) / "reddit-source.jpg"
    Image.new("RGB", (1200, 900), "orange").save(source)
    service._save({"subreddits": [{"name": "ipod", "display_name": "r/ipod", "subscribers": 10, "posts": [{"id": "rd_abc", "type": "photo", "title": "Classic iPod", "body": "", "author": "tester", "source_path": str(source), "score": 25, "comments": 3, "ratio": 98, "created": "2026-08-22", "flair": "Picture"}]}]})
    report = service.sync(mock_device)
    root = Path(mock_device) / ".rockbox/reddit"
    assert report["posts"] == 1
    assert (root / "library.tsv").is_file()
    assert (root / "display/rd_abc.bmp").is_file()
    assert (root / "display/rd_abc.pane.bmp").is_file()
    with Image.open(root / "thumbnails/rd_abc.bmp") as thumb:
        assert thumb.size == (72, 72)
    assert "r/ipod" in (root / "subreddits.tsv").read_text(encoding="utf-8")


def test_reddit_format_switch_installs_selected_video_then_removes_other(
    config, mock_device, monkeypatch,
):
    service = RedditAppService(config, ROOT)
    source = Path(config.get("cache_dir")) / "reddit-video.mp4"
    source.write_bytes(b"source")
    service._save({"subreddits": [{
        "name": "ipod", "display_name": "r/ipod", "posts": [{
            "id": "rd_video", "type": "video", "title": "Video",
            "author": "tester", "source_path": str(source),
        }],
    }]})

    def fake_stage(_source, target, **kwargs):
        Path(target).write_bytes(kwargs["profile"].encode("ascii"))

    monkeypatch.setattr(reddit_app, "stage_app_video", fake_stage)
    monkeypatch.setattr(reddit_app.subprocess, "run", lambda *a, **k: None)
    media = Path(mock_device) / ".rockbox/reddit/media"

    service.sync(mock_device, video_profile="quality")
    assert (media / "rd_video.mpg").is_file()
    (media / "rd_video.source").write_text("legacy", encoding="utf-8")

    def failed_stage(*_args, **_kwargs):
        raise RuntimeError("conversion failed")

    monkeypatch.setattr(reddit_app, "stage_app_video", failed_stage)
    try:
        service.sync(mock_device, video_profile="h264_apple_exact")
    except RuntimeError as exc:
        assert "conversion failed" in str(exc)
    else:
        raise AssertionError("failed conversion unexpectedly succeeded")
    assert (media / "rd_video.mpg").is_file()

    monkeypatch.setattr(reddit_app, "stage_app_video", fake_stage)
    service.sync(mock_device, video_profile="h264_apple_exact")

    assert (media / "rd_video.m4v").read_bytes() == b"h264_apple_exact"
    assert not (media / "rd_video.mpg").exists()
    assert not (media / "rd_video.mpg.source").exists()
    assert not (media / "rd_video.source").exists()
    assert "/.rockbox/reddit/media/rd_video.m4v" in (
        Path(mock_device) / ".rockbox/reddit/library.tsv"
    ).read_text(encoding="utf-8")

"""Twitter import and iPod sync behavior."""

import json
from pathlib import Path

import pytest
from PIL import Image

from services.twitter_app import TwitterAppService, twitter_handle


ROOT = Path(__file__).resolve().parents[2]


def test_twitter_share_url_and_handle():
    assert twitter_handle("https://x.com/reiivalentinaa?s=11") == "reiivalentinaa"
    assert twitter_handle("@Reiivalentinaa") == "reiivalentinaa"
    with pytest.raises(ValueError):
        twitter_handle("https://example.com/reiivalentinaa")


def test_twitter_dump_includes_text_posts_and_reports_authentication():
    metadata = {
        "tweet_id": 123456789,
        "author": {"name": "reiivalentinaa", "nick": "Valentina"},
        "content": "Hello from X", "date": "2026-09-22 12:00:00",
        "favorite_count": 7, "count": 0,
    }
    posts = TwitterAppService._parse_messages([[1, "", metadata]], 5)
    assert posts[0]["text"] == "Hello from X"
    assert posts[0]["source_media"] == []
    posts = TwitterAppService._parse_messages(
        [[2, metadata], [3, "https://pbs.twimg.com/photo.jpg", metadata]], 5
    )
    assert posts[0]["source_media"] == ["https://pbs.twimg.com/photo.jpg"]
    with pytest.raises(ValueError, match="signed-in Firefox"):
        TwitterAppService._parse_messages(
            [[-1, {"error": "AuthRequired"}]], 5
        )


def test_twitter_photo_sync_uses_external_cache_and_installs_icon(
    config, mock_device, tmp_path,
):
    external = tmp_path / "external" / "RockPod" / "twitter"
    external.mkdir(parents=True)
    config.set("twitter_cache_dir", str(external))
    service = TwitterAppService(config, ROOT)
    original = external / "original.jpg"
    Image.new("RGB", (800, 600), "skyblue").save(original)
    avatar = external / "avatar.jpg"
    Image.new("RGB", (100, 100), "orange").save(avatar)
    payload = {"accounts": [{
        "handle": "reiivalentinaa", "display_name": "Valentina",
        "avatar_path": str(avatar), "posts": [{
            "id": "123456789", "handle": "reiivalentinaa",
            "display_name": "Valentina", "date": "2026-09-22",
            "text": "A photo post", "likes": 8, "replies": 2,
            "retweets": 1, "media": [str(original)],
        }],
    }]}
    (external / "library.json").write_text(json.dumps(payload), encoding="utf-8")

    report = service.sync(mock_device)
    device = Path(mock_device)
    assert report["posts"] == 1
    assert report["media"] == 1
    assert (device / ".rockbox/twitter/media/123456789-1.bmp").is_file()
    assert (device / ".rockbox/twitter/assets/twitter-logo.bmp").is_file()
    assert (device / ".rockbox/ipodjs/applications/twitter.46x46x24.bmp").is_file()
    assert "/.rockbox/twitter/media/123456789-1.bmp" in (
        device / ".rockbox/twitter/library.tsv"
    ).read_text(encoding="utf-8")
    assert (external / "work/device-sync/outputs.sqlite3").is_file()
    assert original.is_file()

from pathlib import Path

from PIL import Image

from services.reddit_app import RedditAppService, _subreddit_slug

ROOT = Path(__file__).resolve().parents[2]


def test_reddit_subreddit_url_validation():
    assert _subreddit_slug("https://www.reddit.com/r/ipod/") == "ipod"


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

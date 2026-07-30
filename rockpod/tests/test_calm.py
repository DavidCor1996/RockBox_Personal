import json
from pathlib import Path

from services.calm import CALM_MANIFEST_VERSION, CalmService


class _Config:
    def __init__(self, root):
        self.values = {
            "cache_dir": str(root / "cache"),
            "youtube_calm_binary": "yt-dlp",
        }

    def get(self, key, default=None):
        return self.values.get(key, default)


def _service(tmp_path):
    repo = tmp_path / "repo"
    icon = repo / "assets/ipodjs/rockbox/calm/calm-icon.64x64x24.bmp"
    icon.parent.mkdir(parents=True)
    icon.write_bytes(b"BM-test")
    return CalmService(_Config(tmp_path), repo)


def test_download_command_is_strictly_audio_only(tmp_path):
    service = _service(tmp_path)
    command, category = service.build_download_command(
        "https://www.youtube.com/watch?v=test", "Sleep"
    )
    joined = " ".join(command)
    assert "--extract-audio" in command
    assert "--audio-format mp3" in joined
    assert "--no-playlist" in command
    assert "--no-write-thumbnail" in command
    assert "video" not in joined.lower()
    assert category == "Sleep"


def test_scan_sanitizes_metadata_and_defaults_category(tmp_path):
    service = _service(tmp_path)
    (service.library_root / "abc.mp3").write_bytes(b"sound")
    (service.library_root / "abc.info.json").write_text(
        json.dumps(
            {
                "title": "Rain\tOver\nLeaves",
                "duration": "65.8",
                "webpage_url": "https://youtu.be/abc",
            }
        ),
        encoding="utf-8",
    )
    assert service.scan() == [
        {
            "id": "abc",
            "title": "Rain Over Leaves",
            "category": "Sounds",
            "duration": 65,
            "source_url": "https://youtu.be/abc",
            "audio_path": str((service.library_root / "abc.mp3").resolve()),
            "size": 5,
        }
    ]


def test_sync_is_scoped_and_writes_device_paths(tmp_path):
    service = _service(tmp_path)
    (service.library_root / "ocean.mp3").write_bytes(b"ocean")
    (service.library_root / "ocean.info.json").write_text(
        json.dumps({"title": "Ocean Surf", "duration": 120}),
        encoding="utf-8",
    )
    service.set_category("ocean", "Sleep")
    simdisk = tmp_path / "simdisk"
    result = service.sync(
        {"simulator_simdisk_path": str(simdisk)}, target_mode="simulator"
    )
    target = simdisk / ".rockbox/rockpod/calm"
    rows = (target / "library.tsv").read_text(encoding="utf-8").splitlines()
    assert result["count"] == 1
    assert rows[0] == CALM_MANIFEST_VERSION
    assert rows[1].split("\t")[:4] == [
        "Ocean Surf",
        "Sleep",
        "120",
        "/.rockbox/rockpod/calm/sounds/ocean.mp3",
    ]
    assert (target / "sounds/ocean.mp3").read_bytes() == b"ocean"
    assert (target / "assets/calm-icon.64x64x24.bmp").is_file()
    assert (target / "database.ignore").is_file()

"""Tests for the sync engine — plan building, path construction, file copying."""

import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import pytest
from PIL import Image

from app.config import Config
from app.database import Database
from services.artwork_manager import ArtworkManager
from services.device_detector import DeviceDetector, DeviceInfo, create_mock_device
from services.sync_engine import (
    ALBUM_LIST_DEVICE_DIR,
    SyncEngine,
    SyncPlan,
    SyncWorker,
    VIDEO_LIST_DEVICE_DIR,
    VIDEO_LIST_THUMB_DEVICE_DIR,
    _clear_device_trash,
    _looks_like_auto_duplicate_path,
    build_device_path,
    _sanitize_filename,
)
from services.video_rvp import VideoRvpTranscoder


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _insert_track(db, title, artist, album, **kw):
    """Insert a track into the DB and return its row dict."""
    data = {
        "file_path": kw.get("file_path", f"/music/{artist}/{album}/{title}.mp3"),
        "file_hash": kw.get("file_hash", ""),
        "file_size": kw.get("file_size", 5_000_000),
        "title": title,
        "artist": artist,
        "album": album,
        "album_artist": kw.get("album_artist", artist),
        "genre": kw.get("genre", "Rock"),
        "year": kw.get("year", 2020),
        "track_number": kw.get("track_number", 1),
        "disc_number": kw.get("disc_number", 1),
        "duration": kw.get("duration", 240.0),
        "bitrate": kw.get("bitrate", 320),
        "codec": kw.get("codec", "mp3"),
        "metadata_hash": kw.get("metadata_hash", "abc123"),
        "artwork_hash": kw.get("artwork_hash", ""),
        "synced_to_device": kw.get("synced_to_device", 0),
        "device_path": kw.get("device_path", None),
        "last_synced_metadata_hash": kw.get("last_synced_metadata_hash", ""),
        "last_synced_file_hash": kw.get("last_synced_file_hash", ""),
        "media_type": kw.get("media_type", "audio"),
        "show_title": kw.get("show_title", ""),
        "season_number": kw.get("season_number", None),
        "episode_number": kw.get("episode_number", None),
        "video_kind": kw.get("video_kind", "movie"),
    }
    db.upsert_track(data)
    db.commit()
    row = db.get_track_by_path(data["file_path"])
    return row


def _insert_device_track(db, title, artist, album, device_path, **kw):
    """Insert a device track."""
    data = {
        "device_id": kw.get("device_id", "mock"),
        "device_path": device_path,
        "file_size": kw.get("file_size", 5_000_000),
        "title": title,
        "artist": artist,
        "album": album,
        "album_artist": kw.get("album_artist", artist),
        "genre": kw.get("genre", "Rock"),
        "year": kw.get("year", 2020),
        "track_number": kw.get("track_number", 1),
        "disc_number": kw.get("disc_number", 1),
        "duration": kw.get("duration", 240.0),
        "bitrate": kw.get("bitrate", 320),
        "codec": kw.get("codec", "mp3"),
        "metadata_hash": kw.get("metadata_hash", "abc123"),
        "local_track_id": kw.get("local_track_id", None),
        "file_hash": kw.get("file_hash", ""),
    }
    db.upsert_device_track(data)
    db.commit()


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def env():
    """Create a temporary environment with config, db, mock device, and source files."""
    tmp = tempfile.mkdtemp(prefix="rockpod_sync_test_")
    cfg_path = os.path.join(tmp, "config.json")
    config = Config(cfg_path)
    config.music_dir = os.path.join(tmp, "Music")
    config.db_path = os.path.join(tmp, "library.db")
    config.cache_dir = os.path.join(tmp, "cache")
    config.artwork_cache_dir = os.path.join(tmp, "cache", "artwork")
    config.mock_device_enabled = True
    config.mock_device_path = os.path.join(tmp, "mock_ipod")
    config.ensure_dirs()
    os.makedirs(config.music_dir, exist_ok=True)

    create_mock_device(config.mock_device_path)

    db = Database(config.db_path)

    yield {
        "tmp": tmp,
        "config": config,
        "db": db,
        "device_path": config.mock_device_path,
    }

    db.close()
    shutil.rmtree(tmp, ignore_errors=True)


def _make_source_file(env_dict, artist, album, title, ext=".mp3", size=1024):
    """Create a real file on disk and return its path."""
    path = os.path.join(env_dict["tmp"], "Music", artist, album, f"{title}{ext}")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(os.urandom(size))
    return path


def test_video_device_path_uses_file_stem_when_title_is_youtube():
    rel_path = build_device_path(
        {
            "file_path": "/media/Videos/YouTube/Real Movie.mpg",
            "media_type": "video",
            "title": "youtube",
            "video_kind": "movie",
        },
        "Music/{album_artist}/{album}",
        "{track_number:02d} - {title}{ext}",
    )

    assert rel_path == os.path.join("Videos", "Downloaded", "Real Movie.mpg")


# ===========================================================================
# Tests: _sanitize_filename
# ===========================================================================

class TestSanitizeFilename:
    def test_empty_returns_unknown(self):
        assert _sanitize_filename("") == "Unknown"
        assert _sanitize_filename(None) == "Unknown"

    def test_forbidden_chars_replaced(self):
        assert _sanitize_filename('A<B>C:D"E') == "A_B_C_D_E"
        assert _sanitize_filename("a|b?c*d") == "a_b_c_d"

    def test_whitespace_collapsed(self):
        assert _sanitize_filename("  foo   bar  ") == "foo bar"

    def test_length_capped(self):
        long_name = "x" * 300
        result = _sanitize_filename(long_name)
        assert len(result) <= 200


class TestDeviceMediaDetection:
    def test_device_has_indexable_media_on_disk_uses_music_roots(self, env):
        device = DeviceInfo(env["device_path"])

        stray = os.path.join(env["device_path"], "LooseTrack.flac")
        with open(stray, "wb") as handle:
            handle.write(b"x" * 16)

        assert SyncEngine._device_has_indexable_media_on_disk(device) is False

        music_track = os.path.join(env["device_path"], "Music", "Artist", "Album", "01 - Track.flac")
        os.makedirs(os.path.dirname(music_track), exist_ok=True)
        with open(music_track, "wb") as handle:
            handle.write(b"x" * 16)

        assert SyncEngine._device_has_indexable_media_on_disk(device) is True

    def test_device_inventory_has_local_links_false_for_unverified_empty_device(self, env):
        detector = DeviceDetector(env["config"])
        detector._current_device = DeviceInfo(env["device_path"])
        engine = SyncEngine(env["db"], env["config"], detector)
        engine.set_current_device(detector.current_device)

        assert engine.get_device_tracks() == []
        assert engine.device_inventory_has_local_links() is False


# ===========================================================================
# Tests: build_device_path
# ===========================================================================

class TestBuildDevicePath:
    def test_default_templates(self):
        row = {
            "album_artist": "Pink Floyd",
            "artist": "Pink Floyd",
            "album": "The Dark Side of the Moon",
            "title": "Breathe",
            "track_number": 2,
            "disc_number": 1,
            "file_path": "/music/breathe.flac",
        }
        result = build_device_path(
            row,
            "Music/{album_artist}/{album}",
            "{track_number:02d} - {title}{ext}",
        )
        assert result == os.path.join(
            "Music", "Pink Floyd", "The Dark Side of the Moon", "02 - Breathe.flac"
        )

    def test_missing_album_artist_falls_back_to_artist(self):
        row = {
            "album_artist": "",
            "artist": "Radiohead",
            "album": "OK Computer",
            "title": "Airbag",
            "track_number": 1,
            "disc_number": 1,
            "file_path": "/music/airbag.mp3",
        }
        result = build_device_path(
            row,
            "Music/{album_artist}/{album}",
            "{track_number:02d} - {title}{ext}",
        )
        assert "Radiohead" in result

    def test_missing_everything_gives_sane_defaults(self):
        row = {
            "album_artist": "",
            "artist": "",
            "album": "",
            "title": "",
            "track_number": None,
            "disc_number": None,
            "file_path": "",
        }
        result = build_device_path(
            row,
            "Music/{album_artist}/{album}",
            "{track_number:02d} - {title}{ext}",
        )
        assert "Unknown" in result

    def test_special_chars_sanitized(self):
        row = {
            "album_artist": 'AC/DC',
            "artist": 'AC/DC',
            "album": 'Back in Black: Deluxe',
            "title": 'Shoot to "Thrill"',
            "track_number": 3,
            "disc_number": 1,
            "file_path": "/music/track.mp3",
        }
        result = build_device_path(
            row,
            "Music/{album_artist}/{album}",
            "{track_number:02d} - {title}{ext}",
        )
        # Should not contain forbidden FAT32 chars
        for c in '<>:"/\\|?*':
            # The path separator is allowed (/), but the file and dir components
            # should not have them.
            parts = result.split(os.sep)
            for part in parts:
                if c == os.sep:
                    continue
                assert c not in part, f"Forbidden char '{c}' in path component '{part}'"

    def test_sync_output_ext_overrides_source_extension(self):
        row = {
            "album_artist": "Pink Floyd",
            "artist": "Pink Floyd",
            "album": "Wish You Were Here",
            "title": "Shine On",
            "track_number": 1,
            "disc_number": 1,
            "file_path": "/music/shine_on.flac",
            "sync_output_ext": ".mp3",
        }
        result = build_device_path(
            row,
            "Music/{album_artist}/{album}",
            "{track_number:02d} - {title}{ext}",
        )
        assert result.endswith(os.path.join("Wish You Were Here", "01 - Shine On.mp3"))

    def test_video_rows_default_to_videos_folder(self):
        row = {
            "title": "Family Movie",
            "file_path": "/videos/family.mpg",
            "media_type": "video",
            "video_kind": "home_video",
        }
        result = build_device_path(
            row,
            "Music/{album_artist}/{album}",
            "{track_number:02d} - {title}{ext}",
        )
        assert result == os.path.join("Videos", "Home Videos", "Family Movie.mpg")

    def test_downloaded_video_rows_use_downloaded_folder(self):
        row = {
            "title": "YouTube",
            "file_path": "/videos/YouTube/downloaded.mpg",
            "media_type": "video",
            "video_kind": "movie",
        }
        result = build_device_path(
            row,
            "Music/{album_artist}/{album}",
            "{track_number:02d} - {title}{ext}",
        )
        assert result == os.path.join("Videos", "Downloaded", "downloaded.mpg")

    def test_show_video_rows_use_show_and_season_folder(self):
        row = {
            "title": "Lord of the Nerds",
            "file_path": "/videos/YouTube/Disney's Recess - Lord Of the Nerds.mpg",
            "media_type": "video",
            "video_kind": "show",
            "show_title": "Recess",
            "season_number": 3,
            "episode_number": 37,
            "sync_output_ext": ".rvp",
        }
        result = build_device_path(
            row,
            "Music/{album_artist}/{album}",
            "{track_number:02d} - {title}{ext}",
        )
        assert result == os.path.join(
            "Videos",
            "TV Shows",
            "Recess",
            "Season 03",
            "S03E37 - Lord of the Nerds.rvp",
        )

    def test_show_video_rows_with_imdb_alias_fields_still_use_tv_folders(self):
        row = {
            "title": "Episode 3",
            "file_path": "/videos/6teen.S02E03.mkv",
            "media_type": "video",
            "video_kind": "TV",
            "type": "series",
            "series": "6teen",
            "season": "Season 2",
            "episode": "3",
        }
        result = build_device_path(
            row,
            "Music/{album_artist}/{album}",
            "{track_number:02d} - {title}{ext}",
        )
        assert result == os.path.join(
            "Videos",
            "TV Shows",
            "6teen",
            "Season 02",
            "S02E03 - Episode 3.mkv",
        )

    def test_normalize_video_row_for_sync_maps_imdb_series_aliases(self):
        row = {
            "title": "Pilot",
            "file_path": "/videos/kenny.vs.spenny/s2e1.mp4",
            "media_type": "video",
            "type": "tv",
            "series": "Kenny vs. Spenny",
            "season_number": "2",
            "episode_number": "1",
            "video_kind": "",
        }
        normalized = SyncEngine._normalize_video_row_for_sync(row)
        assert normalized["video_kind"] == "show"
        assert normalized["show_title"] == "Kenny vs. Spenny"
        assert normalized["album"] == "Season 2"
        assert normalized["artist"] == "Kenny vs. Spenny"
        assert normalized["album_artist"] == "Kenny vs. Spenny"


# ===========================================================================
# Tests: SyncPlan
# ===========================================================================

class TestSyncPlan:
    def test_empty_plan(self):
        plan = SyncPlan()
        assert plan.total_operations == 0
        assert plan.copy_count == 0
        assert plan.resync_count == 0
        assert "Nothing to sync" in plan.summary()

    def test_plan_with_copies(self):
        plan = SyncPlan()
        plan.to_copy.append(({"title": "A"}, "Music/A/B/01 - A.mp3"))
        plan.to_copy.append(({"title": "B"}, "Music/A/B/02 - B.mp3"))
        plan.total_bytes = 10 * 1024 * 1024

        assert plan.total_operations == 2
        assert plan.copy_count == 2
        assert "2 new tracks" in plan.summary()
        assert "10.0 MB" in plan.summary()

    def test_plan_with_resyncs(self):
        plan = SyncPlan()
        plan.to_resync.append(({"title": "C"}, "old/path.mp3", "new/path.mp3"))

        assert plan.resync_count == 1
        assert "1 tracks to update" in plan.summary()

    def test_plan_with_deletes(self):
        plan = SyncPlan()
        plan.to_delete.append("Music/Old/orphan.mp3")

        assert plan.total_operations == 1
        assert "duplicate device tracks" in plan.summary()


# ===========================================================================
# Tests: SyncWorker._copy_file
# ===========================================================================

class TestSyncWorkerCopy:
    def test_copy_creates_dest_directories(self, env):
        src = _make_source_file(env, "Art", "Alb", "Track01")
        dest = os.path.join(env["device_path"], "Music", "Art", "Alb", "01 - Track01.mp3")

        plan = SyncPlan()
        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        result, stats = worker._copy_file(src, dest)

        assert result is True
        assert stats["read_seconds"] >= 0.0
        assert stats["write_seconds"] >= 0.0
        assert os.path.isfile(dest)
        # Verify file content matches
        with open(src, "rb") as f:
            src_data = f.read()
        with open(dest, "rb") as f:
            dest_data = f.read()
        assert src_data == dest_data

    def test_copy_missing_source_returns_false(self, env):
        plan = SyncPlan()
        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        result, _stats = worker._copy_file("/nonexistent/file.mp3", "/tmp/out.mp3")
        assert result is False

    def test_copy_overwrites_existing(self, env):
        src = _make_source_file(env, "Art", "Alb", "Song", size=2048)
        dest = os.path.join(env["device_path"], "Music", "Art", "Alb", "Song.mp3")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as f:
            f.write(b"old data")

        plan = SyncPlan()
        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        result, _stats = worker._copy_file(src, dest)

        assert result is True
        assert os.path.getsize(dest) == 2048

    def test_no_temp_file_left_on_success(self, env):
        src = _make_source_file(env, "Art", "Alb", "Clean")
        dest = os.path.join(env["device_path"], "Music", "Art", "Alb", "Clean.mp3")

        plan = SyncPlan()
        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        worker._copy_file(src, dest)

        tmp_file = dest + ".rockpod_tmp"
        assert not os.path.exists(tmp_file)


# ===========================================================================
# Tests: SyncWorker.run  (new tracks)
# ===========================================================================

class TestSyncWorkerRun:
    def test_copies_new_tracks(self, env):
        src = _make_source_file(env, "Art", "Alb", "NewSong")
        rel_path = "Music/Art/Alb/01 - NewSong.mp3"
        row = {
            "id": 1,
            "file_path": src,
            "title": "NewSong",
            "artist": "Art",
            "album": "Alb",
            "metadata_hash": "mh1",
            "file_hash": "fh1",
        }

        plan = SyncPlan()
        plan.to_copy.append((row, rel_path))

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)

        results = {}

        def on_finish(copied, failed, skipped):
            results["copied"] = copied
            results["failed"] = failed
            results["skipped"] = skipped

        worker.finished.connect(on_finish)
        worker.run()

        assert results["copied"] == 1
        assert results["failed"] == 0
        dest = os.path.join(env["device_path"], rel_path)
        assert os.path.isfile(dest)

    def test_copies_local_lrc_sidecar_with_track(self, env):
        src = _make_source_file(env, "Art", "Alb", "TimedSong")
        with open(os.path.splitext(src)[0] + ".lrc", "w", encoding="utf-8") as f:
            f.write("[00:01.00]<00:01.10>Hello")

        rel_path = "Music/Art/Alb/01 - TimedSong.mp3"
        row = {
            "id": 101,
            "file_path": src,
            "title": "TimedSong",
            "artist": "Art",
            "album": "Alb",
            "metadata_hash": "mh101",
            "file_hash": "fh101",
        }

        plan = SyncPlan()
        plan.to_copy.append((row, rel_path))

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        worker.run()

        dest_lrc = os.path.join(env["device_path"], "Music/Art/Alb/01 - TimedSong.lrc")
        assert os.path.isfile(dest_lrc)
        with open(dest_lrc, "r", encoding="utf-8") as f:
            assert f.read() == "[00:01.00]<00:01.10>Hello"

    def test_resync_removes_stale_lrc_when_local_sidecar_is_missing(self, env):
        src = _make_source_file(env, "Art", "Alb", "NoLyrics")
        rel_path = "Music/Art/Alb/01 - NoLyrics.mp3"
        dest = os.path.join(env["device_path"], rel_path)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as f:
            f.write(b"old audio")
        with open(os.path.splitext(dest)[0] + ".lrc", "w", encoding="utf-8") as f:
            f.write("[00:01.00]stale")

        row = {
            "id": 102,
            "file_path": src,
            "title": "NoLyrics",
            "artist": "Art",
            "album": "Alb",
            "metadata_hash": "mh102",
            "file_hash": "fh102",
        }

        plan = SyncPlan()
        plan.to_resync.append((row, rel_path, rel_path))

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        worker.run()

        assert os.path.isfile(dest)
        assert not os.path.exists(os.path.splitext(dest)[0] + ".lrc")

    def test_copies_from_sync_source_path_when_present(self, env):
        src = _make_source_file(env, "Art", "Alb", "OriginalLossless", ext=".flac", size=1024)
        cached = os.path.join(env["tmp"], "cache", "device_transcodes", "mock", "OriginalLossless.mp3")
        os.makedirs(os.path.dirname(cached), exist_ok=True)
        with open(cached, "wb") as handle:
            handle.write(b"transcoded-mp3-data")

        rel_path = "Music/Art/Alb/01 - OriginalLossless.mp3"
        row = {
            "id": 11,
            "file_path": src,
            "sync_source_path": cached,
            "sync_output_ext": ".mp3",
            "title": "OriginalLossless",
            "artist": "Art",
            "album": "Alb",
            "metadata_hash": "mh_sync",
            "file_hash": "fh_sync",
        }

        plan = SyncPlan()
        plan.to_copy.append((row, rel_path))

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        worker.run()

        dest = os.path.join(env["device_path"], rel_path)
        assert os.path.isfile(dest)
        with open(dest, "rb") as handle:
            assert handle.read() == b"transcoded-mp3-data"
        with open(src, "rb") as handle:
            assert handle.read() != b"transcoded-mp3-data"

    def test_copies_video_rvp_bundle_with_device_basename(self, env):
        source_video = _make_source_file(env, "Videos", "Src", "Clip", ext=".mp4", size=128)
        cache_dir = os.path.join(env["tmp"], "cache", "device_video_rvp", "mock")
        os.makedirs(cache_dir, exist_ok=True)
        marker = os.path.join(cache_dir, "cache-name.rvp")
        yuv = os.path.join(cache_dir, "cache-name.yuv")
        pcm = os.path.join(cache_dir, "cache-name.pcm")
        with open(marker, "w", encoding="utf-8") as handle:
            handle.write(VideoRvpTranscoder.marker_text("cache-name.rvp"))
        with open(yuv, "wb") as handle:
            handle.write(b"yuv-data")
        with open(pcm, "wb") as handle:
            handle.write(b"pcm-data")

        rel_path = os.path.join("Videos", "Downloaded", "Clip.rvp")
        row = {
            "id": 303,
            "file_path": source_video,
            "sync_source_path": marker,
            "sync_output_ext": ".rvp",
            "sync_video_bundle_paths": {"rvp": marker, "yuv": yuv, "pcm": pcm},
            "sync_video_bundle_sizes": {"rvp": os.path.getsize(marker), "yuv": 8, "pcm": 8},
            "title": "Clip",
            "media_type": "video",
            "metadata_hash": "mh_video",
            "file_hash": "fh_video",
        }

        plan = SyncPlan()
        plan.to_copy.append((row, rel_path))
        stale_mpg = os.path.join(env["device_path"], "Videos", "Downloaded", "Clip.mpg")
        stale_seg = os.path.join(env["device_path"], "Videos", "Downloaded", "Clip.seg01.yuv")
        os.makedirs(os.path.dirname(stale_mpg), exist_ok=True)
        with open(stale_mpg, "wb") as handle:
            handle.write(b"legacy-mpeg")
        with open(stale_seg, "wb") as handle:
            handle.write(b"stale-segment")
        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        worker.run()

        dest = os.path.join(env["device_path"], rel_path)
        assert os.path.isfile(dest)
        assert os.path.isfile(os.path.splitext(dest)[0] + ".yuv")
        assert os.path.isfile(os.path.splitext(dest)[0] + ".pcm")
        assert not os.path.exists(stale_mpg)
        assert not os.path.exists(stale_seg)
        with open(dest, "r", encoding="utf-8") as handle:
            marker_text = handle.read()
        assert "video=Clip.yuv" in marker_text
        assert "audio=Clip.pcm" in marker_text
        with open(os.path.splitext(dest)[0] + ".yuv", "rb") as handle:
            assert handle.read() == b"yuv-data"

    def test_resync_replaces_old_file(self, env):
        src = _make_source_file(env, "Art", "Alb", "Updated", size=4096)
        old_rel = "Music/Art/OldAlb/01 - Updated.mp3"
        new_rel = "Music/Art/Alb/01 - Updated.mp3"

        # Place a file at the old location
        old_full = os.path.join(env["device_path"], old_rel)
        os.makedirs(os.path.dirname(old_full), exist_ok=True)
        with open(old_full, "wb") as f:
            f.write(b"old version")
        old_lrc = os.path.splitext(old_full)[0] + ".lrc"
        with open(old_lrc, "w", encoding="utf-8") as f:
            f.write("lyrics")

        row = {
            "id": 2,
            "file_path": src,
            "title": "Updated",
            "artist": "Art",
            "album": "Alb",
            "metadata_hash": "mh2",
            "file_hash": "fh2",
        }

        plan = SyncPlan()
        plan.to_resync.append((row, old_rel, new_rel))

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)

        results = {}
        worker.finished.connect(lambda c, f, s: results.update(copied=c, failed=f, skipped=s))
        worker.run()

        assert results["copied"] == 1
        # Old file should be removed
        assert not os.path.isfile(old_full)
        assert not os.path.isfile(old_lrc)
        # New file should exist with correct size
        new_full = os.path.join(env["device_path"], new_rel)
        assert os.path.isfile(new_full)
        assert os.path.getsize(new_full) == 4096

    def test_resync_keeps_old_file_when_new_copy_fails(self, env, monkeypatch):
        src = _make_source_file(env, "Art", "Alb", "UpdatedFail", size=4096)
        old_rel = "Music/Art/OldAlb/01 - UpdatedFail.mp3"
        new_rel = "Music/Art/Alb/01 - UpdatedFail.mp3"

        old_full = os.path.join(env["device_path"], old_rel)
        os.makedirs(os.path.dirname(old_full), exist_ok=True)
        with open(old_full, "wb") as f:
            f.write(b"old version")
        old_lrc = os.path.splitext(old_full)[0] + ".lrc"
        with open(old_lrc, "w", encoding="utf-8") as f:
            f.write("lyrics")

        row = {
            "id": 22,
            "file_path": src,
            "title": "UpdatedFail",
            "artist": "Art",
            "album": "Alb",
            "metadata_hash": "mh22",
            "file_hash": "fh22",
        }
        plan = SyncPlan()
        plan.to_resync.append((row, old_rel, new_rel))

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        monkeypatch.setattr(worker, "_copy_file", lambda src, dest, created_dirs=None: (False, {}))

        results = {}
        worker.finished.connect(lambda c, f, s: results.update(copied=c, failed=f, skipped=s))
        worker.run()

        assert results["copied"] == 0
        assert results["failed"] == 1
        assert os.path.isfile(old_full)
        assert os.path.isfile(old_lrc)
        assert not os.path.exists(os.path.join(env["device_path"], new_rel))

    def test_worker_skips_rockbox_list_manifests_after_media_copy_failure(self, env):
        manifest_src = os.path.join(env["tmp"], "index.tsv")
        with open(manifest_src, "w", encoding="utf-8") as handle:
            handle.write("album_id\tthumb\tslide\tartist\talbum\tgroup_key\tdevice_dirs\n")

        plan = SyncPlan()
        plan.to_copy.append((
            {
                "id": 999,
                "file_path": os.path.join(env["tmp"], "missing.mp3"),
                "title": "Missing",
                "artist": "Art",
                "album": "Alb",
                "metadata_hash": "mh_missing",
                "file_hash": "fh_missing",
            },
            "Music/Art/Alb/01 - Missing.mp3",
        ))
        plan.artwork_to_copy.append((
            manifest_src,
            os.path.join(ALBUM_LIST_DEVICE_DIR, "index.tsv"),
            "albumlist_manifest",
        ))

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        results = {}
        worker.finished.connect(lambda c, f, s: results.update(copied=c, failed=f, skipped=s))
        worker.run()

        assert results == {"copied": 0, "failed": 1, "skipped": 1}
        assert not os.path.exists(os.path.join(env["device_path"], ALBUM_LIST_DEVICE_DIR, "index.tsv"))

    def test_cancel_stops_early(self, env):
        """Cancelling before run starts should emit cancelled and not copy."""
        plan = SyncPlan()
        for i in range(5):
            src = _make_source_file(env, "Art", "Alb", f"Track{i}")
            plan.to_copy.append((
                {"id": i + 1, "file_path": src, "title": f"Track{i}",
                 "metadata_hash": "", "file_hash": ""},
                f"Music/Art/Alb/Track{i}.mp3",
            ))

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        worker.cancel()  # cancel immediately

        cancelled_flag = []
        worker.cancelled.connect(lambda: cancelled_flag.append(True))
        worker.run()

        assert len(cancelled_flag) == 1

    def test_updates_db_after_copy(self, env):
        """After a successful copy, mark_synced should be called on the DB."""
        src = _make_source_file(env, "Art", "Alb", "DBTest")
        _insert_track(env["db"], "DBTest", "Art", "Alb",
                       file_path=src, metadata_hash="mh_db", file_hash="fh_db")
        row = dict(env["db"].get_track_by_path(src))

        plan = SyncPlan()
        rel = "Music/Art/Alb/01 - DBTest.mp3"
        plan.to_copy.append((row, rel))

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        worker.run()

        updated = env["db"].get_track_by_id(row["id"])
        assert updated["synced_to_device"] == 1
        assert updated["device_path"] == rel
        assert updated["last_synced_metadata_hash"] == "mh_db"
        assert updated["last_synced_file_hash"] == "fh_db"

    def test_deletes_duplicate_files_queued_in_plan(self, env):
        rel_path = "Music/Art/Alb/01 - Duplicate.mp3"
        full_path = os.path.join(env["device_path"], rel_path)
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, "wb") as handle:
            handle.write(b"duplicate")
        lrc_path = os.path.splitext(full_path)[0] + ".lrc"
        with open(lrc_path, "w", encoding="utf-8") as handle:
            handle.write("lyrics")

        plan = SyncPlan()
        plan.to_delete.append(rel_path)

        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        results = {}
        worker.finished.connect(lambda c, f, s: results.update(copied=c, failed=f, skipped=s))
        worker.run()

        assert results["failed"] == 0
        assert not os.path.exists(full_path)
        assert not os.path.exists(lrc_path)

    def test_worker_records_execution_profile(self, env):
        src = _make_source_file(env, "Art", "Alb", "Profiled")
        row = {
            "id": 7,
            "file_path": src,
            "title": "Profiled",
            "artist": "Art",
            "album": "Alb",
            "metadata_hash": "mh7",
            "file_hash": "fh7",
        }
        plan = SyncPlan()
        plan.plan_profile = {"build_seconds": 0.01, "artwork_generation_seconds": 0.0}
        plan.to_copy.append((row, "Music/Art/Alb/01 - Profiled.mp3"))
        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        worker.run()

        assert plan.execution_profile["total_seconds"] >= 0.0
        assert plan.execution_profile["track_copy_seconds"] >= 0.0
        assert plan.execution_profile["db_finalize_seconds"] >= 0.0

    def test_worker_clears_device_trash(self, env):
        trash_dir = os.path.join(env["device_path"], ".Trash-1000", "files", "Old Album")
        os.makedirs(trash_dir, exist_ok=True)
        with open(os.path.join(trash_dir, "01 - stale.mp3"), "wb") as handle:
            handle.write(b"stale")

        plan = SyncPlan()
        worker = SyncWorker(plan, env["device_path"], env["config"].db_path)
        results = {}
        worker.finished.connect(lambda c, f, s: results.update(copied=c, failed=f, skipped=s))
        worker.run()

        assert results["failed"] == 0
        assert not os.path.exists(os.path.join(env["device_path"], ".Trash-1000"))


class TestSyncTrashHelpers:
    def test_clear_device_trash_removes_root_trash_dirs(self, env):
        trash_root = os.path.join(env["device_path"], ".Trash-1000")
        os.makedirs(os.path.join(trash_root, "files"), exist_ok=True)
        kept_dir = os.path.join(env["device_path"], "Music")
        os.makedirs(kept_dir, exist_ok=True)

        removed = _clear_device_trash(env["device_path"])

        assert trash_root in removed
        assert not os.path.exists(trash_root)
        assert os.path.isdir(kept_dir)


# ===========================================================================
# Tests: SyncEngine.build_sync_plan (integration with matcher)
# ===========================================================================

class TestSyncEnginePlan:
    def _make_engine(self, env):
        config = env["config"]
        db = env["db"]
        detector = DeviceDetector(config)
        # Manually set the device instead of polling
        device = DeviceInfo(env["device_path"])
        device.name = "Test iPod"
        device.is_rockbox = True
        detector._current_device = device
        return SyncEngine(db, config, detector)

    def test_rockbox_playlist_track_ids_only_include_enabled_playlists(self, env):
        wanted_src = _make_source_file(env, "Run River North", "Wake Up", "Wake Up")
        ignored_src = _make_source_file(env, "Other", "Album", "Ignored")
        wanted = _insert_track(env["db"], "Wake Up", "Run River North", "Wake Up", file_path=wanted_src)
        ignored = _insert_track(env["db"], "Ignored", "Other", "Album", file_path=ignored_src)
        enabled_id = env["db"].create_playlist("Rani's Playlist")
        disabled_id = env["db"].create_playlist("Hidden Playlist")
        env["db"].add_track_to_playlist(enabled_id, wanted["id"])
        env["db"].add_track_to_playlist(disabled_id, ignored["id"])
        env["db"].set_playlist_sync_to_rockbox(disabled_id, False)
        env["db"].commit()

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)

        assert engine.rockbox_playlist_track_ids() == [wanted["id"]]

    def test_playlist_track_ids_plan_copies_missing_playlist_media(self, env):
        src = _make_source_file(env, "Run River North", "Wake Up", "Wake Up")
        track = _insert_track(env["db"], "Wake Up", "Run River North", "Wake Up", file_path=src)
        playlist_id = env["db"].create_playlist("Rani's Playlist")
        env["db"].add_track_to_playlist(playlist_id, track["id"])
        env["db"].commit()

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        plan = engine.build_sync_plan(track_ids=engine.rockbox_playlist_track_ids())

        assert plan.copy_count == 1
        row, rel_path = plan.to_copy[0]
        assert dict(row)["id"] == track["id"]
        assert rel_path == os.path.join("Music", "Run River North", "Wake Up", "01 - Wake Up.mp3")

    def test_plan_no_device(self, env):
        """Plan with no device connected should report an error."""
        config = env["config"]
        db = env["db"]
        detector = DeviceDetector(config)
        detector._current_device = None
        engine = SyncEngine(db, config, detector)

        plan = engine.build_sync_plan()
        assert len(plan.errors) > 0
        assert "No device" in plan.errors[0]

    def test_plan_empty_library(self, env):
        """Empty library should produce an empty plan."""
        engine = self._make_engine(env)
        plan = engine.build_sync_plan()

        assert plan.total_operations == 0

    def test_plan_new_tracks_detected(self, env):
        """Tracks in library but not on device should appear in to_copy."""
        src = _make_source_file(env, "Art", "Alb", "NewTrack")
        _insert_track(env["db"], "NewTrack", "Art", "Alb",
                       file_path=src, metadata_hash="unique_hash_1")

        engine = self._make_engine(env)
        plan = engine.build_sync_plan()

        assert plan.copy_count >= 1
        titles = [dict(r)["title"] if hasattr(r, "keys") else r["title"]
                  for r, _ in plan.to_copy]
        assert "NewTrack" in titles

    def test_plan_relinks_completed_file_after_interrupted_sync(self, env):
        src = _make_source_file(env, "Art", "Alb", "Interrupted", size=4096)
        row = _insert_track(
            env["db"],
            "Interrupted",
            "Art",
            "Alb",
            file_path=src,
            file_size=os.path.getsize(src),
            metadata_hash="meta_interrupted",
            file_hash="file_interrupted",
        )
        engine = self._make_engine(env)
        rel_path = build_device_path(
            dict(row),
            engine._config_value("device_music_template", "Music/{album_artist}/{album}"),
            engine._config_value("device_file_template", "{track_number:02d} - {title}{ext}"),
        )
        dest = os.path.join(env["device_path"], rel_path)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(src, dest)

        plan = engine.build_sync_plan()

        assert plan.to_copy == []
        assert plan.to_resync == []
        assert [(dict(item)["title"], rel) for item, rel in plan.preflight_linked] == [
            ("Interrupted", rel_path)
        ]
        refreshed = env["db"].get_track_by_path(src)
        assert refreshed["synced_to_device"] == 1
        assert refreshed["device_path"] == rel_path
        device_rows = [dict(item) for item in env["db"].get_all_device_tracks(engine.current_device_key)]
        assert [item["device_path"] for item in device_rows] == [rel_path]

    def test_plan_overwrites_mismatched_interrupted_file_without_duplicate_suffix(self, env):
        src = _make_source_file(env, "Art", "Alb", "Partial", size=4096)
        row = _insert_track(
            env["db"],
            "Partial",
            "Art",
            "Alb",
            file_path=src,
            file_size=os.path.getsize(src),
            metadata_hash="meta_partial",
        )
        engine = self._make_engine(env)
        rel_path = build_device_path(
            dict(row),
            engine._config_value("device_music_template", "Music/{album_artist}/{album}"),
            engine._config_value("device_file_template", "{track_number:02d} - {title}{ext}"),
        )
        dest = os.path.join(env["device_path"], rel_path)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as handle:
            handle.write(b"partial")

        plan = engine.build_sync_plan()

        assert plan.to_copy == []
        assert len(plan.to_resync) == 1
        assert plan.to_resync[0][1:] == (rel_path, rel_path)
        assert "(2)" not in plan.to_resync[0][2]

    def test_plan_removes_stale_sync_temp_files(self, env):
        temp_path = os.path.join(
            env["device_path"],
            "Music",
            "Art",
            "Alb",
            "01 - Old.mp3.rockpod_tmp",
        )
        os.makedirs(os.path.dirname(temp_path), exist_ok=True)
        with open(temp_path, "wb") as handle:
            handle.write(b"stale")

        engine = self._make_engine(env)
        plan = engine.build_sync_plan()

        assert not os.path.exists(temp_path)
        assert plan.preflight_removed == ["Music/Art/Alb/01 - Old.mp3.rockpod_tmp"]

    def test_plan_uses_transcoded_extension_when_audio_conversion_enabled(self, env, monkeypatch):
        src = _make_source_file(env, "Art", "Alb", "Lossless", ext=".flac")
        _insert_track(
            env["db"],
            "Lossless",
            "Art",
            "Alb",
            file_path=src,
            metadata_hash="src_meta",
            file_hash="src_file",
            codec="FLAC",
            bitrate=950,
        )
        env["config"].set("convert_audio_for_device", True)
        env["config"].set("audio_conversion_mode", "unsupported_or_lossless")
        env["config"].set("audio_conversion_codec", "mp3")
        env["config"].set("audio_conversion_bitrate_kbps", 160)

        engine = self._make_engine(env)

        def fake_prepare(track_row, device_key, settings):
            row = dict(track_row)
            row.update(
                {
                    "sync_source_path": os.path.join(env["tmp"], "cache", "device_transcodes", "mock", "Lossless.mp3"),
                    "sync_output_ext": ".mp3",
                    "sync_transcoded": True,
                    "bitrate": 160,
                    "codec": "MP3",
                    "file_size": 123456,
                    "file_hash": "transcoded_file_hash",
                    "metadata_hash": "transcoded_meta_hash",
                }
            )
            return row, {"converted": True, "cache_path": row["sync_source_path"], "reason": "transcoded"}

        monkeypatch.setattr(engine._audio_transcoder, "prepare_track_for_sync", fake_prepare)

        plan = engine.build_sync_plan()

        assert plan.transcode_count == 1
        assert plan.copy_count == 1
        row, rel_path = plan.to_copy[0]
        assert dict(row)["sync_source_path"].endswith("Lossless.mp3")
        assert rel_path.endswith(".mp3")
        assert dict(row)["metadata_hash"] == "transcoded_meta_hash"
        assert dict(row)["file_hash"] == "transcoded_file_hash"

    def test_plan_matched_track_not_copied(self, env):
        """A track that already matches a device track should not be in to_copy."""
        src = _make_source_file(env, "Art", "Alb", "Existing")
        _insert_track(env["db"], "Existing", "Art", "Alb",
                       file_path=src, metadata_hash="same_hash")

        engine = self._make_engine(env)
        device_key = engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(env["db"], "Existing", "Art", "Alb",
                              "Music/Art/Alb/01 - Existing.mp3",
                              device_id=device_key,
                              metadata_hash="same_hash")

        plan = engine.build_sync_plan()

        copy_titles = [dict(r)["title"] if hasattr(r, "keys") else r["title"]
                       for r, _ in plan.to_copy]
        assert "Existing" not in copy_titles
        assert plan.up_to_date_count == 1

    def test_plan_marks_extra_device_duplicates_for_deletion(self, env):
        src = _make_source_file(env, "Art", "Alb", "Existing")
        local = _insert_track(env["db"], "Existing", "Art", "Alb", file_path=src, metadata_hash="same_hash")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Existing.mp3",
            device_id=engine.current_device_key,
            metadata_hash="same_hash",
            local_track_id=local["id"],
        )
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Existing (2).mp3",
            device_id=engine.current_device_key,
            metadata_hash="same_hash_variant",
        )
        engine.load_cached_device_inventory(engine.current_device_key)

        plan = engine.build_sync_plan()

        assert plan.copy_count == 0
        assert plan.resync_count == 0
        assert plan.to_delete == ["Music/Art/Alb/01 - Existing (2).mp3"]

    def test_plan_allows_album_sized_duplicate_delete_batch(self, env):
        src = _make_source_file(env, "Art", "Alb", "Existing")
        local = _insert_track(env["db"], "Existing", "Art", "Alb", file_path=src, metadata_hash="same_hash")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Existing.mp3",
            device_id=engine.current_device_key,
            metadata_hash="same_hash",
            local_track_id=local["id"],
        )
        for suffix in range(2, 8):
            _insert_device_track(
                env["db"],
                "Existing",
                "Art",
                "Alb",
                f"Music/Art/Alb/01 - Existing ({suffix}).mp3",
                device_id=engine.current_device_key,
                metadata_hash=f"same_hash_{suffix}",
            )
        engine.load_cached_device_inventory(engine.current_device_key)

        plan = engine.build_sync_plan()

        assert plan.to_delete == [
            f"Music/Art/Alb/01 - Existing ({suffix}).mp3"
            for suffix in range(2, 8)
        ]
        assert plan.errors == []

    def test_plan_blocks_oversized_duplicate_delete_batch(self, env):
        src = _make_source_file(env, "Art", "Alb", "Existing")
        local = _insert_track(env["db"], "Existing", "Art", "Alb", file_path=src, metadata_hash="same_hash")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Existing.mp3",
            device_id=engine.current_device_key,
            metadata_hash="same_hash",
            local_track_id=local["id"],
        )
        for suffix in range(2, 54):
            _insert_device_track(
                env["db"],
                "Existing",
                "Art",
                "Alb",
                f"Music/Art/Alb/01 - Existing ({suffix}).mp3",
                device_id=engine.current_device_key,
                metadata_hash=f"same_hash_{suffix}",
            )
        engine.load_cached_device_inventory(engine.current_device_key)

        plan = engine.build_sync_plan()

        assert plan.to_delete == []
        assert any("safety limit" in error for error in plan.errors)

    def test_plan_skips_duplicate_deletes_when_limit_is_zero(self, env):
        env["config"].max_auto_duplicate_deletes_per_sync = 0
        src = _make_source_file(env, "Art", "Alb", "Existing")
        local = _insert_track(env["db"], "Existing", "Art", "Alb", file_path=src, metadata_hash="same_hash")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Existing.mp3",
            device_id=engine.current_device_key,
            metadata_hash="same_hash",
            local_track_id=local["id"],
        )
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Existing (2).mp3",
            device_id=engine.current_device_key,
            metadata_hash="same_hash_variant",
        )
        engine.load_cached_device_inventory(engine.current_device_key)

        plan = engine.build_sync_plan()

        assert plan.to_delete == []
        assert plan.errors == []

    def test_plan_blocks_duplicate_delete_without_auto_suffix(self, env):
        src = _make_source_file(env, "Art", "Alb", "Existing")
        local = _insert_track(env["db"], "Existing", "Art", "Alb", file_path=src, metadata_hash="same_hash")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Existing.mp3",
            device_id=engine.current_device_key,
            metadata_hash="same_hash",
            local_track_id=local["id"],
        )
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Existing copy.mp3",
            device_id=engine.current_device_key,
            metadata_hash="same_hash_copy",
        )
        engine.load_cached_device_inventory(engine.current_device_key)

        plan = engine.build_sync_plan()

        assert plan.to_delete == []
        assert any("did not look like RockPod duplicate files" in error for error in plan.errors)

    def test_looks_like_auto_duplicate_path_allows_rani_playlist(self):
        assert _looks_like_auto_duplicate_path("Music/Artist/Rani's playlist/01 - Song.mp3")

    def test_plan_allows_rani_playlist_duplicate_paths(self, env):
        src = _make_source_file(env, "Art", "Rani's playlist", "Existing")
        local = _insert_track(
            env["db"],
            "Existing",
            "Art",
            "Rani's playlist",
            file_path=src,
            metadata_hash="same_hash",
        )

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Rani's playlist",
            "Music/Art/Rani's playlist/01 - Existing.mp3",
            local_track_id=local["id"],
            device_id=engine.current_device_key,
            metadata_hash="same_hash",
        )
        _insert_device_track(
            env["db"],
            "Existing",
            "Art",
            "Rani's playlist",
            "Music/Art/Rani's playlist/01 - Existing.m4a",
            device_id=engine.current_device_key,
            metadata_hash="same_hash_copy",
        )
        engine.load_cached_device_inventory(engine.current_device_key)

        plan = engine.build_sync_plan()

        assert plan.to_delete == ["Music/Art/Rani's playlist/01 - Existing.m4a"]
        assert plan.errors == []

    def test_plan_matches_different_filename_without_duplicate(self, env):
        """Matching must use metadata identity, not matching filenames."""
        src = _make_source_file(env, "Art", "Alb", "Local Filename")
        _insert_track(env["db"], "Same Track", "Art", "Alb",
                       file_path=src, metadata_hash="new_hash")

        engine = self._make_engine(env)
        device_key = engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(env["db"], "Same Track", "Art", "Alb",
                              "Music/Art/Alb/01 - Totally Different.mp3",
                              device_id=device_key,
                              metadata_hash="new_hash")

        plan = engine.build_sync_plan()

        assert plan.copy_count == 0
        assert plan.resync_count == 0
        assert plan.up_to_date_count == 1

    def test_plan_resync_on_metadata_change(self, env):
        """A matched track with changed metadata_hash should appear in to_resync."""
        src = _make_source_file(env, "Art", "Alb", "Changed")
        _insert_track(env["db"], "Changed", "Art", "Alb",
                       file_path=src, metadata_hash="new_meta_hash",
                       synced_to_device=1,
                       last_synced_metadata_hash="old_meta_hash")

        engine = self._make_engine(env)
        device_key = engine.set_current_device(engine._device_detector.current_device)
        # Device track has old hash — matcher will match by metadata fields
        # (artist+album+title+duration), but needs_resync detects the hash
        # divergence on the local track.
        _insert_device_track(env["db"], "Changed", "Art", "Alb",
                              "Music/Art/Alb/01 - Changed.mp3",
                              device_id=device_key,
                              metadata_hash="old_meta_hash",
                              local_track_id=None)

        # Ensure resync is enabled
        env["config"].resync_metadata_changes = True
        plan = engine.build_sync_plan()

        resync_titles = [dict(r)["title"] if hasattr(r, "keys") else r["title"]
                         for r, _, _ in plan.to_resync]
        assert "Changed" in resync_titles

    def test_plan_resync_video_bundle_when_marker_or_settings_changed(self, env, monkeypatch):
        src = _make_source_file(env, "Show Artist", "Show Album", "Episode", ext=".mp4", size=2048)
        local_track = _insert_track(
            env["db"],
            "Episode",
            "Show Artist",
            "Show Album",
            file_path=src,
            media_type="video",
            video_kind="show",
            video_sync_category="TV Shows",
            metadata_hash="video_meta",
            file_hash="bundle_hash",
        )

        env["config"].set("resync_metadata_changes", False)
        engine = self._make_engine(env)
        device_key = engine.set_current_device(engine._device_detector.current_device)
        rel_path = os.path.join("Videos", "TV Shows", "Episode.rvp")
        device_mark_path = os.path.join(env["device_path"], rel_path)
        os.makedirs(os.path.dirname(device_mark_path), exist_ok=True)
        marker = os.path.basename(device_mark_path)
        with open(device_mark_path, "w", encoding="utf-8") as handle:
            handle.write(VideoRvpTranscoder.marker_text(marker, width=320, height=240, fps=20, sample_rate=44100))
        with open(os.path.join(env["device_path"], "Videos", "TV Shows", "Episode.yuv"), "wb") as handle:
            handle.write(b"\x00" * 6)
        with open(os.path.join(env["device_path"], "Videos", "TV Shows", "Episode.pcm"), "wb") as handle:
            handle.write(b"\x01" * 4)
        _insert_device_track(
            env["db"],
            "Episode",
            "Show Artist",
            "Show Album",
            rel_path,
            device_id=device_key,
            local_track_id=local_track["id"],
            metadata_hash="video_meta",
            file_hash="bundle_hash",
            file_size=10,
        )

        base = os.path.join(env["tmp"], "cache", "device_video_rvp", device_key)
        os.makedirs(base, exist_ok=True)
        local_bundle = os.path.join(base, "Episode.rvp")

        def fake_prepare(track_row, device_id):
            prepared = dict(track_row)
            prepared["sync_source_path"] = local_bundle
            prepared["sync_output_ext"] = ".rvp"
            prepared["sync_transcoded"] = True
            prepared["sync_video_bundle_paths"] = {
                "rvp": local_bundle,
                "yuv": os.path.splitext(local_bundle)[0] + ".yuv",
                "pcm": os.path.splitext(local_bundle)[0] + ".pcm",
            }
            prepared["sync_video_bundle_sizes"] = {"rvp": 512, "yuv": 6, "pcm": 4}
            prepared["sync_video_width"] = 256
            prepared["sync_video_height"] = 144
            prepared["sync_video_fps"] = 20
            prepared["sync_video_sample_rate"] = 44100
            prepared["sync_video_profile"] = "compact"
            prepared["sync_video_segments"] = []
            prepared["file_size"] = 522
            prepared["file_hash"] = "bundle_hash"
            prepared["metadata_hash"] = "video_meta"
            prepared["codec"] = "RVP"
            return prepared, {"converted": True, "cache_path": local_bundle, "reason": "rvp_video"}

        monkeypatch.setattr(engine._video_transcoder, "prepare_track_for_sync", fake_prepare)

        plan = engine.build_sync_plan()

        assert len(plan.to_resync) == 1
        resync_row = dict(plan.to_resync[0][0])
        assert resync_row["title"] == "Episode"
        assert resync_row["sync_output_ext"] == ".rvp"
        assert plan.to_resync[0][1] == rel_path

    def test_plan_reason_counts_include_update_breakdown(self, env, monkeypatch):
        src = _make_source_file(env, "Art", "Alb", "Changed", ext=".flac")
        local = _insert_track(
            env["db"],
            "Changed",
            "Art",
            "Alb",
            file_path=src,
            codec="flac",
            metadata_hash="new_meta_hash",
            file_hash="new_file_hash",
            synced_to_device=1,
            last_synced_metadata_hash="old_meta_hash",
            last_synced_file_hash="old_file_hash",
        )

        engine = self._make_engine(env)
        device_key = engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(
            env["db"],
            "Changed",
            "Art",
            "Alb",
            "Music/Old/Path/Changed.mp3",
            device_id=device_key,
            metadata_hash="old_meta_hash",
            file_hash="old_file_hash",
            local_track_id=local["id"],
        )
        env["config"].set("convert_audio_for_device", True)
        env["config"].set("audio_conversion_mode", "unsupported_or_lossless")
        env["config"].set("audio_conversion_codec", "mp3")

        def fake_prepare(track_row, device_key, settings):
            row = dict(track_row)
            row.update(
                {
                    "sync_source_path": os.path.join(env["tmp"], "cache", "device_transcodes", "mock", "Changed.mp3"),
                    "sync_output_ext": ".mp3",
                    "sync_transcoded": True,
                    "codec": "MP3",
                    "file_hash": "new_file_hash",
                    "metadata_hash": "new_meta_hash",
                }
            )
            return row, {"converted": True, "cache_path": row["sync_source_path"], "reason": "transcoded"}

        monkeypatch.setattr(engine._audio_transcoder, "prepare_track_for_sync", fake_prepare)

        plan = engine.build_sync_plan()

        assert plan.update_reason_counts["conversion required"] == 1
        assert plan.update_reason_counts["metadata changed"] == 1
        assert plan.update_reason_counts["file changed"] == 1
        assert plan.update_reason_counts["path changed"] == 1

    def test_not_on_device_detection_uses_current_device_index(self, env):
        src_a = _make_source_file(env, "Art", "Alb", "OnDevice")
        src_b = _make_source_file(env, "Art", "Alb", "Missing")
        _insert_track(env["db"], "OnDevice", "Art", "Alb",
                       file_path=src_a, metadata_hash="on_hash")
        _insert_track(env["db"], "Missing", "Art", "Alb",
                       file_path=src_b, metadata_hash="missing_hash")
        engine = self._make_engine(env)
        device_key = engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(env["db"], "OnDevice", "Art", "Alb",
                              "Music/Other/Name.mp3", device_id=device_key, metadata_hash="on_hash")

        missing = engine.get_not_on_device_tracks()

        assert [dict(row)["title"] for row in missing] == ["Missing"]

    def test_not_on_device_detection_ignores_other_cached_devices_when_no_current_key(self, env):
        src = _make_source_file(env, "Art", "Alb", "LocalOnly")
        _insert_track(env["db"], "LocalOnly", "Art", "Alb", file_path=src, metadata_hash="local_hash")
        _insert_device_track(
            env["db"],
            "OtherDeviceSong",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Other.mp3",
            device_id="other-device",
            metadata_hash="other_hash",
        )

        detector = DeviceDetector(env["config"])
        detector._current_device = None
        engine = SyncEngine(env["db"], env["config"], detector)

        assert engine.get_device_tracks() == []
        assert engine.get_not_on_device_tracks() == []

    def test_sync_status_counts_fall_back_when_device_links_are_missing(self, env):
        src_a = _make_source_file(env, "Art", "Alb", "OnDevice")
        src_b = _make_source_file(env, "Art", "Alb", "Missing")
        _insert_track(env["db"], "OnDevice", "Art", "Alb", file_path=src_a, metadata_hash="on_hash")
        _insert_track(env["db"], "Missing", "Art", "Alb", file_path=src_b, metadata_hash="missing_hash")
        engine = self._make_engine(env)
        device_key = engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(
            env["db"],
            "OnDevice",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - OnDevice.mp3",
            device_id=device_key,
            metadata_hash="on_hash",
            local_track_id=None,
        )
        engine.load_cached_device_inventory(device_key)
        counts = engine.get_sync_status_counts()

        assert counts["missing"] == 1

    def test_unscanned_device_inventory_does_not_report_library_as_missing(self, env):
        src = _make_source_file(env, "Art", "Alb", "Song")
        _insert_track(env["db"], "Song", "Art", "Alb", file_path=src, metadata_hash="song_hash")
        device_media = os.path.join(env["device_path"], "Music", "Art", "Alb", "01 - Song.mp3")
        os.makedirs(os.path.dirname(device_media), exist_ok=True)
        with open(device_media, "wb") as handle:
            handle.write(b"device-audio")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)

        counts = engine.get_sync_status_counts()

        assert counts == {"missing": 0, "resync": 0}
        assert engine.device_inventory_has_local_links() is False
        assert engine.get_not_on_device_tracks() == []

    def test_build_sync_plan_scans_device_when_inventory_has_never_been_verified(self, env, monkeypatch):
        src = _make_source_file(env, "Art", "Alb", "Existing")
        _insert_track(env["db"], "Existing", "Art", "Alb", file_path=src, metadata_hash="same_hash")
        device_media = os.path.join(env["device_path"], "Music", "Art", "Alb", "01 - Existing.mp3")
        os.makedirs(os.path.dirname(device_media), exist_ok=True)
        with open(device_media, "wb") as handle:
            handle.write(b"device-audio")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        scans = []

        def fake_scan(force_full=False):
            scans.append(force_full)
            _insert_device_track(
                env["db"],
                "Existing",
                "Art",
                "Alb",
                "Music/Art/Alb/01 - Existing.mp3",
                device_id=engine.current_device_key,
                metadata_hash="same_hash",
            )
            env["db"].mark_device_scanned(engine.current_device_key)
            env["db"].commit()
            engine.load_cached_device_inventory()
            return 1

        monkeypatch.setattr(engine, "scan_device", fake_scan)

        plan = engine.build_sync_plan()

        assert scans == [False]
        assert plan.copy_count == 0
        assert plan.up_to_date_count == 1

    def test_build_sync_plan_scans_when_cached_inventory_is_partial_and_unverified(self, env, monkeypatch):
        src = _make_source_file(env, "Art", "Alb", "Existing")
        _insert_track(env["db"], "Existing", "Art", "Alb", file_path=src, metadata_hash="same_hash")
        device_media = os.path.join(env["device_path"], "Music", "Art", "Alb", "01 - Existing.mp3")
        os.makedirs(os.path.dirname(device_media), exist_ok=True)
        with open(device_media, "wb") as handle:
            handle.write(b"device-audio")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        _insert_device_track(
            env["db"],
            "Unrelated",
            "Other",
            "Other",
            "Music/Other/Other/01 - Unrelated.mp3",
            device_id=engine.current_device_key,
        )
        engine.load_cached_device_inventory(engine.current_device_key)
        scans = []

        def fake_scan(force_full=False):
            scans.append(force_full)
            env["db"].clear_device_tracks(engine.current_device_key)
            _insert_device_track(
                env["db"],
                "Existing",
                "Art",
                "Alb",
                "Music/Art/Alb/01 - Existing.mp3",
                device_id=engine.current_device_key,
                metadata_hash="same_hash",
            )
            env["db"].mark_device_scanned(engine.current_device_key)
            env["db"].commit()
            engine.load_cached_device_inventory(engine.current_device_key)
            return 1

        monkeypatch.setattr(engine, "scan_device", fake_scan)

        plan = engine.build_sync_plan()

        assert scans == [False]
        assert plan.copy_count == 0
        assert plan.up_to_date_count == 1

    def test_build_sync_plan_scans_when_rockbox_db_is_newer_than_cached_scan(self, env, monkeypatch):
        src = _make_source_file(env, "Art", "Alb", "Existing")
        _insert_track(env["db"], "Existing", "Art", "Alb", file_path=src, metadata_hash="same_hash")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        env["db"].mark_device_scanned(engine.current_device_key)
        env["db"].commit()
        engine.load_cached_device_inventory()
        scans = []

        monkeypatch.setattr(
            "services.sync_engine.detect_rockbox_database_state",
            lambda device, row=None: {
                "database_present": True,
                "database_latest_mtime": "2099-01-01T00:00:00",
            },
        )

        def fake_scan(force_full=False):
            scans.append(force_full)
            _insert_device_track(
                env["db"],
                "Existing",
                "Art",
                "Alb",
                "Music/Art/Alb/01 - Existing.mp3",
                device_id=engine.current_device_key,
                metadata_hash="same_hash",
            )
            env["db"].mark_device_scanned(engine.current_device_key)
            env["db"].commit()
            engine.load_cached_device_inventory()
            return 1

        monkeypatch.setattr(engine, "scan_device", fake_scan)

        plan = engine.build_sync_plan()

        assert scans == [False]
        assert plan.copy_count == 0
        assert plan.up_to_date_count == 1

    def test_missing_mount_path_does_not_report_full_library_as_missing(self, env):
        src = _make_source_file(env, "Art", "Alb", "Song")
        _insert_track(env["db"], "Song", "Art", "Alb", file_path=src, metadata_hash="song_hash")

        engine = self._make_engine(env)
        engine.set_current_device(engine._device_detector.current_device)
        shutil.rmtree(env["device_path"])

        counts = engine.get_sync_status_counts()
        plan = engine.build_sync_plan()

        assert counts == {"missing": 0, "resync": 0}
        assert plan.total_operations == 0
        assert "Device mount path is unavailable" in plan.errors

    def test_plan_generates_unique_paths_for_collisions(self, env):
        src_a = _make_source_file(env, "Art", "Alb", "CollisionA")
        src_b = _make_source_file(env, "Art", "Alb", "CollisionB")
        _insert_track(env["db"], "Same Title", "Art", "Alb",
                       file_path=src_a, metadata_hash="hash_a")
        _insert_track(env["db"], "Same Title", "Art", "Alb",
                       file_path=src_b, metadata_hash="hash_b")

        engine = self._make_engine(env)
        plan = engine.build_sync_plan()
        paths = [path for _row, path in plan.to_copy]

        assert len(paths) == 2
        assert len(set(paths)) == 2

    def test_plan_replaces_unlinked_device_file_at_same_target_path(self, env):
        src = _make_source_file(env, "Art", "Alb", "Existing")
        _insert_track(
            env["db"],
            "Existing",
            "Art",
            "Alb",
            file_path=src,
            metadata_hash="local_hash",
            track_number=1,
        )

        engine = self._make_engine(env)
        device_key = engine.set_current_device(engine._device_detector.current_device)
        rel_path = build_device_path(
            dict(env["db"].get_track_by_path(src)),
            engine._config_value("device_music_template", "Music/{album_artist}/{album}"),
            engine._config_value("device_file_template", "{track_number:02d} - {title}{ext}"),
        )
        _insert_device_track(
            env["db"],
            "01 - Existing",
            "",
            "",
            rel_path,
            device_id=device_key,
            metadata_hash="broken_hash",
            local_track_id=None,
            track_number=None,
            album_artist="",
        )

        plan = engine.build_sync_plan()

        assert plan.copy_count == 0
        assert plan.resync_count == 1
        assert plan.to_resync[0][1:] == (rel_path, rel_path)

    def test_force_full_recopy_everything(self, env):
        """Force full mode should re-copy every library track."""
        for i in range(3):
            src = _make_source_file(env, "Art", "Alb", f"Force{i}")
            _insert_track(env["db"], f"Force{i}", "Art", "Alb",
                           file_path=src, metadata_hash=f"fh{i}")

        engine = self._make_engine(env)
        plan = engine.build_sync_plan(force_full=True)

        assert plan.copy_count + plan.resync_count == 3

    def test_plan_selected_tracks_only(self, env):
        """Passing track_ids should limit the plan to those tracks."""
        ids = []
        for i in range(4):
            src = _make_source_file(env, "Art", "Alb", f"Sel{i}")
            row = _insert_track(env["db"], f"Sel{i}", "Art", "Alb",
                                 file_path=src, metadata_hash=f"sel{i}")
            ids.append(row["id"])

        engine = self._make_engine(env)
        plan = engine.build_sync_plan(track_ids={ids[0], ids[2]}, force_full=True)

        assert plan.copy_count + plan.resync_count == 2

    def test_plan_selected_video_ids_uses_video_rows_and_exports_list_thumbnail(self, env, monkeypatch):
        video_dir = os.path.join(env["tmp"], "Videos", "YouTube")
        os.makedirs(video_dir, exist_ok=True)
        src = os.path.join(video_dir, "Real Movie.mpg")
        with open(src, "wb") as handle:
            handle.write(os.urandom(1024))
        Image.new("RGB", (600, 900), color=(30, 80, 120)).save(
            os.path.join(video_dir, "Real Movie.jpg"),
            "JPEG",
        )
        row = _insert_track(
            env["db"],
            "youtube",
            "",
            "youtube",
            file_path=src,
            metadata_hash="video_meta",
            file_hash="video_file",
            media_type="video",
            codec="mpeg",
            video_kind="movie",
        )

        config = env["config"]
        detector = DeviceDetector(config)
        device = DeviceInfo(env["device_path"])
        device.name = "Test iPod"
        device.is_rockbox = True
        detector._current_device = device
        audio_src = _make_source_file(env, "Art", "Alb", "Duped")
        audio = _insert_track(
            env["db"],
            "Duped",
            "Art",
            "Alb",
            file_path=audio_src,
            metadata_hash="dup_hash",
        )
        manager = ArtworkManager(config.artwork_cache_dir, config=config)
        try:
            engine = SyncEngine(env["db"], config, detector, manager)
            engine.set_current_device(device)

            def fake_prepare_video(track_row, device_key):
                prepared = dict(track_row)
                cache_dir = os.path.join(env["tmp"], "cache", "device_video_rvp", device_key)
                marker = os.path.join(cache_dir, "Real Movie.rvp")
                prepared.update(
                    {
                        "sync_source_path": marker,
                        "sync_output_ext": ".rvp",
                        "sync_transcoded": True,
                        "sync_video_bundle_paths": {
                            "rvp": marker,
                            "yuv": os.path.splitext(marker)[0] + ".yuv",
                            "pcm": os.path.splitext(marker)[0] + ".pcm",
                        },
                        "sync_video_bundle_sizes": {"rvp": 128, "yuv": 2048, "pcm": 1024},
                        "file_size": 3200,
                        "file_hash": "rvp_file",
                        "metadata_hash": "rvp_meta",
                        "codec": "RVP",
                    }
                )
                return prepared, {"converted": True, "cache_path": marker, "reason": "rvp_video"}

            monkeypatch.setattr(engine._video_transcoder, "prepare_track_for_sync", fake_prepare_video)
            _insert_device_track(
                env["db"],
                "Duped",
                "Art",
                "Alb",
                os.path.join("Music", "Art", "Alb", "01 - Duped.mp3"),
                device_id=engine.current_device_key,
                metadata_hash="dup_hash",
                local_track_id=audio["id"],
            )
            _insert_device_track(
                env["db"],
                "Duped",
                "Art",
                "Alb",
                os.path.join("Music", "Art", "Alb", "01 - Duped (2).mp3"),
                device_id=engine.current_device_key,
                metadata_hash="dup_hash_variant",
            )
            engine.load_cached_device_inventory(engine.current_device_key)
            plan = engine.build_sync_plan(track_ids={row["id"]})
        finally:
            manager.shutdown()

        assert plan.copy_count == 1
        assert plan.transcode_count == 1
        assert plan.to_delete == []
        planned_row, rel_path = plan.to_copy[0]
        assert planned_row["title"] == "Real Movie"
        assert planned_row["sync_output_ext"] == ".rvp"
        assert rel_path == os.path.join("Videos", "Downloaded", "Real Movie.rvp")

        artwork_paths = {item[1] for item in plan.artwork_to_copy}
        assert os.path.join(ALBUM_LIST_DEVICE_DIR, "index.tsv") not in artwork_paths
        assert os.path.join(VIDEO_LIST_DEVICE_DIR, "index.tsv") in artwork_paths
        assert any(
            rel.startswith(VIDEO_LIST_THUMB_DEVICE_DIR) and rel.endswith(".bmp")
            for rel in artwork_paths
        )


# ===========================================================================
# Tests: SyncEngine.scan_device
# ===========================================================================

class TestScanDevice:
    def _make_engine(self, env):
        config = env["config"]
        db = env["db"]
        detector = DeviceDetector(config)
        device = DeviceInfo(env["device_path"])
        device.name = "Test iPod"
        device.is_rockbox = True
        detector._current_device = device
        return SyncEngine(db, config, detector)

    def test_scan_empty_device(self, env):
        engine = self._make_engine(env)
        count = engine.scan_device()
        assert count == 0

    def test_scan_no_device_returns_zero(self, env):
        config = env["config"]
        db = env["db"]
        detector = DeviceDetector(config)
        detector._current_device = None
        engine = SyncEngine(db, config, detector)
        assert engine.scan_device() == 0

    def test_scan_device_uses_cache_for_unchanged_files(self, env, monkeypatch):
        from models.track import Track
        from services import sync_engine

        engine = self._make_engine(env)
        full_path = os.path.join(env["device_path"], "Music", "Art", "Alb", "Cached.mp3")
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, "wb") as f:
            f.write(b"device-audio")

        reads = []

        def fake_read_metadata(path):
            reads.append(path)
            stat = os.stat(path)
            return Track(
                file_path=path,
                title="Cached",
                artist="Art",
                album="Alb",
                duration=120.0,
                file_size=stat.st_size,
                metadata_hash="cached_hash",
            )

        monkeypatch.setattr(sync_engine, "read_metadata", fake_read_metadata)

        assert engine.scan_device() == 1
        assert engine.scan_device() == 1
        assert reads == [full_path]

    def test_scan_device_hashes_only_in_strict_mode(self, env, monkeypatch):
        from models.track import Track
        from services import sync_engine

        full_path = os.path.join(env["device_path"], "Music", "Art", "Alb", "HashMe.mp3")
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, "wb") as f:
            f.write(b"device-audio")

        def fake_read_metadata(path):
            stat = os.stat(path)
            return Track(
                file_path=path,
                title="HashMe",
                artist="Art",
                album="Alb",
                duration=120.0,
                file_size=stat.st_size,
                metadata_hash="hash_me_meta",
            )

        hash_calls = []
        monkeypatch.setattr(sync_engine, "read_metadata", fake_read_metadata)
        monkeypatch.setattr(
            sync_engine,
            "compute_file_hash",
            lambda path: hash_calls.append(path) or "device_file_hash",
        )

        env["config"].duplicate_strictness = "metadata_only"
        engine = self._make_engine(env)
        assert engine.scan_device() == 1
        assert hash_calls == []

        env["config"].duplicate_strictness = "metadata_and_hash"
        assert engine.scan_device() == 1
        assert hash_calls == [full_path]
        assert env["db"].get_all_device_tracks()[0]["file_hash"] == "device_file_hash"

    def test_build_sync_plan_profiles_timing(self, env):
        src = _make_source_file(env, "Art", "Alb", "PlanProfile")
        _insert_track(env["db"], "PlanProfile", "Art", "Alb",
                       file_path=src, metadata_hash="profile_hash")

        engine = self._make_engine(env)
        plan = engine.build_sync_plan()

        assert plan.plan_profile["build_seconds"] >= 0.0
        assert plan.plan_profile["local_fetch_seconds"] >= 0.0
        assert plan.plan_profile["device_fetch_seconds"] >= 0.0
        assert plan.plan_profile["artwork_generation_seconds"] >= 0.0


class TestSyncCacheCleanup:
    def _make_engine(self, env):
        config = env["config"]
        db = env["db"]
        detector = DeviceDetector(config)
        device = DeviceInfo(env["device_path"])
        device.name = "Test iPod"
        device.is_rockbox = True
        detector._current_device = device
        engine = SyncEngine(db, config, detector)
        engine.set_current_device(device)
        return engine

    def test_cleanup_local_sync_cache_keeps_current_device_cache(self, env):
        config = env["config"]
        config.set("convert_audio_for_device", True)
        config.set("audio_conversion_codec", "mp3")
        config.set("audio_conversion_mode", "unsupported_or_lossless")

        engine = self._make_engine(env)

        audio_src = _make_source_file(env, "Artist A", "Album A", "Sync Audio", ext=".flac", size=1024)
        video_src = _make_source_file(env, "Artist V", "Album V", "Sync Video", ext=".mp4", size=128)
        audio_track = _insert_track(
            env["db"],
            "Sync Audio",
            "Artist A",
            "Album A",
            file_path=audio_src,
            media_type="audio",
            codec="flac",
            bitrate=1411,
            metadata_hash="audio-meta",
            file_hash="audio-file",
        )
        video_track = _insert_track(
            env["db"],
            "Sync Video",
            "Artist V",
            "Album V",
            file_path=video_src,
            media_type="video",
            codec="mp4",
            video_kind="movie",
            metadata_hash="video-meta",
            file_hash="video-file",
        )
        _insert_device_track(
            env["db"],
            "Sync Audio",
            "Artist A",
            "Album A",
            os.path.join("Music", "Artist A", "Album A", "01 - Sync Audio.mp3"),
            device_id=engine.current_device_key,
            local_track_id=audio_track["id"],
            metadata_hash="audio-meta",
        )
        _insert_device_track(
            env["db"],
            "Sync Video",
            "Artist V",
            "Album V",
            os.path.join("Videos", "Downloaded", "Sync Video.rvp"),
            device_id=engine.current_device_key,
            local_track_id=video_track["id"],
            metadata_hash="video-meta",
        )

        settings = engine._audio_conversion_settings()
        audio_cache_path = engine._audio_transcoder._cache_path(
            audio_src,
            dict(audio_track),
            engine.current_device_key,
            engine._audio_transcoder._normalize_codec(settings.get("target_codec")),
            engine._audio_transcoder._normalize_bitrate(settings.get("target_bitrate_kbps")),
        )
        os.makedirs(os.path.dirname(audio_cache_path), exist_ok=True)
        with open(audio_cache_path, "wb") as handle:
            handle.write(b"audio-rvp")

        video_marker = engine._video_transcoder._cache_marker_path(video_src, dict(video_track), engine.current_device_key)
        video_base = os.path.splitext(video_marker)[0]
        os.makedirs(os.path.dirname(video_marker), exist_ok=True)
        with open(video_marker, "w", encoding="utf-8") as handle:
            handle.write(VideoRvpTranscoder.marker_text(os.path.basename(video_marker)))
        with open(video_base + ".yuv", "wb") as handle:
            handle.write(b"yuv")
        with open(video_base + ".pcm", "wb") as handle:
            handle.write(b"pcm")

        stale_audio = os.path.join(env["tmp"], "cache", "device_transcodes", engine.current_device_key, "stale-old.mp3")
        stale_video = os.path.join(env["tmp"], "cache", "device_video_rvp", engine.current_device_key, "stale-old.rvp")
        stale_other_device = os.path.join(
            env["tmp"],
            "cache",
            "device_transcodes",
            "other-device",
            "other-device-old.mp3",
        )
        os.makedirs(os.path.dirname(stale_audio), exist_ok=True)
        os.makedirs(os.path.dirname(stale_video), exist_ok=True)
        with open(stale_audio, "wb") as handle:
            handle.write(b"stale-audio")
        with open(stale_video, "wb") as handle:
            handle.write(b"stale-video")
        os.makedirs(os.path.dirname(stale_other_device), exist_ok=True)
        with open(stale_other_device, "wb") as handle:
            handle.write(b"other-device-audio")

        result = engine.cleanup_local_sync_cache(device_key=engine.current_device_key)

        assert os.path.isfile(audio_cache_path)
        assert os.path.isfile(video_marker)
        assert os.path.isfile(video_base + ".yuv")
        assert os.path.isfile(video_base + ".pcm")
        assert not os.path.exists(stale_audio)
        assert not os.path.exists(stale_video)
        assert os.path.isfile(stale_other_device)
        assert result["kept"]["audio"] == 1
        assert result["kept"]["video"] == 3
        assert set(result["removed"]) == {stale_audio, stale_video}

    def test_cleanup_segmented_video_cache_keeps_only_needed_segments(self, env):
        engine = self._make_engine(env)

        video_src = _make_source_file(env, "Artist V", "Album V", "Segment Video", ext=".mp4", size=128)
        video_track = _insert_track(
            env["db"],
            "Segment Video",
            "Artist V",
            "Album V",
            file_path=video_src,
            media_type="video",
            codec="mp4",
            video_kind="movie",
            metadata_hash="segment-meta",
            file_hash="segment-file",
        )
        _insert_device_track(
            env["db"],
            "Segment Video",
            "Artist V",
            "Album V",
            os.path.join("Videos", "Downloaded", "Segment Video.rvp"),
            device_id=engine.current_device_key,
            local_track_id=video_track["id"],
            metadata_hash="segment-meta",
        )

        video_marker = engine._video_transcoder._cache_marker_path(video_src, dict(video_track), engine.current_device_key)
        marker_base = os.path.splitext(video_marker)[0]
        os.makedirs(os.path.dirname(video_marker), exist_ok=True)
        with open(video_marker, "w", encoding="utf-8") as handle:
            handle.write(
                VideoRvpTranscoder.segmented_marker_text(
                    [("segment.seg01.yuv", "segment.seg01.pcm")],
                    width=320,
                    height=240,
                )
            )
        base_video = marker_base + ".yuv"
        base_audio = marker_base + ".pcm"
        segment_video = os.path.join(os.path.dirname(marker_base), "segment.seg01.yuv")
        segment_audio = os.path.join(os.path.dirname(marker_base), "segment.seg01.pcm")
        with open(base_video, "wb") as handle:
            handle.write(b"stale yuv")
        with open(base_audio, "wb") as handle:
            handle.write(b"stale pcm")
        with open(segment_video, "wb") as handle:
            handle.write(b"seg yuv")
        with open(segment_audio, "wb") as handle:
            handle.write(b"seg pcm")

        result = engine.cleanup_local_sync_cache(device_key=engine.current_device_key)

        assert os.path.isfile(video_marker)
        assert os.path.isfile(segment_video)
        assert os.path.isfile(segment_audio)
        assert not os.path.exists(base_video)
        assert not os.path.exists(base_audio)
        assert set(result["removed"]).isdisjoint({video_marker, segment_video, segment_audio})



# ===========================================================================
# Tests: SyncEngine.delete_device_track
# ===========================================================================

class TestDeleteDeviceTrack:
    def _make_engine(self, env):
        config = env["config"]
        db = env["db"]
        detector = DeviceDetector(config)
        device = DeviceInfo(env["device_path"])
        device.name = "Test iPod"
        device.is_rockbox = True
        detector._current_device = device
        return SyncEngine(db, config, detector)

    def test_delete_removes_file_and_db_row(self, env):
        engine = self._make_engine(env)

        # Put a file on the "device"
        rel_path = "Music/Art/Alb/01 - Track.mp3"
        full_path = os.path.join(env["device_path"], rel_path)
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, "wb") as f:
            f.write(b"data")
        lrc_path = os.path.splitext(full_path)[0] + ".lrc"
        with open(lrc_path, "w", encoding="utf-8") as f:
            f.write("lyrics")

        _insert_device_track(env["db"], "Track", "Art", "Alb", rel_path)
        env["db"].commit()

        dev_rows = env["db"].get_all_device_tracks()
        assert len(dev_rows) == 1
        dev_row = dev_rows[0]

        ok, msg = engine.delete_device_track(dev_row)
        assert ok is True
        assert not os.path.isfile(full_path)
        assert not os.path.isfile(lrc_path)
        assert len(env["db"].get_all_device_tracks()) == 0

    def test_delete_no_device_returns_error(self, env):
        config = env["config"]
        db = env["db"]
        detector = DeviceDetector(config)
        detector._current_device = None
        engine = SyncEngine(db, config, detector)

        ok, msg = engine.delete_device_track({"id": 1, "device_path": "x"})
        assert ok is False
        assert "No device" in msg


class TestDuplicateDeviceCleanup:
    def _make_engine(self, env):
        config = env["config"]
        db = env["db"]
        detector = DeviceDetector(config)
        device = DeviceInfo(env["device_path"])
        device.name = "Test iPod"
        device.is_rockbox = True
        detector._current_device = device
        engine = SyncEngine(db, config, detector)
        engine.set_current_device(device)
        return engine

    def test_find_and_remove_duplicate_device_tracks(self, env):
        src = _make_source_file(env, "Art", "Alb", "Duped")
        local = _insert_track(
            env["db"],
            "Duped",
            "Art",
            "Alb",
            file_path=src,
            metadata_hash="dup_hash",
            synced_to_device=1,
            device_path="Music/Art/Alb/01 - Duped.mp3",
        )
        for rel in ("Music/Art/Alb/01 - Duped.mp3", "Music/Art/Alb/01 - Duped (2).mp3"):
            full = os.path.join(env["device_path"], rel)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "wb") as handle:
                handle.write(b"audio")
            with open(os.path.splitext(full)[0] + ".lrc", "w", encoding="utf-8") as handle:
                handle.write("lyrics")

        engine = self._make_engine(env)
        _insert_device_track(
            env["db"],
            "Duped",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Duped.mp3",
            device_id=engine.current_device_key,
            metadata_hash="dup_hash",
            local_track_id=local["id"],
        )
        _insert_device_track(
            env["db"],
            "Duped",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Duped (2).mp3",
            device_id=engine.current_device_key,
            metadata_hash="dup_hash",
        )
        engine.load_cached_device_inventory(engine.current_device_key)
        duplicates = engine.find_duplicate_device_tracks()

        assert len(duplicates) == 1
        assert len(duplicates[0]["remove"]) == 1
        assert duplicates[0]["remove"][0]["device_path"].endswith("(2).mp3")

        result = engine.remove_duplicate_device_tracks()

        assert result["success"] is True
        assert result["deleted"] == ["Music/Art/Alb/01 - Duped (2).mp3"]
        assert os.path.isfile(os.path.join(env["device_path"], "Music/Art/Alb/01 - Duped.mp3"))
        assert not os.path.exists(os.path.join(env["device_path"], "Music/Art/Alb/01 - Duped (2).mp3"))
        assert not os.path.exists(os.path.join(env["device_path"], "Music/Art/Alb/01 - Duped (2).lrc"))
        remaining = env["db"].get_all_device_tracks(engine.current_device_key)
        assert len(remaining) == 1
        assert remaining[0]["local_track_id"] == local["id"]

    def test_apply_successful_sync_removes_deleted_duplicate_rows(self, env):
        src = _make_source_file(env, "Art", "Alb", "Duped")
        local = _insert_track(
            env["db"],
            "Duped",
            "Art",
            "Alb",
            file_path=src,
            metadata_hash="dup_hash",
            synced_to_device=1,
            device_path="Music/Art/Alb/01 - Duped.mp3",
        )

        engine = self._make_engine(env)
        _insert_device_track(
            env["db"],
            "Duped",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Duped.mp3",
            device_id=engine.current_device_key,
            metadata_hash="dup_hash",
            local_track_id=local["id"],
        )
        _insert_device_track(
            env["db"],
            "Duped",
            "Art",
            "Alb",
            "Music/Art/Alb/01 - Duped (2).mp3",
            device_id=engine.current_device_key,
            metadata_hash="dup_hash_variant",
        )
        engine.load_cached_device_inventory(engine.current_device_key)

        plan = SyncPlan()
        plan.to_delete.append("Music/Art/Alb/01 - Duped (2).mp3")
        engine.apply_successful_sync_to_cache(plan)

        remaining = env["db"].get_all_device_tracks(engine.current_device_key)
        assert [row["device_path"] for row in remaining] == ["Music/Art/Alb/01 - Duped.mp3"]
        assert remaining[0]["local_track_id"] == local["id"]

    def test_duplicate_cleanup_respects_intentional_local_duplicates(self, env):
        src_a = _make_source_file(env, "Art", "Alb", "Same A")
        src_b = _make_source_file(env, "Art", "Alb", "Same B")
        local_a = _insert_track(env["db"], "Same", "Art", "Alb", file_path=src_a, metadata_hash="same_hash_a", track_number=1)
        local_b = _insert_track(env["db"], "Same", "Art", "Alb", file_path=src_b, metadata_hash="same_hash_b", track_number=1)

        rel_a = "Music/Art/Alb/01 - Same.mp3"
        rel_b = "Music/Art/Alb/01 - Same (2).mp3"
        for rel in (rel_a, rel_b):
            full = os.path.join(env["device_path"], rel)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "wb") as handle:
                handle.write(b"audio")

        engine = self._make_engine(env)
        _insert_device_track(env["db"], "Same", "Art", "Alb", rel_a, device_id=engine.current_device_key, metadata_hash="same_hash_a", local_track_id=local_a["id"])
        _insert_device_track(env["db"], "Same", "Art", "Alb", rel_b, device_id=engine.current_device_key, metadata_hash="same_hash_b", local_track_id=local_b["id"])
        engine.load_cached_device_inventory(engine.current_device_key)

        assert engine.find_duplicate_device_tracks() == []

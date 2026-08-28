"""The video clip manifest is what makes the RVP cache disposable."""

import os
import sys
import threading

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from services.video_clip_manifest import (  # noqa: E402
    NoVideoClipManifest,
    VideoClipManifest,
)

DEV = "rockbox:test"


def test_records_and_returns_everything_the_plan_needs(tmp_path):
    """Size and hash alone are not enough.

    _prepare_mpeg_track() feeds bitrate, duration and codec into
    compute_metadata_hash(). If the manifest cannot supply them, a reused clip
    gets a different metadata_hash, the plan decides the metadata changed and
    re-syncs it anyway - so the manifest would have bought nothing.
    """
    m = VideoClipManifest(str(tmp_path / "video_clips.db"))
    m.record(DEV, "clip-abc.mpg", "hash1", 4096, 12.5, 1600, "MPEG-2", "/a.mp4")

    got = m.lookup(DEV, "clip-abc.mpg")
    assert got == {
        "file_hash": "hash1",
        "file_size": 4096,
        "duration": 12.5,
        "bitrate": 1600,
        "codec": "MPEG-2",
    }
    m.close()


def test_unknown_and_zero_sized_entries_are_not_reusable(tmp_path):
    m = VideoClipManifest(str(tmp_path / "video_clips.db"))
    assert m.lookup(DEV, "never-recorded.mpg") is None
    m.record(DEV, "empty.mpg", "hash", 0)
    assert m.lookup(DEV, "empty.mpg") is None, "a zero-byte clip is not reusable"
    m.close()


def test_entries_are_scoped_to_their_device(tmp_path):
    """Two iPods hold different encodes; one must never vouch for the other."""
    m = VideoClipManifest(str(tmp_path / "video_clips.db"))
    m.record(DEV, "clip.mpg", "hash1", 4096)
    assert m.lookup("rockbox:other", "clip.mpg") is None
    m.close()


def test_usable_from_another_thread(tmp_path):
    """Sync planning runs on a worker; the engine is built on the UI thread.

    sqlite3's default check_same_thread would raise ProgrammingError, which is
    a DatabaseError and so would be swallowed into a None lookup - silently
    re-transcoding everything instead of failing loudly.
    """
    m = VideoClipManifest(str(tmp_path / "video_clips.db"))
    m.record(DEV, "clip.mpg", "hash1", 4096)
    seen = []
    worker = threading.Thread(
        target=lambda: seen.append(m.lookup(DEV, "clip.mpg")))
    worker.start()
    worker.join()
    assert seen and seen[0] is not None, "manifest unusable from a worker thread"
    assert seen[0]["file_size"] == 4096
    m.close()


def test_a_broken_manifest_degrades_instead_of_raising(tmp_path):
    """Losing the manifest costs a transcode; it must never break a sync."""
    m = VideoClipManifest("/proc/definitely-not-a-dir/video_clips.db")
    assert m.lookup(DEV, "clip.mpg") is None
    m.record(DEV, "clip.mpg", "h", 1)   # must not raise
    assert m.count(DEV) == 0
    m.close()


def test_null_manifest_matches_the_real_interface():
    null = NoVideoClipManifest()
    assert null.lookup(DEV, "clip.mpg") is None
    assert null.count(DEV) == 0
    null.record(DEV, "clip.mpg", "h", 1)
    null.close()

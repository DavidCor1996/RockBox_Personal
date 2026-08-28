"""Tests for Rockbox tagcache inventory import."""

import os
import struct
from pathlib import Path

import pytest

import services.rockbox_tagcache as rockbox_tagcache
from models.track import Track
from services.device_inventory import device_record_from_info, verify_device_inventory
from services.device_detector import DeviceInfo, create_mock_device
from services.rockbox_tagcache import (
    TagcacheError,
    read_rockbox_tagcache_tracks,
    write_rockbox_tagcache_from_device_inventory,
    write_rockbox_tagcache_tracks,
)


TAGCACHE_MAGIC = 0x54434810
TAG_COUNT = 23
ROW_SIZE = (TAG_COUNT + 1) * 4

TAG_ARTIST = 0
TAG_ALBUM = 1
TAG_GENRE = 2
TAG_TITLE = 3
TAG_FILENAME = 4
TAG_COMPOSER = 5
TAG_COMMENT = 6
TAG_ALBUMARTIST = 7
TAG_GROUPING = 8
TAG_YEAR = 9
TAG_DISCNUMBER = 10
TAG_TRACKNUMBER = 11
TAG_VIRT_CANONICALARTIST = 12
TAG_BITRATE = 13
TAG_LENGTH = 14
TAG_PLAYCOUNT = 15
TAG_RATING = 16
TAG_PLAYTIME = 17
TAG_LASTPLAYED = 18

STRING_TAGS = {
    TAG_ARTIST: "database_0.tcd",
    TAG_ALBUM: "database_1.tcd",
    TAG_GENRE: "database_2.tcd",
    TAG_TITLE: "database_3.tcd",
    TAG_FILENAME: "database_4.tcd",
    TAG_COMPOSER: "database_5.tcd",
    TAG_COMMENT: "database_6.tcd",
    TAG_ALBUMARTIST: "database_7.tcd",
    TAG_GROUPING: "database_8.tcd",
    TAG_VIRT_CANONICALARTIST: "database_12.tcd",
}


def _device(path):
    create_mock_device(path)
    device = DeviceInfo(path)
    device.name = "Test iPod"
    device.is_rockbox = True
    return device


def _append_tag(blob, value, idx_id):
    encoded = str(value).encode("utf-8") + b"\0"
    offset = len(blob)
    blob.extend(struct.pack("<ii", len(encoded), idx_id))
    blob.extend(encoded)
    return offset


def _write_mock_tagcache(mount_path, entries):
    rockbox_dir = os.path.join(mount_path, ".rockbox")
    os.makedirs(rockbox_dir, exist_ok=True)

    tag_blobs = {tag: bytearray(struct.pack("<iii", TAGCACHE_MAGIC, 0, len(entries))) for tag in STRING_TAGS}
    rows = []

    for idx_id, entry in enumerate(entries):
        row = [0] * TAG_COUNT + [0]
        for tag, filename in STRING_TAGS.items():
            value = entry.get(tag, "")
            row[tag] = _append_tag(tag_blobs[tag], value, idx_id if tag in (TAG_TITLE, TAG_FILENAME) else -1)
        row[TAG_YEAR] = entry.get(TAG_YEAR, 0)
        row[TAG_DISCNUMBER] = entry.get(TAG_DISCNUMBER, 1)
        row[TAG_TRACKNUMBER] = entry.get(TAG_TRACKNUMBER, 0)
        row[TAG_BITRATE] = entry.get(TAG_BITRATE, 0)
        row[TAG_LENGTH] = entry.get(TAG_LENGTH, 0)
        row[TAG_PLAYCOUNT] = entry.get(TAG_PLAYCOUNT, 0)
        row[TAG_RATING] = entry.get(TAG_RATING, 0)
        row[TAG_PLAYTIME] = entry.get(TAG_PLAYTIME, 0)
        row[TAG_LASTPLAYED] = entry.get(TAG_LASTPLAYED, 0)
        rows.append(row)

    for tag, filename in STRING_TAGS.items():
        blob = tag_blobs[tag]
        struct.pack_into("<iii", blob, 0, TAGCACHE_MAGIC, max(len(blob) - 12, 0), len(entries))
        with open(os.path.join(rockbox_dir, filename), "wb") as handle:
            handle.write(blob)

    master = bytearray(struct.pack("<6i", TAGCACHE_MAGIC, len(rows) * ROW_SIZE, len(rows), 0, 1, 0))
    for row in rows:
        master.extend(struct.pack(f"<{TAG_COUNT + 1}i", *row))
    with open(os.path.join(rockbox_dir, "database_idx.tcd"), "wb") as handle:
        handle.write(master)


def test_read_rockbox_tagcache_tracks_parses_audio_rows(tmp_dir):
    mount_path = os.path.join(tmp_dir, "ipod")
    os.makedirs(os.path.join(mount_path, "Music"), exist_ok=True)
    _write_mock_tagcache(
        mount_path,
        [
            {
                TAG_ARTIST: "Artist",
                TAG_ALBUM: "Album",
                TAG_GENRE: "Rock",
                TAG_TITLE: "Song",
                TAG_FILENAME: "/Music/Artist/Album/01 - Song.mp3",
                TAG_ALBUMARTIST: "Artist",
                TAG_YEAR: 2024,
                TAG_DISCNUMBER: 1,
                TAG_TRACKNUMBER: 1,
                TAG_BITRATE: 320,
                TAG_LENGTH: 245000,
                TAG_PLAYCOUNT: 12,
                TAG_PLAYTIME: 2410000,
                TAG_RATING: 8,
                TAG_LASTPLAYED: 44,
            },
            {
                TAG_ARTIST: "Second Artist",
                TAG_ALBUM: "Another Album",
                TAG_TITLE: "Second Song",
                TAG_FILENAME: "/Music/Second Artist/Another Album/02 - Second Song.flac",
                TAG_ALBUMARTIST: "Second Artist",
                TAG_YEAR: 2023,
                TAG_DISCNUMBER: 1,
                TAG_TRACKNUMBER: 2,
                TAG_BITRATE: 900,
                TAG_LENGTH: 200500,
            },
        ],
    )

    tracks = read_rockbox_tagcache_tracks(mount_path, include_runtime=True)

    assert len(tracks) == 2
    assert tracks[0]["device_path"] == "Music/Artist/Album/01 - Song.mp3"
    assert tracks[0]["title"] == "Song"
    assert tracks[0]["album_artist"] == "Artist"
    assert tracks[0]["duration"] == 245.0
    assert tracks[0]["codec"] == "MP3"
    assert tracks[0]["play_count"] == 12
    assert tracks[0]["play_time"] == 2410000
    assert tracks[0]["rating"] == 8
    assert tracks[0]["last_played_serial"] == 44
    assert tracks[1]["device_path"].endswith("Second Song.flac")
    assert tracks[1]["codec"] == "FLAC"


def test_read_rockbox_tagcache_skips_deleted_rows_before_tag_offsets(tmp_dir):
    mount_path = os.path.join(tmp_dir, "ipod")
    _write_mock_tagcache(
        mount_path,
        [
            {TAG_TITLE: "Deleted", TAG_FILENAME: "/Music/Deleted.mp3"},
            {TAG_TITLE: "Live", TAG_FILENAME: "/Music/Live.mp3"},
        ],
    )
    master_path = Path(mount_path) / ".rockbox" / "database_idx.tcd"
    master = bytearray(master_path.read_bytes())
    row = list(struct.unpack_from(f"<{TAG_COUNT + 1}i", master, 24))
    for tag in STRING_TAGS:
        row[tag] = -123456789
    row[-1] = rockbox_tagcache.FLAG_DELETED
    struct.pack_into(f"<{TAG_COUNT + 1}i", master, 24, *row)
    master_path.write_bytes(master)

    tracks = read_rockbox_tagcache_tracks(mount_path)

    assert [track["title"] for track in tracks] == ["Live"]


def test_write_rockbox_tagcache_tracks_roundtrips_generated_database(tmp_dir):
    mount_path = os.path.join(tmp_dir, "ipod")
    audio_dir = os.path.join(mount_path, "Music", "Artist", "Album")
    os.makedirs(audio_dir, exist_ok=True)
    track_path = os.path.join(audio_dir, "01 - Song.mp3")
    with open(track_path, "wb") as handle:
        handle.write(b"song")

    result = write_rockbox_tagcache_tracks(
        mount_path,
        [
            {
                "device_path": "Music/Artist/Album/01 - Song.mp3",
                "title": "Song",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "Artist",
                "genre": "Rock",
                "year": 2026,
                "track_number": 1,
                "disc_number": 1,
                "duration": 123.4,
                "bitrate": 320,
            }
        ],
    )
    tracks = read_rockbox_tagcache_tracks(mount_path)

    assert result["success"] is True
    assert result["track_count"] == 1
    assert os.path.isfile(os.path.join(mount_path, ".rockbox", "database_idx.tcd"))
    assert os.path.isfile(os.path.join(mount_path, ".rockbox", "database_12.tcd"))
    snapshot = Path(mount_path) / ".rockbox" / rockbox_tagcache.RECOVERY_BACKUP_DIR
    for filename in rockbox_tagcache.HOST_TAGCACHE_FILES:
        assert (snapshot / filename).read_bytes() == (
            Path(mount_path) / ".rockbox" / filename
        ).read_bytes()
    assert tracks[0]["device_path"] == "Music/Artist/Album/01 - Song.mp3"
    assert tracks[0]["title"] == "Song"
    assert tracks[0]["artist"] == "Artist"
    assert tracks[0]["album"] == "Album"
    assert tracks[0]["duration"] == 123.4


def test_read_rockbox_tagcache_rejects_dirty_master(tmp_dir):
    mount_path = os.path.join(tmp_dir, "ipod")
    _write_mock_tagcache(
        mount_path,
        [{TAG_TITLE: "Song", TAG_FILENAME: "/Music/Song.mp3"}],
    )
    master_path = os.path.join(mount_path, ".rockbox", "database_idx.tcd")
    with open(master_path, "r+b") as handle:
        handle.seek(20)
        handle.write(struct.pack("<i", 1))

    with pytest.raises(TagcacheError, match="marked dirty"):
        read_rockbox_tagcache_tracks(mount_path)


def test_write_rockbox_tagcache_rolls_back_partial_publish(tmp_dir, monkeypatch):
    mount_path = os.path.join(tmp_dir, "ipod")
    audio_dir = os.path.join(mount_path, "Music", "Artist", "Album")
    os.makedirs(audio_dir, exist_ok=True)
    track_path = os.path.join(audio_dir, "01 - Song.mp3")
    with open(track_path, "wb") as handle:
        handle.write(b"song")

    base_track = {
        "device_path": "Music/Artist/Album/01 - Song.mp3",
        "title": "Original title",
        "artist": "Artist",
        "album": "Album",
        "album_artist": "Artist",
    }
    write_rockbox_tagcache_tracks(mount_path, [base_track])
    rockbox_dir = os.path.join(mount_path, ".rockbox")
    original_files = {
        name: (Path(rockbox_dir) / name).read_bytes()
        for name in rockbox_tagcache.HOST_TAGCACHE_FILES
    }

    original_replace = os.replace
    failed = False

    def fail_during_publish(source, destination):
        nonlocal failed
        if not failed and os.path.basename(destination) == "database_3.tcd":
            failed = True
            raise OSError("injected publish failure")
        return original_replace(source, destination)

    monkeypatch.setattr(rockbox_tagcache.os, "replace", fail_during_publish)
    changed_track = dict(base_track, title="Replacement title")

    with pytest.raises(OSError, match="injected publish failure"):
        write_rockbox_tagcache_tracks(mount_path, [changed_track])

    restored_files = {
        name: (Path(rockbox_dir) / name).read_bytes()
        for name in rockbox_tagcache.HOST_TAGCACHE_FILES
    }
    assert restored_files == original_files
    assert not os.path.exists(os.path.join(rockbox_dir, rockbox_tagcache.HOST_COMMIT_MARKER))
    assert not os.path.exists(os.path.join(mount_path, rockbox_tagcache.HOST_TRANSACTION_DIR))


def test_repair_refuses_empty_inventory_without_replacing_database(tmp_dir):
    mount_path = os.path.join(tmp_dir, "ipod")
    rockbox_dir = os.path.join(mount_path, ".rockbox")
    os.makedirs(rockbox_dir, exist_ok=True)
    existing_path = os.path.join(rockbox_dir, "database_idx.tcd")
    with open(existing_path, "wb") as handle:
        handle.write(b"existing database")

    with pytest.raises(TagcacheError, match="No supported audio tracks"):
        write_rockbox_tagcache_tracks(
            mount_path,
            [],
            require_tracks=True,
        )

    with open(existing_path, "rb") as handle:
        assert handle.read() == b"existing database"


def test_write_rockbox_tagcache_removes_stale_database_files_without_touching_music(tmp_dir):
    mount_path = os.path.join(tmp_dir, "ipod")
    audio_dir = os.path.join(mount_path, "Music", "Artist", "Album")
    rockbox_dir = os.path.join(mount_path, ".rockbox")
    os.makedirs(audio_dir, exist_ok=True)
    os.makedirs(rockbox_dir, exist_ok=True)
    stale_subdir = os.path.join(rockbox_dir, "database")
    os.makedirs(stale_subdir, exist_ok=True)
    track_path = os.path.join(audio_dir, "01 - Song.mp3")
    with open(track_path, "wb") as handle:
        handle.write(b"song")
    stale_tmp = os.path.join(rockbox_dir, "database_tmp.tcd")
    with open(stale_tmp, "wb") as handle:
        handle.write(b"stale")
    stale_nested = os.path.join(stale_subdir, "database_idx.tcd")
    with open(stale_nested, "wb") as handle:
        handle.write(b"stale nested")

    write_rockbox_tagcache_tracks(
        mount_path,
        [
            {
                "device_path": "Music/Artist/Album/01 - Song.mp3",
                "title": "Song",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "Artist",
            }
        ],
    )

    assert not os.path.exists(stale_tmp)
    assert not os.path.exists(stale_nested)
    assert os.path.isfile(track_path)


def test_write_rockbox_tagcache_from_device_inventory_uses_cached_rows(config, db):
    mount_path = config.mock_device_path
    audio_dir = os.path.join(mount_path, "Music", "Artist", "Album")
    os.makedirs(audio_dir, exist_ok=True)
    with open(os.path.join(audio_dir, "01 - Song.mp3"), "wb") as handle:
        handle.write(b"song")
    device = _device(mount_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "duration": 45.0,
            "bitrate": 192,
        }
    )
    db.commit()

    result = write_rockbox_tagcache_from_device_inventory(db, device, key)
    tracks = read_rockbox_tagcache_tracks(mount_path)

    assert result["success"] is True
    assert result["track_count"] == 1
    assert tracks[0]["title"] == "Song"
    assert tracks[0]["duration"] == 45.0


def test_write_rockbox_tagcache_from_device_inventory_prunes_missing_cached_rows(config, db):
    mount_path = config.mock_device_path
    audio_dir = os.path.join(mount_path, "Music", "Artist", "Album")
    os.makedirs(audio_dir, exist_ok=True)
    with open(os.path.join(audio_dir, "01 - Song.mp3"), "wb") as handle:
        handle.write(b"song")
    device = _device(mount_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
        }
    )
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Rani's playlist/02 - Stale.mp3",
            "title": "Stale",
            "artist": "Artist",
            "album": "Rani's playlist",
            "album_artist": "Artist",
        }
    )
    db.commit()

    result = write_rockbox_tagcache_from_device_inventory(db, device, key)
    tracks = read_rockbox_tagcache_tracks(mount_path)
    stale = db.fetchone(
        "SELECT present_on_device FROM device_tracks WHERE device_id = ? AND device_path = ?",
        (key, "Music/Artist/Rani's playlist/02 - Stale.mp3"),
    )

    assert result["success"] is True
    assert result["track_count"] == 1
    assert [track["title"] for track in tracks] == ["Song"]
    assert stale["present_on_device"] == 0


def test_verify_device_inventory_prefers_rockbox_tagcache(config, db, monkeypatch):
    mount_path = config.mock_device_path
    audio_dir = os.path.join(mount_path, "Music", "Artist", "Album")
    os.makedirs(audio_dir, exist_ok=True)
    with open(os.path.join(audio_dir, "01 - Song.mp3"), "wb") as handle:
        handle.write(b"song")
    _write_mock_tagcache(
        mount_path,
        [
            {
                TAG_ARTIST: "Artist",
                TAG_ALBUM: "Album",
                TAG_GENRE: "Rock",
                TAG_TITLE: "Song",
                TAG_FILENAME: "/Music/Artist/Album/01 - Song.mp3",
                TAG_ALBUMARTIST: "Artist",
                TAG_YEAR: 2024,
                TAG_DISCNUMBER: 1,
                TAG_TRACKNUMBER: 1,
                TAG_BITRATE: 320,
                TAG_LENGTH: 245000,
            }
        ],
    )
    device = _device(mount_path)

    monkeypatch.setattr(
        "services.device_inventory.read_metadata",
        lambda _path: (_ for _ in ()).throw(AssertionError("tagcache import should bypass file parsing")),
    )

    summary = verify_device_inventory(db, device, "metadata_only")
    rows = db.get_all_device_tracks(summary["device_key"])

    assert summary["inventory_source"] == "rockbox_tagcache"
    assert summary["inventory_confirmed"] is True
    assert summary["tagcache_track_count"] == 1
    assert summary["filesystem_track_count"] == 1
    assert summary["scanned"] == 1
    assert len(rows) == 1
    assert rows[0]["device_path"] == "Music/Artist/Album/01 - Song.mp3"
    assert rows[0]["title"] == "Song"


def test_verify_device_inventory_uses_tagcache_even_when_count_mismatch(config, db, monkeypatch):
    mount_path = config.mock_device_path
    audio_dir = os.path.join(mount_path, "Music", "Artist", "Album")
    os.makedirs(audio_dir, exist_ok=True)
    with open(os.path.join(audio_dir, "01 - Song.mp3"), "wb") as handle:
        handle.write(b"song")
    with open(os.path.join(audio_dir, "02 - Bonus.mp3"), "wb") as handle:
        handle.write(b"bonus")
    _write_mock_tagcache(
        mount_path,
        [
            {
                TAG_ARTIST: "Artist",
                TAG_ALBUM: "Album",
                TAG_GENRE: "Rock",
                TAG_TITLE: "Song",
                TAG_FILENAME: "/Music/Artist/Album/01 - Song.mp3",
                TAG_ALBUMARTIST: "Artist",
                TAG_YEAR: 2024,
                TAG_DISCNUMBER: 1,
                TAG_TRACKNUMBER: 1,
                TAG_BITRATE: 320,
                TAG_LENGTH: 245000,
            }
        ],
    )
    device = _device(mount_path)

    monkeypatch.setattr(
        "services.device_inventory.read_metadata",
        lambda path: Track(
            file_path=path,
            title="Bonus" if path.endswith("02 - Bonus.mp3") else "Song",
            artist="Artist",
            album="Album",
            album_artist="Artist",
            duration=245.0,
            file_size=os.path.getsize(path),
            metadata_hash="bonus-mh" if path.endswith("02 - Bonus.mp3") else "song-mh",
        ),
    )
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "MUSIC/Artist/Album/legacy.mp3",
            "title": "Legacy",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "duration": 200.0,
            "file_size": 50,
            "metadata_hash": "legacy-mh",
        }
    )
    db.commit()

    summary = verify_device_inventory(db, device, "metadata_only")
    rows = db.get_all_device_tracks(summary["device_key"])
    all_rows = db.get_all_device_tracks(summary["device_key"], present_only=False)

    assert summary["inventory_source"] == "rockbox_tagcache"
    assert summary["inventory_confirmed"] is True
    assert summary["inventory_confirmation_reason"] == "matched"
    assert summary["tagcache_track_count"] == 1
    assert summary["filesystem_track_count"] == 2
    assert summary["missing"] == 1
    assert len(rows) == 2
    assert sorted(row["device_path"] for row in rows) == [
        "Music/Artist/Album/01 - Song.mp3",
        "Music/Artist/Album/02 - Bonus.mp3",
    ]
    legacy = next(row for row in all_rows if row["device_path"] == "MUSIC/Artist/Album/legacy.mp3")
    assert legacy["present_on_device"] == 0

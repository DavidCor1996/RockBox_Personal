"""Tests for Rockbox playlist export/import integration."""

import json
import os

from services.device_detector import DeviceInfo
from services.rockbox_playlists import MANAGED_PLAYLIST_DIR, export_device_playlists


def _device(mount_path):
    device = DeviceInfo(mount_path)
    device.name = "Test iPod"
    device.is_rockbox = True
    return device


def _insert_track(db, file_path, title, artist, album):
    db.upsert_track(
        {
            "file_path": file_path,
            "title": title,
            "artist": artist,
            "album": album,
            "album_artist": artist,
            "track_number": 1,
            "disc_number": 1,
            "duration": 245.0,
            "bitrate": 320,
            "codec": "mp3",
            "metadata_hash": f"mh:{title}",
            "file_hash": f"fh:{title}",
        }
    )
    db.commit()
    return db.get_track_by_path(file_path)


def _insert_device_track(db, device_id, local_track_id, device_path, title, artist, album):
    db.upsert_device_track(
        {
            "device_id": device_id,
            "local_track_id": local_track_id,
            "device_path": device_path,
            "title": title,
            "artist": artist,
            "album": album,
            "album_artist": artist,
            "duration": 245.0,
            "bitrate": 320,
            "codec": "mp3",
            "metadata_hash": f"mh:{title}",
            "file_hash": f"fh:{title}",
            "present_on_device": 1,
        }
    )
    db.commit()


def test_export_device_playlists_writes_managed_m3u8_and_imports_cache(db, mock_device):
    device = _device(mock_device)
    on_device = _insert_track(db, "/music/on.mp3", "On Device", "Art", "Alb")
    off_device = _insert_track(db, "/music/off.mp3", "Off Device", "Art", "Alb")

    playlist_id = db.create_playlist("Road Mix")
    db.add_tracks_to_playlist(playlist_id, [on_device["id"], off_device["id"]])
    _insert_device_track(
        db,
        device.stable_device_key,
        on_device["id"],
        "Music/Art/Alb/01 - On Device.mp3",
        "On Device",
        "Art",
        "Alb",
    )

    result = export_device_playlists(db, device)
    db.commit()

    assert result["success"] is True
    assert len(result["exported"]) == 1
    playlist_path = os.path.join(mock_device, MANAGED_PLAYLIST_DIR, "Road Mix.m3u8")
    assert os.path.isfile(playlist_path)
    with open(playlist_path, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "#EXTM3U" in content
    assert "/Music/Art/Alb/01 - On Device.mp3" in content
    assert "Off Device" not in content

    imported = db.get_device_playlists(device.stable_device_key)
    assert len(imported) == 1
    assert imported[0]["name"] == "Road Mix"
    assert imported[0]["track_count"] == 1

    imported_tracks = db.get_device_playlist_tracks(imported[0]["id"])
    assert len(imported_tracks) == 1
    assert imported_tracks[0]["device_device_path"] == "Music/Art/Alb/01 - On Device.mp3"


def test_export_device_playlists_exports_smart_playlists_and_removes_stale_managed_files(db, mock_device):
    device = _device(mock_device)
    row = _insert_track(db, "/music/smart.mp3", "Smart Song", "Smart Artist", "Smart Album")
    rules = {
        "match": "all",
        "rules": [{"field": "artist", "operator": "is", "value": "Smart Artist"}],
    }
    playlist_id = db.create_playlist("Smart: Favorites", is_smart=True, rules_json=json.dumps(rules))
    _insert_device_track(
        db,
        device.stable_device_key,
        row["id"],
        "Music/Smart Artist/Smart Album/01 - Smart Song.mp3",
        "Smart Song",
        "Smart Artist",
        "Smart Album",
    )

    stale_path = os.path.join(mock_device, MANAGED_PLAYLIST_DIR, "Old Playlist.m3u8")
    os.makedirs(os.path.dirname(stale_path), exist_ok=True)
    with open(stale_path, "w", encoding="utf-8") as handle:
        handle.write("#EXTM3U\n")

    result = export_device_playlists(db, device)
    db.commit()

    assert result["success"] is True
    assert result["removed"] == [f"{MANAGED_PLAYLIST_DIR}/Old Playlist.m3u8"]
    exported_paths = {item["source_path"] for item in result["exported"]}
    assert f"{MANAGED_PLAYLIST_DIR}/Smart_ Favorites.m3u8" in exported_paths
    assert not os.path.exists(stale_path)

    smart_playlist = os.path.join(mock_device, MANAGED_PLAYLIST_DIR, "Smart_ Favorites.m3u8")
    with open(smart_playlist, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "/Music/Smart Artist/Smart Album/01 - Smart Song.mp3" in content

    imported = db.get_device_playlists(device.stable_device_key)
    assert len(imported) == 1
    assert imported[0]["name"] == "Smart_ Favorites"
    assert imported[0]["track_count"] == 1

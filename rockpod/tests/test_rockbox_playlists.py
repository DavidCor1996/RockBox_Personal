"""Tests for Rockbox playlist export/import integration."""

import json
import os

from services.device_detector import DeviceInfo
from services.rockbox_playlists import (
    LEGACY_MANAGED_PLAYLIST_DIR,
    LOCAL_MANAGED_PLAYLIST_DIR,
    MANAGED_PLAYLIST_DIR,
    MANAGED_PLAYLIST_MANIFEST,
    _ensure_apple_music_transcode,
    export_device_playlists,
    export_local_music_playlists,
)


def _device(mount_path):
    device = DeviceInfo(mount_path)
    device.name = "Test iPod"
    device.is_rockbox = True
    return device


class _FakeCommandResult:
    def __init__(self, returncode=0, stdout="", stderr="", log_path=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.log_path = log_path

    def failure_message(self):
        return f"Command failed with exit status {self.returncode}: fake ffmpeg\nLog: {self.log_path}"


class _FakeCommandRunner:
    def __init__(self, result=None, write_output=True):
        self.result = result or _FakeCommandResult()
        self.write_output = write_output
        self.commands = []
        self.cwd = ""

    def run(self, command, cwd="", timeout=None, env=None):
        self.commands.append(command)
        self.cwd = cwd
        if self.write_output and self.result.returncode == 0:
            with open(command[-1], "wb") as handle:
                handle.write(b"converted")
        return self.result


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
    assert not [
        name for name in os.listdir(os.path.dirname(playlist_path))
        if name.startswith("tmp")
    ]
    manifest_dir = os.path.join(mock_device, ".rockbox")
    assert os.path.isfile(os.path.join(mock_device, MANAGED_PLAYLIST_MANIFEST))
    assert not [name for name in os.listdir(manifest_dir) if name.startswith("tmp")]

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

    stale_path = os.path.join(mock_device, LEGACY_MANAGED_PLAYLIST_DIR, "Old Playlist.m3u8")
    os.makedirs(os.path.dirname(stale_path), exist_ok=True)
    with open(stale_path, "w", encoding="utf-8") as handle:
        handle.write("#EXTM3U\n")

    result = export_device_playlists(db, device)
    db.commit()

    assert result["success"] is True
    assert result["removed"] == [f"{LEGACY_MANAGED_PLAYLIST_DIR}/Old Playlist.m3u8"]
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


def test_export_device_playlists_only_writes_playlists_selected_for_rockbox(db, mock_device):
    device = _device(mock_device)
    synced = _insert_track(db, "/music/synced.mp3", "Synced", "Art", "Alb")
    hidden = _insert_track(db, "/music/hidden.mp3", "Hidden", "Art", "Alb")
    shown_id = db.create_playlist("Shown")
    hidden_id = db.create_playlist("Hidden")
    db.add_track_to_playlist(shown_id, synced["id"])
    db.add_track_to_playlist(hidden_id, hidden["id"])
    db.set_playlist_sync_to_rockbox(hidden_id, False)
    _insert_device_track(
        db,
        device.stable_device_key,
        synced["id"],
        "Music/Art/Alb/01 - Synced.mp3",
        "Synced",
        "Art",
        "Alb",
    )
    _insert_device_track(
        db,
        device.stable_device_key,
        hidden["id"],
        "Music/Art/Alb/02 - Hidden.mp3",
        "Hidden",
        "Art",
        "Alb",
    )

    result = export_device_playlists(db, device)

    assert result["success"] is True
    assert [item["name"] for item in result["exported"]] == ["Shown"]
    assert os.path.isfile(os.path.join(mock_device, MANAGED_PLAYLIST_DIR, "Shown.m3u8"))
    assert not os.path.exists(os.path.join(mock_device, MANAGED_PLAYLIST_DIR, "Hidden.m3u8"))


def test_export_device_playlists_preserves_user_catalog_playlists(db, mock_device):
    device = _device(mock_device)
    row = _insert_track(db, "/music/on.mp3", "On Device", "Art", "Alb")
    playlist_id = db.create_playlist("Road Mix")
    db.add_track_to_playlist(playlist_id, row["id"])
    _insert_device_track(
        db,
        device.stable_device_key,
        row["id"],
        "Music/Art/Alb/01 - On Device.mp3",
        "On Device",
        "Art",
        "Alb",
    )

    user_playlist = os.path.join(mock_device, MANAGED_PLAYLIST_DIR, "Road Mix.m3u8")
    os.makedirs(os.path.dirname(user_playlist), exist_ok=True)
    with open(user_playlist, "w", encoding="utf-8") as handle:
        handle.write("#EXTM3U\n/user-owned.mp3\n")

    result = export_device_playlists(db, device)

    assert result["success"] is True
    assert result["exported"][0]["source_path"] == f"{MANAGED_PLAYLIST_DIR}/Road Mix (2).m3u8"
    with open(user_playlist, "r", encoding="utf-8") as handle:
        assert "/user-owned.mp3" in handle.read()
    assert os.path.isfile(os.path.join(mock_device, MANAGED_PLAYLIST_DIR, "Road Mix (2).m3u8"))
    assert os.path.isfile(os.path.join(mock_device, MANAGED_PLAYLIST_MANIFEST))


def test_export_device_playlists_matches_existing_unlinked_device_tracks(db, mock_device):
    device = _device(mock_device)
    row = _insert_track(db, "/music/existing.mp3", "Existing", "Art", "Alb")
    playlist_id = db.create_playlist("Existing Mix")
    db.add_track_to_playlist(playlist_id, row["id"])
    db.upsert_device_track(
        {
            "device_id": device.stable_device_key,
            "device_path": "Music/Art/Alb/01 - Existing.mp3",
            "title": "Existing",
            "artist": "Art",
            "album": "Alb",
            "album_artist": "Art",
            "duration": 245.0,
            "bitrate": 320,
            "codec": "mp3",
            "metadata_hash": "device-only-hash",
            "file_hash": "",
            "present_on_device": 1,
        }
    )
    db.commit()

    result = export_device_playlists(db, device)

    assert result["success"] is True
    playlist_path = os.path.join(mock_device, MANAGED_PLAYLIST_DIR, "Existing Mix.m3u8")
    with open(playlist_path, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "/Music/Art/Alb/01 - Existing.mp3" in content


def test_export_local_music_playlists_writes_relative_m3u8_for_network_share(db, tmp_dir):
    music_root = os.path.join(tmp_dir, "Music")
    album_dir = os.path.join(music_root, "Artist", "Album")
    os.makedirs(album_dir, exist_ok=True)
    first_path = os.path.join(album_dir, "01 - One.flac")
    second_path = os.path.join(album_dir, "02 - Two.flac")
    with open(first_path, "wb") as handle:
        handle.write(b"one")
    with open(second_path, "wb") as handle:
        handle.write(b"two")

    first = _insert_track(db, first_path, "One", "Artist", "Album")
    second = _insert_track(db, second_path, "Two", "Artist", "Album")
    playlist_id = db.create_playlist("Road Mix")
    db.add_tracks_to_playlist(playlist_id, [first["id"], second["id"]])
    db.commit()

    result = export_local_music_playlists(db, music_root)

    assert result["success"] is True
    assert result["exported"][0]["source_path"] == f"{LOCAL_MANAGED_PLAYLIST_DIR}/Road Mix.m3u8"
    playlist_path = os.path.join(music_root, LOCAL_MANAGED_PLAYLIST_DIR, "Road Mix.m3u8")
    with open(playlist_path, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "#EXTM3U" in content
    assert "../.." in content
    assert "../../Artist/Album/01 - One.flac" in content
    assert "../../Artist/Album/02 - Two.flac" in content
    assert "/home/" not in content
    assert not [
        name for name in os.listdir(os.path.dirname(playlist_path))
        if name.startswith("tmp")
    ]


def test_export_local_music_playlists_removes_stale_managed_files(db, tmp_dir):
    music_root = os.path.join(tmp_dir, "Music")
    managed = os.path.join(music_root, LOCAL_MANAGED_PLAYLIST_DIR)
    os.makedirs(managed, exist_ok=True)
    stale_path = os.path.join(managed, "Old.m3u8")
    with open(stale_path, "w", encoding="utf-8") as handle:
        handle.write("#EXTM3U\n")

    result = export_local_music_playlists(db, music_root)

    assert result["success"] is True
    assert result["removed"] == [f"{LOCAL_MANAGED_PLAYLIST_DIR}/Old.m3u8"]
    assert not os.path.exists(stale_path)


def test_export_local_music_playlists_can_point_to_converted_apple_music_files(db, tmp_dir, monkeypatch):
    music_root = os.path.join(tmp_dir, "Music")
    album_dir = os.path.join(music_root, "Artist", "Album")
    media_dir = os.path.join(music_root, "Playlists", "RockPod Media")
    os.makedirs(album_dir, exist_ok=True)
    source_path = os.path.join(album_dir, "01 - One.flac")
    converted_path = os.path.join(media_dir, "01 - One.m4a")
    with open(source_path, "wb") as handle:
        handle.write(b"flac")

    def fake_transcode(path, target_dir, ffmpeg_path=""):
        assert path == source_path
        assert target_dir == media_dir
        os.makedirs(target_dir, exist_ok=True)
        with open(converted_path, "wb") as handle:
            handle.write(b"m4a")
        return converted_path

    monkeypatch.setattr(
        "services.rockbox_playlists._ensure_apple_music_transcode",
        fake_transcode,
    )

    first = _insert_track(db, source_path, "One", "Artist", "Album")
    playlist_id = db.create_playlist("Road Mix")
    db.add_track_to_playlist(playlist_id, first["id"])
    db.commit()

    result = export_local_music_playlists(
        db,
        music_root,
        include_smart=False,
        convert_for_apple_music=True,
    )

    assert result["success"] is True
    playlist_path = os.path.join(music_root, LOCAL_MANAGED_PLAYLIST_DIR, "Road Mix.m3u8")
    with open(playlist_path, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "../RockPod Media/01 - One.m4a" in content
    assert ".flac" not in content


def test_apple_music_transcode_uses_shared_command_runner(tmp_dir):
    media_root = os.path.join(tmp_dir, "media")
    source_path = os.path.join(tmp_dir, "One.flac")
    with open(source_path, "wb") as handle:
        handle.write(b"flac")
    runner = _FakeCommandRunner()

    converted = _ensure_apple_music_transcode(source_path, media_root, ffmpeg_path="/usr/bin/ffmpeg", command_runner=runner)

    assert os.path.isfile(converted)
    assert converted.endswith(".m4a")
    assert runner.cwd == media_root
    assert runner.commands[0][0] == "/usr/bin/ffmpeg"
    assert runner.commands[0][-1].endswith(".tmp.m4a")


def test_apple_music_transcode_failure_removes_temp_and_reports_log(tmp_dir):
    media_root = os.path.join(tmp_dir, "media")
    source_path = os.path.join(tmp_dir, "One.flac")
    with open(source_path, "wb") as handle:
        handle.write(b"flac")
    result = _FakeCommandResult(returncode=7, stderr="", log_path=os.path.join(tmp_dir, "ffmpeg.log"))
    runner = _FakeCommandRunner(result=result, write_output=False)

    try:
        _ensure_apple_music_transcode(source_path, media_root, ffmpeg_path="/usr/bin/ffmpeg", command_runner=runner)
    except OSError as exc:
        assert "audio conversion failed" in str(exc)
        assert "exit status 7" in str(exc)
        assert result.log_path in str(exc)
    else:
        raise AssertionError("Transcode failure was not reported")

    assert not [
        name for name in os.listdir(media_root)
        if name.endswith(".tmp.m4a")
    ]

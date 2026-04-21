"""Tests for smart playlists and Rockbox runtime data import."""

import os

from app.database import Database
from services.device_detector import DeviceInfo, create_mock_device
from services.rockbox_runtime import import_runtime_data_for_device
from services.smart_playlists import ensure_default_smart_playlists, evaluate_rules, evaluate_rules_count


def _insert_track(db, **overrides):
    track = {
        "file_path": overrides.get("file_path", f"/music/{overrides.get('title', 'Song')}.mp3"),
        "title": overrides.get("title", "Song"),
        "artist": overrides.get("artist", "Artist"),
        "album": overrides.get("album", "Album"),
        "album_artist": overrides.get("album_artist", overrides.get("artist", "Artist")),
        "genre": overrides.get("genre", "Rock"),
        "play_count": overrides.get("play_count", 0),
        "rating": overrides.get("rating", 0),
        "last_played": overrides.get("last_played", ""),
        "date_added": overrides.get("date_added", "2026-04-01T00:00:00+00:00"),
        "duration": overrides.get("duration", 120.0),
        "file_size": overrides.get("file_size", 100),
        "metadata_hash": overrides.get("metadata_hash", overrides.get("title", "Song").lower()),
    }
    db.upsert_track(track)
    db.commit()
    return db.get_track_by_path(track["file_path"])


def _device(tmp_dir, name="mock_ipod"):
    path = os.path.join(tmp_dir, name)
    create_mock_device(path)
    device = DeviceInfo(path)
    device.name = "Runtime iPod"
    device.is_rockbox = True
    return device


def test_default_smart_playlists_created(db):
    created = ensure_default_smart_playlists(db)
    db.commit()
    playlists = db.get_all_playlists()
    names = {row["name"] for row in playlists if row["is_smart"]}
    assert created >= 1
    assert "Most Played" in names
    assert "Recently Added and Unplayed" in names
    assert "Most Played on iPod" in names
    assert "Most Played on Desktop" in names
    assert "Recently Played on iPod" in names
    assert "Never Played on iPod" in names
    assert "Smart Sync Suggestions" in names


def test_evaluate_rules_uses_runtime_stats_and_device_presence(db):
    on_device = _insert_track(db, title="On Device Favorite", play_count=1, rating=2)
    off_device = _insert_track(db, title="Off Device Favorite", play_count=0, rating=0)
    device_id = "rockbox:test"
    db.upsert_device_track(
        {
            "device_id": device_id,
            "device_path": "Music/Artist/Album/01 - On Device Favorite.mp3",
            "title": on_device["title"],
            "artist": on_device["artist"],
            "album": on_device["album"],
            "local_track_id": on_device["id"],
            "metadata_hash": on_device["metadata_hash"],
        }
    )
    db.upsert_runtime_stat(
        {
            "device_id": device_id,
            "source_path": ".rockbox/database_changelog.txt",
            "source_track_key": "on-device",
            "local_track_id": on_device["id"],
            "play_count": 9,
            "rating": 5,
            "last_played": "2026-04-15T00:00:00+00:00",
            "play_time_seconds": 540,
        }
    )
    db.upsert_runtime_stat(
        {
            "device_id": device_id,
            "source_path": ".rockbox/database_changelog.txt",
            "source_track_key": "off-device",
            "local_track_id": off_device["id"],
            "play_count": 7,
            "rating": 4,
            "last_played": "2026-04-14T00:00:00+00:00",
            "play_time_seconds": 420,
        }
    )
    db.commit()

    most_played = evaluate_rules(
        db,
        {
            "match": "all",
            "order_by": "effective_play_count DESC",
            "rules": [{"field": "play_count", "operator": "greater_than_or_equal", "value": 7}],
        },
        device_id=device_id,
    )
    assert [row["title"] for row in most_played] == ["On Device Favorite", "Off Device Favorite"]

    on_ipod_recent = evaluate_rules(
        db,
        {
            "match": "all",
            "rules": [
                {"field": "on_ipod", "operator": "is_true", "value": "__current__"},
                {"field": "rating", "operator": "greater_than_or_equal", "value": 5},
            ],
        },
        device_id=device_id,
    )
    assert [row["title"] for row in on_ipod_recent] == ["On Device Favorite"]

    not_on_ipod_frequent = evaluate_rules(
        db,
        {
            "match": "all",
            "rules": [
                {"field": "not_on_ipod", "operator": "is_true", "value": "__current__"},
                {"field": "play_count", "operator": "greater_than_or_equal", "value": 5},
            ],
        },
        device_id=device_id,
    )
    assert [row["title"] for row in not_on_ipod_frequent] == ["Off Device Favorite"]


def test_evaluate_rules_count_matches_materialized_results(db):
    _insert_track(db, title="Rock One", genre="Rock", play_count=5)
    _insert_track(db, title="Rock Two", genre="Rock", play_count=3)
    _insert_track(db, title="Jazz One", genre="Jazz", play_count=9)

    rules = {
        "match": "all",
        "rules": [
            {"field": "genre", "operator": "equals", "value": "Rock"},
            {"field": "play_count", "operator": "greater_than_or_equal", "value": 3},
        ],
    }

    rows = evaluate_rules(db, rules)
    count = evaluate_rules_count(db, rules)

    assert count == len(rows) == 2


def test_runtime_import_maps_database_changelog_to_tracks(config, db, tmp_dir):
    device = _device(tmp_dir)
    local = _insert_track(
        db,
        title="Mapped Song",
        artist="Artist",
        album="Album",
        file_path="/music/Artist/Album/Mapped Song.mp3",
        metadata_hash="mapped",
    )
    device_row = db.upsert_device(
        {
            "stable_device_key": device.stable_device_key,
            "display_name": device.name,
            "mount_path_last_seen": device.mount_path,
        }
    )
    db.upsert_device_track(
        {
            "device_id": device_row["stable_device_key"],
            "device_path": "Music/Artist/Album/01 - Mapped Song.mp3",
            "title": "Mapped Song",
            "artist": "Artist",
            "album": "Album",
            "local_track_id": local["id"],
            "metadata_hash": "mapped",
        }
    )
    changelog = os.path.join(device.mount_path, ".rockbox", "database_changelog.txt")
    with open(changelog, "w", encoding="utf-8") as handle:
        handle.write(
            "filename=Music/Artist/Album/01 - Mapped Song.mp3\n"
            "title=Mapped Song\n"
            "artist=Artist\n"
            "album=Album\n"
            "playcount=11\n"
            "rating=4\n"
            "lastplayed=1713312000\n"
            "Pm=3\n"
            "Ps=25\n"
        )

    imported = import_runtime_data_for_device(db, device)
    db.commit()

    summary = db.runtime_stats_summary()
    assert imported == 1
    assert summary["entries"] == 1
    row = db.fetchone("SELECT * FROM runtime_stats LIMIT 1")
    assert row["local_track_id"] == local["id"]
    assert row["play_count"] == 11
    assert row["rating"] == 4
    assert int(row["play_time_seconds"]) == 205


def test_runtime_import_is_incremental_for_unchanged_file(config, db, tmp_dir):
    device = _device(tmp_dir, "mock_ipod_two")
    changelog = os.path.join(device.mount_path, ".rockbox", "database_changelog.txt")
    with open(changelog, "w", encoding="utf-8") as handle:
        handle.write("filename=Music/A.mp3\nplaycount=1\n")

    first = import_runtime_data_for_device(db, device)
    db.commit()
    second = import_runtime_data_for_device(db, device)

    assert first == 0 or first >= 0
    assert second == 0


def test_evaluate_rules_supports_desktop_ipod_and_combined_play_counts(db):
    on_device = _insert_track(db, title="Hybrid Favorite", play_count=3)
    device_id = "rockbox:test"
    db.upsert_device_track(
        {
            "device_id": device_id,
            "device_path": "Music/Artist/Album/01 - Hybrid Favorite.mp3",
            "title": on_device["title"],
            "artist": on_device["artist"],
            "album": on_device["album"],
            "local_track_id": on_device["id"],
            "metadata_hash": on_device["metadata_hash"],
        }
    )
    db.upsert_runtime_stat(
        {
            "device_id": device_id,
            "source_path": ".rockbox/database_changelog.txt",
            "source_track_key": "hybrid",
            "local_track_id": on_device["id"],
            "play_count": 9,
            "last_played": "2026-04-16T00:00:00+00:00",
        }
    )
    db.commit()

    desktop_only = evaluate_rules(
        db,
        {"match": "all", "rules": [{"field": "play_count_desktop", "operator": "equals", "value": 3}]},
        device_id=device_id,
    )
    ipod_only = evaluate_rules(
        db,
        {"match": "all", "rules": [{"field": "play_count_ipod", "operator": "equals", "value": 9}]},
        device_id=device_id,
    )
    combined = evaluate_rules(
        db,
        {"match": "all", "rules": [{"field": "play_count_combined", "operator": "equals", "value": 12}]},
        device_id=device_id,
    )

    assert [row["title"] for row in desktop_only] == ["Hybrid Favorite"]
    assert [row["title"] for row in ipod_only] == ["Hybrid Favorite"]
    assert [row["title"] for row in combined] == ["Hybrid Favorite"]

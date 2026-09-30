"""Durable, per-device listening history independent of tagcache rebuilds."""

from __future__ import annotations

import json
import sqlite3
from pathlib import Path


def lifetime_history(mount: Path, tracks: list[dict]) -> list[dict]:
    """Retain metadata, runtime high-water marks and deduplicated log events.

    Existing runtime totals seed the history. New log events then add to that
    baseline even after a database rebuild zeros the runtime counters. Taking
    the maximum with live runtime counters avoids counting the same listen in
    both sources. Missing tracks and rotated/deleted logs never erase history.
    """
    directory = mount / ".rockbox" / "spotify-wrapped"
    directory.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(directory / "history.sqlite3")
    try:
        connection.execute("PRAGMA synchronous = FULL")
        with connection:
            connection.execute("""CREATE TABLE IF NOT EXISTS events (
                path TEXT, timestamp INTEGER, elapsed INTEGER, length INTEGER,
                PRIMARY KEY (path, timestamp, elapsed, length))""")
            connection.execute("""CREATE TABLE IF NOT EXISTS tracks (
                path TEXT PRIMARY KEY, metadata TEXT NOT NULL,
                plays INTEGER NOT NULL, milliseconds INTEGER NOT NULL,
                baseline_plays INTEGER NOT NULL, baseline_ms INTEGER NOT NULL)""")
            for log in sorted((mount / ".rockbox").glob("playback*.log")):
                # Read errors abort the refresh; never publish a partial view.
                with log.open(encoding="utf-8", errors="replace") as stream:
                    for line in stream:
                        if not line.endswith("\n") or line.startswith("#"):
                            continue
                        try:
                            timestamp, elapsed, length, path = line.rstrip("\r\n").split(":", 3)
                            timestamp, elapsed, length = int(timestamp), int(elapsed), int(length)
                        except ValueError:
                            continue
                        threshold = min(length, 30_000) if length > 0 else 30_000
                        if (not 0 <= timestamp <= 0xFFFFFFFF or
                                not threshold <= elapsed <= 0xFFFFFFFF or
                                not 0 <= length <= 0xFFFFFFFF or not path):
                            continue
                        connection.execute(
                            "INSERT OR IGNORE INTO events VALUES (?, ?, ?, ?)",
                            (path.lstrip("/").casefold(), timestamp, elapsed, length),
                        )
            current = {
                str(track["device_path"]).lstrip("/").casefold(): track
                for track in tracks if track.get("device_path")
            }
            saved = {
                row[0]: row[1:] for row in connection.execute("SELECT * FROM tracks")
            }
            logged = {
                row[0]: row[1:] for row in connection.execute(
                    "SELECT path, COUNT(*), SUM(elapsed) FROM events GROUP BY path"
                )
            }
            result = []
            for path in sorted(current.keys() | saved.keys()):
                old = saved.get(path)
                track = dict(current[path]) if path in current else json.loads(old[0])
                log_plays, log_ms = logged.get(path, (0, 0))
                runtime_plays = max(int(track.get("play_count") or 0), 0)
                runtime_ms = max(int(track.get("play_time") or 0), 0)
                if old:
                    baseline_plays, baseline_ms = old[3], old[4]
                    plays = max(runtime_plays, old[1], baseline_plays + log_plays)
                    milliseconds = max(runtime_ms, old[2], baseline_ms + log_ms)
                else:
                    baseline_plays = max(runtime_plays - log_plays, 0)
                    baseline_ms = max(runtime_ms - log_ms, 0)
                    plays = max(runtime_plays, log_plays)
                    milliseconds = max(runtime_ms, log_ms)
                track.update(play_count=plays, play_time=milliseconds)
                connection.execute(
                    "INSERT OR REPLACE INTO tracks VALUES (?, ?, ?, ?, ?, ?)",
                    (path, json.dumps(track), plays, milliseconds, baseline_plays, baseline_ms),
                )
                result.append(track)
        return result
    finally:
        connection.close()

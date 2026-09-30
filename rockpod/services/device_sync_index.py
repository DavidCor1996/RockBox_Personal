"""Transactional device-output index for incremental RockPod application syncs."""

from __future__ import annotations

import json
import os
import sqlite3
import time
import uuid
from pathlib import Path

from services.file_safety import atomic_write_text
from services.path_safety import resolve_under_root


SYNC_ID_PATH = ".rockbox/rockpod/sync-device-id"


class DeviceSyncIndex:
    """Remember proven output conversions without probing every device file.

    The database lives in RockPod's fast local cache and is partitioned by a
    stable ID stored on the iPod. A missing database row may be seeded once
    from legacy sidecars/output files; subsequent syncs are one indexed lookup.
    """

    def __init__(self, config, mount_path):
        self.mount = os.path.realpath(os.fspath(mount_path))
        id_path = Path(resolve_under_root(self.mount, SYNC_ID_PATH))
        try:
            device_id = id_path.read_text(encoding="ascii").strip()
            uuid.UUID(device_id)
        except (OSError, ValueError):
            device_id = str(uuid.uuid4())
            id_path.parent.mkdir(parents=True, exist_ok=True)
            atomic_write_text(id_path, device_id + "\n")
        self.device_id = device_id
        cache = Path(config.get("cache_dir")) / "device-sync"
        cache.mkdir(parents=True, exist_ok=True)
        self.path = cache / "outputs.sqlite3"
        # Each artifact update must be visible immediately. Leaving sqlite3's
        # implicit transaction open while ffmpeg converts a large profile held
        # the single writer lock for minutes and made another application sync
        # fail with "database is locked". Autocommit releases it straight away.
        self.connection = sqlite3.connect(
            self.path, timeout=30, isolation_level=None
        )
        self.connection.execute("PRAGMA busy_timeout=30000")
        self.connection.execute("PRAGMA journal_mode=WAL")
        self.connection.execute("PRAGMA synchronous=NORMAL")
        self.connection.execute(
            "CREATE TABLE IF NOT EXISTS output_state ("
            "device_id TEXT NOT NULL, app TEXT NOT NULL, item_id TEXT NOT NULL, "
            "artifact TEXT NOT NULL, signature TEXT NOT NULL, outputs_json TEXT NOT NULL, "
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "PRIMARY KEY(device_id, app, item_id, artifact))"
        )
        self.connection.execute(
            "CREATE TABLE IF NOT EXISTS app_state ("
            "device_id TEXT NOT NULL, app TEXT NOT NULL, key TEXT NOT NULL, "
            "value TEXT NOT NULL, updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "PRIMARY KEY(device_id, app, key))"
        )
        self.connection.commit()

    def close(self):
        if self.connection is not None:
            self.connection.commit()
            self.connection.close()
            self.connection = None

    def current(self, app, item_id, artifact, signature, outputs=None):
        row = self._execute(
            "SELECT signature, outputs_json FROM output_state WHERE device_id=? AND app=? "
            "AND item_id=? AND artifact=?",
            (self.device_id, str(app), str(item_id), str(artifact)),
        ).fetchone()
        if not row or row[0] != str(signature):
            return False
        if outputs is None:
            return True
        try:
            recorded = [os.path.realpath(path) for path in json.loads(row[1])]
        except (TypeError, ValueError):
            return False
        expected = [os.path.realpath(os.fspath(path)) for path in outputs]
        return bool(expected) and recorded == expected

    def mark(self, app, item_id, artifact, signature, outputs=()):
        normalized = [os.fspath(path) for path in outputs]
        self._execute(
            "INSERT INTO output_state(device_id,app,item_id,artifact,signature,outputs_json) "
            "VALUES(?,?,?,?,?,?) ON CONFLICT(device_id,app,item_id,artifact) DO UPDATE SET "
            "signature=excluded.signature, outputs_json=excluded.outputs_json, "
            "updated_at=CURRENT_TIMESTAMP",
            (
                self.device_id,
                str(app),
                str(item_id),
                str(artifact),
                str(signature),
                json.dumps(normalized),
            ),
        )

    def current_or_seed(
        self,
        app,
        item_id,
        artifact,
        signature,
        outputs=(),
        *,
        legacy_marker=None,
        legacy_signature=None,
        trust_existing=False,
    ):
        if self.current(app, item_id, artifact, signature, outputs):
            return True
        output_paths = [Path(path) for path in outputs]
        if not output_paths or not all(path.is_file() for path in output_paths):
            return False
        proven = bool(trust_existing)
        if legacy_marker is not None:
            try:
                proven = Path(legacy_marker).read_text(encoding="ascii").strip() == str(
                    legacy_signature if legacy_signature is not None else signature
                )
            except OSError:
                proven = False
        if not proven:
            return False
        self.mark(app, item_id, artifact, signature, output_paths)
        return True

    def commit(self):
        self.connection.commit()

    def prune(self, app, valid_item_ids):
        valid = {str(value) for value in valid_item_ids}
        rows = self._execute(
            "SELECT DISTINCT item_id FROM output_state WHERE device_id=? AND app=?",
            (self.device_id, str(app)),
        ).fetchall()
        stale = [row[0] for row in rows if row[0] not in valid]
        if stale:
            self._executemany(
                "DELETE FROM output_state WHERE device_id=? AND app=? AND item_id=?",
                [(self.device_id, str(app), item_id) for item_id in stale],
            )
        return stale

    def needs_legacy_reconcile(self, app):
        row = self._execute(
            "SELECT value FROM app_state WHERE device_id=? AND app=? AND key=?",
            (self.device_id, str(app), "legacy-reconciled"),
        ).fetchone()
        return not row or row[0] != "1"

    def mark_legacy_reconciled(self, app):
        self._execute(
            "INSERT INTO app_state(device_id,app,key,value) VALUES(?,?,?,?) "
            "ON CONFLICT(device_id,app,key) DO UPDATE SET "
            "value=excluded.value, updated_at=CURRENT_TIMESTAMP",
            (self.device_id, str(app), "legacy-reconciled", "1"),
        )

    def _execute(self, statement, parameters=()):
        """Run a short SQLite operation, yielding briefly to another sync."""
        for attempt in range(5):
            try:
                return self.connection.execute(statement, parameters)
            except sqlite3.OperationalError as exc:
                if "locked" not in str(exc).lower() and "busy" not in str(exc).lower():
                    raise
                if attempt == 4:
                    raise
                time.sleep(0.05 * (attempt + 1))

    def _executemany(self, statement, parameters):
        for attempt in range(5):
            try:
                return self.connection.executemany(statement, parameters)
            except sqlite3.OperationalError as exc:
                if "locked" not in str(exc).lower() and "busy" not in str(exc).lower():
                    raise
                if attempt == 4:
                    raise
                time.sleep(0.05 * (attempt + 1))

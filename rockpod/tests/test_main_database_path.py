import logging
from pathlib import Path

import main as rockpod_main


class _Config:
    def __init__(self, db_path):
        self.db_path = str(db_path)


def test_stale_runtime_database_path_recovers_persistent_database(
    tmp_path, monkeypatch
):
    persistent = tmp_path / "persistent" / "library.db"
    persistent.parent.mkdir()
    persistent.write_bytes(b"database")
    fallback = tmp_path / "rockpod-runtime" / "library.db"
    monkeypatch.setattr(rockpod_main, "DEFAULT_DB_PATH", str(persistent))
    monkeypatch.setattr(rockpod_main.tempfile, "gettempdir", lambda: str(tmp_path))

    recovered = rockpod_main._persistent_db_path(
        _Config(fallback), logging.getLogger("test")
    )

    assert recovered == str(persistent)

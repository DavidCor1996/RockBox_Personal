"""Shared test fixtures for RockPod tests."""

import os
import shutil
import tempfile

import pytest

# Force headless Qt for CI/local test runs before any Qt import path is hit.
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

# Ensure project root is importable
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from app.config import Config
from app.database import Database
from services.device_detector import create_mock_device


@pytest.fixture
def tmp_dir():
    """A temporary directory that is removed after the test."""
    d = tempfile.mkdtemp(prefix="rockpod_test_")
    yield d
    shutil.rmtree(d, ignore_errors=True)


@pytest.fixture
def config(tmp_dir):
    """A Config instance pointing at temporary directories."""
    cfg_path = os.path.join(tmp_dir, "config.json")
    c = Config(cfg_path)
    c.music_dir = os.path.join(tmp_dir, "Music")
    c.video_dir = os.path.join(tmp_dir, "Videos")
    c.db_path = os.path.join(tmp_dir, "library.db")
    c.cache_dir = os.path.join(tmp_dir, "cache")
    c.artwork_cache_dir = os.path.join(tmp_dir, "cache", "artwork")
    c.mock_device_enabled = True
    c.mock_device_path = os.path.join(tmp_dir, "mock_ipod")
    c.ensure_dirs()
    os.makedirs(c.music_dir, exist_ok=True)
    for video_dir in c.video_dirs:
        os.makedirs(video_dir, exist_ok=True)
    return c


@pytest.fixture
def db(config):
    """A fresh database for testing."""
    database = Database(config.db_path)
    yield database
    database.close()


@pytest.fixture
def mock_device(config):
    """A mock Rockbox device directory."""
    device_path = config.mock_device_path
    create_mock_device(device_path)
    return device_path


@pytest.fixture
def sample_music_dir(config):
    """Create a sample music folder structure with fake audio files.

    Since we cannot create real audio files in tests without heavy dependencies,
    we create empty files with correct extensions and test the scanner's ability
    to handle them gracefully (metadata extraction will return defaults).
    """
    music = config.music_dir
    artists = [
        ("Pink Floyd", "The Dark Side of the Moon", [
            "01 - Speak to Me.mp3",
            "02 - Breathe.mp3",
            "03 - On the Run.flac",
        ]),
        ("Radiohead", "OK Computer", [
            "01 - Airbag.mp3",
            "02 - Paranoid Android.mp3",
            "03 - Subterranean Homesick Alien.ogg",
        ]),
        ("Various Artists", "Compilation", [
            "01 - Track One.m4a",
            "02 - Track Two.mp3",
        ]),
    ]

    created_files = []
    for artist, album, tracks in artists:
        album_dir = os.path.join(music, artist, album)
        os.makedirs(album_dir, exist_ok=True)
        for track_name in tracks:
            track_path = os.path.join(album_dir, track_name)
            # Create a minimal file (not a valid audio file, but enough for path scanning)
            with open(track_path, "wb") as f:
                f.write(b"\x00" * 1024)
            created_files.append(track_path)

    return music, created_files

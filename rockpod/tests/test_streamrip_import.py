import os

import pytest

from services.streamrip_import import (
    StreamripImportError,
    StreamripImporter,
    build_streamrip_command,
    discover_imported_audio_files,
    ensure_rockbox_cover_files,
    ensure_streamrip_config,
    is_supported_streamrip_url,
    streamrip_source_tag_matches,
    streamrip_quality_for_url,
    streamrip_url_info,
)


def test_supported_streamrip_urls_include_lossless_sources():
    assert is_supported_streamrip_url("https://listen.tidal.com/album/123") is True
    assert is_supported_streamrip_url("https://www.qobuz.com/us-en/album/example/abc") is True
    assert is_supported_streamrip_url("https://www.deezer.com/album/123") is True


def test_build_streamrip_command_caps_tidal_at_quality_three(tmp_dir):
    music_dir = os.path.join(tmp_dir, "Music")
    config_path = os.path.join(tmp_dir, "streamrip", "config.toml")

    command = build_streamrip_command(
        "rip",
        music_dir,
        "https://listen.tidal.com/album/123",
        "flac",
        4,
        config_path,
    )

    assert command[:9] == ["rip", "-f", music_dir, "--no-db", "-q", "3", "-c", "FLAC", "--no-progress"]
    assert command[-2:] == ["url", "https://listen.tidal.com/album/123"]
    assert "--config-path" in command
    assert config_path in command


def test_streamrip_quality_caps_follow_service_limits():
    assert streamrip_quality_for_url("https://listen.tidal.com/album/123", 4) == 3
    assert streamrip_quality_for_url("https://www.qobuz.com/us-en/album/example/abc", 4) == 4
    assert streamrip_quality_for_url("https://www.deezer.com/album/123", 4) == 2


def test_streamrip_url_info_extracts_album_and_track_ids():
    assert streamrip_url_info("https://tidal.com/album/513960426") == {
        "source": "tidal",
        "media_type": "album",
        "id": "513960426",
    }
    assert streamrip_url_info("https://www.deezer.com/track/42") == {
        "source": "deezer",
        "media_type": "track",
        "id": "42",
    }


def test_streamrip_source_tag_matches_streamrip_ids_case_insensitively():
    tags = {
        "tidal_album_id": ["513960426"],
        "TITLE": ["Song"],
    }

    assert streamrip_source_tag_matches(tags, "tidal", "album", "513960426") is True
    assert streamrip_source_tag_matches(tags, "tidal", "track", "513960426") is False


def test_build_streamrip_command_rejects_unknown_urls(tmp_dir):
    with pytest.raises(StreamripImportError):
        build_streamrip_command("rip", tmp_dir, "https://example.com/album/123")


def test_streamrip_importer_uses_cache_state_dirs(config):
    importer = StreamripImporter(config)
    env = importer.env()

    assert env["XDG_CONFIG_HOME"].startswith(config.cache_dir)
    assert env["XDG_CACHE_HOME"].startswith(config.cache_dir)
    assert importer.config_path.startswith(config.cache_dir)
    assert importer.log_dir.startswith(config.cache_dir)


def test_discover_imported_audio_files_filters_by_time(tmp_dir):
    old_file = os.path.join(tmp_dir, "old.flac")
    new_file = os.path.join(tmp_dir, "new.flac")
    ignored = os.path.join(tmp_dir, "cover.jpg")
    for path in (old_file, new_file, ignored):
        with open(path, "wb") as handle:
            handle.write(b"x")
    os.utime(old_file, (1, 1))

    found = discover_imported_audio_files(tmp_dir, since_timestamp=2)

    assert new_file in found
    assert old_file not in found
    assert ignored not in found


def test_ensure_rockbox_cover_files_copies_parent_cover_to_disc_folder(tmp_dir):
    album_dir = os.path.join(tmp_dir, "Artist - Album")
    disc_dir = os.path.join(album_dir, "Disc 1")
    os.makedirs(disc_dir)
    source_cover = os.path.join(album_dir, "cover.jpg")
    track = os.path.join(disc_dir, "01. Track.flac")
    with open(source_cover, "wb") as handle:
        handle.write(b"cover")
    with open(track, "wb") as handle:
        handle.write(b"audio")

    copied = ensure_rockbox_cover_files([track], tmp_dir)

    target = os.path.join(disc_dir, "cover.jpg")
    assert copied == [target]
    assert os.path.isfile(target)


def test_prepare_import_creates_log_and_streamrip_config(config):
    importer = StreamripImporter(config)
    request = importer.prepare_import("https://listen.tidal.com/album/123", "flac")

    assert os.path.isfile(request.log_path)
    assert request.log_path.startswith(importer.log_dir)
    assert os.path.isfile(importer.config_path)


def test_prepare_album_search_builds_storefront_search_request(config):
    importer = StreamripImporter(config)
    request = importer.prepare_album_search("linkin park", "tidal", limit=12)

    assert request.output_path.startswith(importer.search_dir)
    assert request.command[1].endswith("streamrip_store_search.py")
    assert "--source" in request.command
    assert "tidal" in request.command
    assert "--query" in request.command
    assert "linkin park" in request.command
    assert "--cover-dir" in request.command
    assert importer.cover_dir in request.command
    assert os.path.isfile(importer.config_path)


def test_prepare_store_homepage_builds_tidal_homepage_request(config):
    importer = StreamripImporter(config)
    request = importer.prepare_store_homepage(limit=18, home_tab="rock")

    assert request.output_path.startswith(importer.search_dir)
    assert request.command[1].endswith("streamrip_store_search.py")
    assert "--homepage" in request.command
    assert "--home-tab" in request.command
    assert "rock" in request.command
    assert "--limit" in request.command
    assert "18" in request.command
    assert "--cover-dir" in request.command
    assert importer.cover_dir in request.command
    assert os.path.isfile(importer.config_path)


def test_prepare_album_detail_builds_storefront_detail_request(config):
    importer = StreamripImporter(config)
    request = importer.prepare_album_detail("tidal", "513960426")

    assert request.output_path.startswith(importer.search_dir)
    assert request.command[1].endswith("streamrip_store_search.py")
    assert "--source" in request.command
    assert "tidal" in request.command
    assert "--album-id" in request.command
    assert "513960426" in request.command
    assert "--cover-dir" in request.command
    assert importer.cover_dir in request.command
    assert os.path.isfile(importer.config_path)


def test_ensure_streamrip_config_sets_rockpod_album_defaults(config):
    path = ensure_streamrip_config(
        os.path.join(config.cache_dir, "streamrip", "config.toml"),
        config.music_dir,
        4,
    )
    with open(path, "r") as handle:
        text = handle.read()

    assert 'save_artwork = true' in text
    assert 'embed = true' in text
    assert 'folder_format = "{albumartist} - {title} ({year})"' in text
    assert 'track_format = "{tracknumber:02}. {title}"' in text

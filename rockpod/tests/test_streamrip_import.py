import os

import pytest

from tomlkit import parse

from services.streamrip_import import (
    StreamripImportError,
    _align_streamrip_config,
    patch_streamrip_tidal_lyrics,
    StreamripImporter,
    build_streamrip_command,
    discover_imported_audio_files,
    ensure_rockbox_cover_files,
    ensure_streamrip_config,
    find_existing_streamrip_source_files,
    find_existing_library_item_files,
    is_supported_streamrip_url,
    streamrip_source_tag_matches,
    streamrip_quality_for_url,
    streamrip_url_info,
)


def _temp_names(path):
    return [name for name in os.listdir(path) if name.startswith("tmp")]


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


def test_build_streamrip_command_converts_spotify_playlist_through_tidal(tmp_dir):
    music_dir = os.path.join(tmp_dir, "Music")
    config_path = os.path.join(tmp_dir, "streamrip", "config.toml")

    command = build_streamrip_command(
        "rip",
        music_dir,
        "https://open.spotify.com/playlist/37i9dQZF1DXcBWIGoYBM5M",
        "flac",
        4,
        config_path,
    )

    assert command[0].endswith("python") or "python" in os.path.basename(command[0])
    assert command[1].endswith("spotify_playlist_tidal_import.py")
    assert "--source" in command
    assert "tidal" in command
    assert "--url" in command
    assert "https://open.spotify.com/playlist/37i9dQZF1DXcBWIGoYBM5M" in command


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
    assert streamrip_url_info("https://open.spotify.com/playlist/37i9dQZF1DXcBWIGoYBM5M") == {
        "source": "spotify",
        "media_type": "playlist",
        "id": "37i9dQZF1DXcBWIGoYBM5M",
    }


def test_streamrip_source_tag_matches_streamrip_ids_case_insensitively():
    tags = {
        "tidal_album_id": ["513960426"],
        "TITLE": ["Song"],
    }

    assert streamrip_source_tag_matches(tags, "tidal", "album", "513960426") is True
    assert streamrip_source_tag_matches(tags, "tidal", "track", "513960426") is False


def test_existing_streamrip_album_requires_complete_metadata_order(tmp_path, monkeypatch):
    album = tmp_path / "Album"
    album.mkdir()
    first = album / "first.flac"
    second = album / "second.flac"
    first.write_bytes(b"one")
    second.write_bytes(b"two")
    tags = {
        str(first): {"tidal_album_id": ["55"], "discnumber": ["1"], "disctotal": ["1"], "tracknumber": ["1"], "tracktotal": ["2"]},
        str(second): {"tidal_album_id": ["55"], "discnumber": ["1"], "disctotal": ["1"], "tracknumber": ["2"], "tracktotal": ["2"]},
    }
    monkeypatch.setattr("services.streamrip_import.mutagen.File", lambda path, easy=False: type("Audio", (), {"tags": tags[path]})())

    assert find_existing_streamrip_source_files(tmp_path, "tidal", "album", "55") == [str(first), str(second)]
    second.unlink()
    assert find_existing_streamrip_source_files(tmp_path, "tidal", "album", "55") == []


def test_existing_library_album_matches_tags_without_provider_id(tmp_path, monkeypatch):
    album = tmp_path / "The Strokes - Reality Awaits"
    album.mkdir()
    first = album / "first.flac"
    second = album / "second.flac"
    partial = tmp_path / "The Strokes - Partial"
    partial.mkdir()
    missing_first = partial / "second.flac"
    for path in (first, second, missing_first):
        path.write_bytes(b"audio")
    tags = {
        str(first): {"title": ["One"], "album": ["Reality Awaits"], "albumartist": ["The Strokes"], "tracknumber": ["1/2"]},
        str(second): {"title": ["Two"], "album": ["Reality Awaits"], "albumartist": ["The Strokes"], "tracknumber": ["2/2"]},
        str(missing_first): {"title": ["Two"], "album": ["Partial"], "albumartist": ["The Strokes"], "tracknumber": ["2/2"]},
    }
    monkeypatch.setattr("services.streamrip_import.mutagen.File", lambda path, easy=True: type("Audio", (), {"tags": tags[path]})())

    assert find_existing_library_item_files(tmp_path, "album", "Reality Awaits", "The Strokes") == [str(first), str(second)]
    assert find_existing_library_item_files(tmp_path, "album", "Partial", "The Strokes") == []


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
    assert not _temp_names(os.path.dirname(importer.config_path))


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
    assert not _temp_names(os.path.dirname(path))
    assert 'embed = true' in text
    assert 'folder_format = "{albumartist} - {title} ({year})"' in text
    assert 'track_format = "{tracknumber:02}. {title}"' in text


_SCHEMA_2_0_6 = """[deezer]
arl = ""
quality = 2

[tidal]
quality = 3
access_token = ""

[misc]
version = "2.0.6"
"""

_SCHEMA_2_2_0 = """[deezer]
arl = ""
quality = 2
lower_quality_if_not_available = false

[tidal]
quality = 3
access_token = ""

[misc]
version = "2.2.0"
"""


def test_align_streamrip_config_drops_settings_the_installed_version_lacks():
    data = parse(_SCHEMA_2_2_0)
    data["tidal"]["access_token"] = "saved-token"

    _align_streamrip_config(data, parse(_SCHEMA_2_0_6))

    assert "lower_quality_if_not_available" not in data["deezer"]
    assert data["misc"]["version"] == "2.0.6"
    assert data["tidal"]["access_token"] == "saved-token"


def test_align_streamrip_config_adds_settings_a_newer_version_expects():
    data = parse(_SCHEMA_2_0_6)
    data["tidal"]["access_token"] = "saved-token"

    _align_streamrip_config(data, parse(_SCHEMA_2_2_0))

    assert data["deezer"]["lower_quality_if_not_available"] is False
    assert data["misc"]["version"] == "2.2.0"
    assert data["tidal"]["access_token"] == "saved-token"


def test_align_streamrip_config_reports_a_missing_section():
    data = parse(_SCHEMA_2_0_6)
    del data["deezer"]

    with pytest.raises(StreamripImportError) as excinfo:
        _align_streamrip_config(data, parse(_SCHEMA_2_0_6))

    assert "deezer" in str(excinfo.value)


def test_align_streamrip_config_reports_unexpected_settings():
    data = parse(_SCHEMA_2_0_6)
    data["tidal"]["mystery_setting"] = 1

    with pytest.raises(StreamripImportError) as excinfo:
        _align_streamrip_config(data, parse(_SCHEMA_2_0_6))

    assert "tidal.mystery_setting" in str(excinfo.value)


_TIDAL_LYRICS_CALL = """                resp = await self._api_request(
                    f"tracks/{item_id!s}/lyrics", base="https://listen.tidal.com/v1"
                )
"""


def test_patch_streamrip_tidal_lyrics_removes_the_redirecting_host():
    patched = patch_streamrip_tidal_lyrics(_TIDAL_LYRICS_CALL)

    assert 'base="https://tidal.com/v1"' in patched
    assert "listen.tidal.com" not in patched


def test_patch_streamrip_tidal_lyrics_leaves_a_patched_copy_alone():
    patched = patch_streamrip_tidal_lyrics(_TIDAL_LYRICS_CALL)

    assert patch_streamrip_tidal_lyrics(patched) is None


def test_patch_streamrip_tidal_lyrics_reports_an_unknown_request():
    with pytest.raises(StreamripImportError):
        patch_streamrip_tidal_lyrics('resp = await self._api_request("tracks/1/lyrics")')

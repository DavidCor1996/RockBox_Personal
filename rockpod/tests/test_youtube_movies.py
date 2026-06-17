import os

import pytest

from services.youtube_movies import (
    YoutubeMovieImportError,
    YoutubeMovieImporter,
    is_supported_youtube_url,
)


def test_supported_youtube_urls():
    assert is_supported_youtube_url("https://www.youtube.com/watch?v=abc") is True
    assert is_supported_youtube_url("https://youtu.be/abc") is True
    assert is_supported_youtube_url("https://example.com/watch?v=abc") is False


def test_prepare_import_builds_youtube_movie_command(config):
    config.set("youtube_movie_binary", "/usr/bin/yt-dlp")
    config.set("ffmpeg_binary", "/usr/bin/ffmpeg")
    importer = YoutubeMovieImporter(config)

    request = importer.prepare_import("https://www.youtube.com/watch?v=abc")

    assert request.command[1].endswith("youtube_movie_import.py")
    assert "--url" in request.command
    assert "https://www.youtube.com/watch?v=abc" in request.command
    assert "--output-dir" in request.command
    assert importer.output_dir in request.command
    assert os.path.isfile(request.log_path)


def test_prepare_browse_builds_youtube_movie_browse_command(config):
    config.set("youtube_movie_binary", "/usr/bin/yt-dlp")
    importer = YoutubeMovieImporter(config)

    request = importer.prepare_browse("public domain full movies", limit=12)

    assert request.output_path.startswith(importer.browse_dir)
    assert request.command[1].endswith("youtube_movie_browse.py")
    assert "--query" in request.command
    assert "public domain full movies" in request.command
    assert "--limit" in request.command
    assert "12" in request.command
    assert "--output" in request.command
    assert request.output_path in request.command


def test_prepare_import_rejects_non_youtube_url(config):
    importer = YoutubeMovieImporter(config)

    with pytest.raises(YoutubeMovieImportError):
        importer.prepare_import("https://example.com/movie")

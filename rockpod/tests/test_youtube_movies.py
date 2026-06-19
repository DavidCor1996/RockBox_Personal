import os

import pytest
from PIL import Image

from services.youtube_movies import (
    YoutubeMovieImportError,
    YoutubeMovieImporter,
    existing_movie_duplicate,
    is_supported_youtube_url,
    parse_movie_import_progress,
    persist_movie_import_poster,
    sanitize_movie_title,
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
    assert "--thumbnail-dir" in request.command
    assert importer.thumbnail_dir in request.command
    assert os.path.isdir(importer.thumbnail_dir)


def test_prepare_import_rejects_non_youtube_url(config):
    importer = YoutubeMovieImporter(config)

    with pytest.raises(YoutubeMovieImportError):
        importer.prepare_import("https://example.com/movie")


def test_parse_movie_import_progress_from_ytdlp_and_ffmpeg_lines():
    download = parse_movie_import_progress("[download]  42.5% of 12.00MiB at 1.00MiB/s ETA 00:07")
    assert download["phase"] == "Downloading"
    assert download["progress"] == "42.5%"

    convert = parse_movie_import_progress("frame=  240 fps=30 q=2.0 size=1024kB time=00:00:08.00 bitrate=1048.6kbits/s")
    assert convert["phase"] == "Converting"
    assert convert["progress"] == "00:00:08.00"

    done = parse_movie_import_progress("ROCKPOD_MOVIE_OUTPUT=/tmp/videos/Movie.mpg")
    assert done["phase"] == "Completed"
    assert done["progress"] == "100%"
    assert done["detail"] == "Movie.mpg"


def test_movie_duplicate_detection_matches_existing_title_and_output_path(tmp_dir):
    output_dir = os.path.join(tmp_dir, "Videos", "YouTube")
    os.makedirs(output_dir, exist_ok=True)
    result = {"title": "A Movie: The Cut", "url": "https://www.youtube.com/watch?v=abc"}
    expected = os.path.join(output_dir, f"{sanitize_movie_title(result['title'])}.mpg")

    by_title = existing_movie_duplicate(
        [{"title": "A Movie_ The Cut", "file_path": os.path.join(output_dir, "other.mpg")}],
        result,
        output_dir,
    )
    by_path = existing_movie_duplicate([], result, output_dir)
    assert by_title["title"] == "A Movie_ The Cut"
    assert by_path is None

    open(expected, "wb").write(b"movie")
    by_existing_file = existing_movie_duplicate([], result, output_dir)
    assert by_existing_file["file_path"] == expected


def test_persist_movie_import_poster_writes_sidecar_jpeg(tmp_dir):
    movie_path = os.path.join(tmp_dir, "Imported Movie.mpg")
    source_path = os.path.join(tmp_dir, "store-thumb.png")
    open(movie_path, "wb").write(b"movie")
    Image.new("RGB", (320, 180), "#336699").save(source_path, "PNG")

    poster_path = persist_movie_import_poster(movie_path, source_path)

    assert poster_path == os.path.join(tmp_dir, "Imported Movie.jpg")
    assert os.path.isfile(poster_path)
    with Image.open(poster_path) as image:
        assert image.format == "JPEG"

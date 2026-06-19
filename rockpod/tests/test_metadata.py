"""Tests for the metadata reader and fingerprinting."""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from models.track import Track, compute_metadata_hash, compute_artwork_hash
from services.metadata_reader import read_metadata_details, read_lyrics, _apply_video_path_fallback


class TestMetadataHash:
    """Test metadata fingerprint computation."""

    def test_identical_metadata_same_hash(self):
        h1 = compute_metadata_hash(
            "Song", "Artist", "Album", "Artist",
            1, 1, "Rock", 2007, "Composer", 240.0, 320, "MP3",
        )
        h2 = compute_metadata_hash(
            "Song", "Artist", "Album", "Artist",
            1, 1, "Rock", 2007, "Composer", 240.0, 320, "MP3",
        )
        assert h1 == h2

    def test_case_insensitive(self):
        h1 = compute_metadata_hash(
            "Song", "Artist", "Album", "Artist",
            1, 1, "Rock", 2007, "", 240.0, 320, "MP3",
        )
        h2 = compute_metadata_hash(
            "song", "artist", "album", "artist",
            1, 1, "rock", 2007, "", 240.0, 320, "mp3",
        )
        assert h1 == h2

    def test_whitespace_normalized(self):
        h1 = compute_metadata_hash(
            "  Song  ", " Artist ", " Album ", "Artist",
            1, 1, "Rock", 2007, "", 240.0, 320, "MP3",
        )
        h2 = compute_metadata_hash(
            "Song", "Artist", "Album", "Artist",
            1, 1, "Rock", 2007, "", 240.0, 320, "MP3",
        )
        assert h1 == h2

    def test_different_title_different_hash(self):
        h1 = compute_metadata_hash(
            "Song A", "Artist", "Album", "", 1, 1, "", 2007, "", 240.0, 320, "MP3",
        )
        h2 = compute_metadata_hash(
            "Song B", "Artist", "Album", "", 1, 1, "", 2007, "", 240.0, 320, "MP3",
        )
        assert h1 != h2

    def test_different_bitrate_different_hash(self):
        h1 = compute_metadata_hash(
            "Song", "Artist", "Album", "", 1, 1, "", 2007, "", 240.0, 128, "MP3",
        )
        h2 = compute_metadata_hash(
            "Song", "Artist", "Album", "", 1, 1, "", 2007, "", 240.0, 320, "MP3",
        )
        assert h1 != h2

    def test_none_values_handled(self):
        h = compute_metadata_hash(
            None, None, None, None, None, None, None, None, None, 0.0, 0, "",
        )
        assert isinstance(h, str)
        assert len(h) == 32

    def test_track_recompute(self):
        t = Track(
            title="Hello", artist="World", album="Test", album_artist="World",
            track_number=1, disc_number=1, genre="Pop", year=2020,
            composer="Me", duration=180.0, bitrate=256, codec="FLAC",
        )
        h = t.recompute_metadata_hash()
        assert h == t.metadata_hash
        assert len(h) == 32


def test_video_path_fallback_replaces_generic_youtube_title(tmp_dir):
    video_dir = os.path.join(tmp_dir, "Videos", "YouTube")
    os.makedirs(video_dir, exist_ok=True)
    video_path = os.path.join(video_dir, "Real Movie.mpg")
    with open(video_path, "wb") as handle:
        handle.write(b"video")
    track = Track(file_path=video_path, title="youtube", album="youtube", artist="")

    _apply_video_path_fallback(track, video_path)

    assert track.title == "Real Movie"
    assert track.album == "Real Movie"


class TestArtworkHash:
    def test_empty_returns_empty(self):
        assert compute_artwork_hash(None) == ""
        assert compute_artwork_hash(b"") == ""

    def test_same_data_same_hash(self):
        data = b"\x89PNG\r\n\x1a\nfakedata"
        h1 = compute_artwork_hash(data)
        h2 = compute_artwork_hash(data)
        assert h1 == h2

    def test_different_data_different_hash(self):
        h1 = compute_artwork_hash(b"image_a")
        h2 = compute_artwork_hash(b"image_b")
        assert h1 != h2


class TestTrackNeedsResync:
    def test_not_synced_does_not_need_resync(self):
        t = Track(synced_to_device=0, metadata_hash="abc", last_synced_metadata_hash="xyz")
        assert not t.needs_resync

    def test_same_hash_no_resync(self):
        t = Track(
            synced_to_device=1,
            metadata_hash="abc123",
            last_synced_metadata_hash="abc123",
        )
        assert not t.needs_resync

    def test_different_metadata_hash_needs_resync(self):
        t = Track(
            synced_to_device=1,
            metadata_hash="new_hash",
            last_synced_metadata_hash="old_hash",
        )
        assert t.needs_resync


class TestMetadataReaderDiagnostics:
    def test_flac_with_embedded_art_is_indexed(self, monkeypatch, tmp_dir):
        path = os.path.join(tmp_dir, "song.flac")
        with open(path, "wb") as f:
            f.write(b"audio")

        class FakeInfo:
            length = 123.0
            bitrate = 900000
            sample_rate = 44100
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {
                "title": ["Song"],
                "artist": ["Artist"],
                "album": ["Album"],
                "albumartist": ["Artist"],
            }
            pictures = [object()]

        monkeypatch.setattr("services.metadata_reader.mutagen.File", lambda _path: FakeAudio())
        monkeypatch.setattr("services.metadata_reader.has_embedded_artwork", lambda _path: True)
        monkeypatch.setattr(
            "services.metadata_reader.extract_artwork_data",
            lambda _path: (b"fake-image", "image/jpeg"),
        )

        track, info = read_metadata_details(path)

        assert track.title == "Song"
        assert track.artist == "Artist"
        assert track.album == "Album"
        assert track.has_embedded_artwork == 1
        assert track.artwork_hash != ""
        assert info["parsed_ok"] is True

    def test_artwork_failure_does_not_abort_track_import(self, monkeypatch, tmp_dir):
        path = os.path.join(tmp_dir, "song.flac")
        with open(path, "wb") as f:
            f.write(b"audio")

        class FakeInfo:
            length = 123.0
            bitrate = 900000
            sample_rate = 44100
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {
                "title": ["Song"],
                "artist": ["Artist"],
                "album": ["Album"],
            }
            pictures = [object()]

        monkeypatch.setattr("services.metadata_reader.mutagen.File", lambda _path: FakeAudio())
        monkeypatch.setattr("services.metadata_reader.has_embedded_artwork", lambda _path: True)

        def fail_art(_path):
            raise RuntimeError("bad art")

        track, info = read_metadata_details(path, artwork_extractor_func=fail_art)

        assert track.title == "Song"
        assert track.artist == "Artist"
        assert track.album == "Album"
        assert track.artwork_hash == ""
        assert "artwork extraction failure" in info["warnings"]

    def test_parser_failure_falls_back_to_basic_metadata(self, monkeypatch, tmp_dir):
        path = os.path.join(tmp_dir, "Broken Song.mp3")
        with open(path, "wb") as f:
            f.write(b"audio")

        def fail_mutagen(_path):
            raise ValueError("bad tags")

        track, info = read_metadata_details(path, mutagen_file_func=fail_mutagen)

        assert track.title == "Broken Song"
        assert track.file_path == path
        assert "metadata read failure" in info["warnings"]

    def test_sparse_matroska_tags_are_mapped_for_video_library(self, tmp_dir):
        path = os.path.join(tmp_dir, "Show", "Season 1", "S01E02 - The Pilot.mkv")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1500.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {
                "TITLE": ["Episode Title"],
                "DIRECTOR": ["Series Creator"],
                "SHOW": ["The Show"],
                "GENRE": ["Drama"],
                "DATE": ["2024"],
            }

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.title == "Episode Title"
        assert track.artist == "Series Creator"
        assert track.album == "The Show"
        assert track.album_artist == "The Show"
        assert track.show_title == "The Show"
        assert track.season_number == 1
        assert track.episode_number == 2
        assert track.track_number == 2
        assert track.video_kind == "show"
        assert track.genre == "Drama"
        assert track.year == 2024
        assert info["parsed_ok"] is True

    def test_video_path_fallback_uses_series_and_season_when_tags_missing(self, tmp_dir):
        path = os.path.join(tmp_dir, "The Show", "Season 2", "S02E03 - Return_to_Form.1080p.mkv")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {}

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.title == "Return to Form"
        assert track.album == "Season 2"
        assert track.artist == "The Show"
        assert track.album_artist == "The Show"
        assert track.show_title == "The Show"
        assert track.season_number == 2
        assert track.episode_number == 3
        assert track.video_kind == "show"
        assert track.track_number == 3
        assert "basic metadata only" in info["warnings"]

    def test_mp4_tv_tags_populate_show_season_and_episode(self, tmp_dir):
        path = os.path.join(tmp_dir, "Library", "Example.m4v")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1800.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {
                "\xa9nam": ["The Episode"],
                "tvsh": ["The Series"],
                "tvsn": [4],
                "tves": [9],
                "\xa9gen": ["Drama"],
            }

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.title == "The Episode"
        assert track.show_title == "The Series"
        assert track.season_number == 4
        assert track.episode_number == 9
        assert track.track_number == 9
        assert track.video_kind == "show"
        assert info["parsed_ok"] is True

    def test_tv_show_folder_without_season_is_still_inferred_as_show(self, tmp_dir):
        path = os.path.join(tmp_dir, "TV Shows", "Death Note", "01 - Rebirth.mkv")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {}

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.show_title == "Death Note"
        assert track.video_kind == "show"
        assert track.episode_number == 1
        assert track.track_number == 1
        assert "basic metadata only" in info["warnings"]

    def test_specials_folder_stays_attached_to_show(self, tmp_dir):
        path = os.path.join(tmp_dir, "TV Shows", "Death Note", "Specials", "02 - L Change the World.mkv")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {}

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.show_title == "Death Note"
        assert track.album == "Specials"
        assert track.video_kind == "show"
        assert track.track_number == 2
        assert "basic metadata only" in info["warnings"]

    def test_slug_show_folder_and_s01_e01_filename_are_normalized(self, tmp_dir):
        path = os.path.join(
            tmp_dir,
            "TV Shows",
            "tales-from-the-cryptkeeper-full-series",
            "S01_E01 - While the Cat's Away-quicktime.mov",
        )
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {}

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.show_title == "tales from the cryptkeeper"
        assert track.album == "Season 1"
        assert track.title == "While the Cat's Away"
        assert track.season_number == 1
        assert track.episode_number == 1
        assert track.track_number == 1
        assert track.video_kind == "show"
        assert "basic metadata only" in info["warnings"]

    def test_noisy_archive_title_is_replaced_with_episode_title_from_filename(self, tmp_dir):
        path = os.path.join(
            tmp_dir,
            "TV Shows",
            "6teen (2004)",
            "Season 00",
            "6teen - S00E02 - Snow Job.mp4",
        )
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {
                "TITLE": ["6teen (2004-2010) Complete Series including specials - https://archive.org/details/6teen_2004_complete_series_202508"],
            }

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.show_title == "6teen"
        assert track.album == "Season 00"
        assert track.title == "Snow Job"
        assert track.season_number == 0
        assert track.episode_number == 2
        assert track.track_number == 2
        assert track.video_kind == "show"
        assert "basic metadata only" not in info["warnings"]

    def test_show_metadata_strips_trailing_year_and_keeps_year_field(self, tmp_dir):
        path = os.path.join(tmp_dir, "TV Shows", "6teen (2004)", "Season 1", "S01E01 - Pilot.mp4")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {
                "SHOW": ["6teen (2004)"],
                "ARTIST": ["6teen (2004)"],
                "TITLE": ["Pilot"],
            }

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.show_title == "6teen"
        assert track.artist == "6teen"
        assert track.album_artist == "6teen"
        assert track.year == 2004
        assert track.video_kind == "show"
        assert "basic metadata only" not in info["warnings"]

    def test_archive_style_episode_path_infers_show_title(self, tmp_dir):
        path = os.path.join(
            tmp_dir,
            "_Archive",
            "6teen_2004_complete_series_202508",
            "6teen S01E01 Take This Job and Squeeze It.mkv",
        )
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {}

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.show_title == "6teen"
        assert track.title == "Take This Job and Squeeze It"
        assert track.season_number == 1
        assert track.episode_number == 1
        assert track.track_number == 1
        assert track.video_kind == "show"
        assert "basic metadata only" in info["warnings"]

    def test_read_lyrics_prefers_local_lrc_sidecar(self, monkeypatch, tmp_dir):
        path = os.path.join(tmp_dir, "Song.mp3")
        with open(path, "wb") as f:
            f.write(b"audio")
        sidecar = os.path.splitext(path)[0] + ".lrc"
        with open(sidecar, "w", encoding="utf-8") as f:
            f.write("[00:01.00]<00:01.10>Hello")

        monkeypatch.setattr("services.metadata_reader.mutagen.File", lambda _path: None)

        assert read_lyrics(path) == "[00:01.00]<00:01.10>Hello"

    def test_media_drive_package_folder_infers_clean_show_title(self, tmp_dir):
        path = os.path.join(
            tmp_dir,
            "TV Shows",
            "_MediaDrive",
            "Pluribus.S01.COMPLETE.2160p.ATVP.WEB-DL.DV.HDR10+.MULTi.DDP5.1.Atmos.H265.MP4-BEN.THE.MEN",
            "Pluribus.S01E01.2160p.ATVP.WEB-DL.DV.HDR10+.MULTi[Ben The Men].mp4",
        )
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: None)

        assert track.show_title == "Pluribus"
        assert track.album_artist == "Pluribus"
        assert track.artist == "Pluribus"
        assert track.season_number == 1
        assert track.episode_number == 1
        assert track.video_kind == "show"
        assert "metadata read failure" in info["warnings"]

    def test_video_path_fallback_runs_when_mutagen_returns_none(self, tmp_dir):
        path = os.path.join(
            tmp_dir,
            "TV Shows",
            "Another (2012)",
            "Season 01",
            "Another (2012) - S01E01.mkv",
        )
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: None)

        assert track.show_title == "Another"
        assert track.title == "Another"
        assert track.season_number == 1
        assert track.episode_number == 1
        assert track.track_number == 1
        assert track.video_kind == "show"
        assert "metadata read failure" in info["warnings"]

    def test_filename_prefix_can_seed_show_title_when_folder_is_movies(self, tmp_dir):
        path = os.path.join(tmp_dir, "Movies", "Star Trek TOS S01E00 The Cage.m4v")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: None)

        assert track.show_title == "Star Trek TOS"
        assert track.title == "The Cage"
        assert track.season_number == 1
        assert track.episode_number == 0
        assert track.video_kind == "show"
        assert "metadata read failure" in info["warnings"]

    def test_movie_title_strips_trailing_year_and_keeps_year_field(self, tmp_dir):
        path = os.path.join(tmp_dir, "Movies", "A Hard Day's Night (1964).mp4")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {"TITLE": ["A Hard Day's Night (1964)"]}

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.title == "A Hard Day's Night"
        assert track.year == 1964
        assert track.video_kind == "movie"
        assert "basic metadata only" not in info["warnings"]

    def test_movie_release_filename_is_normalized(self, tmp_dir):
        path = os.path.join(tmp_dir, "Movies", "Flow.2024.2160p.4K.WEB.x265.10bit.AAC5.1.mkv")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: None)

        assert track.title == "Flow"
        assert track.album == "Flow"
        assert track.year == 2024
        assert track.video_kind == "movie"
        assert "metadata read failure" in info["warnings"]

    def test_bracketed_release_filename_is_normalized(self, tmp_dir):
        path = os.path.join(tmp_dir, "Movies", "Drake And Josh Go Hollywood[2006]DVDRip[Eng][Xvid].avi")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: None)

        assert track.title == "Drake And Josh Go Hollywood"
        assert track.album == "Drake And Josh Go Hollywood"
        assert track.year == 2006
        assert track.video_kind == "movie"
        assert "metadata read failure" in info["warnings"]

    def test_release_noisy_show_package_name_and_episode_stem_are_normalized(self, tmp_dir):
        path = os.path.join(
            tmp_dir,
            "Movies",
            "_MediaDrive",
            "The.Beatles.Get.Back.2021.VOSTF.S01.COMPLETE.1080p.WEBRip.E-AC3.x264",
            "The.Beatles.Get.Back.2021.VOSTF.S01E01.1080p.WEBRip.E-AC3.x264@3000kbs.mkv",
        )
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: None)

        assert track.show_title == "The Beatles Get Back"
        assert track.artist == "The Beatles Get Back"
        assert track.album_artist == "The Beatles Get Back"
        assert track.album == "Season 1"
        assert track.title == "Episode 1"
        assert track.season_number == 1
        assert track.episode_number == 1
        assert track.year == 2021
        assert track.video_kind == "show"
        assert "metadata read failure" in info["warnings"]

    def test_trailing_parenthetical_note_with_year_is_removed_from_title(self, tmp_dir):
        path = os.path.join(tmp_dir, "Home Videos", "The Beatles Anthology (ABC Broadcast Version 1995).mp4")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: None)

        assert track.title == "The Beatles Anthology"
        assert track.year == 1995
        assert track.video_kind == "home_video"
        assert "metadata read failure" in info["warnings"]

    def test_year_from_full_date_tag_is_normalized_to_four_digits(self, tmp_dir):
        path = os.path.join(tmp_dir, "Sports", "Hockey", "Game.mp4")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {
                "TITLE": ["NHL Game"],
                "DATE": ["2023-02-08"],
            }

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.title == "NHL Game"
        assert track.year == 2023
        assert track.video_kind == "movie"
        assert "basic metadata only" not in info["warnings"]

    def test_leading_filename_date_overrides_unrelated_container_year(self, tmp_dir):
        path = os.path.join(
            tmp_dir,
            "Sports",
            "Hockey",
            "1980-11-08 - NHL - Los Angeles Kings vs Montreal Canadiens [7wJrYuuTauI].mp4",
        )
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {
                "TITLE": ["NHL Nov.08/1980 Los Angeles Kings - Montreal Canadiens"],
                "DATE": ["2023-02-08"],
            }

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.title == "NHL Nov 08/1980 Los Angeles Kings - Montreal Canadiens"
        assert track.year == 1980
        assert track.video_kind == "movie"
        assert "basic metadata only" not in info["warnings"]

    def test_generic_specials_show_title_is_replaced_by_parent_show(self, tmp_dir):
        path = os.path.join(tmp_dir, "TV Shows", "Death Note (2006)", "Specials", "01 - Special.mkv")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(b"video")

        class FakeInfo:
            length = 1320.0
            bitrate = 0
            sample_rate = 0
            channels = 2

        class FakeAudio:
            info = FakeInfo()
            tags = {"SHOW": ["Specials"]}

        track, info = read_metadata_details(path, mutagen_file_func=lambda _path: FakeAudio())

        assert track.show_title == "Death Note"
        assert track.album == "Specials"
        assert track.video_kind == "show"
        assert "basic metadata only" not in info["warnings"]

    def test_different_file_hash_needs_resync(self):
        t = Track(
            synced_to_device=1,
            metadata_hash="same",
            last_synced_metadata_hash="same",
            file_hash="new_file",
            last_synced_file_hash="old_file",
        )
        assert t.needs_resync

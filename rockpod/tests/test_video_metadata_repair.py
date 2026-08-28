"""Filename and provider repairs for video shows, seasons and episodes."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from models.track import Track
from services.metadata_reader import (
    _apply_video_path_fallback,
    _canonicalize_series_name,
    _clean_video_title,
    _title_parts_from_stem,
    _youtube_full_episode_parts,
    looks_like_release_group,
    looks_like_specials_folder,
)
from services.library_scanner import _preserve_verified_video_metadata
from services.video_metadata_backfill import (
    VideoMetadataBackfill,
    group_show_rows,
)


class StubProvider:
    """A metadata provider with fixed answers and no network."""

    def __init__(self, shows=None, seasons=None, episodes=None):
        self.shows = list(shows or [])
        self.seasons = list(seasons or [])
        self.episodes = dict(episodes or {})
        self.season_calls = 0

    def lookup_by_imdb(self, imdb_id):
        for show in self.shows:
            if show.get("imdb_id") == imdb_id:
                return show
        return None

    def search_shows(self, query, year=None):
        results = list(self.shows)
        if year:
            exact = [s for s in results if s.get("year") == year]
            if exact:
                return exact
        return results

    def list_seasons(self, provider_id):
        self.season_calls += 1
        return list(self.seasons)

    def list_episodes(self, provider_id):
        return dict(self.episodes)


def _show(title, imdb_id, year, artwork_url="https://example.invalid/show.jpg"):
    return {
        "provider": "tvmaze",
        "provider_id": "1",
        "imdb_id": imdb_id,
        "title": title,
        "media_type": "tv_show",
        "year": year,
        "genre": "Comedy",
        "plot_short": "A synopsis.",
        "plot_long": "A synopsis.",
        "artwork_url": artwork_url,
        "content_rating": "",
    }


class TestPreSeasonFolders:
    """A "Pre-Season 5" folder holds specials, it is not a show called Pre."""

    def test_folder_is_recognised_as_specials(self):
        assert looks_like_specials_folder("Pre-Season 5")
        assert looks_like_specials_folder("Preseason")
        assert not looks_like_specials_folder("Season 5")

    def test_series_name_never_becomes_pre(self):
        assert _canonicalize_series_name("Pre-Season 5") == ""

    def test_show_comes_from_the_parent_series_folder(self):
        track = Track(
            file_path="/v/Trailer Park Boys/Pre-Season 5/Christmas Special.mkv",
            media_type="video",
        )
        _apply_video_path_fallback(
            track, "/v/Trailer Park Boys/Pre-Season 5/Christmas Special.mkv"
        )
        assert track.show_title == "Trailer Park Boys"
        assert track.season_number == 0
        assert track.album == "Specials"


class TestSceneReleaseTitles:
    """A release group left in a filename is not an episode title."""

    def test_group_name_is_replaced_by_the_episode_number(self):
        title, season, episode = _title_parts_from_stem(
            "The.Office.US.S02E01.1080p.BluRay.x265-RARBG"
        )
        assert (title, season, episode) == ("Episode 1", 2, 1)

    def test_a_real_episode_title_survives(self):
        title, season, episode = _title_parts_from_stem(
            "Show.Name.S01E02.The.Real.Title.1080p.WEB-DL"
        )
        assert (title, season, episode) == ("The Real Title", 1, 2)

    def test_bracketed_uploader_tag_is_dropped(self):
        assert _clean_video_title("Some Episode [YIFY]") == "Some Episode"

    def test_ordinary_words_are_not_treated_as_groups(self):
        assert looks_like_release_group("RARBG")
        assert not looks_like_release_group("The Fire")


class TestSeasonPackNames:
    def test_seasons_range_is_stripped_from_the_series_name(self):
        assert (
            _canonicalize_series_name(
                "Avatar The Last Airbender 2005 Seasons 1 to 3 Complete 720p WEB x264 [i_c]"
            )
            == "Avatar The Last Airbender"
        )


class TestYoutubeFullEpisodeNames:
    def test_show_is_read_from_the_upload_title(self):
        assert _youtube_full_episode_parts(
            "Episode 1 _ Hey Arnold _ FULL EPISODE _ RETRO RERUN"
        ) == ("Hey Arnold", 1)

    def test_plain_episode_dash_title_is_left_alone(self):
        # Without an upload marker the text after the dash is the episode
        # title, and reading it as a show name would invent a series.
        assert _youtube_full_episode_parts("Episode 3 - The Real Title") == ("", 3)


class TestShowGrouping:
    def test_punctuation_variants_collapse_into_one_series(self):
        rows = [
            {"id": 1, "video_kind": "show", "show_title": "Kenny vs. Spenny"},
            {"id": 2, "video_kind": "show", "show_title": "Kenny vs Spenny"},
        ]
        groups = group_show_rows(rows)
        assert len(groups) == 1
        group = next(iter(groups.values()))
        assert len(group.rows) == 2
        assert group.titles == {"Kenny vs. Spenny", "Kenny vs Spenny"}


class TestBackfillPlanning:
    def _backfill(self, tmp_path, provider, downloaded=None):
        def download(url):
            if downloaded is not None:
                downloaded.append(url)
            return b"\xff\xd8\xff\xd9"

        return VideoMetadataBackfill(
            catalog_dir=tmp_path,
            provider=provider,
            download_image=download,
        )

    def test_empty_fields_are_filled_and_existing_values_kept(self, tmp_path):
        provider = StubProvider(
            shows=[_show("The Office", "tt0386676", 2005)],
            episodes={(2, 1): {"title": "The Dundies", "plot_short": "Awards.",
                               "plot_long": "Awards.", "year": 2005}},
        )
        rows = [{
            "id": 1, "video_kind": "show", "show_title": "The Office US",
            "season_number": 2, "episode_number": 1, "title": "Episode 1",
            "genre": "Sitcom", "year": None, "imdb_id": "", "plot_short": "",
        }]
        plan = self._backfill(tmp_path, provider).plan_show(
            next(iter(group_show_rows(rows).values()))
        )
        values = plan["updates"][0]["values"]
        assert values["imdb_id"] == "tt0386676"
        assert values["year"] == 2005
        assert values["title"] == "The Dundies"
        assert values["plot_short"] == "Awards."
        assert values["show_title"] == "The Office"
        # A genre the library already carries is not replaced.
        assert "genre" not in values

    def test_locked_rows_are_never_rewritten(self, tmp_path):
        provider = StubProvider(shows=[_show("The Office", "tt0386676", 2005)])
        rows = [{
            "id": 1, "video_kind": "show", "show_title": "The Office US",
            "season_number": 2, "episode_number": 1, "metadata_locked": 1,
        }]
        plan = self._backfill(tmp_path, provider).plan_show(
            next(iter(group_show_rows(rows).values()))
        )
        assert plan["updates"] == []

    def test_library_spellings_are_recorded_as_aliases(self, tmp_path):
        provider = StubProvider(shows=[_show("The Office", "tt0386676", 2005)])
        rows = [{
            "id": 1, "video_kind": "show", "show_title": "The Office US",
            "season_number": 2, "episode_number": 1,
        }]
        plan = self._backfill(tmp_path, provider).plan_show(
            next(iter(group_show_rows(rows).values()))
        )
        assert "The Office US" in plan["entry"]["aliases"]

    def test_a_title_without_published_art_gets_no_art(self, tmp_path):
        provider = StubProvider(shows=[_show("No Art Show", "tt1", 1999, "")])
        rows = [{
            "id": 1, "video_kind": "show", "show_title": "No Art Show",
            "season_number": 1, "episode_number": 1,
        }]
        plan = self._backfill(tmp_path, provider).plan_show(
            next(iter(group_show_rows(rows).values()))
        )
        assert not plan["entry"].get("show_art")

    def test_seasons_absent_from_the_library_are_not_fetched(self, tmp_path):
        provider = StubProvider(shows=[_show("Some Show", "tt2", 2001)])
        rows = [{
            "id": 1, "video_kind": "show", "show_title": "Some Show",
            "season_number": None, "episode_number": 7,
        }]
        self._backfill(tmp_path, provider).plan_show(
            next(iter(group_show_rows(rows).values()))
        )
        assert provider.season_calls == 0

    def test_season_covers_are_stored_per_season(self, tmp_path):
        provider = StubProvider(
            shows=[_show("Some Show", "tt2", 2001)],
            seasons=[
                {"number": 1, "artwork_url": "https://example.invalid/s1.jpg"},
                {"number": 2, "artwork_url": ""},
            ],
        )
        rows = [
            {"id": 1, "video_kind": "show", "show_title": "Some Show",
             "season_number": 1, "episode_number": 1},
            {"id": 2, "video_kind": "show", "show_title": "Some Show",
             "season_number": 2, "episode_number": 1},
        ]
        entry = self._backfill(tmp_path, provider).plan_show(
            next(iter(group_show_rows(rows).values()))
        )["entry"]
        assert entry["seasons"]["1"] == "some-show-season-1.jpg"
        # No published cover for season 2, so it shows the series cover
        # rather than anything invented for it.
        assert entry["seasons"]["2"] == entry["show_art"]
        assert (tmp_path / "some-show-season-1.jpg").is_file()

    def test_a_remake_does_not_steal_the_older_show(self, tmp_path):
        provider = StubProvider(
            shows=[
                _show("Avatar: The Last Airbender", "tt9018736", 2024),
                _show("Avatar: The Last Airbender", "tt0417299", 2005),
            ]
        )
        rows = [{
            "id": 1, "video_kind": "show", "year": 2005,
            "show_title": "Avatar The Last Airbender",
            "season_number": 1, "episode_number": 1,
        }]
        plan = self._backfill(tmp_path, provider).plan_show(
            next(iter(group_show_rows(rows).values())), fetch_artwork=False
        )
        assert plan["entry"]["imdb_id"] == "tt0417299"


class TestRescanPreservesVerifiedMatches:
    """A rescan must not overrule a deliberate video match."""

    def _scanned(self):
        return {
            "file_path": "/v/The.Office.US.S02E01.x265-RARBG.mp4",
            "media_type": "video",
            "title": "Episode 1",
            "show_title": "The Office US",
            "year": None,
            "genre": "",
            "video_kind": "show",
            "file_size": 10,
        }

    def test_matched_row_keeps_its_identity(self):
        cached = {"imdb_id": "tt0386676", "metadata_locked": 0}
        kept = _preserve_verified_video_metadata(self._scanned(), cached)
        assert "title" not in kept
        assert "show_title" not in kept
        assert "year" not in kept
        # Filesystem facts still refresh.
        assert kept["file_size"] == 10

    def test_locked_row_keeps_its_identity(self):
        cached = {"imdb_id": "", "metadata_locked": 1}
        assert "show_title" not in _preserve_verified_video_metadata(
            self._scanned(), cached
        )

    def test_unmatched_row_is_refreshed_from_the_filename(self):
        cached = {"imdb_id": "", "metadata_locked": 0}
        kept = _preserve_verified_video_metadata(self._scanned(), cached)
        assert kept["show_title"] == "The Office US"

    def test_a_new_row_is_written_in_full(self):
        kept = _preserve_verified_video_metadata(self._scanned(), None)
        assert kept["show_title"] == "The Office US"

    def test_audio_rows_are_untouched(self):
        row = {"media_type": "audio", "title": "Song", "artist": "Band"}
        cached = {"imdb_id": "tt0386676", "metadata_locked": 0}
        assert _preserve_verified_video_metadata(row, cached) == row

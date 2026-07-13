"""Targeted tests for YouTube auto-metadata matching helper functions."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from services.youtube_import_metadata import (
    _candidate_queries,
    _extract_episode_hints,
    build_youtube_import_metadata_payload,
    choose_video_metadata_match,
    youtube_metadata_lookup_query,
)


class _FakeMetadataService:
    def __init__(self, search_map=None, episode_map=None):
        self.search_map = dict(search_map or {})
        self.episode_map = dict(episode_map or {})
        self.search_calls = []
        self.episode_calls = []

    def search(self, query, media_type, year=None):
        self.search_calls.append((query, media_type, year))
        return list(self.search_map.get((query, media_type, year), []))

    def lookup_episode(self, show_title, season, episode, track_title=None):
        self.episode_calls.append((show_title, season, episode, track_title))
        return self.episode_map.get((show_title, season, episode, track_title), None)


class TestYoutubeImportMetadataHelpers(unittest.TestCase):
    def test_lookup_query_prefers_pending_hint(self):
        track = {"title": "From Title", "file_path": "/video/sample-file.mpg"}
        pending = {"query_hint": "Pending Title"}
        self.assertEqual(youtube_metadata_lookup_query(track, pending), "Pending Title")

    def test_lookup_query_falls_back_to_show_or_title(self):
        track = {"show_title": "Show Name", "title": "Episode", "file_path": "/video/sample-file.mpg"}
        self.assertEqual(youtube_metadata_lookup_query(track, {}), "Show Name")
        track = {"title": "Episode Name", "file_path": "/video/sample-file.mpg"}
        self.assertEqual(youtube_metadata_lookup_query(track, {}), "Episode Name")

    def test_lookup_query_falls_back_to_filename_stem(self):
        track = {"file_path": "/tmp/Kenny_vs._Spenny-S01E01.mpg"}
        self.assertEqual(youtube_metadata_lookup_query(track, {}), "Kenny vs. Spenny S01E01")

    def test_candidate_queries_include_stripped_episode_variants(self):
        track = {
            "title": "Kenny vs Spenny S01E01",
            "file_path": "/tmp/Kenny_vs_Spenny_S01E01.mpg",
        }
        pending = {"query_hint": "Kenny vs Spenny S01E01"}
        hints = _extract_episode_hints(track, pending)
        queries = _candidate_queries(track, pending, hints)

        self.assertIn("Kenny vs Spenny", queries)
        self.assertIn("Kenny vs Spenny S01E01", queries)

    def test_parse_episode_hint_from_plain_episode_suffix(self):
        track = {
            "title": "Kenny vs Spenny Episode 1",
            "file_path": "/tmp/Kenny_vs_Spenny_Episode_1.mpg",
        }
        pending = {"query_hint": "Kenny vs Spenny Episode 1"}
        hints = _extract_episode_hints(track, pending)

        self.assertEqual(hints["show_title"], "Kenny vs Spenny")
        self.assertEqual(hints["season_number"], 1)
        self.assertEqual(hints["episode_number"], 1)

    def test_parse_competition_show_suffix_without_year_or_episode_number(self):
        track = {
            "title": "Who Can Stand Up The Longest - Kenny vs. Spenny (HD)",
            "file_path": "/tmp/Who Can Stand Up The Longest_ - Kenny vs. Spenny (HD).mpg",
        }
        hints = _extract_episode_hints(track, {"query_hint": track["title"]})

        self.assertEqual(hints["show_title"], "Kenny vs. Spenny")
        self.assertEqual(hints["season_number"], 1)
        self.assertEqual(hints["episode_title"], "Who Can Stand Up The Longest")

    def test_payload_uses_stripped_show_query_for_show_lookup(self):
        service = _FakeMetadataService(
            search_map={
                ("Kenny vs Spenny", "tv_show", None): [
                    {"media_type": "tv_show", "title": "Kenny vs Spenny"}
                ],
            }
        )
        payload = build_youtube_import_metadata_payload(
            {
                "media_type": "video",
                "title": "Kenny vs Spenny S01E01",
                "file_path": "/tmp/Kenny_vs_Spenny_S01E01.mpg",
            },
            {"query_hint": "Kenny vs Spenny S01E01"},
            service,
        )

        self.assertIsNotNone(payload)
        self.assertEqual(payload["updates"]["video_kind"], "show")
        calls = service.search_calls
        self.assertIn(("Kenny vs Spenny", "tv_show", None), calls)

    def test_choose_video_metadata_match_prefers_exact_or_containing_title(self):
        results = [
            {"title": "Episode 1"},
            {"title": "Kenny vs. Spenny"},
            {"title": "Kenny vs. Spenny - Special"},
        ]
        self.assertEqual(
            choose_video_metadata_match("Kenny vs. Spenny", results)["title"],
            "Kenny vs. Spenny",
        )
        self.assertEqual(
            choose_video_metadata_match("kenny", results)["title"],
            "Kenny vs. Spenny",
        )
        self.assertEqual(choose_video_metadata_match("", results), results[0])

    def test_payload_skips_locked_or_pre_typed_metadata(self):
        service = _FakeMetadataService()
        track = {"media_type": "video", "metadata_locked": True}
        self.assertIsNone(build_youtube_import_metadata_payload(track, {}, service))
        track = {"media_type": "video", "metadata_source": "online"}
        self.assertIsNone(build_youtube_import_metadata_payload(track, {}, service))
        track = {"media_type": "video", "metadata_source": "filename"}
        self.assertIsNotNone(
            build_youtube_import_metadata_payload(
                {"media_type": "video", "metadata_source": "filename", "title": "Movie Title"},
                {"query_hint": "Movie Title"},
                _FakeMetadataService({
                    ("Movie Title", "movie", None): [{"media_type": "movie", "title": "Movie Title"}]
                }),
            )
        )

    def test_payload_matches_movie_and_populates_updates(self):
        service = _FakeMetadataService({
            ("Kenny vs Spenny", "movie", None): [
                {
                    "media_type": "movie",
                    "title": "Kenny vs Spenny",
                    "imdb_id": "tt001",
                    "tmdb_id": "tm001",
                    "genre": "Comedy",
                    "year": 2003,
                    "plot_short": "Episode chaos",
                    "artwork_url": "http://art",
                }
            ]
        })
        payload = build_youtube_import_metadata_payload(
            {"media_type": "video", "title": "Kenny vs Spenny"},
            {"query_hint": "Kenny vs Spenny"},
            service,
        )
        self.assertIsNotNone(payload)
        updates = payload["updates"]
        self.assertEqual(updates["metadata_source"], "online")
        self.assertEqual(updates["video_kind"], "movie")
        self.assertEqual(updates["imdb_id"], "tt001")
        self.assertEqual(updates["title"], "Kenny vs Spenny")
        self.assertEqual(payload["artwork_url"], "http://art")
        self.assertEqual(service.search_calls[0], ("Kenny vs Spenny", "movie", None))

    def test_payload_does_not_fallback_to_tv_when_no_show_evidence(self):
        service = _FakeMetadataService({
            ("Pitch", "movie", None): [],
            ("Pitch", "tv_show", None): [
                {"media_type": "tv_show", "title": "Fever Pitch"},
            ],
        })
        payload = build_youtube_import_metadata_payload(
            {"media_type": "video", "title": "Pitch"},
            {"query_hint": "Pitch"},
            service,
        )
        self.assertIsNone(payload)

    def test_payload_keeps_download_metadata_when_online_lookup_misses(self):
        payload = build_youtube_import_metadata_payload(
            {"media_type": "video", "title": "Downloaded Video"},
            {
                "query_hint": "Show Name S02E03",
                "show_title": "Show Name",
                "season_number": 2,
                "episode_number": 3,
                "episode_title": "The Episode",
                "uploader": "Official Channel",
                "year": 2024,
                "description": "An official episode description.",
                "thumbnail": "https://example.test/thumb.jpg",
                "source_url": "https://youtube.test/watch?v=abc",
            },
            _FakeMetadataService(),
        )

        updates = payload["updates"]
        self.assertEqual(updates["metadata_source"], "youtube")
        self.assertEqual(updates["show_title"], "Show Name")
        self.assertEqual(updates["season_number"], 2)
        self.assertEqual(updates["episode_number"], 3)
        self.assertEqual(updates["track_number"], 3)
        self.assertEqual(updates["album"], "Season 02")
        self.assertEqual(updates["title"], "The Episode")
        self.assertEqual(payload["artwork_url"], "https://example.test/thumb.jpg")

    def test_payload_matches_tv_show_episode_and_uses_episode_lookup(self):
        service = _FakeMetadataService(
            search_map={
                ("Kenny vs. Spenny", "tv_show", None): [
                    {"media_type": "tv_show", "title": "Kenny vs. Spenny"},
                ]
            },
            episode_map={
                ("Kenny vs. Spenny", 1, 2, "Pilot"): {
                    "title": "Season 1 Episode 2",
                    "plot_short": "Episode two",
                },
            },
        )
        payload = build_youtube_import_metadata_payload(
            {
                "media_type": "video",
                "video_kind": "show",
                "show_title": "Kenny vs. Spenny",
                "season_number": 1,
                "episode_number": 2,
                "title": "Pilot",
            },
            {"query_hint": "Kenny vs. Spenny"},
            service,
        )
        self.assertIsNotNone(payload)
        updates = payload["updates"]
        self.assertEqual(updates["video_kind"], "show")
        self.assertEqual(updates["title"], "Season 1 Episode 2")
        self.assertEqual(updates["plot_short"], "Episode two")
        self.assertEqual(service.episode_calls, [("Kenny vs. Spenny", 1, 2, "Pilot")])

    def test_payload_allows_show_lookup_when_track_is_already_marked_online(self):
        service = _FakeMetadataService(
            search_map={
                ("Triumph of the Will S01E01", "tv_show", None): [
                    {"media_type": "tv_show", "title": "Triumph of the Will", "imdb_id": "tt123"}
                ],
            },
            episode_map={
                ("Triumph of the Will", 1, 1, "Triumph of the Will S01E01"): {
                    "title": "The Opening Episode",
                },
            },
        )

        payload = build_youtube_import_metadata_payload(
            {
                "media_type": "video",
                "metadata_source": "online",
                "title": "Triumph of the Will S01E01",
                "file_path": "/tmp/Triumph_of_the_Will_S01E01.mpg",
            },
            {"query_hint": "Triumph of the Will S01E01"},
            service,
        )

        self.assertIsNotNone(payload)
        updates = payload["updates"]
        self.assertEqual(updates["video_kind"], "show")
        self.assertEqual(updates["show_title"], "Triumph of the Will")

    def test_payload_resolves_triumph_of_the_will_episode_by_show_hint(self):
        service = _FakeMetadataService(
            search_map={
                ("Triumph of the Will", "tv_show", None): [
                    {"media_type": "tv_show", "title": "Triumph of the Will"},
                ],
            },
            episode_map={
                ("Triumph of the Will", 1, 1, "Triumph of the Will S01E01"): {
                    "title": "Episode 1",
                    "plot_short": "Historical opening chapter",
                }
            },
        )

        payload = build_youtube_import_metadata_payload(
            {
                "media_type": "video",
                "video_kind": "show",
                "title": "Triumph of the Will S01E01",
                "file_path": "/tmp/Triumph_of_the_Will_S01E01.mpg",
            },
            {"query_hint": "Triumph of the Will"},
            service,
        )

        self.assertIsNotNone(payload)
        updates = payload["updates"]
        self.assertEqual(updates["video_kind"], "show")
        self.assertEqual(updates["title"], "Episode 1")
        self.assertEqual(updates["season_number"], 1)
        self.assertEqual(updates["episode_number"], 1)

    def test_extract_episode_hints_applies_triumph_override_without_episode_marker(self):
        track = {
            "title": "Triumph of the Will - The French ReConnection",
            "file_path": "/tmp/Triumph of the Will - The French ReConnection.mp4",
            "media_type": "video",
            "video_kind": "movie",
        }
        hints = _extract_episode_hints(track, {"query_hint": "Triumph of the Will - The French ReConnection"})
        self.assertEqual(hints["show_title"], "Triumph of the Will")
        self.assertEqual(hints["season_number"], 1)
        self.assertEqual(hints["episode_number"], 5)
        self.assertEqual(hints["episode_title"], "The French ReConnection")

    def test_payload_uses_triumph_override_even_when_already_marked_online(self):
        service = _FakeMetadataService(
            search_map={
                ("Triumph of the Will", "tv_show", None): [
                    {"media_type": "tv_show", "title": "Triumph of the Will"},
                ],
                ("Triumph of the Will", "movie", None): [
                    {"media_type": "movie", "title": "Triumph of the Will"},
                ],
                ("Triumph of the Will", 1, 5, "The French ReConnection"): {
                    "title": "The French ReConnection",
                    "plot_short": "Episode details",
                },
            },
        )
        payload = build_youtube_import_metadata_payload(
            {
                "media_type": "video",
                "metadata_source": "online",
                "title": "Triumph of the Will - The French ReConnection",
                "file_path": "/tmp/Triumph_of_the_Will_-_The_French_ReConnection.mp4",
            },
            {"query_hint": "Triumph of the Will - The French ReConnection"},
            service,
        )

        self.assertIsNotNone(payload)
        updates = payload["updates"]
        self.assertEqual(updates["video_kind"], "show")
        self.assertEqual(updates["show_title"], "Triumph of the Will")
        self.assertEqual(updates["season_number"], 1)
        self.assertEqual(updates["episode_number"], 5)

    def test_payload_returns_none_without_search_results(self):
        service = _FakeMetadataService()
        payload = build_youtube_import_metadata_payload(
            {"media_type": "video", "title": "Unknown"},
            {"query_hint": "Unknown"},
            service,
        )
        self.assertIsNone(payload)

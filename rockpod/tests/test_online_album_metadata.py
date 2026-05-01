"""Tests for online album metadata lookup."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from services.online_album_metadata import ITunesAlbumMetadataLookup, OnlineAlbumMetadataNetworkError


def test_fetch_album_metadata_combines_album_details_and_lyrics(monkeypatch):
    lookup = ITunesAlbumMetadataLookup(storefront="us", min_interval_seconds=0.0)

    def fake_get_json(base_url, params=None, timeout=None, return_empty_on_404=False):
        if base_url == "https://itunes.apple.com/search":
            return {
                "results": [
                    {
                        "collectionId": 123,
                        "collectionName": "Test Album",
                        "artistName": "Test Artist",
                        "collectionType": "Album",
                    }
                ]
            }
        if base_url == "https://itunes.apple.com/lookup":
            return {
                "results": [
                    {
                        "wrapperType": "collection",
                        "collectionId": 123,
                        "collectionName": "Test Album",
                        "artistName": "Test Artist",
                        "releaseDate": "2024-02-03T08:00:00Z",
                        "primaryGenreName": "Rock",
                        "trackCount": 2,
                        "copyright": "2024 Test Label",
                        "collectionViewUrl": "https://example.com/album",
                        "artworkUrl100": "https://example.com/100x100bb.jpg",
                    },
                    {
                        "wrapperType": "track",
                        "trackName": "Song One",
                        "artistName": "Test Artist",
                        "composerName": "Composer One",
                        "trackNumber": 1,
                        "discNumber": 1,
                        "trackTimeMillis": 180000,
                    },
                    {
                        "wrapperType": "track",
                        "trackName": "Song Two",
                        "artistName": "Test Artist",
                        "composerName": "Composer Two",
                        "trackNumber": 2,
                        "discNumber": 1,
                        "trackTimeMillis": 210000,
                    },
                ]
            }
        if base_url == "https://lrclib.net/api/search":
            params = params or {}
            if params.get("track_name") == "Song One":
                return [
                    {
                        "trackName": "Song One",
                        "artistName": "Test Artist",
                        "syncedLyrics": "[00:01.00]<00:01.10>lyrics one",
                        "plainLyrics": "lyrics one",
                    }
                ]
            return []
        if base_url.startswith("https://api.lyrics.ovh/v1/"):
            return {}
        raise AssertionError(f"unexpected URL: {base_url}")

    monkeypatch.setattr(lookup, "_get_json", fake_get_json)

    result = lookup.fetch_album_metadata("Test Album", "Test Artist", include_lyrics=True)

    assert result["album"] == "Test Album"
    assert result["artist"] == "Test Artist"
    assert result["year"] == 2024
    assert result["primary_genre"] == "Rock"
    assert result["track_count"] == 2
    assert result["artwork_url"] == "https://example.com/1200x1200bb.jpg"
    assert len(result["tracks"]) == 2
    assert result["tracks"][0]["lyrics"] == "[00:01.00]<00:01.10>lyrics one"
    assert result["tracks"][1]["lyrics"] == ""


def test_fetch_album_metadata_returns_none_when_no_match(monkeypatch):
    lookup = ITunesAlbumMetadataLookup(storefront="us", min_interval_seconds=0.0)
    monkeypatch.setattr(lookup, "_get_json", lambda *args, **kwargs: {"results": []})

    result = lookup.fetch_album_metadata("Missing", "Unknown", include_lyrics=False)

    assert result is None


def test_fetch_album_metadata_splits_artist_album_combo_when_artist_missing(monkeypatch):
    lookup = ITunesAlbumMetadataLookup(storefront="us", min_interval_seconds=0.0)
    search_calls = []

    def fake_get_json(base_url, params=None, timeout=None, return_empty_on_404=False):
        if base_url == "https://itunes.apple.com/search":
            params = params or {}
            search_calls.append((params.get("term"),))
            if params.get("term") == "Riot Paramore":
                return {
                    "results": [
                        {
                            "collectionId": 123,
                            "collectionName": "Riot!",
                            "artistName": "Paramore",
                            "collectionType": "Album",
                        }
                    ]
                }
            return {"results": []}
        if base_url == "https://itunes.apple.com/lookup":
            return {
                "results": [
                    {
                        "wrapperType": "collection",
                        "collectionId": 123,
                        "collectionName": "Riot!",
                        "artistName": "Paramore",
                        "releaseDate": "2007-06-12T07:00:00Z",
                        "primaryGenreName": "Alternative",
                        "trackCount": 1,
                        "artworkUrl100": "https://example.com/100x100bb.jpg",
                    },
                    {
                        "wrapperType": "track",
                        "trackName": "Misery Business",
                        "artistName": "Paramore",
                        "trackNumber": 1,
                        "discNumber": 1,
                        "trackTimeMillis": 211000,
                    },
                ]
            }
        if base_url == "https://lrclib.net/api/search":
            return []
        if base_url.startswith("https://api.lyrics.ovh/v1/"):
            return {}
        raise AssertionError(f"unexpected URL: {base_url}")

    monkeypatch.setattr(lookup, "_get_json", fake_get_json)

    result = lookup.fetch_album_metadata("Paramore - Riot", "", include_lyrics=False)

    assert result is not None
    assert result["album"] == "Riot!"
    assert result["artist"] == "Paramore"
    assert ("Riot Paramore",) in search_calls


def test_lookup_variants_ignores_unknown_artist_placeholder():
    lookup = ITunesAlbumMetadataLookup(storefront="us", min_interval_seconds=0.0)

    variants = lookup._lookup_variants("Riot!", "Unknown Artist")

    assert ("Riot!", "") in variants
    assert ("Riot!", "Unknown Artist") not in variants


def test_fetch_album_metadata_ignores_lyrics_network_failure(monkeypatch):
    lookup = ITunesAlbumMetadataLookup(storefront="us", min_interval_seconds=0.0)

    def fake_get_json(base_url, params=None, timeout=None, return_empty_on_404=False):
        if base_url == "https://itunes.apple.com/search":
            return {
                "results": [
                    {
                        "collectionId": 123,
                        "collectionName": "Riot!",
                        "artistName": "Paramore",
                        "collectionType": "Album",
                    }
                ]
            }
        if base_url == "https://itunes.apple.com/lookup":
            return {
                "results": [
                    {
                        "wrapperType": "collection",
                        "collectionId": 123,
                        "collectionName": "Riot!",
                        "artistName": "Paramore",
                        "releaseDate": "2007-06-12T07:00:00Z",
                        "primaryGenreName": "Alternative",
                        "trackCount": 1,
                        "artworkUrl100": "https://example.com/100x100bb.jpg",
                    },
                    {
                        "wrapperType": "track",
                        "trackName": "Misery Business",
                        "artistName": "Paramore",
                        "trackNumber": 1,
                        "discNumber": 1,
                        "trackTimeMillis": 211000,
                    },
                ]
            }
        raise AssertionError(f"unexpected URL: {base_url}")

    monkeypatch.setattr(lookup, "_get_json", fake_get_json)
    monkeypatch.setattr(
        lookup,
        "fetch_track_lyrics",
        lambda artist, title: (_ for _ in ()).throw(OnlineAlbumMetadataNetworkError("lyrics api unavailable")),
    )

    result = lookup.fetch_album_metadata("Riot!", "Paramore", include_lyrics=True)

    assert result is not None
    assert result["album"] == "Riot!"
    assert result["tracks"][0]["lyrics"] == ""


def test_fetch_album_metadata_returns_partial_result_when_lookup_has_no_data(monkeypatch):
    lookup = ITunesAlbumMetadataLookup(storefront="us", min_interval_seconds=0.0)

    def fake_get_json(base_url, params=None, timeout=None, return_empty_on_404=False):
        if base_url == "https://itunes.apple.com/search":
            return {
                "results": [
                    {
                        "collectionId": 123,
                        "collectionName": "Riot!",
                        "artistName": "Paramore",
                        "collectionType": "Album",
                        "releaseDate": "2007-06-12T07:00:00Z",
                        "primaryGenreName": "Alternative",
                        "trackCount": 11,
                        "collectionViewUrl": "https://example.com/album",
                        "artistViewUrl": "https://example.com/artist",
                        "artworkUrl100": "https://example.com/100x100bb.jpg",
                    }
                ]
            }
        if base_url == "https://itunes.apple.com/lookup":
            return {"results": []}
        raise AssertionError(f"unexpected URL: {base_url}")

    monkeypatch.setattr(lookup, "_get_json", fake_get_json)

    result = lookup.fetch_album_metadata("Riot!", "Paramore", include_lyrics=False)

    assert result is not None
    assert result["album"] == "Riot!"
    assert result["artist"] == "Paramore"
    assert result["source"] == "itunes_search_partial"
    assert result["track_count"] == 11
    assert result["tracks"] == []


def test_fetch_track_lyrics_prefers_synced_lrc_from_lrclib(monkeypatch):
    lookup = ITunesAlbumMetadataLookup(storefront="us", min_interval_seconds=0.0)

    def fake_get_json(base_url, params=None, timeout=None, return_empty_on_404=False):
        if base_url == "https://lrclib.net/api/search":
            return [
                {
                    "trackName": "Song One",
                    "artistName": "Test Artist",
                    "syncedLyrics": "[00:01.00]<00:01.10>Hello",
                    "plainLyrics": "Hello",
                }
            ]
        if base_url.startswith("https://api.lyrics.ovh/v1/"):
            return {"lyrics": "plain fallback"}
        raise AssertionError(f"unexpected URL: {base_url}")

    monkeypatch.setattr(lookup, "_get_json", fake_get_json)

    assert lookup.fetch_track_lyrics("Test Artist", "Song One") == "[00:01.00]<00:01.10>Hello"
    detail = lookup.fetch_track_lyrics_detail("Test Artist", "Song One")
    assert detail["timed"] is True
    assert detail["per_word"] is True
    assert detail["source"] == "lrclib"


def test_fetch_track_lyrics_detail_marks_line_timed_without_per_word(monkeypatch):
    lookup = ITunesAlbumMetadataLookup(storefront="us", min_interval_seconds=0.0)

    def fake_get_json(base_url, params=None, timeout=None, return_empty_on_404=False):
        if base_url == "https://lrclib.net/api/search":
            return [
                {
                    "trackName": "Song One",
                    "artistName": "Test Artist",
                    "syncedLyrics": "[00:01.00]Hello world",
                    "plainLyrics": "Hello world",
                }
            ]
        if base_url.startswith("https://api.lyrics.ovh/v1/"):
            return {}
        raise AssertionError(f"unexpected URL: {base_url}")

    monkeypatch.setattr(lookup, "_get_json", fake_get_json)

    detail = lookup.fetch_track_lyrics_detail("Test Artist", "Song One")

    assert detail["timed"] is True
    assert detail["per_word"] is False


def test_fetch_track_lyrics_falls_back_to_lyrics_ovh_when_lrclib_has_no_match(monkeypatch):
    lookup = ITunesAlbumMetadataLookup(storefront="us", min_interval_seconds=0.0)

    def fake_get_json(base_url, params=None, timeout=None, return_empty_on_404=False):
        if base_url == "https://lrclib.net/api/search":
            return []
        if base_url.startswith("https://api.lyrics.ovh/v1/"):
            return {"lyrics": "plain fallback"}
        raise AssertionError(f"unexpected URL: {base_url}")

    monkeypatch.setattr(lookup, "_get_json", fake_get_json)

    assert lookup.fetch_track_lyrics("Test Artist", "Song One") == "plain fallback"

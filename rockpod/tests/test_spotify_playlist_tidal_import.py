import base64
import json

import scripts.spotify_playlist_tidal_import as spotify_import
from scripts.spotify_playlist_tidal_import import fetch_spotify_playlist, parse_spotify_playlist_html


def _spotify_page(payload):
    encoded = base64.b64encode(json.dumps(payload).encode("utf-8")).decode("ascii")
    return f'<html><script id="initialState" type="text/plain">{encoded}</script></html>'


def test_parse_spotify_playlist_html_extracts_playlist_tracks():
    state = {
        "entities": {
            "items": {
                "spotify:playlist:abc123": {
                    "uri": "spotify:playlist:abc123",
                    "name": "Road Mix",
                    "content": {
                        "totalCount": 2,
                        "items": [
                            {
                                "itemV2": {
                                    "data": {
                                        "name": "First Song",
                                        "uri": "spotify:track:one",
                                        "artists": {
                                            "items": [
                                                {"profile": {"name": "First Artist"}},
                                            ],
                                        },
                                    },
                                },
                            },
                            {
                                "itemV2": {
                                    "data": {
                                        "name": "Second Song",
                                        "uri": "spotify:track:two",
                                        "artists": {
                                            "items": [
                                                {"profile": {"name": "Second Artist"}},
                                            ],
                                        },
                                    },
                                },
                            },
                        ],
                    },
                },
            },
        },
    }
    html = _spotify_page(state)

    playlist = parse_spotify_playlist_html(html, "abc123")

    assert playlist.name == "Road Mix"
    assert playlist.total_count == 2
    assert [track.query for track in playlist.tracks] == [
        "First Song First Artist",
        "Second Song Second Artist",
    ]


def test_fetch_spotify_playlist_falls_back_to_curl_fetcher(monkeypatch):
    state = {
        "entities": {
            "items": {
                "spotify:playlist:abc123": {
                    "uri": "spotify:playlist:abc123",
                    "name": "Fallback Mix",
                    "content": {
                        "totalCount": 1,
                        "items": [
                            {
                                "itemV2": {
                                    "data": {
                                        "name": "Fallback Song",
                                        "artists": {"items": [{"profile": {"name": "Fallback Artist"}}]},
                                    },
                                },
                            },
                        ],
                    },
                },
            },
        },
    }

    monkeypatch.setattr(spotify_import, "_fetch_with_urllib", lambda _url, _timeout: "<html><title>Spotify</title></html>")
    monkeypatch.setattr(spotify_import, "_fetch_with_curl", lambda _url, _timeout: _spotify_page(state))

    playlist = fetch_spotify_playlist("https://open.spotify.com/playlist/abc123?si=share")

    assert playlist.name == "Fallback Mix"
    assert playlist.tracks[0].query == "Fallback Song Fallback Artist"

"""Tests for the track matching engine."""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from services.track_matcher import TrackMatcher, MatchResult


def _make_local(tid, title, artist, album, duration=240.0, metadata_hash="",
                file_hash="", file_path="", **kw):
    d = {
        "id": tid, "title": title, "artist": artist, "album": album,
        "album_artist": kw.get("album_artist", ""),
        "duration": duration, "metadata_hash": metadata_hash,
        "file_hash": file_hash, "file_path": file_path or f"/music/{title}.mp3",
        "synced_to_device": kw.get("synced_to_device", 0),
        "last_synced_metadata_hash": kw.get("last_synced_metadata_hash", ""),
        "last_synced_file_hash": kw.get("last_synced_file_hash", ""),
    }
    d.update(kw)
    return d


def _make_device(did, title, artist, album, duration=240.0, metadata_hash="",
                 file_hash="", device_path="", local_track_id=None, **kw):
    return {
        "id": did, "title": title, "artist": artist, "album": album,
        "album_artist": kw.get("album_artist", ""),
        "duration": duration, "metadata_hash": metadata_hash,
        "file_hash": file_hash,
        "device_path": device_path or f"Music/{artist}/{album}/{title}.mp3",
        "local_track_id": local_track_id,
        "codec": kw.get("codec", ""),
    }


class TestTrackMatcher:
    def test_match_by_id(self):
        local = [_make_local(1, "Song", "Art", "Alb")]
        device = [_make_device(10, "Song", "Art", "Alb", local_track_id=1)]

        matcher = TrackMatcher()
        matched, unmatched, orphaned, resync = matcher.match_all(local, device)

        assert len(matched) == 1
        assert len(unmatched) == 0
        assert matched[0].match_type == MatchResult.MATCH_ID

    def test_match_by_metadata_hash(self):
        local = [_make_local(1, "Song", "Art", "Alb", metadata_hash="hash123")]
        device = [_make_device(10, "Song", "Art", "Alb", metadata_hash="hash123")]

        matcher = TrackMatcher()
        matched, unmatched, orphaned, resync = matcher.match_all(local, device)

        assert len(matched) == 1
        assert matched[0].match_type == MatchResult.MATCH_METADATA_HASH

    def test_stale_links_across_codecs_fall_back_to_correct_hash_matches(self):
        local = [
            _make_local(1, "Song", "Art", "Alb", metadata_hash="aac_hash", codec="AAC"),
            _make_local(2, "Song", "Art", "Alb", metadata_hash="flac_hash", codec="FLAC"),
        ]
        device = [
            _make_device(
                10,
                "Song",
                "Art",
                "Alb",
                metadata_hash="flac_hash",
                local_track_id=1,
                codec="FLAC",
                device_path="Music/Art/Alb/01 - Song.flac",
            ),
            _make_device(
                11,
                "Song",
                "Art",
                "Alb",
                metadata_hash="aac_hash",
                local_track_id=2,
                codec="M4A",
                device_path="Music/Art/Alb/01 - Song.m4a",
            ),
        ]

        matcher = TrackMatcher()
        matched, unmatched, orphaned, resync = matcher.match_all(local, device)

        assert not unmatched
        assert not orphaned
        assert not resync
        assert [result.device_track["codec"] for result in matched] == ["M4A", "FLAC"]

    def test_match_by_metadata_fields(self):
        local = [_make_local(1, "Song Title", "Artist Name", "Album Name", duration=180.0)]
        device = [_make_device(10, "Song Title", "Artist Name", "Album Name", duration=180.0)]

        matcher = TrackMatcher()
        matched, unmatched, orphaned, resync = matcher.match_all(local, device)

        assert len(matched) == 1
        assert matched[0].match_type == MatchResult.MATCH_METADATA

    def test_case_insensitive_metadata_match(self):
        local = [_make_local(1, "SONG", "ARTIST", "ALBUM", duration=200.0)]
        device = [_make_device(10, "song", "artist", "album", duration=200.0)]

        matcher = TrackMatcher()
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 1

    def test_unmatched_track(self):
        local = [_make_local(1, "Local Only", "Art", "Alb")]
        device = []

        matcher = TrackMatcher()
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 0
        assert len(unmatched) == 1

    def test_orphaned_device_track(self):
        local = []
        device = [_make_device(10, "Device Only", "Art", "Alb")]

        matcher = TrackMatcher()
        matched, unmatched, orphaned, _ = matcher.match_all(local, device)

        assert len(orphaned) == 1

    def test_match_by_file_hash(self):
        local = [_make_local(1, "Song A", "Art A", "Alb A", file_hash="deadbeef")]
        device = [_make_device(10, "Song B", "Art B", "Alb B", file_hash="deadbeef")]

        matcher = TrackMatcher("metadata_and_hash")
        matched, _, _, _ = matcher.match_all(local, device)

        assert len(matched) == 1
        assert matched[0].match_type == MatchResult.MATCH_FILE_HASH

    def test_needs_resync_detected(self):
        local = [_make_local(
            1, "Song", "Art", "Alb",
            metadata_hash="new_hash",
            synced_to_device=1,
            last_synced_metadata_hash="old_hash",
        )]
        device = [_make_device(10, "Song", "Art", "Alb", local_track_id=1)]

        matcher = TrackMatcher()
        matched, unmatched, orphaned, resync = matcher.match_all(local, device)

        assert len(matched) == 1
        assert len(resync) == 1

    def test_device_baseline_overrides_legacy_global_sync_state(self):
        local = [_make_local(
            1,
            "Song",
            "Art",
            "Alb",
            metadata_hash="current",
            last_synced_metadata_hash="other-device-old",
        )]
        device = [_make_device(
            10,
            "Song",
            "Art",
            "Alb",
            metadata_hash="current",
            local_track_id=1,
        )]
        device[0]["last_synced_metadata_hash"] = "current"
        device[0]["last_synced_file_hash"] = ""

        matched, _unmatched, _orphaned, resync = TrackMatcher().match_all(local, device)

        assert len(matched) == 1
        assert resync == []

    def test_filename_match_is_not_used(self):
        local = [_make_local(1, "X", "Y", "Z", file_path="/music/My Song.mp3")]
        device = [_make_device(10, "A", "B", "C", device_path="Music/Other/My Song.mp3")]

        matcher = TrackMatcher()
        matched, _, _, _ = matcher.match_all(local, device)

        assert len(matched) == 0

    def test_minor_metadata_difference_matches(self):
        local = [_make_local(1, "Song Title", "The Artist", "Album Name", duration=180.0)]
        device = [_make_device(10, "Song Title ", "the artist", "Album Name", duration=181.0)]

        matcher = TrackMatcher()
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 1
        assert len(unmatched) == 0

    def test_duration_tolerance_matches_real_device_rounding(self):
        local = [_make_local(1, "Song", "Artist", "Album", duration=200.4)]
        device = [_make_device(10, "Song", "Artist", "Album", duration=202.1)]

        matcher = TrackMatcher(duration_tolerance=2.0)
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 1
        assert len(unmatched) == 0

    def test_duration_outside_tolerance_does_not_match_identity(self):
        local = [_make_local(1, "Song", "Artist", "Album", duration=200.0)]
        device = [_make_device(10, "Song", "Artist", "Album", duration=208.0)]

        matcher = TrackMatcher(duration_tolerance=2.0)
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 0
        assert len(unmatched) == 1

    def test_punctuation_and_feat_normalization(self):
        local = [_make_local(1, "Don\u2019t Stop", "Artist feat. Guest", "Album", duration=180)]
        device = [_make_device(10, "Don't Stop", "Artist featuring Guest", "Album", duration=181)]

        matcher = TrackMatcher()
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 1
        assert len(unmatched) == 0

    def test_album_artist_falls_back_for_device_matching(self):
        local = [_make_local(1, "Song", "Guest", "Compilation", album_artist="Various Artists")]
        device = [
            _make_device(
                10,
                "Song",
                "",
                "Compilation",
                album_artist="Various Artists",
            )
        ]

        matcher = TrackMatcher()
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 1
        assert len(unmatched) == 0

    def test_incomplete_device_album_metadata_can_fallback_match(self):
        local = [_make_local(1, "Song", "Artist", "Album", duration=180)]
        device = [_make_device(10, "Song", "Artist", "", duration=181)]

        matcher = TrackMatcher()
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 1
        assert len(unmatched) == 0

    def test_previous_per_device_mapping_wins_over_metadata_difference(self):
        local = [_make_local(42, "Song", "Artist", "Album", duration=180)]
        device = [_make_device(10, "Different", "Other", "Else", local_track_id=42)]

        matcher = TrackMatcher()
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 1
        assert matched[0].match_type == MatchResult.MATCH_ID
        assert len(unmatched) == 0

    def test_large_device_inventory_matches_expected_tracks(self):
        local = [
            _make_local(i, f"Song {i}", "Artist", "Album", duration=180 + i)
            for i in range(1, 501)
        ]
        device = [
            _make_device(i + 1000, f"Song {i}", "Artist", "Album", duration=180 + i + 1)
            for i in range(1, 501)
        ]

        matcher = TrackMatcher()
        matched, unmatched, orphaned, _ = matcher.match_all(local, device)

        assert len(matched) == 500
        assert len(unmatched) == 0
        assert len(orphaned) == 0
        assert matcher.last_profile["local_count"] == 500
        assert matcher.last_profile["device_count"] == 500
        assert matcher.last_profile["matched_count"] == 500
        assert matcher.last_profile["unmatched_count"] == 0
        assert matcher.last_profile["orphaned_count"] == 0
        assert matcher.last_profile["identity_candidates_examined"] == 500
        assert matcher.last_profile["similarity_candidates_examined"] == 0
        assert matcher.last_profile["match_seconds"] >= 0.0

    def test_indexed_identity_match_preserves_device_order_with_multiple_artists(self):
        local = [_make_local(
            1,
            "Song",
            "Primary",
            "Album",
            album_artist="Compilation Artist",
        )]
        device = [
            _make_device(
                10,
                "Song",
                "Guest",
                "Album",
                album_artist="Compilation Artist",
            ),
            _make_device(11, "Song", "Primary", "Album"),
        ]

        matcher = TrackMatcher()
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert not unmatched
        assert matched[0].device_track["id"] == 10

    def test_bulk_unmatched_explanations_match_single_row_api(self):
        rows = [
            _make_local(1, "Known", "Wrong Artist", "Album"),
            _make_local(2, "Missing", "Artist", "Album"),
        ]
        device = [_make_device(10, "Known", "Artist", "Album")]
        matcher = TrackMatcher()

        bulk = matcher.explain_unmatched_many(rows, device)

        assert bulk == [matcher.explain_unmatched(row, device) for row in rows]
        assert bulk == ["artist mismatch", "title mismatch"]

    def test_similar_numbered_titles_do_not_false_match(self):
        local = [_make_local(1, "Song 1", "Artist", "Album", duration=200.0)]
        device = [_make_device(10, "Song 2", "Artist", "Album", duration=200.0)]

        matcher = TrackMatcher()
        matched, unmatched, _, _ = matcher.match_all(local, device)

        assert len(matched) == 0
        assert len(unmatched) == 1

    def test_device_metadata_hash_difference_needs_resync(self):
        local = [_make_local(1, "Song", "Art", "Alb", metadata_hash="new_hash")]
        device = [_make_device(10, "Song", "Art", "Alb", metadata_hash="old_hash")]

        matcher = TrackMatcher()
        matched, _, _, resync = matcher.match_all(local, device)

        assert len(matched) == 1
        assert len(resync) == 1

    def test_multiple_tracks_correct_matching(self):
        local = [
            _make_local(1, "Alpha", "Art", "Alb", metadata_hash="h1",
                        file_path="/music/Alpha.mp3"),
            _make_local(2, "Bravo", "Art", "Alb", metadata_hash="h2",
                        file_path="/music/Bravo.mp3"),
            _make_local(3, "Charlie", "Art", "Alb", metadata_hash="h3",
                        file_path="/music/Charlie.mp3"),
        ]
        device = [
            _make_device(10, "Alpha", "Art", "Alb", metadata_hash="h1",
                         device_path="Music/Art/Alb/Alpha.mp3"),
            _make_device(11, "Bravo", "Art", "Alb", metadata_hash="h2",
                         device_path="Music/Art/Alb/Bravo.mp3"),
        ]

        matcher = TrackMatcher()
        matched, unmatched, orphaned, _ = matcher.match_all(local, device)

        assert len(matched) == 2
        assert len(unmatched) == 1
        assert unmatched[0].local_track["title"] == "Charlie"

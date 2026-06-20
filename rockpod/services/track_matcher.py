"""Track matching engine — compares local library against device contents.

Matching priority:
  1. Persistent internal track ID (local_track_id foreign key)
  2. Metadata fingerprint
  3. Strong normalized identity (title + artist/album artist + album + duration tolerance)
  4. Fallback normalized identity (title + artist/album artist + duration tolerance)
  5. Metadata similarity
  6. File content hash in strict mode
"""

import logging
import re
import unicodedata
from difflib import SequenceMatcher

logger = logging.getLogger(__name__)


_PUNCT_TRANSLATION = str.maketrans(
    {
        "\u2018": "'",
        "\u2019": "'",
        "\u201a": "'",
        "\u201b": "'",
        "\u2032": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u201e": '"',
        "\u2033": '"',
        "\u2010": "-",
        "\u2011": "-",
        "\u2012": "-",
        "\u2013": "-",
        "\u2014": "-",
        "\u2212": "-",
        "\u00a0": " ",
    }
)


def _normalize(s):
    """Normalize user/device metadata for real-world iPod matching."""
    if not s:
        return ""
    s = unicodedata.normalize("NFKC", str(s)).translate(_PUNCT_TRANSLATION)
    s = s.strip().casefold()
    s = re.sub(r"\b(feat|ft)\.?\b", "featuring", s)
    s = re.sub(r"&", " and ", s)
    s = re.sub(r"[`´]", "'", s)
    s = re.sub(r"\s*[-_/]+\s*", " ", s)
    s = re.sub(r"[^\w\s'\"().,+]", " ", s)
    s = re.sub(r"\s+", " ", s)
    return s.strip()


def _duration_seconds(dur):
    try:
        return round(float(dur or 0))
    except (TypeError, ValueError):
        return 0


def _duration_ok(local_duration, device_duration, tolerance):
    local = _duration_seconds(local_duration)
    device = _duration_seconds(device_duration)
    return local == 0 or device == 0 or abs(local - device) <= tolerance


def _artist_keys(row):
    artist = _normalize(row.get("artist", ""))
    album_artist = _normalize(row.get("album_artist", ""))
    keys = {a for a in (artist, album_artist) if a}
    return keys


def _artist_matches(local, device):
    local_keys = _artist_keys(local)
    device_keys = _artist_keys(device)
    return bool(local_keys and device_keys and local_keys.intersection(device_keys))


class MatchResult:
    """Result of matching a local track against device tracks."""

    MATCH_NONE = "none"
    MATCH_ID = "id"
    MATCH_METADATA_HASH = "metadata_hash"
    MATCH_METADATA = "metadata"
    MATCH_METADATA_SIMILARITY = "metadata_similarity"
    MATCH_FILE_HASH = "file_hash"

    def __init__(self, local_track, device_track=None, match_type=MATCH_NONE,
                 confidence=0.0):
        self.local_track = local_track
        self.device_track = device_track
        self.match_type = match_type
        self.confidence = confidence

    @property
    def is_matched(self):
        return self.match_type != self.MATCH_NONE

    @property
    def needs_resync(self):
        """True if matched but metadata or content has diverged."""
        if not self.is_matched:
            return False
        lt = self.local_track
        dt = self.device_track
        local = dict(lt) if hasattr(lt, "keys") else lt
        device = dict(dt) if hasattr(dt, "keys") else dt

        if local and device:
            local_mh = local.get("metadata_hash", "")
            device_mh = device.get("metadata_hash", "")
            if local_mh and device_mh and local_mh != device_mh:
                return True

            local_fh = local.get("file_hash", "")
            device_fh = device.get("file_hash", "")
            if local_fh and device_fh and local_fh != device_fh:
                return True

        if hasattr(lt, "needs_resync") and lt.needs_resync:
            return True
        if hasattr(lt, "__getitem__"):
            mh = lt.get("metadata_hash", "")
            lsmh = lt.get("last_synced_metadata_hash", "")
            if mh and lsmh and mh != lsmh:
                return True
            fh = lt.get("file_hash", "")
            lsfh = lt.get("last_synced_file_hash", "")
            if fh and lsfh and fh != lsfh:
                return True
        return False

    def __repr__(self):
        title = ""
        if hasattr(self.local_track, "title"):
            title = self.local_track.title
        elif hasattr(self.local_track, "__getitem__"):
            title = self.local_track.get("title", "")
        return f"<MatchResult {self.match_type} conf={self.confidence:.2f} '{title}'>"


class TrackMatcher:
    """Compares a local library against device tracks to find matches and gaps."""

    def __init__(self, strictness="metadata_and_hash", duration_tolerance=2.0):
        self.strictness = strictness
        self.duration_tolerance = float(duration_tolerance or 2.0)

    def match_all(self, local_tracks, device_tracks):
        """Match all local tracks against device tracks.

        Returns:
            matched: list of MatchResult with is_matched=True
            unmatched: list of MatchResult with is_matched=False (not on device)
            orphaned: list of device tracks with no local match
            resync: list of MatchResult that are matched but need re-copy
        """
        # Build indices on device tracks. Values are lists so duplicate-looking
        # metadata does not overwrite earlier candidates.
        dev_by_id = {}
        dev_by_mhash = {}
        dev_by_fhash = {}
        device_rows = []
        dev_matched = set()

        for dt in device_tracks:
            d = dict(dt) if hasattr(dt, "keys") else dt
            device_rows.append(d)
            did = d.get("id")
            lid = d.get("local_track_id")

            if lid:
                dev_by_id[lid] = d

            mh = d.get("metadata_hash", "")
            if mh:
                dev_by_mhash.setdefault(mh, []).append(d)

            fh = d.get("file_hash", "")
            if fh:
                dev_by_fhash.setdefault(fh, []).append(d)

        matched = []
        unmatched = []
        resync = []

        for lt in local_tracks:
            row = dict(lt) if hasattr(lt, "keys") else lt
            result = self._match_one(row, dev_by_id, dev_by_mhash,
                                     dev_by_fhash, device_rows, dev_matched)
            if result.is_matched:
                matched.append(result)
                if result.needs_resync:
                    resync.append(result)
            else:
                unmatched.append(result)

        # Orphaned device tracks = those not matched to any local track
        orphaned = []
        for dt in device_tracks:
            d = dict(dt) if hasattr(dt, "keys") else dt
            if _match_id(d) not in dev_matched:
                orphaned.append(d)

        logger.info(
            "Match results: %d matched, %d unmatched (not on device), "
            "%d orphaned (on device only), %d need resync",
            len(matched), len(unmatched), len(orphaned), len(resync),
        )
        return matched, unmatched, orphaned, resync

    def _match_one(self, row, dev_by_id, dev_by_mhash,
                   dev_by_fhash, device_rows, dev_matched):
        """Try to match a single local track against device indices."""
        tid = row.get("id")

        # Priority 1: persistent ID link
        if tid and tid in dev_by_id:
            dt = dev_by_id[tid]
            if not _is_matched(dt, dev_matched):
                _mark_matched(dt, dev_matched)
                return MatchResult(row, dt, MatchResult.MATCH_ID, 1.0)

        # Priority 2: metadata fingerprint hash.
        mh = row.get("metadata_hash", "")
        if mh and mh in dev_by_mhash:
            dt = _first_available(dev_by_mhash[mh], dev_matched)
            if dt is not None:
                _mark_matched(dt, dev_matched)
                return MatchResult(row, dt, MatchResult.MATCH_METADATA_HASH, 0.98)

        # Priority 3: strong normalized identity.
        dt = self._first_identity_match(row, device_rows, dev_matched, require_album=True)
        if dt is not None:
            _mark_matched(dt, dev_matched)
            return MatchResult(row, dt, MatchResult.MATCH_METADATA, 0.95)

        # Priority 4: fallback identity for incomplete device metadata.
        dt = self._first_identity_match(row, device_rows, dev_matched, require_album=False)
        if dt is not None:
            _mark_matched(dt, dev_matched)
            return MatchResult(row, dt, MatchResult.MATCH_METADATA, 0.88)

        # Priority 5: metadata similarity. This deliberately requires a strong
        # title match and compatible artist/album/duration to avoid matching
        # distinct tracks like "Song 1", "Song 2", and "Song 3".
        dt, confidence = self._best_metadata_similarity(row, device_rows, dev_matched)
        if dt is not None:
            _mark_matched(dt, dev_matched)
            return MatchResult(row, dt, MatchResult.MATCH_METADATA_SIMILARITY, confidence)

        # Priority 6: file content hash (strict mode).
        if self.strictness in ("metadata_and_hash", "file_hash_only"):
            fh = row.get("file_hash", "")
            if fh and fh in dev_by_fhash:
                dt = _first_available(dev_by_fhash[fh], dev_matched)
                if dt is not None:
                    _mark_matched(dt, dev_matched)
                    return MatchResult(row, dt, MatchResult.MATCH_FILE_HASH, 0.85)

        return MatchResult(row)

    def _first_identity_match(self, row, device_rows, dev_matched, require_album=True):
        local_title = _normalize(row.get("title", ""))
        local_album = _normalize(row.get("album", ""))
        if not local_title or not _artist_keys(row):
            return None
        if require_album and not local_album:
            return None

        for dt in device_rows:
            if _is_matched(dt, dev_matched):
                continue
            device_title = _normalize(dt.get("title", ""))
            if not device_title or local_title != device_title:
                continue
            if not _artist_matches(row, dt):
                continue
            if require_album:
                device_album = _normalize(dt.get("album", ""))
                if not device_album or device_album != local_album:
                    continue
            if not _duration_ok(
                row.get("duration", 0),
                dt.get("duration", 0),
                self.duration_tolerance,
            ):
                continue
            return dt
        return None

    def _best_metadata_similarity(self, row, device_rows, dev_matched):
        local_title = _normalize(row.get("title", ""))
        local_artists = _artist_keys(row)
        local_album = _normalize(row.get("album", ""))

        if not local_title or not local_artists:
            return None, 0.0

        best_dt = None
        best_score = 0.0

        for dt in device_rows:
            if _is_matched(dt, dev_matched):
                continue

            device_title = _normalize(dt.get("title", ""))
            device_artists = _artist_keys(dt)
            device_album = _normalize(dt.get("album", ""))

            if not device_title or not device_artists:
                continue

            title_score = SequenceMatcher(None, local_title, device_title).ratio()
            artist_score = max(
                SequenceMatcher(None, la, da).ratio()
                for la in local_artists
                for da in device_artists
            )
            album_score = SequenceMatcher(None, local_album, device_album).ratio()
            duration_ok = _duration_ok(
                row.get("duration", 0),
                dt.get("duration", 0),
                self.duration_tolerance,
            )

            if title_score < 0.92 or artist_score < 0.90 or not duration_ok:
                continue
            if local_album and device_album and album_score < 0.86:
                continue

            score = (
                title_score * 0.45
                + artist_score * 0.25
                + (album_score if local_album and device_album else 1.0) * 0.20
                + (1.0 if duration_ok else 0.0) * 0.10
            )
            if score > best_score:
                best_score = score
                best_dt = dt

        if best_score >= 0.90:
            return best_dt, best_score
        return None, 0.0

    def explain_unmatched(self, row, device_rows):
        """Return a concise reason why a local row did not match."""
        row = dict(row) if hasattr(row, "keys") else dict(row or {})
        device_rows = [
            dict(dt) if hasattr(dt, "keys") else dict(dt or {})
            for dt in (device_rows or [])
        ]
        title = _normalize(row.get("title", ""))
        if not title:
            return "missing title"
        if not _artist_keys(row):
            return "metadata incomplete: missing artist"
        if not device_rows:
            return "device inventory empty"

        same_title = [dt for dt in device_rows if _normalize(dt.get("title", "")) == title]
        if not same_title:
            missing_title_count = sum(1 for dt in device_rows if not _normalize(dt.get("title", "")))
            if missing_title_count:
                return "metadata incomplete: device titles missing"
            return "title mismatch"

        same_artist = [dt for dt in same_title if _artist_matches(row, dt)]
        if not same_artist:
            return "artist mismatch"

        album = _normalize(row.get("album", ""))
        same_album = [
            dt for dt in same_artist
            if not album or not _normalize(dt.get("album", "")) or _normalize(dt.get("album", "")) == album
        ]
        if not same_album:
            return "album mismatch"

        if not any(
            _duration_ok(row.get("duration", 0), dt.get("duration", 0), self.duration_tolerance)
            for dt in same_album
        ):
            return "duration mismatch"
        return "no available unmatched candidate"


def _metadata_key(row):
    return (
        sorted(_artist_keys(row))[0] if _artist_keys(row) else "",
        _normalize(row.get("album", "")),
        _normalize(row.get("title", "")),
        _duration_seconds(row.get("duration", 0)),
    )


def _has_identity(key):
    artist, album, title, _duration = key
    return bool(artist and title and album)


def _match_id(row):
    return row.get("id") or row.get("device_path") or id(row)


def _is_matched(row, matched_ids):
    return _match_id(row) in matched_ids


def _mark_matched(row, matched_ids):
    matched_ids.add(_match_id(row))


def _first_available(rows, matched_ids):
    for row in rows:
        if not _is_matched(row, matched_ids):
            return row
    return None

"""Track data model."""

import hashlib
from dataclasses import dataclass, field, asdict
from typing import Optional


def compute_metadata_hash(title, artist, album, album_artist, track_number,
                          disc_number, genre, year, composer, duration, bitrate,
                          codec):
    """Compute a deterministic hash of the metadata fields that matter for sync.

    This intentionally excludes file path, file size, play count, rating, and
    other mutable-but-non-content fields so that the hash only changes when
    the actual musical identity or encoding of a track changes.
    """
    parts = [
        str(title or "").strip().lower(),
        str(artist or "").strip().lower(),
        str(album or "").strip().lower(),
        str(album_artist or "").strip().lower(),
        str(track_number or ""),
        str(disc_number or "1"),
        str(genre or "").strip().lower(),
        str(year or ""),
        str(composer or "").strip().lower(),
        f"{float(duration or 0):.1f}",
        str(bitrate or ""),
        str(codec or "").strip().lower(),
    ]
    raw = "|".join(parts)
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()[:32]


def compute_artwork_hash(artwork_bytes):
    """Hash raw artwork data so we can detect artwork changes."""
    if not artwork_bytes:
        return ""
    return hashlib.sha256(artwork_bytes).hexdigest()[:32]


@dataclass
class Track:
    """Represents a media item in the library."""

    id: Optional[int] = None
    media_type: str = "audio"
    video_kind: str = ""
    file_path: str = ""
    file_hash: str = ""
    file_size: int = 0
    last_modified: float = 0.0
    title: str = ""
    artist: str = ""
    album: str = ""
    album_artist: str = ""
    show_title: str = ""
    genre: str = ""
    year: Optional[int] = None
    season_number: Optional[int] = None
    episode_number: Optional[int] = None
    track_number: Optional[int] = None
    track_total: Optional[int] = None
    disc_number: int = 1
    disc_total: Optional[int] = None
    duration: float = 0.0
    bitrate: int = 0
    sample_rate: int = 0
    channels: int = 2
    codec: str = ""
    composer: str = ""
    comment: str = ""
    compilation: int = 0
    rating: int = 0
    play_count: int = 0
    last_played: Optional[str] = None
    artwork_path: Optional[str] = None
    has_embedded_artwork: int = 0
    date_added: Optional[str] = None
    synced_to_device: int = 0
    device_path: Optional[str] = None
    metadata_hash: str = ""
    artwork_hash: str = ""
    last_synced_metadata_hash: str = ""
    last_synced_file_hash: str = ""

    def recompute_metadata_hash(self):
        """Recompute and store the metadata fingerprint."""
        self.metadata_hash = compute_metadata_hash(
            self.title, self.artist, self.album, self.album_artist,
            self.track_number, self.disc_number, self.genre, self.year,
            self.composer, self.duration, self.bitrate, self.codec,
        )
        return self.metadata_hash

    @property
    def needs_resync(self):
        """True when the track is marked synced but its content has changed."""
        if not self.synced_to_device:
            return False
        if self.metadata_hash and self.last_synced_metadata_hash:
            if self.metadata_hash != self.last_synced_metadata_hash:
                return True
        if self.file_hash and self.last_synced_file_hash:
            if self.file_hash != self.last_synced_file_hash:
                return True
        return False

    @property
    def display_artist(self):
        return self.album_artist if self.album_artist else self.artist

    @property
    def duration_str(self):
        if self.duration <= 0:
            return "0:00"
        mins = int(self.duration) // 60
        secs = int(self.duration) % 60
        return f"{mins}:{secs:02d}"

    @property
    def bitrate_str(self):
        if self.bitrate <= 0:
            return ""
        return f"{self.bitrate} kbps"

    @property
    def size_str(self):
        if self.file_size <= 0:
            return ""
        mb = self.file_size / (1024 * 1024)
        if mb >= 1.0:
            return f"{mb:.1f} MB"
        kb = self.file_size / 1024
        return f"{kb:.0f} KB"

    def to_dict(self):
        d = asdict(self)
        if d.get("id") is None:
            del d["id"]
        return d

    @classmethod
    def from_row(cls, row):
        """Create Track from a sqlite3.Row."""
        if row is None:
            return None
        data = dict(row)
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})


@dataclass
class DeviceTrack:
    """Represents a track found on a Rockbox device."""

    id: Optional[int] = None
    device_id: str = ""
    device_path: str = ""
    file_hash: str = ""
    file_size: int = 0
    title: str = ""
    artist: str = ""
    album: str = ""
    album_artist: str = ""
    genre: str = ""
    year: Optional[int] = None
    track_number: Optional[int] = None
    disc_number: int = 1
    duration: float = 0.0
    bitrate: int = 0
    codec: str = ""
    local_track_id: Optional[int] = None
    metadata_hash: str = ""
    last_scan: Optional[str] = None

    def to_dict(self):
        d = asdict(self)
        if d.get("id") is None:
            del d["id"]
        return d

    @classmethod
    def from_row(cls, row):
        if row is None:
            return None
        data = dict(row)
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})

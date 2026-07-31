"""Live TV: channel line-up, schedule generation and iPod sync.

Live TV is deliberately separate from the ordinary video library and from the
Netflix browser. Shows come from ``~/Videos/Live`` and commercials from
``~/Videos/Live/ADS``; both are excluded from the normal library scan.

The schedule is a seven day rotation keyed on the weekday. Every slot carries
an absolute start offset from local midnight and a real duration measured with
ffprobe, so the PC guide, the iPod guide and the iPod player all resolve the
same instant to the same programme. Nothing is decided at playback time.

See docs/livetv-directv-guide-spec.md.
"""

from __future__ import annotations

import filecmp
import hashlib
import json
import logging
import math
import os
import random
import re
import shutil
import subprocess
import time
from dataclasses import dataclass, field, asdict
from datetime import datetime
from pathlib import Path

logger = logging.getLogger(__name__)

LIVETV_SOURCE_DIR_NAME = "Live"
LIVETV_ADS_DIR_NAME = "ADS"
LIVETV_DEVICE_DIR = "Videos/LiveTV"

LIVETV_MAX_CHANNELS = 24
LIVETV_FIRST_CHANNEL = 100
# Must match LIVETV_MAX_SLOTS in apps/plugins/mpegplayer/livetv.h. The iPod
# holds today and tomorrow, so that pair is what has to fit.
LIVETV_DEVICE_MAX_SLOTS = 2048
LIVETV_DAY_SECONDS = 86400
LIVETV_MIN_TAIL_SECONDS = 60

MEDIA_EXTENSIONS = {
    ".mp4", ".m4v", ".mkv", ".avi", ".webm", ".mov",
    ".mpg", ".mpeg", ".mpe", ".ts", ".m2ts",
}
SIDECAR_EXTENSIONS = {
    ".vtt", ".txt", ".srt", ".sub", ".json", ".nfo", ".xml",
    ".jpg", ".jpeg", ".png", ".webp", ".part", ".ytdl", ".info",
}

DEFAULT_AD_BREAK_MIN = 2
DEFAULT_AD_BREAK_MAX = 3

# Sampled from the DIRECTV receiver artwork; these must match the
# LIVETV_BANNER_TOP and LIVETV_CHAN_BG constants in livetv_guide.c so that
# artwork padding disappears into the guide behind it.
GUIDE_BANNER_BG = "0xDCEEF9"
GUIDE_CHANNEL_BG = "0x122549"

# Must match LIVETV_LOGO_W / LIVETV_LOGO_H and LIVETV_BRAND_W /
# LIVETV_BRAND_H in apps/plugins/mpegplayer/livetv_guide.c. Those size
# the fixed cache buffers the player hands to read_bmp_file(), so a
# bitmap larger than this simply fails to load and the guide silently
# falls back to the text call sign.
LIVETV_LOGO_W = 40
LIVETV_LOGO_H = 18
LIVETV_BRAND_W = 64
LIVETV_BRAND_H = 22

# Bump this whenever the encode changes so cached clips are rebuilt rather
# than silently reused in the old framing.
LIVETV_MPEG_PROFILE = "mpeg2-320x240-fill-v2"

# The Weather channel is recognised by category, not by name, so a user
# rename survives a rebuild exactly like any other channel (see
# LiveTvLineup.autobuild()'s docstring on the same rule).
LIVETV_WEATHER_CATEGORY = "Weather"
LIVETV_WEATHER_CALLSIGN = "WX"
# One block per song would blow LIVETV_DEVICE_MAX_SLOTS (see
# docs/livetv-weather-channel-spec.md section 3), so the channel instead
# rotates a handful of hour-long bumpers built from the user's own local
# music, looped/concatenated to fill the hour.
LIVETV_WEATHER_BLOCK_SECONDS = 3600
LIVETV_WEATHER_VARIANTS = 3
LIVETV_WEATHER_MUSIC_EXTENSIONS = {".mp3", ".m4a", ".flac", ".wav", ".ogg"}
LIVETV_WEATHER_IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".webp"}
LIVETV_WEATHER_VIDEO_EXTENSIONS = MEDIA_EXTENSIONS
LIVETV_WEATHER_FLOW_PROFILE = "broadcast-flow-v9"
LIVETV_WEATHER_CLOCK_SECONDS = 120
LIVETV_WEATHER_REPORT_START = 48
LIVETV_WEATHER_REPORT_SECONDS = 72
LIVETV_WEATHER_MIN_NATURAL_REPORT_SECONDS = 45
LIVETV_WEATHER_MIN_NEWS_BREAK_SECONDS = 24
LIVETV_WEATHER_MAX_REPORT_SECONDS = 112
LIVETV_WEATHER_MAX_NEWS_BREAK_SECONDS = 300
LIVETV_TV_INFORMATION_CATEGORY_PREFIX = "TVInfo:"

# These live channels deliberately use the publishers' own YouTube channel
# pages, rather than a search result that can be reordered by YouTube.  The
# duration floor keeps highlights, trailers and Shorts out of the broadcast
# rotation; the requested number of newest full programmes is then downloaded
# into the ordinary Live TV staging area and follows the normal MPEG/guide
# sync path.
LIVETV_YOUTUBE_CHANNELS = {
    "majority-report": {
        "number": 99,
        "callsign": "MR",
        "name": "The Majority Report",
        "series": "The Majority Report",
        "url": "https://www.youtube.com/@TheMajorityReport/streams",
        "minimum_duration": 40 * 60,
        "logo_text": "MR",
        "logo_fill": "#bc1f2d",
    },
    "channel-5": {
        "number": 98,
        "callsign": "C5",
        "name": "Channel 5",
        "series": "Channel 5 with Andrew Callaghan",
        "url": "https://www.youtube.com/@Channel5YouTube/videos",
        "minimum_duration": 8 * 60,
        "logo_text": "CHANNEL 5",
        "logo_fill": "#f4d000",
    },
}
LIVETV_YOUTUBE_VIDEO_LIMIT = 3
LIVETV_YOUTUBE_CANDIDATE_LIMIT = 24

# The wordmark DIRECTV used in the 2004-2011 receivers, which is the era
# this guide reproduces. Public domain (simple geometry, © The DirecTV
# Group). Wikimedia rejects arbitrary thumbnail widths, so ask for one of
# the standard sizes. Failure is not fatal: the guide falls back to drawing
# the brand text itself.
BRAND_LOGO_SOURCES = (
    "https://upload.wikimedia.org/wikipedia/commons/thumb/b/b6/"
    "DirecTV_logo_%282004-2011%29.svg/330px-DirecTV_logo_%282004-2011%29"
    ".svg.png",
    "https://upload.wikimedia.org/wikipedia/commons/b/b6/"
    "DirecTV_logo_%282004-2011%29.svg",
)

_YEAR_RE = re.compile(r"\b(19|20)\d{2}\b")
_DATE_RE = re.compile(r"\d{1,2}[_\-./]\d{1,2}[_\-./]\d{2,4}")
_EPISODE_RE = re.compile(r"\bS\d{1,2}\s*E\d{1,2}\b", re.IGNORECASE)
_NOISE_RE = re.compile(
    r"\b(1080p|720p|480p|xvid|x264|x265|h264|hevc|hdtv|dvdrip|webrip|"
    r"web-dl|bluray|lq|hq|shd|sdtv|proper|complete|season|full|commercial|"
    r"remastered)\b",
    re.IGNORECASE,
)
# Trailing broadcast dates such as "... 2006 01 09" once the year is gone.
_TRAILING_NUMBERS_RE = re.compile(r"(?:\s+#?\d{1,4})+\s*$")
_NON_PRINTABLE_TEXT = re.compile(r"[\x00-\x1f\x7f]+|\u200b")


def _clean_text(value) -> str:
    text = str(value or "")
    text = _NON_PRINTABLE_TEXT.sub(" ", text)
    text = text.replace("\ufeff", "").strip()
    return " ".join(text.split())


def _first_non_empty_text(*values):
    for value in values:
        text = _clean_text(value)
        if text:
            return text
    return ""


def livetv_source_dir(video_dir: str) -> str:
    """The Live TV source folder inside a configured video directory."""
    return os.path.join(os.path.abspath(os.path.expanduser(video_dir or "")),
                        LIVETV_SOURCE_DIR_NAME)


def livetv_ads_dir(video_dir: str) -> str:
    return os.path.join(livetv_source_dir(video_dir), LIVETV_ADS_DIR_NAME)


def is_livetv_path(path: str, video_dirs) -> bool:
    """True when a path lives under any configured Live TV source folder."""
    try:
        target = os.path.abspath(os.path.expanduser(str(path or "")))
    except (TypeError, ValueError):
        return False
    if not target:
        return False
    for video_dir in video_dirs or []:
        root = livetv_source_dir(video_dir)
        if target == root or target.startswith(root + os.sep):
            return True
    return False


def _safe_name(value: str, fallback: str = "item") -> str:
    text = re.sub(r"[^A-Za-z0-9._-]+", "_", str(value or "").strip())
    text = text.strip("._-")
    return text[:60] or fallback


def clean_title(stem: str) -> str:
    """Turn a download filename into something a guide can print."""
    text = str(stem or "").replace("_", " ")
    text = re.sub(r"\[[^\]]*\]", " ", text)
    text = re.sub(r"\.(ia)$", "", text, flags=re.IGNORECASE)
    text = _NOISE_RE.sub(" ", text)
    text = re.sub(r"\s+", " ", text).strip(" -.")
    return text or str(stem or "").strip() or "Program"


def program_description(path: str, series: str, category: str = "Series") -> str:
    """A DIRECTV style synopsis line: "Series, Variety (1983). ..."."""
    stem = str(Path(path).stem or "")
    year_match = _YEAR_RE.search(stem)
    lead = f"{category}, {series}"
    if year_match:
        lead += f" ({year_match.group(0)})"
    detail = clean_title(stem)
    if detail.lower().startswith(series.lower()):
        detail = detail[len(series):].strip(" -–(),.#")
    if detail:
        return f"{lead}. {detail}."
    return f"{lead}."


def program_title(path: str, series: str) -> str:
    """The guide label for one episode.

    Broadcast rips are usually named after their air date. A guide prints
    the programme name, so keep the series and add the date only when it is
    what distinguishes one recording from another.
    """
    name = clean_title(Path(path).stem)
    series = str(series or "").strip()
    if not series:
        return name

    remainder = name
    if remainder.lower().startswith(series.lower()):
        remainder = remainder[len(series):]
    remainder = remainder.strip(" -–(),.#")
    remainder = re.sub(r"\s+", " ", remainder)

    if not remainder or remainder.lower() == series.lower():
        return series
    # A file sitting in a folder of nearly the same name would otherwise be
    # printed twice over, e.g. "SpongeBob VHS: SpongeBob VHS Goes...".
    if series.lower() in remainder.lower():
        return remainder if len(remainder) <= 60 \
            else remainder[:57].rstrip() + "..."
    if len(remainder) > 40:
        remainder = remainder[:37].rstrip() + "..."
    return f"{series}: {remainder}"


def series_name(path: str, root: str) -> str:
    """Group key for a show: its folder under Live, else a cleaned title."""
    try:
        relative = os.path.relpath(path, root)
    except ValueError:
        relative = os.path.basename(path)
    parts = Path(relative).parts
    if len(parts) > 1:
        return clean_title(parts[0])

    text = clean_title(Path(path).stem)
    text = _EPISODE_RE.sub(" ", text)
    text = _DATE_RE.sub(" ", text)
    text = _YEAR_RE.sub(" ", text)
    text = re.sub(r"\([^)]*\)", " ", text)
    text = re.split(r"\s+[-–]\s+", text)[0]
    text = re.sub(r"\s+", " ", text).strip(" -.#")
    text = _TRAILING_NUMBERS_RE.sub("", text).strip(" -.#")
    return text or clean_title(Path(path).stem)


def _is_media_file(path: str) -> bool:
    suffix = Path(path).suffix.lower()
    if suffix in SIDECAR_EXTENSIONS:
        return False
    return suffix in MEDIA_EXTENSIONS


def _dedupe_internet_archive(paths):
    """``X.mp4`` and ``X.ia.mp4`` are the same Internet Archive item.

    Keeping both would play the same commercial twice in one break.
    """
    by_key = {}
    for path in paths:
        directory = os.path.dirname(path)
        stem = Path(path).stem
        base = stem[:-3] if stem.lower().endswith(".ia") else stem
        key = (directory, base.lower())
        current = by_key.get(key)
        if current is None:
            by_key[key] = path
            continue
        # Prefer the plain name over the .ia duplicate, then the larger file.
        current_is_ia = Path(current).stem.lower().endswith(".ia")
        new_is_ia = stem.lower().endswith(".ia")
        if current_is_ia and not new_is_ia:
            by_key[key] = path
        elif current_is_ia == new_is_ia:
            try:
                if os.path.getsize(path) > os.path.getsize(current):
                    by_key[key] = path
            except OSError:
                pass
    return sorted(by_key.values())


@dataclass
class LiveTvMedia:
    """One show or commercial available to the line-up.

    A single recording can hold several programmes — an evening's tape, or a
    show with the adverts still in it. ``segment`` names a piece of the file
    to use: ``start``/``end`` bound it and ``cuts`` are ranges dropped from
    inside it. When ``segment`` is empty the whole file is used.
    """

    path: str
    kind: str            # "show" or "ad"
    title: str
    series: str
    duration: int = 0
    rating: str = ""
    description: str = ""
    renamed: bool = False
    segment: str = ""
    start: int = 0
    end: int = 0         # 0 means "to the end of the file"
    cuts: list = field(default_factory=list)   # [[start, end], ...]

    @property
    def key(self) -> str:
        """Identity of this programme, unique per segment of a file."""
        return f"{self.path}#{self.segment}" if self.segment else self.path

    @property
    def is_segment(self) -> bool:
        return bool(self.segment)

    def keep_ranges(self):
        """The portions of the source that survive editing, in order."""
        start = max(0, int(self.start or 0))
        end = int(self.end or 0)
        if end <= start:
            end = start + max(0, int(self.duration or 0)) + sum(
                max(0, int(b) - int(a)) for a, b in self.cuts or [])
        if end <= start:
            return [(start, 0)]  # unbounded: to the end of the file

        ranges = []
        cursor = start
        for cut_start, cut_end in sorted(
                ([max(start, int(a)), min(end, int(b))]
                 for a, b in (self.cuts or [])),
                key=lambda pair: pair[0]):
            if cut_end <= cursor:
                continue
            if cut_start > cursor:
                ranges.append((cursor, min(cut_start, end)))
            cursor = max(cursor, cut_end)
        if cursor < end:
            ranges.append((cursor, end))
        return ranges or [(start, end)]

    def edited_duration(self) -> int:
        """Seconds of programme once the cuts are taken out."""
        total = 0
        for begin, finish in self.keep_ranges():
            if finish <= begin:
                return int(self.duration or 0)
            total += finish - begin
        return total

    def device_relative(self) -> str:
        stem = _safe_name(Path(self.path).stem, "clip")
        if self.segment:
            stem = f"{stem}-{_safe_name(self.segment, 'part')}"
        if self.kind == "ad":
            return f"ads/{stem}.mpg"
        return f"shows/{_safe_name(self.series, 'series')}/{stem}.mpg"

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class LiveTvChannel:
    number: int
    callsign: str
    name: str
    category: str = "Series"
    logo: str = ""
    favourite: bool = True
    parental_locked: bool = False
    shows: list = field(default_factory=list)
    ads: list = field(default_factory=list)

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class LiveTvSlot:
    """One file the player will open.

    A guide lists *programmes*, not the commercials inside them: at 8:15 the
    listing still says the eight o'clock show. Every slot therefore also
    carries the enclosing programme block, and commercials inherit the block
    of the show they interrupt.
    """

    channel: int
    day: int
    start: int
    duration: int
    kind: str
    title: str           # the programme title, for both shows and ads
    rating: str
    description: str
    path: str            # device relative
    source: str = ""     # PC path, for the rockpod guide
    block_start: int = 0
    block_duration: int = 0
    ad_title: str = ""   # a commercial's own name, for the rockpod list

    @property
    def end(self) -> int:
        return self.start + self.duration

    @property
    def block_end(self) -> int:
        return self.block_start + self.block_duration


class LiveTvLibrary:
    """Scans the Live TV folders and caches probed durations."""

    def __init__(self, config, command_runner=None):
        self._config = config
        self._runner = command_runner
        self._duration_cache = {}
        self._cache_path = os.path.join(self.state_dir(), "durations.json")
        self._load_duration_cache()
        self._overrides_path = os.path.join(self.state_dir(), "titles.json")
        self._overrides = {}
        self._load_overrides()
        self._edits_path = os.path.join(self.state_dir(), "edits.json")
        self._edits = {}
        self._load_edits()

    # -- paths ---------------------------------------------------------

    def state_dir(self) -> str:
        base = os.path.join(os.path.expanduser("~"), ".rockpod", "livetv")
        os.makedirs(base, exist_ok=True)
        return base

    def cache_dir(self) -> str:
        path = os.path.join(self.state_dir(), "cache")
        os.makedirs(path, exist_ok=True)
        return path

    def staging_dir(self) -> str:
        path = os.path.join(self.state_dir(), "staging")
        os.makedirs(path, exist_ok=True)
        return path

    def video_dirs(self):
        dirs = list(getattr(self._config, "video_dirs", None) or [])
        if not dirs:
            single = getattr(self._config, "video_dir", "")
            if single:
                dirs = [single]
        return dirs

    def source_roots(self):
        roots = []
        for video_dir in self.video_dirs():
            root = livetv_source_dir(video_dir)
            if os.path.isdir(root) and root not in roots:
                roots.append(root)
        return roots

    def ffmpeg_bin(self) -> str:
        configured = str(getattr(self._config, "ffmpeg_binary", "") or "").strip()
        return configured or shutil.which("ffmpeg") or "ffmpeg"

    def ffprobe_bin(self) -> str:
        ffmpeg = self.ffmpeg_bin()
        candidate = os.path.join(os.path.dirname(ffmpeg), "ffprobe")
        if os.path.dirname(ffmpeg) and os.path.isfile(candidate):
            return candidate
        return shutil.which("ffprobe") or "ffprobe"

    # -- duration cache -------------------------------------------------

    # -- titles the user has edited ---------------------------------------

    def _load_overrides(self):
        try:
            with open(self._overrides_path, "r", encoding="utf-8") as handle:
                self._overrides = dict(json.load(handle) or {})
        except (OSError, ValueError):
            self._overrides = {}

    def save_overrides(self):
        try:
            with open(self._overrides_path, "w", encoding="utf-8") as handle:
                json.dump(self._overrides, handle, indent=1, sort_keys=True)
        except OSError:
            logger.warning("Could not write Live TV title overrides")

    def override_for(self, path: str) -> dict:
        return dict(self._overrides.get(str(path), {}))

    def set_title(self, path: str, title: str, description: str = None):
        """Rename a show or commercial as it appears in the guide.

        Titles are derived from filenames, which are often air dates rather
        than programme names, so the user needs to be able to correct them.
        The override is keyed by path and survives rescans.
        """
        key = str(path)
        entry = dict(self._overrides.get(key, {}))
        title = str(title or "").strip()

        if title:
            entry["title"] = title
        else:
            entry.pop("title", None)

        if description is not None:
            description = str(description).strip()
            if description:
                entry["description"] = description
            else:
                entry.pop("description", None)

        if entry:
            self._overrides[key] = entry
        else:
            self._overrides.pop(key, None)
        self.save_overrides()

    def clear_title(self, path: str):
        """Go back to the title derived from the filename."""
        self._overrides.pop(str(path), None)
        self.save_overrides()

    def _apply_override(self, item: "LiveTvMedia"):
        entry = self._overrides.get(item.path)
        if not entry:
            return item
        if entry.get("title"):
            item.title = self._safe_value(entry["title"]) or item.title
            item.renamed = True
        if entry.get("description"):
            item.description = self._safe_value(entry["description"])
        if entry.get("series"):
            item.series = self._safe_value(entry["series"])
        return item

    @staticmethod
    def _safe_value(value):
        return _clean_text(value)

    def _first_non_empty(self, *values):
        for value in values:
            text = self._safe_value(value)
            if text:
                return text
        return ""

    # -- splitting and cropping -------------------------------------------

    def _load_edits(self):
        try:
            with open(self._edits_path, "r", encoding="utf-8") as handle:
                self._edits = dict(json.load(handle) or {})
        except (OSError, ValueError):
            self._edits = {}

    def save_edits(self):
        try:
            with open(self._edits_path, "w", encoding="utf-8") as handle:
                json.dump(self._edits, handle, indent=1, sort_keys=True)
        except OSError:
            logger.warning("Could not write Live TV edits")

    def edits_for(self, path: str) -> list:
        """The episodes a recording has been split into, if any."""
        return [dict(entry) for entry in self._edits.get(str(path), [])]

    def set_edits(self, path: str, episodes):
        """Define how a recording is split up and what is cut out.

        Each episode is ``{"id", "title", "start", "end", "cuts"}`` in
        seconds. An empty list restores the whole file as one programme.
        """
        key = str(path)
        cleaned = []
        for index, entry in enumerate(episodes or [], 1):
            start = max(0, int(entry.get("start") or 0))
            end = int(entry.get("end") or 0)
            if end and end <= start:
                continue
            cuts = []
            for cut in entry.get("cuts") or []:
                try:
                    cut_start, cut_end = int(cut[0]), int(cut[1])
                except (TypeError, ValueError, IndexError):
                    continue
                if cut_end > cut_start:
                    cuts.append([cut_start, cut_end])
            cleaned.append({
                "id": str(entry.get("id") or f"part{index}"),
                "title": str(entry.get("title") or "").strip(),
                "start": start,
                "end": end,
                "cuts": sorted(cuts),
            })

        if cleaned:
            self._edits[key] = cleaned
        else:
            self._edits.pop(key, None)
        self.save_edits()

    def _finish_item(self, item: "LiveTvMedia"):
        """Split a scanned recording and give each piece its real length."""
        source_duration = int(item.duration or 0)
        parts = self._expand_edits(item)

        for part in parts:
            if not part.is_segment:
                continue
            if not part.end and source_duration:
                part.end = source_duration
            part.duration = part.edited_duration()
            # A per-episode title override wins over the one on the file.
            entry = self._overrides.get(part.key)
            if entry:
                if entry.get("title"):
                    part.title = entry["title"]
                    part.renamed = True
                if entry.get("description"):
                    part.description = entry["description"]
            if not self._safe_value(part.title):
                part.title = self._first_non_empty(
                    part.title,
                    part.series,
                    clean_title(Path(part.path).stem),
                    "Program",
                )
        return parts

    def _expand_edits(self, item: "LiveTvMedia"):
        """Turn one recording into the programmes it has been cut into."""
        episodes = self._edits.get(item.path)
        if not episodes:
            return [item]

        parts = []
        for entry in episodes:
            part = LiveTvMedia(
                path=item.path,
                kind=item.kind,
                title=entry.get("title") or item.title,
                series=item.series,
                rating=item.rating,
                description=item.description,
                segment=str(entry.get("id") or "part"),
                start=int(entry.get("start") or 0),
                end=int(entry.get("end") or 0),
                cuts=[list(cut) for cut in entry.get("cuts") or []],
            )
            part.title = self._first_non_empty(
                part.title,
                item.series,
                clean_title(Path(item.path).stem),
                "Program",
            )
            part.renamed = bool(entry.get("title"))
            parts.append(part)
        return parts

    def _load_duration_cache(self):
        try:
            with open(self._cache_path, "r", encoding="utf-8") as handle:
                self._duration_cache = dict(json.load(handle) or {})
        except (OSError, ValueError):
            self._duration_cache = {}

    def save_duration_cache(self):
        try:
            with open(self._cache_path, "w", encoding="utf-8") as handle:
                json.dump(self._duration_cache, handle, indent=1, sort_keys=True)
        except OSError:
            logger.warning("Could not write Live TV duration cache")

    def duration_for(self, path: str) -> int:
        """Seconds, probed once and cached by path, size and mtime."""
        try:
            stat = os.stat(path)
        except OSError:
            return 0
        key = f"{path}|{stat.st_size}|{int(stat.st_mtime)}"
        cached = self._duration_cache.get(key)
        if cached is not None:
            return int(cached)

        seconds = self._probe_duration(path)
        self._duration_cache[key] = seconds
        return seconds

    def _probe_duration(self, path: str) -> int:
        command = [
            self.ffprobe_bin(), "-v", "error",
            "-show_entries", "format=duration",
            "-of", "default=noprint_wrappers=1:nokey=1",
            path,
        ]
        try:
            result = subprocess.run(command, capture_output=True, text=True,
                                    timeout=120, check=False)
        except (OSError, subprocess.SubprocessError):
            return 0
        if result.returncode != 0:
            return 0
        try:
            return max(0, int(round(float((result.stdout or "").strip()))))
        except (TypeError, ValueError):
            return 0

    # -- scanning --------------------------------------------------------

    def scan(self, probe_durations=True, progress=None):
        """Return ``(shows, ads)`` as lists of :class:`LiveTvMedia`."""
        show_paths = []
        ad_paths = []

        for root in self.source_roots():
            ads_root = os.path.join(root, LIVETV_ADS_DIR_NAME)
            for directory, dirnames, filenames in os.walk(root):
                dirnames[:] = [d for d in sorted(dirnames)
                               if not d.startswith(".")]
                in_ads = (directory == ads_root or
                          directory.startswith(ads_root + os.sep))
                for filename in sorted(filenames):
                    full = os.path.join(directory, filename)
                    if not _is_media_file(full):
                        continue
                    (ad_paths if in_ads else show_paths).append(full)

        # The staging area feeds the same pools so Store downloads appear
        # before they are synced.
        staging = self.staging_dir()
        staging_ads = os.path.join(staging, LIVETV_ADS_DIR_NAME)
        for directory, dirnames, filenames in os.walk(staging):
            dirnames[:] = [d for d in sorted(dirnames) if not d.startswith(".")]
            in_ads = (directory == staging_ads or
                      directory.startswith(staging_ads + os.sep))
            for filename in sorted(filenames):
                full = os.path.join(directory, filename)
                if not _is_media_file(full):
                    continue
                (ad_paths if in_ads else show_paths).append(full)

        show_paths = _dedupe_internet_archive(show_paths)
        ad_paths = _dedupe_internet_archive(ad_paths)

        roots = self.source_roots() + [staging]
        shows = []
        ads = []
        total = len(show_paths) + len(ad_paths)
        done = 0

        for path in show_paths:
            root = next((r for r in roots if path.startswith(r + os.sep)),
                        os.path.dirname(path))
            # Live TV is explicitly folder/filename organised (one
            # sub-folder per series), unlike a curated music or TV
            # library. Container metadata on broadcast rips and
            # downloads is frequently a generic tag left by whatever
            # tool produced the file (seen in practice: title/album/
            # artist all reading "Live" regardless of the actual show),
            # so it must never override the deliberate folder placement
            # here - doing so silently reclassifies the show, which
            # changes its device/cache path and forces a full re-encode.
            series = series_name(path, root)
            item = LiveTvMedia(
                path=path,
                kind="show",
                title=program_title(path, series),
                series=series,
                rating="TV-PG",
                description=program_description(path, series),
            )
            if not self._safe_value(item.title):
                item.title = self._first_non_empty(series, Path(path).stem,
                                                  "Program")
            self._apply_override(item)
            if not self._safe_value(item.title):
                item.title = self._first_non_empty(
                    item.series,
                    Path(item.path).stem,
                    "Program",
                )
            if probe_durations:
                item.duration = self.duration_for(path)
            shows.extend(self._finish_item(item))
            done += 1
            if progress and total:
                progress(done, total, item.title)

        for path in ad_paths:
            item = LiveTvMedia(
                path=path,
                kind="ad",
                title=clean_title(Path(path).stem),
                series="Commercials",
                rating="",
            )
            self._apply_override(item)
            if probe_durations:
                item.duration = self.duration_for(path)
            ads.extend(self._finish_item(item))
            done += 1
            if progress and total:
                progress(done, total, item.title)

        if probe_durations:
            self.save_duration_cache()

        return shows, ads


class LiveTvLineup:
    """Channel line-up, persisted alongside the duration cache."""

    def __init__(self, library: LiveTvLibrary):
        self._library = library
        self._path = os.path.join(library.state_dir(), "lineup.json")
        self.channels = []
        self.ad_break_min = DEFAULT_AD_BREAK_MIN
        self.ad_break_max = DEFAULT_AD_BREAK_MAX
        self.seed = 20051115
        self.load()

    def load(self):
        try:
            with open(self._path, "r", encoding="utf-8") as handle:
                payload = dict(json.load(handle) or {})
        except (OSError, ValueError):
            payload = {}

        self.ad_break_min = int(payload.get("ad_break_min",
                                            DEFAULT_AD_BREAK_MIN))
        self.ad_break_max = int(payload.get("ad_break_max",
                                            DEFAULT_AD_BREAK_MAX))
        self.seed = int(payload.get("seed", 20051115))
        self.channels = []
        for row in payload.get("channels") or []:
            try:
                self.channels.append(LiveTvChannel(
                    number=int(row.get("number") or 0),
                    callsign=str(row.get("callsign") or "CH"),
                    name=str(row.get("name") or ""),
                    category=str(row.get("category") or "Series"),
                    logo=str(row.get("logo") or ""),
                    favourite=bool(row.get("favourite", True)),
                    parental_locked=bool(row.get("parental_locked", False)),
                    shows=[str(p) for p in row.get("shows") or []],
                    ads=[str(p) for p in row.get("ads") or []],
                ))
            except (TypeError, ValueError):
                continue

        # Repair line-ups written before numbers were kept unique.
        if self.ensure_unique_numbers():
            logger.info("Repaired duplicate Live TV channel numbers")
            self.save()

    def save(self):
        payload = {
            "ad_break_min": self.ad_break_min,
            "ad_break_max": self.ad_break_max,
            "seed": self.seed,
            "channels": [channel.to_dict() for channel in self.channels],
        }
        try:
            with open(self._path, "w", encoding="utf-8") as handle:
                json.dump(payload, handle, indent=1)
        except OSError:
            logger.warning("Could not write Live TV line-up")

    # -- automatic line-up ----------------------------------------------

    def autobuild(self, shows, ads=None):
        """Add a channel for each series that does not have one yet.

        This is additive on purpose: channels the user added, renamed or
        renumbered by hand are left exactly as they are, and shows already
        assigned somewhere are not moved.
        """
        grouped = {}
        for item in shows:
            grouped.setdefault(item.series or "Live TV", []).append(item.key)

        by_name = {channel.name: channel for channel in self.channels}
        # Which channel already carries a given show. A renamed channel is
        # still recognised by what is assigned to it, so rebuilding never
        # resurrects a duplicate of a channel the user renamed.
        owner = {}
        for channel in self.channels:
            for path in channel.shows:
                owner[path] = channel
        for name in sorted(grouped):
            paths = sorted(grouped[name])
            channel = next((owner[path] for path in paths if path in owner),
                           None) or by_name.get(name)

            if channel is None:
                if len(self.channels) >= LIVETV_MAX_CHANNELS:
                    break
                channel = LiveTvChannel(
                    number=self.next_free_number(),
                    callsign=_callsign_for(name, self.channels),
                    name=name,
                )
                self.channels.append(channel)
                by_name[name] = channel

            for path in paths:
                if path not in owner:
                    channel.shows.append(path)
                    owner[path] = channel
            channel.shows = sorted(set(channel.shows))

        self.ensure_unique_numbers()
        self.channels.sort(key=lambda channel: channel.number)
        return self.channels

    def next_free_number(self) -> int:
        """The lowest channel number not already in use."""
        used = {channel.number for channel in self.channels}
        number = LIVETV_FIRST_CHANNEL
        while number in used:
            number += 1
        return number

    def ensure_unique_numbers(self) -> bool:
        """Give every channel its own number.

        The device looks channels up by number and takes the first match, so
        a duplicate would silently hide one channel's listings behind
        another's. Returns True when something had to be repaired.
        """
        seen = set()
        repaired = False
        for channel in sorted(self.channels, key=lambda c: c.number):
            if channel.number in seen or channel.number <= 0:
                candidate = LIVETV_FIRST_CHANNEL
                while candidate in seen:
                    candidate += 1
                channel.number = candidate
                repaired = True
            seen.add(channel.number)
        if repaired:
            self.channels.sort(key=lambda channel: channel.number)
        return repaired

    def channel_by_number(self, number: int):
        for channel in self.channels:
            if channel.number == int(number):
                return channel
        return None


def _callsign_for(name: str, taken_channels) -> str:
    words = [w for w in re.split(r"[^A-Za-z0-9]+", str(name or "")) if w]
    if not words:
        base = "CH"
    elif len(words) == 1:
        base = words[0][:4].upper()
    else:
        base = "".join(word[0] for word in words)[:4].upper()
    base = base or "CH"
    used = {channel.callsign for channel in taken_channels}
    candidate = base
    suffix = 1
    while candidate in used:
        suffix += 1
        candidate = f"{base[:3]}{suffix}"
    return candidate


class LiveTvScheduler:
    """Builds the seven day rotation and resolves instants against it."""

    def __init__(self, lineup: LiveTvLineup, shows, ads):
        self._lineup = lineup
        # Keyed by programme, not by file: one recording can hold several.
        self._by_path = {item.key: item for item in list(shows) + list(ads)}
        # A station-specific Weather safety spot should not leak into game
        # shows or sports just because those channels use the default pool.
        self._ads = [
            item for item in ads
            if item.duration > 0 and
            "/ads/weather/" not in item.path.replace("\\", "/").lower()
        ]

    def _channel_ads(self, channel: LiveTvChannel):
        if channel.ads:
            pool = [self._by_path.get(path) for path in channel.ads]
            pool = [item for item in pool if item and item.duration > 0]
            if pool:
                return pool
        # Never fill a missing seasonal Weather pool with unrelated national
        # commercials. A forecast block without a break is the honest
        # fallback.
        if (
            channel.category == LIVETV_WEATHER_CATEGORY or
            channel.category.startswith(LIVETV_TV_INFORMATION_CATEGORY_PREFIX)
        ):
            return []
        return self._ads

    def build(self):
        """Return every slot for the whole week, ordered for the device."""
        slots = []
        for channel in self._lineup.channels:
            shows = [self._by_path.get(path) for path in channel.shows]
            shows = [item for item in shows if item and item.duration > 0]
            if not shows:
                continue
            ads = self._channel_ads(channel)
            for day in range(7):
                slots.extend(self._build_day(channel, day, shows, ads))
        return slots

    def _build_day(self, channel: LiveTvChannel, day: int, shows, ads):
        rng = random.Random(f"{self._lineup.seed}:{channel.number}:{day}")
        order = list(shows)
        rng.shuffle(order)

        slots = []
        cursor = 0
        show_index = 0
        last_ad = None

        while cursor < LIVETV_DAY_SECONDS:
            show = order[show_index % len(order)]
            show_index += 1
            duration = min(show.duration, LIVETV_DAY_SECONDS - cursor)
            if duration <= 0:
                break

            block_start = cursor
            block = [LiveTvSlot(
                channel=channel.number,
                day=day,
                start=cursor,
                duration=duration,
                kind="S",
                title=show.title,
                rating=show.rating or "TV-PG",
                description=show.description or f"{channel.name}.",
                path=show.device_relative(),
                source=show.path,
                block_start=block_start,
            )]
            cursor += duration

            if ads and cursor < LIVETV_DAY_SECONDS:
                break_length = rng.randint(self._lineup.ad_break_min,
                                           self._lineup.ad_break_max)
                for _ in range(break_length):
                    if cursor >= LIVETV_DAY_SECONDS:
                        break
                    candidates = [ad for ad in ads
                                  if ad.path != last_ad] or ads
                    ad = rng.choice(candidates)
                    last_ad = ad.path
                    ad_duration = min(ad.duration,
                                      LIVETV_DAY_SECONDS - cursor)
                    if ad_duration <= 0:
                        break
                    block.append(LiveTvSlot(
                        channel=channel.number,
                        day=day,
                        start=cursor,
                        duration=ad_duration,
                        kind="A",
                        # A guide keeps naming the programme through the
                        # break, so the commercial inherits the show's title.
                        title=show.title,
                        rating=show.rating or "TV-PG",
                        description=show.description or f"{channel.name}.",
                        path=ad.device_relative(),
                        source=ad.path,
                        block_start=block_start,
                        ad_title=ad.title,
                    ))
                    cursor += ad_duration

            block_duration = cursor - block_start
            for slot in block:
                slot.block_duration = block_duration
            slots.extend(block)

        # Never leave a sliver at the end of the day: stretch the last slot
        # and the block it belongs to.
        if slots:
            remaining = LIVETV_DAY_SECONDS - slots[-1].end
            if 0 < remaining < LIVETV_MIN_TAIL_SECONDS:
                last_block = slots[-1].block_start
                slots[-1].duration += remaining
                for slot in slots:
                    if slot.block_start == last_block:
                        slot.block_duration += remaining

        return slots


# -- weather channel ------------------------------------------------------
#
# The Weather channel is deliberately not a special slot kind: it is an
# ordinary channel whose "shows" are real MPEG files (a generated colour
# cycle carrying the user's own looped music), built and scheduled through
# the exact same LiveTvScheduler/LiveTvSync path as any other channel. See
# docs/livetv-weather-channel-spec.md section 2 for why.

def weather_forecast_synced(config) -> bool:
    """True once RockPod has produced a forecast bundle at least once.

    This is the same host-side cache build_weather_bundle() in
    services/weather.py writes to - WEATHER_REL_DIR is imported from there
    rather than re-derived here, since it previously drifted out of sync
    (this function checked cache_dir/weather/forecast.tsv, but the real
    path nests the on-device tree under it: cache_dir/weather/<
    WEATHER_REL_DIR>/forecast.tsv). The Weather channel is gated on it
    existing, so the channel simply is not in the line-up until a forecast
    has actually been synced, the same way Live TV itself has no channels
    until something is in ~/Videos/Live.
    """
    from services.weather import WEATHER_REL_DIR

    cache_dir = str(getattr(config, "cache_dir", "") or "")
    if not cache_dir:
        return False
    path = Path(cache_dir) / "weather" / WEATHER_REL_DIR / "forecast.tsv"
    try:
        return path.is_file() and path.stat().st_size > 0
    except OSError:
        return False


def weather_music_files(config) -> list:
    """The user's own local tracks for the Weather channel's music bed.

    Nothing is downloaded here or anywhere else in this feature - if the
    folder is empty the channel simply does not build, exactly like Live TV
    itself when ~/Videos/Live is empty.
    """
    music_dir = str(getattr(config, "livetv_weather_music_dir", "") or
                     os.path.join(os.path.expanduser("~"), "Documents",
                                  "Weather Songs"))
    if not os.path.isdir(music_dir):
        return []

    files = []
    for name in sorted(os.listdir(music_dir)):
        ext = os.path.splitext(name)[1].lower()
        if ext in LIVETV_WEATHER_MUSIC_EXTENSIONS:
            files.append(os.path.join(music_dir, name))
    return files


def weather_assets_dir(config) -> str:
    return str(getattr(config, "livetv_weather_assets_dir", "") or
               os.path.join(os.path.expanduser("~"), "Documents",
                            "Weather Channel"))


def _weather_asset_files(config, relative: str, extensions: set) -> list:
    root = os.path.join(weather_assets_dir(config), relative)
    if not os.path.isdir(root):
        return []
    return [
        os.path.join(root, name)
        for name in sorted(os.listdir(root))
        if os.path.isfile(os.path.join(root, name)) and
        os.path.splitext(name)[1].lower() in extensions
    ]


def _weather_unique_media(paths) -> tuple:
    """Return byte-distinct clips and the duplicate paths that were skipped."""
    unique = []
    duplicates = []
    fingerprints = set()
    for path in paths:
        digest = hashlib.sha256()
        try:
            with open(path, "rb") as handle:
                while True:
                    chunk = handle.read(1024 * 1024)
                    if not chunk:
                        break
                    digest.update(chunk)
        except OSError:
            continue
        fingerprint = digest.digest()
        if fingerprint in fingerprints:
            duplicates.append(path)
            continue
        fingerprints.add(fingerprint)
        unique.append(path)
    return unique, duplicates


def _weather_media_has_audio(path: str, ffprobe_bin="ffprobe") -> bool:
    try:
        result = subprocess.run(
            [
                ffprobe_bin, "-v", "error", "-select_streams", "a:0",
                "-show_entries", "stream=index", "-of", "csv=p=0", path,
            ],
            capture_output=True, text=True, check=False,
        )
    except OSError:
        return False
    return result.returncode == 0 and bool(result.stdout.strip())


def weather_current_code(config) -> str:
    """Return the hourly condition nearest the host's local wall clock.

    Forecast hourly stamps are deliberately local, matching the clock used
    by the iPod schedule. A daily row is the conservative fallback for old
    forecast bundles that predate hourly data.
    """
    from services.weather import WEATHER_REL_DIR

    path = (Path(str(getattr(config, "cache_dir", "") or "")) /
            "weather" / WEATHER_REL_DIR / "forecast.tsv")
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return ""

    now = datetime.now().replace(tzinfo=None)
    nearest = None
    daily = ""
    for line in lines[1:]:
        fields = line.split("\t")
        if not fields:
            continue
        if fields[0] == "hourly" and len(fields) > 2:
            try:
                stamp = datetime.fromisoformat(fields[1]).replace(tzinfo=None)
            except ValueError:
                continue
            distance = abs((stamp - now).total_seconds())
            if nearest is None or distance < nearest[0]:
                nearest = (distance, fields[2].strip().lower())
        elif not daily and len(fields) > 1:
            daily = fields[1].strip().lower()
    return nearest[1] if nearest is not None else daily


def _weather_forecast_path(config) -> Path:
    from services.weather import WEATHER_REL_DIR

    return (Path(str(getattr(config, "cache_dir", "") or "")) /
            "weather" / WEATHER_REL_DIR / "forecast.tsv")


def _weather_number(value, default=0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return float(default)


def _weather_celsius(value, units: str) -> int:
    temperature = _weather_number(value)
    if units == "imperial":
        temperature = (temperature - 32.0) * 5.0 / 9.0
    return int(round(temperature))


def _weather_kmh(value, units: str) -> int:
    speed = _weather_number(value)
    if units == "imperial":
        speed *= 1.609344
    return int(round(speed))


def _weather_wind_direction(value) -> str:
    degrees = _weather_number(value) % 360.0
    points = (
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW",
    )
    return points[int((degrees + 11.25) // 22.5) % len(points)]


def weather_forecast_for_presenter(config, now=None) -> dict:
    """Read the synced forecast into exact, display-ready metric values.

    Presenter artwork is composed on the host, never in the iPod decode or
    framebuffer path. Temperatures are always returned in Celsius and wind
    speed in km/h, including when an older/user-supplied forecast bundle was
    fetched in imperial units.
    """
    path = _weather_forecast_path(config)
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return {}
    if not lines:
        return {}

    header = lines[0].split("\t")
    location = header[1].strip() if len(header) > 1 else "Local Weather"
    units = header[7].strip().lower() if len(header) > 7 else "metric"
    if units not in {"metric", "imperial"}:
        units = "metric"

    daily = []
    hourly = []
    for line in lines[1:]:
        fields = line.split("\t")
        if not fields:
            continue
        if fields[0] == "hourly":
            if len(fields) < 8:
                continue
            try:
                stamp = datetime.fromisoformat(fields[1]).replace(tzinfo=None)
            except ValueError:
                continue
            hourly.append({
                "stamp": stamp,
                "code": fields[2].strip().lower(),
                "condition": fields[3].strip() or "Weather",
                "temperature_c": _weather_celsius(fields[4], units),
                "precipitation": int(round(_weather_number(fields[5]))),
                "wind_kmh": _weather_kmh(fields[6], units),
                "wind_direction": _weather_wind_direction(fields[7]),
            })
            continue
        if len(fields) < 8:
            continue
        try:
            stamp = datetime.fromisoformat(fields[0]).replace(tzinfo=None)
        except ValueError:
            continue
        daily.append({
            "stamp": stamp,
            "code": fields[1].strip().lower(),
            "condition": fields[2].strip() or "Weather",
            "low_c": _weather_celsius(fields[3], units),
            "high_c": _weather_celsius(fields[4], units),
            "precipitation": int(round(_weather_number(fields[5]))),
            "wind_kmh": _weather_kmh(fields[6], units),
            "wind_direction": _weather_wind_direction(fields[7]),
        })

    if not hourly and not daily:
        return {}
    wall_clock = (now or datetime.now()).replace(tzinfo=None)
    current = min(
        hourly, key=lambda row: abs((row["stamp"] - wall_clock).total_seconds())
    ) if hourly else {
        "stamp": daily[0]["stamp"],
        "code": daily[0]["code"],
        "condition": daily[0]["condition"],
        "temperature_c": daily[0]["high_c"],
        "precipitation": daily[0]["precipitation"],
        "wind_kmh": daily[0]["wind_kmh"],
        "wind_direction": daily[0]["wind_direction"],
    }
    today = min(
        daily, key=lambda row: abs((row["stamp"].date() -
                                   wall_clock.date()).days)
    ) if daily else {
        "low_c": current["temperature_c"],
        "high_c": current["temperature_c"],
    }
    return {
        "location": location,
        "current": current,
        "today": today,
        "daily": daily,
        "hourly": hourly,
    }


def _weather_font(size: int, bold=False):
    from PIL import ImageFont

    names = (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
        if bold else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf",
    )
    for name in names:
        try:
            return ImageFont.truetype(name, max(8, int(size)))
        except OSError:
            pass
    return ImageFont.load_default()


def _weather_text(draw, xy, text, size, *, bold=False,
                  fill=(245, 250, 255, 255), anchor=None):
    draw.text(
        xy, str(text), font=_weather_font(size, bold=bold), fill=fill,
        anchor=anchor, stroke_width=max(1, int(size) // 28),
        stroke_fill=(0, 14, 34, 230),
    )


def _render_weather_current(image, forecast):
    from PIL import ImageDraw

    width, height = image.size
    sx, sy = width / 1448.0, height / 1086.0
    draw = ImageDraw.Draw(image, "RGBA")
    box = (45 * sx, 105 * sy, 800 * sx, 845 * sy)
    draw.rounded_rectangle(box, radius=24 * sx, fill=(3, 22, 55, 244),
                           outline=(78, 184, 255, 255), width=max(2, int(4*sx)))
    current = forecast["current"]
    today = forecast["today"]
    _weather_text(draw, (92*sx, 142*sy), "CURRENT CONDITIONS",
                  48*sx, bold=True)
    _weather_text(draw, (92*sx, 220*sy), forecast["location"].upper(),
                  30*sx, bold=True, fill=(104, 211, 255, 255))
    _weather_text(draw, (92*sx, 292*sy), current["condition"].upper(),
                  38*sx, bold=True)
    _weather_text(draw, (92*sx, 370*sy),
                  f'{current["temperature_c"]}°C', 150*sx, bold=True)
    rule = (92*sx, 575*sy, 740*sx, 579*sy)
    draw.rectangle(rule, fill=(63, 160, 232, 230))
    details = (
        ("HIGH", f'{today["high_c"]}°C'),
        ("LOW", f'{today["low_c"]}°C'),
        ("WIND", f'{current["wind_direction"]} {current["wind_kmh"]} km/h'),
    )
    for index, (label, value) in enumerate(details):
        y = (615 + index * 72) * sy
        _weather_text(draw, (92*sx, y), label, 31*sx, bold=True)
        _weather_text(draw, (742*sx, y), value, 31*sx, bold=True,
                      anchor="ra")


def _render_weather_extended(image, forecast):
    from PIL import ImageDraw

    width, height = image.size
    sx, sy = width / 1448.0, height / 1086.0
    draw = ImageDraw.Draw(image, "RGBA")
    draw.rounded_rectangle(
        (35*sx, 95*sy, 925*sx, 800*sy), radius=22*sx,
        fill=(3, 22, 55, 245), outline=(78, 184, 255, 255),
        width=max(2, int(4*sx)),
    )
    _weather_text(draw, (78*sx, 128*sy), "EXTENDED OUTLOOK",
                  47*sx, bold=True)
    _weather_text(draw, (870*sx, 143*sy),
                  forecast["location"].upper(), 24*sx, bold=True,
                  fill=(104, 211, 255, 255), anchor="ra")
    days = forecast["daily"][:5]
    if not days:
        return
    left, top, column_w = 65, 230, 164
    for index, day in enumerate(days):
        x = (left + index * column_w) * sx
        right = (left + index * column_w + 146) * sx
        draw.rounded_rectangle(
            (x, top*sy, right, 755*sy), radius=14*sx,
            fill=(8, 51, 100, 232), outline=(74, 146, 211, 255),
            width=max(2, int(3*sx)),
        )
        center = (left + index * column_w + 73) * sx
        _weather_text(draw, (center, 270*sy),
                      day["stamp"].strftime("%a").upper(),
                      31*sx, bold=True, anchor="mm")
        condition = day["condition"].upper()
        if len(condition) > 13:
            condition = condition[:12] + "…"
        _weather_text(draw, (center, 375*sy), condition,
                      20*sx, bold=True, anchor="mm")
        _weather_text(draw, (center, 520*sy), f'{day["high_c"]}°C',
                      49*sx, bold=True, anchor="mm")
        _weather_text(draw, (center, 610*sy), f'{day["low_c"]}°C',
                      37*sx, bold=True, fill=(155, 210, 255, 255),
                      anchor="mm")
        _weather_text(draw, (center, 700*sy),
                      f'{day["precipitation"]}% rain',
                      20*sx, anchor="mm")


def _render_weather_tracker(image, forecast):
    from PIL import ImageDraw

    width, height = image.size
    sx, sy = width / 1448.0, height / 1086.0
    draw = ImageDraw.Draw(image, "RGBA")
    draw.rounded_rectangle(
        (805*sx, 165*sy, 1402*sx, 870*sy), radius=22*sx,
        fill=(3, 22, 55, 246), outline=(78, 184, 255, 255),
        width=max(2, int(4*sx)),
    )
    current = forecast["current"]
    today = forecast["today"]
    _weather_text(draw, (1102*sx, 215*sy), "LOCAL WEATHER",
                  39*sx, bold=True, anchor="mm")
    _weather_text(draw, (1102*sx, 280*sy),
                  forecast["location"].upper(), 25*sx, bold=True,
                  fill=(104, 211, 255, 255), anchor="mm")
    _weather_text(draw, (1102*sx, 390*sy),
                  current["condition"].upper(), 45*sx, bold=True,
                  anchor="mm")
    _weather_text(draw, (1102*sx, 500*sy),
                  f'{current["temperature_c"]}°C', 90*sx, bold=True,
                  anchor="mm")
    details = (
        f'RAIN {current["precipitation"]}%',
        f'HIGH {today["high_c"]}°C  LOW {today["low_c"]}°C',
        f'WIND {current["wind_direction"]} {current["wind_kmh"]} km/h',
    )
    for index, value in enumerate(details):
        _weather_text(draw, (1102*sx, (640 + index*68)*sy), value,
                      28*sx, bold=True, anchor="mm")


def _weather_board_base(title, forecast, subtitle=""):
    from PIL import Image, ImageDraw

    width, height = 1448, 1086
    image = Image.new("RGBA", (width, height), (3, 22, 55, 255))
    draw = ImageDraw.Draw(image, "RGBA")
    for y in range(height):
        ratio = y / max(1, height - 1)
        draw.line(
            (0, y, width, y),
            fill=(
                int(3 + 12 * ratio),
                int(22 + 48 * ratio),
                int(55 + 61 * ratio),
                255,
            ),
        )
    draw.rectangle((0, 0, width, 126), fill=(4, 45, 91, 255))
    draw.rectangle((0, 120, width, 132), fill=(47, 177, 236, 255))
    draw.rectangle((0, height - 78, width, height), fill=(4, 39, 82, 250))
    _weather_text(draw, (52, 55), title, 55, bold=True, anchor="lm")
    _weather_text(
        draw, (width - 52, 55), forecast["location"].upper(),
        31, bold=True, fill=(105, 214, 255, 255), anchor="rm")
    if subtitle:
        _weather_text(
            draw, (52, height - 39), subtitle, 27, bold=True,
            fill=(178, 225, 250, 255), anchor="lm")
    _weather_text(
        draw, (width - 52, height - 39), "WX 102", 28, bold=True,
        anchor="rm")
    return image, draw


def _render_weather_hourly_board(forecast):
    hourly = list(forecast.get("hourly") or [])
    current_stamp = forecast["current"].get("stamp")
    if current_stamp is not None:
        upcoming = [row for row in hourly if row["stamp"] >= current_stamp]
    else:
        upcoming = hourly
    rows = (upcoming or hourly)[:8]
    image, draw = _weather_board_base(
        "NEXT 24 HOURS", forecast,
        "TEMPERATURE  •  PRECIPITATION  •  WIND")
    if not rows:
        _weather_text(draw, (724, 540), "HOURLY DATA UNAVAILABLE",
                      46, bold=True, anchor="mm")
        return image

    left, right = 68, 1380
    top, bottom = 225, 900
    column_w = (right - left) / len(rows)
    temperatures = [row["temperature_c"] for row in rows]
    low = min(temperatures)
    high = max(temperatures)
    spread = max(4, high - low)
    points = []
    for index, row in enumerate(rows):
        x = left + column_w * (index + 0.5)
        y = bottom - 145 - (row["temperature_c"] - low) / spread * 315
        points.append((x, y))
        if index % 2:
            draw.rectangle(
                (left + index * column_w, top,
                 left + (index + 1) * column_w, bottom),
                fill=(16, 67, 116, 105),
            )
        _weather_text(
            draw, (x, top + 38), row["stamp"].strftime("%-I %p"),
            28, bold=True, anchor="mm")
        _weather_text(
            draw, (x, bottom - 78), f'{row["precipitation"]}%',
            28, bold=True, fill=(105, 214, 255, 255), anchor="mm")
        _weather_text(
            draw, (x, bottom - 22),
            f'{row["wind_direction"]} {row["wind_kmh"]}',
            22, anchor="mm")
    for index in range(1, len(points)):
        draw.line((points[index - 1], points[index]),
                  fill=(255, 196, 67, 255), width=10)
    for (x, y), row in zip(points, rows):
        draw.ellipse((x - 13, y - 13, x + 13, y + 13),
                     fill=(255, 196, 67, 255),
                     outline=(255, 255, 255, 255), width=4)
        _weather_text(draw, (x, y - 52), f'{row["temperature_c"]}°',
                      34, bold=True, anchor="mm")
        condition = row["condition"].upper()
        if len(condition) > 12:
            condition = condition[:11] + "…"
        _weather_text(draw, (x, y + 48), condition,
                      18, bold=True, anchor="mm")
    return image


def _render_weather_seven_day_board(forecast):
    days = list(forecast.get("daily") or [])[:7]
    image, draw = _weather_board_base(
        "7 DAY FORECAST", forecast,
        "FORECAST GENERATED FROM THE LATEST WEATHER SYNC")
    if not days:
        _weather_text(draw, (724, 540), "DAILY DATA UNAVAILABLE",
                      46, bold=True, anchor="mm")
        return image

    left, top = 45, 205
    gap = 12
    width = (1448 - left * 2 - gap * 6) / 7
    for index, day in enumerate(days):
        x = left + index * (width + gap)
        fill = (9, 70, 126, 245) if index == 0 else (7, 50, 99, 238)
        draw.rounded_rectangle(
            (x, top, x + width, 915), radius=18, fill=fill,
            outline=(72, 164, 220, 255), width=3)
        _weather_text(
            draw, (x + width / 2, top + 55),
            "TODAY" if index == 0 else day["stamp"].strftime("%a").upper(),
            29, bold=True, anchor="mm")
        condition = day["condition"].upper()
        if len(condition) > 13:
            condition = condition[:12] + "…"
        _weather_text(draw, (x + width / 2, top + 170), condition,
                      20, bold=True, anchor="mm")
        _weather_text(draw, (x + width / 2, top + 325),
                      f'{day["high_c"]}°', 70, bold=True, anchor="mm")
        _weather_text(draw, (x + width / 2, top + 425),
                      f'{day["low_c"]}°', 49, bold=True,
                      fill=(160, 214, 255, 255), anchor="mm")
        _weather_text(draw, (x + width / 2, top + 545),
                      f'RAIN {day["precipitation"]}%', 22,
                      bold=True, anchor="mm")
        _weather_text(
            draw, (x + width / 2, top + 620),
            f'{day["wind_direction"]} {day["wind_kmh"]} km/h',
            19, anchor="mm")
    return image


def _weather_upcoming_hourly(forecast, count=12):
    hourly = list(forecast.get("hourly") or [])
    current_stamp = (forecast.get("current") or {}).get("stamp")
    if current_stamp is not None:
        upcoming = [row for row in hourly if row["stamp"] >= current_stamp]
    else:
        upcoming = hourly
    return (upcoming or hourly)[:count]


def _render_weather_precipitation_board(forecast):
    rows = _weather_upcoming_hourly(forecast)
    image, draw = _weather_board_base(
        "PRECIPITATION TIMELINE", forecast,
        "HOURLY CHANCE OF PRECIPITATION  •  LIVE FORECAST DATA")
    if not rows:
        _weather_text(draw, (724, 540), "HOURLY DATA UNAVAILABLE",
                      46, bold=True, anchor="mm")
        return image

    left, right = 64, 1384
    top, bottom = 215, 900
    width = (right - left) / len(rows)
    for guide in (25, 50, 75, 100):
        y = bottom - guide / 100 * 540
        draw.line((left, y, right, y), fill=(92, 151, 194, 150), width=2)
        _weather_text(draw, (left + 5, y - 10), f"{guide}%",
                      20, anchor="lm")
    for index, row in enumerate(rows):
        x0 = left + index * width + 8
        x1 = left + (index + 1) * width - 8
        value = max(0, min(100, int(row["precipitation"])))
        y = bottom - value / 100 * 540
        fill = (
            (70, 207, 255, 255) if value < 50 else
            (36, 151, 239, 255) if value < 75 else
            (116, 87, 230, 255)
        )
        draw.rounded_rectangle(
            (x0, y, x1, bottom), radius=8, fill=fill,
            outline=(206, 240, 255, 220), width=2)
        _weather_text(draw, ((x0 + x1) / 2, max(top + 28, y - 27)),
                      f"{value}%", 24, bold=True, anchor="mm")
        _weather_text(draw, ((x0 + x1) / 2, bottom + 38),
                      row["stamp"].strftime("%-I %p"), 20,
                      bold=True, anchor="mm")
        condition = row["condition"].upper()
        if len(condition) > 10:
            condition = condition[:9] + "…"
        _weather_text(draw, ((x0 + x1) / 2, bottom + 82),
                      condition, 16, anchor="mm")
    return image


def _render_weather_wind_board(forecast):
    rows = _weather_upcoming_hourly(forecast, count=8)
    image, draw = _weather_board_base(
        "WIND OUTLOOK", forecast,
        "SPEED AND DIRECTION  •  NEXT 24 HOURS")
    if not rows:
        _weather_text(draw, (724, 540), "HOURLY DATA UNAVAILABLE",
                      46, bold=True, anchor="mm")
        return image

    left, top = 58, 215
    gap = 12
    width = (1448 - left * 2 - gap * (len(rows) - 1)) / len(rows)
    maximum = max(20, max(row["wind_kmh"] for row in rows))
    for index, row in enumerate(rows):
        x = left + index * (width + gap)
        draw.rounded_rectangle(
            (x, top, x + width, 905), radius=15,
            fill=(7, 51, 99, 242), outline=(72, 164, 220, 255), width=3)
        _weather_text(draw, (x + width / 2, top + 52),
                      row["stamp"].strftime("%-I %p"), 27,
                      bold=True, anchor="mm")
        bar_bottom = top + 535
        bar_top = bar_bottom - row["wind_kmh"] / maximum * 310
        draw.rounded_rectangle(
            (x + width * .29, bar_top, x + width * .71, bar_bottom),
            radius=12, fill=(43, 187, 239, 255),
            outline=(212, 243, 255, 230), width=3)
        _weather_text(draw, (x + width / 2, bar_top - 42),
                      f'{row["wind_kmh"]}', 42, bold=True, anchor="mm")
        _weather_text(draw, (x + width / 2, bar_bottom + 42),
                      "km/h", 20, anchor="mm")
        _weather_text(draw, (x + width / 2, bar_bottom + 104),
                      row["wind_direction"], 36, bold=True,
                      fill=(255, 197, 71, 255), anchor="mm")
    return image


def _render_weather_radar_board(forecast, radar_path, metadata):
    from PIL import Image, ImageDraw

    with Image.open(radar_path) as opened:
        radar = opened.convert("RGBA")
    image, draw = _weather_board_base(
        "LOCAL RADAR", forecast,
        "RADAR: RAINVIEWER  •  MAP: OPENSTREETMAP")
    scale = min(1340 / radar.width, 790 / radar.height)
    radar = radar.resize(
        (max(1, int(round(radar.width * scale))),
         max(1, int(round(radar.height * scale)))),
        Image.Resampling.LANCZOS)
    x = (1448 - radar.width) // 2
    y = 168 + (790 - radar.height) // 2
    image.alpha_composite(radar, (x, y))
    draw = ImageDraw.Draw(image, "RGBA")
    draw.rectangle((x, y, x + radar.width, y + 58),
                   fill=(2, 20, 50, 215))
    stamp = ""
    frames = list((metadata or {}).get("frames") or [])
    target_name = os.path.basename(radar_path)
    row = next(
        (item for item in frames if item.get("file") == target_name), {})
    generated = str(row.get("generated_utc") or "")
    if generated:
        try:
            stamp = datetime.fromisoformat(
                generated.replace("Z", "+00:00")).strftime("%H:%M UTC")
        except ValueError:
            stamp = generated
    _weather_text(
        draw, (x + 24, y + 29),
        f'LATEST AVAILABLE RADAR  {stamp}'.strip(),
        25, bold=True, anchor="lm")
    current = forecast["current"]
    _weather_text(
        draw, (x + radar.width - 24, y + 29),
        f'{current["condition"].upper()}  {current["temperature_c"]}°C',
        25, bold=True, fill=(105, 214, 255, 255), anchor="rm")
    return image


def render_weather_data_panels(config, output_dir: str, forecast) -> list:
    """Render data-only 4:3 boards and the latest real radar map."""
    map_dir = _weather_forecast_path(config).parent / "maps"
    os.makedirs(output_dir, exist_ok=True)
    forecast_mtime = _weather_forecast_path(config).stat().st_mtime
    renderer_mtime = os.path.getmtime(__file__)
    specs = (
        ("hourly-forecast.png", _render_weather_hourly_board),
        ("seven-day-forecast.png", _render_weather_seven_day_board),
        ("precipitation-timeline.png",
         _render_weather_precipitation_board),
        ("wind-outlook.png", _render_weather_wind_board),
    )
    results = []
    for name, renderer in specs:
        target = os.path.join(output_dir, name)
        if not (os.path.isfile(target) and os.path.getsize(target) > 0 and
                os.path.getmtime(target) >=
                max(forecast_mtime, renderer_mtime)):
            renderer(forecast).convert("RGB").save(
                target, "PNG", optimize=True)
        results.append(target)

    radar_files = sorted(map_dir.glob("radar-*.png"))
    metadata = {}
    metadata_path = map_dir / "radar.json"
    try:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, TypeError):
        metadata = {}
    for index, source in enumerate(radar_files):
        target = os.path.join(
            output_dir, f"local-radar-frame-{index}.png")
        newest = max(
            forecast_mtime, source.stat().st_mtime, renderer_mtime)
        if not (os.path.isfile(target) and os.path.getsize(target) > 0 and
                os.path.getmtime(target) >= newest):
            _render_weather_radar_board(
                forecast, source, metadata).convert("RGB").save(
                    target, "PNG", optimize=True)
        results.append(target)
    return results


def render_weather_presenters(config, output_dir: str) -> list:
    """Compose exact forecast values onto presenter and data-only boards."""
    from PIL import Image

    backplates = _weather_asset_files(
        config, "presenter-backplates", LIVETV_WEATHER_IMAGE_EXTENSIONS)
    if not backplates:
        backplates = _weather_asset_files(
            config, "presenter", LIVETV_WEATHER_IMAGE_EXTENSIONS)
    forecast = weather_forecast_for_presenter(config)
    if not forecast:
        return []

    os.makedirs(output_dir, exist_ok=True)
    forecast_mtime = _weather_forecast_path(config).stat().st_mtime
    renderer_mtime = os.path.getmtime(__file__)
    results = render_weather_data_panels(config, output_dir, forecast)
    for source in backplates:
        stem = Path(source).stem
        target = os.path.join(output_dir, f"{stem}-celsius.png")
        newest = max(
            forecast_mtime, os.path.getmtime(source), renderer_mtime)
        if (os.path.isfile(target) and os.path.getsize(target) > 0 and
                os.path.getmtime(target) >= newest):
            results.append(target)
            continue
        try:
            with Image.open(source) as opened:
                image = opened.convert("RGBA")
            lower = stem.lower()
            if "extended" in lower or "outlook" in lower:
                _render_weather_extended(image, forecast)
            elif "storm" in lower or "tracker" in lower:
                _render_weather_tracker(image, forecast)
            else:
                _render_weather_current(image, forecast)
            image.convert("RGB").save(target, "PNG", optimize=True)
        except (OSError, ValueError) as exc:
            logger.warning("Weather presenter render failed for %s: %s",
                           source, exc)
            continue
        results.append(target)
    # Interleave data-only boards with presenter backplates so the 120-second
    # panel clock never feels like a presenter slideshow.
    data = [path for path in results if path not in {
        os.path.join(output_dir, f"{Path(source).stem}-celsius.png")
        for source in backplates
    }]
    presenters = [path for path in results if path not in data]
    radar = [
        path for path in data
        if "local-radar-frame-" in os.path.basename(path)
    ]
    boards = [path for path in data if path not in radar]
    ordered = []
    if boards:
        ordered.append(boards.pop(0))
    if presenters:
        ordered.append(presenters.pop(0))
    ordered.extend(radar)
    if boards:
        ordered.append(boards.pop(0))
    while boards or presenters:
        if presenters:
            ordered.append(presenters.pop(0))
        if boards:
            ordered.append(boards.pop(0))
    return ordered


def render_weather_transition_slates(config, output_dir: str) -> tuple:
    """Build forecast-aware station IDs for the report insert boundaries."""
    from PIL import Image, ImageDraw

    forecast = weather_forecast_for_presenter(config)
    if not forecast:
        return ()
    os.makedirs(output_dir, exist_ok=True)
    forecast_mtime = _weather_forecast_path(config).stat().st_mtime
    targets = (
        os.path.join(output_dir, "local-weather-intro.png"),
        os.path.join(output_dir, "forecast-continues.png"),
    )
    if all(
        os.path.isfile(path) and os.path.getsize(path) > 0 and
        os.path.getmtime(path) >= forecast_mtime
        for path in targets
    ):
        return targets

    width, height = 640, 480

    def background():
        image = Image.new("RGBA", (width, height), (3, 22, 55, 255))
        draw = ImageDraw.Draw(image, "RGBA")
        for y in range(height):
            ratio = y / max(1, height - 1)
            draw.line(
                (0, y, width, y),
                fill=(
                    int(3 + 7 * ratio),
                    int(32 + 42 * ratio),
                    int(73 + 67 * ratio),
                    255,
                ),
            )
        # Original cable-weather glass/swoosh treatment.
        for offset, alpha in ((0, 120), (16, 75), (34, 40)):
            draw.arc(
                (-170 + offset, 150 - offset, 730 + offset, 660 + offset),
                190, 350, fill=(52, 187, 255, alpha), width=8,
            )
        draw.polygon(
            [(0, 382), (640, 310), (640, 480), (0, 480)],
            fill=(5, 36, 79, 205),
        )
        draw.rectangle((0, 458, 640, 480), fill=(43, 167, 235, 230))
        return image, draw

    current = forecast["current"]
    intro, draw = background()
    _weather_text(draw, (596, 42), "WX 102", 23, bold=True, anchor="ra",
                  fill=(105, 214, 255, 255))
    _weather_text(draw, (42, 50), forecast["location"].upper(),
                  28, bold=True, fill=(105, 214, 255, 255))
    _weather_text(draw, (42, 100), "LOCAL WEATHER", 58, bold=True)
    _weather_text(draw, (42, 188), f'{current["temperature_c"]}°C',
                  112, bold=True)
    _weather_text(
        draw, (45, 335),
        f'{current["condition"].upper()}  •  '
        f'RAIN {current["precipitation"]}%  •  '
        f'WIND {current["wind_direction"]} {current["wind_kmh"]} km/h',
        25, bold=True,
    )
    _weather_text(draw, (594, 438), "LIVE FORECAST", 23, bold=True,
                  anchor="ra")
    intro.convert("RGB").save(targets[0], "PNG", optimize=True)

    outro, draw = background()
    _weather_text(draw, (596, 42), "WX 102", 23, bold=True, anchor="ra",
                  fill=(105, 214, 255, 255))
    _weather_text(draw, (320, 132), "FORECAST", 66, bold=True, anchor="mm")
    _weather_text(draw, (320, 207), "CONTINUES", 66, bold=True,
                  fill=(105, 214, 255, 255), anchor="mm")
    draw.rounded_rectangle(
        (105, 292, 535, 363), radius=14,
        fill=(5, 27, 60, 220), outline=(66, 180, 244, 255), width=3,
    )
    _weather_text(draw, (320, 328), "NEXT  •  CURRENT CONDITIONS",
                  26, bold=True, anchor="mm")
    _weather_text(draw, (320, 405), forecast["location"].upper(),
                  25, bold=True, anchor="mm")
    outro.convert("RGB").save(targets[1], "PNG", optimize=True)
    return targets


def render_weather_channel_bug(output_dir: str, *,
                               label="RECORDED REPORT",
                               filename="wx-102-recorded-report-v2.png") -> str:
    """Return a small identity overlay for recorded Weather inserts."""
    from PIL import Image, ImageDraw

    os.makedirs(output_dir, exist_ok=True)
    target = os.path.join(output_dir, filename)
    if os.path.isfile(target) and os.path.getsize(target) > 0:
        return target
    image = Image.new("RGBA", (98, 34), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image, "RGBA")
    draw.rounded_rectangle(
        (0, 0, 97, 33), radius=7, fill=(2, 20, 50, 205),
        outline=(77, 188, 247, 235), width=2,
    )
    _weather_text(draw, (49, 13), "WX 102", 16, bold=True, anchor="mm")
    _weather_text(draw, (49, 26), label, 7, bold=True,
                  fill=(105, 214, 255, 255), anchor="mm")
    image.save(target, "PNG", optimize=True)
    return target


def weather_condition_is_snow(code: str) -> bool:
    code = (code or "").strip().lower()
    return any(word in code for word in
               ("snow", "sleet", "ice", "freezing"))


def weather_interstitial_kinds(code: str, now=None) -> tuple:
    """Return condition/season folders in priority order.

    Snow and ice always win, including an out-of-season forecast. Summer
    anchor reads are used from June through August; every seasonal folder
    falls back to the general rotation when it is absent or empty.
    """
    if weather_condition_is_snow(code):
        return ("snow", "general")
    wall_clock = now or datetime.now()
    if wall_clock.month in (6, 7, 8):
        return ("summer", "general")
    return ("general",)


def _weather_condition_group(code: str) -> str:
    code = (code or "").strip().lower()
    if weather_condition_is_snow(code):
        return "snow"
    if any(word in code for word in ("thunder", "storm")):
        return "thunder"
    if any(word in code for word in ("rain", "drizzle", "shower")):
        return "rain"
    if any(word in code for word in ("fog", "mist", "haze")):
        return "fog"
    if "partly" in code or "mainly_clear" in code:
        return "partly_cloudy"
    if any(word in code for word in ("cloud", "overcast")):
        return "cloudy"
    if any(word in code for word in ("clear", "sunny")):
        return "clear"
    return "unknown"


def render_weather_clip_lower_third(config, output_dir: str) -> str:
    """Render the live 64-pixel weather ribbon used on recorded news.

    The icon comes from the committed dimensional broadcast atlas.  Every
    number and label comes from the same synced forecast as the full-screen
    boards, so recorded news never pretends to contain live forecast data.
    """
    from PIL import Image, ImageDraw

    forecast = weather_forecast_for_presenter(config)
    if not forecast:
        return ""
    current = forecast["current"]
    today = forecast["today"]
    group = _weather_condition_group(current.get("code", ""))
    icon_names = {
        "clear": "clear_day",
        "partly_cloudy": "partly_cloudy",
        "cloudy": "cloudy",
        "rain": "rain",
        "thunder": "thunderstorm",
        "snow": "snow",
        "fog": "fog",
        "unknown": "unknown",
    }
    icon_path = os.path.join(
        _weather_icons_source_dir(),
        f"{icon_names.get(group, 'unknown')}.64x64x24.bmp")
    os.makedirs(output_dir, exist_ok=True)
    target = os.path.join(output_dir, "live-weather-lower-third-v1.png")
    forecast_mtime = _weather_forecast_path(config).stat().st_mtime
    newest = max(
        forecast_mtime,
        os.path.getmtime(icon_path) if os.path.isfile(icon_path) else 0,
        os.path.getmtime(__file__),
    )
    if (os.path.isfile(target) and os.path.getsize(target) > 0 and
            os.path.getmtime(target) >= newest):
        return target

    image = Image.new("RGBA", (320, 64), (3, 22, 55, 255))
    draw = ImageDraw.Draw(image, "RGBA")
    for y in range(64):
        ratio = y / 63
        draw.line(
            (0, y, 320, y),
            fill=(3, int(24 + ratio * 31), int(58 + ratio * 43), 255))
    draw.rectangle((0, 0, 320, 3), fill=(47, 190, 244, 255))
    draw.rectangle((0, 3, 67, 64), fill=(4, 48, 96, 255))
    draw.rectangle((67, 3, 69, 64), fill=(47, 147, 210, 230))
    _weather_text(draw, (33, 23), "WX 102", 14, bold=True, anchor="mm")
    _weather_text(
        draw, (33, 43), "LOCAL", 8, bold=True,
        fill=(105, 214, 255, 255), anchor="mm")
    _weather_text(draw, (33, 55), "WEATHER", 7, bold=True, anchor="mm")

    if os.path.isfile(icon_path):
        try:
            with Image.open(icon_path) as opened:
                icon = opened.convert("RGBA").resize(
                    (48, 48), Image.Resampling.LANCZOS)
            pixels = []
            for red, green, blue, alpha in icon.getdata():
                if red > 205 and blue > 205 and green < 100:
                    pixels.append((red, green, blue, 0))
                else:
                    pixels.append((red, green, blue, alpha))
            icon.putdata(pixels)
            image.alpha_composite(icon, (72, 10))
        except (OSError, ValueError):
            pass

    location = str(forecast.get("location") or "LOCAL").upper()
    if len(location) > 19:
        location = location[:18] + "…"
    condition = str(current.get("condition") or "Weather").upper()
    if len(condition) > 19:
        condition = condition[:18] + "…"
    _weather_text(draw, (124, 16), location, 10, bold=True, anchor="lm")
    _weather_text(
        draw, (312, 20), f'{current["temperature_c"]}°C',
        21, bold=True, fill=(255, 204, 78, 255), anchor="rm")
    _weather_text(
        draw, (124, 34), condition, 9, bold=True,
        fill=(151, 222, 255, 255), anchor="lm")
    details = (
        f'H {today["high_c"]}°  L {today["low_c"]}°'
        f'   RAIN {current["precipitation"]}%'
    )
    _weather_text(draw, (124, 51), details, 8, bold=True, anchor="lm")
    _weather_text(
        draw, (312, 51),
        f'{current["wind_direction"]} {current["wind_kmh"]}',
        8, bold=True, anchor="rm")
    image.save(target, "PNG", optimize=True)
    return target


def _weather_overlay_style(value, default="lower-third") -> str:
    style = str(value or "").strip().lower().replace("_", "-")
    aliases = {
        "bottom": "lower-third",
        "lower": "lower-third",
        "lower-third": "lower-third",
        "right": "sidebar",
        "side": "sidebar",
        "sidebar": "sidebar",
    }
    return aliases.get(style, default)


def render_weather_clip_sidebar(config, output_dir: str) -> str:
    """Render a full-frame alternate overlay with an hourly sidebar.

    The bottom 64 pixels intentionally match the regular lower third. The
    upper-right 104 pixels show three real forecast hours, leaving a
    216x176 program window. Icons come from the dimensional broadcast atlas.
    """
    from PIL import Image, ImageDraw

    forecast = weather_forecast_for_presenter(config)
    lower_third = render_weather_clip_lower_third(config, output_dir)
    if not forecast or not lower_third:
        return ""

    os.makedirs(output_dir, exist_ok=True)
    target = os.path.join(output_dir, "live-weather-sidebar-v1.png")
    forecast_mtime = _weather_forecast_path(config).stat().st_mtime
    newest = max(
        forecast_mtime,
        os.path.getmtime(lower_third),
        os.path.getmtime(__file__),
    )
    if (os.path.isfile(target) and os.path.getsize(target) > 0 and
            os.path.getmtime(target) >= newest):
        return target

    image = Image.new("RGBA", (320, 240), (0, 0, 0, 0))
    with Image.open(lower_third) as opened:
        image.alpha_composite(opened.convert("RGBA"), (0, 176))
    draw = ImageDraw.Draw(image, "RGBA")
    for x in range(216, 320):
        ratio = (x - 216) / 103
        draw.line(
            (x, 0, x, 176),
            fill=(3, int(27 + ratio * 18), int(66 + ratio * 39), 255))
    draw.rectangle((216, 0, 219, 176), fill=(47, 190, 244, 255))
    draw.rectangle((219, 0, 320, 24), fill=(4, 48, 96, 255))
    _weather_text(
        draw, (269, 12), "NEXT 3 HOURS", 9, bold=True, anchor="mm")

    current_stamp = forecast["current"].get("stamp")
    upcoming = [
        row for row in forecast.get("hourly") or []
        if current_stamp is None or row.get("stamp") > current_stamp
    ][:3]
    if not upcoming:
        upcoming = (forecast.get("hourly") or [])[:3]
    for index, row in enumerate(upcoming):
        top = 25 + index * 50
        if index:
            draw.line(
                (221, top, 316, top),
                fill=(64, 145, 205, 190), width=1)
        stamp = row.get("stamp")
        time_label = stamp.strftime("%-I %p") if stamp else "NEXT"
        _weather_text(
            draw, (223, top + 11), time_label, 8, bold=True,
            fill=(105, 214, 255, 255), anchor="lm")
        _weather_text(
            draw, (314, top + 13), f'{row["temperature_c"]}°',
            15, bold=True, fill=(255, 204, 78, 255), anchor="rm")
        group = _weather_condition_group(row.get("code", ""))
        icon_name = {
            "clear": "clear_day",
            "partly_cloudy": "partly_cloudy",
            "cloudy": "cloudy",
            "rain": "rain",
            "thunder": "thunderstorm",
            "snow": "snow",
            "fog": "fog",
            "unknown": "unknown",
        }.get(group, "unknown")
        icon_path = os.path.join(
            _weather_icons_source_dir(), f"{icon_name}.40x40x24.bmp")
        if os.path.isfile(icon_path):
            try:
                with Image.open(icon_path) as opened:
                    icon = opened.convert("RGBA").resize(
                        (24, 24), Image.Resampling.LANCZOS)
                pixels = []
                for red, green, blue, alpha in icon.getdata():
                    if red > 205 and blue > 205 and green < 100:
                        pixels.append((red, green, blue, 0))
                    else:
                        pixels.append((red, green, blue, alpha))
                icon.putdata(pixels)
                image.alpha_composite(icon, (222, top + 21))
            except (OSError, ValueError):
                pass
        _weather_text(
            draw, (249, top + 29), f'RAIN {row["precipitation"]}%',
            7, bold=True, anchor="lm")
        _weather_text(
            draw, (249, top + 42),
            f'{row["wind_direction"]} {row["wind_kmh"]} km/h',
            7, anchor="lm")

    image.save(target, "PNG", optimize=True)
    return target


def _weather_news_overlay_styles(config, news: list) -> dict:
    """Return stable per-clip styles with optional personal manifest overrides."""
    styles = {
        path: ("lower-third" if index % 2 == 0 else "sidebar")
        for index, path in enumerate(news)
    }
    news_dir = os.path.join(
        weather_assets_dir(config), "interstitials", "news")
    manifest_path = os.path.join(news_dir, "clips.json")
    try:
        with open(manifest_path, "r", encoding="utf-8") as handle:
            rules = list((json.load(handle) or {}).get("clips") or [])
    except (OSError, ValueError, AttributeError):
        rules = []
    by_name = {os.path.basename(path): path for path in news}
    for rule in rules:
        if not isinstance(rule, dict):
            continue
        path = by_name.get(str(rule.get("file") or "").strip())
        if path:
            styles[path] = _weather_overlay_style(
                rule.get("overlay_style"), styles[path])
    return styles


def _weather_rule_number(rule: dict, name: str):
    value = rule.get(name)
    if value in (None, ""):
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def _weather_conditional_rule_matches(rule: dict, forecast: dict,
                                      wall_clock: datetime) -> tuple:
    """Return whether a personally curated recorded report fits now.

    These clips never supply live numbers. They are condition-matched
    archival reports and retain the RECORDED REPORT bug in the carrier.
    """
    male_role = str(rule.get("male_role") or "").strip().lower()
    approved_focus = (
        rule.get("solo_woman") is True or
        male_role in {"introduction", "handoff"}
    )
    if not approved_focus:
        return False, (
            "not approved as a solo-woman clip or brief male introduction")

    months = {
        int(value) for value in (rule.get("months") or [])
        if str(value).isdigit()
    }
    if months and wall_clock.month not in months:
        return False, f"month {wall_clock.month} is outside {sorted(months)}"

    current = forecast.get("current") or {}
    today = forecast.get("today") or {}
    current_group = _weather_condition_group(current.get("code", ""))
    today_group = _weather_condition_group(today.get("code", ""))
    groups = {current_group, today_group}
    accepted = {
        str(value).strip().lower()
        for value in (rule.get("conditions") or [])
        if str(value).strip()
    }
    if accepted and not groups.intersection(accepted):
        return False, (
            f"conditions {sorted(groups)} do not match {sorted(accepted)}")

    current_temp = _weather_number(current.get("temperature_c"))
    high_temp = _weather_number(today.get("high_c"), current_temp)
    precipitation = max(
        _weather_number(current.get("precipitation")),
        _weather_number(today.get("precipitation")),
    )
    values = {
        "min_temp_c": current_temp,
        "max_temp_c": current_temp,
        "min_high_c": high_temp,
        "max_high_c": high_temp,
        "min_precipitation": precipitation,
        "max_precipitation": precipitation,
    }
    for name, actual in values.items():
        limit = _weather_rule_number(rule, name)
        if limit is None:
            continue
        if name.startswith("min_") and actual < limit:
            return False, f"{name} requires {limit:g}, actual {actual:g}"
        if name.startswith("max_") and actual > limit:
            return False, f"{name} allows {limit:g}, actual {actual:g}"

    return True, (
        f"{current_group}, {current_temp:g}C, high {high_temp:g}C, "
        f"precipitation {precipitation:g}%")


def select_weather_interstitials(config, now=None) -> tuple:
    """Build an accurate-first rotation with distinct Carissa fallbacks.

    The conditional library is retained on the host; weather sync merely
    adds matching clips to the active carrier set. When that set is thin,
    general Carissa anchor and field clips fill it out so an hour never
    collapses to one report. Byte-identical files are counted only once.
    """
    wall_clock = (now or datetime.now()).replace(tzinfo=None)
    forecast = weather_forecast_for_presenter(config, now=wall_clock)
    current = forecast.get("current") or {}
    group = _weather_condition_group(current.get("code", ""))

    carissa_candidates = []
    for insert_kind in weather_interstitial_kinds(
            current.get("code", ""), now=wall_clock):
        carissa_candidates.extend(_weather_asset_files(
            config, os.path.join("interstitials", insert_kind),
            LIVETV_WEATHER_VIDEO_EXTENSIONS))
    carissa, duplicate_carissa = _weather_unique_media(carissa_candidates)

    def carissa_matches(path):
        name = os.path.basename(path).lower()
        if group == "snow":
            return "snow" in name
        if group in {"rain", "thunder"}:
            return any(word in name for word in
                       ("rain", "storm", "dreary", "road-condition"))
        return not any(word in name for word in
                       ("rain", "snow", "storm", "dreary"))

    carissa_exact = [path for path in carissa if carissa_matches(path)]
    carissa_fallback = [path for path in carissa
                        if path not in carissa_exact]
    decisions = [{
        "file": path,
        "presenter": "Carissa Codel",
        "active": False,
        "weather_overlay": False,
        "overlay_style": "lower-third",
        "reason": "byte-identical duplicate omitted from rotation",
    } for path in duplicate_carissa]

    conditional_dir = os.path.join(
        weather_assets_dir(config), "interstitials", "conditional")
    manifest_path = os.path.join(conditional_dir, "clips.json")
    try:
        with open(manifest_path, "r", encoding="utf-8") as handle:
            rules = list((json.load(handle) or {}).get("clips") or [])
    except (OSError, ValueError, AttributeError):
        rules = []

    conditional_root = os.path.realpath(conditional_dir)
    matching = []
    for rule in rules:
        if not isinstance(rule, dict):
            continue
        relative = str(rule.get("file") or "").strip()
        path = os.path.realpath(os.path.join(conditional_root, relative))
        safe = (path.startswith(conditional_root + os.sep) and
                os.path.isfile(path) and
                os.path.splitext(path)[1].lower() in
                LIVETV_WEATHER_VIDEO_EXTENSIONS)
        if not safe:
            decisions.append({
                "file": relative,
                "presenter": str(rule.get("presenter") or ""),
                "active": False,
                "weather_overlay": bool(rule.get("weather_overlay")),
                "overlay_style": _weather_overlay_style(
                    rule.get("overlay_style")),
                "reason": "missing or unsafe conditional clip path",
            })
            continue
        matched, reason = _weather_conditional_rule_matches(
            rule, forecast, wall_clock)
        decisions.append({
            "file": path,
            "presenter": str(rule.get("presenter") or ""),
            "active": matched,
            "weather_overlay": bool(rule.get("weather_overlay")),
            "overlay_style": _weather_overlay_style(
                rule.get("overlay_style")),
            "reason": reason,
        })
        if matched:
            matching.append((
                -int(_weather_number(rule.get("priority"))),
                os.path.basename(path).lower(),
                path,
            ))

    matching.sort()
    accurate = carissa_exact + [
        path for _priority, _name, path in matching
    ]
    accurate, _duplicates = _weather_unique_media(accurate)
    # Three different reports is the floor, not the ceiling. If current
    # conditions produce fewer than that, fill the rotation with distinct
    # Carissa anchor/field pieces from the season-appropriate general pool.
    fallback_count = max(0, LIVETV_WEATHER_VARIANTS - len(accurate))
    active = accurate + carissa_fallback[:fallback_count]
    active, _duplicates = _weather_unique_media(active)
    for path in carissa:
        is_active = path in active
        decisions.append({
            "file": path,
            "presenter": "Carissa Codel",
            "active": is_active,
            "weather_overlay": False,
            "overlay_style": "lower-third",
            "reason": (
                "condition-matched Carissa report"
                if path in carissa_exact else
                "distinct Carissa fallback for rotation"
                if is_active else
                "fallback not needed for this forecast"
            ),
        })
    report = {
        "version": 1,
        "generated_at": wall_clock.isoformat(timespec="seconds"),
        "forecast": {
            "location": forecast.get("location", ""),
            "condition": current.get("condition", ""),
            "condition_group": group,
            "temperature_c": current.get("temperature_c"),
            "high_c": (forecast.get("today") or {}).get("high_c"),
            "precipitation": max(
                _weather_number(current.get("precipitation")),
                _weather_number(
                    (forecast.get("today") or {}).get("precipitation")),
            ),
        },
        "active_clips": active,
        "decisions": decisions,
    }
    return active, report


def _weather_icons_source_dir() -> str:
    # rockpod/services/livetv.py -> rockpod/assets/weather/icons
    return os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        "assets", "weather", "icons")


def install_weather_icons(mount_path: str) -> int:
    """Copy the committed dimensional broadcast-weather icon set.

    ``tools/generate_weather_icons.py`` downsamples one consistent CGI atlas
    to the same device path weather.c already reads (WEATHER_ICON_DIR), so
    the Weather channel and standalone app never substitute drawn symbols.
    Returns how many files were copied or updated.
    """
    source_dir = _weather_icons_source_dir()
    if not os.path.isdir(source_dir):
        return 0

    target_dir = os.path.join(os.path.abspath(mount_path), ".rockbox",
                              "rockpod", "weather", "icons")
    os.makedirs(target_dir, exist_ok=True)

    copied = 0
    for name in os.listdir(source_dir):
        if not name.endswith(".bmp"):
            continue
        source = os.path.join(source_dir, name)
        target = os.path.join(target_dir, name)
        if (os.path.isfile(target) and
                filecmp.cmp(source, target, shallow=False)):
            continue
        try:
            shutil.copy2(source, target)
            copied += 1
        except OSError:
            pass
    return copied


def _weather_ad_keys(config, ads: list, now=None) -> list:
    """Return the current season's spots plus the Weather general pool."""
    condition = weather_current_code(config)
    kinds = weather_interstitial_kinds(condition, now=now)
    matches = []
    for kind in kinds:
        marker = f"/ads/weather/{kind}/"
        matches.extend(
            item.key for item in ads
            if marker in item.path.replace("\\", "/").lower()
        )
    return sorted(set(matches))


def ensure_weather_channel(sync: "LiveTvSync", lineup: "LiveTvLineup",
                           config, shows: list, ads=None) -> list:
    """Add the Weather channel's programme media to *shows*, additive.

    Call this after LiveTvLineup.autobuild() (so autobuild never sees, and
    never tries to group, this synthetic media) and before
    LiveTvScheduler/LiveTvSync.sync() (so the returned list is what actually
    gets scheduled and copied). Returns a new list; the caller's list is
    left untouched.

    Does nothing beyond returning *shows* unchanged - leaving any
    previously-built channel out of this sync's schedule rather than
    crashing or reusing stale data - unless a forecast has actually been
    synced and at least one local music file is configured.
    """
    shows = list(shows)
    if not weather_forecast_synced(config):
        return shows

    music_files = weather_music_files(config)
    if not music_files:
        return shows

    bumpers = sync.ensure_weather_sources(music_files, config)
    if not bumpers:
        return shows

    media = [
        LiveTvMedia(
            path=source,
            kind="show",
            title="Local Forecast",
            series=LIVETV_WEATHER_CATEGORY,
            duration=LIVETV_WEATHER_BLOCK_SECONDS,
            rating="TV-G",
            description="Continuous local weather and music.",
        )
        for source in bumpers
    ]

    channel = next((c for c in lineup.channels
                     if c.category == LIVETV_WEATHER_CATEGORY), None)
    if channel is None:
        if len(lineup.channels) >= LIVETV_MAX_CHANNELS:
            logger.warning("Weather channel skipped: channel line-up is full")
            return shows + media
        channel = LiveTvChannel(
            number=lineup.next_free_number(),
            callsign=LIVETV_WEATHER_CALLSIGN,
            name="Weather",
            category=LIVETV_WEATHER_CATEGORY,
        )
        lineup.channels.append(channel)

    channel.shows = sorted({item.key for item in media})
    # A real specialty channel has its own commercial inventory. Weather
    # PSAs placed under Live/ADS/Weather/<season> are already ordinary ad
    # media, so channel-specific scheduling needs no new slot kind or player
    # behavior. If the seasonal pool is absent, an empty list intentionally
    # preserves the scheduler's existing shared-ad fallback.
    channel.ads = _weather_ad_keys(config, list(ads or []))

    # Same generic, already-existing per-channel logo mechanism every other
    # channel uses (install_logos() below) - nothing new, and nothing here
    # is ever fetched by RockPod or committed to the git tree. A user who
    # points this at a real, currently trademarked logo file on their own
    # disk is doing the same thing they could already do for any channel;
    # it reaches the device only, exactly like every other channel logo.
    logo_source = str(getattr(config, "livetv_weather_logo", "") or "").strip()
    if logo_source and os.path.isfile(logo_source):
        channel.logo = logo_source

    lineup.ensure_unique_numbers()
    lineup.channels.sort(key=lambda c: c.number)
    return shows + media


def ensure_tv_information_channels(
        lineup: "LiveTvLineup", config, shows: list, progress=None,
        force=False):
    """Refresh and add RockPod's renewable information channels.

    The service produces ordinary host-rendered programme reels.  Keeping
    them in the existing show/logo path means the guide and MPEG player need
    no network, decoder, framebuffer, or audio-lifecycle special cases.
    Returns ``(shows, refresh_result)``.
    """
    from services.tv_information import TvInformationService

    shows = list(shows)
    service = TvInformationService(config)
    result = service.refresh(progress=progress, force=force)
    media_items = []
    for current in result.media:
        channel_spec = current.channel
        media = LiveTvMedia(
            path=current.path,
            kind="show",
            title=channel_spec.title,
            series=channel_spec.name,
            duration=service.block_seconds,
            rating=channel_spec.rating,
            description=channel_spec.description,
        )
        media_items.append(media)

        channel = next((
            item for item in lineup.channels
            if item.category == channel_spec.category
        ), None)
        if channel is None:
            if len(lineup.channels) >= LIVETV_MAX_CHANNELS:
                result.warnings.append(
                    f"{channel_spec.name} was skipped because the "
                    "24-channel line-up is full.")
                media_items.pop()
                continue
            channel = LiveTvChannel(
                number=lineup.next_free_number(),
                callsign=channel_spec.callsign,
                name=channel_spec.name,
                category=channel_spec.category,
            )
            lineup.channels.append(channel)

        channel.shows = [media.key]
        # Refreshing a renewable programme must not erase a station-specific
        # commercial pool the user assigned to the channel.
        logo = result.logos.get(channel_spec.category, "")
        if logo and os.path.isfile(logo):
            channel.logo = logo

    lineup.ensure_unique_numbers()
    lineup.channels.sort(key=lambda item: item.number)
    return shows + media_items, result


def max_two_day_slot_count(slots) -> int:
    """Largest number of listings the iPod would have to hold at once."""
    per_day = {}
    for slot in slots:
        per_day[slot.day] = per_day.get(slot.day, 0) + 1
    if not per_day:
        return 0
    return max(per_day.get(day, 0) + per_day.get((day + 1) % 7, 0)
               for day in range(7))


def resolve_slot(slots, channel_number: int, when=None):
    """What is airing on a channel at ``when``? Returns ``(slot, offset)``.

    This is the same lookup the iPod performs, so the PC guide and the device
    always agree.
    """
    moment = time.localtime(when) if when is not None else time.localtime()
    day = (moment.tm_wday + 1) % 7  # Python: Monday=0; C tm_wday: Sunday=0
    secs = moment.tm_hour * 3600 + moment.tm_min * 60 + moment.tm_sec

    for slot in slots:
        if slot.channel != channel_number or slot.day != day:
            continue
        if slot.start <= secs < slot.end:
            return slot, secs - slot.start
    return None, 0


def slots_in_window(slots, channel_number: int, start_epoch: float,
                    window_seconds: int):
    """Slots overlapping a window, for drawing a guide grid row."""
    results = []
    step = 0
    while step < window_seconds:
        slot, offset = resolve_slot(slots, channel_number,
                                    start_epoch + step)
        if slot is None:
            step += 1800
            continue
        results.append((slot, start_epoch + step - offset))
        step += slot.duration - offset
        if slot.duration - offset <= 0:
            step += 1
    return results


def device_logo_path(channel) -> str:
    """Where a channel's converted logo lives on the player.

    The lineup stores the artwork the user picked on this PC, which is
    meaningless to the iPod. install_logos() renders it into the device
    logos/ folder under a name derived from the call sign, so the path
    written to channels.tsv is derived the same way rather than copied
    from the source field.
    """
    source = str(getattr(channel, "logo", "") or "").strip()
    if not source:
        return ""
    if source.startswith("logos/"):
        return source
    return f"logos/{_safe_name(channel.callsign, 'chan')}.bmp"


def write_channels_tsv(path: str, channels, logo_dir: str = ""):
    lines = [
        "# number\tcallsign\tname\tcategory\tlogo\tfavourite\tparental_locked"
    ]
    for channel in channels:
        logo = device_logo_path(channel)
        # Only advertise a logo that actually reached the device;
        # otherwise the player would try to open a file that is not
        # there and fall back to text anyway.
        if logo and logo_dir and not os.path.isfile(
                os.path.join(logo_dir, os.path.basename(logo))):
            logo = ""
        lines.append("\t".join([
            str(channel.number),
            channel.callsign or "CH",
            channel.name or channel.callsign or "Channel",
            channel.category or "Series",
            logo,
            "1" if channel.favourite else "0",
            "1" if channel.parental_locked else "0",
        ]))
    _write_text(path, "\n".join(lines) + "\n")


def write_guide_tsv(path: str, slots):
    """Grouped by channel then day then start: the device relies on it."""
    ordered = sorted(slots, key=lambda s: (s.channel, s.day, s.start))
    lines = ["# chan\tday\tstart\tdur\tkind\ttitle\trating\tdesc\tpath"
             "\tblockstart\tblockdur"]
    for slot in ordered:
        title = _first_non_empty_text(
            slot.title,
            clean_title(Path(slot.path).stem),
            "Program",
        )
        lines.append("\t".join([
            str(slot.channel),
            str(slot.day),
            str(slot.start),
            str(slot.duration),
            slot.kind,
            _tsv_clean(title),
            _tsv_clean(slot.rating) or "--",
            _tsv_clean(slot.description) or "--",
            slot.path,
            str(slot.block_start),
            str(slot.block_duration or slot.duration),
        ]))
    _write_text(path, "\n".join(lines) + "\n")


def _tsv_clean(value: str) -> str:
    return re.sub(r"\s+", " ", _clean_text(value)).strip()


def _write_text(path, content: str):
    path = os.fspath(path)
    directory = os.path.dirname(path) or "."
    os.makedirs(directory, exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as handle:
        handle.write(content)
        handle.flush()
        os.fsync(handle.fileno())
    os.replace(tmp, path)
    # The device may reset or disappear from USB immediately after a sync.
    # Persist the rename itself before publishing the dependent channels
    # table, otherwise channels.tsv can survive while guide.tsv silently
    # rolls back and leaves visible channels with no programmes.
    try:
        flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
        directory_fd = os.open(directory, flags)
        try:
            os.fsync(directory_fd)
        finally:
            os.close(directory_fd)
    except OSError:
        # Some removable filesystems do not support directory fsync. The
        # file fsync above still gives the strongest guarantee they expose.
        pass


class LiveTvYoutubeChannelSync:
    """Fetch recent full-length uploads for the built-in YouTube channels.

    Downloads are intentionally staged, not imported into the user's normal
    video library.  :class:`LiveTvSync` can therefore encode them, put them on
    the iPod and remove only the staged source once that copy is verified.
    """

    def __init__(self, library: LiveTvLibrary, config, command_runner=None):
        self._library = library
        self._config = config
        self._runner = command_runner or subprocess.run

    def _config_value(self, key, default=""):
        getter = getattr(self._config, "get", None)
        if callable(getter):
            return getter(key, default)
        return getattr(self._config, key, default)

    def _yt_dlp_bin(self):
        configured = str(self._config_value("youtube_movie_binary", "") or "")
        return configured.strip() or shutil.which("yt-dlp") or "yt-dlp"

    @staticmethod
    def _channel_spec(channel_key):
        try:
            return LIVETV_YOUTUBE_CHANNELS[str(channel_key)]
        except KeyError as error:
            raise RuntimeError(f"Unknown Live TV YouTube channel: {channel_key}") from error

    def _run(self, command, **kwargs):
        return self._runner(command, **kwargs)

    def _recent_full_episodes(self, spec):
        command = [
            self._yt_dlp_bin(), "--flat-playlist", "--dump-single-json",
            "--playlist-end", str(LIVETV_YOUTUBE_CANDIDATE_LIMIT), spec["url"],
        ]
        try:
            result = self._run(command, capture_output=True, text=True,
                               timeout=120, check=False)
        except (OSError, subprocess.SubprocessError) as error:
            raise RuntimeError(f"Could not read {spec['name']} from YouTube: {error}")
        if result.returncode != 0:
            detail = (result.stderr or result.stdout or "").strip()[-300:]
            raise RuntimeError(f"Could not read {spec['name']} from YouTube: {detail}")
        try:
            payload = json.loads(result.stdout)
        except (TypeError, ValueError) as error:
            raise RuntimeError(f"YouTube returned an invalid episode list for {spec['name']}") from error

        episodes = []
        for entry in payload.get("entries") or []:
            if not isinstance(entry, dict):
                continue
            duration = entry.get("duration") or 0
            try:
                duration = int(float(duration))
            except (TypeError, ValueError):
                duration = 0
            video_id = str(entry.get("id") or "").strip()
            if not video_id or duration < spec["minimum_duration"]:
                continue
            episodes.append({
                "id": video_id,
                "title": _clean_text(entry.get("title")) or "Episode",
                "url": f"https://www.youtube.com/watch?v={video_id}",
            })
            if len(episodes) == LIVETV_YOUTUBE_VIDEO_LIMIT:
                break
        if len(episodes) != LIVETV_YOUTUBE_VIDEO_LIMIT:
            raise RuntimeError(
                f"YouTube did not list {LIVETV_YOUTUBE_VIDEO_LIMIT} recent "
                f"full episodes for {spec['name']}."
            )
        return episodes

    def _logo_path(self, spec):
        """Create a small, stable source logo which LiveTvSync turns into BMP."""
        path = os.path.join(self._library.state_dir(),
                            f"{_safe_name(spec['callsign'])}-youtube-logo.png")
        if os.path.isfile(path) and os.path.getsize(path) > 0:
            return path
        try:
            from PIL import Image, ImageDraw
            image = Image.new("RGB", (320, 144), spec["logo_fill"])
            draw = ImageDraw.Draw(image)
            draw.rectangle((5, 5, 314, 138), outline="white", width=7)
            font = _weather_font(42 if len(spec["logo_text"]) < 5 else 31,
                                 bold=True)
            draw.text((160, 72), spec["logo_text"], font=font, fill="white",
                      anchor="mm", stroke_width=2, stroke_fill="#122549")
            image.save(path, "PNG")
        except (OSError, ImportError):
            return ""
        return path if os.path.isfile(path) else ""

    @staticmethod
    def _preferred_number(lineup, desired):
        used = {channel.number for channel in lineup.channels}
        if desired not in used:
            return desired
        number = LIVETV_FIRST_CHANNEL
        while number in used:
            number += 1
        return number

    def ensure_channel(self, lineup: LiveTvLineup, channel_key, shows):
        """Install/update the channel identity and assign its downloaded shows."""
        spec = self._channel_spec(channel_key)
        channel = next((item for item in lineup.channels
                        if item.callsign == spec["callsign"]), None)
        if channel is None:
            channel = next((item for item in lineup.channels
                            if item.name == spec["name"]), None)
        if channel is None:
            channel = LiveTvChannel(
                number=self._preferred_number(lineup, spec["number"]),
                callsign=spec["callsign"], name=spec["name"],
                category="YouTube",
            )
            lineup.channels.append(channel)
        channel.callsign = spec["callsign"]
        channel.name = spec["name"]
        channel.category = "YouTube"
        logo = self._logo_path(spec)
        if logo:
            channel.logo = logo
        channel.shows = sorted({item.key for item in shows
                                if item.series == spec["series"]})
        lineup.ensure_unique_numbers()
        lineup.channels.sort(key=lambda item: item.number)
        return channel

    def stale_device_paths(self, channel_key, previous_keys, retained_keys):
        """Return only this station's superseded iPod clip paths.

        The paths are reconstructed from the old line-up entries rather than
        from a broad directory scan, so a channel refresh can never remove a
        user's unrelated Live TV recording.
        """
        spec = self._channel_spec(channel_key)
        stale = sorted(set(previous_keys or []) - set(retained_keys or []))
        paths = []
        for key in stale:
            path, separator, segment = str(key).rpartition("#")
            if not separator:
                path, segment = str(key), ""
            item = LiveTvMedia(
                path=path, kind="show", title="Episode",
                series=spec["series"], segment=segment,
            )
            paths.append(item.device_relative())
        return paths

    def download(self, channel_key, progress=None):
        spec = self._channel_spec(channel_key)
        episodes = self._recent_full_episodes(spec)
        destination = os.path.join(self._library.staging_dir(), spec["series"])
        os.makedirs(destination, exist_ok=True)
        for index, episode in enumerate(episodes, 1):
            if progress:
                progress(index, len(episodes), f"Downloading {spec['name']}: {episode['title']}")
            command = [
                self._yt_dlp_bin(), "--no-playlist", "--no-progress",
                "-f", "bv*[height<=480]+ba/b[height<=480]",
                "--merge-output-format", "mp4",
                "-o", os.path.join(destination, "%(title)s [%(id)s].%(ext)s"),
                episode["url"],
            ]
            try:
                result = self._run(command, capture_output=True, text=True,
                                   timeout=3600, check=False)
            except (OSError, subprocess.SubprocessError) as error:
                raise RuntimeError(f"Could not download {episode['title']}: {error}")
            if result.returncode != 0:
                detail = (result.stderr or result.stdout or "").strip()[-300:]
                raise RuntimeError(f"Could not download {episode['title']}: {detail}")
        return episodes


class LiveTvSync:
    """Transcodes to MPEG and copies the whole line-up onto an iPod."""

    def __init__(self, library: LiveTvLibrary):
        self._library = library

    def device_root(self, mount_path: str) -> str:
        return os.path.join(os.path.abspath(mount_path), LIVETV_DEVICE_DIR)

    # -- reconciliation ---------------------------------------------------

    def cached_stand_in_for(self, key: str, kind: str):
        """Rebuild a schedulable item for a lineup entry whose source file
        is no longer on this PC, from the copy a previous sync already
        cached. This is what makes it safe to delete a source file from
        ~/Videos/Live once it has reached the iPod: the schedule no longer
        depends on the file still being there.
        """
        if "#" in key:
            path, segment = key.rsplit("#", 1)
        else:
            path, segment = key, ""

        roots = self._library.source_roots()
        root = next((r for r in roots if path.startswith(r + os.sep)),
                    os.path.dirname(path))

        if kind == "ad":
            series = "Commercials"
            rating = ""
            title = clean_title(Path(path).stem)
            description = ""
        else:
            series = series_name(path, root)
            rating = "TV-PG"
            title = program_title(path, series)
            description = program_description(path, series)

        item = LiveTvMedia(path=path, kind=kind, title=title, series=series,
                           rating=rating, description=description,
                           segment=segment)
        self._library._apply_override(item)

        if segment:
            for entry in self._library.edits_for(path):
                if str(entry.get("id")) == segment:
                    if entry.get("title"):
                        item.title = entry["title"]
                    break
            segment_override = self._library.override_for(item.key)
            if segment_override.get("title"):
                item.title = segment_override["title"]
            if segment_override.get("description"):
                item.description = segment_override["description"]

        cached = self.cached_mpeg_path(item)
        if not (os.path.isfile(cached) and os.path.getsize(cached) > 0):
            return None

        duration = self._library._probe_duration(cached)
        if duration <= 0:
            return None
        item.duration = duration
        return item

    def reconcile_missing_sources(self, lineup: LiveTvLineup, shows, ads):
        """Add back lineup entries whose source file has been deleted
        locally but which are already cached from a previous sync.

        Additive only: shows/ads still on disk are untouched, and a path
        that has neither a local file nor a cached copy is simply dropped,
        the way it always was.
        """
        shows = list(shows)
        ads = list(ads)
        known = {item.key for item in shows} | {item.key for item in ads}

        for channel in lineup.channels:
            for key in channel.shows:
                if key in known:
                    continue
                stand_in = self.cached_stand_in_for(key, "show")
                if stand_in is not None:
                    shows.append(stand_in)
                    known.add(key)
            for key in channel.ads:
                if key in known:
                    continue
                stand_in = self.cached_stand_in_for(key, "ad")
                if stand_in is not None:
                    ads.append(stand_in)
                    known.add(key)

        return shows, ads

    # -- transcode -------------------------------------------------------

    def cached_mpeg_path(self, item: LiveTvMedia) -> str:
        # The profile is part of the cache path, so changing how videos are
        # framed re-converts everything instead of silently reusing clips
        # encoded the old way.
        return os.path.join(self._library.cache_dir(), LIVETV_MPEG_PROFILE,
                            item.device_relative())

    def prune_stale_cache(self) -> int:
        """Delete clips encoded with a superseded profile.

        Without this, changing how videos are framed would leave the old
        letterboxed copies on disk forever.
        """
        cache = self._library.cache_dir()
        removed = 0
        try:
            entries = sorted(os.listdir(cache))
        except OSError:
            return 0

        for name in entries:
            path = os.path.join(cache, name)
            if not os.path.isdir(path) or name == LIVETV_MPEG_PROFILE:
                continue
            try:
                shutil.rmtree(path)
                removed += 1
                logger.info("Removed stale Live TV encode cache: %s", name)
            except OSError:
                pass
        return removed

    def prune_orphaned_cache(self, keep_relatives) -> int:
        """Delete cached clips no longer needed by the current schedule.

        A show or ad can leave the schedule by being unassigned, deleted
        from ~/Videos/Live with no channel still referencing it, or
        renamed/re-foldered so its series (and so its cache path) changed.
        Safe to run unconditionally: reconcile_missing_sources() has
        already re-added anything still scheduled whose source file is
        merely missing locally, so whatever is left really is unused.
        """
        keep = set(keep_relatives)
        cache_root = os.path.join(self._library.cache_dir(),
                                  LIVETV_MPEG_PROFILE)
        removed = 0
        for dirpath, _dirnames, filenames in os.walk(cache_root,
                                                      topdown=False):
            for name in filenames:
                if not name.endswith(".mpg"):
                    continue
                full = os.path.join(dirpath, name)
                relative = os.path.relpath(full, cache_root)
                if relative not in keep:
                    try:
                        os.remove(full)
                        removed += 1
                    except OSError:
                        pass
            try:
                if dirpath != cache_root and not os.listdir(dirpath):
                    os.rmdir(dirpath)
            except OSError:
                pass
        return removed

    # -- weather channel ---------------------------------------------------

    def weather_state_dir(self) -> str:
        path = os.path.join(self._library.state_dir(), "weather")
        os.makedirs(path, exist_ok=True)
        return path

    def ensure_weather_sources(self, music_files, config=None) -> list:
        """Build hour-long Weather carriers with a 120-second broadcast flow.

        The fixed video clock alternates rendered forecast panels with two
        presenter IDs and a sentence-complete report up to 70 seconds long.
        The remainder of the 72-second report phase is a forecast-aware
        station slate, never a frozen presenter or an arbitrary speech cut.
        Once near the middle of each hour, a separate Carissa viewer-comment
        insert replaces that report as a format break. Every insert is scaled
        up and centre-cropped to 320x240, so portrait, widescreen, and
        split-source footage always fills the iPod display. Forecast panels
        and reports share the same decoded video timeline so native overlays
        cannot cover speech on hardware. Audio follows the normal mpegplayer
        lifecycle.

        Returns the list of intermediate source paths built (or reused from
        a previous sync when none of its inputs changed), skipping any
        variant ffmpeg fails to produce.
        """
        if not music_files:
            return []

        presenters = (
            render_weather_presenters(
                config, os.path.join(self.weather_state_dir(), "presenters"))
            if config is not None else [])
        channel_bug = render_weather_channel_bug(
            os.path.join(self.weather_state_dir(), "transitions"))
        comment_bug = render_weather_channel_bug(
            os.path.join(self.weather_state_dir(), "transitions"),
            label="VIEWER COMMENTS",
            filename="wx-102-viewer-comments-v2.png")
        news_bug = render_weather_channel_bug(
            os.path.join(self.weather_state_dir(), "transitions"),
            label="NEWS BREAK",
            filename="wx-102-news-break-v2.png")
        weather_overlay_art = {}
        if config is not None:
            overlay_dir = os.path.join(
                self.weather_state_dir(), "transitions")
            weather_overlay_art = {
                "lower-third": render_weather_clip_lower_third(
                    config, overlay_dir),
                "sidebar": render_weather_clip_sidebar(
                    config, overlay_dir),
            }
        transition_slates = (
            render_weather_transition_slates(
                config, os.path.join(self.weather_state_dir(), "transitions"))
            if config is not None else ())
        forecast_outro = transition_slates[1] if len(
            transition_slates) >= 2 else ""
        inserts = []
        comments = []
        news = []
        weather_overlay_inserts = set()
        weather_overlay_styles = {}
        news_overlay_styles = {}
        selection_report = {}
        if config is not None:
            inserts, selection_report = select_weather_interstitials(config)
            audible = [
                path for path in inserts
                if _weather_media_has_audio(
                    path, self._library.ffprobe_bin())
            ]
            silent = [path for path in inserts if path not in audible]
            if silent:
                selection_report["skipped_silent_clips"] = silent
                selection_report["active_clips"] = audible
                logger.warning(
                    "Weather rotation skipped %d clip(s) with no audio",
                    len(silent))
            natural = [
                path for path in audible
                if (LIVETV_WEATHER_MIN_NATURAL_REPORT_SECONDS <=
                    self._library.duration_for(path) <=
                    LIVETV_WEATHER_MAX_REPORT_SECONDS)
            ]
            short = [
                path for path in audible
                if self._library.duration_for(path) <
                LIVETV_WEATHER_MIN_NATURAL_REPORT_SECONDS
            ]
            oversized = [
                path for path in audible
                if self._library.duration_for(path) >
                LIVETV_WEATHER_MAX_REPORT_SECONDS
            ]
            if short:
                selection_report["skipped_short_clips"] = short
                selection_report["active_clips"] = natural
                for decision in selection_report.get("decisions") or []:
                    if decision.get("file") in short:
                        decision["active"] = False
                        decision["reason"] = (
                            "omitted: shorter than the complete-report floor")
                logger.warning(
                    "Weather rotation skipped %d unnaturally short clip(s)",
                    len(short))
            if oversized:
                selection_report["skipped_oversized_clips"] = oversized
                selection_report["active_clips"] = natural
                for decision in selection_report.get("decisions") or []:
                    if decision.get("file") in oversized:
                        decision["active"] = False
                        decision["reason"] = (
                            "omitted: too long to air complete in one clock")
                logger.warning(
                    "Weather rotation skipped %d clip(s) too long for a "
                    "complete clock", len(oversized))
            inserts = natural
            comments = _weather_asset_files(
                config, os.path.join("interstitials", "comments"),
                LIVETV_WEATHER_VIDEO_EXTENSIONS)
            comments = [
                path for path in comments
                if (_weather_media_has_audio(
                    path, self._library.ffprobe_bin()) and
                    LIVETV_WEATHER_MIN_NATURAL_REPORT_SECONDS <=
                    self._library.duration_for(path) <=
                    LIVETV_WEATHER_MAX_REPORT_SECONDS)
            ]
            news = _weather_asset_files(
                config, os.path.join("interstitials", "news"),
                LIVETV_WEATHER_VIDEO_EXTENSIONS)
            news = [
                path for path in news
                if (_weather_media_has_audio(
                    path, self._library.ffprobe_bin()) and
                    LIVETV_WEATHER_MIN_NEWS_BREAK_SECONDS <=
                    self._library.duration_for(path) <=
                    LIVETV_WEATHER_MAX_NEWS_BREAK_SECONDS)
            ]
            news_overlay_styles = _weather_news_overlay_styles(config, news)
            weather_overlay_inserts = {
                decision.get("file")
                for decision in selection_report.get("decisions") or []
                if (decision.get("active") and
                    decision.get("weather_overlay") and
                    decision.get("file") in inserts)
            }
            weather_overlay_styles = {
                decision.get("file"): _weather_overlay_style(
                    decision.get("overlay_style"))
                for decision in selection_report.get("decisions") or []
                if (decision.get("active") and
                    decision.get("weather_overlay") and
                    decision.get("file") in inserts)
            }
            selection_report["active_news_clips"] = news
            selection_report["news_overlay_styles"] = news_overlay_styles
            selection_report["weather_overlay_clips"] = sorted(
                weather_overlay_inserts)
            selection_report["weather_overlay_styles"] = (
                weather_overlay_styles)
            report_path = os.path.join(
                self.weather_state_dir(), "active-clips.json")
            try:
                with open(report_path, "w", encoding="utf-8") as handle:
                    json.dump(selection_report, handle, indent=2,
                              sort_keys=True)
                    handle.write("\n")
            except OSError:
                logger.warning("Could not write active Weather clip report")

        # The feature degrades to the original continuous panel carrier until
        # a complete presenter/video asset set has been supplied.
        # A thin forecast-report pool must not disable otherwise valid news
        # and viewer-comment breaks. The three report clocks fall back to
        # panel service when every candidate misses the natural-report floor.
        broadcast_assets = (
            len(presenters) >= 2 and bool(forecast_outro))
        broadcast_comments = broadcast_assets and bool(comments)
        broadcast_news = broadcast_assets and bool(news)
        newest_input = 0.0
        # Carrier construction is generated code: a new audio/transition
        # recipe must invalidate an older carrier even when its media assets
        # are unchanged.
        input_paths = list(music_files) + [os.path.abspath(__file__)]
        if broadcast_assets:
            input_paths.extend(presenters)
            input_paths.extend(inserts)
            input_paths.append(channel_bug)
            input_paths.append(forecast_outro)
        if broadcast_comments:
            input_paths.extend(comments)
            input_paths.append(comment_bug)
        if broadcast_news:
            input_paths.extend(news)
            input_paths.append(news_bug)
        if broadcast_news or weather_overlay_inserts:
            input_paths.extend(
                path for path in weather_overlay_art.values() if path)
        if config is not None:
            news_manifest = os.path.join(
                weather_assets_dir(config), "interstitials", "news",
                "clips.json")
            if os.path.isfile(news_manifest):
                input_paths.append(news_manifest)
        if config is not None:
            from services.weather import WEATHER_REL_DIR
            input_paths.append(str(
                Path(str(getattr(config, "cache_dir", "") or "")) /
                "weather" / WEATHER_REL_DIR / "forecast.tsv"))
        for path in input_paths:
            try:
                newest_input = max(newest_input, os.path.getmtime(path))
            except OSError:
                pass

        out_dir = self.weather_state_dir()
        results = []
        for index in range(LIVETV_WEATHER_VARIANTS):
            target = os.path.join(
                out_dir, f"{LIVETV_WEATHER_FLOW_PROFILE}-{index}.mkv")
            if (os.path.isfile(target) and os.path.getsize(target) > 0 and
                    os.path.getmtime(target) >= newest_input):
                results.append(target)
                continue

            offset = index % len(music_files)
            rotated = music_files[offset:] + music_files[:offset]

            # Repeat the rotation until it covers the block outright,
            # rather than looping it at playback time with ffmpeg's
            # "-stream_loop -1" on a concat demuxer: that construct
            # reopens the source files on every lap, and empirically
            # fails partway through ("Operation not permitted") once a
            # short track's file needs reopening often enough to fill an
            # hour. A single, sufficiently long concat pass has no
            # reopening to fail.
            durations = [self._library.duration_for(path)
                        for path in rotated]
            total = sum(durations)
            if total <= 0:
                continue
            playlist = list(rotated)
            covered = total
            while covered < LIVETV_WEATHER_BLOCK_SECONDS:
                playlist.extend(rotated)
                covered += total

            listing = os.path.join(out_dir, f"variant-{index}.txt")
            try:
                with open(listing, "w", encoding="utf-8") as handle:
                    for path in playlist:
                        escaped = path.replace("'", "'\\''")
                        handle.write(f"file '{escaped}'\n")
                        handle.write(
                            f"duration {self._library.duration_for(path)}\n")
            except OSError:
                continue

            music_bed = ""
            music_bed_samples = 0
            if broadcast_assets:
                music_bed = os.path.join(
                    out_dir,
                    f"{LIVETV_WEATHER_FLOW_PROFILE}-music-bed-"
                    f"{index}.mka")
                music_bed_tmp = music_bed + ".tmp"
                music_filters = []
                music_labels = []
                music_command = [
                    self._library.ffmpeg_bin(), "-y", "-loglevel", "error",
                ]
                for music_index, source in enumerate(rotated):
                    music_command.extend(["-i", source])
                    label = f"music{music_index}"
                    music_filters.append(
                        f"[{music_index}:a]aformat=sample_rates=44100:"
                        "channel_layouts=stereo,asetpts=PTS-STARTPTS"
                        f"[{label}]")
                    music_labels.append(f"[{label}]")
                music_filters.append(
                    f"{''.join(music_labels)}concat=n={len(music_labels)}:"
                    "v=0:a=1,asetpts=N/SR/TB[outa]")
                music_command.extend([
                    "-filter_complex", ";".join(music_filters),
                    "-map", "[outa]",
                    "-c:a", "aac", "-b:a", "160k",
                    "-f", "matroska", music_bed_tmp,
                ])
                try:
                    os.remove(music_bed_tmp)
                except FileNotFoundError:
                    pass
                music_result = subprocess.run(
                    music_command, capture_output=True, text=True,
                    check=False)
                if (music_result.returncode != 0 or
                        not os.path.isfile(music_bed_tmp) or
                        os.path.getsize(music_bed_tmp) <= 0):
                    try:
                        os.remove(music_bed_tmp)
                    except FileNotFoundError:
                        pass
                    logger.warning(
                        "Weather music bed encode failed: %s",
                        music_result.stderr.strip())
                    continue
                os.replace(music_bed_tmp, music_bed)
                music_bed_samples = max(
                    1, int(round(
                        self._library.duration_for(music_bed) * 44100)))

            regular_clocks = []
            panel_clock = ""
            comment_clock = ""
            news_clocks = []
            clock_durations = {}
            if broadcast_assets:
                panel_clock = os.path.join(
                    out_dir,
                    f"{LIVETV_WEATHER_FLOW_PROFILE}-panel-clock-"
                    f"{index}.mkv")
                panel_tmp = panel_clock + ".tmp"
                panel_sources = (
                    presenters[index % len(presenters):] +
                    presenters[:index % len(presenters)]
                )[:12]
                radar_count = sum(
                    "local-radar-frame-" in os.path.basename(path)
                    for path in panel_sources)
                regular_count = len(panel_sources) - radar_count
                radar_hold = 3.0
                regular_hold = (
                    (120.0 - radar_count * radar_hold) / regular_count
                    if regular_count else 12.0)
                panel_holds = [
                    radar_hold
                    if "local-radar-frame-" in os.path.basename(path)
                    else regular_hold
                    for path in panel_sources
                ]
                panel_filters = [
                    f"[{slot}:v]scale=320:240:"
                    "force_original_aspect_ratio=increase,crop=320:240,"
                    "fps=20,setsar=1,format=yuv420p,"
                    f"trim=duration={panel_holds[slot] + 1.0},"
                    f"setpts=PTS-STARTPTS[p{slot}]"
                    for slot in range(len(panel_sources))
                ]
                previous = "p0"
                panel_offset = panel_holds[0]
                for slot in range(1, len(panel_sources)):
                    output = f"px{slot}"
                    previous_path = panel_sources[slot - 1]
                    current_path = panel_sources[slot]
                    if (
                        "local-radar-frame-" in
                        os.path.basename(previous_path) or
                        "local-radar-frame-" in os.path.basename(current_path)
                    ):
                        transition = "fade"
                    else:
                        transition = (
                            "fade", "wipeleft", "smoothleft", "slideup"
                        )[(slot + index) % 4]
                    panel_filters.append(
                        f"[{previous}][p{slot}]xfade="
                        f"transition={transition}:"
                        f"duration=1:offset={panel_offset}[{output}]")
                    previous = output
                    panel_offset += panel_holds[slot]
                panel_filters.extend([
                    f"[{previous}]trim=duration=120,"
                    "setpts=PTS-STARTPTS[outv]",
                    "anullsrc=r=44100:cl=stereo,"
                    "atrim=duration=120[outa]",
                ])
                panel_graph = ";".join(panel_filters)
                panel_command = [
                    self._library.ffmpeg_bin(), "-y", "-loglevel", "error",
                ]
                for source, hold in zip(panel_sources, panel_holds):
                    panel_command.extend([
                        "-loop", "1", "-t", str(hold + 1.0), "-i", source,
                    ])
                panel_command.extend([
                    "-filter_complex", panel_graph,
                    "-map", "[outv]", "-map", "[outa]",
                    "-c:v", "libx264", "-preset", "ultrafast",
                    "-crf", "22", "-bf", "0", "-g", "12",
                    "-sc_threshold", "0",
                    "-c:a", "aac", "-b:a", "128k",
                    "-f", "matroska", panel_tmp,
                ])
                try:
                    os.remove(panel_tmp)
                except FileNotFoundError:
                    pass
                panel_result = subprocess.run(
                    panel_command, capture_output=True, text=True,
                    check=False)
                if (panel_result.returncode != 0 or
                        not os.path.isfile(panel_tmp) or
                        os.path.getsize(panel_tmp) <= 0):
                    try:
                        os.remove(panel_tmp)
                    except FileNotFoundError:
                        pass
                    logger.warning(
                        "Weather panel clock encode failed: %s",
                        panel_result.stderr.strip())
                    continue
                os.replace(panel_tmp, panel_clock)

                clock_specs = []
                for clip_index, insert in enumerate(inserts):
                    clock_target = os.path.join(
                        out_dir,
                        f"{LIVETV_WEATHER_FLOW_PROFILE}-clock-"
                        f"{index}-{clip_index}.mkv")
                    clock_specs.append((
                        clock_target, insert, channel_bug, clip_index,
                        weather_overlay_styles.get(insert, ""),
                        LIVETV_WEATHER_CLOCK_SECONDS))
                    regular_clocks.append(clock_target)
                    clock_durations[clock_target] = (
                        LIVETV_WEATHER_CLOCK_SECONDS)
                if broadcast_comments:
                    comment_clock = os.path.join(
                        out_dir,
                        f"{LIVETV_WEATHER_FLOW_PROFILE}-comment-clock-"
                        f"{index}.mkv")
                    clock_specs.append((
                        comment_clock,
                        comments[index % len(comments)],
                        comment_bug,
                        len(inserts),
                        "",
                        LIVETV_WEATHER_CLOCK_SECONDS,
                    ))
                    clock_durations[comment_clock] = (
                        LIVETV_WEATHER_CLOCK_SECONDS)
                if broadcast_news:
                    for news_index, news_insert in enumerate(news):
                        news_clock = os.path.join(
                            out_dir,
                            f"{LIVETV_WEATHER_FLOW_PROFILE}-news-clock-"
                            f"{index}-{news_index}.mkv")
                        news_clocks.append(news_clock)
                        news_duration = self._library.duration_for(
                            news_insert)
                        news_clock_seconds = max(
                            LIVETV_WEATHER_CLOCK_SECONDS,
                            int(math.ceil(
                                (news_duration + 8.0) /
                                LIVETV_WEATHER_CLOCK_SECONDS)) *
                            LIVETV_WEATHER_CLOCK_SECONDS,
                        )
                        clock_specs.append((
                            news_clock, news_insert, news_bug,
                            len(inserts) + 1 + news_index,
                            news_overlay_styles.get(
                                news_insert, "lower-third"),
                            news_clock_seconds,
                        ))
                        clock_durations[news_clock] = news_clock_seconds
                clocks_ready = True
                for (clock_target, clock_insert, clock_bug,
                     _clip_index, overlay_style,
                     clock_seconds) in clock_specs:
                    report_seconds = max(
                        1.0, self._library.duration_for(clock_insert))
                    # Long, sentence-complete station uploads get the space
                    # they need.  A four-second station lead-in and outro is
                    # enough at the 112-second ceiling; never preserve a
                    # fixed 20-second lead-in by trimming speech at 120.
                    pre_seconds = max(
                        4.0,
                        min(
                            float(LIVETV_WEATHER_REPORT_START),
                            clock_seconds - report_seconds - 4.0))
                    post_seconds = max(
                        0.0, clock_seconds - pre_seconds - report_seconds)
                    slate_seconds = min(3.0, post_seconds)
                    panel_seconds = max(0.0, post_seconds - slate_seconds)
                    overlay_art = weather_overlay_art.get(overlay_style, "")
                    if overlay_style == "sidebar" and overlay_art:
                        report_video = (
                            "[1:v]scale=216:162:"
                            "force_original_aspect_ratio=decrease,"
                            "pad=216:176:0:(176-ih)/2:color=0x031637,"
                            "fps=20,setsar=1,format=yuv420p,"
                            f"trim=duration={report_seconds},"
                            "setpts=PTS-STARTPTS,"
                            "pad=320:240:0:0:color=0x031637"
                            "[vreportbase];"
                            "[5:v]format=rgba[weatherchrome];"
                            "[vreportbase][weatherchrome]"
                            "overlay=0:0:shortest=1:format=auto"
                            "[vreportweather];"
                        )
                        bug_x = "216-w-5"
                        report_base = "vreportweather"
                    elif overlay_style == "lower-third" and overlay_art:
                        report_video = (
                            "[1:v]scale=320:176:"
                            "force_original_aspect_ratio=increase,"
                            "crop=320:176,fps=20,setsar=1,"
                            "format=yuv420p,"
                            f"trim=duration={report_seconds},"
                            "setpts=PTS-STARTPTS,"
                            "pad=320:240:0:0:color=0x031637"
                            "[vreportbase];"
                            "[5:v]format=rgba[weatherbar];"
                            "[vreportbase][weatherbar]"
                            "overlay=0:176:shortest=1:format=auto"
                            "[vreportweather];"
                        )
                        bug_x = "W-w-5"
                        report_base = "vreportweather"
                    else:
                        report_video = (
                            "[1:v]scale=320:240:"
                            "force_original_aspect_ratio=increase,"
                            "crop=320:240,fps=20,setsar=1,"
                            "format=yuv420p,"
                            f"trim=duration={report_seconds},"
                            "setpts=PTS-STARTPTS[vreportbase];"
                        )
                        bug_x = "W-w-5"
                        report_base = "vreportbase"
                    filter_graph = (
                        "[0:v]scale=320:240:"
                        "force_original_aspect_ratio=increase,"
                        "crop=320:240,fps=20,setsar=1,format=yuv420p,"
                        f"trim=duration={pre_seconds},"
                        "setpts=PTS-STARTPTS[v0];"
                        + report_video +
                        "[2:v]format=rgba[bug];"
                        f"[{report_base}][bug]overlay={bug_x}:5:"
                        "shortest=1:format=auto,"
                        "format=yuv420p[vreport];"
                        "[3:v]scale=320:240:"
                        "force_original_aspect_ratio=increase,"
                        "crop=320:240,fps=20,setsar=1,format=yuv420p,"
                        f"trim=duration={slate_seconds},"
                        "setpts=PTS-STARTPTS[vslate];"
                        "[4:v]scale=320:240:"
                        "force_original_aspect_ratio=increase,"
                        "crop=320:240,fps=20,setsar=1,format=yuv420p,"
                        f"trim=duration={panel_seconds},"
                        "setpts=PTS-STARTPTS[vpost];"
                        "[v0][vreport][vslate][vpost]"
                        "concat=n=4:v=1:a=0[vclock];"
                        "[vclock]tpad=stop_mode=clone:stop_duration=0.2,"
                        f"trim=duration={clock_seconds},"
                        "setpts=PTS-STARTPTS[outv];"
                        "[1:a]aformat=sample_rates=44100:"
                        "channel_layouts=stereo,"
                        f"atrim=duration={report_seconds},"
                        "asetpts=PTS-STARTPTS,"
                        "afade=t=in:st=0:d=0.03[areport];"
                        "anullsrc=r=44100:cl=stereo,"
                        f"atrim=duration={pre_seconds}[apre];"
                        "anullsrc=r=44100:cl=stereo,"
                        f"atrim=duration={post_seconds}[apost];"
                        "[apre][areport][apost]"
                        "concat=n=3:v=0:a=1[outa]"
                    )
                    clock_tmp = clock_target + ".tmp"
                    try:
                        os.remove(clock_tmp)
                    except FileNotFoundError:
                        pass
                    clock_command = [
                        self._library.ffmpeg_bin(), "-y", "-loglevel",
                        "error",
                        "-i", panel_clock,
                        "-i", clock_insert,
                        "-loop", "1", "-t", str(report_seconds),
                        "-i", clock_bug,
                        "-loop", "1", "-t", str(slate_seconds),
                        "-i", forecast_outro,
                        "-i", panel_clock,
                    ]
                    if overlay_art:
                        clock_command.extend([
                            "-loop", "1", "-t", str(report_seconds),
                            "-i", overlay_art,
                        ])
                    clock_command.extend([
                        "-filter_complex", filter_graph,
                        "-map", "[outv]", "-map", "[outa]", "-t",
                        str(clock_seconds),
                        "-c:v", "libx264", "-preset", "ultrafast",
                        "-crf", "22", "-bf", "0", "-g", "12",
                        "-sc_threshold", "0",
                        "-c:a", "aac", "-b:a", "128k",
                        "-f", "matroska", clock_tmp,
                    ])
                    try:
                        clock_result = subprocess.run(
                            clock_command, capture_output=True, text=True,
                            check=False)
                    except OSError:
                        clocks_ready = False
                        break
                    if (clock_result.returncode != 0 or
                            not os.path.isfile(clock_tmp) or
                            os.path.getsize(clock_tmp) <= 0):
                        try:
                            os.remove(clock_tmp)
                        except FileNotFoundError:
                            pass
                        logger.warning(
                            "Weather clock encode failed: %s",
                            clock_result.stderr.strip())
                        clocks_ready = False
                        break
                    os.replace(clock_tmp, clock_target)
                if not clocks_ready:
                    continue

            clock_listing = ""
            hour_clock_paths = []
            if broadcast_assets:
                clock_listing = os.path.join(
                    out_dir, f"hour-clock-{index}.txt")
                try:
                    with open(
                            clock_listing, "w",
                            encoding="utf-8") as handle:
                        # One forecast report every twenty minutes lets a
                        # three-clip condition match air each woman once per
                        # hour. Four separate NEWS BREAK clocks and the
                        # viewer-comment clock retain a lively channel pace
                        # without repeating the forecast itself.
                        report_cycles = (2, 12, 22)
                        news_cycles = (5, 10, 17, 21, 25)
                        variant_news_slots = []
                        if news_clocks:
                            variant_news_slots = [
                                (
                                    index * len(news_cycles) + position
                                ) % len(news_clocks)
                                for position in range(len(news_cycles))
                            ]
                            # Multi-block features air last so their complete
                            # natural ending cannot jump over a later break.
                            variant_news_slots.sort(
                                key=lambda slot: clock_durations.get(
                                    news_clocks[slot],
                                    LIVETV_WEATHER_CLOCK_SECONDS) >
                                LIVETV_WEATHER_CLOCK_SECONDS)
                        cycle = 0
                        while cycle < 30:
                            if comment_clock and cycle == 15:
                                path = comment_clock
                            elif news_clocks and cycle in news_cycles:
                                news_slot = variant_news_slots[
                                    news_cycles.index(cycle)]
                                path = news_clocks[news_slot]
                            elif cycle in report_cycles and regular_clocks:
                                path = regular_clocks[
                                    (cycle + index) % len(regular_clocks)]
                            else:
                                path = panel_clock
                            blocks = max(
                                1,
                                int(clock_durations.get(
                                    path,
                                    LIVETV_WEATHER_CLOCK_SECONDS) /
                                    LIVETV_WEATHER_CLOCK_SECONDS),
                            )
                            if cycle + blocks > 30:
                                path = panel_clock
                                blocks = 1
                            hour_clock_paths.append(path)
                            escaped = path.replace("'", "'\\''")
                            handle.write(f"file '{escaped}'\n")
                            handle.write(
                                f"duration {blocks * LIVETV_WEATHER_CLOCK_SECONDS}\n")
                            cycle += blocks
                except OSError:
                    continue

            tmp = target + ".tmp"
            try:
                os.remove(tmp)
            except FileNotFoundError:
                pass

            command = [self._library.ffmpeg_bin(), "-y", "-loglevel", "error"]
            if broadcast_assets:
                command.extend([
                    "-safe", "0", "-f", "concat", "-i", clock_listing,
                    "-i", music_bed,
                ])
                for path in hour_clock_paths:
                    command.extend(["-i", path])
            else:
                command.extend([
                    "-f", "lavfi", "-i",
                    "color=c=0x094871:s=320x240:d=%d" %
                    LIVETV_WEATHER_BLOCK_SECONDS,
                ])
                command.extend([
                    "-safe", "0", "-f", "concat", "-i", listing,
                ])
            command.extend([
                "-t", str(LIVETV_WEATHER_BLOCK_SECONDS),
            ])
            if broadcast_assets:
                report_inputs = "".join(
                    f"[{audio_index}:a]"
                    for audio_index in range(2, 2 + len(hour_clock_paths))
                )
                audio_mix = (
                    f"{report_inputs}concat=n={len(hour_clock_paths)}:"
                    "v=0:a=1,apad=whole_dur=3600,"
                    "atrim=duration=3600,asetpts=N/SR/TB[program];"
                    "[1:a]aformat=sample_rates=44100:"
                    "channel_layouts=stereo,"
                    f"aloop=loop=-1:size={music_bed_samples},"
                    "atrim=duration=3600,asetpts=N/SR/TB,"
                    "afade=t=in:st=0:d=2,volume=0.92[music];"
                    "[program]aformat=sample_rates=44100:"
                    "channel_layouts=stereo,asetpts=N/SR/TB,"
                    "asplit=2[reportbase][keybase];"
                    "[keybase]atrim=start=0.35,asetpts=N/SR/TB,"
                    "apad=pad_dur=0.35[key];"
                    "[music][key]sidechaincompress="
                    "threshold=0.008:ratio=20:attack=20:release=900:"
                    "makeup=1[ducked];"
                    "[reportbase]volume=1.25[report];"
                    "[ducked][report]amix=inputs=2:"
                    "duration=longest:normalize=0,"
                    "alimiter=limit=0.95[aout]"
                )
                command.extend([
                    "-filter_complex", audio_mix,
                    "-map", "0:v", "-map", "[aout]",
                ])
            else:
                command.extend(["-map", "0:v", "-map", "1:a"])
            if broadcast_assets:
                # Each clock is already normalized, low-delay H.264. Packet
                # copy keeps the hour render fast; ensure_mpeg() performs
                # the device MPEG-2 encode later.
                command.extend(["-c:v", "copy"])
            else:
                command.extend([
                    "-c:v", "libx264", "-preset", "ultrafast", "-crf", "28",
                ])
            command.extend([
                "-c:a", "aac", "-b:a", "160k",
                # Explicit, since the temp path ends in ".tmp" rather than
                # ".mkv" (avoids clobbering the target until the encode
                # succeeds), and ffmpeg cannot infer a muxer from that.
                "-f", "matroska",
                tmp,
            ])
            try:
                result = subprocess.run(command, capture_output=True,
                                        text=True, check=False)
            except OSError:
                continue
            if result.returncode != 0 or not os.path.isfile(tmp) or \
                    os.path.getsize(tmp) <= 0:
                try:
                    os.remove(tmp)
                except FileNotFoundError:
                    pass
                continue

            os.replace(tmp, target)
            results.append(target)

        return results

    @staticmethod
    def video_filter() -> str:
        """Fill the iPod's 320x240 screen, the way a 4:3 set did.

        Fitting widescreen inside 4:3 leaves black bars and a small picture.
        A television of this era centre-cut it instead, so scale until the
        screen is covered and crop the overhang. ``scale=iw*sar:ih`` first
        squares the pixels, which matters for anamorphic 720x480 rips.
        """
        return (
            "scale=iw*sar:ih,"
            "scale=320:240:force_original_aspect_ratio=increase,"
            "crop=320:240,setsar=1,fps=20"
        )

    @classmethod
    def edit_filter_complex(cls, ranges):
        """Filter graph that keeps only the given ranges, in order.

        Splitting a recording and cutting sections out of it are the same
        operation: keep some ranges and drop the rest. Everything is
        re-encoded anyway, so trimming in the filter graph costs nothing
        extra and is frame accurate, unlike seeking with -ss on a copy.
        """
        parts = []
        labels = []
        for index, (begin, finish) in enumerate(ranges):
            window = f"start={begin}"
            if finish:
                window += f":end={finish}"
            parts.append(
                f"[0:v]trim={window},setpts=PTS-STARTPTS[v{index}];"
                f"[0:a]atrim={window},asetpts=PTS-STARTPTS[a{index}]")
            labels.append(f"[v{index}][a{index}]")

        graph = ";".join(parts)
        graph += (f";{''.join(labels)}concat=n={len(ranges)}:v=1:a=1[vc][ac]"
                  f";[vc]{cls.video_filter()}[vout]")
        return graph

    def _mpeg_command(self, source: str, target: str, item=None):
        """320x240 MPEG-2 program stream, the profile mpegplayer expects."""
        ranges = item.keep_ranges() if item is not None and (
            item.is_segment or item.cuts) else None

        if ranges:
            return [
                self._library.ffmpeg_bin(), "-y", "-loglevel", "error",
                "-i", source,
                "-filter_complex", self.edit_filter_complex(ranges),
                "-map", "[vout]", "-map", "[ac]",
                "-c:v", "mpeg2video", "-pix_fmt", "yuv420p",
                "-bf", "0", "-g", "12", "-flags", "+low_delay",
                "-sc_threshold", "0", "-q:v", "2",
                "-maxrate", "1600k", "-bufsize", "800k",
                "-c:a", "mp2", "-ar", "44100", "-ac", "2", "-b:a", "112k",
                "-packetsize", "2048", "-f", "mpeg",
                target,
            ]

        return [
            self._library.ffmpeg_bin(), "-y", "-loglevel", "error",
            "-i", source,
            "-vf", self.video_filter(),
            "-map", "0:v:0", "-map", "0:a:0?",
            "-c:v", "mpeg2video", "-pix_fmt", "yuv420p",
            "-bf", "0", "-g", "12", "-flags", "+low_delay",
            "-sc_threshold", "0", "-q:v", "2",
            "-maxrate", "1600k", "-bufsize", "800k",
            "-c:a", "mp2", "-ar", "44100", "-ac", "2", "-b:a", "112k",
            "-packetsize", "2048", "-f", "mpeg",
            target,
        ]

    def ensure_mpeg(self, item: LiveTvMedia) -> str:
        """Convert once and reuse. Returns the cached .mpg path."""
        target = self.cached_mpeg_path(item)
        source_mtime = 0.0
        try:
            source_mtime = os.path.getmtime(item.path)
        except OSError:
            pass

        def cache_is_current():
            try:
                return (
                    os.path.isfile(target) and
                    os.path.getsize(target) > 0 and
                    os.path.getmtime(target) >= source_mtime
                )
            except OSError:
                return False

        if cache_is_current():
            return target

        os.makedirs(os.path.dirname(target), exist_ok=True)
        # Sync planning and the background TV-information refresh can overlap.
        # A shared ``target.tmp`` lets one successful encoder rename the other
        # encoder's output out from under it, producing a blank conversion
        # failure even though a valid cache file now exists. Give every job its
        # own staging path instead.
        tmp = f"{target}.{os.getpid()}-{time.time_ns()}.tmp"

        result = subprocess.run(self._mpeg_command(item.path, tmp, item),
                                capture_output=True, text=True, check=False)
        if result.returncode != 0 or not os.path.isfile(tmp) or \
                os.path.getsize(tmp) <= 0:
            try:
                os.remove(tmp)
            except FileNotFoundError:
                pass
            # A competing job may have completed while this encoder was
            # running. Its current, non-empty result is safe to reuse.
            if cache_is_current():
                logger.info(
                    "Using Live TV MPEG cache completed by another job: %s",
                    target,
                )
                return target
            detail = (result.stderr or "").strip()[-300:]
            if not detail:
                detail = f"ffmpeg exited with status {result.returncode}"
            raise RuntimeError(f"MPEG conversion failed for "
                               f"{os.path.basename(item.path)}: {detail}")

        os.replace(tmp, target)
        return target

    # -- sync ------------------------------------------------------------

    def sync(self, mount_path: str, lineup: LiveTvLineup, shows, ads,
             progress=None, cancel=None, remove_device_relatives=None):
        """Convert, copy and write the guide. Returns a summary dict."""
        shows, ads = self.reconcile_missing_sources(lineup, shows, ads)
        by_path = {item.key: item for item in list(shows) + list(ads)}
        # The device resolves channels by number, so never ship a duplicate.
        if lineup.ensure_unique_numbers():
            lineup.save()
        scheduler = LiveTvScheduler(lineup, shows, ads)
        slots = scheduler.build()
        if not slots:
            raise RuntimeError(
                "No Live TV programming. Add videos to ~/Videos/Live and "
                "commercials to ~/Videos/Live/ADS, then rebuild channels."
            )

        root = self.device_root(mount_path)
        os.makedirs(root, exist_ok=True)
        self.prune_stale_cache()

        needed = {}
        for slot in slots:
            item = by_path.get(slot.source)
            if item is not None:
                needed[slot.path] = item

        staging = self._library.staging_dir()
        summary = {
            "converted": 0, "copied": 0, "skipped": 0,
            "removed": 0,
            "staged_deleted": 0, "channels": len(lineup.channels),
            "slots": len(slots), "errors": [], "warnings": [],
            "cache_pruned": 0,
        }

        worst_pair = max_two_day_slot_count(slots)
        if worst_pair > LIVETV_DEVICE_MAX_SLOTS:
            summary["warnings"].append(
                f"The busiest two days hold {worst_pair} programmes but the "
                f"iPod keeps {LIVETV_DEVICE_MAX_SLOTS}. Reduce the number of "
                "channels or use longer shows so no listings are dropped."
            )

        total = len(needed)
        on_device = set()
        for index, (relative, item) in enumerate(sorted(needed.items()), 1):
            if cancel and cancel():
                break
            if progress:
                progress(index, total, item.title)
            try:
                cached = self.ensure_mpeg(item)
            except RuntimeError as error:
                summary["errors"].append(str(error))
                continue
            summary["converted"] += 1

            target = os.path.join(root, relative)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            source_size = os.path.getsize(cached)
            copied_to_device = False
            if os.path.isfile(target) and os.path.getsize(target) == source_size:
                summary["skipped"] += 1
            else:
                try:
                    shutil.copy2(cached, target)
                except OSError as error:
                    summary["errors"].append(
                        f"Could not copy {os.path.basename(item.path)} to "
                        f"the device: {error}")
                    continue
                summary["copied"] += 1
                copied_to_device = True

            if os.path.isfile(target) and \
                    os.path.getsize(target) == source_size:
                on_device.add(relative)

            # Auto-delete only applies to files the Store downloaded into the
            # staging area. The user's own ~/Videos/Live library is never
            # touched by a sync.
            # A split recording feeds several programmes, so removing the
            # source after the first would strand the rest.
            if (copied_to_device and
                    not item.is_segment and
                    item.path.startswith(staging + os.sep)):
                if relative in on_device:
                    try:
                        os.remove(item.path)
                        summary["staged_deleted"] += 1
                    except OSError:
                        pass

        # A slot whose file never made it to the device would otherwise air
        # as "Channel unavailable", so it must not be listed at all.
        playable_slots = [slot for slot in slots if slot.path in on_device]
        dropped = len(slots) - len(playable_slots)
        if dropped:
            summary["warnings"].append(
                f"{dropped} programme slot(s) could not be copied to the "
                "device and were left out of the guide instead of airing "
                "an unplayable channel. See errors above."
            )

        summary["cache_pruned"] = self.prune_orphaned_cache(needed.keys())

        # Logos first: channels.tsv records where the artwork landed on
        # the device, so it cannot be written until conversion has run.
        self.install_logos(root, lineup)
        if any(channel.category == LIVETV_WEATHER_CATEGORY
               for channel in lineup.channels):
            install_weather_icons(mount_path)

        # Content first, channel publication second. If the removable device
        # resets between these two durable writes, its old channels table
        # simply ignores the new guide rows. It can never expose a newly
        # published channel whose guide rows have not reached disk.
        playable_channels = {slot.channel for slot in playable_slots}
        device_channels = [
            channel for channel in lineup.channels
            if channel.number in playable_channels
        ]
        omitted = len(lineup.channels) - len(device_channels)
        if omitted:
            summary["warnings"].append(
                f"{omitted} channel(s) had no playable listings and were "
                "removed from the device guide.")

        # A managed online channel keeps only its current episode set.  Do
        # this after all replacement clips were copied successfully but
        # before the new guide is published, preserving the same
        # content-before-metadata transaction rule as the normal sync.
        root_abs = os.path.abspath(root)
        for relative in sorted(set(remove_device_relatives or [])):
            target = os.path.abspath(os.path.join(root_abs, relative))
            try:
                inside_root = os.path.commonpath([root_abs, target]) == root_abs
            except ValueError:
                inside_root = False
            if not inside_root or not os.path.isfile(target):
                continue
            try:
                os.remove(target)
                summary["removed"] += 1
            except OSError as error:
                summary["warnings"].append(
                    f"Could not remove superseded Live TV clip "
                    f"{os.path.basename(target)}: {error}")
        write_guide_tsv(os.path.join(root, "guide.tsv"), playable_slots)
        write_channels_tsv(os.path.join(root, "channels.tsv"),
                           device_channels, os.path.join(root, "logos"))
        summary["channels"] = len(device_channels)

        return summary

    # -- artwork ---------------------------------------------------------

    def install_logos(self, device_root: str, lineup: LiveTvLineup):
        """Copy the brand mark and any channel logos as Rockbox BMPs."""
        logo_dir = os.path.join(device_root, "logos")
        os.makedirs(logo_dir, exist_ok=True)

        brand = self.ensure_brand_logo()
        if brand and os.path.isfile(brand):
            try:
                shutil.copy2(brand, os.path.join(logo_dir, "directv.bmp"))
            except OSError:
                pass

        for channel in lineup.channels:
            source = str(channel.logo or "").strip()
            if not source:
                continue
            target_name = f"{_safe_name(channel.callsign, 'chan')}.bmp"
            target = os.path.join(logo_dir, target_name)
            if source.startswith("logos/"):
                # Already a device path from an older sync: nothing to
                # convert, and no source left to convert from.
                continue
            if not os.path.isfile(source):
                continue
            # The channel column is the DIRECTV navy, so pad to match.
            # LIVETV_LOGO_W/H in livetv_guide.c size the player's cache
            # buffer; a larger bitmap will not load, so these must agree.
            self._convert_bmp(source, target, LIVETV_LOGO_W, LIVETV_LOGO_H,
                              background=GUIDE_CHANNEL_BG)

    def _convert_bmp(self, source: str, target: str, width: int,
                     height: int, background: str = "black") -> bool:
        """Render an image as a Rockbox-loadable 24-bit BMP.

        Rockbox draws these opaque, so the padding has to be the colour of
        whatever the guide puts behind them; black bars round a logo on the
        pale banner would look nothing like the real receiver.
        """
        command = [
            self._library.ffmpeg_bin(), "-y", "-loglevel", "error",
            "-i", source,
            "-vf",
            f"scale={width}:{height}:force_original_aspect_ratio=decrease,"
            f"pad={width}:{height}:(ow-iw)/2:(oh-ih)/2:{background},"
            f"format=bgr24",
            "-pix_fmt", "bgr24", "-frames:v", "1",
            target,
        ]
        try:
            result = subprocess.run(command, capture_output=True, text=True,
                                    check=False)
        except OSError:
            return False
        return result.returncode == 0 and os.path.isfile(target)

    def ensure_brand_logo(self) -> str:
        """Fetch a real DIRECTV wordmark once; text fallback if offline."""
        cached = os.path.join(self._library.state_dir(), "directv.bmp")
        if os.path.isfile(cached) and os.path.getsize(cached) > 0:
            return cached

        raw = os.path.join(self._library.state_dir(), "directv-source.png")
        if not os.path.isfile(raw):
            for url in BRAND_LOGO_SOURCES:
                if self._download(url, raw):
                    break
        if not os.path.isfile(raw):
            return ""

        # The banner behind the wordmark is the pale DIRECTV blue.
        if self._convert_bmp(raw, cached, LIVETV_BRAND_W, LIVETV_BRAND_H,
                             background=GUIDE_BANNER_BG):
            return cached
        return ""

    @staticmethod
    def _download(url: str, target: str) -> bool:
        import urllib.error
        import urllib.request

        request = urllib.request.Request(
            url, headers={"User-Agent": "rockpod-livetv/1.0"})
        try:
            with urllib.request.urlopen(request, timeout=20) as response:
                payload = response.read()
        except (urllib.error.URLError, OSError, ValueError):
            return False
        if not payload:
            return False
        try:
            with open(target, "wb") as handle:
                handle.write(payload)
        except OSError:
            return False
        return True


try:
    from PySide6.QtCore import QObject, QRunnable, Signal, Slot
except ImportError:  # Allow the service to be used without Qt (tests, CLI).
    QObject = QRunnable = None
else:

    class LiveTvJobSignals(QObject):
        progress = Signal(int, int, str)
        finished = Signal(dict)

    def _emit(signal, *args):
        """Emit unless the window has already gone away underneath us."""
        try:
            signal.emit(*args)
        except RuntimeError:
            pass

    class LiveTvScanJob(QRunnable):
        """Walk the Live folders and probe durations off the UI thread."""

        def __init__(self, library: LiveTvLibrary):
            super().__init__()
            self._library = library
            self.signals = LiveTvJobSignals()

        @Slot()
        def run(self):
            try:
                shows, ads = self._library.scan(
                    probe_durations=True,
                    progress=lambda done, total, label:
                        _emit(self.signals.progress, done, total, label),
                )
                result = {"success": True, "shows": shows, "ads": ads}
            except Exception as error:  # never strand the panel
                logger.exception("Live TV scan failed")
                result = {"success": False, "message": str(error)}
            _emit(self.signals.finished, result)

    class LiveTvSyncJob(QRunnable):
        """Convert, copy and write the guide off the UI thread."""

        def __init__(self, sync: LiveTvSync, mount_path: str,
                     lineup: LiveTvLineup, shows, ads, config=None):
            super().__init__()
            self._sync = sync
            self._mount_path = mount_path
            self._lineup = lineup
            self._shows = list(shows)
            self._ads = list(ads)
            # Optional: when given, the Weather channel's bumper videos
            # (ffmpeg work, hence done here off the UI thread rather than
            # eagerly on every panel render) are built and added to the
            # line-up right before this sync, if a forecast has been
            # synced and local music is configured. See
            # ensure_weather_channel() and docs/livetv-weather-channel-spec.md.
            self._config = config
            self.signals = LiveTvJobSignals()

        @Slot()
        def run(self):
            try:
                if self._config is not None:
                    self._shows = ensure_weather_channel(
                        self._sync, self._lineup, self._config, self._shows,
                        self._ads)
                    self._lineup.save()
                summary = self._sync.sync(
                    self._mount_path, self._lineup, self._shows, self._ads,
                    progress=lambda done, total, label:
                        _emit(self.signals.progress, done, total, label),
                )
                summary["success"] = True
                result = summary
            except Exception as error:
                logger.exception("Live TV sync failed")
                result = {"success": False, "message": str(error)}
            _emit(self.signals.finished, result)

    class LiveTvYoutubeChannelSyncJob(QRunnable):
        """Refresh one built-in YouTube station, then publish the TV guide."""

        def __init__(self, config, mount_path: str, channel_key: str):
            super().__init__()
            self._config = config
            self._mount_path = mount_path
            self._channel_key = channel_key
            self.signals = LiveTvJobSignals()

        @Slot()
        def run(self):
            try:
                library = LiveTvLibrary(self._config)
                lineup = LiveTvLineup(library)
                youtube = LiveTvYoutubeChannelSync(library, self._config)
                spec = youtube._channel_spec(self._channel_key)
                existing = next(
                    (item for item in lineup.channels
                     if item.callsign == spec["callsign"] or
                     item.name == spec["name"]),
                    None,
                )
                previous_keys = set(existing.shows) if existing else set()
                youtube.download(
                    self._channel_key,
                    progress=lambda done, total, label:
                        _emit(self.signals.progress, done, total, label),
                )
                shows, ads = library.scan(
                    probe_durations=True,
                    progress=lambda done, total, label:
                        _emit(self.signals.progress, done, total, label),
                )
                lineup.autobuild(shows, ads)
                channel = youtube.ensure_channel(lineup, self._channel_key,
                                                 shows)
                stale_paths = youtube.stale_device_paths(
                    self._channel_key, previous_keys, channel.shows)
                lineup.save()
                sync = LiveTvSync(library)
                summary = sync.sync(
                    self._mount_path, lineup, shows, ads,
                    progress=lambda done, total, label:
                        _emit(self.signals.progress, done, total, label),
                    remove_device_relatives=stale_paths,
                )
                summary.update({
                    "success": True,
                    "youtube_channel": channel.name,
                    "shows": shows,
                    "ads": ads,
                })
                result = summary
            except Exception as error:
                logger.exception("Live TV YouTube channel sync failed")
                result = {"success": False, "message": str(error)}
            _emit(self.signals.finished, result)

    class TvInformationSyncJob(QRunnable):
        """Refresh renewable channels and sync the complete Live TV guide."""

        def __init__(self, config, mount_path: str):
            super().__init__()
            self._config = config
            self._mount_path = mount_path
            self.signals = LiveTvJobSignals()

        @Slot()
        def run(self):
            try:
                library = LiveTvLibrary(self._config)
                shows, ads = library.scan(
                    probe_durations=True,
                    progress=lambda done, total, label:
                        _emit(self.signals.progress, done, total, label),
                )
                lineup = LiveTvLineup(library)
                lineup.autobuild(shows, ads)
                sync = LiveTvSync(library)

                shows = ensure_weather_channel(
                    sync, lineup, self._config, shows, ads)
                shows, information = ensure_tv_information_channels(
                    lineup, self._config, shows,
                    progress=lambda done, total, label:
                        _emit(self.signals.progress, done, total, label),
                    force=False,
                )
                if not information.media:
                    details = "; ".join(information.warnings[:3])
                    raise RuntimeError(
                        "No TV information channel could be prepared"
                        + (f": {details}" if details else "."))

                lineup.save()
                summary = sync.sync(
                    self._mount_path, lineup, shows, ads,
                    progress=lambda done, total, label:
                        _emit(self.signals.progress, done, total, label),
                )
                summary["success"] = True
                summary["information_channels"] = len(information.media)
                summary["warnings"].extend(information.warnings)
                result = summary
            except Exception as error:
                logger.exception("TV information sync failed")
                result = {"success": False, "message": str(error)}
            _emit(self.signals.finished, result)

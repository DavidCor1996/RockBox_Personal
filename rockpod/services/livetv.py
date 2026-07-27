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

import json
import logging
import os
import random
import re
import shutil
import subprocess
import time
from dataclasses import dataclass, field, asdict
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

# Bump this whenever the encode changes so cached clips are rebuilt rather
# than silently reused in the old framing.
LIVETV_MPEG_PROFILE = "mpeg2-320x240-fill-v2"

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
    """One show or commercial available to the line-up."""

    path: str
    kind: str            # "show" or "ad"
    title: str
    series: str
    duration: int = 0
    rating: str = ""
    description: str = ""
    renamed: bool = False

    @property
    def key(self) -> str:
        return self.path

    def device_relative(self) -> str:
        stem = _safe_name(Path(self.path).stem, "clip")
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
            item.title = entry["title"]
            item.renamed = True
        if entry.get("description"):
            item.description = entry["description"]
        if entry.get("series"):
            item.series = entry["series"]
        return item

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
            series = series_name(path, root)
            item = LiveTvMedia(
                path=path,
                kind="show",
                title=program_title(path, series),
                series=series,
                rating="TV-PG",
                description=program_description(path, series),
            )
            self._apply_override(item)
            if probe_durations:
                item.duration = self.duration_for(path)
            shows.append(item)
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
            ads.append(item)
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
            grouped.setdefault(item.series or "Live TV", []).append(item.path)

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
        self._by_path = {item.path: item for item in list(shows) + list(ads)}
        self._ads = [item for item in ads if item.duration > 0]

    def _channel_ads(self, channel: LiveTvChannel):
        if channel.ads:
            pool = [self._by_path.get(path) for path in channel.ads]
            pool = [item for item in pool if item and item.duration > 0]
            if pool:
                return pool
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


def write_channels_tsv(path: str, channels):
    lines = ["# number\tcallsign\tname\tcategory\tlogo\tfavourite"]
    for channel in channels:
        lines.append("\t".join([
            str(channel.number),
            channel.callsign or "CH",
            channel.name or channel.callsign or "Channel",
            channel.category or "Series",
            channel.logo or "",
            "1" if channel.favourite else "0",
        ]))
    _write_text(path, "\n".join(lines) + "\n")


def write_guide_tsv(path: str, slots):
    """Grouped by channel then day then start: the device relies on it."""
    ordered = sorted(slots, key=lambda s: (s.channel, s.day, s.start))
    lines = ["# chan\tday\tstart\tdur\tkind\ttitle\trating\tdesc\tpath"
             "\tblockstart\tblockdur"]
    for slot in ordered:
        lines.append("\t".join([
            str(slot.channel),
            str(slot.day),
            str(slot.start),
            str(slot.duration),
            slot.kind,
            _tsv_clean(slot.title),
            _tsv_clean(slot.rating) or "--",
            _tsv_clean(slot.description) or "--",
            slot.path,
            str(slot.block_start),
            str(slot.block_duration or slot.duration),
        ]))
    _write_text(path, "\n".join(lines) + "\n")


def _tsv_clean(value: str) -> str:
    return re.sub(r"\s+", " ", str(value or "")).strip()


def _write_text(path, content: str):
    path = os.fspath(path)
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as handle:
        handle.write(content)
    os.replace(tmp, path)


class LiveTvSync:
    """Transcodes to MPEG and copies the whole line-up onto an iPod."""

    def __init__(self, library: LiveTvLibrary):
        self._library = library

    def device_root(self, mount_path: str) -> str:
        return os.path.join(os.path.abspath(mount_path), LIVETV_DEVICE_DIR)

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

    def _mpeg_command(self, source: str, target: str):
        """320x240 MPEG-2 program stream, the profile mpegplayer expects."""
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
        if os.path.isfile(target) and os.path.getsize(target) > 0:
            source_mtime = 0.0
            try:
                source_mtime = os.path.getmtime(item.path)
            except OSError:
                pass
            if os.path.getmtime(target) >= source_mtime:
                return target

        os.makedirs(os.path.dirname(target), exist_ok=True)
        tmp = target + ".tmp"
        try:
            os.remove(tmp)
        except FileNotFoundError:
            pass

        result = subprocess.run(self._mpeg_command(item.path, tmp),
                                capture_output=True, text=True, check=False)
        if result.returncode != 0 or not os.path.isfile(tmp) or \
                os.path.getsize(tmp) <= 0:
            try:
                os.remove(tmp)
            except FileNotFoundError:
                pass
            detail = (result.stderr or "").strip()[-300:]
            raise RuntimeError(f"MPEG conversion failed for "
                               f"{os.path.basename(item.path)}: {detail}")

        os.replace(tmp, target)
        return target

    # -- sync ------------------------------------------------------------

    def sync(self, mount_path: str, lineup: LiveTvLineup, shows, ads,
             progress=None, cancel=None):
        """Convert, copy and write the guide. Returns a summary dict."""
        by_path = {item.path: item for item in list(shows) + list(ads)}
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
            "staged_deleted": 0, "channels": len(lineup.channels),
            "slots": len(slots), "errors": [], "warnings": [],
        }

        worst_pair = max_two_day_slot_count(slots)
        if worst_pair > LIVETV_DEVICE_MAX_SLOTS:
            summary["warnings"].append(
                f"The busiest two days hold {worst_pair} programmes but the "
                f"iPod keeps {LIVETV_DEVICE_MAX_SLOTS}. Reduce the number of "
                "channels or use longer shows so no listings are dropped."
            )

        total = len(needed)
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
            if os.path.isfile(target) and os.path.getsize(target) == source_size:
                summary["skipped"] += 1
            else:
                shutil.copy2(cached, target)
                summary["copied"] += 1

            # Auto-delete only applies to files the Store downloaded into the
            # staging area. The user's own ~/Videos/Live library is never
            # touched by a sync.
            if item.path.startswith(staging + os.sep):
                if os.path.isfile(target) and \
                        os.path.getsize(target) == source_size:
                    try:
                        os.remove(item.path)
                        summary["staged_deleted"] += 1
                    except OSError:
                        pass

        write_channels_tsv(os.path.join(root, "channels.tsv"),
                           lineup.channels)
        write_guide_tsv(os.path.join(root, "guide.tsv"), slots)
        self.install_logos(root, lineup)

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
            if not source or not os.path.isfile(source):
                continue
            target_name = f"{_safe_name(channel.callsign, 'chan')}.bmp"
            target = os.path.join(logo_dir, target_name)
            # The channel column is the DIRECTV navy, so pad to match.
            if self._convert_bmp(source, target, 40, 18,
                                 background=GUIDE_CHANNEL_BG):
                channel.logo = f"logos/{target_name}"
        lineup.save()

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
        if self._convert_bmp(raw, cached, 64, 22,
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
                     lineup: LiveTvLineup, shows, ads):
            super().__init__()
            self._sync = sync
            self._mount_path = mount_path
            self._lineup = lineup
            self._shows = list(shows)
            self._ads = list(ads)
            self.signals = LiveTvJobSignals()

        @Slot()
        def run(self):
            try:
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

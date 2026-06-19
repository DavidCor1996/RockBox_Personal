"""Metadata extraction using mutagen."""

import hashlib
import logging
import os
import re
from pathlib import Path

import mutagen
from mutagen.mp3 import MP3
from mutagen.flac import FLAC
from mutagen.oggvorbis import OggVorbis
from mutagen.mp4 import MP4
from mutagen.aiff import AIFF
from mutagen.wavpack import WavPack
from mutagen.id3 import ID3

from models.track import Track, compute_artwork_hash

logger = logging.getLogger(__name__)
VIDEO_EXTENSIONS = {".mp4", ".m4v", ".mov", ".mkv", ".avi", ".webm", ".mpg", ".mpeg", ".mpe"}
_GENERIC_VIDEO_FOLDER_NAMES = {
    "video", "videos", "movie", "movies", "tv", "tv shows", "shows", "series", "anime",
    "archive", "_archive", "mediadrive", "_mediadrive", "youtube", "sample", "sample & cover",
    "other cartoons you'd probably like, here",
}
_TV_SHOW_FOLDER_NAMES = {"tv", "tv shows", "shows", "series", "anime"}
_ARCHIVE_LIBRARY_FOLDER_NAMES = {"_archive", "archive", "_mediadrive", "mediadrive"}
_HOME_VIDEO_KEYWORDS = {
    "camera", "dcim", "iphone", "gopro", "phone", "vacation", "holiday",
    "birthday", "wedding", "family", "home video", "home videos", "clip", "clips",
}
_NOISE_TOKENS = {
    "2160p", "1080p", "720p", "480p", "x264", "x265", "h264", "h265", "hevc",
    "xvid", "divx", "bluray", "brrip", "dvdrip", "webrip", "webdl", "web-dl",
    "hdrip", "hdr", "eng", "ita", "jap", "multi", "vostf", "vf2", "proper",
    "repack",
}
_CAMERA_FILENAME_RE = re.compile(
    r"(?i)^(?:img|vid|mov|pxl|dsc|mvi|clip|gopr|gp\d{2}|gh\d{2})[-_ ]?\d+"
)
_GENERIC_SHOW_TITLES = {"", "show", "shows", "series", "tv", "tv shows", "special", "specials"}
_URL_RE = re.compile(r"(?i)\bhttps?://\S+")
_ARCHIVE_SUFFIX_RE = re.compile(r"(?i)\s*[-:]\s*(?:https?://\S+|archive\.org/details/\S+)\s*$")
_TRAILING_YEAR_RE = re.compile(r"(?i)\s*[\[(](19\d{2}|20\d{2})(?:\s*[-/]\s*(19\d{2}|20\d{2}))?[\])]\s*$")
_TRAILING_BARE_YEAR_RE = re.compile(r"(?i)\s+(19\d{2}|20\d{2})\s*$")
_RELEASE_TAIL_RE = re.compile(
    r"""(?ix)
    (?:^|[\s._\-\[(\]])
    (
        2160p|1080p|720p|480p|4k|8k|sdr|hdr10\+?|hdr|dolby[ ._-]?vision|dv|
        web(?:[ ._-]?dl)?|webrip|bluray|brrip|dvdrip|hdtv|atvp|amzn|dsnp|nf|
        aac(?:\d(?:\.\d)?)?|ddp(?:\d(?:\.\d)?)?|dd(?:\d(?:\.\d)?)?|ac-?3|e-?ac-?3|
        dts(?:[ ._-]?hd)?|truehd|atmos|x264|x265|h264|h265|hevc|av1|10bit|8bit|
        proper|repack|remux|vostf|vf2|multi|dubbed|subbed|uncut|extended|
        yts(?:[ ._-]?am)?|galaxyrg\d*|pophd|nahom|ben[ ._-]?the[ ._-]?men
    )
    (?:$|[\s._\-\])])
    """
)
_TRAILING_BRACKETED_ID_RE = re.compile(r"\s*\[[A-Za-z0-9_-]{6,16}\]\s*$")

CODEC_MAP = {
    ".mp3": "MP3",
    ".flac": "FLAC",
    ".ogg": "OGG Vorbis",
    ".opus": "Opus",
    ".mp4": "MPEG-4 Video",
    ".m4v": "MPEG-4 Video",
    ".m4a": "AAC",
    ".aac": "AAC",
    ".alac": "ALAC",
    ".mov": "QuickTime",
    ".mkv": "Matroska",
    ".avi": "AVI",
    ".webm": "WebM",
    ".mpg": "MPEG Video",
    ".mpeg": "MPEG Video",
    ".mpe": "MPEG Video",
    ".aiff": "AIFF",
    ".aif": "AIFF",
    ".wav": "WAV",
    ".wma": "WMA",
    ".ape": "APE",
    ".wv": "WavPack",
}


def compute_file_hash(filepath, block_size=65536):
    """Compute MD5 hash of a file for deduplication."""
    h = hashlib.md5()
    try:
        with open(filepath, "rb") as f:
            while True:
                block = f.read(block_size)
                if not block:
                    break
                h.update(block)
        return h.hexdigest()
    except OSError:
        return ""


def _safe_str(val):
    """Extract string from mutagen tag value."""
    if val is None:
        return ""
    if isinstance(val, list):
        return str(val[0]) if val else ""
    return str(val)


def _safe_int(val):
    """Extract integer from mutagen tag value."""
    s = _safe_str(val)
    if not s:
        return None
    try:
        if "/" in s:
            s = s.split("/")[0]
        return int(float(s))
    except (ValueError, TypeError):
        return None


def _safe_year(val):
    """Extract a plausible four-digit year from a tag value."""
    s = _safe_str(val).strip()
    if not s:
        return None
    match = re.search(r"(?<!\d)(19\d{2}|20\d{2})(?!\d)", s)
    if match:
        return int(match.group(1))
    if re.fullmatch(r"(19\d{2}|20\d{2})\d{4}", s):
        return int(s[:4])
    return None


def _safe_track_total(val):
    """Extract track total from 'N/M' format."""
    s = _safe_str(val)
    if "/" in s:
        try:
            return int(s.split("/")[1])
        except (ValueError, IndexError):
            pass
    return None


def _extract_id3_tags(audio):
    """Extract tags from ID3-tagged files (MP3, AIFF)."""
    tags = audio.tags
    if tags is None:
        return {}
    return {
        "title": _safe_str(tags.get("TIT2")),
        "artist": _safe_str(tags.get("TPE1")),
        "album": _safe_str(tags.get("TALB")),
        "album_artist": _safe_str(tags.get("TPE2")),
        "genre": _safe_str(tags.get("TCON")),
        "year": _safe_year(tags.get("TDRC") or tags.get("TYER")),
        "track_number": _safe_int(tags.get("TRCK")),
        "track_total": _safe_track_total(tags.get("TRCK")),
        "disc_number": _safe_int(tags.get("TPOS")) or 1,
        "disc_total": _safe_track_total(tags.get("TPOS")),
        "composer": _safe_str(tags.get("TCOM")),
        "comment": _safe_str(tags.get("COMM::eng") or tags.get("COMM")),
        "compilation": 1 if _safe_str(tags.get("TCMP")) == "1" else 0,
    }


def _extract_vorbis_tags(audio):
    """Extract tags from Vorbis comment files (FLAC, OGG)."""
    tags = audio.tags or audio
    if tags is None:
        return {}

    def g(key):
        val = tags.get(key)
        if isinstance(val, list) and val:
            return val[0]
        return val

    return {
        "title": _safe_str(g("title")),
        "artist": _safe_str(g("artist")),
        "album": _safe_str(g("album")),
        "album_artist": _safe_str(g("albumartist") or g("album artist")),
        "genre": _safe_str(g("genre")),
        "year": _safe_year(g("date") or g("year")),
        "track_number": _safe_int(g("tracknumber")),
        "track_total": _safe_int(g("tracktotal") or g("totaltracks")),
        "disc_number": _safe_int(g("discnumber")) or 1,
        "disc_total": _safe_int(g("disctotal") or g("totaldiscs")),
        "composer": _safe_str(g("composer")),
        "comment": _safe_str(g("comment")),
        "compilation": 1 if _safe_str(g("compilation")) == "1" else 0,
    }


def _extract_mp4_tags(audio):
    """Extract tags from MP4/M4A/AAC files."""
    tags = audio.tags
    if tags is None:
        return {}

    def g(key):
        val = tags.get(key)
        if isinstance(val, list) and val:
            return val[0]
        return val

    tn = g("trkn")
    dn = g("disk")
    track_num = tn[0] if isinstance(tn, tuple) else None
    track_tot = tn[1] if isinstance(tn, tuple) and len(tn) > 1 else None
    disc_num = dn[0] if isinstance(dn, tuple) else 1
    disc_tot = dn[1] if isinstance(dn, tuple) and len(dn) > 1 else None

    return {
        "title": _safe_str(g("\xa9nam")),
        "artist": _safe_str(g("\xa9ART")),
        "album": _safe_str(g("\xa9alb")),
        "album_artist": _safe_str(g("aART")),
        "genre": _safe_str(g("\xa9gen")),
        "year": _safe_year(g("\xa9day")),
        "track_number": track_num,
        "track_total": track_tot,
        "disc_number": disc_num or 1,
        "disc_total": disc_tot,
        "composer": _safe_str(g("\xa9wrt")),
        "comment": _safe_str(g("\xa9cmt")),
        "compilation": 1 if g("cpil") else 0,
    }


def _extract_mp4_video_tags(audio):
    tags = getattr(audio, "tags", None)
    if tags is None:
        return {}

    def g(key):
        val = tags.get(key)
        if isinstance(val, list) and val:
            return val[0]
        return val

    return {
        "show_title": _safe_str(g("tvsh")),
        "season_number": _safe_int(g("tvsn")),
        "episode_number": _safe_int(g("tves")),
        "video_kind": _normalize_video_kind(
            g("stik") or g("media type") or g("mediatype") or g("contenttype")
        ),
    }


def _normalize_tag_name(key):
    text = str(key or "").strip().lower()
    return re.sub(r"[\s_.:/-]+", "", text)


def _generic_tag_value(tags, *keys):
    if tags is None:
        return None
    normalized = {}
    try:
        items = tags.items()
    except AttributeError:
        return None
    for raw_key, raw_val in items:
        normalized[_normalize_tag_name(raw_key)] = raw_val
    for key in keys:
        value = normalized.get(_normalize_tag_name(key))
        if value not in (None, "", []):
            return value
    return None


def _extract_generic_tags(audio):
    tags = getattr(audio, "tags", None)
    if tags is None and hasattr(audio, "get"):
        tags = audio
    if tags is None:
        return {}
    return {
        "title": _safe_str(_generic_tag_value(tags, "title", "inam", "name")),
        "artist": _safe_str(_generic_tag_value(tags, "artist", "performer", "director", "author", "iart")),
        "album": _safe_str(_generic_tag_value(tags, "album", "show", "series", "tvshow", "iprd", "collection")),
        "album_artist": _safe_str(_generic_tag_value(tags, "albumartist", "album artist", "show", "series", "tvshow", "director")),
        "genre": _safe_str(_generic_tag_value(tags, "genre", "igen")),
        "year": _safe_year(_generic_tag_value(tags, "date", "year", "icrd", "recordingdate")),
        "track_number": _safe_int(_generic_tag_value(tags, "tracknumber", "track", "trck", "itrk", "partnumber")),
        "track_total": _safe_int(_generic_tag_value(tags, "tracktotal", "totaltracks")),
        "disc_number": _safe_int(_generic_tag_value(tags, "discnumber", "disc", "disk")) or 1,
        "disc_total": _safe_int(_generic_tag_value(tags, "disctotal", "totaldiscs")),
        "composer": _safe_str(_generic_tag_value(tags, "composer")),
        "comment": _safe_str(_generic_tag_value(tags, "comment", "description", "summary", "synopsis", "icmt")),
    }


def _extract_generic_video_tags(audio):
    tags = getattr(audio, "tags", None)
    if tags is None and hasattr(audio, "get"):
        tags = audio
    if tags is None:
        return {}
    return {
        "show_title": _safe_str(
            _generic_tag_value(tags, "show", "series", "tvshow", "showtitle", "collection")
        ),
        "season_number": _safe_int(
            _generic_tag_value(tags, "season", "seasonnumber", "tvseason", "seasonnum")
        ),
        "episode_number": _safe_int(
            _generic_tag_value(
                tags,
                "episode",
                "episodenumber",
                "episodeid",
                "tvepisode",
                "tvepisodenum",
                "partnumber",
            )
        ),
        "video_kind": _normalize_video_kind(
            _generic_tag_value(tags, "mediatype", "contenttype", "category", "type")
        ),
    }


def _clean_media_name(value):
    text = str(value or "").strip()
    if not text:
        return ""
    text = _URL_RE.sub("", text)
    text = re.sub(r"[\[\]]", " ", text)
    text = re.sub(r"[._]+", " ", text)
    if " " not in text and "-" in text:
        text = text.replace("-", " ")
    text = re.sub(r"\s+", " ", text).strip(" -_")
    parts = [part for part in text.split() if part.casefold() not in _NOISE_TOKENS]
    cleaned = " ".join(parts).strip(" -_")
    return cleaned or text


def _strip_release_tail(value):
    text = str(value or "").strip()
    if not text:
        return ""
    text = re.sub(r"[._]+", " ", text)
    text = re.sub(r"\s+", " ", text).strip()
    match = _RELEASE_TAIL_RE.search(text)
    if match:
        text = text[:match.start()].rstrip(" -_.([")
        text = re.sub(r"[\[(][^)\]]*$", "", text).rstrip(" -_.([")
    text = _TRAILING_BRACKETED_ID_RE.sub("", text).strip(" -_.")
    return re.sub(r"\s+", " ", text).strip()


def _strip_trailing_year(value):
    text = str(value or "").strip()
    if not text:
        return "", None
    match = _TRAILING_YEAR_RE.search(text)
    if match:
        start_year = int(match.group(1))
        return text[:match.start()].strip(" -_"), start_year
    match = _TRAILING_BARE_YEAR_RE.search(text)
    if match and re.search(r"[A-Za-z]", text[:match.start()]):
        return text[:match.start()].strip(" -_"), int(match.group(1))
    match = re.search(r"(?i)\s*[\[(][^)\]]*?(19\d{2}|20\d{2})[^)\]]*[\])]\s*$", text)
    if match:
        return text[:match.start()].strip(" -_"), int(match.group(1))
    return text, None


def _looks_like_year_suffixed_text(value):
    text = str(value or "").strip()
    if not text:
        return False
    if _TRAILING_YEAR_RE.search(text):
        return True
    return bool(_TRAILING_BARE_YEAR_RE.search(text) and re.search(r"[A-Za-z]", text))


def _canonicalize_series_name(value):
    text = _clean_media_name(value)
    if not text:
        return ""
    lowered = text.casefold()
    if lowered in _GENERIC_SHOW_TITLES:
        return ""
    text = _strip_release_tail(text)
    text = re.sub(r"(?i)^\d+\.\s*", "", text).strip(" -_")
    text = _ARCHIVE_SUFFIX_RE.sub("", text).strip(" -_")
    text = re.sub(r"(?i)\barchive\.org/details/\S+\b", "", text).strip(" -_")
    text = re.sub(r"(?i)\bcomplete\b.*$", "", text).strip(" -_")
    text = re.sub(r"(?i)\bseason\s+\d+(?:\s*-\s*\d+)?\b.*$", "", text).strip(" -_")
    text = re.sub(r"(?i)\bs\d{1,2}(?:\s*-\s*s?\d{1,2})?\b.*$", "", text).strip(" -_")
    text = re.sub(r"(?i)\b(?:complete|full)\s+series\b", "", text).strip(" -_")
    text = re.sub(r"(?i)\bseason\s+\d+\b", "", text).strip(" -_")
    text = re.sub(r"(?i)\bspecials?\b", "", text).strip(" -_")
    text = re.sub(r"(?i)\b(?:archive|archives)\b", "", text).strip(" -_")
    text = re.sub(r"(?i)[-_ ]\d{6,8}$", "", text).strip(" -_")
    text, _year = _strip_trailing_year(text)
    text = re.sub(r"\s+", " ", text).strip()
    lowered = text.casefold()
    if lowered in _GENERIC_SHOW_TITLES:
        return ""
    return text


def _normalize_video_kind(value):
    text = str(value or "").strip().casefold().replace("-", " ").replace("_", " ")
    if not text:
        return ""
    if any(token in text for token in ("tv show", "show", "series", "episode")):
        return "show"
    if any(token in text for token in ("home video", "camcorder", "personal", "family")):
        return "home_video"
    if any(token in text for token in ("movie", "film")):
        return "movie"
    return ""


def _looks_like_season(name):
    text = str(name or "").strip()
    return bool(
        re.match(
            r"(?i)^(?:\d+\.\s*)?(?:season\s*\d+(?:\s*-\s*\d+)?|s\d{1,2}(?:\s*-\s*s?\d{1,2})?)(?:\s*\([^)]*\))?$",
            text,
        )
    )


def _looks_like_specials(name):
    text = str(name or "").strip().casefold()
    return text in {"special", "specials", "ova", "ovas", "ona", "onas", "extras", "bonus", "bonus features"}


def _season_number_from_text(value):
    match = re.search(r"(?i)(?:season|s)\s*(\d{1,2})", str(value or ""))
    return int(match.group(1)) if match else None


def _episode_number_from_text(value):
    text = str(value or "")
    match = re.search(r"(?i)s\d{1,2}[ ._-]*e(\d{1,3})", text)
    if match:
        return int(match.group(1))
    match = re.search(r"(?i)(?:episode|ep)\s*(\d{1,3})", text)
    if match:
        return int(match.group(1))
    return None


def _infer_show_name_from_path(path):
    path = Path(path)
    parent = _canonicalize_series_name(path.parent.name)
    grandparent = _canonicalize_series_name(path.parent.parent.name if path.parent.parent != path.parent else "")
    great_grandparent = _canonicalize_series_name(
        path.parent.parent.parent.name
        if path.parent.parent.parent != path.parent.parent
        else ""
    )
    parent_raw = str(path.parent.name or "").strip().casefold()
    grandparent_raw = str(path.parent.parent.name if path.parent.parent != path.parent else "").strip().casefold()
    great_grandparent_raw = str(
        path.parent.parent.parent.name if path.parent.parent.parent != path.parent.parent else ""
    ).strip().casefold()

    if (_looks_like_season(parent_raw) or _looks_like_specials(parent_raw)) and grandparent:
        return grandparent
    if parent and grandparent_raw in _ARCHIVE_LIBRARY_FOLDER_NAMES:
        return parent
    if grandparent and great_grandparent_raw in _ARCHIVE_LIBRARY_FOLDER_NAMES:
        return grandparent
    if parent and grandparent_raw in _TV_SHOW_FOLDER_NAMES:
        return parent
    if grandparent and great_grandparent_raw in _TV_SHOW_FOLDER_NAMES:
        return grandparent
    return ""


def _extract_year_from_text(*values):
    for value in values:
        match = re.search(r"(?<!\d)(19\d{2}|20\d{2})(?!\d)", str(value or ""))
        if match:
            return int(match.group(1))
    return None


def _leading_date_year(value):
    match = re.match(r"(?i)^\s*(19\d{2}|20\d{2})[-_.](\d{2})[-_.](\d{2})(?:\b|\s*-)", str(value or ""))
    return int(match.group(1)) if match else None


def _clean_video_title(value):
    text = _clean_media_name(value)
    if not text:
        return ""
    text = _strip_release_tail(text)
    text = _ARCHIVE_SUFFIX_RE.sub("", text).strip(" -_")
    text = re.sub(r"(?i)\barchive\.org/details/\S+\b", "", text).strip(" -_")
    text = re.sub(r"(?i)\bquicktime\b", "", text).strip(" -_")
    text, _year = _strip_trailing_year(text)
    text = re.sub(r"[\[(][^)\]]*$", "", text).strip(" -_")
    text = re.sub(r"\s+", " ", text).strip(" -_")
    return text


def _normalize_movie_fields(track, path, parent):
    folder_title = ""
    if parent and parent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES:
        folder_title = _clean_video_title(parent)
    cleaned_album = _clean_video_title(track.album)
    cleaned_artist = _clean_video_title(track.artist)
    cleaned_title = _clean_video_title(track.title)
    if folder_title and (_title_needs_cleanup(track.title) or cleaned_title == _clean_video_title(path.stem)):
        cleaned_title = folder_title
    if cleaned_title:
        track.title = cleaned_title
    if folder_title and (not cleaned_album or _title_needs_cleanup(track.album)):
        cleaned_album = folder_title
    elif cleaned_title and (not cleaned_album or _title_needs_cleanup(track.album)):
        cleaned_album = cleaned_title
    if folder_title and (not cleaned_artist or _title_needs_cleanup(track.artist)):
        cleaned_artist = folder_title
    if cleaned_album:
        track.album = cleaned_album
    if cleaned_artist:
        track.artist = cleaned_artist


def _title_needs_cleanup(title):
    text = str(title or "").strip()
    if not text:
        return True
    lowered = text.casefold()
    if "archive.org/details/" in lowered:
        return True
    if "http://" in lowered or "https://" in lowered:
        return True
    if "quicktime" in lowered:
        return True
    if lowered in {"youtube", "mpeg video", "youtube mpeg video"}:
        return True
    if "complete series" in lowered or "full series" in lowered:
        return True
    if re.search(r"(?i)s\d{1,2}[ ._-]*e\d{1,3}", text):
        return True
    if _RELEASE_TAIL_RE.search(text):
        return True
    if _TRAILING_BRACKETED_ID_RE.search(text):
        return True
    if _looks_like_year_suffixed_text(text):
        return True
    cleaned, extracted_year = _strip_trailing_year(text)
    if extracted_year and cleaned != text:
        return True
    return False


def _title_parts_from_stem(stem):
    raw_text = _clean_media_name(stem)
    season_episode = re.match(
        r"(?i)^(?:(.+?)(?:\s*-\s*|\s+))?s(\d{1,2})[ ._-]*e(\d{1,3})(?:\s*[- ]\s*|\s+)?(.*)$",
        raw_text,
    )
    if season_episode:
        prefix = _clean_video_title(season_episode.group(1).strip()) if season_episode.group(1) else ""
        season = int(season_episode.group(2))
        episode = int(season_episode.group(3))
        raw_suffix = season_episode.group(4).strip()
        title = _clean_video_title(raw_suffix)
        if not title:
            title = f"Episode {episode}" if raw_suffix else (prefix or _clean_video_title(raw_text))
        return title, season, episode
    text = _clean_video_title(raw_text)
    episode_named = re.match(r"(?i)^e(\d{1,3})\s*[- ]+\s*(.+)$", text)
    if episode_named:
        return _clean_video_title(episode_named.group(2).strip()), None, int(episode_named.group(1))
    numbered = re.match(r"(?i)^(\d{1,3})\s*[- ]+\s*(.+)$", text)
    if numbered:
        return _clean_video_title(numbered.group(2).strip()), None, int(numbered.group(1))
    return text, None, None


def _show_name_from_stem(stem):
    text = _clean_video_title(stem)
    match = re.match(
        r"(?i)^(.+?)(?:\s*-\s*|\s+)s\d{1,2}[ ._-]*e\d{1,3}(?:\s*[- ]\s*|\s+.*)?$",
        text,
    )
    if not match:
        return ""
    return _canonicalize_series_name(match.group(1).strip())


def _apply_video_path_fallback(track, filepath):
    path = Path(filepath)
    stem_title, season_num, inferred_track = _title_parts_from_stem(path.stem)
    stem_show_name = _show_name_from_stem(path.stem)
    parent = _clean_media_name(path.parent.name)
    grandparent = _clean_media_name(path.parent.parent.name if path.parent.parent != path.parent else "")
    great_grandparent = _clean_media_name(
        path.parent.parent.parent.name
        if path.parent.parent.parent != path.parent.parent
        else ""
    )
    tagged_show = _canonicalize_series_name(track.show_title)
    album = _clean_media_name(track.album)
    album_artist = _canonicalize_series_name(track.album_artist)
    artist = _canonicalize_series_name(track.artist)
    path_show_name = _infer_show_name_from_path(path)

    if not track.year:
        for candidate in (track.title, track.show_title, track.album, track.artist, track.album_artist):
            _cleaned, extracted_year = _strip_trailing_year(candidate)
            if extracted_year:
                track.year = extracted_year
                break
    path_date_year = _leading_date_year(path.stem)
    if path_date_year and (not track.year or abs(int(track.year) - path_date_year) > 1):
        track.year = path_date_year

    if track.season_number is None:
        for candidate in (
            season_num,
            _season_number_from_text(track.album),
            _season_number_from_text(parent),
            _season_number_from_text(path.stem),
        ):
            if candidate is not None:
                track.season_number = candidate
                break
    if track.season_number is None and _looks_like_specials(parent):
        track.season_number = 0
    if track.episode_number is None:
        for candidate in (
            inferred_track,
            _episode_number_from_text(path.stem),
            _safe_int(track.track_number),
        ):
            if candidate is not None:
                track.episode_number = candidate
                break

    if not track.title or _title_needs_cleanup(track.title):
        track.title = stem_title
    if track.episode_number is not None and not track.track_number:
        track.track_number = track.episode_number
    if track.season_number and not track.album and (_looks_like_season(parent) or season_num):
        track.album = f"Season {track.season_number}"
    elif track.album:
        cleaned_album = _clean_video_title(track.album)
        if cleaned_album:
            track.album = cleaned_album

    show_evidence = bool(
        tagged_show
        or track.season_number
        or track.episode_number
        or _looks_like_season(parent)
        or _looks_like_specials(parent)
        or re.search(r"(?i)s\d{1,2}[ ._-]*e\d{1,3}", path.stem)
        or (
            parent
            and parent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES
            and grandparent.casefold() in _TV_SHOW_FOLDER_NAMES
        )
        or (
            grandparent
            and grandparent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES
            and great_grandparent.casefold() in _TV_SHOW_FOLDER_NAMES
        )
        or (
            parent
            and parent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES
            and grandparent.casefold() in _ARCHIVE_LIBRARY_FOLDER_NAMES
            and re.search(r"(?i)s\d{1,2}[ ._-]*e\d{1,3}", path.stem)
        )
    )

    if not track.show_title and show_evidence:
        if tagged_show:
            track.show_title = tagged_show
        elif path_show_name:
            track.show_title = path_show_name
        elif album_artist and not _looks_like_season(album_artist):
            track.show_title = album_artist
        elif album and not _looks_like_season(album):
            track.show_title = album
        elif _looks_like_season(parent) and grandparent and grandparent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES:
            track.show_title = grandparent
        elif _looks_like_specials(parent) and grandparent and grandparent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES:
            track.show_title = grandparent
        elif (track.season_number or track.episode_number) and parent and parent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES:
            track.show_title = grandparent if _looks_like_season(parent) else parent
        elif parent and parent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES and grandparent.casefold() in _TV_SHOW_FOLDER_NAMES:
            track.show_title = parent
        elif grandparent and grandparent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES and great_grandparent.casefold() in _TV_SHOW_FOLDER_NAMES:
            track.show_title = grandparent
        elif stem_show_name:
            track.show_title = stem_show_name
        elif re.search(r"(?i)s\d{1,2}[ ._-]*e\d{1,3}", path.stem) and parent and parent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES:
            track.show_title = _canonicalize_series_name(parent) or parent
        elif re.search(r"(?i)s\d{1,2}[ ._-]*e\d{1,3}", path.stem) and artist:
            track.show_title = artist

    track.show_title = _canonicalize_series_name(track.show_title) or path_show_name or track.show_title
    if not track.show_title and path_show_name:
        track.show_title = path_show_name
    if track.album_artist:
        track.album_artist = _canonicalize_series_name(track.album_artist) or _clean_video_title(track.album_artist)
    if track.artist:
        if track.show_title:
            track.artist = _canonicalize_series_name(track.artist) or track.show_title
        else:
            track.artist = _clean_video_title(track.artist)

    if _looks_like_season(parent):
        if not track.album:
            track.album = parent
    elif _looks_like_specials(parent):
        if not track.album:
            track.album = "Specials"
    elif parent and parent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES:
        if not track.album and not track.show_title:
            track.album = parent
        if not track.artist:
            if grandparent and grandparent.casefold() not in _GENERIC_VIDEO_FOLDER_NAMES:
                track.artist = grandparent
            else:
                track.artist = parent

    if track.show_title:
        track.show_title = _canonicalize_series_name(track.show_title) or path_show_name or track.show_title
        canonical_album_artist = _canonicalize_series_name(track.album_artist)
        canonical_artist = _canonicalize_series_name(track.artist)
        if canonical_album_artist:
            track.album_artist = canonical_album_artist
        else:
            track.album_artist = track.show_title
        if canonical_artist:
            track.artist = canonical_artist
        else:
            track.artist = track.show_title
        if not track.album and track.season_number:
            track.album = f"Season {track.season_number}"
    if not track.year:
        track.year = _extract_year_from_text(path.stem, parent, grandparent)
    if not track.video_kind:
        text = " ".join(
            [
                str(track.genre or ""),
                str(track.comment or ""),
                str(track.show_title or ""),
                str(path),
            ]
        ).casefold()
        if track.show_title or track.season_number or track.episode_number:
            track.video_kind = "show"
        elif any(keyword in text for keyword in _HOME_VIDEO_KEYWORDS) or _CAMERA_FILENAME_RE.match(path.stem):
            track.video_kind = "home_video"
        else:
            track.video_kind = "movie"
    if track.video_kind == "movie":
        _normalize_movie_fields(track, path, parent)


def read_lyrics(filepath, mutagen_file_func=None):
    """Read lyrics from a sidecar or embedded tags when available."""
    try:
        sidecar = Path(str(filepath)).with_suffix(".lrc")
        if sidecar.is_file():
            return sidecar.read_text(encoding="utf-8-sig").strip()
    except OSError as exc:
        logger.debug("Lyrics sidecar read failed for %s: %s", filepath, exc)

    ext = Path(str(filepath)).suffix.lower()
    mutagen_file_func = mutagen_file_func or mutagen.File
    try:
        audio = mutagen_file_func(filepath)
        if audio is None:
            return ""

        if ext == ".mp3" or ext in (".aiff", ".aif"):
            tags = getattr(audio, "tags", None)
            if not tags:
                return ""
            values = []
            for key in tags.keys():
                if str(key).startswith("USLT"):
                    frame = tags.get(key)
                    text = getattr(frame, "text", "")
                    if isinstance(text, list):
                        text = "\n".join(str(part) for part in text if str(part).strip())
                    text = str(text or "").strip()
                    if text:
                        values.append(text)
            return "\n\n".join(values).strip()

        if ext in (".flac", ".ogg", ".opus"):
            tags = getattr(audio, "tags", None) or audio
            if not tags:
                return ""
            for key in ("lyrics", "unsyncedlyrics", "lyric"):
                value = tags.get(key)
                if isinstance(value, list):
                    text = "\n".join(str(part) for part in value if str(part).strip())
                else:
                    text = _safe_str(value)
                text = text.strip()
                if text:
                    return text
            return ""

        if ext in (".mp4", ".m4v", ".m4a", ".aac", ".alac", ".mov"):
            tags = getattr(audio, "tags", None)
            if not tags:
                return ""
            value = tags.get("\xa9lyr")
            if isinstance(value, list):
                return "\n".join(str(part) for part in value if str(part).strip()).strip()
            return _safe_str(value).strip()
    except Exception as exc:
        logger.debug("Lyrics extraction failed for %s: %s", filepath, exc)
    return ""


def has_embedded_artwork(filepath):
    """Check if a file has embedded artwork."""
    ext = Path(filepath).suffix.lower()
    try:
        audio = mutagen.File(filepath)
        if audio is None:
            return False

        if ext == ".mp3" or ext in (".aiff", ".aif"):
            if audio.tags:
                for key in audio.tags:
                    if key.startswith("APIC"):
                        return True
        elif ext == ".flac":
            if audio.pictures:
                return True
        elif ext in (".mp4", ".m4v", ".m4a", ".aac", ".alac", ".mov"):
            if audio.tags and audio.tags.get("covr"):
                return True
        elif ext == ".ogg":
            if audio.tags:
                pic = audio.tags.get("metadata_block_picture")
                if pic:
                    return True
    except Exception:
        pass
    return False


def extract_artwork_data(filepath):
    """Extract embedded artwork bytes from a file. Returns (data, mime_type) or (None, None)."""
    ext = Path(filepath).suffix.lower()
    try:
        audio = mutagen.File(filepath)
        if audio is None:
            return None, None

        if ext == ".mp3" or ext in (".aiff", ".aif"):
            if audio.tags:
                for key in audio.tags:
                    if key.startswith("APIC"):
                        pic = audio.tags[key]
                        return pic.data, pic.mime
        elif ext == ".flac":
            if audio.pictures:
                pic = audio.pictures[0]
                return pic.data, pic.mime
        elif ext in (".mp4", ".m4v", ".m4a", ".aac", ".alac", ".mov"):
            if audio.tags:
                covers = audio.tags.get("covr")
                if covers:
                    return bytes(covers[0]), "image/jpeg"
        elif ext == ".ogg":
            if audio.tags:
                import base64
                pic_data = audio.tags.get("metadata_block_picture")
                if pic_data:
                    from mutagen.flac import Picture
                    p = Picture(base64.b64decode(pic_data[0]))
                    return p.data, p.mime
    except Exception as e:
        logger.debug("Artwork extraction failed for %s: %s", filepath, e)
    return None, None


def read_metadata_details(filepath, mutagen_file_func=None, artwork_extractor_func=None):
    """Read metadata from a local media file.

    Returns `(track, info)` where `info` contains:
    - `parsed_ok`: True when container metadata was read successfully
    - `warnings`: non-fatal issues encountered while reading metadata/artwork
    """
    filepath = str(filepath)
    ext = Path(filepath).suffix.lower()
    is_video = ext in VIDEO_EXTENSIONS
    mutagen_file_func = mutagen_file_func or mutagen.File
    artwork_extractor_func = artwork_extractor_func or extract_artwork_data

    track = Track(file_path=filepath)
    track.codec = CODEC_MAP.get(ext, ext.upper().lstrip("."))
    info = {
        "parsed_ok": False,
        "warnings": [],
    }
    container_metadata_found = False

    try:
        stat = os.stat(filepath)
        track.file_size = stat.st_size
        track.last_modified = stat.st_mtime
    except OSError:
        info["warnings"].append("file stat failure")

    try:
        audio = mutagen_file_func(filepath)
        if audio is None:
            track.title = Path(filepath).stem
            info["warnings"].append("metadata read failure")
        else:
            if audio.info:
                track.duration = getattr(audio.info, "length", 0.0) or 0.0
                br = getattr(audio.info, "bitrate", 0) or 0
                track.bitrate = br // 1000 if br > 1000 else br
                track.sample_rate = getattr(audio.info, "sample_rate", 0) or 0
                track.channels = getattr(audio.info, "channels", 2) or 2

            if ext == ".mp3" or ext in (".aiff", ".aif"):
                tags = _extract_id3_tags(audio)
            elif ext in (".flac", ".ogg", ".opus"):
                tags = _extract_vorbis_tags(audio)
                if is_video:
                    tags.update(_extract_generic_video_tags(audio))
            elif ext in (".mp4", ".m4v", ".m4a", ".aac", ".alac", ".mov"):
                tags = _extract_mp4_tags(audio)
                if is_video:
                    for key, value in _extract_generic_tags(audio).items():
                        if tags.get(key) in (None, "", 0) and value not in (None, "", 0):
                            tags[key] = value
                    tags.update(_extract_mp4_video_tags(audio))
            elif is_video:
                tags = _extract_generic_tags(audio)
                tags.update(_extract_generic_video_tags(audio))
            else:
                tags = _extract_generic_tags(audio)
            container_metadata_found = any(
                tags.get(key) not in (None, "", 0)
                for key in (
                    "title", "artist", "album", "album_artist", "genre", "year",
                    "track_number", "track_total", "disc_total", "composer", "comment",
                    "show_title", "season_number", "episode_number", "video_kind",
                )
            )
            for key, val in tags.items():
                if val is not None and val != "":
                    setattr(track, key, val)

            track.has_embedded_artwork = 1 if has_embedded_artwork(filepath) else 0
            info["parsed_ok"] = True

    except Exception as e:
        logger.warning("Failed to read metadata from %s: %s", filepath, e)
        info["warnings"].append("metadata read failure")
        track.title = Path(filepath).stem

    if is_video:
        _apply_video_path_fallback(track, filepath)

    if not track.title:
        track.title = Path(filepath).stem

    # Compute metadata fingerprint for sync change-detection
    track.recompute_metadata_hash()

    # Compute artwork hash separately so broken artwork never blocks indexing.
    try:
        art_data, _ = artwork_extractor_func(filepath)
        track.artwork_hash = compute_artwork_hash(art_data)
    except Exception as e:
        logger.debug("Artwork extraction failed for %s: %s", filepath, e)
        info["warnings"].append("artwork extraction failure")
        track.artwork_hash = ""

    if is_video:
        if not container_metadata_found:
            info["warnings"].append("basic metadata only")
    elif not track.artist and not track.album and not track.album_artist:
        info["warnings"].append("basic metadata only")

    return track, info


def read_metadata(filepath):
    """Read metadata from an audio file. Returns a Track object."""
    track, _info = read_metadata_details(filepath)

    return track

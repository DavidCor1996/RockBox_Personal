"""Write editable media metadata back to source files."""

from pathlib import Path

import mutagen
from mutagen.id3 import COMM, TALB, TCOM, TCON, TDRC, TIT2, TPE1, TPE2, TPOS, TRCK, TCMP


FILE_TAG_METADATA_FIELDS = {
    "title",
    "artist",
    "album",
    "album_artist",
    "genre",
    "year",
    "track_number",
    "track_total",
    "disc_number",
    "disc_total",
    "composer",
    "comment",
    "compilation",
    "show_title",
    "season_number",
    "episode_number",
}

LIBRARY_ONLY_METADATA_FIELDS = {
    "rating",
    "play_count",
    "last_played",
    "date_added",
    "video_kind",
}


class MetadataWriteError(RuntimeError):
    """Raised when metadata could not be written back to the media file."""


def _text(value):
    return str(value or "").strip()


def _number(value):
    try:
        return int(value or 0)
    except (TypeError, ValueError):
        return 0


def _track_position(number, total):
    num = _number(number)
    tot = _number(total)
    if not num and not tot:
        return ""
    if tot:
        return f"{num}/{tot}"
    return str(num)


def _ensure_tags(audio):
    if getattr(audio, "tags", None) is None and hasattr(audio, "add_tags"):
        audio.add_tags()
    tags = getattr(audio, "tags", None)
    if tags is None:
        raise MetadataWriteError("File has no writable tag block")
    return tags


def _set_id3_text(tags, frame_id, frame_type, value):
    tags.delall(frame_id)
    text = _text(value)
    if text:
        tags.add(frame_type(encoding=3, text=[text]))


def _set_id3_comment(tags, value):
    tags.delall("COMM")
    text = _text(value)
    if text:
        tags.add(COMM(encoding=3, lang="eng", desc="", text=[text]))


def _write_id3_tags(audio, updates):
    tags = _ensure_tags(audio)
    for field, frame_id, frame_type in (
        ("title", "TIT2", TIT2),
        ("artist", "TPE1", TPE1),
        ("album", "TALB", TALB),
        ("album_artist", "TPE2", TPE2),
        ("genre", "TCON", TCON),
        ("year", "TDRC", TDRC),
        ("composer", "TCOM", TCOM),
    ):
        if field in updates:
            _set_id3_text(tags, frame_id, frame_type, updates[field])
    if "track_number" in updates or "track_total" in updates:
        current_number, current_total = _id3_position(tags, "TRCK")
        _set_id3_text(
            tags,
            "TRCK",
            TRCK,
            _track_position(
                updates.get("track_number", current_number),
                updates.get("track_total", current_total),
            ),
        )
    if "disc_number" in updates or "disc_total" in updates:
        current_number, current_total = _id3_position(tags, "TPOS")
        _set_id3_text(
            tags,
            "TPOS",
            TPOS,
            _track_position(
                updates.get("disc_number", current_number),
                updates.get("disc_total", current_total),
            ),
        )
    if "comment" in updates:
        _set_id3_comment(tags, updates["comment"])
    if "compilation" in updates:
        _set_id3_text(tags, "TCMP", TCMP, "1" if _number(updates["compilation"]) else "")


def _id3_position(tags, frame_id):
    frames = tags.getall(frame_id)
    if not frames or not getattr(frames[0], "text", None):
        return 0, 0
    parts = str(frames[0].text[0]).split("/", 1)
    return _number(parts[0]), _number(parts[1] if len(parts) > 1 else 0)


def _set_mapping_text(tags, key, value):
    text = _text(value)
    if text:
        tags[key] = [text]
    else:
        tags.pop(key, None)


def _write_vorbis_tags(audio, updates):
    tags = _ensure_tags(audio)
    for field, key in (
        ("title", "title"),
        ("artist", "artist"),
        ("album", "album"),
        ("album_artist", "albumartist"),
        ("genre", "genre"),
        ("year", "date"),
        ("composer", "composer"),
        ("comment", "comment"),
    ):
        if field in updates:
            _set_mapping_text(tags, key, updates[field])
    for field, key in (
        ("track_number", "tracknumber"),
        ("track_total", "tracktotal"),
        ("disc_number", "discnumber"),
        ("disc_total", "disctotal"),
    ):
        if field in updates:
            _set_mapping_text(tags, key, _number(updates[field]) or "")
    if "compilation" in updates:
        _set_mapping_text(tags, "compilation", "1" if _number(updates["compilation"]) else "")


def _set_mp4_text(tags, key, value):
    text = _text(value)
    if text:
        tags[key] = [text]
    else:
        tags.pop(key, None)


def _set_mp4_int(tags, key, value):
    number = _number(value)
    if number:
        tags[key] = [number]
    else:
        tags.pop(key, None)


def _set_mp4_pair(tags, key, number, total):
    num = _number(number)
    tot = _number(total)
    if num or tot:
        tags[key] = [(num, tot)]
    else:
        tags.pop(key, None)


def _write_mp4_tags(audio, updates):
    tags = _ensure_tags(audio)
    for field, key in (
        ("title", "\xa9nam"),
        ("artist", "\xa9ART"),
        ("album", "\xa9alb"),
        ("album_artist", "aART"),
        ("genre", "\xa9gen"),
        ("year", "\xa9day"),
        ("composer", "\xa9wrt"),
        ("comment", "\xa9cmt"),
        ("show_title", "tvsh"),
    ):
        if field in updates:
            _set_mp4_text(tags, key, updates[field])
    if "track_number" in updates or "track_total" in updates:
        current_number, current_total = _mp4_pair(tags, "trkn")
        _set_mp4_pair(
            tags,
            "trkn",
            updates.get("track_number", current_number),
            updates.get("track_total", current_total),
        )
    if "disc_number" in updates or "disc_total" in updates:
        current_number, current_total = _mp4_pair(tags, "disk")
        _set_mp4_pair(
            tags,
            "disk",
            updates.get("disc_number", current_number),
            updates.get("disc_total", current_total),
        )
    if "compilation" in updates:
        if _number(updates["compilation"]):
            tags["cpil"] = True
        else:
            tags.pop("cpil", None)
    if "season_number" in updates:
        _set_mp4_int(tags, "tvsn", updates["season_number"])
    if "episode_number" in updates:
        _set_mp4_int(tags, "tves", updates["episode_number"])


def _mp4_pair(tags, key):
    value = tags.get(key)
    if isinstance(value, list) and value:
        value = value[0]
    if isinstance(value, (tuple, list)):
        number = value[0] if value else 0
        total = value[1] if len(value) > 1 else 0
        return _number(number), _number(total)
    return 0, 0


def write_track_metadata_to_file(filepath, updates, mutagen_file_func=None):
    """Persist editable tag fields to the media file on disk."""
    filepath = str(filepath or "")
    tag_updates = {
        key: value
        for key, value in (updates or {}).items()
        if key in FILE_TAG_METADATA_FIELDS
    }
    if not tag_updates:
        return set()

    mutagen_file_func = mutagen_file_func or mutagen.File
    audio = mutagen_file_func(filepath)
    if audio is None:
        raise MetadataWriteError("Unsupported or unreadable media file")

    ext = Path(filepath).suffix.lower()
    try:
        if ext in {".mp3", ".aiff", ".aif"}:
            _write_id3_tags(audio, tag_updates)
        elif ext in {".flac", ".ogg", ".opus"}:
            _write_vorbis_tags(audio, tag_updates)
        elif ext in {".mp4", ".m4a", ".m4v", ".mov", ".alac"}:
            _write_mp4_tags(audio, tag_updates)
        else:
            raise MetadataWriteError(f"Writing tags for {ext or 'this format'} is not supported yet")
        audio.save()
    except MetadataWriteError:
        raise
    except Exception as exc:
        raise MetadataWriteError(str(exc)) from exc

    return set(tag_updates)

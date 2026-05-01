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
    _set_id3_text(tags, "TIT2", TIT2, updates.get("title"))
    _set_id3_text(tags, "TPE1", TPE1, updates.get("artist"))
    _set_id3_text(tags, "TALB", TALB, updates.get("album"))
    _set_id3_text(tags, "TPE2", TPE2, updates.get("album_artist"))
    _set_id3_text(tags, "TCON", TCON, updates.get("genre"))
    _set_id3_text(tags, "TDRC", TDRC, updates.get("year"))
    _set_id3_text(
        tags,
        "TRCK",
        TRCK,
        _track_position(updates.get("track_number"), updates.get("track_total")),
    )
    _set_id3_text(
        tags,
        "TPOS",
        TPOS,
        _track_position(updates.get("disc_number"), updates.get("disc_total")),
    )
    _set_id3_text(tags, "TCOM", TCOM, updates.get("composer"))
    _set_id3_comment(tags, updates.get("comment"))
    _set_id3_text(tags, "TCMP", TCMP, "1" if _number(updates.get("compilation")) else "")


def _set_mapping_text(tags, key, value):
    text = _text(value)
    if text:
        tags[key] = [text]
    else:
        tags.pop(key, None)


def _write_vorbis_tags(audio, updates):
    tags = _ensure_tags(audio)
    _set_mapping_text(tags, "title", updates.get("title"))
    _set_mapping_text(tags, "artist", updates.get("artist"))
    _set_mapping_text(tags, "album", updates.get("album"))
    _set_mapping_text(tags, "albumartist", updates.get("album_artist"))
    _set_mapping_text(tags, "genre", updates.get("genre"))
    _set_mapping_text(tags, "date", updates.get("year"))
    _set_mapping_text(tags, "tracknumber", _number(updates.get("track_number")) or "")
    _set_mapping_text(tags, "tracktotal", _number(updates.get("track_total")) or "")
    _set_mapping_text(tags, "discnumber", _number(updates.get("disc_number")) or "")
    _set_mapping_text(tags, "disctotal", _number(updates.get("disc_total")) or "")
    _set_mapping_text(tags, "composer", updates.get("composer"))
    _set_mapping_text(tags, "comment", updates.get("comment"))
    _set_mapping_text(tags, "compilation", "1" if _number(updates.get("compilation")) else "")


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
    _set_mp4_text(tags, "\xa9nam", updates.get("title"))
    _set_mp4_text(tags, "\xa9ART", updates.get("artist"))
    _set_mp4_text(tags, "\xa9alb", updates.get("album"))
    _set_mp4_text(tags, "aART", updates.get("album_artist"))
    _set_mp4_text(tags, "\xa9gen", updates.get("genre"))
    _set_mp4_text(tags, "\xa9day", updates.get("year"))
    _set_mp4_text(tags, "\xa9wrt", updates.get("composer"))
    _set_mp4_text(tags, "\xa9cmt", updates.get("comment"))
    _set_mp4_pair(tags, "trkn", updates.get("track_number"), updates.get("track_total"))
    _set_mp4_pair(tags, "disk", updates.get("disc_number"), updates.get("disc_total"))
    if _number(updates.get("compilation")):
        tags["cpil"] = True
    else:
        tags.pop("cpil", None)
    _set_mp4_text(tags, "tvsh", updates.get("show_title"))
    _set_mp4_int(tags, "tvsn", updates.get("season_number"))
    _set_mp4_int(tags, "tves", updates.get("episode_number"))


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

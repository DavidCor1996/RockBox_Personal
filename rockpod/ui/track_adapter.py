"""Normalize mixed track row shapes for UI widgets."""


UI_TRACK_DEFAULTS = {
    "id": None,
    "media_type": "audio",
    "video_kind": "",
    "file_path": "",
    "device_path": "",
    "title": "",
    "artist": "",
    "album": "",
    "album_artist": "",
    "show_title": "",
    "genre": "",
    "year": None,
    "season_number": None,
    "episode_number": None,
    "track_number": None,
    "disc_number": 1,
    "duration": 0.0,
    "bitrate": 0,
    "codec": "",
    "rating": 0,
    "file_size": 0,
    "synced_to_device": False,
    "artwork_path": None,
    "has_embedded_artwork": 0,
    "metadata_hash": "",
    "file_hash": "",
}


def normalize_track_for_ui(track):
    """Return a dict with the minimum fields every UI track view expects."""
    data = dict(UI_TRACK_DEFAULTS)

    if track is None:
        return data

    if hasattr(track, "keys"):
        source = dict(track)
    elif hasattr(track, "__dict__"):
        source = {
            key: value
            for key, value in vars(track).items()
            if not key.startswith("_")
        }
    elif isinstance(track, dict):
        source = track
    else:
        source = {}

    data.update(source)
    data["file_path"] = data.get("file_path") or ""
    data["title"] = data.get("title") or ""
    data["artist"] = data.get("artist") or ""
    data["album"] = data.get("album") or ""
    data["duration"] = data.get("duration") or 0.0
    data["synced_to_device"] = bool(data.get("synced_to_device", False))
    return data


def normalize_tracks_for_ui(tracks):
    return [normalize_track_for_ui(track) for track in tracks]

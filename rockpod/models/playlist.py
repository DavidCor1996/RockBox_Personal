"""Playlist data model."""

import json
from dataclasses import dataclass, asdict
from typing import Optional, List


@dataclass
class Playlist:
    """Represents a standard or smart playlist."""

    id: Optional[int] = None
    name: str = ""
    is_smart: bool = False
    rules_json: Optional[str] = None
    sync_to_rockbox: bool = True
    date_created: Optional[str] = None
    date_modified: Optional[str] = None

    @property
    def rules(self):
        if self.rules_json:
            try:
                return json.loads(self.rules_json)
            except json.JSONDecodeError:
                pass
        return {}

    @rules.setter
    def rules(self, value):
        self.rules_json = json.dumps(value)

    def to_dict(self):
        d = asdict(self)
        d["is_smart"] = int(d["is_smart"])
        d["sync_to_rockbox"] = int(d["sync_to_rockbox"])
        if d.get("id") is None:
            del d["id"]
        return d

    @classmethod
    def from_row(cls, row):
        if row is None:
            return None
        data = dict(row)
        data["is_smart"] = bool(data.get("is_smart", 0))
        data["sync_to_rockbox"] = bool(data.get("sync_to_rockbox", 1))
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})


@dataclass
class SmartPlaylistRule:
    """A single rule for a smart playlist."""

    field: str = "artist"
    operator: str = "contains"
    value: str = ""

    OPERATORS = [
        "contains", "does_not_contain", "is", "is_not",
        "starts_with", "ends_with",
        "greater_than", "less_than", "greater_than_or_equal",
        "less_than_or_equal", "within_last_days", "older_than_days",
        "is_true", "is_false", "equals", "not_equals",
    ]

    FIELDS = [
        "title", "artist", "album", "album_artist", "genre",
        "year", "rating", "play_count", "bitrate", "codec",
        "duration", "date_added", "last_played", "play_time",
        "on_ipod", "not_on_ipod",
    ]

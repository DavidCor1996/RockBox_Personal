"""Smart playlist persistence and evaluation."""

import json
from datetime import datetime, timedelta, timezone


DEFAULT_SMART_PLAYLISTS = [
    {
        "name": "Most Played",
        "rules": {
            "match": "all",
            "order_by": "effective_play_count DESC, artist, album, track_number",
            "rules": [{"field": "play_count", "operator": "greater_than", "value": 0}],
        },
    },
    {
        "name": "Most Played on iPod",
        "rules": {
            "match": "all",
            "order_by": "ipod_play_count DESC, effective_last_played DESC, artist, album, track_number",
            "rules": [
                {"field": "on_ipod", "operator": "is_true", "value": "__current__"},
                {"field": "play_count_ipod", "operator": "greater_than", "value": 0},
            ],
        },
    },
    {
        "name": "Most Played on Desktop",
        "rules": {
            "match": "all",
            "order_by": "desktop_play_count DESC, artist, album, track_number",
            "rules": [{"field": "play_count_desktop", "operator": "greater_than", "value": 0}],
        },
    },
    {
        "name": "Least Played",
        "rules": {
            "match": "all",
            "order_by": "effective_play_count ASC, artist, album, track_number",
            "rules": [{"field": "play_count", "operator": "greater_than", "value": 0}],
        },
    },
    {
        "name": "Never Played",
        "rules": {"match": "all", "rules": [{"field": "play_count", "operator": "equals", "value": 0}]},
    },
    {
        "name": "Never Played on iPod",
        "rules": {
            "match": "all",
            "rules": [
                {"field": "on_ipod", "operator": "is_true", "value": "__current__"},
                {"field": "play_count", "operator": "equals", "value": 0},
            ],
        },
    },
    {
        "name": "Recently Played",
        "rules": {
            "match": "all",
            "order_by": "effective_last_played DESC, artist, album, track_number",
            "rules": [
                {"field": "last_played", "operator": "within_last_days", "value": 14},
                {"field": "play_count", "operator": "greater_than", "value": 0},
            ],
        },
    },
    {
        "name": "Recently Played on iPod",
        "rules": {
            "match": "all",
            "order_by": "ipod_last_played DESC, artist, album, track_number",
            "rules": [
                {"field": "on_ipod", "operator": "is_true", "value": "__current__"},
                {"field": "last_played_ipod", "operator": "within_last_days", "value": 30},
                {"field": "play_count_ipod", "operator": "greater_than", "value": 0},
            ],
        },
    },
    {
        "name": "Played 10+ Times",
        "rules": {
            "match": "all",
            "order_by": "combined_play_count DESC, artist, album",
            "rules": [{"field": "play_count_combined", "operator": "greater_than_or_equal", "value": 10}],
        },
    },
    {
        "name": "Not Played In 30 Days",
        "rules": {
            "match": "all",
            "rules": [
                {"field": "play_count", "operator": "greater_than", "value": 0},
                {"field": "last_played", "operator": "older_than_days", "value": 30},
            ],
        },
    },
    {
        "name": "Highest Rated",
        "rules": {
            "match": "all",
            "order_by": "effective_rating DESC, effective_play_count DESC, artist, album",
            "rules": [{"field": "rating", "operator": "greater_than_or_equal", "value": 4}],
        },
    },
    {
        "name": "Rating >= 3",
        "rules": {
            "match": "all",
            "order_by": "effective_rating DESC, artist, album",
            "rules": [{"field": "rating", "operator": "greater_than_or_equal", "value": 3}],
        },
    },
    {
        "name": "Recently Added and Unplayed",
        "rules": {
            "match": "all",
            "order_by": "date_added DESC, artist, album, track_number",
            "rules": [
                {"field": "date_added", "operator": "within_last_days", "value": 30},
                {"field": "play_count", "operator": "equals", "value": 0},
            ],
        },
    },
    {
        "name": "On iPod and Played Recently",
        "rules": {
            "match": "all",
            "order_by": "effective_last_played DESC, artist, album",
            "rules": [
                {"field": "on_ipod", "operator": "is_true", "value": "__current__"},
                {"field": "last_played", "operator": "within_last_days", "value": 14},
            ],
        },
    },
    {
        "name": "Not On iPod and Frequently Played",
        "rules": {
            "match": "all",
            "order_by": "combined_play_count DESC, artist, album",
            "rules": [
                {"field": "not_on_ipod", "operator": "is_true", "value": "__current__"},
                {"field": "play_count_combined", "operator": "greater_than_or_equal", "value": 5},
            ],
        },
    },
    {
        "name": "Smart Sync Suggestions",
        "rules": {
            "match": "all",
            "order_by": "combined_play_count DESC, date_added DESC, artist, album",
            "rules": [
                {"field": "not_on_ipod", "operator": "is_true", "value": "__current__"},
                {"field": "date_added", "operator": "within_last_days", "value": 90},
                {"field": "play_count_combined", "operator": "greater_than_or_equal", "value": 2},
            ],
        },
    },
]


def _json_rules(rules):
    return json.dumps(rules, sort_keys=True)


def ensure_default_smart_playlists(db):
    existing = {
        row["name"]: row
        for row in db.fetchall("SELECT id, name, rules_json FROM playlists WHERE is_smart = 1")
    }
    created = 0
    for spec in DEFAULT_SMART_PLAYLISTS:
        rules_json = _json_rules(spec["rules"])
        current = existing.get(spec["name"])
        if current is None:
            db.create_playlist(spec["name"], is_smart=True, rules_json=rules_json)
            created += 1
        elif (current["rules_json"] or "") != rules_json:
            db.execute(
                "UPDATE playlists SET rules_json = ?, date_modified = datetime('now') "
                "WHERE id = ?",
                (rules_json, current["id"]),
            )
    return created


def parse_rules(playlist_row):
    if playlist_row is None:
        raw = None
    elif hasattr(playlist_row, "get"):
        raw = playlist_row.get("rules_json")
    elif hasattr(playlist_row, "keys"):
        raw = playlist_row["rules_json"] if "rules_json" in playlist_row.keys() else None
    else:
        raw = None
    if not raw:
        return {"match": "all", "rules": []}
    try:
        parsed = json.loads(raw)
    except json.JSONDecodeError:
        return {"match": "all", "rules": []}
    parsed.setdefault("match", "all")
    parsed.setdefault("rules", [])
    return parsed


def evaluate_playlist(db, playlist_row, device_id=None):
    rules = parse_rules(playlist_row)
    return evaluate_rules(db, rules, device_id=device_id)


def evaluate_playlist_count(db, playlist_row, device_id=None):
    rules = parse_rules(playlist_row)
    return evaluate_rules_count(db, rules, device_id=device_id)


def evaluate_rules(db, rules, device_id=None):
    match_mode = "OR" if str(rules.get("match", "all")).lower() == "any" else "AND"
    order_by = _sanitize_order_by(rules.get("order_by") or "artist, album, disc_number, track_number")
    clauses = []
    params = []
    for rule in rules.get("rules", []):
        clause, clause_params = _rule_to_sql(rule, device_id=device_id)
        if clause:
            clauses.append(clause)
            params.extend(clause_params)
    where_sql = f" {match_mode} ".join(f"({clause})" for clause in clauses)
    return db.smart_playlist_query(where_sql, params, order_by=order_by)


def evaluate_rules_count(db, rules, device_id=None):
    match_mode = "OR" if str(rules.get("match", "all")).lower() == "any" else "AND"
    clauses = []
    params = []
    for rule in rules.get("rules", []):
        clause, clause_params = _rule_to_sql(rule, device_id=device_id)
        if clause:
            clauses.append(clause)
            params.extend(clause_params)
    where_sql = f" {match_mode} ".join(f"({clause})" for clause in clauses)
    return db.smart_playlist_count_query(where_sql, params)


def _sanitize_order_by(order_by):
    allowed = {
        "artist", "album", "title", "genre", "year", "track_number", "disc_number",
        "date_added", "effective_play_count", "effective_last_played",
        "effective_rating", "effective_play_time_seconds", "desktop_play_count",
        "ipod_play_count", "combined_play_count", "desktop_last_played", "ipod_last_played",
    }
    parts = []
    for raw in str(order_by).split(","):
        token = raw.strip()
        if not token:
            continue
        fields = token.split()
        field = fields[0]
        direction = fields[1].upper() if len(fields) > 1 else "ASC"
        if field in allowed and direction in ("ASC", "DESC"):
            parts.append(f"{field} {direction}")
    return ", ".join(parts) if parts else "artist ASC, album ASC, disc_number ASC, track_number ASC"


def _rule_to_sql(rule, device_id=None):
    field = str(rule.get("field") or "").strip()
    operator = str(rule.get("operator") or "").strip()
    value = rule.get("value")
    expr = _field_expr(field, device_id=device_id, value=value)
    if not expr:
        return "", []
    if field in ("on_ipod", "not_on_ipod"):
        return _device_presence_sql(field, operator, value, device_id)
    if operator == "contains":
        return f"{expr} LIKE ?", [f"%{value}%"]
    if operator == "does_not_contain":
        return f"{expr} NOT LIKE ?", [f"%{value}%"]
    if operator == "equals":
        return f"{expr} = ?", [value]
    if operator == "not_equals":
        return f"{expr} != ?", [value]
    if operator == "starts_with":
        return f"{expr} LIKE ?", [f"{value}%"]
    if operator == "ends_with":
        return f"{expr} LIKE ?", [f"%{value}"]
    if operator == "greater_than":
        return f"{expr} > ?", [value]
    if operator == "less_than":
        return f"{expr} < ?", [value]
    if operator == "greater_than_or_equal":
        return f"{expr} >= ?", [value]
    if operator == "less_than_or_equal":
        return f"{expr} <= ?", [value]
    if operator == "within_last_days":
        cutoff = _days_ago_iso(value)
        return f"{expr} >= ?", [cutoff]
    if operator == "older_than_days":
        cutoff = _days_ago_iso(value)
        return f"({expr} != '' AND {expr} < ?)", [cutoff]
    if operator == "is_true":
        return f"{expr} = 1", []
    if operator == "is_false":
        return f"{expr} = 0", []
    return "", []


def _field_expr(field, device_id=None, value=None):
    if field == "title":
        return "t.title"
    if field == "artist":
        return "t.artist"
    if field == "album":
        return "t.album"
    if field == "album_artist":
        return "t.album_artist"
    if field == "genre":
        return "t.genre"
    if field == "year":
        return "COALESCE(t.year, 0)"
    if field == "rating":
        return "COALESCE(rt.runtime_rating, t.rating, 0)"
    if field == "play_count":
        return "COALESCE(rt.runtime_play_count, t.play_count, 0)"
    if field == "play_count_desktop":
        return "COALESCE(t.play_count, 0)"
    if field == "play_count_ipod":
        return "COALESCE(rt.runtime_play_count, 0)"
    if field == "play_count_combined":
        return "(COALESCE(t.play_count, 0) + COALESCE(rt.runtime_play_count, 0))"
    if field == "play_time":
        return "COALESCE(rt.runtime_play_time_seconds, 0)"
    if field == "last_played":
        return "COALESCE(NULLIF(rt.runtime_last_played, ''), t.last_played, '')"
    if field == "last_played_desktop":
        return "COALESCE(t.last_played, '')"
    if field == "last_played_ipod":
        return "COALESCE(NULLIF(rt.runtime_last_played, ''), '')"
    if field == "date_added":
        return "COALESCE(t.date_added, '')"
    if field == "on_ipod":
        return "1"
    if field == "not_on_ipod":
        return "1"
    return ""


def _device_presence_sql(field, operator, value, device_id):
    resolved_device = device_id
    if isinstance(value, str) and value and value != "__current__":
        resolved_device = value
    if not resolved_device:
        return ("0 = 1", []) if field == "on_ipod" else ("1 = 1", [])
    exists_sql = (
        "EXISTS (SELECT 1 FROM device_tracks dt "
        "WHERE dt.local_track_id = t.id AND dt.device_id = ? "
        "AND COALESCE(dt.present_on_device, 1) = 1)"
    )
    if field == "on_ipod":
        return exists_sql, [resolved_device]
    return f"NOT {exists_sql}", [resolved_device]


def _days_ago_iso(days):
    try:
        delta = int(days)
    except (TypeError, ValueError):
        delta = 0
    cutoff = datetime.now(timezone.utc) - timedelta(days=max(0, delta))
    return cutoff.replace(microsecond=0).isoformat()

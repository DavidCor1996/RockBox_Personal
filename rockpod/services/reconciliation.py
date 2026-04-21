"""Diagnostic reconciliation reports for local vs device inventories."""

from services.track_matcher import TrackMatcher


def build_reconciliation_report(
    db,
    device_key=None,
    strictness="metadata_and_hash",
    duration_tolerance=2.0,
    sample_limit=25,
    skipped=None,
):
    """Compare local and device libraries and return diagnostic counts/examples."""
    local_tracks = [dict(row) for row in db.get_all_tracks()]
    device_tracks = [
        dict(row) for row in db.get_all_device_tracks(device_key, present_only=True)
    ]
    matcher = TrackMatcher(strictness, duration_tolerance)
    matched, unmatched, orphaned, resync = matcher.match_all(local_tracks, device_tracks)

    examples = []
    for result in unmatched[:sample_limit]:
        row = dict(result.local_track)
        examples.append(
            {
                "title": row.get("title", ""),
                "artist": row.get("artist", ""),
                "album": row.get("album", ""),
                "file_path": row.get("file_path", ""),
                "reason": matcher.explain_unmatched(row, device_tracks),
            }
        )

    return {
        "local_track_count": len(local_tracks),
        "device_track_count": len(device_tracks),
        "matched_count": len(matched),
        "unmatched_count": len(unmatched),
        "orphaned_count": len(orphaned),
        "resync_count": len(resync),
        "unmatched_examples": examples,
        "skipped_device_files": list(skipped or [])[:sample_limit],
    }


MISSING_TAG_FIELDS = (
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
)


def _normalized_codec(value):
    return str(value or "").strip().upper()


def _is_missing_tag_value(field, value):
    if field == "disc_number":
        return value in (None, 0, "")
    return value in (None, "")


def build_missing_tag_report(
    db,
    codecs=("FLAC", "AIFF"),
    fields=MISSING_TAG_FIELDS,
    sample_limit=200,
):
    """Report missing non-artwork tags for tracks in selected codecs."""
    codec_filter = {_normalized_codec(codec) for codec in codecs}
    tracks = [
        dict(row)
        for row in db.get_all_tracks(order_by="codec, artist, album, disc_number, track_number")
        if _normalized_codec(dict(row).get("codec")) in codec_filter
    ]

    missing_by_field = {field: [] for field in fields}
    missing_examples = []
    per_codec_counts = {codec: 0 for codec in sorted(codec_filter)}

    for row in tracks:
        codec = _normalized_codec(row.get("codec"))
        row_missing = []
        for field in fields:
            if _is_missing_tag_value(field, row.get(field)):
                missing_by_field[field].append(row)
                row_missing.append(field)
        if row_missing:
            per_codec_counts[codec] = per_codec_counts.get(codec, 0) + 1
            if len(missing_examples) < sample_limit:
                missing_examples.append(
                    {
                        "file_path": row.get("file_path", ""),
                        "codec": codec,
                        "title": row.get("title", ""),
                        "artist": row.get("artist", ""),
                        "album": row.get("album", ""),
                        "missing_fields": row_missing,
                    }
                )

    missing_field_counts = {
        field: len(rows)
        for field, rows in missing_by_field.items()
    }

    return {
        "codecs": sorted(codec_filter),
        "fields": list(fields),
        "track_count": len(tracks),
        "tracks_with_missing_tags_count": sum(1 for row in tracks if any(
            _is_missing_tag_value(field, row.get(field)) for field in fields
        )),
        "tracks_with_missing_tags_by_codec": per_codec_counts,
        "missing_field_counts": missing_field_counts,
        "tracks_with_missing_tags": missing_examples,
    }


def format_reconciliation_report(report):
    """Format a reconciliation report for terminal/debug output."""
    lines = [
        "RockPod reconciliation report",
        f"Local tracks: {report['local_track_count']}",
        f"Scanned device tracks: {report['device_track_count']}",
        f"Matched: {report['matched_count']}",
        f"Unmatched / Not On iPod: {report['unmatched_count']}",
        f"Device-only/orphaned: {report['orphaned_count']}",
        f"Resync needed: {report['resync_count']}",
    ]

    examples = report.get("unmatched_examples") or []
    if examples:
        lines.append("")
        lines.append("Unmatched examples:")
        for item in examples:
            label = " - ".join(
                part for part in (item["artist"], item["album"], item["title"]) if part
            )
            lines.append(f"- {label or item['file_path']}: {item['reason']}")

    skipped = report.get("skipped_device_files") or []
    if skipped:
        lines.append("")
        lines.append("Skipped device files:")
        for item in skipped:
            lines.append(f"- {item.get('path', '')}: {item.get('reason', '')}")

    return "\n".join(lines)


def format_missing_tag_report(report):
    """Format a missing-tag audit for UI/debug output."""
    lines = [
        "RockPod missing tag report",
        f"Codecs: {', '.join(report.get('codecs') or [])}",
        f"Tracks scanned: {report.get('track_count', 0)}",
        f"Tracks with missing tags: {report.get('tracks_with_missing_tags_count', 0)}",
    ]

    by_codec = report.get("tracks_with_missing_tags_by_codec") or {}
    if by_codec:
        lines.append("")
        lines.append("Missing-tag tracks by codec:")
        for codec in sorted(by_codec):
            lines.append(f"- {codec}: {by_codec[codec]}")

    field_counts = report.get("missing_field_counts") or {}
    nonzero_fields = [(field, count) for field, count in field_counts.items() if count]
    if nonzero_fields:
        lines.append("")
        lines.append("Missing fields:")
        for field, count in nonzero_fields:
            lines.append(f"- {field}: {count}")

    examples = report.get("tracks_with_missing_tags") or []
    if examples:
        lines.append("")
        lines.append("Examples:")
        for item in examples:
            label = " - ".join(
                part for part in (item.get("artist", ""), item.get("album", ""), item.get("title", "")) if part
            )
            fields_text = ", ".join(item.get("missing_fields") or [])
            lines.append(f"- {label or item.get('file_path', '')} [{item.get('codec', '')}]: {fields_text}")

    if not nonzero_fields:
        lines.append("")
        lines.append("No missing FLAC/AIFF tags were found.")

    return "\n".join(lines)

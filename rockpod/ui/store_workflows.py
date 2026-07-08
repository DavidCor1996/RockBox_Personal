"""UI workflow helpers for store import operations."""

from __future__ import annotations


def store_import_failure_message(exit_code, output_parts, log_path):
    detail = " ".join(str(part or "").strip() for part in (output_parts or []) if str(part or "").strip())
    if detail:
        return f"streamrip failed with exit code {exit_code}: {detail[-600:]}\nLog: {log_path}"
    return f"streamrip failed with exit code {exit_code}\nLog: {log_path}"


def store_import_empty_message(output_parts, log_path):
    detail = " ".join(str(part or "").strip() for part in (output_parts or []) if str(part or "").strip())
    if detail:
        return f"No new audio files detected. Last streamrip output: {detail[-600:]}\nLog: {log_path}"
    return (
        "No new audio files detected. Check Tidal/Qobuz auth and whether the album is "
        f"available in your region.\nLog: {log_path}"
    )


def store_import_success_message(imported_count, cover_count, log_path):
    return (
        f"Imported {imported_count} audio file{'s' if imported_count != 1 else ''}; "
        f"added {cover_count} Rockbox cover file{'s' if cover_count != 1 else ''}. "
        f"Refreshing library.\nLog: {log_path}"
    )

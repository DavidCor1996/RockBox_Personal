"""UI workflow helpers for Android media import results."""

from __future__ import annotations

import os


def summarize_android_import(report):
    report = dict(report or {})
    scanned = int(report.get("scanned", 0) or 0)
    imported = int(report.get("imported", 0) or 0)
    imported_photos = int(report.get("imported_photos", 0) or 0)
    imported_videos = int(report.get("imported_videos", 0) or 0)
    skipped = int(report.get("skipped_existing", 0) or 0)
    failures = list(report.get("failures") or [])
    cancelled = bool(report.get("cancelled"))
    device_subdir = report.get("device_subdir", "")
    for_tiktok_plugin = bool(report.get("for_tiktok_plugin"))
    feed_entries = int(report.get("feed_entries", 0) or 0)

    if scanned == 0 and not failures:
        return {
            "empty": True,
            "warning": False,
            "status_text": "No Android media found",
            "dialog_text": "No photos or videos were found under the selected Android storage root.",
        }

    lines = [
        f"Scanned {scanned} Android media items.",
        f"Imported {imported} files to /{device_subdir}.",
    ]
    if imported:
        lines.append(f"Imported breakdown: {imported_photos} photos, {imported_videos} videos.")
    if skipped:
        lines.append(f"Skipped {skipped} items already present on the iPod.")
    if for_tiktok_plugin:
        lines.append(f"Updated iPodTikTok feed with {feed_entries} clips.")
    if cancelled:
        lines.append("Import was cancelled before all items finished.")
    if failures:
        lines.append("")
        lines.append("Failures:")
        for item in failures[:8]:
            lines.append(
                f"{os.path.basename(item.get('path', ''))}: "
                f"{item.get('error', 'conversion failed')}"
            )
        if len(failures) > 8:
            lines.append(f"...and {len(failures) - 8} more.")

    status_text = ""
    if imported and not cancelled:
        if for_tiktok_plugin:
            status_text = f"Imported iPodTikTok clips: {imported} files, feed now has {feed_entries} clips"
        else:
            status_text = f"Imported Android media: {imported} files ({imported_photos} photos, {imported_videos} videos)"
    elif cancelled:
        status_text = "Android import cancelled"

    return {
        "empty": False,
        "warning": bool(failures),
        "status_text": status_text,
        "dialog_text": "\n".join(lines),
    }

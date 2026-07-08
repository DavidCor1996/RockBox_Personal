from ui.android_workflows import summarize_android_import


def test_android_import_summary_reports_empty_import():
    summary = summarize_android_import({"scanned": 0})

    assert summary["empty"] is True
    assert summary["warning"] is False
    assert summary["status_text"] == "No Android media found"
    assert "No photos or videos" in summary["dialog_text"]


def test_android_import_summary_reports_normal_import():
    summary = summarize_android_import(
        {
            "scanned": 4,
            "imported": 3,
            "imported_photos": 1,
            "imported_videos": 2,
            "skipped_existing": 1,
            "device_subdir": "AndroidMedia",
        }
    )

    assert summary["empty"] is False
    assert summary["warning"] is False
    assert summary["status_text"] == "Imported Android media: 3 files (1 photos, 2 videos)"
    assert "Imported 3 files to /AndroidMedia." in summary["dialog_text"]
    assert "Skipped 1 items already present" in summary["dialog_text"]


def test_android_import_summary_reports_ipodtiktok_feed():
    summary = summarize_android_import(
        {
            "scanned": 2,
            "imported": 2,
            "imported_videos": 2,
            "device_subdir": "Videos/iPodTikTok",
            "for_tiktok_plugin": True,
            "feed_entries": 9,
        }
    )

    assert summary["status_text"] == "Imported iPodTikTok clips: 2 files, feed now has 9 clips"
    assert "Updated iPodTikTok feed with 9 clips." in summary["dialog_text"]


def test_android_import_summary_reports_failures_and_truncates_list():
    failures = [{"path": f"/phone/DCIM/item-{index}.mp4", "error": "bad codec"} for index in range(10)]

    summary = summarize_android_import({"scanned": 10, "failures": failures, "cancelled": True})

    assert summary["warning"] is True
    assert summary["status_text"] == "Android import cancelled"
    assert "Failures:" in summary["dialog_text"]
    assert "item-0.mp4: bad codec" in summary["dialog_text"]
    assert "...and 2 more." in summary["dialog_text"]

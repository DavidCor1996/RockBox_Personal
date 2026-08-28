from pathlib import Path

from services.device_sync_index import DeviceSyncIndex


def test_device_sync_index_skips_without_rechecking_outputs(config, mock_device):
    output = Path(mock_device) / ".rockbox/example.bin"
    output.write_bytes(b"ready")
    index = DeviceSyncIndex(config, mock_device)
    assert index.current_or_seed("test", "one", "media", "sig", [output], trust_existing=True)
    index.commit()
    output.unlink()
    # The indexed fast path intentionally does not touch the device output.
    assert index.current_or_seed(
        "test", "one", "media", "sig", [output], trust_existing=True
    )
    assert not index.current("test", "one", "media", "changed")
    index.close()


def test_device_sync_index_migrates_legacy_marker(config, mock_device):
    output = Path(mock_device) / ".rockbox/legacy.bmp"
    marker = Path(str(output) + ".source")
    output.write_bytes(b"bmp")
    marker.write_text("legacy-signature", encoding="ascii")
    index = DeviceSyncIndex(config, mock_device)
    assert index.current_or_seed(
        "test", "two", "thumbnail", "new-db-signature", [output],
        legacy_marker=marker, legacy_signature="legacy-signature",
    )
    index.close()


def test_device_sync_index_tracks_stale_items_and_one_time_reconcile(
    config, mock_device
):
    index = DeviceSyncIndex(config, mock_device)
    index.mark("test", "keep", "media", "one")
    index.mark("test", "remove", "media", "two")
    assert index.needs_legacy_reconcile("test")
    assert index.prune("test", {"keep"}) == ["remove"]
    index.mark_legacy_reconciled("test")
    index.commit()
    assert not index.needs_legacy_reconcile("test")
    index.close()

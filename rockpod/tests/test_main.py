"""Tests for RockPod process-level startup safety."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from main import _acquire_ui_instance_lock, _release_ui_instance_lock


def test_ui_instance_lock_allows_only_one_holder(tmp_path):
    lock_path = str(tmp_path / "rockpod-ui.lock")
    first = _acquire_ui_instance_lock(lock_path)
    assert first is not None
    try:
        assert _acquire_ui_instance_lock(lock_path) is None
    finally:
        _release_ui_instance_lock(first)

    second = _acquire_ui_instance_lock(lock_path)
    assert second is not None
    _release_ui_instance_lock(second)

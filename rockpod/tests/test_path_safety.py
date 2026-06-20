import os

from services.path_safety import is_within_root, resolve_under_root, validate_device_root


def test_resolve_under_root_accepts_normal_device_relative_path(tmp_dir):
    root = os.path.join(tmp_dir, "mock-ipod")
    os.makedirs(root, exist_ok=True)

    resolved = resolve_under_root(root, ".rockbox/themes/iPone.cfg")

    assert resolved == os.path.join(root, ".rockbox", "themes", "iPone.cfg")


def test_resolve_under_root_rejects_parent_escape(tmp_dir):
    root = os.path.join(tmp_dir, "mock-ipod")
    os.makedirs(root, exist_ok=True)

    try:
        resolve_under_root(root, "../outside.txt")
    except ValueError as exc:
        assert "escapes device root" in str(exc)
    else:
        raise AssertionError("Accepted a path outside the device root")


def test_is_within_root_handles_sibling_prefixes(tmp_dir):
    root = os.path.join(tmp_dir, "ipod")
    sibling = os.path.join(tmp_dir, "ipod-other", "file.txt")
    os.makedirs(root, exist_ok=True)
    os.makedirs(os.path.dirname(sibling), exist_ok=True)

    assert is_within_root(root, os.path.join(root, ".rockbox", "config.cfg")) is True
    assert is_within_root(root, sibling) is False


def test_validate_device_root_allows_mock_child_and_rejects_system_root(tmp_dir):
    root = os.path.join(tmp_dir, "mock-ipod")
    os.makedirs(root, exist_ok=True)

    assert validate_device_root(root) == os.path.abspath(root)

    try:
        validate_device_root(os.path.abspath(os.sep))
    except ValueError as exc:
        assert "unsafe device root" in str(exc)
    else:
        raise AssertionError("Accepted system root as a device root")

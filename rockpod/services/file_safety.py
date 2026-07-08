"""Atomic file-write helpers for generated RockPod state."""

from __future__ import annotations

import os
import json
from tempfile import NamedTemporaryFile


def atomic_write_text(path, text, encoding="utf-8"):
    """Write text by replacing the target after a complete temp-file write."""
    target = os.path.abspath(path)
    directory = os.path.dirname(target)
    os.makedirs(directory, exist_ok=True)
    tmp_name = ""
    try:
        with NamedTemporaryFile("w", encoding=encoding, dir=directory, delete=False) as handle:
            tmp_name = handle.name
            handle.write(text)
        os.replace(tmp_name, target)
    finally:
        if tmp_name and os.path.exists(tmp_name):
            try:
                os.remove(tmp_name)
            except OSError:
                pass
    return target


def atomic_write_json(path, payload, indent=2, sort_keys=True):
    """Write JSON state through the same replace-after-write path."""
    return atomic_write_text(
        path,
        json.dumps(payload, indent=indent, sort_keys=sort_keys) + "\n",
    )

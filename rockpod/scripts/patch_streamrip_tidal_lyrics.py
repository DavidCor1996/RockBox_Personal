#!/usr/bin/env python3
"""Keep Tidal track downloads authorised in the installed streamrip.

streamrip asks listen.tidal.com for lyrics while resolving each track. Tidal
answers with a 301 to tidal.com, and aiohttp drops the Authorization header
across that hop, so every track fails with 401 and an album import writes no
audio at all. This rewrites the request to name tidal.com directly, which is
where the token was already being sent before aiohttp tightened redirects.

scripts/install_streamrip.sh runs this after installing streamrip. A manual
pip install of streamrip replaces the patched file, so re-run the installer
rather than pip if the 401s come back.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from services.streamrip_import import (  # noqa: E402
    StreamripImportError,
    patch_streamrip_tidal_lyrics,
)


def main() -> int:
    spec = importlib.util.find_spec("streamrip.client.tidal")
    if spec is None or not spec.origin:
        print("streamrip is not installed in this interpreter.", file=sys.stderr)
        return 1

    source = Path(spec.origin)
    try:
        patched = patch_streamrip_tidal_lyrics(source.read_text())
    except StreamripImportError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    if patched is None:
        print(f"Tidal lyrics request already patched: {source}")
        return 0

    source.write_text(patched)
    print(f"Patched Tidal lyrics request: {source}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

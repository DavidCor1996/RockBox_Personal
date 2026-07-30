#!/usr/bin/env python3
"""Stage one native Desktop Mode host runtime from a desktop1080 build."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

from install_desktop_mode_portable import PLATFORMS, REQUIRED_PLUGINS


def build_bundle(build: Path, output: Path, platform: str) -> Path:
    build = build.expanduser().resolve()
    output = output.expanduser().resolve() / platform
    runtime_name = PLATFORMS[platform]
    runtime_source = build / runtime_name
    simdisk = build / "simdisk"
    if not runtime_source.is_file() or not simdisk.is_dir():
        raise RuntimeError(
            f"{build} is not an installed desktop1080 simulator build"
        )
    for relative in REQUIRED_PLUGINS:
        if not (simdisk / relative).is_file():
            raise RuntimeError(f"{build}: missing {relative}; run make install")

    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    shutil.copy2(runtime_source, output / runtime_name)
    if platform.startswith("macos-"):
        (output / runtime_name).chmod(0o755)

    # Keep runtime libraries beside the executable. SDL's Windows/macOS
    # packaging conventions both resolve these locations without installation.
    for pattern in ("*.dll", "*.dylib"):
        for source in sorted(build.glob(pattern)):
            shutil.copy2(source, output / source.name)

    system = output / "system-root/.rockbox"
    for name in ("rocks", "rocks.data", "fonts", "langs", "icons"):
        source = simdisk / ".rockbox" / name
        if source.is_dir():
            shutil.copytree(source, system / name)

    info = {
        "format": 1,
        "platform": platform,
        "target": "desktop1080",
        "runtime": runtime_name,
        "system_root": "system-root",
    }
    (output / "bundle-info.json").write_text(
        json.dumps(info, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--platform", choices=sorted(PLATFORMS), required=True)
    args = parser.parse_args()
    try:
        result = build_bundle(
            args.build_dir, args.output, args.platform
        )
    except (OSError, RuntimeError) as exc:
        parser.error(str(exc))
    print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

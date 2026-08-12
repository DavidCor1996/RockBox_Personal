#!/usr/bin/env python3
"""Static/build regression checks for the iPod touch 4G OpenLara port."""

from __future__ import annotations

import json
import re
import struct
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "tools/ios/OpenLaraTouch"
STAGER = ROOT / "tools/openlara_touch_stage.py"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"openlara_touch_regression: {message}")


def contains(path: Path, *needles: str) -> None:
    text = path.read_text(encoding="utf-8")
    for needle in needles:
            require(needle in text, f"{path.relative_to(ROOT)} lacks {needle!r}")


def write_test_pkd(path: Path) -> None:
    data = bytearray(228)
    counts = [0] * 14
    counts[0] = counts[1] = 1
    struct.pack_into("<14H", data, 4, *counts)
    struct.pack_into("<35I", data, 32, *([172] * 35))
    path.write_bytes(data)


def test_stager(temp: Path) -> None:
    source = temp / "converted"
    levels = source / "levels"
    levels.mkdir(parents=True)
    for index, name in enumerate(("title.pkd", "GYM.PKD", "level1.pkd",
                                  "LEVEL2.PKD")):
        write_test_pkd((source if index == 0 else levels) / name)
    (source / "title.scr").write_bytes(bytes(240 * 160))

    output = temp / "device-data"
    subprocess.check_call([str(STAGER), str(source), str(output)])
    manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
    names = [entry["name"] for entry in manifest["files"]]
    require(names == ["TITLE.PKD", "GYM.PKD", "LEVEL1.PKD", "LEVEL2.PKD",
                      "TITLE.SCR"], "stager output order or optional handling changed")
    require(all(len(entry["sha256"]) == 64 for entry in manifest["files"]),
            "stager manifest lacks SHA-256 values")

    duplicate = subprocess.run([str(STAGER), str(source), str(output)],
                               text=True, capture_output=True)
    require(duplicate.returncode != 0 and "output already exists" in duplicate.stderr,
            "stager must not overwrite an existing directory")

    invalid_source = temp / "invalid"
    invalid_source.mkdir()
    for name in ("TITLE.PKD", "GYM.PKD", "LEVEL1.PKD", "LEVEL2.PKD"):
        write_test_pkd(invalid_source / name)
    (invalid_source / "LEVEL2.PKD").write_bytes(bytes(172))
    invalid = subprocess.run([str(STAGER), str(invalid_source),
                              str(temp / "invalid-output")],
                             text=True, capture_output=True)
    require(invalid.returncode != 0 and "tile and room counts" in invalid.stderr,
            "stager accepted an invalid PKD header")


def main() -> None:
    makefile = APP / "Makefile"
    frontend = APP / "OLGameViewController.mm"
    platform = APP / "OLPlatform.c"
    info = APP / "Resources/Info.plist"
    common = ROOT / "apps/plugins/openlara/fixed/common.h"
    lara = ROOT / "apps/plugins/openlara/fixed/lara.h"

    contains(makefile, "iphone:clang:9.3:6.0", "ARCHS = armv7",
             "AudioToolbox", "-D__IOS__")
    require("GameController" not in makefile.read_text(encoding="utf-8"),
            "iOS 6 target must not link GameController.framework")
    contains(info, "<string>6.0</string>", "UIFileSharingEnabled",
             "UIInterfaceOrientationLandscapeLeft",
             "UIInterfaceOrientationLandscapeRight")
    contains(common, "defined(__IOS__)", "#define FRAME_WIDTH  320",
             "#define SND_OUTPUT_FREQ  22050")
    contains(lara, "#elif defined(__IOS__)",
             "if (keys & IK_C) input |= IN_WEAPON;",
             "if (keys & IK_Y) input |= IN_LOOK;",
             "if (keys & IK_Z) input |= IN_UP | IN_DOWN;")
    contains(frontend, "<UIKeyInput>", "ol_icade_update(",
             "ol_touch_keys(", "multipleTouchEnabled = YES",
             "AudioQueueNewOutput", "AudioQueueStop", "osSaveGame",
             "counts[10] > MAX_ITEMS", "CADisplayLink",
             "@interface OLScreenView", "[self.screenView setNeedsDisplay]",
             "audio_underruns=%lu", "worst_engine_ms=%.3f")
    contains(platform, "{'w', 'e', OL_KEY_UP}",
             "{'y', 't', OL_KEY_A}", "{'o', 'g', OL_KEY_Z}",
             "{'l', 'v', OL_KEY_SELECT}",
             "void ol_palette_rgba")
    contains(STAGER, "validate_pkd", "MAX_ROOMS = 139",
             '"engine_upstream_commit"', "os.replace(staged, output)")

    controls = set(re.findall(r'label:@"([A-Z]+)"',
                              frontend.read_text(encoding="utf-8")))
    require(controls == {"ACT", "JUMP", "GUN", "WALK", "LOOK", "ROLL"},
            f"unexpected on-screen action set: {sorted(controls)}")

    with tempfile.TemporaryDirectory(prefix="openlara-touch-test-") as temp:
        temp_path = Path(temp)
        test_binary = temp_path / "ol_platform_tests"
        subprocess.check_call([
            "cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
            "-I", str(APP), str(APP / "tests/OLPlatformTests.c"),
            str(platform), "-o", str(test_binary),
        ])
        subprocess.check_call([str(test_binary)])
        test_stager(temp_path)

    binary = APP / ".theos/obj/armv7/OpenLaraTouch.app/OpenLaraTouch"
    if binary.is_file():
        output = subprocess.check_output(["file", str(binary)], text=True)
        require("Mach-O armv7 executable" in output,
                "built application is not an armv7 Mach-O")
        otool = Path("/home/david/theos/toolchain/linux/iphone/bin/otool")
        if otool.is_file():
            loads = subprocess.check_output([str(otool), "-l", str(binary)],
                                            text=True)
            require("LC_VERSION_MIN_IPHONEOS" in loads and
                    re.search(r"\n\s+version 6\.0\n", loads) is not None,
                    "built application minimum iOS is not 6.0")

    print("OpenLara Touch regression: PASS")


if __name__ == "__main__":
    main()

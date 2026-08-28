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


def png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    require(header[:8] == b"\x89PNG\r\n\x1a\n" and
            header[12:16] == b"IHDR",
            f"{path.relative_to(ROOT)} is not a PNG")
    return struct.unpack(">II", header[16:24])


TR1_LEVELS = (
    "TITLE.PHD", "GYM.PHD", "LEVEL1.PHD", "LEVEL2.PHD", "LEVEL3A.PHD",
    "LEVEL3B.PHD", "CUT1.PHD", "LEVEL4.PHD", "LEVEL5.PHD", "LEVEL6.PHD",
    "LEVEL7A.PHD", "LEVEL7B.PHD", "CUT2.PHD", "LEVEL8A.PHD",
    "LEVEL8B.PHD", "LEVEL8C.PHD", "LEVEL10A.PHD", "CUT3.PHD",
    "LEVEL10B.PHD", "CUT4.PHD", "LEVEL10C.PHD", "EGYPT.PHD", "CAT.PHD",
    "END.PHD", "END2.PHD",
)


def write_test_phd(path: Path) -> None:
    path.write_bytes(struct.pack("<I", 0x20) + bytes(124))


def test_stager(temp: Path) -> None:
    source = temp / "tr1"
    data = source / "data"
    data.mkdir(parents=True)
    for name in TR1_LEVELS:
        write_test_phd(data / name.lower())
    (data / "titleh.pcx").write_bytes(b"PCX")
    audio = source / "Audio" / "1"
    audio.mkdir(parents=True)
    (audio / "track_02.ogg").write_bytes(b"OggS")
    fmv = source / "fmv"
    fmv.mkdir()
    (fmv / "corelogo.rpl").write_bytes(b"RPL")

    output = temp / "device-data"
    subprocess.check_call([str(STAGER), str(source), str(output)])
    manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
    names = {entry["name"] for entry in manifest["files"]}
    require({f"DATA/{name}" for name in TR1_LEVELS}.issubset(names),
            "stager omitted a full-campaign TR1 level")
    require("DATA/TITLEH.PCX" in names and "audio/1/track_02.ogg" in names and
            "FMV/CORELOGO.RPL" in names,
            "stager omitted optional title, audio, or FMV data")
    require(manifest["schema"] == 2 and manifest["engine"] == "OpenLara full",
            "stager manifest does not identify the full engine format")
    require(sum(bool(entry["required"]) for entry in manifest["files"]) == 25,
            "stager manifest required-file count is not the full TR1 campaign")
    require(all(len(entry["sha256"]) == 64 for entry in manifest["files"]),
            "stager manifest lacks SHA-256 values")

    duplicate = subprocess.run([str(STAGER), str(source), str(output)],
                               text=True, capture_output=True)
    require(duplicate.returncode != 0 and "output already exists" in duplicate.stderr,
            "stager must not overwrite an existing directory")

    invalid_source = temp / "invalid" / "DATA"
    invalid_source.mkdir(parents=True)
    for name in TR1_LEVELS:
        write_test_phd(invalid_source / name)
    (invalid_source / "LEVEL2.PHD").write_bytes(struct.pack("<I", 0x21))
    invalid = subprocess.run([str(STAGER), str(invalid_source),
                              str(temp / "invalid-output")],
                             text=True, capture_output=True)
    require(invalid.returncode != 0 and "PHD magic" in invalid.stderr,
            "stager accepted a non-TR1 PHD header")


def main() -> None:
    makefile = APP / "Makefile"
    frontend = APP / "OLFullGameViewController.mm"
    platform = APP / "OLPlatform.c"
    info = APP / "Resources/Info.plist"
    upstream = APP / "UpstreamFull/UPSTREAM.md"
    gameflow = APP / "UpstreamFull/gameflow.h"

    contains(makefile, "iphone:clang:9.3:6.0", "ARCHS = armv7",
             "AudioToolbox", "OpenGLES", "GLKit",
             "OLFullGameViewController.mm", "UpstreamFull/libs/stb_vorbis",
             "-D_GAPI_GLES2")
    require("minimp3.cpp" not in makefile.read_text(encoding="utf-8"),
            "LGPL MP3 decoder must not be statically linked into the app")
    require("GameController" not in makefile.read_text(encoding="utf-8"),
            "iOS 6 target must not link GameController.framework")
    contains(info, "<string>6.0</string>", "UIFileSharingEnabled",
             "<string>0.2.4</string>", "CFBundleIcons",
             "CFBundleIconFiles", "Icon.png", "Icon-Small.png",
             "UIPrerenderedIcon",
             "UIInterfaceOrientationLandscapeLeft",
             "UIInterfaceOrientationLandscapeRight")
    require("<key>UIApplicationExitsOnSuspend</key><true/>" in
            info.read_text(encoding="utf-8"),
            "iOS 6 PowerVR build must exit rather than suspend with GL locks")
    icon_sizes = {
        "Icon.png": (57, 57), "Icon@2x.png": (114, 114),
        "Icon-Small.png": (29, 29), "Icon-Small@2x.png": (58, 58),
        "iTunesArtwork": (512, 512),
    }
    for name, dimensions in icon_sizes.items():
        require(png_dimensions(APP / "Resources" / name) == dimensions,
                f"{name} has the wrong dimensions")
    contains(upstream, "8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea",
             "BSD-2-Clause")
    contains(APP / "UpstreamFull/gapi/gl.h", "OPENLARA_IPOD_TOUCH4",
             "#define glTexImage3D(...)", "#define glGetProgramBinary(...)")
    contains(APP / "UpstreamFull/sound.h", "!defined(OPENLARA_IPOD_TOUCH4)")
    contains(APP / "Resources/THIRDPARTY-NOTICES.txt",
             "BSD 2-Clause License", 'Timur "XProger" Gagiev')
    contains(gameflow, '{ "LEVEL3A"', '{ "LEVEL10C"', '{ "END2"')
    contains(frontend, "<UIKeyInput>", "ol_icade_update(",
             "ol_touch_keys(", "multipleTouchEnabled = YES",
             "AudioQueueNewOutput", "AudioQueueStop", "Game::init()",
             "Game::deinit()", "view.contentScaleFactor = 1.0",
             "self.preferredFramesPerSecond = 30", "TITLE.PHD", "END2.PHD",
             "Input::setJoyDown(0, jkA", "Input::setJoyDown(0, jkStart",
             "audio_underruns=%lu", "[self.controlsView resetInput]",
             "expireHardwareInputAtTime", "glFinish()", "deleteDrawable",
             "self.context = nil", 'snprintf(saveDir',
             "olValidTR1Level", 'writeDiagnostics:@"memory-warning"')
    contains(platform, "{'w', 'e', OL_KEY_UP}",
             "{'y', 't', OL_KEY_A}", "{'o', 'g', OL_KEY_Z}",
             "{'k', 'p', OL_KEY_START}", "{'l', 'v', OL_KEY_SELECT}",
             "void ol_palette_rgba")
    contains(STAGER, "TR1_LEVELS", "TR1_PC_MAGIC = 0x20",
             '"engine": "OpenLara full"', "os.replace(staged, output)",
             '("audio", "audio", False)', '("FMV", "FMV", True)')

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
            dylibs = subprocess.check_output([str(otool), "-L", str(binary)],
                                             text=True)
            require("OpenGLES.framework" in dylibs and "GLKit.framework" in dylibs,
                    "full-engine OpenGL ES/GLKit dependencies are missing")
            require("GameController.framework" not in dylibs,
                    "binary unexpectedly imports post-iOS-6 GameController")
        nm = Path("/home/david/theos/toolchain/linux/iphone/bin/nm")
        if nm.is_file():
            imports = subprocess.check_output([str(nm), "-u", str(binary)],
                                              text=True)
            es3_imports = (
                "_glGetStringi", "_glTexImage3D", "_glTexSubImage3D",
                "_glGetProgramBinary", "_glProgramBinary",
            )
            require(not any(symbol in imports for symbol in es3_imports),
                    "binary imports an OpenGL ES 3-only entry point")

    print("OpenLara Touch regression: PASS")


if __name__ == "__main__":
    main()

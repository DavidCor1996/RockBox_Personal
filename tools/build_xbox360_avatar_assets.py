#!/usr/bin/env python3
"""Build the bundled default Xbox 360 avatar pack and its UI sound banks.

Character frames are rendered from the real Ms-PL XNA rig extracted by
`tools/xbox_avatar_rigpack.py`; this tool only bakes the untouched default
appearance for each body into the RAV2 clips and portraits that ship with the
repository, plus the authentic dashboard sound banks.  Nothing is drawn here.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.xbox_avatar import (  # noqa: E402
    CLIPS,
    TURNTABLE_CLIP,
    AvatarProfile,
    XboxAvatarService,
    encode_rav1,
    encode_ui_bank,
    _save_avatar_portrait,
)
from services.xbox_avatar_render import BODY_PRESETS  # noqa: E402


def _read(path):
    return Path(path).read_bytes()


def build_sound_banks(pcm_root, output_root):
    """Transcode the authentic dashboard and unlock effects into device banks."""
    sounds_root = Path(output_root) / "sounds"
    sounds_root.mkdir(parents=True, exist_ok=True)
    written = []
    for rate in (44100, 48000):
        effects = {
            name: _read(Path(pcm_root) / f"{name}-{rate}.pcm")
            for name in ("navigate", "select", "back")
        }
        bank = sounds_root / f"ui-{rate}.uib"
        bank.write_bytes(encode_ui_bank(rate, effects))
        written.append(bank)
        unlock = sounds_root / f"unlock-{rate}.pcm"
        shutil.copy2(Path(pcm_root) / f"unlock-{rate}.pcm", unlock)
        written.append(unlock)
    return written


def build_default_pack(output_root):
    """Render the untouched default appearance for every real body preset."""
    output_root = Path(output_root)
    summary = {
        "schema": 3,
        "frame_ms": 125,
        "turntable_frame_ms": 200,
        "bodies": {},
        "provenance": {
            "character_and_motion": (
                "Microsoft XNA Game Studio 4.0 Avatar Animation Pack"
            ),
            "character_license": "Microsoft Permissive License (Ms-PL)",
            "rig_pack": "assets/ipodjs/sources/xbox360/avatar/rig",
            "renderer": "services.xbox_avatar_render",
            "dashboard_ui_sounds": (
                "Xbox 360 SystemUpdate 17559 AvatarEditor.xex"
            ),
            "achievement_sound": (
                "Xbox Wire Major Nelson master; non-commercial personal use"
            ),
            "generated_or_hand_drawn_character_art": False,
        },
    }
    for body in BODY_PRESETS:
        service = XboxAvatarService(ROOT, AvatarProfile(body=body).as_dict())
        if not service.available():
            raise SystemExit(
                "the real XNA rig pack is missing; run "
                "tools/xbox_avatar_rigpack.py first"
            )
        body_summary = {}
        for clip in CLIPS:
            payload = encode_rav1(
                service.frames(clip, "device", matte=True),
                200 if clip == TURNTABLE_CLIP else 125,
                opaque=True,
                keyframes=clip == TURNTABLE_CLIP,
            )
            destination = output_root / body / "clips" / f"{clip}.rav"
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(payload)
            body_summary[clip] = {"bytes": len(payload)}
        _save_avatar_portrait(
            service.frames("jump", "preview")[0],
            output_root / body / "portrait.80x80x24.bmp",
        )
        summary["bodies"][body] = body_summary
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--pcm-root",
        help="directory of decoded navigate/select/back/unlock PCM effects",
    )
    parser.add_argument(
        "--sounds-only", action="store_true",
        help="rebuild only the UI and unlock sound banks",
    )
    parser.add_argument(
        "--output-root",
        default=str(ROOT / "assets/ipodjs/rockbox/achievements/avatar/base"),
    )
    parser.add_argument(
        "--manifest",
        default=str(ROOT / "assets/ipodjs/sources/xbox360/avatar"
                    "/build-manifest.json"),
    )
    args = parser.parse_args()

    if args.pcm_root:
        build_sound_banks(args.pcm_root, args.output_root)
    elif args.sounds_only:
        parser.error("--sounds-only requires --pcm-root")

    if args.sounds_only:
        print(json.dumps({"sounds": True}, indent=2))
        return

    summary = build_default_pack(args.output_root)
    Path(args.manifest).write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps({
        "bodies": list(summary["bodies"]),
        "clips": len(CLIPS),
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()

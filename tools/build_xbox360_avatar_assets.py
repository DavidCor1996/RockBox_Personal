#!/usr/bin/env python3
"""Package authentic Microsoft XNA avatar renders for RockPod and iPod.

This tool does not draw or invent character artwork. It combines textured RGB
and silhouette renders made from the original XNA FBX rig, then encodes the
result to the bounded RAV2 format used by the Achievements plugin.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.xbox_avatar import (  # noqa: E402
    APPEARANCE_FIELDS,
    AvatarProfile,
    XboxAvatarService,
    encode_rav1,
    encode_ui_bank,
    load_strip,
    load_official_face_parts,
    render_official_face,
    TURNTABLE_CLIP,
)


CLIPS = {
    "jump": ("jump", 14),
    "throw": ("throw", 10),
    "faint": ("faint", 11),
    "sit-idle": ("sit-idle", 16),
    "punch": ("punch", 12),
    "kick": ("kick", 12),
    "walk": ("walk", 16),
    "turntable": ("turntable", 24),
}
BODIES = (
    ("boy", "xna-boy"),
    ("girl", "xna-girl"),
    ("girl-heels", "xna-girl-heels"),
)


def _rgba_frames(render_root, body, render_name):
    color_root = render_root / body / render_name / "color"
    mask_root = render_root / body / render_name / "mask"
    color_paths = sorted(color_root.glob("frame-*.png"))
    mask_paths = sorted(mask_root.glob("frame-*.png"))
    if not color_paths or len(color_paths) != len(mask_paths):
        raise ValueError(f"Incomplete render set for {body}/{render_name}")
    frames = []
    for color_path, mask_path in zip(color_paths, mask_paths):
        with Image.open(color_path) as color_source:
            color = color_source.convert("RGBA")
        with Image.open(mask_path) as mask_source:
            mask = mask_source.convert("L")
        if color.size != mask.size:
            raise ValueError(f"Mask mismatch for {color_path}")
        alpha = mask.point(lambda value: 0 if value < 8 else value)
        maximum = max(alpha.getextrema()[1], 1)
        alpha = alpha.point(lambda value: min(255, value * 255 // maximum))
        rgba_pixels = []
        for (red, green, blue, _old_alpha), amount in zip(
                color.getdata(), alpha.getdata()):
            if amount:
                rgba_pixels.append((
                    min(255, red * 255 // amount),
                    min(255, green * 255 // amount),
                    min(255, blue * 255 // amount),
                    amount,
                ))
            else:
                rgba_pixels.append((0, 0, 0, 0))
        color.putdata(rgba_pixels)
        frames.append(color)
    return frames


def _save_strip(frames, path):
    strip = Image.new(
        "RGBA", (frames[0].width * len(frames), frames[0].height),
        (0, 0, 0, 0),
    )
    for index, frame in enumerate(frames):
        strip.alpha_composite(frame, (index * frame.width, 0))
    path.parent.mkdir(parents=True, exist_ok=True)
    strip.save(path, "PNG", optimize=True)


def _save_portrait(frame, path):
    alpha = frame.getchannel("A")
    bounds = alpha.getbbox() or (0, 0, frame.width, frame.height)
    left, top, right, bottom = bounds
    body_width = max(1, right - left)
    head_size = max(32, min(frame.width, int(body_width * 1.55)))
    center_x = (left + right) // 2
    crop_top = max(0, top - 2)
    crop = (
        max(0, center_x - head_size // 2), crop_top,
        min(frame.width, center_x + head_size // 2),
        min(frame.height, crop_top + head_size),
    )
    avatar = frame.crop(crop)
    avatar.thumbnail((72, 72), Image.Resampling.LANCZOS)
    canvas = Image.new("RGB", (80, 80), (222, 224, 226))
    draw = ImageDraw.Draw(canvas)
    for y in range(80):
        green = 184 - y // 3
        draw.line((0, y, 79, y), fill=(112, max(115, green), 38))
    canvas.paste(
        avatar.convert("RGB"),
        ((80 - avatar.width) // 2, 80 - avatar.height),
        avatar.getchannel("A"),
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path, "BMP")


def _read(path):
    with open(path, "rb") as handle:
        return handle.read()


def _finish_summary(summary):
    summary["provenance"] = {
        "character_and_motion": "Microsoft XNA Avatar Animation Pack 4.0",
        "character_license": "Microsoft Permissive License (Ms-PL)",
        "character_textures": (
            "Original Microsoft XNA Avatar_texture color maps"
        ),
        "desktop_model": (
            "Qt Quick 3D mesh converted from the genuine XNA FBX geometry"
        ),
        "render_conversion": (
            "ufbx 0.23.0; original UV color maps; studio lighting; "
            "416x672 RGBA; 24-angle 360-degree turntable"
        ),
        "dashboard_ui_sounds": "Xbox 360 SystemUpdate 17559 AvatarEditor.xex",
        "achievement_sound": (
            "Xbox Wire Major Nelson master; non-commercial personal use"
        ),
        "generated_or_hand_drawn_character_art": False,
    }
    summary["appearance"] = {
        "editable_materials": list(APPEARANCE_FIELDS),
        "method": (
            "tint authentic textured material regions while preserving "
            "source UV detail, lighting, silhouette, and alpha"
        ),
        "replacement_or_hand_drawn_parts": False,
    }
    return summary


def _write_manifest(source_root, summary):
    (source_root / "build-manifest.json").write_text(
        json.dumps(_finish_summary(summary), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def build(args):
    render_root = Path(args.render_root).resolve()
    output_root = Path(args.output_root).resolve()
    source_root = Path(args.source_output).resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    source_root.mkdir(parents=True, exist_ok=True)

    summary = {
        "schema": 2,
        "bodies": {},
        "frame_ms": 125,
        "turntable_frame_ms": 200,
    }
    for body, preset in BODIES:
        body_source = source_root / "master" / preset
        body_summary = {}
        for clip, (render_name, frame_limit) in CLIPS.items():
            frames = _rgba_frames(render_root, body, render_name)[:frame_limit]
            strip_path = body_source / f"{clip}.rgba.png"
            _save_strip(frames, strip_path)
            body_summary[clip] = {
                "frames": len(frames),
                "strip": str(strip_path.relative_to(ROOT)),
            }
        summary["bodies"][preset] = body_summary

    encoded = rebuild_face_outputs(source_root, output_root)
    for body, clips in encoded["bodies"].items():
        for clip, values in clips.items():
            summary["bodies"][body][clip].update(values)

    if not args.renders_only:
        sounds_root = output_root / "sounds"
        sounds_root.mkdir(parents=True, exist_ok=True)
        for rate in (44100, 48000):
            effects = {
                name: _read(Path(args.pcm_root) / f"{name}-{rate}.pcm")
                for name in ("navigate", "select", "back")
            }
            (sounds_root / f"ui-{rate}.uib").write_bytes(
                encode_ui_bank(rate, effects)
            )
            shutil.copy2(
                Path(args.pcm_root) / f"unlock-{rate}.pcm",
                sounds_root / f"unlock-{rate}.pcm",
            )

    _write_manifest(source_root, summary)
    return summary


def rebuild_face_outputs(source_root, output_root):
    """Re-encode high-resolution masters through the exact device path."""
    source_root = Path(source_root).resolve()
    output_root = Path(output_root).resolve()
    summary = {
        "schema": 2,
        "bodies": {},
        "frame_ms": 125,
        "turntable_frame_ms": 200,
    }
    for _render_body, body in BODIES:
        service = XboxAvatarService(ROOT, AvatarProfile(body=body).as_dict())
        body_summary = {}
        portrait_frame = None
        for clip in CLIPS:
            frames = service.frames(clip, "device", matte=True)
            payload = encode_rav1(
                frames, 200 if clip == TURNTABLE_CLIP else 125,
                opaque=True,
                keyframes=clip == TURNTABLE_CLIP,
            )
            destination = output_root / body / "clips" / f"{clip}.rav"
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(payload)
            body_summary[clip] = {
                "frames": len(frames),
                "bytes": len(payload),
                "strip": str(
                    (source_root / "master" / body /
                     f"{clip}.rgba.png").relative_to(ROOT)
                ),
            }
            if clip == "jump":
                portrait_frame = service.frames(clip, "preview")[0]
        _save_portrait(
            portrait_frame, output_root / body / "portrait.80x80x24.bmp"
        )
        summary["bodies"][body] = body_summary
    return summary


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--render-root")
    parser.add_argument("--pcm-root")
    parser.add_argument(
        "--renders-only", action="store_true",
        help="normalize real FBX renders without rebuilding sound banks",
    )
    parser.add_argument(
        "--faces-only", action="store_true",
        help="rebuild RAV/portrait outputs from the checked-in normalized strips",
    )
    parser.add_argument(
        "--output-root",
        default=ROOT / "assets/ipodjs/rockbox/achievements/avatar/base",
    )
    parser.add_argument(
        "--source-output",
        default=ROOT / "assets/ipodjs/sources/xbox360/avatar",
    )
    args = parser.parse_args()
    if args.faces_only:
        summary = rebuild_face_outputs(args.source_output, args.output_root)
        _write_manifest(Path(args.source_output).resolve(), summary)
    else:
        if not args.render_root or (not args.pcm_root and not args.renders_only):
            parser.error(
                "--render-root and --pcm-root are required unless "
                "--renders-only is used"
            )
        summary = build(args)
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()

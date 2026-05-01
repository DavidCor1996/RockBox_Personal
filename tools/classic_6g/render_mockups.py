#!/usr/bin/env python3
"""Render first-pass ZeroSlackr/iPodLinux classic shell mockups.

This is a safe visual-dev path. It does not patch firmware, rewrite boot logic,
or modify the live iPod runtime. It stages a profile-driven Apple Classic style
mockup renderer based on the ZeroSlackr shell structure and Rockbox iClassic
reference art.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path
from textwrap import wrap
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[2]
PROFILE_DIR = Path(__file__).resolve().parent / "profiles"
DEFAULT_OUT = REPO_ROOT / "screenshots" / "classic_6g"


SCREENS = [
    {
        "slug": "main_menu",
        "title": "Main Menu",
        "selected": 0,
        "items": ["Music", "Now Playing", "Settings", "Extras / Games", "About / Debug"],
        "preview_title": "ZeroSlackr Shell",
        "preview_lines": [
            "Apple Classic-style visual shell",
            "running on the ZeroSlackr/iPodLinux base.",
            "This milestone keeps the runtime and boot chain intact."
        ],
        "meta_top": "classic_6g visual profile",
        "meta_bottom": "Select opens a stub screen"
    },
    {
        "slug": "music",
        "title": "Music",
        "selected": 1,
        "items": ["Back", "Artists", "Albums", "Songs", "Playlists"],
        "preview_title": "Music",
        "preview_lines": [
            "Library and playback stay untouched in this milestone.",
            "This screen is a shell-only stub using Classic layout proportions.",
            "No audio or database rewrite yet."
        ],
        "meta_top": "Safe stub screen",
        "meta_bottom": "Library hooks later"
    },
    {
        "slug": "now_playing",
        "title": "Now Playing",
        "selected": 1,
        "items": ["Back", "Track View", "Seek", "Queue", "Options"],
        "preview_title": "Now Playing",
        "preview_lines": [
            "Track: Midnight Shuttle",
            "Artist: ZeroSlackr Research Unit",
            "Album: Classic Shell Study",
            "00:42 / 04:18"
        ],
        "meta_top": "Visual stub only",
        "meta_bottom": "Playback remains external"
    },
    {
        "slug": "settings",
        "title": "Settings",
        "selected": 2,
        "items": ["Back", "Appearance", "Wheel / Buttons", "Display", "Power"],
        "preview_title": "Settings",
        "preview_lines": [
            "Future profile toggle:",
            "classic_6g / legacy_zeroslackr",
            "Current work only stages the visual profile and mockups."
        ],
        "meta_top": "No destructive changes",
        "meta_bottom": "Fallback remains available"
    },
    {
        "slug": "extras_games",
        "title": "Extras / Games",
        "selected": 3,
        "items": ["Back", "Emulators", "Tools", "Notes", "Diagnostics"],
        "preview_title": "Extras / Games",
        "preview_lines": [
            "Placeholder category for old ZeroSlackr packs",
            "and future custom-shell launch targets.",
            "No pack loading behavior changed yet."
        ],
        "meta_top": "Pack-safe shell view",
        "meta_bottom": "Launch wiring later"
    },
    {
        "slug": "about_debug",
        "title": "About / Debug",
        "selected": 4,
        "items": ["Back", "System Info", "UI Profile", "Input Test", "Return Safety"],
        "preview_title": "About / Debug",
        "preview_lines": [
            "Base runtime: ZeroSlackr on iPodLinux",
            "Reference hardware behavior: Rockbox",
            "Long-term target: custom 6G/7G-style OS on 5G hardware"
        ],
        "meta_top": "Visual milestone",
        "meta_bottom": "BOOT -> DRAW -> NAVIGATE -> EXIT"
    }
]


@dataclass
class Profile:
    name: str
    data: dict

    @property
    def canvas(self) -> dict:
        return self.data["canvas"]

    @property
    def palette(self) -> dict:
        return self.data["palette"]

    @property
    def layout(self) -> dict:
        return self.data["layout"]

    @property
    def fonts(self) -> dict:
        return self.data["fonts"]

    @property
    def assets(self) -> dict:
        return self.data["assets"]


def load_profile(name: str) -> Profile:
    path = PROFILE_DIR / f"{name}.json"
    with path.open("r", encoding="utf-8") as handle:
        return Profile(name=name, data=json.load(handle))


def svg_escape(text: str) -> str:
    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def write_svg(path: Path, profile: Profile, screen: dict) -> None:
    p = profile.palette
    l = profile.layout
    f = profile.fonts
    width = profile.canvas["width"]
    height = profile.canvas["height"]

    left = l["menu_x"]
    top = l["menu_y"]
    menu_w = l["menu_width"]
    menu_h = l["menu_height"]
    preview_x = l["preview_x"]
    preview_y = l["preview_y"]
    preview_w = l["preview_width"]
    preview_h = l["preview_height"]
    art_x = l["preview_art_x"]
    art_y = l["preview_art_y"]
    art_w = l["preview_art_width"]
    art_h = l["preview_art_height"]
    selected = screen["selected"]

    reference_menu = (REPO_ROOT / profile.assets["reference_menu"]).resolve().as_uri()
    reference_wps = (REPO_ROOT / profile.assets["reference_wps"]).resolve().as_uri()
    background_ref = (REPO_ROOT / profile.assets["reference_background"]).resolve().as_uri()

    lines = screen["preview_lines"]
    preview_text_svg = []
    y = 186
    for line in lines:
        for wrapped in wrap(line, width=28) or [""]:
            preview_text_svg.append(
                f'<text x="{preview_x + 10}" y="{y}" font-family="{f["body_family"]}" '
                f'font-size="{f["body_size"]}" fill="{p["preview_text"]}">{svg_escape(wrapped)}</text>'
            )
            y += 14

    item_svg = []
    for idx, item in enumerate(screen["items"]):
        item_y = top + 8 + idx * (l["menu_item_height"] + l["menu_item_gap"])
        if idx == selected:
            item_svg.append(
                f'<rect x="{left + 4}" y="{item_y - 14}" rx="4" ry="4" '
                f'width="{menu_w - 8}" height="{l["menu_item_height"]}" fill="url(#selgrad)"/>'
            )
            fill = p["highlight_text"]
        else:
            fill = p["menu_text"]
        item_svg.append(
            f'<text x="{left + l["menu_padding_x"]}" y="{item_y}" '
            f'font-family="{f["menu_family"]}" font-size="{f["menu_size"]}" '
            f'font-weight="{f["menu_weight"]}" fill="{fill}">{svg_escape(item)}</text>'
        )

    art_reference = reference_wps if screen["slug"] == "now_playing" else reference_menu
    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <defs>
    <linearGradient id="bggrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="{p['background']}"/>
      <stop offset="100%" stop-color="#ececec"/>
    </linearGradient>
    <linearGradient id="headergrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="{p['header_top']}"/>
      <stop offset="100%" stop-color="{p['header_bottom']}"/>
    </linearGradient>
    <linearGradient id="selgrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="{p['highlight_top']}"/>
      <stop offset="100%" stop-color="{p['highlight_bottom']}"/>
    </linearGradient>
  </defs>

  <rect x="0" y="0" width="{width}" height="{height}" fill="url(#bggrad)"/>
  <image href="{background_ref}" x="0" y="0" width="{width}" height="{height}" opacity="0.18"/>

  <rect x="0" y="0" width="{width}" height="{l['header_height']}" fill="url(#headergrad)"/>
  <line x1="0" y1="{l['header_height']}" x2="{width}" y2="{l['header_height']}" stroke="{p['separator']}" stroke-width="1"/>

  <rect x="{left}" y="{top}" width="{menu_w}" height="{menu_h}" fill="{p['panel']}" opacity="0.9"/>
  <line x1="{menu_w + 1}" y1="{top}" x2="{menu_w + 1}" y2="{height}" stroke="{p['separator']}" stroke-width="1"/>

  <rect x="{preview_x}" y="{preview_y}" width="{preview_w}" height="{preview_h}" rx="8" ry="8"
        fill="{p['panel']}" stroke="{p['panel_border']}" stroke-width="1"/>
  <rect x="{art_x}" y="{art_y}" width="{art_w}" height="{art_h}" rx="10" ry="10"
        fill="#fcfcfc" stroke="{p['panel_border']}" stroke-width="1"/>
  <image href="{art_reference}" x="{art_x + 6}" y="{art_y + 6}" width="{art_w - 12}" height="{art_h - 12}" preserveAspectRatio="xMidYMid slice"/>
  <rect x="{art_x + 6}" y="{art_y + 6}" width="{art_w - 12}" height="{art_h - 12}" fill="#ffffff" opacity="0.08"/>

  <text x="8" y="13" font-family="{f['title_family']}" font-size="{f['title_size']}" font-weight="{f['title_weight']}" fill="{p['header_text']}">{svg_escape(screen['title'])}</text>
  <text x="{width - 85}" y="13" font-family="{f['small_family']}" font-size="{f['small_size']}" fill="{p['header_text']}">11:28 PM</text>

  <rect x="{width - 36}" y="4" width="20" height="9" rx="2" ry="2" fill="none" stroke="{p['header_text']}" stroke-width="1"/>
  <rect x="{width - 15}" y="6" width="2" height="5" rx="1" ry="1" fill="{p['header_text']}"/>
  <rect x="{width - 34}" y="6" width="15" height="5" rx="1" ry="1" fill="{p['accent']}"/>
  <polygon points="{width - 110},5 {width - 103},9 {width - 110},13" fill="{p['accent']}"/>

  {''.join(item_svg)}

  <text x="{preview_x + 10}" y="{preview_y + 20}" font-family="{f['menu_family']}" font-size="{f['menu_size']}" font-weight="{f['menu_weight']}" fill="{p['preview_title']}">{svg_escape(screen['preview_title'])}</text>
  <line x1="{preview_x + 10}" y1="{preview_y + 28}" x2="{preview_x + preview_w - 10}" y2="{preview_y + 28}" stroke="{p['separator']}" stroke-width="1"/>
  <text x="{preview_x + 10}" y="{preview_y + 44}" font-family="{f['small_family']}" font-size="{f['small_size']}" fill="{p['preview_subtle']}">{svg_escape(screen['meta_top'])}</text>
  <text x="{preview_x + 10}" y="{l['footer_y']}" font-family="{f['small_family']}" font-size="{f['small_size']}" fill="{p['preview_subtle']}">{svg_escape(screen['meta_bottom'])}</text>
  {''.join(preview_text_svg)}
</svg>
"""
    path.write_text(svg, encoding="utf-8")


def render_svg_to_png(svg_path: Path, png_path: Path) -> None:
    subprocess.run(
        ["magick", "convert", str(svg_path), str(png_path)],
        check=True,
    )


def render_reference_contact_sheet(out_dir: Path) -> None:
    sources = [
        REPO_ROOT / "assets/classic_6g/mockups/reference-rockbox/menu-iClassic_v1.0_Menu.png",
        REPO_ROOT / "assets/classic_6g/mockups/reference-rockbox/wps-iClassic_v1.0_WPS.png",
        REPO_ROOT / "assets/classic_6g/mockups/reference-rockbox/1-iClassic_v1.0_Volume.png",
        REPO_ROOT / "assets/classic_6g/mockups/reference-rockbox/3-iClassic_v1.0_FMS.png"
    ]
    subprocess.run(
        [
            "magick",
            "montage",
            *(str(p) for p in sources),
            "-tile",
            "2x2",
            "-geometry",
            "320x240+8+8",
            str(out_dir / "reference_contact_sheet.png"),
        ],
        check=True,
    )


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", default="classic_6g", choices=["classic_6g", "legacy_zeroslackr"])
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    args = parser.parse_args(list(argv) if argv is not None else None)

    profile = load_profile(args.profile)
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    for screen in SCREENS:
        svg_path = out_dir / f"{args.profile}_{screen['slug']}.svg"
        png_path = out_dir / f"{args.profile}_{screen['slug']}.png"
        write_svg(svg_path, profile, screen)
        render_svg_to_png(svg_path, png_path)

    render_reference_contact_sheet(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""Tests for Rockbox WPS/SBS album-art size discovery."""

import os

from services.rockbox_wps_art import (
    parse_wps_album_art_sizes,
    wps_album_art_sizes_for_config,
    wps_album_art_sizes_for_profile,
)


def test_wps_album_art_size_parser_handles_valid_and_malformed_tags():
    text = "%Cl(91,10,138,138,1)\n%Cl(bad)\n%Cl(0,0,54,54)\n%Cl(1,2,-3,4)"

    assert parse_wps_album_art_sizes(text) == [(138, 138), (54, 54)]


def test_profile_size_discovery_reads_wps_then_sbs(tmp_dir):
    wps_dir = os.path.join(tmp_dir, "wps")
    os.makedirs(wps_dir, exist_ok=True)
    with open(os.path.join(wps_dir, "iPone.wps"), "w", encoding="utf-8") as handle:
        handle.write("%Cl(91,10,138,138,1)\n")
    with open(os.path.join(wps_dir, "iPone.sbs"), "w", encoding="utf-8") as handle:
        handle.write("%Cl(12,12,51,51)\n")

    sizes = wps_album_art_sizes_for_profile(
        {
            "source_repo_path": tmp_dir,
            "selected_theme": "iPone",
            "screen_resolution": "320x240",
        },
        max_sizes=2,
    )

    assert sizes == [(138, 138), (51, 51)]


def test_config_size_discovery_prefers_connected_device_config(config, tmp_dir):
    mount = os.path.join(tmp_dir, "ipod")
    os.makedirs(os.path.join(mount, ".rockbox", "wps"), exist_ok=True)
    with open(os.path.join(mount, ".rockbox", "config.cfg"), "w", encoding="utf-8") as handle:
        handle.write("wps: /.rockbox/wps/Active.wps\nsbs: /.rockbox/wps/Active.sbs\n")
    with open(os.path.join(mount, ".rockbox", "wps", "Active.wps"), "w", encoding="utf-8") as handle:
        handle.write("%Cl(1,2,100,100)\n")
    with open(os.path.join(mount, ".rockbox", "wps", "Active.sbs"), "w", encoding="utf-8") as handle:
        handle.write("%Cl(1,2,44,44)\n")

    repo_wps = os.path.join(tmp_dir, "repo", "wps")
    os.makedirs(repo_wps, exist_ok=True)
    with open(os.path.join(repo_wps, "iPone.wps"), "w", encoding="utf-8") as handle:
        handle.write("%Cl(91,10,138,138,1)\n")
    config.rockbox_profiles = [
        {
            "id": "ipod-320x240",
            "device_mount_path": mount,
            "source_repo_path": os.path.join(tmp_dir, "repo"),
            "selected_theme": "iPone",
            "screen_resolution": "320x240",
        }
    ]
    config.rockbox_selected_profile_id = "ipod-320x240"

    assert wps_album_art_sizes_for_config(config, mount) == [(100, 100), (44, 44)]

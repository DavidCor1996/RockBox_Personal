#!/usr/bin/env python3
"""Validate the read-only iPodJS simulator navigation/playback trace."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


def load_trace(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def collapse_screens(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    result: list[dict[str, str]] = []
    for row in rows:
        if row["kind"] not in {"list", "screen", "wps"}:
            continue
        if row["name"] in {
            "Context Menu",
            "Lockscreen",
            "Quick Settings",
            "Transition",
            "Preview Fade",
            "Menu Storm",
            "WPS Menu Storm",
            "WPS Select",
            "WPS Lyrics",
            "Tagtree Cache",
        }:
            continue
        if row["update"] in {"apple-slider", "title-scroll"}:
            continue
        key = (row["kind"], row["name"])
        if not result or (result[-1]["kind"], result[-1]["name"]) != key:
            result.append(row)
    return result


def validate(
    path: Path,
    max_wps_full_frames: int = 4,
    require_lockscreen: bool = True,
    require_playback_transition: bool = True,
    lyrics_home_cycle: bool = False,
) -> None:
    rows = load_trace(path)
    if not rows:
        raise SystemExit("iPodJS trace is empty")

    transition_groups: list[list[dict[str, str]]] = []
    for row in rows:
        if row["kind"] == "screen" and row["name"] == "Transition":
            if not transition_groups or int(row["first"]) == 0:
                transition_groups.append([])
            transition_groups[-1].append(row)
    playback_transition_seen = False
    for group in transition_groups:
        reveals = [int(row["selected"]) for row in group]
        frames = [int(row["first"]) for row in group]
        ticks = [int(row["tick"]) for row in group]
        directions = {row["update"] for row in group}
        if (
            not 2 <= len(group) <= 8
            or reveals[0] != 0
            or reveals[-1] != 320
            or reveals != sorted(reveals)
            or frames != list(range(len(group) - 1)) + [7]
            or len(directions) != 1
            or ticks != sorted(ticks)
            or not 20 <= ticks[-1] - ticks[0] <= 35
        ):
            raise SystemExit(
                "incomplete iPodJS transition: "
                f"direction={sorted(directions)} frames={frames} "
                f"reveals={reveals} ticks={ticks}"
            )

        active = [row for row in group if int(row["audio"]) & 1]
        if active:
            playback_transition_seen = True
            identities = {
                (row["path"], row["playlist_index"], row["playlist_count"])
                for row in active
            }
            if len(identities) != 1:
                raise SystemExit(
                    "playback identity changed during iPodJS transition: "
                    f"{sorted(identities)}"
                )
            core_states = {
                (row["core_available"], row["core_allocatable"])
                for row in active
            }
            if len(core_states) != 1:
                raise SystemExit(
                    "core arena changed during playback-active transition: "
                    f"{sorted(core_states)}"
                )

    if (require_playback_transition and transition_groups and
            not playback_transition_seen):
        raise SystemExit("trace contains no playback-active iPodJS transition")

    menu_storms = [
        (index, row) for index, row in enumerate(rows)
        if row["kind"] == "screen" and row["name"] == "Menu Storm"
    ]
    for index, storm in menu_storms:
        if not int(storm["audio"]) & 1:
            raise SystemExit("Menu storm coalesced only after playback stopped")
        if any(
            row["name"] == "Loading Music" and row["update"] == "failed"
            for row in rows[: index + 1]
        ):
            raise SystemExit("Menu storm followed a failed tagcache recovery")
        try:
            home = next(
                row for row in rows[index + 1 :]
                if row["kind"] == "screen" and row["name"] == "Home"
            )
        except StopIteration as exc:
            raise SystemExit("Menu storm did not coalesce to Home") from exc
        for field in ("path", "playlist_index", "playlist_count"):
            if home[field] != storm[field]:
                raise SystemExit(
                    f"Menu storm changed playback {field}: "
                    f"before={storm[field]!r} after={home[field]!r}"
                )

    wps_menu_storms = [
        (index, row) for index, row in enumerate(rows)
        if row["kind"] == "screen" and row["name"] == "WPS Menu Storm"
    ]
    for index, storm in wps_menu_storms:
        if not int(storm["audio"]) & 1:
            raise SystemExit("WPS Menu storm stopped playback")
        try:
            home = next(
                row for row in rows[index + 1 :]
                if row["kind"] == "screen" and row["name"] == "Home"
            )
        except StopIteration as exc:
            raise SystemExit("WPS Menu storm did not coalesce to Home") from exc
        if any(
            row["name"] == "Loading Music"
            for row in rows[index : rows.index(home) + 1]
        ):
            raise SystemExit("WPS Menu storm entered tagcache recovery")
        for field in ("path", "playlist_index", "playlist_count"):
            if home[field] != storm[field]:
                raise SystemExit(
                    f"WPS Menu storm changed playback {field}: "
                    f"before={storm[field]!r} after={home[field]!r}"
                )

    loading_starts = [
        (index, row) for index, row in enumerate(rows)
        if row["kind"] == "screen" and row["name"] == "Loading Music"
        and row["update"] == "start"
    ]
    for index, start in loading_starts:
        try:
            resolved = next(
                row for row in rows[index + 1 :]
                if row["kind"] == "screen" and
                row["name"] == "Loading Music" and
                row["update"] in {"recovered", "revalidated", "failed"}
            )
        except StopIteration as exc:
            raise SystemExit("Loading Music never resolved") from exc
        if int(resolved["tick"]) - int(start["tick"]) > 15 * 100 + 5:
            raise SystemExit(
                "Loading Music exceeded its hard deadline: "
                f"start={start['tick']} end={resolved['tick']}"
            )

    lyrics_launches = [
        (index, row) for index, row in enumerate(rows)
        if row["kind"] == "screen" and row["name"] == "WPS Lyrics"
        and row["update"] == "launch"
    ]
    for index, launch in lyrics_launches:
        before = next(
            row for row in reversed(rows[:index]) if row["kind"] == "wps"
        )
        after = next(
            row for row in rows[index + 1 :]
            if row["kind"] in {"wps", "screen", "list"} and
            (row["kind"] == "list" or
             row["name"] in {"Now Playing", "Home"})
        )
        for field in (
            "core_available", "core_allocatable", "rockbox_open_files",
            "path", "playlist_index", "playlist_count",
        ):
            if after[field] != before[field]:
                raise SystemExit(
                    f"Lyrics round-trip changed {field}: "
                    f"before={before[field]!r} after={after[field]!r}"
                )
        if not int(after["audio"]) & 1:
            raise SystemExit("Lyrics round-trip stopped playback")

    preview_fades = [
        row for row in rows
        if row["kind"] == "screen" and row["name"] == "Preview Fade"
    ]
    fade_groups = []
    for row in preview_fades:
        if not fade_groups or int(row["first"]) == 0:
            fade_groups.append([])
        fade_groups[-1].append(row)
    for group in fade_groups:
        alphas = [int(row["selected"]) for row in group]
        frames = [int(row["first"]) for row in group]
        ticks = [int(row["tick"]) for row in group]
        if (
            not 2 <= len(group) <= 8
            or alphas[0] != 0
            or alphas[-1] != 256
            or alphas != sorted(alphas)
            or frames != list(range(len(group) - 1)) + [7]
            or not 20 <= ticks[-1] - ticks[0] <= 35
        ):
            raise SystemExit(
                "incomplete iPodJS preview fade: "
                f"frames={frames} alpha={alphas} ticks={ticks}"
            )

    screens = collapse_screens(rows)
    try:
        wps_screen = next(
            index for index, row in enumerate(screens) if row["kind"] == "wps"
        )
    except StopIteration as exc:
        raise SystemExit("trace contains no While Playing screen") from exc
    before = screens[:wps_screen]
    after = screens[wps_screen + 1 :]
    try:
        music_screen = max(
            index for index, row in enumerate(before) if row["name"] == "Music"
        )
        journey_home = max(
            index
            for index, row in enumerate(before[:music_screen])
            if row["name"] == "Home"
        )
    except ValueError as exc:
        raise SystemExit("trace contains no Home -> Music browser journey") from exc
    before = before[journey_home:]
    if len(before) < 5:
        raise SystemExit(f"incomplete browser descent before WPS: {before}")
    if before[0]["name"] != "Home" or before[1]["name"] != "Music":
        raise SystemExit(
            f"journey did not begin Home -> Music: "
            f"{[(row['kind'], row['name']) for row in before]}"
        )
    browser = before[-3:]
    browser_names = [row["name"] for row in browser]
    returned_names = [row["name"] for row in after]
    if not wps_menu_storms and not menu_storms and not lyrics_home_cycle:
        expected_return = list(reversed(browser_names)) + ["Music", "Home"]
        cursor = 0
        for name in expected_return:
            try:
                cursor = returned_names.index(name, cursor) + 1
            except ValueError as exc:
                raise SystemExit(
                    f"browser return lost {name!r}; expected {expected_return}, "
                    f"observed {returned_names}"
                ) from exc

        source_name = browser[-1]["name"]
        first_wps_row = next(
            index for index, row in enumerate(rows) if row["kind"] == "wps"
        )
        source = next(
            row
            for row in reversed(rows[:first_wps_row])
            if row["kind"] == "list" and row["name"] == source_name
        )
        restored = next(row for row in after if row["name"] == source["name"])
        for field in ("selected", "first", "items"):
            if restored[field] != source[field]:
                raise SystemExit(
                    f"source list {field} was not restored: "
                    f"before={source[field]} after={restored[field]}"
                )

    wps = [row for row in rows if row["kind"] == "wps"]
    if not wps:
        raise SystemExit("trace contains no While Playing frame")
    playback_wps = [row for row in wps if row["path"]]
    paths = {row["path"] for row in playback_wps}
    if len(paths) != 1:
        raise SystemExit(f"While Playing changed track path: {sorted(paths)}")
    path_value = next(iter(paths))
    if not path_value.lower().endswith((".mp3", ".flac", ".aiff", ".aif")):
        raise SystemExit(f"unexpected traced playback path: {path_value}")

    playlist_state = {
        (row["playlist_index"], row["playlist_count"])
        for row in playback_wps
    }
    if len(playlist_state) != 1:
        raise SystemExit(
            f"While Playing changed playlist identity: {sorted(playlist_state)}"
        )
    if any(int(row["audio"]) == 0 for row in playback_wps):
        raise SystemExit("While Playing trace contains stopped playback")

    elapsed = [int(row["elapsed_ms"]) for row in playback_wps]
    if elapsed != sorted(elapsed):
        raise SystemExit(f"While Playing elapsed time regressed: {elapsed}")

    full_frames = [row for row in wps if row["update"].startswith("full")]
    if len(full_frames) > max_wps_full_frames:
        raise SystemExit(
            f"While Playing produced excessive full frames: {len(full_frames)}"
        )
    if not any(row["update"] == "bottom" for row in wps):
        raise SystemExit("idle While Playing produced no bounded progress update")
    if (not lyrics_home_cycle and
            wps[-1]["update"].startswith("full")):
        raise SystemExit("idle While Playing ended with an unnecessary full frame")

    lockscreens = [row for row in rows if row["name"] == "Lockscreen"]
    if require_lockscreen and not lockscreens:
        raise SystemExit("trace contains no lockscreen frame")

    crt = [row for row in rows if row["name"] == "Shutdown CRT"]
    if crt:
        updates = [row["update"] for row in crt]
        expected = ["band"] * 6 + ["line"] * 5 + ["dot", "black"]
        if updates != expected:
            raise SystemExit(
                f"incomplete CRT shutdown sequence: {updates}"
            )
        first_crt = rows.index(crt[0])
        previous_home = max(
            index
            for index, row in enumerate(rows[:first_crt])
            if row["name"] == "Home"
        )
        leaked = [
            (row["kind"], row["name"], row["update"])
            for row in rows[previous_home + 1 : first_crt]
            if row["name"] not in {"Home", "Shutdown CRT"}
        ]
        if leaked:
            raise SystemExit(
                f"non-home frame flashed before CRT shutdown: {leaked}"
            )
        if any(int(row["audio"]) != 0 for row in crt):
            raise SystemExit("CRT shutdown began before playback stopped")

    print(
        "validated iPodJS navigation trace: "
        f"{len(rows)} records, track={path_value}, "
        f"playlist={next(iter(playlist_state))}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-wps-full-frames", type=int, default=4)
    parser.add_argument("--allow-no-lockscreen", action="store_true")
    parser.add_argument("--allow-no-playback-transition", action="store_true")
    parser.add_argument("--lyrics-home-cycle", action="store_true")
    parser.add_argument("trace", type=Path)
    args = parser.parse_args()
    validate(
        args.trace,
        args.max_wps_full_frames,
        require_lockscreen=not args.allow_no_lockscreen,
        require_playback_transition=not args.allow_no_playback_transition,
        lyrics_home_cycle=args.lyrics_home_cycle,
    )


if __name__ == "__main__":
    main()

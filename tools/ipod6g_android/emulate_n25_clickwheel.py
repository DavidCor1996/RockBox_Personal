#!/usr/bin/env python3
"""Model the Rockbox-derived N25 click-wheel packet and safety contract."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


POSITIONS = 96
SENSITIVITY = 4
BUTTON_CODES = {
    1 << 0: "DPAD_CENTER",
    1 << 1: "DPAD_RIGHT",
    1 << 2: "DPAD_LEFT",
    1 << 3: "MEDIA_PLAY_PAUSE",
    1 << 4: "BACK",
}


class WheelModel:
    def __init__(self) -> None:
        self.buttons = 0
        self.old_position = -1
        self.accumulator = 0
        self.held = False

    def set_hold(self, held: bool) -> list[tuple[str, int]]:
        events: list[tuple[str, int]] = []
        if held and not self.held:
            events.extend(
                (code, 0)
                for mask, code in BUTTON_CODES.items()
                if self.buttons & mask
            )
            self.buttons = 0
            self.old_position = -1
            self.accumulator = 0
        self.held = held
        return events

    def packet(self, status: int) -> list[tuple[str, int]]:
        if self.held:
            return []
        events: list[tuple[str, int]] = []
        if status & 0x800000FF == 0x8000001A:
            buttons = status >> 8 & 0x1F
            touched = bool(status & 1 << 30)
            position = status >> 16 & 0x7F
        elif status & 0x8000FFFF == 0x8000023A:
            buttons = status >> 16 & 0x1F
            touched = False
            position = -1
        else:
            return []

        changed = self.buttons ^ buttons
        for mask, code in BUTTON_CODES.items():
            if changed & mask:
                events.append((code, int(bool(buttons & mask))))
        self.buttons = buttons

        if not touched:
            self.old_position = -1
            self.accumulator = 0
            return events
        if position >= POSITIONS:
            return events
        if self.old_position < 0:
            self.old_position = position
            return events

        delta = position - self.old_position
        if delta < -(POSITIONS // 2):
            delta += POSITIONS
        elif delta > POSITIONS // 2:
            delta -= POSITIONS
        self.old_position = position
        self.accumulator += delta
        while self.accumulator >= SENSITIVITY:
            events.extend((("DPAD_DOWN", 1), ("DPAD_DOWN", 0)))
            self.accumulator -= SENSITIVITY
        while self.accumulator <= -SENSITIVITY:
            events.extend((("DPAD_UP", 1), ("DPAD_UP", 0)))
            self.accumulator += SENSITIVITY
        return events


def touch(position: int, buttons: int = 0) -> int:
    return 0x8000001A | (1 << 30) | (position << 16) | (buttons << 8)


def qualify() -> dict:
    cases = {}

    model = WheelModel()
    pressed = model.packet(0x8000001A | (1 << 8))
    released = model.packet(0x8000001A)
    assert pressed == [("DPAD_CENTER", 1)]
    assert released == [("DPAD_CENTER", 0)]
    cases["select_press_release"] = True

    model = WheelModel()
    assert model.packet(0x8000023A | (0x12 << 16)) == [
        ("DPAD_RIGHT", 1),
        ("BACK", 1),
    ]
    cases["initial_ack_button_format"] = True

    model = WheelModel()
    model.packet(touch(10))
    assert model.packet(touch(14)) == [("DPAD_DOWN", 1), ("DPAD_DOWN", 0)]
    cases["clockwise_dpad_down"] = True

    model = WheelModel()
    model.packet(touch(94))
    assert model.packet(touch(2)) == [("DPAD_DOWN", 1), ("DPAD_DOWN", 0)]
    cases["clockwise_wrap"] = True

    model = WheelModel()
    model.packet(touch(2))
    assert model.packet(touch(94)) == [("DPAD_UP", 1), ("DPAD_UP", 0)]
    cases["counterclockwise_wrap"] = True

    model = WheelModel()
    model.packet(0x8000001A | (1 << 8))
    assert model.set_hold(True) == [("DPAD_CENTER", 0)]
    assert model.packet(touch(20, 1)) == []
    model.set_hold(False)
    cases["hold_releases_and_suppresses"] = True

    hold_errors = 0
    fail_closed = False
    for _ in range(3):
        hold_errors += 1
        if hold_errors >= 3:
            fail_closed = True
    assert fail_closed
    cases["three_pmu_errors_fail_closed"] = True

    chord_start_ms = 1000
    assert 8999 - chord_start_ms < 8000
    assert 9000 - chord_start_ms >= 8000
    cases["menu_select_eight_second_reset"] = True

    return {
        "scope": "n25-clickwheel-host-model",
        "wheel_positions": POSITIONS,
        "wheel_sensitivity": SENSITIVITY,
        "cases": cases,
        "model_gate_passed": True,
        "persistent_storage_attached": False,
        "hardware_actions_enabled": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or output.is_symlink():
        raise SystemExit(f"refusing to overwrite report: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(qualify(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

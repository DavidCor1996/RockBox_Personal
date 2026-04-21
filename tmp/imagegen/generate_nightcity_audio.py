#!/usr/bin/env python3

from __future__ import annotations

import math
from pathlib import Path
import random


ROOT = Path("/home/david/Documents/RockBox_Personal-master")
OUT = ROOT / "apps/plugins/nightcity/audio_assets.h"
RATE = 22050


def env(t: float, dur: float, attack: float, decay: float, sustain: float, release: float) -> float:
    if t < attack:
        return t / max(attack, 1e-6)
    if t < attack + decay:
        p = (t - attack) / max(decay, 1e-6)
        return 1.0 + (sustain - 1.0) * p
    if t < max(attack + decay, dur - release):
        return sustain
    if t < dur:
        p = (t - (dur - release)) / max(release, 1e-6)
        return sustain * max(0.0, 1.0 - p)
    return 0.0


def wave_mix(freq: float, t: float) -> float:
    sine = math.sin(2 * math.pi * freq * t)
    square = 1.0 if sine >= 0 else -1.0
    tri = 2.0 * abs(2.0 * ((freq * t) % 1.0) - 1.0) - 1.0
    return 0.58 * sine + 0.24 * square + 0.18 * tri


def note_track(freq: float, dur: float, level: float = 1.0) -> list[float]:
    count = int(RATE * dur)
    out = []
    for i in range(count):
        t = i / RATE
        e = env(t, dur, 0.008, 0.06, 0.52, 0.08)
        wobble = 1.0 + 0.0035 * math.sin(2 * math.pi * 4.1 * t)
        out.append(level * e * wave_mix(freq * wobble, t))
    return out


def noise_track(dur: float, seed: int, tone: float = 0.0) -> list[float]:
    rng = random.Random(seed)
    count = int(RATE * dur)
    out = []
    lp = 0.0
    for i in range(count):
        t = i / RATE
        e = env(t, dur, 0.001, 0.04, 0.25, 0.05)
        n = rng.uniform(-1.0, 1.0)
        lp = 0.76 * lp + 0.24 * n
        s = lp + 0.35 * math.sin(2 * math.pi * tone * t) if tone > 0 else lp
        out.append(0.7 * e * s)
    return out


def mix(length: int, tracks: list[tuple[int, list[float]]]) -> list[int]:
    out = [0.0] * length
    for start, samples in tracks:
        for i, sample in enumerate(samples):
            idx = start + i
            if 0 <= idx < length:
                out[idx] += sample
    peak = max(max(abs(v) for v in out), 1e-6)
    scale = 25000.0 / peak
    return [max(-32768, min(32767, int(v * scale))) for v in out]


def stereo(samples: list[int], pan_l: float = 1.0, pan_r: float = 1.0) -> list[int]:
    out = []
    for s in samples:
        out.append(int(s * pan_l))
        out.append(int(s * pan_r))
    return out


def title_sound() -> list[int]:
    total = int(RATE * 0.72)
    tracks = [
        (0, note_track(392.0, 0.16, 0.9)),
        (int(RATE * 0.12), note_track(523.25, 0.17, 0.8)),
        (int(RATE * 0.24), note_track(659.25, 0.22, 0.78)),
        (int(RATE * 0.38), note_track(783.99, 0.24, 0.72)),
        (0, noise_track(0.10, 1, 1800.0)),
    ]
    return stereo(mix(total, tracks), 0.95, 0.95)


def afterglow_sound() -> list[int]:
    total = int(RATE * 0.86)
    tracks = [
        (0, note_track(440.0, 0.20, 0.7)),
        (int(RATE * 0.10), note_track(554.37, 0.24, 0.62)),
        (int(RATE * 0.22), note_track(659.25, 0.28, 0.58)),
        (int(RATE * 0.38), note_track(830.61, 0.32, 0.56)),
    ]
    return stereo(mix(total, tracks), 0.9, 0.96)


def move_sound() -> list[int]:
    total = int(RATE * 0.07)
    tracks = [(0, note_track(988.0, 0.07, 0.55))]
    return stereo(mix(total, tracks), 0.88, 0.76)


def confirm_sound() -> list[int]:
    total = int(RATE * 0.14)
    tracks = [
        (0, note_track(740.0, 0.08, 0.55)),
        (int(RATE * 0.035), note_track(1174.66, 0.10, 0.62)),
    ]
    return stereo(mix(total, tracks), 0.92, 0.92)


def back_sound() -> list[int]:
    total = int(RATE * 0.12)
    tracks = [
        (0, note_track(659.25, 0.06, 0.5)),
        (int(RATE * 0.03), note_track(440.0, 0.09, 0.55)),
    ]
    return stereo(mix(total, tracks), 0.82, 0.9)


def transition_sound() -> list[int]:
    total = int(RATE * 0.24)
    tracks = [
        (0, noise_track(0.12, 7, 1600.0)),
        (int(RATE * 0.03), note_track(622.25, 0.10, 0.48)),
        (int(RATE * 0.08), noise_track(0.10, 9, 2200.0)),
        (int(RATE * 0.10), note_track(932.33, 0.12, 0.54)),
    ]
    return stereo(mix(total, tracks), 0.95, 0.85)


def emit_array(name: str, data: list[int]) -> str:
    lines = [f"static const int16_t {name}[] = {{"]
    for i in range(0, len(data), 12):
        chunk = ", ".join(str(v) for v in data[i:i + 12])
        lines.append(f"    {chunk},")
    lines.append("};")
    lines.append(f"#define {name.upper()}_COUNT ((int)(sizeof({name}) / sizeof({name}[0])))")
    return "\n".join(lines)


def main() -> int:
    arrays = {
        "nc_sound_title": title_sound(),
        "nc_sound_afterglow": afterglow_sound(),
        "nc_sound_move": move_sound(),
        "nc_sound_confirm": confirm_sound(),
        "nc_sound_back": back_sound(),
        "nc_sound_transition": transition_sound(),
    }

    parts = [
        "/* Auto-generated NightCity PCM assets. */",
        "#ifndef NIGHTCITY_AUDIO_ASSETS_H",
        "#define NIGHTCITY_AUDIO_ASSETS_H",
        "#include <stdint.h>",
        f"#define NC_AUDIO_RATE {RATE}",
        "",
    ]
    for name, data in arrays.items():
        parts.append(emit_array(name, data))
        parts.append("")
    parts.append("#endif")
    OUT.write_text("\n".join(parts), encoding="utf-8")
    print(OUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

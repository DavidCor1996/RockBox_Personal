#!/usr/bin/env python3
"""Verify source geometry and descriptor behavior against the exact stock image.

Requires Unicorn. This is host emulation evidence, not a physical output pass.
"""
import argparse
import json
from pathlib import Path

from ipod6g_stock_vp_probe import probe, probe_format8, verified_body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    image = args.image.read_bytes()
    body = verified_body(image)
    corrupt = bytearray(body)
    corrupt[0] ^= 1
    try:
        verified_body(bytes(corrupt))
    except ValueError:
        pass
    else:
        raise RuntimeError("corrupted input was accepted")
    results = []
    for width, height, h_ratio, v_ratio in ((320, 240, 227, 128),
                                          (640, 480, 455, 256)):
        geometry = probe(body, width, height, 0, 0, 0, 0, 1, 1)
        expected = dict(src_x=0, src_y=0, src_w=width, src_h=height,
                        dst_x=0, dst_y=0, dst_w=720, dst_h=480,
                        h_ratio=h_ratio, v_ratio=v_ratio)
        if geometry != expected:
            raise RuntimeError(f"unexpected geometry: {geometry}")
        planes = probe_format8(body, width, height)
        y = 0x09000000
        expected = dict(image_w=width, image_h=height, y=y,
                        cb=y + width * height,
                        cr=y + width * height * 5 // 4, unused=0,
                        plane_mode=1, y_stride=width, c_stride=width // 2)
        if planes != expected:
            raise RuntimeError(f"unexpected descriptor setup: {planes}")
        results.append(dict(geometry=geometry, planes=planes))
    print(json.dumps(dict(status="stock emulation passed",
                          physical_output="not tested", results=results), indent=2))


if __name__ == "__main__":
    main()

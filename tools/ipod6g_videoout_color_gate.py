#!/usr/bin/env python3
"""Exhaustively verify the optimized RGB565-to-YCbCr conversion."""


def clamp_byte(value: int) -> int:
    return max(0, min(value, 255))


def reference(pixel: int) -> tuple[int, int, int]:
    red = ((pixel >> 11) & 0x1F) * 255 // 31
    green = ((pixel >> 5) & 0x3F) * 255 // 63
    blue = (pixel & 0x1F) * 255 // 31
    return (
        clamp_byte(((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16),
        clamp_byte(((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128),
        clamp_byte(((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128),
    )


rgb5_to_8 = [value * 255 // 31 for value in range(32)]
rgb6_to_8 = [value * 255 // 63 for value in range(64)]


def optimized(pixel: int) -> tuple[int, int, int]:
    red = rgb5_to_8[(pixel >> 11) & 0x1F]
    green = rgb6_to_8[(pixel >> 5) & 0x3F]
    blue = rgb5_to_8[pixel & 0x1F]
    return (
        clamp_byte(((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16),
        clamp_byte(((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128),
        clamp_byte(((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128),
    )


def main() -> None:
    for pixel in range(1 << 16):
        expected = reference(pixel)
        actual = optimized(pixel)
        if actual != expected:
            raise SystemExit(
                f"RGB565 0x{pixel:04x}: expected {expected}, got {actual}"
            )

    print("videoout color gate: all 65,536 RGB565 values are bit-identical")


if __name__ == "__main__":
    main()

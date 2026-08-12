# iPod Classic 6G/7G SVID recovery

This note records the evidence behind the opt-in composite-video diagnostic.
It is not a claim that TV-out is qualified for normal playback yet.

## Scope

- Primary target: iPod Classic 6G, 6.5G, and 7G using S5L8702.
- Physical target: Philips DCP750 composite input on legacy dock pin 8.
- No iPod Touch code or device path is involved.
- The diagnostic is manual and is never enabled during normal boot/playback.

## Reproduced reference-package evidence

The reference is Apple's signed iPod Classic 35.2.0.4 IPSW. The repository's
verifier independently confirms these immutable inputs:

| Artifact | Size | Digest |
| --- | ---: | --- |
| `iPod_35.2.0.4.ipsw` | 61,118,350 | SHA-1 `94fc68f5aad63a5dc8cf8b62408907ec050a4bc7` |
| IPSW | 61,118,350 | SHA-256 `7ef835c74b08f0bda3566001496cb764afbe0600cb1afec1145c259bc34ad7d0` |
| `Firmware-35.9.0.4` | 93,601,792 | SHA-256 `0b36bdb4b93685a4cb24153201a96f44124d9f50eb7c6239e203d3e917405942` |
| encrypted OSOS at `0x04e07000`, length `0x00a1ba53` | 10,598,995 | SHA-256 `e48ead9c3d68a39861622a9df9f584c26176683489e9fb84fd2c38f3a8f57678` |

Run `tools/ipod6g_stock_firmware_verify.py /path/to/iPod_35.2.0.4.ipsw`
to reproduce those checks. The official Apple download is not redistributed
with Rockbox.

## Provenance boundary

OSOS is hardware-key encrypted. The claimed decrypted-image mapping (file
offset `0xB6D8` to runtime `0x08000000`) and the complete register tables below
originated in earlier reverse engineering, but the decrypted image, its digest,
function addresses, and disassembly excerpts are not checked into this tree.
The official encrypted-container verification above therefore does **not**
independently qualify the register sequence. Reproducing that last step requires
a compatible Classic in DFU mode and wInd3x; do not interrupt a working mounted
iPod solely to do it.

The following is the current candidate interpretation, not a physical-pass
claim.

The stock driver uses three MMIO blocks:

| Base | Role |
| --- | --- |
| `0x39100000` | compositor, scaler, color tables |
| `0x39200000` | layer bases, geometry, format, routing |
| `0x39300000` | analog encoder and timing |

The recovered platform enable is interpreted as these operations in order:

1. Configure clock 14 with source 3 and divider 4.
2. Enable the clock-14 disable bit and power mask `0x1C000` (gates 14-16).
3. Change GPIO E4 to function 15 (`GPIOCMD=0x000A040F`).
4. Initialize compositor, router, and encoder blocks.

The recovered cleanup disables encoder, router, then compositor, waiting for bit 1 in
each control register, and restores E4 to function 14.

The qualification build no longer calls the presumed stock routine at
`0x2200200c`. That address is not a stable Rockbox API and could contain stale
IRAM. Rockbox now writes the equivalent PLL2/divide-by-four SVID clock locally,
bounds the readback wait, and restores the exact prior SVID clock value, power
gate bits, and GPIO E4 function on exit.

## Composite and framebuffer path

The non-component topology is selected. The current diagnostic uses the
stock NTSC/480 profile. The UI path is confirmed as:

- router layer 0;
- layer format nibble `0xF`;
- source-format mapping `0 -> 8` for the normal 16-bit UI surface;
- supplemental packing mode 3 (`0x3920000C` bit 17);
- 8-bit layer alpha `0xFF`;
- framebuffer base at `0x39200010`;
- layer/global enables at `0x39200004` bits 3 and 0.

The diagnostic feeds Rockbox's 320x240 RGB565 framebuffer to the compositor
and requests a 640x480 output rectangle. Static base changes commit the bounded
framebuffer range. Moving YUV video commits only each converted dirty band, so
an eventual external-only mode does not depend on internal-LCD DMA and does not
flush the entire CPU cache for every band.

## Qualification sequence

The debug-menu item **Test composite video** advances only on SELECT:

1. Sync with no framebuffer layer.
2. Solid blue RGB565 framebuffer.
3. Eight-bar RGB565 test pattern.
4. Static Rockbox framebuffer.
5. Explicit manual refreshes.

MENU always attempts reverse-order shutdown. Every active stage also times out
after 60 seconds, performs the same shutdown, and reports that output is off on
the local LCD. Automatic LCD-update hooks,
video playback integration, settings, cable detection, PAL selection, and
performance tuning remain gated on physical proof of these stages.

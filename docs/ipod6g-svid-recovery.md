# iPod Classic 6G/7G SVID recovery

This note records the evidence behind the opt-in composite-video diagnostic.
It is not a claim that TV-out is qualified for normal playback yet.

This document contains historical recovery steps and earlier stage names. For
the current physical DCP750 results, rejected configurations, and exact build
under test, use
[`ipod6g-dcp750-videoout-results.md`](ipod6g-dcp750-videoout-results.md).

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
| wInd3x decrypted IMG1 | 10,599,920 | SHA-256 `f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4` |

Run `tools/ipod6g_stock_firmware_verify.py /path/to/iPod_35.2.0.4.ipsw`
to reproduce those checks. The official Apple download is not redistributed
with Rockbox.

## Reproduced decrypted-image evidence

The encrypted OSOS was decrypted on a Classic with the official wInd3x DFU
AES path. The resulting unsigned IMG1 has a `0x800`-byte wrapper and a
`0xA1B5F0`-byte plaintext body. Its first body word at file offset `0x800` is
the ARM reset-vector branch to file offset `0x90C4`; the earlier claimed
`0xB6D8` mapping is not used.

The TV-out driver is the ARM cluster at file offsets `0x1692E8..0x16AEE0`.
The relevant independently reproduced routines are:

| File offset | Evidence |
| ---: | --- |
| `0x1692E8` | Programs a layer and starts compositor, mixer, and DACs |
| `0x169BA8` | Initializes the `0x39100000` compositor/video processor |
| `0x169F44` | Programs graphic-plane base addresses |
| `0x16A070` | Packs graphic-plane position at mixer offsets `0x14`, `0x20`, `0x2C`, and `0x38` |
| `0x16A1E0` | Programs graphic pixel-type bits and alpha controls |
| `0x16A504` | Initializes the `0x39300000` SDO encoder |
| `0x16AE40` | Initializes the `0x39200000` output mixer |

The higher-level layer path at `0x1212D4` maps a two-byte-per-pixel source to
hardware type 3. Type 3 sets mixer config bit 17; its layer alpha remains
`0xFF`. This establishes the native RGB565 configuration used by the current
diagnostic. The plaintext image is not redistributed by this repository, but
its digest and offsets above make the analysis reproducible.

An old shared Rockbox S5L87xx header labels `0x39200000` as CLCD, but that
label does not fit either RetailOS or physical readback. QEMU-iOS independently
maps the same three addresses on the closely related S5L8720 as TVOUT mixer 2,
TVOUT mixer 1, and TVOUT SDO.

The stock driver uses three MMIO blocks:

| Base | Role |
| --- | --- |
| `0x39100000` | TVOUT video processor / mixer 2 |
| `0x39200000` | TVOUT output mixer / mixer 1 |
| `0x39300000` | Standard Definition Output (SDO) analog encoder |

The recovered platform enable is interpreted as these operations in order:

1. Configure clock 14 with source 3 and divider 4.
2. Enable the clock-14 disable bit and power mask `0x1C000` (gates 14-16).
3. Change GPIO E4 to function 15 (`GPIOCMD=0x000A040F`).
4. Initialize compositor, encoder, then output mixer.

The recovered cleanup disables encoder, router, then compositor, waiting for bit 1 in
each control register, and restores E4 to function 14.

The qualification build no longer calls the presumed stock routine at
`0x2200200c`. That address is not a stable Rockbox API and could contain stale
IRAM. Rockbox now writes the equivalent PLL2/divide-by-four SVID clock locally,
bounds the readback wait, and restores the exact prior SVID clock value, power
gate bits, and GPIO E4 function on exit.

## Mixer producer path

The non-component topology is selected. The current diagnostic uses the stock
NTSC-M encoder profile, restores the stock-derived DAC mux value `0x1200`, and
powers all three package DAC bits. Mixer YCbCr backgrounds have physically
produced black, white, and yellow on the DCP750, proving that the mixer feeds
SDO. Clearing the DAC mux bits or attempting to run `0x39200000` as CLCD
produces only SDO's green no-input field.

Graphics DMA is the remaining physical qualification target. The decrypted
driver proves that mixer offset `0x14` is packed layer position, not source
span. The previous diagnostic wrote `320` there; this explains why framebuffer
text or bars appeared briefly while registers latched and then disappeared.
The same driver proves that the later-Samsung format nibble at `0x40` must stay
zero for this path. RGB565 instead uses type 3 at layer config `0x0C`, yielding
`0x000200FF` with opaque alpha. Layer enable is mixer config bit 3 (`0x08`).

RetailOS configures a 640x480 NTSC surface. Rockbox now expands its 320x240
RGB565 framebuffer to 640x480 in a dedicated cache-aligned buffer, writes base
at `0x10`, position zero at `0x14`, and dimensions `0x028001E0` at `0x18`.

## Qualification sequence

The debug-menu item **Test composite video** advances only on SELECT:

1. Sync with no framebuffer layer.
2. White through the mixer YCbCr background registers.
3. Stock-derived 640x480 RGB565 with mixer config `0x08`, layer config
   `0x000200ff`, position `0`, dimensions `0x028001e0`, and format nibble `0`.

All active stages show mixer status/configuration, layer base, position,
dimensions, format bits, background, and the live SDO field value.

MENU always attempts reverse-order shutdown. Every active stage also times out
after 60 seconds, performs the same shutdown, and reports that output is off on
the local LCD. Automatic LCD-update hooks,
video playback integration, settings, cable detection, PAL selection, and
performance tuning remain gated on physical proof of these stages.

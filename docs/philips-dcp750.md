# Philips DCP750 dock support

## Status

Physical DCP750/37 testing began on 2026-08-25. Charging, analog audio, dock
detection, NTSC sync, mixer backgrounds, and VP framebuffer DMA are proven on
the real unit. Full-frame format-8 packing is still being qualified. The
authoritative pass/fail ledger is
[`ipod6g-dcp750-videoout-results.md`](ipod6g-dcp750-videoout-results.md); update
that ledger before trying another register or geometry change.

Audio, accessory power, line-out control, serial iAP input, remote-button
translation, and iAP tracing already exist for the iPod Video and iPod Classic
targets. An opt-in, staged Classic 6G/7G composite diagnostic and Quick
Settings toggle now exist, but full-screen continuous mirroring is not yet
qualified. The candidate register sequence has now been checked against the
decrypted Apple 35.2.0.4 firmware and physical stage results; see the ledger
for the exact remaining failure.

Do not add guessed register writes or guessed Broadcom command payloads. Follow
the proof sequence in this document and record the observed result after every
stage.

## RockPod host simulator

RockPod exposes `Rockbox > Video Out / Dock Lab` as a host-only preview and
arrival-day bench card. It can render a generated 320x240 iPod framebuffer,
NTSC color bars, a moving cadence marker, or a loaded simulator screenshot
inside a 480x234 DCP750 panel model. The preview offers aspect-correct 4:3,
panel-stretch, and overscan views, plus simulated dock power, source, CVBS
carrier, and iAP remote events.

This screen does not access the iPod, program video registers, generate analog
CVBS, or validate electrical behavior. Its charging, line-out, remote, and
composite checkboxes are manual observations for the physical bench test. A
host preview must never be cited as evidence that Classic 6G/7G video out is
implemented or working.

## Desired result

The dock should act as all of the following while the iPod remains locally
usable:

- a continuously updated mirror of the 320x240 Rockbox framebuffer;
- an analog line-out speaker dock;
- a charger;
- a serial remote-control accessory;
- a dock that does not take ownership of playback memory or disable the iPod
  clickwheel.

The primary target is the iPod Classic 6G/7G (`IPOD_6G`, S5L8702). The
secondary target is the iPod Video 5G/5.5G (`IPOD_VIDEO`, PP5022 plus
BCM2722).

## Evidence and confidence

### Verified from Philips documentation

The DCP750 dock connector schematic exposes these relevant signals:

- left and right iPod analog audio (`IPOD/PI_O_L`, `IPOD/PI_O_R`);
- composite video (`IPOD-CVBS`) on the dock connector video-out pin;
- iPod UART transmit and receive plus `UART ENABLE`;
- dock power/charge wiring;
- no USB data transport between the dock and iPod.

The service manual is the source for the schematic. The Philips user manual
requires `TV OUT` to be enabled and instructs users to select NTSC. Philips
only advertised compatibility with the 30 GB, 60 GB, and 80 GB iPod with
video, which are 5G/5.5G models. The DCP750 panel is 480x234 pixels, but it
receives CVBS rather than a native pixel bus; that panel resolution does not
define the transmitted raster or a framebuffer format.

Philips documents charging while the player is powered off when external dock
power is present. While the player is on, the DCP750 must be in `DOCK` source
for charging. This behavior still needs confirmation with each iPod and
Rockbox build.

Sources:

- [Philips DCP750/850 service manual](https://retronik.silicium.org/DOCUMENTS/Audiovideo/Philips/Philips-DCP-750-Service-Manual.pdf)
- [Philips DCP750/850 user manual](https://www.documents.philips.com/assets/20231121/c0e83f6ea9c74100b69cb0c1007a2336.pdf)
- [Philips DCP750/850 quick-start guide](https://www.documents.philips.com/assets/20231118/30eb937d8f4f490b9f23b0be01274b9b.pdf)

### Verified in this Rockbox tree

The existing transport path is:

```text
DCP750 keys
    |
    | 30-pin UART / iAP packets
    v
target serial driver -> apps/iap -> BUTTON_RC_* -> normal action/keymap path
                                             +-> /iap-trace.txt

Rockbox PCM -> target codec line-out -> 30-pin analog L/R -> DCP750 amplifier

DCP750 external power -> dock power pins -> target charger/power management

Rockbox framebuffer -> internal LCD driver -> internal panel
                   \-> external CVBS backend (not implemented)
```

The current tree already provides:

- serial iAP support on both targets;
- serial bitrate selection including automatic detection;
- accessory 3.3 V supply control;
- codec line-out power control;
- Simple Remote and Extended Interface playback command translation to
  `BUTTON_RC_*` events;
- merging of remote and physical clickwheel events rather than replacement of
  physical input;
- a `Debug IAP` menu;
- a 64-packet timestamped iAP trace ring and `/iap-trace.txt` export;
- serial receive/error/relaunch/dropout counters on the 6G target;
- settings for accessory power, line-out, and serial bitrate.

The iPod plugin keymaps also accept standard iAP remote events in Desktop Mode
and Live TV without replacing clickwheel input.  Before a DCP750 trace exists,
the conservative receiver-style mapping is:

| DCP/iAP event | Live TV while watching | Live TV guide | Desktop Mode |
| --- | --- | --- | --- |
| Previous / rewind | Previous playable channel | Previous programme/time | Previous control / left |
| Next / fast-forward | Next playable channel | Next programme/time | Next control / right |
| Play/pause | Full guide | Tune selected channel | Click / open |
| Select | Information banner | Tune selected channel | Click / open |
| Up / down | Volume up / down | Previous / next channel row | Scroll vertically |
| Menu / stop | Guide / exit | Leave guide | Back / Apple menu |

Philips only documents play/pause, search, and previous/next as docked-iPod
controls.  The other rows are supported if the DCP750 actually transmits those
standard iAP events; the physical trace remains authoritative.

The DCP750 manual assigns the numeric keypad to direct disc title, track, or
chapter entry and does not list it among the controls available for a docked
iPod.  The Simple Remote contextual-button bitmap likewise has no numeric-key
bits.  Direct Live TV channel entry therefore requires a trace proving that the
DCP750 emits a distinct proprietary or dedicated-media packet for each digit;
without such packets, iPod software cannot observe those infrared keypresses.

The iPod Video LCD driver names two BCM2722 TV bitmap commands and one movie-off
command:

```text
BCMCMD_TV_PALBMP
BCMCMD_TV_NTSCBMP
BCMCMD_TV_MVOFF
```

It also names likely TV framebuffer and bitmap-data addresses. These constants
were added from Apple diagnostic reverse engineering, but neither the tree nor
the historical patch documents the required payload, buffer layout, setup
sequence, color format, stride, synchronization, or shutdown sequence. The
current driver never invokes the commands.

The S5L8702 headers identify `CG16_SVID`, and the current experimental target
backend contains candidate compositor, router, encoder, pin-mux, and NTSC
tables. Normal settings force the backend off; only the staged debug item can
write the hardware. The official encrypted reference package is reproducibly
identified by `tools/ipod6g_stock_firmware_verify.py`, but the decrypted OSOS
artifact and annotated disassembly still need to be reproduced. A build and
encrypted-container match are not sufficient evidence for analog output.

### Upstream comparison

Current upstream Rockbox contains a newer standalone USB iAP stack under
`firmware/usbstack/iap/`. This personal tree instead has an extended app-layer
iAP implementation and a USB HID transport feeding it. The upstream USB work
is relevant to general iAP transport separation and USB regression testing,
but it does not provide DCP750 support: the Philips schematic shows that the
DCP750 uses UART, analog line-out, and analog CVBS, not USB iAP.

The historical Rockbox change that introduced the dormant BCM2722 TV command
names does not contain a working TV-out call site. FreeMyiPod/emCORE sources
were also searched; they contain internal-display support but no usable SVID,
CVBS, or external-display driver for the Classic.

References:

- [Current upstream Rockbox](https://github.com/Rockbox/rockbox)
- [Upstream USB iAP merge](https://github.com/Rockbox/rockbox/commit/3bb6566)
- [FreeMyiPod](https://freemyipod.org/wiki/Main_Page)
- [BCM2722 TV constant history](https://github.com/Rockbox/rockbox/commit/c567fc)

### Unverified hypotheses

- The DCP750 probably emits standard Simple Remote commands for its iPod-facing
  keys. The exact commands and repeat behavior must come from a trace.
- A powered DCP750 may enable its UART using its dedicated control line. The
  negotiation and idle bitrate must come from a trace.
- The 5G BCM2722 is capable of television output, but the dormant constant
  names alone do not prove that Rockbox can safely upload a bitmap.
- The 6G silicon may include an external-video block associated with `SVID`,
  but no safe programming contract is currently known.
- The DCP750 may not satisfy the authentication behavior expected by stock 6G
  firmware because Philips only advertised 5G compatibility. This is not a
  reason to bypass an unknown authentication or hardware sequence blindly.

## Current capability matrix

`Implemented` means a code path exists, not that it has been tested on this
dock.

| Capability | iPod Video 5G/5.5G | iPod Classic 6G/7G | DCP750 result |
| --- | --- | --- | --- |
| Analog line-out enable | Implemented | Implemented | Not tested |
| Accessory 3.3 V supply | Implemented | Implemented | Not tested |
| External-power detection | Implemented | Implemented | Not tested |
| Battery charging | Implemented by target PMIC/charger path | Implemented by target PMIC/charger path | Not tested |
| UART iAP receive/transmit | Implemented | Implemented with recovery counters | Not tested |
| Remote plus clickwheel input | Implemented | Implemented | Not tested |
| iAP packet trace | Implemented | Implemented | Not captured |
| Internal LCD | Implemented | Implemented | Not applicable |
| Solid-color CVBS | Hardware path suspected | Staged diagnostic, unqualified | Not tested |
| Static framebuffer CVBS | Dormant command names only | Staged diagnostic, unqualified | Not tested |
| Continuous framebuffer mirror | Not implemented | Backend path present but locked off | Not tested |
| USB iAP through DCP750 | Not a dock transport | Not a dock transport | Not applicable |

## Mandatory proof-of-concept sequence

Do not skip ahead. Each stage must leave enough evidence to reproduce or reject
the next stage. A failed stage is a result, not permission to try unrelated
registers.

### Stage 0: establish the stock reference

Test the 5G first because it is the only model Philips documented as compatible.
With Apple firmware, set TV output on and NTSC, power the DCP750, select its
`DOCK` source, and play a known-good local video. Record:

- exact iPod model and firmware version;
- whether the dock identifies the iPod or displays an error;
- whether video appears and its aspect ratio, crop, orientation, and color;
- whether analog audio plays;
- whether the charging indicator is stable;
- every working and non-working DCP750 remote key;
- whether the iPod's own controls remain usable.
- whether Live TV Previous/Next changes one channel per press, Play opens the
  mini guide, Menu returns to the full guide, and guide selection remains
  responsive while the picture-in-guide continues playing;
- whether Desktop Mode Previous/Next moves focus and Play opens the focused
  control without disabling the clickwheel.

Repeat on the 6G/7G only after recording the 5G reference. Do not infer 6G
compatibility from a successful 5G test.

### Stage 1: capture Rockbox dock negotiation

Use an otherwise unmodified Rockbox build. Enable accessory power and line-out,
set serial bitrate to automatic, and dock while the DCP750 is powered and in
`DOCK` source. Open `System > Debug > Debug IAP`; this freezes and exports the
trace to `/iap-trace.txt`. Record the debug-screen fields and preserve the file.

Run one capture without pressing remote keys. Run a second capture pressing,
one at a time, play/pause, previous, next, menu, up, down, left, right, and
select. Allow at least one second between keys so repeats and unsolicited
notifications are distinguishable.

The trace must establish the observed lingo, command bytes, direction, timing,
and reconnect behavior before adding a DCP750-specific command mapping.

### Stage 2: synchronous solid color

Only after obtaining a documented target programming sequence from an Apple
firmware trace, executable diagnostic, datasheet, or a previously working open
implementation, create a target-local probe that:

- saves every register or state value it changes;
- enables only the documented clock, pin, encoder, and buffer path;
- displays one solid color synchronously;
- times out and restores all saved state;
- leaves the internal LCD and audio path running;
- performs no continuous DMA and allocates no playback memory.

Test black first, then a dim primary color. Do not start with full-white output.
Record current draw, heat, display lock, and recovery after undocking.

### Stage 3: test pattern

Show a fixed low-risk pattern with color bars, one-pixel and eight-pixel borders,
center crosshairs, and orientation labels. Determine:

- color order and pixel packing;
- active width, height, and stride;
- scaling and aspect-ratio behavior;
- safe-area crop and overscan;
- PAL/NTSC mode behavior;
- whether the dock needs a command or only continuous CVBS.

### Stage 4: static framebuffer

Copy a snapshot of the Rockbox framebuffer into a dedicated external-display
buffer. The buffer must be statically reserved or use a non-playback allocation
whose lifetime and maximum size are explicit. It must not call
`plugin_get_audio_buffer()`, shrink audio buffers, stop playback, restart
playback, or reuse codec/plugin memory.

### Stage 5: manual refresh

Bind a temporary debug action to one external refresh. Exercise menus, WPS,
lists, the keyboard, USB screens, charging screens, and plugin screens. Confirm
that internal LCD updates are unaffected and that playback does not underrun.

### Stage 6: automatic updates

Hook the proven backend below the drawing APIs, at the target LCD update/update-
rectangle boundary. Do not patch every UI call site. The external-display layer
must accept full updates and dirty rectangles, coalesce rapid updates, and make
no assumption that drawing originates in a particular theme or screen.

### Stage 7: optimize and harden

Measure before optimizing. Bound the refresh rate, skip identical frames or
unchanged rectangles, and prefer DMA only after proving correct cache coherency
and ownership. Add clean enable/disable, dock removal, USB insertion, shutdown,
sleep, and error recovery. A failed external update must never block the UI or
audio thread indefinitely.

## Intended software boundary after hardware proof

The generic interface should remain small and target-neutral:

```c
bool external_display_available(void);
bool external_display_enable(bool enable);
void external_display_update(void);
void external_display_update_rect(int x, int y, int width, int height);
```

Target code owns encoder clocks, pins, timing, transfer format, and shutdown.
Core LCD code owns only update notification and rectangle clipping. The target
backend must degrade to an unavailable/no-op implementation when the hardware
contract is not proven.

An `External display` setting is appropriate only after a target passes through
static framebuffer and manual refresh. Suggested values are `Off`, `Auto`,
`NTSC`, and `PAL`; do not expose modes a backend cannot implement. The local
screen-sleep setting must not implicitly stop external output, and external
display enable must not force the internal backlight on.

## Regression requirements

Before merging a proven backend, build both target configurations and run all
available static-analysis gates for modified files. Simulator coverage can
exercise rectangle clipping, state transitions, and no-op behavior, but cannot
validate composite timing or electrical safety.

The hardware matrix must include:

| Scenario | 5G/5.5G | 6G/7G |
| --- | --- | --- |
| Boot undocked | Pending | Pending |
| Boot docked, dock unpowered | Pending | Pending |
| Boot docked, dock powered | Pending | Pending |
| Dock/undock while idle | Pending | Pending |
| Dock/undock during playback | Pending | Pending |
| Line-out audio continuity | Pending | Pending |
| Remote key press and repeat | Pending | Pending |
| Simultaneous clickwheel input | Pending | Pending |
| Charging with screen on/off | Pending | Pending |
| USB connection while docked | Pending | Pending |
| Screen sleep with external video | Pending | Pending |
| Shutdown while docked | Pending | Pending |
| NTSC/PAL switching | Pending | Pending |
| Menus, WPS, plugins, keyboard, USB UI | Pending | Pending |
| One-hour playback/mirroring stress | Pending | Pending |

Also verify normal USB mass storage/HID behavior undocked and with unrelated
accessories. The DCP750 has no USB transport, but a display change must not
regress the shared firmware USB state machine.

## Safety stop conditions

Immediately stop a hardware probe if any of these occur:

- the iPod or dock heats abnormally;
- current draw rises unexpectedly;
- the internal LCD loses synchronization;
- audio stops, clicks repeatedly, or playback buffers underrun;
- controls lock up or shutdown no longer completes;
- undocking does not restore the previous state;
- a changed register cannot be restored from a recorded prior value.

No physical result should be marked `Pass` unless the exact model, build,
procedure, and observed outcome are recorded. A simulator or successful build
is not evidence that analog video, charging, or remote commands work on a
DCP750.

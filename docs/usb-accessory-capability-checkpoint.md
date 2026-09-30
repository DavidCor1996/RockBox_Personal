# Classic 7G USB accessory capability checkpoint

Prepared 2026-09-30. Physical results pending. The requested 7G / camera kit /
powered hub / DualSense / Plugable USB-VGA-165 chain remains **unproven**.
This pass stops at section 28A of the supplied research handoff.

## Diagnostic contract

`USB_CAPABILITY_SNAPSHOT_BUILD` enables a passive diagnostic only in native
`ipod6g` firmware. Normal builds, simulators and bootloaders omit it. This
build does not enable `USB_HOST_ROLE_PROBE_BUILD`.

Open **System > Debug (Keep Out!) > USB capabilities (read only)**. The screen
reads cached RAM, never live USB registers. During ordinary DEVICE driver
initialization, after its existing clock/PHY hooks and `PCGCCTL` ungating,
a target hook reads GSNPSID and GHWCFG1–4 twice. It adds no USB MMIO writes,
reset, role forcing, PHY setup, ADC routing, charge-limit changes, transfers,
logging or allocation. Existing normal DEVICE initialization still performs
its ordinary hardware setup. If it has not run, the screen reports no snapshot.
Connect to a computer normally, unplug, and reopen the screen to obtain one.

The last raw snapshot survives normal USB shutdown. Each capture compares
its two reads and the previous capture; an inconsistency stays latched until
reboot. Briefly masking interrupts protects the UI's RAM copy, with no waits
or hardware accesses inside that copy. The hook uses two small stack objects
and one persistent snapshot, with no playback/plugin buffer ownership.

GSNPSID must identify the DWC2 OTG family (`0x4f542xxx`). Missing, reserved,
suspicious or inconsistent readings are inconclusive. Preserve every raw word
even if decoding fails. GHWCFG1 zero can be legitimate. Decode:

| Field | Encoding |
| --- | --- |
| Operation mode | GHWCFG2 bits 2:0 |
| DMA architecture | GHWCFG2 bits 4:3: 0 slave, 1 external DMA, 2 internal DMA, 3 reserved |
| Host channels | GHWCFG2 bits 17:14 plus one |
| Dynamic FIFO | GHWCFG2 bit 19 |
| FIFO capacity | GHWCFG3 bits 31:16, in 32-bit words |

Modes 0/1/2 permit further dual-role investigation. Modes 3/4 mean device-only
and stop a software-only host implementation once identity/read validity is
verified. Modes 5/6 mean host-only and need scrutiny on a normally functioning
USB-device iPod. Mode 7 is undefined. Channel count cannot override mode.
No result here proves adapter activation, power safety or a host transaction.

## Current-source reconciliation

Base commit at the start of this pass:
`b44ccf7814861c12fec04283ef482840f03a7534`, branch
`codex/6g-upstream-sync-before-video`, with extensive existing personal changes.
The older public `cee72c7` tree has not replaced this checkout.

The existing September Phase 2A role probe is an active experiment: it enables
clocks, resets the core, forces HOST, observes HPRT and restores DEVICE.
It records GSNPSID/GHWCFG2 only. It cannot satisfy this earlier passive gate.
Its implementation and prior artifacts remain separate and unmodified.

The current iPhone host experiment still has these source-level limitations:

| Finding | Current evidence / required follow-up |
| --- | --- |
| FIFO overlap | RX `[0,0x360)`, nonperiodic TX `[0x360,0x3e0)`, periodic TX `[0x100,0x4e0)` from `HPTXFSIZ=(0x3e0<<16)\|0x100`; do not reuse. Plan disjoint intervals against measured capacity and queue requirements later. |
| EP0 sizing | `control_transfer()` uses 64 for all stages; generic enumeration must first read and honor bMaxPacketSize0 and speed. |
| Single-device discovery | One address/state, Apple-only VID and ipheth/usbmux interface binding; no hub or interrupt endpoint scheduler. |
| Blocking service | Channel polling can take `HZ/2`, plus halt waits; unsuitable for input/display scheduling. |
| DMA | Static aligned buffers and S5L physical translation/cache operations exist; rounded IN sizes, buffer lifetime after failed halt, and cancellation require review before reuse. |
| Power/role | Core reset/forced HOST and HPRT power are already active operations; no physical VBUS/kit qualification. Do not use them for this snapshot. |
| iAP | Pre-auth UART StartIDPS rejection, all-zero 64-bit options for 0x4B, UART EndIDPS rejection/no status-3 switch handler, and disabled lingo 0x06 remain. Preserve Kokkia fallback; do not advertise a host backend that does not exist. |

The fork retains its current `0x2f0000` plugin region (3 MiB minus 64 KiB)
and `0x100000` codec region. No plugin API,
framebuffer, game frontend, audio lifecycle or UI animation change is needed
for this checkpoint. DisplayLink and DualSense parsers/drivers are deferred.

## Sources and next host boundary

No third-party runtime was imported for this pass. The new diagnostic and
fixtures are GPL-2.0-or-later. Field encodings were checked against the supplied
[Linux DWC2 definitions at 551c722](https://github.com/torvalds/linux/blob/551c722f40809618230001baccf219193e22fc5a/drivers/usb/dwc2/hw.h).
The local role/desktop work had selected an older TinyUSB revision; the supplied
handoff pins
[TinyUSB 011591a](https://github.com/hathach/tinyusb/blob/011591a25891ee50d8565399fab5331d3041b0f7/src/portable/synopsys/dwc2/hcd_dwc2.c).
Its DWC2 host driver contains DMA/cache hooks, hub split state and periodic
scheduling. `hcd_init()` still waits for HOST without a deadline and enables
HPRT power; queue availability also has polling loops. It cannot be invoked
unchanged for a bounded, electrically unqualified Rockbox diagnostic.

After passing capability **and** adapter/power gates, prefer one MIT TinyUSB
DWC2/hub/HID port behind the Rockbox USB worker and target clock/IRQ/cache layer.
Audit its platform selector, custom OS adapter, physical DMA addresses, channel
limits, FIFO/EP-info capacity, cancellation and bounded initialization first.
Keep the existing device stack and one exclusive hardware owner. Start with
one bounded GET_DESCRIPTOR session with clean PC-device recovery, then hub
enumeration. UART negotiation must have a separately opted-in session and
actual adapter traces before changing Kokkia-compatible behavior. No host
stack or peripheral feature is qualified by this source inspection.

## Build and first physical evidence

Run `tools/build_usb_capability_snapshot.sh` in a fresh build directory. It
produces firmware, ELF, build information, source hashes and a manifest under
`.rockpod-private/usb-capability-20260930/` by default, without deployment.
`make bin` creates the firmware; it is not a full codec/plugin package.
Keep the matching personal runtime and known-good recovery firmware.
The manifest records exact checksums, `hardware_tested=false`, `deployed=false`.

Before an authorized firmware-only deployment, back up both installed firmware
copies. Repository rules require updating **both** `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`, verifying their SHA-256 values against this build,
then syncing/ejecting. A full package deploy must use
`tools/deploy_ipod6g_preserve_database.sh`; never replace `.rockbox` wholesale.
Do not install this personal diagnostic in the official upstream application
slot. Roll back by restoring both backed-up firmware copies and checking them.

David's smallest required observation:

1. Boot without accessories. Open the passive screen and photograph all raw
   words, build ID, capture count and consistency. If unavailable, perform an
   ordinary computer USB connection, unplug, and reopen it.
2. Repeat the normal PC connect/unplug once; capture again. Verify normal PC
   enumeration/reconnection and music playback still work. Return both sets
   of raw words, not just the decoded verdict.
3. In **Debug > View SysCfg**, record only model number, HwId and HwVr. Do not
   share personal serial-number fields. Record exact USB/SD adapter markings,
   hub model, supply rating and upstream/downstream topology separately.

No camera kit or powered hub is needed for the passive register checkpoint.
Do not run the active Phase 2A probe before these results are reviewed. Host
communication, controller reports, SD reads, VGA patterns and simultaneous
operation remain later physical gates.

## Verification

The capability fixtures exercise all eight operation modes, wrong identities,
no captures, inconsistent reads, reserved DMA, absent FIFO and suspicious raw
words, plus consistency latching and capture-count saturation. Run them with
`bash tools/usb_dw_capability_tests.sh`. These are software classification
tests, not physical USB results.

This pass completed both a fresh diagnostic ARM firmware build and a fresh
normal ARM firmware build. The fixtures passed with AddressSanitizer and
UndefinedBehaviorSanitizer (leak detection disabled because of the sandbox
tracer; the tested code performs no allocation). The normal ELF has no new
capability or active-role-probe symbols. The diagnostic ELF has a 28-byte
cached snapshot and no active-role-probe symbols. Its firmware size is
3,406,308 bytes and total reported RAM usage is 12,149,464 bytes. Build ID:
`b44ccf7814M-260930`. Existing unrelated compiler warnings remain. Scoped
whitespace checks passed; the pre-existing whole-tree diff also has whitespace
findings in generated maps and unrelated changes.

Exact artifact checksums and source hashes are in the saved manifest.
Physical boot, PC reconnect, audio and capability readings remain untested.

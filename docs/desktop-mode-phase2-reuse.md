# Phase 2 open-source reuse audit

Reviewed 2026-09-08, following the requirement to reuse existing peripheral
support. **Preferred implementation: port TinyUSB's host stack to the Rockbox
S5L8702 target, then adapt an existing DL-1xx driver.** Keep new code focused on
Rockbox/board integration, resource ownership and the existing Desktop Mode
interfaces. A fresh hub, HID or DisplayLink stack is not the default plan.

This is a source audit and integration decision. These projects have not been
compiled for or physically tested on this iPod. Gate 2A still comes first.

## What already exists and what to reuse

| Component | Existing implementation | Decision |
|---|---|---|
| S5L8702 clocks/PHY and normal USB DEVICE | This tree's `usb-s5l8702.c`, `usb-designware.c`, `usb-designware.h` | Reuse target hooks and preserve the working DEVICE stack; qualify HOST through the small role probe |
| DWC2 host transfers and split scheduling | [TinyUSB HCD](https://github.com/hathach/tinyusb/blob/84330416a229ed608c5ff3741bcc4bcef1d8fda0/src/portable/synopsys/dwc2/hcd_dwc2.c) | Preferred host engine for 2B/2C; port the platform boundary, with reviewed timeout/power changes |
| Hub enumeration and port management | [TinyUSB hub](https://github.com/hathach/tinyusb/blob/84330416a229ed608c5ff3741bcc4bcef1d8fda0/src/host/hub.c) | Reuse its class driver, including transaction-translator handling; adapt capability callbacks |
| Keyboard and mouse transport | [TinyUSB HID host](https://github.com/hathach/tinyusb/blob/84330416a229ed608c5ff3741bcc4bcef1d8fda0/src/class/hid/hid_host.c) | Reuse endpoint/report transport; feed existing generic desktop events; initially qualify Boot Keyboard/Mouse |
| DL-165/other DL-1xx initialization, modes and uploads | [tusb_libdlo](https://github.com/ianhan/tusb_libdlo/tree/b1d4ed2408cb8f64a7aafd04716a020a30683321) | First C implementation to adapt once TinyUSB is available; reuse mode/programming and encoding code where suitable |
| Alternative DL-1xx implementation | [Pico_USB_Disp protocol backend](https://github.com/htlabnet/Pico_USB_Disp/blob/3d76208f8f191a74c88f63ee4db533479b07511f/src/usb_disp_prot_dl-1xx.cpp) | Concrete MIT-licensed alternative if the libdlo port's dependencies or ownership model are too costly; adapt this backend rather than devise a new protocol |
| DL-165 protocol behavior and known pitfalls | [EspUsbHost's DL-1xx notes](https://github.com/tanakamasayuki/EspUsbHost/blob/6992fc668e3fe545a6f1e11a35cfab3910f4e894/docs/usb-display-spec.md) | Use as independent test expectations and endpoint/mode-recovery guidance |
| Mature OS implementation for cross-checking | [OpenBSD udl](https://github.com/openbsd/src/blob/master/sys/dev/usb/udl.c), [Linux udlfb](https://github.com/torvalds/linux/blob/master/drivers/video/fbdev/udlfb.c) | Reference device quirks and register sequences; port only the relevant mechanism, preserving notices |

The Acer hub should be handled through the USB hub class after its descriptors
are known; its marketing name is insufficient to identify a quirk. Likewise,
choose HID interfaces from real descriptors. Do not assume that every keyboard
or mouse uses a Boot report, or that the user's DL-165 has the same product ID
as a developer's example.

## TinyUSB port: reuse the engine, implement the platform boundary

The inspected [DWC2 port selector](https://github.com/hathach/tinyusb/blob/84330416a229ed608c5ff3741bcc4bcef1d8fda0/src/portable/synopsys/dwc2/dwc2_common.h)
has no S5L8702 case. It requires a controller table and platform PHY/clock/IRQ
hooks. That establishes a missing port boundary in this revision; it does not
establish that no other public fork has ever experimented with this SoC.

After Gate 2A passes:

1. Vendor a pinned, minimal TinyUSB host subset with its MIT notices, source
   revision and a small local patch series. Use its host enumeration, DWC2 HCD,
   hub and HID code. Do not bring in its device stack to replace Rockbox USB.
2. Add an S5L8702 port for the observed controller revision/capabilities, MMIO
   base, UTMI width, clocks, PHY reset and IRQ routing. Reuse Rockbox hooks.
3. Use TinyUSB's [custom OS adapter](https://github.com/hathach/tinyusb/blob/84330416a229ed608c5ff3741bcc4bcef1d8fda0/src/osal/osal.h)
   for Rockbox time, queues, mutexes and worker scheduling. It explicitly
   provides `OPT_OS_CUSTOM`/`tusb_os_custom.h`. Do not treat a preemptive Rockbox
   worker as an unreviewed bare-metal single-thread loop.
4. Keep the existing USB worker responsible for choosing controller ownership.
   Only the selected host or device handler may service USB IRQs. Cancel and
   retire outstanding transfers before changing ownership. Retain default
   DEVICE behavior and a complete shutdown/error path.
5. Review HCD initialization before calling it: this revision writes HPRT
   power during initialization, and its role/reset paths contain polling loops
   without deadlines. Add target-appropriate, bounded failure propagation and
   an explicit power policy after the physical gate. The role probe deliberately
   does less than `hcd_init()` and cannot be replaced by calling it unchanged.
6. Validate register layout, DMA/cache requirements and fixed endpoint/FIFO
   allocations against the actual chip. Qualify hub-only first, then HID and
   split transactions. Retain software fault tests around the adapter.

This plan uses the existing Phase 1 `desktop_usb` types as application-facing
contracts/test helpers where useful. They need not become a competing host
scheduler. If an existing helper duplicates the imported stack's enumeration
or hub lifecycle, use the imported implementation and retain only the necessary
capability/input adapter and useful tests.

## DisplayLink port: concrete candidates and integration costs

`tusb_libdlo` is already a TinyUSB conversion of libdlo and supports the
DL-1x0/DL-1x5 families. Its [CMake configuration](https://github.com/ianhan/tusb_libdlo/blob/b1d4ed2408cb8f64a7aafd04716a020a30683321/src/CMakeLists.txt)
selects 16bpp scanout and mixed encoding and exposes command-buffer sizing.
The library is C; the provided build packaging expects Pico SDK's
`tinyusb_host` target. Adapt the Rockbox build definitions without importing
Pico firmware/examples.

The [transport implementation](https://github.com/ianhan/tusb_libdlo/blob/b1d4ed2408cb8f64a7aafd04716a020a30683321/src/dlo_usb.c)
uses TinyUSB private endpoint APIs, synchronous control helpers, allocator
hooks, weak mount callbacks and a transfer-drain loop that calls `tuh_task()`.
These require a pinned compatible TinyUSB revision and review of reentrancy,
bounded waits, cancellation and DMA-safe buffer ownership in Rockbox. Its
default 16 KiB allocation is split into two halves; Phase 1's helper instead
reserves two separate 16 KiB buffers. Select **one** buffer owner and budget,
not both allocations. Reuse mode/encoding functions while adapting transport
completion behavior to the USB worker.

If that port is disproportionately intrusive, use the MIT
`usb_disp_prot_dl-1xx.cpp` backend from Pico_USB_Disp as the alternative. It
already separates protocol operations through HAL control/bulk functions, but
uses shared scratch storage with a single-caller assumption. The surrounding
C++/platform wrappers and that ownership rule require adaptation. The existing
Rockbox desktop supplies rendering; PicoGraph's ISA/VGA emulation and external
GUI frameworks are unnecessary for this project. [PicoGraph](https://github.com/ianhan/picograph)
is useful evidence of the TinyUSB/libdlo composition, not a required subsystem.

Keep the Phase 1 encoder/EDID test vectors as regression inputs when adopting
the selected driver. Preserve the desktop surface/damage contract so applications
do not depend on which DL implementation is underneath. Do not maintain two
active mode encoders or reinvent register sequences already present upstream.

The EspUsbHost notes report three especially useful checks from another DL-165:
select the intended command endpoint when multiple bulk OUT endpoints exist;
static output persists without keepalive traffic; and monitor-side disconnect
can require mode reprogramming, beyond resending pixels. Treat these as test
cases for this adapter, not as physical results from our iPod/Eyoyo setup.

## Licensing and pinned evidence

The reviewed files are preserved for inspection under
`.rockpod-private/desktop-phase2/references/`. The reference manifest records
repository URLs, exact commits and per-file hashes. These snapshots are not
linked into firmware. Preserve the applicable notices when adapting code.

| Project | Pinned revision | Notices observed |
|---|---|---|
| TinyUSB | `84330416a229ed608c5ff3741bcc4bcef1d8fda0` | MIT |
| tusb_libdlo | `b1d4ed2408cb8f64a7aafd04716a020a30683321` | Repository GPL-2.0; libdlo source headers retain GNU Library GPL version 2 notices |
| Pico_USB_Disp | `3d76208f8f191a74c88f63ee4db533479b07511f` | MIT, with bundled third-party notices; retain relevant file attribution |
| EspUsbHost | `6992fc668e3fe545a6f1e11a35cfab3910f4e894` | MIT |

No proprietary DisplayLink SDK, binary driver, firmware blob or new third-party
runtime dependency was introduced. New runtime code at this stop point is the
bounded role diagnostic and its target/worker/UI adapter. It reuses local PHY
hooks and leaves peripheral drivers for the gated port above.

Next week's decision is therefore concrete: collect Gate 2A evidence, then
start the TinyUSB platform port. Before writing a peripheral mechanism, check
this matrix and the pinned implementation. Record a specific incompatibility
before choosing a custom replacement.

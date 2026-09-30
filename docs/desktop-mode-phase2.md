# Desktop Mode Phase 2 — specification and hardware handoff

Updated 2026-09-08. **Stopped at Gate 2A, awaiting physical testing.**
Phase 2A is implemented and built locally; it has not been installed or run on
an iPod. No physical device was modified during this phase. Phase 1's installed
firmware remains the last verified deployment. Build evidence is recorded in
`.rockpod-private/desktop-phase2/manifest.json` and `verification/`.

## Goal and constraints

Make the existing Desktop Mode work through this chain:

```
iPod Classic 6G -> genuine Apple A1362 CCK -> externally powered Acer USB hub
                                                  |-> USB keyboard
                                                  |-> USB mouse
                                                  `-> DL-165 -> VGA -> Eyoyo
```

The hub supplies its peripherals; the iPod remains battery powered. No hardware
modification, intentional VBUS backfeed, or assumption of charging through the
CCK. The physical wiring/power behavior is not yet verified. Start with manual
activation; automatic activation remains Off until the complete path is proven.

Preserve normal computer USB DEVICE mode, click wheel, music, hardware MP4,
composite, existing laptop USB display, photo access restrictions, and the
Phase 1 desktop. Reuse the existing menu bar, input abstractions, rendering
surface and damage tracking. Decorative or display work must not steal playback
memory. USB storage, Ethernet and gamepads are outside this initial scope.

## Milestones and acceptance gates

| Stage | Implementation | Evidence required before advancing |
|---|---|---|
| **2A: role diagnostic — implemented** | Opt-in iPod firmware; force HOST, observe controller/port, restore DEVICE; no USB peripheral transfers | Real HOST entry and DEVICE restoration, baseline and CCK/hub logs, ordinary computer USB reconnection |
| 2B: root hub enumeration | Arbitrate controller ownership, qualify port power behavior, initialize FIFOs/channels, EP0 control transfers, enumerate directly connected Acer hub | Stable device/configuration/hub descriptors, bounded reset/transfer failure recovery, detach and computer reconnect |
| 2C: hub ports and HID | Hub interrupt endpoint and per-port power/debounce/reset, transaction translators where needed, Boot Keyboard/Mouse polling | Each device alone and together, key up/down/modifiers/rollover, pointer buttons/wheel, repeated hotplug without stuck input |
| 2D: DL-165 transport | Identify interfaces/endpoints, EDID reads, supported mode setup, completion-owned bulk command buffers | Verified monitor timing, 640x480 RGB565 test image, correct color/stride, partial updates, unplug during upload |
| 2E: desktop integration | Allocate external surface, connect damage sink and generic input, expose live capabilities, then enable requested activation policies | Full desktop use, all detach combinations and re-entry, music/MP4/composite/USB DEVICE regression tests |

Each stage stops on its first unverified hardware dependency. Do not implement
an assumed VBUS drive sequence, hub speed, transaction translator behavior or
DL initialization sequence as if this exact hardware has already passed it.

The Phase 1 report originally grouped role switching and hub enumeration into
one first test. This specification splits them: **2A checks role switching
before 2B introduces port power, peripheral reset or transfers.** This narrows
the first failure to the SoC/PHY/adapter boundary.

## Implemented 2A contract

`USB_HOST_ROLE_PROBE_BUILD` enables `HAVE_USB_HOST_ROLE_PROBE` only for native
ipod6g firmware. Normal firmware, simulator and bootloader configurations omit
it. The diagnostic adds no plugin API fields and does not renumber existing USB
events. Opening the diagnostic does nothing until Select is pressed.

Location: **System -> Debug (Keep Out!) -> USB role probe (Phase 2A)**.

- The UI queues a request; all register access and waits execute on the existing
  USB worker. The tick and IRQ handlers do not perform the sequence.
- Start is refused while a USB cable/session is detected, storage handoff is
  active or pending, the existing iPhone host experiment is active, or an earlier
  role restoration failed. Cancel before worker entry avoids register access.
- Disable only the controller's USB interrupt. Reuse the target's clock/PHY
  hooks and UTMI-16 configuration with turnaround 5; allow normal global
  interrupts and scheduler operation throughout the waits.
- Disable controller DMA/global interrupt enable and interrupt masks; wait for
  AHB idle, perform a bounded **controller core reset**, force HOST, and wait
  for the controller's current-mode bit. This core reset is not a peripheral
  USB bus reset.
- Observe the host port read-only for ten seconds; retain whether connection
  was ever observed. Abort if HOST mode disappears or the user cancels.
- Every sequence that begins attempts forced DEVICE restoration, even on
  timeout/cancellation. Cancellation cannot cancel the restoration wait.
  Power down/reset the PHY and gate clocks off afterward. Normal insertion
  subsequently initializes the existing DEVICE stack.
- If DEVICE restoration cannot be observed, suppress subsequent insertion
  handling until reboot. The UI reports this explicitly. Do not retry a failed
  restoration in the same boot.
- Append and flush a small record to `/.rockbox/usb-role-probe.log`. Failed or
  partial log writes are shown as `Log WRITE FAILED`; preserve the screen data
  instead. Logging is skipped for a refused request during an existing USB
  session/storage handoff, so the probe never writes while the computer owns
  storage. The screen identifies this case. No music, photo, desktop or database
  file is edited by the probe.

Each register wait has a one-second deadline, using wrap-safe elapsed time.
A successful observation normally takes about ten seconds; all register waits
combined are bounded to roughly 15.1 seconds plus scheduling/clock-hook time.
Filesystem logging follows controller shutdown and has ordinary filesystem
latency. Leaving the screen requests cancellation; the worker finishes cleanup
without needing the screen to remain open.

**This implementation does not write HPRT, enable root-port power, reset a USB
peripheral, enumerate a hub, schedule host channels, allocate transfer buffers,
use DMA, or send display commands.** A zero connection bit is inconclusive when
port power has not been enabled. Software tests cannot validate electrical
behavior or prove the board's physical VBUS routing.

The report's `restored=1` means CMOD was observed as DEVICE before shutdown.
It does not prove that a computer can enumerate the iPod afterward: that is a
separate required physical check.

## Code map and next implementation boundaries

The [open-source reuse audit](desktop-mode-phase2-reuse.md) selects TinyUSB's
existing DWC2/host/hub/HID implementation as the preferred port and
`tusb_libdlo` as the first DisplayLink candidate, with Pico_USB_Disp as an
alternative. The requirements below describe what that integration must do;
they do not call for writing another host or peripheral stack. Pinned source
snapshots and licensing notes are saved with the artifacts.

| File | Responsibility |
|---|---|
| `firmware/usbhost/role_probe.[ch]` | Pure register-operation sequence, deadlines, cancellation and result records |
| `firmware/target/arm/s5l8702/usb-host-probe.c` | S5L8702 MMIO, existing PHY/clock hooks, Rockbox tick/sleep adapter |
| `firmware/export/usb_host_probe.h` | Manual request/cancel/snapshot boundary |
| `firmware/usb.c` | USB worker arbitration, log persistence, failed-restoration latch |
| `apps/debug_menu.c` | Opt-in diagnostic page |
| `firmware/export/config/ipod6g.h`, `firmware/SOURCES`, `firmware/export/usb.h` | Build guard, source inclusion, appended diagnostic event |
| `tools/tests/desktop_phase2_role.c` | Fault-injected controller tests |
| `tools/build_desktop_phase2_role.sh`, `tools/record_desktop_phase2_build.py` | Fresh diagnostic build and artifact/source manifest |

For 2B, port the existing TinyUSB host engine, retain one controller owner and
keep S5L registers in target code. Adapt the application-facing
`usb_host_operations`/`usb_host_channel` contracts in
`firmware/usbhost/desktop_usb.h`; those Phase 1 helpers still have no installed
transfer backend. Do not activate the separate `usb-iphone-tether.c` experiment
alongside a new host owner. Define ownership transfer, cancellation and channel
drain before enabling interrupts or DMA. Check cache/DMA alignment and bounds
for every transfer. Bound retries for NAK, NYET, STALL and transaction errors;
release channels and buffers exactly once on completion, timeout and detach.
An IRQ should publish completion state; the worker performs policy and waits.

Enumerate only the root-connected hub first: qualify observed speed and port
power/reset behavior, read the initial device descriptor/EP0 packet size,
assign address, read bounded full descriptors, select configuration and read
the hub descriptor. Reject malformed or unsupported descriptors. Stop there
for the second physical gate before adding downstream device scheduling.

For 2C, reuse TinyUSB hub/HID transport and map its reports to the existing
generic input layer. Keep useful Phase 1 parsers/tests, without maintaining
a second hub scheduler. Qualify the existing transaction-translator/split
scheduling if
required by low/full-speed HID behind the high-speed hub; the Phase 1 `split`
field alone does not implement it. Use an explicit supported-topology limit,
not unbounded enumeration. On detach, remove only that device's capabilities
and synthesize releases for held keys/buttons. Map HID usages into the existing
generic desktop events, without putting USB details into desktop apps.

For 2D, adapt the selected existing DL-1xx driver and retain useful
`firmware/usbhost/dl1xx.[ch]` test vectors; EDID control reads and DL mode
programming are not yet integrated. Validate the real
DL-165 identity, firmware behavior and Eyoyo timings before mode writes.
Never infer 640x480 support merely from finding a different detailed EDID mode.
The two 16 KiB command buffers remain owned until transfer completion; retry
failed damage instead of claiming it was displayed.

For 2E, bind `apps/plugins/lib/desktop_surface.[ch]` to the DL transport and the
existing generic input/capability paths. A 640x480 RGB565 surface costs 614,400
bytes; two command buffers add 32,768 bytes before bookkeeping. Establish a
native allocation/lifetime budget before integration. Do not obtain the shared
playback buffer for decorative UI. Start with one surface and partial updates;
measure transfer throughput and audio continuity before tuning scheduling.
Auto policies must react to live capabilities and recover cleanly on detach.
The laptop/device transport remains a separate working path.

## Durable build, rollback and validation

Artifacts are under `.rockpod-private/desktop-phase2/` in this repository:

- `rockbox.ipod`, `rockbox.elf`, `rockbox-info.txt`: test firmware and symbols.
- `manifest.json`: exact artifact/source SHA256 values, base commit and flags.
- `source-snapshot/`: copies of the relevant implementation and build scripts.
- `build-metadata/`: generated Makefile, target configuration and linker map.
- `verification/`: build/test results and the prior Phase 1 deployment log.
- `references/`: pinned upstream source excerpts and a separate hash manifest.
- `rollback/`: previously deployed Phase 1 firmware, info and full package.

These are local ignored artifacts, not an upstream patchset. The working tree
has substantial unrelated personal changes. The bounded source snapshot is an
inspection aid, not a complete standalone checkout. Keep this repository and
the artifact directory for the continuation. No new commit or upload was made.

The saved Phase 1 rollback firmware SHA256 is:

```
14a5aa702b05af5d188074258ce373162b3b4b694645f976e3efc87db6f3519d
```

Build a new test artifact from this repository with:

```bash
tools/build_desktop_phase2_role.sh /tmp/role-probe-next-build \
    .rockpod-private/desktop-phase2-next
```

Use a fresh build directory. The script refuses to overwrite a recorded
artifact directory, enables the diagnostic explicitly, builds the binary and
records the artifacts. It does **not** deploy. Use an absolute artifact path
when invoking from outside this repository. Do not use `make reconf` on the
diagnostic build, which can discard its explicit extra define.

Validation completed for this implementation:

- Fresh native ipod6g diagnostic build; normal ipod6g and simulator core builds.
- ASan/UBSan pure role tests: success, AHB busy, stuck reset, HOST timeout,
  lost HOST mode, failed DEVICE restore, 1,001 cancellation points, cancellation
  during a restore failure, clock wraparound and register-write restrictions.
- Phase 1 pure C regression vectors for hierarchy, geometry, surface
  backpressure, descriptors, hub, HID, capabilities, EDID and DL encoding.
- Shell/Python syntax checks and scoped whitespace checks.

Commands for the pure tests:

```bash
ASAN_OPTIONS=detect_leaks=0 tools/desktop_phase2_role_tests.sh
ASAN_OPTIONS=detect_leaks=0 tools/desktop_phase1_host_tests.sh
```

LeakSanitizer cannot run under this sandbox's tracing environment; address and
undefined-behavior sanitizers remain enabled. The tested role engine does not
allocate heap memory. These tests model controller responses; they are not
hardware evidence. The diagnostic page and normal USB recovery still need
physical validation. Existing unrelated build warnings are retained in logs.

## Next week's first session: Gate 2A

1. Read this document and the artifact manifest. Compare source snapshot hashes
   with current files before rebuilding; unrelated work may have moved on.
   Record the actual iPod, A1362, Acer hub/power supply and cable identities.
   Have only the iPod, CCK and powered hub for this gate; leave DL-165, keyboard
   and mouse disconnected. Confirm the intended no-backfeed wiring before use.
2. Before installing anything, prove ordinary computer USB access. Capture the
   current firmware at **both** paths, config and codec/database checksums and
   a rollback copy outside the mounted iPod. The saved Phase 1 package is an
   additional fallback, not proof that next week's installed files are the same.
3. Install the exact recorded test firmware at both `/rockbox.ipod` and
   `/.rockbox/rockbox.ipod`. Verify both hashes against the manifest before
   sync/eject. A firmware-only diagnostic does not require replacing plugins
   or the runtime directory. If a full package is needed, use
   `tools/deploy_ipod6g_preserve_database.sh`; never replace `.rockbox`
   wholesale. Existing database/tagcache files must remain readable and
   unchanged and `tagcache_autoupdate` must stay enabled.
4. Cold boot the diagnostic with all USB accessories unplugged. Open the Debug
   page and press Select. For **run A**, leave it unplugged through completion.
   Record the result, elapsed time, HOST/DEVICE flags and log status.
5. Only if run A reports `result=2`, `host_seen=1`, `restored=1`, perform
   **run B**. Start unplugged, press Select, then attach CCK + already powered
   empty hub during the ten-second observation. Wait for completion, record
   the screen and unplug the hub. A connection bit of zero alone is not failure
   or permission to improvise port-power writes.
6. **Run C:** start unplugged again and leave the page with Menu while it runs.
   Allow two seconds for restoration, reopen it and record `CANCELLED` and
   `restored=1`. Do not start another run until the prior result is complete.
7. With the accessory chain detached, reconnect the computer. Confirm ordinary
   USB enumeration, mounted database readability and unchanged config/codec/
   database hashes. Copy `/.rockbox/usb-role-probe.log` into the artifact
   directory. Record whether music/click wheel and normal boot still behave
   normally; longer combined audio/video testing belongs to later gates.
8. Restore the recorded pre-test firmware at both paths, verify hashes, sync/
   eject and reboot for everyday use. Keep the test binary unchanged alongside
   its evidence. If any code is changed, build and test a new artifact.

Stop on a failed role transition, timeout, freeze, unexpected USB session or
DEVICE restoration failure. For `result=8`, detach accessories and reboot
before reconnecting the computer; the firmware intentionally latches insertion
handling off. If a computer still cannot see the device, stop and use the
established bootloader/disk-mode recovery path to restore the saved firmware.
Do not continue to hub enumeration, display mode writes or peripheral tests.

Gate 2A passes only after the logs show HOST and restored DEVICE and a computer
subsequently recognizes the iPod normally. A successful gate licenses 2B
implementation; it does not prove hub enumeration or electrical power control.

## Result key and pending evidence

| Code | Meaning | Next action |
|---|---|---|
| 0 | Idle | Select explicitly starts a run |
| 1 | Running/queued | Wait or leave to cancel |
| 2 | Role observation completed | Check HOST/DEVICE flags, then computer reconnection |
| 3 | Refused | Unplug; check existing USB/host state; reboot if restore previously failed |
| 4 | Cancelled | Verify restored flag; a request cancelled before worker start has no role flags |
| 5 | AHB idle timeout | Save log and stop |
| 6 | Core reset timeout | Save log and stop |
| 7 | HOST timeout or HOST lost | Save log and stop |
| 8 | DEVICE restore failed | Unplug and reboot; preserve `operation` as the original outcome |

`id` is GSNPSID, `hw` is GHWCFG2, and `port` is the last HPRT read.
`before`, `host`, `after` are GUSBCFG snapshots. `before` is taken after the
adapter establishes its initial DEVICE/PHY configuration, not at cold boot.
`connected` records any observed connection during the run. `tick` identifies
when the log was written, not a wall-clock timestamp. Refused or pre-start
cancelled runs have zero registers and do not imply a failed hardware restore.

Copy and complete this evidence record next week:

```text
Date / tester:
iPod model / storage / battery:
A1362 identity / Acer hub model / power supply / cabling:
Test firmware SHA256:
Pre-test firmware/config/codec/database evidence location:
Run A, no accessory: result / HOST / DEVICE / elapsed / log:
Run B, CCK + powered empty hub: result / HOST / connected / DEVICE / log:
Run C, cancellation: result / DEVICE / log:
Computer USB access before and after:
Post-test checksum comparison:
Normal boot / click wheel / music observations:
Rollback firmware at both paths verified:
Gate decision: PASS / FAIL / INCOMPLETE (reason):
Next implementation: 2B only after PASS; otherwise diagnose 2A from raw log.
```

## References and provenance

Reuse the local DesignWare target register definitions and PHY/clock hooks in
`firmware/export/usb-designware.h` and
`firmware/target/arm/s5l8702/usb-s5l8702.c`. The existing iPhone host experiment
is context, not evidence that the requested chain works.

Architecture references consulted: [TinyUSB DWC2 host](https://github.com/hathach/tinyusb/blob/master/src/portable/synopsys/dwc2/hcd_dwc2.c)
and [hub worker](https://github.com/hathach/tinyusb/blob/master/src/host/hub.c)
(MIT; no implementation vendored). Later DisplayLink work should use the
protocol sources/provenance listed in `docs/desktop-mode-phase1.md` and verify
actual hardware responses. The role diagnostic and tests are GPL-2.0-or-later.

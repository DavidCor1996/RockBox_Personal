# iPod Nano 3G Hardware Bring-up (Phase 11)

This document defines the Nano 3G hardware-readiness gate for first real-device boots.
It intentionally prioritizes safety over functionality.

## 1) Safe bring-up policy

- Global guard macro: `NAN03G_SAFE_BRINGUP` (set to `1` in `firmware/export/config/ipodnano3g.h`)
- When enabled, Nano 3G target code blocks risky hardware-write paths and prefers fail-safe halt behavior.
- This phase does **not** add real NAND, audio codec, or validated LCD/USB driver programming.

## 2) Hardware touchpoint audit

Classification meanings used here:

- `SAFE`: no real hardware writes, or writes are hard-gated in safe bring-up mode
- `UNKNOWN`: register semantics or electrical behavior still unverified on Nano 3G hardware
- `IMPLEMENTED`: code path is implemented and intentionally active in this phase

| Subsystem | Primary file(s) | Classification | Notes |
|---|---|---|---|
| Early boot trace buffer/failsafe | `firmware/target/arm/s5l8702/ipodnano3g/bringup-nano3g.c` | IMPLEMENTED | In-memory ring trace + `DEBUGF` output + non-returning failsafe halt |
| System preinit/init trace hooks | `firmware/target/arm/s5l8702/system-s5l8702.c` | IMPLEMENTED | Logs boot stages; exceptions route to failsafe halt in safe mode |
| LCD panel-ID read path | `firmware/target/arm/s5l8702/ipodnano3g/lcd-nano3g.c` | IMPLEMENTED | Detects known panel IDs; unknown panel now failsafe-halts |
| LCD controller init/write path | `firmware/target/arm/s5l8702/lcd-s5l8702.c` | SAFE | Entire Nano3G LCD init path skipped in safe mode (no controller/panel writes) |
| Backlight PMU writes | `firmware/target/arm/s5l8702/ipodnano3g/backlight-nano3g.c` | SAFE | On/off/brightness writes blocked in safe mode |
| PMU I2C read path | `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c` | IMPLEMENTED | Reads remain available for status sampling |
| PMU I2C write path / preinit writes | `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c` | SAFE | `pmu_write*`/`pmu_wr*` writes blocked in safe mode |
| PMU register semantics (rails/charging/standby) | `pmu-nano3g.c`, `power-nano3g.c` | UNKNOWN | Values mostly inherited/scaffold; blocked by safe mode pending validation |
| Power-off standby transition | `firmware/target/arm/s5l8702/ipodnano3g/power-nano3g.c` | SAFE | Safe mode converts power-off requests into failsafe halt |
| USB PHY/clock/IRQ register writes | `firmware/target/arm/s5l8702/usb-s5l8702.c` | SAFE | USB init/clock/IRQ writes blocked in Nano3G safe mode |
| Audio codec access shim | `firmware/target/arm/s5l8702/ipodnano3g/cscodec-nano3g.c` | SAFE | Shadow-register model only; no hardware I2C writes |
| PCM/I2S/DMA audio hardware writes | `firmware/target/arm/s5l8702/pcm-s5l8702.c` | SAFE | Nano3G safe mode exits early before I2S/DMA register programming |
| NAND storage path | `firmware/target/arm/s5l8702/ipodnano3g/nand-nano3g.c` | SAFE | Synthetic read-zero/write-denied stub (no NAND controller writes) |
| ADC battery channels | `firmware/target/arm/s5l8702/ipodnano3g/adc-nano3g.c` | UNKNOWN | Still stubbed/unvalidated |
| RTC | `firmware/target/arm/s5l8702/ipodnano3g/rtc-nano3g.c` | UNKNOWN | Config-disabled (`CONFIG_RTC 0`) |

## 3) First hardware boot checklist (safe mode)

1. Confirm safe guard is enabled:
   - `firmware/export/config/ipodnano3g.h` contains `#define NAN03G_SAFE_BRINGUP 1`
2. Build bootloader + firmware image for Nano 3G.
3. Verify no local Phase 12+ experimental changes are mixed in.
4. Install via your existing exploit path (wInd3x/NorBoot workflow).
5. Boot with serial/log capture if available.
6. Expected behavior in Phase 11:
   - no NAND writes
   - no USB PHY bring-up writes
   - no audio/I2S DMA start
   - LCD init may be intentionally skipped in safe mode
   - unknown panel/exception paths halt safely (no reboot loop)

## 4) Runtime safety checks

- If panic/exception occurs on Nano 3G in safe mode, device should halt in failsafe path.
- Boot trace should be present in in-memory buffer and emitted via `DEBUGF`.
- `power_off()` should not enter unverified standby write sequence in safe mode.

## 5) Recovery / DFU guidance

If a test boot hangs or appears unrecoverable:

1. Force reset device:
   - hold `MENU + SELECT` for about 6-10 seconds
2. Enter DFU mode (common Apple sequence):
   - after reset, switch to `SELECT + PLAY/PAUSE` and hold until host detects DFU
3. On host, verify DFU detection (e.g. `lsusb` on Linux).
4. Restore stock firmware with Apple restore tooling (Finder/iTunes or platform-equivalent).
5. Re-test only with `NAN03G_SAFE_BRINGUP 1` images until touchpoints are validated.

Note: exact button timing can vary slightly by unit state and battery level; repeat the sequence if DFU is not detected on first attempt.

## 6) Out of scope for Phase 11

- Real Nano 3G NAND controller + FTL implementation
- Real Nano 3G codec identification/programming
- Real validated LCD panel programming beyond safe-gated scaffolding
- Real USB device-mode bring-up on hardware

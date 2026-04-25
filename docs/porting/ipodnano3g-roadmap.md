# iPod Nano 3G Rockbox Port — Architecture Roadmap

**Status:** Experimental / In-Progress  
**Date:** 2026-04-14  
**Author:** Port roadmap generated from repository audit  
**Target define:** `IPOD_NANO3G`  
**Config header:** `firmware/export/config/ipodnano3g.h`  
**Hardware tree:** `firmware/target/arm/s5l8702/ipodnano3g/`

---

## 1. Device Overview

| Property        | Value                                      |
|-----------------|---------------------------------------------|
| SoC             | Samsung S5L8702 (ARM926EJ-S, same as iPod Classic 6G) |
| CPU speed       | 216 MHz (adjustable)                        |
| RAM             | 32 MB                                       |
| LCD             | 320×240 px, RGB565, 16-bit, 160 DPI         |
| LCD controller  | S5L8702 on-chip parallel 9-bit MPU interface|
| LCD panel IDs   | 0x38B3, 0x38C4, 0x38D5, 0x38E6, 0x58xx    |
| Audio codec     | UNKNOWN (CS42L55 is placeholder — WRONG)    |
| Storage         | NAND flash via internal controller          |
| USB             | DesignWare USB OTG (same as iPod 6G)        |
| Input           | Clickwheel (same physical layout as 4G/6G)  |
| Backlight       | PMU-controlled LED driver (D1671)           |
| PMU             | Dialog D1671 (I2C address 0xE6)             |
| Boot method     | Exploit-based (wInd3x / NorBoot)            |
| Model number    | 117 (Rockbox internal)                      |
| Scramble tag    | `nn3g`                                      |

---

## 2. Subsystem Status Table

| Subsystem        | Status          | File(s)                                          | Reuse From       | Notes |
|------------------|-----------------|--------------------------------------------------|------------------|-------|
| Boot/NorBoot     | Partial         | `bootloader/ipod-s5l87xx.c`, `firmware/target/arm/s5l8702/norboot-s5l8702.c` | iPod6G | Requires wInd3x exploit path; no user installer |
| SoC core         | Reused          | `firmware/target/arm/s5l8702/system-s5l8702.c`, `kernel-s5l8702.c`, `timer-s5l8702.c`, `gpio-s5l8702.c`, `clocking-s5l8702.c` | iPod6G/Nano4G | All S5L8702 shared |
| LCD controller   | Partial         | `lcd-nano3g.c`, `firmware/target/arm/s5l8702/lcd-s5l8702.c` | Nano4G/iPod6G | Panel IDs documented; init sequences implemented in bootloader path |
| LCD simulator    | **Done**        | `sim-ui-defines.h` (entry added Phase 2)         | iPod 6G layout   | 380×500 window, LCD at (30,20) |
| Backlight        | Partial/Stubbed | `backlight-nano3g.c`                             | Nano2G/Nano4G    | PMU register paths need hardware verification |
| Input/Clickwheel | Shared          | `firmware/target/arm/ipod/button-clickwheel.c`   | All iPod S5L8702 | Clickwheel logic shared; button codes in `button-target.h` |
| Button sim map   | **Done**        | `uisimulator/buttonmap/ipod.c`                   | iPod6G coords    | `IPOD_NANO3G` case added Phase 2 |
| PMU              | Partial/Stubbed | `pmu-nano3g.c`                                   | Nano2G/Nano4G    | I2C read/write implemented; power functions stubbed |
| ADC/Battery      | Stubbed         | `adc-nano3g.c`                                   | —                | All functions return 0; hardware channels unknown |
| Power management | Stubbed         | `power-nano3g.c`, `powermgmt-nano3g.c`           | Nano2G template  | charging_state() returns false; voltage curves from Nano2G (likely wrong) |
| Storage (NAND)   | **Stubbed only**| `nand-nano3g.c`                                  | —                | nand_init() returns 0; read/write do nothing; FTL layer missing |
| Audio codec      | **Scaffolded**  | `cscodec-nano3g.c`, `audio-nano3g.c`, `drivers/audio/cs42l55.c` | iPod6G + sim-first Nano3G | Hardware register writes disabled in Nano 3G shim; shadow register model used until real codec path is verified |
| PCM/DMA          | Shared+Hooked   | `firmware/target/arm/s5l8702/pcm-s5l8702.c`, `dma-s5l8702.c` | iPod6G | Shared S5L8702 DMA path retained; Nano3G output scaffold hooks added (init/start/stop/submit) |
| USB OTG          | Shared          | `firmware/target/arm/s5l8702/usb-s5l8702.c`     | iPod6G           | DesignWare OTG; should work once firmware boots |
| RTC              | Disabled        | `rtc-nano3g.c` (excluded via `CONFIG_RTC 0`)     | —                | RTC chip unknown; disabled in config |
| Serial/UART      | Present         | `serial-nano3g.c`, `uart-s5l8702.c`             | S5L8702 shared   | Available for debug logging |
| SPI              | Shared          | `spi-s5l8702.c`                                  | iPod6G           | Used for NOR flash access |
| I2C              | Shared          | `i2c-s5l8702.c`                                  | All S5L8702      | Used for PMU, codec |
| Crypto           | Shared          | `crypto-s5l8702.c`                               | iPod6G           | Used in secure boot path |
| Piezo            | Present         | `piezo-nano3g.c`                                 | Nano2G/Nano4G    | Used in bootloader for error tones |
| Simulator target | **Done**        | `sim-ui-defines.h`, `buttonmap/ipod.c`           | iPod6G layout    | Completed Phase 2 |

---

## 3. File Locations

### SoC-level shared code (all S5L8702 targets)

```
firmware/target/arm/s5l8702/
    system-s5l8702.c        — CPU init, cache, MMU
    kernel-s5l8702.c        — Tick interrupt, scheduler init
    timer-s5l8702.c         — Hardware timers
    gpio-s5l8702.c          — GPIO bank abstraction
    clocking-s5l8702.c      — PLL/clock tree
    dma-s5l8702.c           — PL080 DMA engine
    pl080.c                 — ARM PL080 DMA driver
    pcm-s5l8702.c           — PCM/I2S audio DMA
    usb-s5l8702.c           — DesignWare USB OTG
    lcd-s5l8702.c           — MPU LCD controller (parallel interface)
    lcd-asm-s5l8702.S       — Optimised LCD DMA blit
    spi-s5l8702.c           — SPI controller
    norboot-s5l8702.c       — NOR flash boot
    i2c-s5l8702.c           — I2C controller
    uart-s5l8702.c          — UART driver
    crypto-s5l8702.c        — AES/SHA hardware
    debug-s5l8702.c         — JTAG/debug stubs
    app.lds / boot.lds      — Linker scripts
```

### Nano 3G target-specific code

```
firmware/target/arm/s5l8702/ipodnano3g/
    lcd-nano3g.c            — LCD panel type detection + init sequences
    lcd-target.h            — LCD target defines
    backlight-nano3g.c      — PMU LED backlight control
    backlight-target.h
    button-target.h         — Button bit assignments (IPOD_4G_PAD layout)
    pmu-nano3g.c            — Dialog D1671 PMU driver
    pmu-target.h
    adc-nano3g.c            — ADC stub (all zeros)
    adc-target.h
    power-nano3g.c          — Power on/off, USB power
    powermgmt-nano3g.c      — Battery voltage curves
    nand-nano3g.c           — NAND storage stub
    audio-nano3g.c          — Audio input mux
    cscodec-nano3g.c        — Codec I2C shim (wrong codec, placeholder)
    rtc-nano3g.c            — RTC stub (excluded)
    serial-nano3g.c         — Serial port init
    piezo-nano3g.c          — Piezo speaker driver
    piezo.h
```

### Target config

```
firmware/export/config/ipodnano3g.h     — All hardware capability flags
```

### Bootloader

```
bootloader/ipod-s5l87xx.c              — S5L87xx family bootloader (handles IPOD_NANO3G)
```

### Build system

```
tools/configure                         — Entry: 80|ipodnano3g (blonly="yes" — not enforced)
firmware/SOURCES                        — IPOD_NANO3G section (under #if !defined(SIMULATOR))
```

### Simulator support (Phase 2 — COMPLETE)

```
firmware/target/hosted/sdl/sim-ui-defines.h    — ADDED: IPOD_NANO3G entry (380×500 window, LCD at 30,20, after IPOD_6G)
uisimulator/buttonmap/ipod.c                    — ADDED: IPOD_NANO3G case (wheel centre 190,390, outer radius 90)
uisimulator/bitmaps/UI-ipodnano3g.bmp           — ADDED: custom 380×500 24bpp BMP created with ImageMagick
build-sim-ipodnano3g/                           — BUILD DIR: configured, compiled, installed
```

---

## 4. Reuse Candidates

| Component              | Reuse Source              | Action Required |
|------------------------|---------------------------|-----------------|
| SoC core drivers       | Shared S5L8702 code       | None — already used |
| USB OTG                | iPod6G                    | None — shared |
| PCM/DMA                | S5L8702 shared            | None — shared |
| LCD controller         | Shared `lcd-s5l8702.c`    | None — panel init already in lcd-nano3g.c |
| Clickwheel             | Shared `button-clickwheel.c` | None — shared |
| Bootloader             | `ipod-s5l87xx.c`          | `IPOD_NANO3G` already handled |
| PMU backlight          | Nano2G/Nano4G patterns    | Port D1671 register addresses (partly done) |
| Battery curves         | Nano2G (placeholder)      | Replace with real Nano3G measurements |
| Codec driver           | CS42L55 (iPod6G)          | Wrong for Nano3G — real codec unknown |
| Simulator LCD          | iPod6G sim UI             | Add IPOD_NANO3G entry to sim-ui-defines.h |
| Simulator buttonmap    | iPod6G buttonmap coords   | Add IPOD_NANO3G case to ipod.c |

---

## 5. Missing / Incomplete Pieces

### Critical (blocks any progress)

~~1. **Simulator UI entry** — `sim-ui-defines.h` has no IPOD_NANO3G case → simulator window undefined~~  
~~2. **Simulator buttonmap** — `buttonmap/ipod.c` has no IPOD_NANO3G case → no clickwheel in sim~~  
~~3. **Simulator bitmap** — No `UI-ipodnano3g.bmp` → sim shows blank background~~

**All Phase 2 simulator blockers resolved.**

### Important (blocks real hardware)

4. **NAND/Storage driver** — `nand-nano3g.c` is entirely stubbed; no real FTL
5. **Audio codec** — CS42L55 is wrong; real codec unidentified (possibly WM1870 or similar)
6. **Battery/ADC** — All channels return 0; voltage curves are Nano2G placeholders
7. **RTC** — Disabled; chip unidentified

### Known unknowns / TODO markers in source

- `cscodec-nano3g.c`: "TODO: not tested", "XXX: not tested"
- `backlight-nano3g.c`: "TODO: test"
- `power-nano3g.c`: `charging_state()` returns false
- `pmu-nano3g.c`: `pmu_ldo_set_voltage()` etc. all `#if 0`
- `ipodnano3g.h`: `CONFIG_NAND 0` — real NAND type not defined
- `ipodnano3g.h`: `HAVE_CS42L55` — "XXX: dummy for preliminary build, WRONG CODEC!!!"
- `ipodnano3g.h`: `CONFIG_RTC 0` — RTC disabled

---

## 6. Architecture Map

```
                    ┌─────────────────────────────────┐
                    │  apps/ (UI, playback, plugins)   │  ← target-independent
                    └─────────────┬───────────────────┘
                                  │
                    ┌─────────────▼───────────────────┐
                    │  firmware/  (kernel, FS, codecs) │
                    └──────┬──────────────┬────────────┘
                           │              │
              ┌────────────▼──┐    ┌──────▼──────────────────┐
              │  SIMULATOR    │    │  PLATFORM_NATIVE         │
              │  (SDL layer)  │    │                          │
              │               │    │  S5L8702 SoC drivers     │
              │  lcd-sdl.c    │    │  (shared kernel/timer/   │
              │  pcm-sdl.c    │    │   gpio/dma/usb/pcm)      │
              │  button-sdl.c │    │                          │
              │  system-sdl.c │    │  ipodnano3g/ target      │
              └───────────────┘    │  (lcd/pmu/adc/power/     │
                                   │   nand/audio/backlight)  │
                                   └──────────────────────────┘
```

---

## 7. Phase Plan

| Phase | Name                       | Status      | Depends On |
|-------|----------------------------|-------------|------------|
| 1     | Repository Audit           | **DONE**    | —          |
| 2     | Simulator Target           | **DONE**    | —          |
| 3     | Boot Flow + Dev Docs       | **DONE**    | Phase 2    |
| 4     | Minimal Firmware Bring-up  | **DONE**    | Phase 3    |
| 5     | LCD (sim + real)           | **DONE**    | Phase 4    |
| 6     | Input (clickwheel)         | **DONE**    | Phase 4    |
| 7     | Storage (read-only first)  | **DONE**    | Phase 6    |
| 8     | Audio Pipeline             | **DONE**    | Phase 7    |
| 9     | Power / RTC / Battery      | **DONE**    | Phase 8    |
| 10    | USB + State Handling       | **DONE**    | Phase 9    |
| 11    | Hardware Readiness Gate    | **DONE**    | Phase 10   |
| 12    | Cleanup + Docs + Installer | Pending     | Phase 11   |

---

## 8. Assumptions Made

1. The Nano 3G uses the same S5L8702 SoC as the iPod Classic 6G and Nano 4G based on configure script and comments in source.
2. The LCD panel uses the same physical interface (parallel 9-bit MPU) as documented in `lcd-s5l8702.c`. Panel IDs were discovered empirically (based on comments in `lcd-nano3g.c`).
3. The `blonly="yes"` flag in `tools/configure` for nano3g is never checked by the configure script and does NOT prevent simulator builds.
4. Battery voltage curves are copied from Nano2G and are almost certainly wrong for Nano3G.
5. The audio codec comment `HAVE_WM1870` (disabled with `#if 0`) suggests WM1870 was investigated but unconfirmed. The active placeholder `HAVE_CS42L55` is acknowledged wrong in the source.
6. Boot requires an exploit-based path (e.g. wInd3x) because Apple signed all firmware images for the S5L8702.

---

## 9. Acceptance Criteria — Phase 1

- [x] Subsystem table exists with file locations and status
- [x] Reuse candidates identified
- [x] Missing pieces documented  
- [x] Architecture map produced
- [x] All TODOs and unknowns explicitly documented (not silently assumed)

---

## 10. Acceptance Criteria — Phase 2

- [x] `sim-ui-defines.h` has `IPOD_NANO3G` entry (380×500 window, LCD at 30,20, title "iPod Nano 3G")
- [x] `uisimulator/buttonmap/ipod.c` has `IPOD_NANO3G` case with iPod 6G wheel coordinates
- [x] `uisimulator/bitmaps/UI-ipodnano3g.bmp` exists — custom 380×500 24bpp skin generated via ImageMagick (not a copy of iPod 6G skin)
- [x] `build-sim-ipodnano3g/` created and configured via `tools/configure` (target 80, type S)
- [x] `make -j$(nproc)` completes without errors (two pre-existing build bugs fixed: stray `f` in `statusbar-skinned.c` line 32 and missing `sb_skin_theme_owns_fullscreen` declaration in `statusbar-skinned.h`)
- [x] `make install` populates `simdisk/` with `.rockbox/` tree
- [x] Simulator binary `rockboxui` responds to `--background` arg without crashing

### Build bugs fixed (pre-existing, not Nano 3G specific)

| File | Bug | Fix |
|------|-----|-----|
| `apps/gui/statusbar-skinned.c:32` | Stray `f` after `#include` directive caused `-Wimplicit` cascade | Removed stray `f` |
| `apps/gui/statusbar-skinned.h` | `sb_skin_theme_owns_fullscreen()` defined in `.c` but not declared in header; `viewport.c` had no prototype | Added `bool sb_skin_theme_owns_fullscreen(enum screen_type screen);` to header |

---

## 11. Acceptance Criteria — Phase 3

- [x] `docs/porting/ipodnano3g-boot-flow.md` created with full boot flow documentation
- [x] NOR flash memory map documented (constants, offsets, NORBOOT_OFF, NORBOOT_MAXSZ)
- [x] IM3 image format described (AES-encryption, HMAC-sign, header layout)
- [x] Boot sequence timing documented (t0–t5 with approximate ms values)
- [x] Dualboot install/uninstall mechanism documented (NOR layout ASCII diagrams)
- [x] wInd3x install path documented (prerequisites, build instructions)
- [x] SysCfg structure documented (magic `SCfg`, all known tags including `Codc` for codec discovery)
- [x] Development bootloader menu documented (PMU info, GPIO info, SysCfg reader, NOR dump, LCD sleep test)
- [x] Open issue documented: LCD init uncertainty (`sleep(HZ)` workaround at `bootloader/ipod-s5l87xx.c:836`)
- [x] UART debug logging instructions documented
- [x] Phase 3 row updated to DONE in roadmap phase table

---

## 12. Acceptance Criteria — Phase 4

- [x] `build-native-ipodnano3g/` created and configured via `tools/configure` (target 80, type N)
- [x] `make -j$(nproc)` completes without errors
- [x] `rockbox.elf` (1.2 MB) and `rockbox.ipod` (802 KB) produced in `build-native-ipodnano3g/`
- [x] Simulator build (`build-sim-ipodnano3g/`) still passes after changes (regression-free)
- [x] Phase 4 row updated to DONE in roadmap phase table

### Build fixes applied in Phase 4

| File | Bug | Fix |
|------|-----|-----|
| `firmware/target/arm/s5l8702/ipodnano3g/nand-nano3g.c` | Missing `nand_get_ssd_mode()` — `storage.h` maps `storage_get_ssd_mode()` → `nand_get_ssd_mode()` for `STORAGE_NAND` targets; `backlight.c` calls it unconditionally, causing linker failure | Added stub returning `false` |
| `firmware/export/config/ipodnano3g.h` | `PLUGIN_BUFFER_SIZE 0x80000` (512 KB) too small for `cdogs.rock` (~668 KB `.data`); plugin section overflowed by 159592 bytes | Raised to `0x200000` (2 MB); Nano 3G has 32 MB RAM; audio buffer still ~29 MB |

### Native firmware binary info

```
rockbox.elf  — 1.2 MB (ARM ELF, not scrambled)
rockbox.ipod — 802 KB (scrambled, ready for install once bootloader is in place)
```

---

*This document should be updated as each phase completes.*

---

## 13. Acceptance Criteria — Phase 5

- [x] All LCD-related source files fully audited: `lcd-nano3g.c`, `lcd-target.h`, `lcd-s5l8702.c`, `lcd-s5l8702.h`, `lcd-asm-s5l8702.S`
- [x] Sim/native separation confirmed clean: all hardware register access guarded by `#if !defined(SIMULATOR)` in `firmware/SOURCES`; simulator uses `firmware/drivers/lcd-16bit.c` + SDL backend exclusively
- [x] `sim-ui-defines.h` entry verified: 380×500 window, LCD at (30,20); right margin 30px (symmetric), lower area 240px for clickwheel — consistent with Nano 3G physical form factor
- [x] iPod 6G entry compared (350×591, LCD at 14,12) — Nano 3G entry is distinct and correct
- [x] Simulator runs cleanly under offscreen SDL driver: initialises display + audio, enters main UI loop, no crashes, clean exit when killed
- [x] YUV blit (`lcd-asm-s5l8702.S`) confirmed to use `VERSION_ARMV5TE_WST` — explicitly optimised for ARM926EJ-S (the Nano 3G SoC) with `.align CACHEALIGN_BITS` loop alignment
- [x] Phase 5 row updated to DONE in roadmap phase table
- [x] All open issues documented below

### LCD subsystem: files and roles

| File | Role |
|------|------|
| `firmware/target/arm/s5l8702/ipodnano3g/lcd-nano3g.c` | Panel type detection via 9-bit parallel read ID, per-panel sleep/awake/init sequences (5 panel types: 0x38B3/C4/D5/E6/58xx) |
| `firmware/target/arm/s5l8702/ipodnano3g/lcd-target.h` | Defines `S5L_LCD_WITH_READID`; does NOT define `S5L_LCD_WITH_CMDSET16` (correct — Nano 3G uses 8-bit command set only) |
| `firmware/target/arm/s5l8702/lcd-s5l8702.c` | SoC-level LCD controller: PL080 DMA (32 LLIs), command/frame mode switching, `lcd_init_device()`, `lcd_update()`, `lcd_update_rect()`, `lcd_blit_yuv()` |
| `firmware/target/arm/s5l8702/lcd-s5l8702.h` | Sequence action enum (CMD/SLEEP/END), MPU interface enum (PAR9/PAR18/SERIAL), `lcd_info_rec` struct |
| `firmware/target/arm/s5l8702/lcd-asm-s5l8702.S` | ARMv5TE_WST YUV420→RGB565 conversion, 2-line interleaved, 4 pixels/iter, saturation tables, optimised for ARM926EJ-S |

### Hardware details confirmed

- Command mode: `LCD_MODE_P8` (8-bit parallel, 0x80000c20)
- Frame mode: `LCD_MODE_P9` (9-bit parallel, 0x81100db8) — 2 transfers per RGB565 pixel
- `LCD_PHTIME = 0x33` (Nano 3G and iPod 6G share this value)
- DMA channel: `s5l8702_dmac0`, peripheral `S5L8702_DMAC0_PERI_LCD_WR`, 16-bit width
- Double-buffer: `lcd_dblbuf[LCD_HEIGHT][LCD_WIDTH]` in DRAM

### Open issues found during Phase 5 audit

| Location | Severity | Issue |
|----------|----------|-------|
| `lcd-nano3g.c:417` | Medium | Non-bootloader path: `while(1)` on unknown panel ID — no graceful poweroff or error screen; should call `panicf()` or `power_off()` |
| `lcd-nano3g.c:369` | Low | `uint8_t lcd_id[4]; // XXX: DEBUG` global — debug artifact, should be removed or made local |
| `lcd-s5l8702.h:33` | Low | `SLEEP` sequence action: comment says "unit: ms. but using HZ" — sequences use Rockbox HZ ticks; inconsistency between documentation and code (functional but misleading) |
| `lcd-s5l8702.c:388` | Low | Known mutex deadlock: `/* FIXME: ISR()->panicf()->lcd_update() blocks forever */` — panic path cannot safely call `lcd_update()` under mutex; existing known limitation |
| `lcd-s5l8702.c:44` | Low | `// TODO TODO TODO: HAVE_LCD_ENABLE` — `HAVE_LCD_ENABLE` commented out in `ipodnano3g.h`; impacts `lcd_enable()` availability; not critical for sim work |
| `lcd-s5l8702.h:50` | Trivial | `LCD_CMDSET_8BIT` comment "similar to TODO" — incomplete documentation |

---

## 14. Acceptance Criteria — Phase 6

- [x] `button-target.h` audited: full 7-button `IPOD_4G_PAD` layout confirmed — SELECT(0x01), MENU(0x02), LEFT(0x04), RIGHT(0x08), SCROLL_FWD(0x10), SCROLL_BACK(0x20), PLAY(0x40); `HAS_BUTTON_HOLD` defined; `POWEROFF_BUTTON=PLAY`, `POWEROFF_COUNT=40`
- [x] `button-clickwheel.c` audited: Nano 3G explicitly included via `#if defined(IPOD_6G) || defined(IPOD_NANO3G) || defined(IPOD_NANO4G)` — includes `pmu-target.h` and `clocking-s5l8702.h`; `CLICKWHEEL_DATA = WHEELRX`; `button_hold()` calls `pmu_holdswitch_locked()`; `headphones_inserted()` reads `PDAT10 bit 6`; `WHEELCLICKS_PER_ROTATION = 96`
- [x] `firmware/SOURCES` structure confirmed: `button-clickwheel.c` included via shared `#if CONFIG_CPU == S5L8702 || CONFIG_CPU == S5L8720` block (line 799–800) — correct design, no Nano 3G specific inclusion needed
- [x] All required config flags present in `ipodnano3g.h`: `HAVE_SCROLLWHEEL` (line 65), `HAVE_WHEEL_ACCELERATION` (line 66), `HAVE_WHEEL_POSITION` (line 71), `IPOD_ACCESSORY_PROTOCOL` (line 263, guarded `!BOOTLOADER && !LOGF_SERIAL`), `HAVE_HEADPHONE_DETECTION` (line 76), `HAVE_HARDWARE_CLICK` (line 214)
- [x] All hardware register defines confirmed present in `firmware/export/s5l87xx.h`: `WHEELRX` (line 1637), `WHEELINT` (line 1636), `WHEEL00/04/08/0C/10` (lines 1631–1635), `WHEELTX` (line 1638), `PCON14` (line 1411), `IRQ_WHEEL` = 23 (line 1888)
- [x] `CLOCKGATE_CWHEEL = 33` confirmed in `clocking-s5l8702.h` (line 125 comment + `s5l87xx.h` line 325) — APB clock gate for clickwheel peripheral
- [x] `pmu_holdswitch_locked()` declared in `pmu-target.h` (line 134) — D1671 `EVENTB_INPUT2` bit reads hold-switch state
- [x] `uisimulator/buttonmap/ipod.c` `IPOD_NANO3G` case verified (Phase 2): full 7-button layout with pixel-accurate coordinates and radii
- [x] Simulator keyboard mapping confirmed complete via `firmware/target/hosted/sdl/button-sdl.c` + `ipod.c` `key_to_button()`: all 7 buttons mapped; scroll wheel via SDL mouse wheel; hold toggle via `h` key
- [x] Phase 6 row updated to DONE in roadmap phase table
- [x] Both builds verified clean: `build-native-ipodnano3g/` (rockbox.ipod 802 KB), `build-sim-ipodnano3g/` (rockboxui linked)

### Input subsystem: files and roles

| File | Role |
|------|------|
| `firmware/target/arm/s5l8702/ipodnano3g/button-target.h` | Button bitmask definitions, `POWEROFF_BUTTON`, `HAS_BUTTON_HOLD` |
| `firmware/target/arm/ipod/button-clickwheel.c` | Clickwheel ISR (`INT_WHEEL`), `s5l_clickwheel_init()`, `button_hold()`, `headphones_inserted()`, wheel position + acceleration logic |
| `firmware/target/hosted/sdl/button-sdl.c` | Simulator button layer: SDL event loop, hold toggle, mouse-wheel scroll |
| `uisimulator/buttonmap/ipod.c` | Simulator click-area map for `IPOD_NANO3G` (7 hit-regions with pixel coords + radii) |

### Hardware details confirmed

- Clickwheel hardware base: `WHEEL_BASE = 0x3C200000`
- Wheel data register: `WHEELRX` (offset 0x18 from base)
- Interrupt: `IRQ_WHEEL = 23`, cleared by writing `WHEELINT`
- Clock gate: `CLOCKGATE_CWHEEL = 33` (APB PWRCON_APB bit 1)
- Pin config: `PCON14` configures port 14 pins for wheel I/O
- Hold switch: read via `pmu_holdswitch_locked()` → D1671 `STATUSB_INPUT2` bit (PMU I2C)
- Headphones: `PDAT10 bit 6` (GPIO port 10, pin 6)
- Wheel sensitivity: `WHEEL_SENSITIVITY = 4` (default — Nano 3G uses same as iPod 6G Classic; see open issue below)

### Open issues found during Phase 6 audit

| Location | Severity | Issue |
|----------|----------|-------|
| `button-clickwheel.c:67–71` | Low | `WHEEL_SENSITIVITY = 4` (default) — Nano 3G has a physically smaller clickwheel than the 6G Classic but is not listed alongside `IPOD_NANO` / `IPOD_NANO2G` (which use `6`). Cannot tune without hardware testing; document as potential post-hardware tuning item. Nano 4G also uses default 4 and is reportedly functional. |
| `button-target.h` | Informational | `IPOD_ACCESSORY_PROTOCOL` remote buttons (accessory remote) present in header as placeholders; IAP is compiled in main firmware (`!BOOTLOADER`) but requires Apple accessory hardware to test |

---

## 15. Acceptance Criteria — Phase 7

- [x] `nand-nano3g.c` fully audited and compared against `nand-nano4g.c` and `nand-nano2g.c`
- [x] Storage API expectations audited (`storage.h`, `nand.h`, `storage.c`) and all required symbols confirmed for non-multi NAND targets
- [x] Root cause documented: no S5L8702 NAND controller base in `s5l87xx.h`; on S5L8702 address `0x3C200000` is `WHEEL_BASE`, so S5L8700 NAND code cannot be reused directly
- [x] FTL status documented: no S5L8702 FTL exists in-tree; only `ftl-nano2g.c` (S5L8700) is present
- [x] `nand_event()` fixed to call `storage_event_default_handler()` so storage-idle notifications are throttled correctly (~3 s inactivity)
- [x] Stub behaviour made explicit and safe: `nand_read_sectors()` returns deterministic zero-filled buffers; `nand_write_sectors()` returns `-1` (writes gated until hardware support exists)
- [x] `nand_get_ssd_mode()` stub retained (required by `backlight.c` via `storage_get_ssd_mode()` on NAND targets)
- [x] Native and simulator builds both verified clean after changes
- [x] Phase 7 row updated to DONE in roadmap phase table

### Storage subsystem: files and roles

| File | Role |
|------|------|
| `firmware/target/arm/s5l8702/ipodnano3g/nand-nano3g.c` | Nano 3G NAND storage stub; now documents architecture blockers and enforces read-only bring-up policy |
| `firmware/target/arm/s5l8702/ipodnano4g/nand-nano4g.c` | Nano 4G NAND stub (same maturity level; useful parity reference) |
| `firmware/target/arm/s5l8700/ipodnano2g/nand-nano2g.c` | Full S5L8700 NAND driver with FMC/ECC/DMA path and power sequencing; not directly portable to S5L8702 without correct register map |
| `firmware/target/arm/s5l8700/ipodnano2g/ftl-nano2g.c` | Only Apple NAND FTL in-tree; S5L8702 equivalent missing |
| `firmware/export/s5l87xx.h` | SoC register definitions; confirms clickwheel registers at `0x3C200000` on S5L8702 and no S5L8702 `FMC_BASE` define |
| `firmware/export/storage.h` / `firmware/export/nand.h` / `firmware/storage.c` | Storage thread/event contract and NAND driver integration points |

### Hardware details confirmed

- `WHEEL_BASE = 0x3C200000` for S5L8702 (`s5l87xx.h`), confirming that reusing S5L8700 NAND base would collide with clickwheel MMIO
- `FMC_BASE` is only defined for S5L8700 (`0x3C200000`) and S5L8701 (`0x39400000`) in current headers; S5L8702 FMC base remains unknown
- Clock gates exist for NAND and NAND ECC (`CLOCKGATE_NAND`, `CLOCKGATE_NANDECC`) but without verified S5L8702 FMC mapping these are insufficient to implement the driver safely

### Open issues found during Phase 7 audit

| Location | Severity | Issue |
|----------|----------|-------|
| `firmware/export/s5l87xx.h:866–871` | High | Missing S5L8702 `FMC_BASE` definition blocks any real NAND register access implementation |
| `firmware/target/arm/s5l8702/ipodnano3g/nand-nano3g.c` | High | Real NAND I/O path absent (still stubbed by design); read-only behaviour is synthetic until FMC/FTL are implemented |
| `firmware/target/arm/s5l8700/ipodnano2g/ftl-nano2g.c` | High | No S5L8702 FTL equivalent; must design/port translation logic before real sector reads can work |
| `firmware/target/arm/s5l8702/ipodnano3g/nand-nano3g.c:102` | Medium | `nand_last_disk_activity()` remains constant 0 in stub mode; acceptable for bring-up but not representative of real activity telemetry |

---

## 16. Acceptance Criteria — Phase 8

- [x] Audio pipeline audited end-to-end: codec thread -> DSP -> `pcmbuf` -> mixer (`pcm_mixer.c`) -> PCM core (`pcm.c`) -> target DMA backend (`pcm-s5l8702.c` native, `pcm-sdl.c` simulator)
- [x] S5L87xx/iPod6G audio references audited: `audio-6g.c`, `cscodec-6g.c`, `pcm-s5l8702.c`, `drivers/audio/cs42l55.c`
- [x] Nano 3G hardware audio scaffold implemented and isolated in target code (`audio-nano3g.c` + hook calls from `pcm-s5l8702.c`), with init/start/stop/submit entry points
- [x] Nano 3G codec shim made simulator-first safe: `cscodec-nano3g.c` no longer touches unverified hardware registers/I2C; shadow-register model used until hardware path is known
- [x] Simulator audio path hardened for no-device environments: `pcm-sdl.c` now falls back to simulated PCM sink when SDL device open fails, consumes buffers on timing thread, and keeps playback pipeline advancing
- [x] Playback debug screen added: `apps/debug_menu.c` -> "Audio playback screen" showing current track, playback state, PCM buffer fill, and elapsed/length progression; includes controls for pause/resume, next/prev, stop
- [x] File testing completed using simulator storage backend with symlinked laptop music folder and dedicated test files under `simdisk/AudioTests`
- [x] WAV and MP3 decode/playback validated in simulator (`test440.wav`, `test440.mp3`), including pause/resume, track switching (next/prev), and stop; no crashes observed
- [x] Phase 8 row updated to DONE in roadmap phase table

### Audio pipeline overview

- Decode/control: `apps/playback.c` + codec thread load/decode compressed file frames
- DSP: decoded PCM passes through DSP stages before buffering
- PCM buffering: `apps/pcmbuf.c` commits PCM chunks and tracks timing/position metadata
- Mixer: `firmware/pcm_mixer.c` merges channels into output frames and drives `pcm_play_data(...)`
- PCM core: `firmware/pcm.c` handles state machine/callback contract and calls target `pcm_play_dma_*`
- Target output backend:
  - Native Nano 3G: `firmware/target/arm/s5l8702/pcm-s5l8702.c` (shared DMA/I2S path), now with Nano3G scaffold hook calls
  - Simulator: `firmware/target/hosted/sdl/pcm-sdl.c` (SDL callback path, plus simulated sink fallback)

### Simulated vs real (Phase 8 boundary)

- Simulated/validated now:
  - Full playback control pipeline runs in simulator for WAV + MP3
  - PCM consumption timing progresses even when no SDL audio device is available (simulated sink mode)
  - Debug observability via new playback test screen and logf/stdio events
- Real hardware intentionally deferred:
  - No Nano 3G DAC programming, GPIO toggling, PMU LDO control, or unverified I2C writes in Nano3G codec shim
  - No assumption that CS42L55 is the final Nano 3G codec

### Phase 8 test setup and results

- Simulator media path: `build-sim-ipodnano3g/simdisk/Music` symlinked to `/home/david/Music`
- Deterministic test media generated into `build-sim-ipodnano3g/simdisk/AudioTests`:
  - `test440.wav` (5s sine wave)
  - `test440.mp3` (transcoded from WAV)
- Playback source forced via `.playlist_control` to `/AudioTests` entries for repeatable startup
- Logged results in `/tmp/rb_phase8_test.log` show sequential metadata/codec activity for WAV then MP3 and no crash during automated key-driven pause/resume/next/prev/stop actions

### Open issues found during Phase 8 audit

| Location | Severity | Issue |
|----------|----------|-------|
| `firmware/export/config/ipodnano3g.h:141` | High | `HAVE_CS42L55` remains marked wrong placeholder; real Nano 3G codec identity still unconfirmed |
| `firmware/target/arm/s5l8702/ipodnano3g/cscodec-nano3g.c` | High | Shadow codec shim is intentionally non-hardware; must be replaced with verified power/reset/clock/I2C sequence once codec is identified |
| `firmware/target/arm/s5l8702/pcm-s5l8702.c` | Medium | Shared S5L8702 DMA path is active, but Nano3G hardware output hooks are currently scaffold-only (no physical sink enable/disable) |
| `firmware/target/hosted/sdl/pcm-sdl.c` | Low | Simulated sink fallback logs elapsed progress and preserves timing; this is a simulator-only test aid and must not leak into native hardware paths |

---

## 17. Acceptance Criteria — Phase 9

- [x] Power-management stack audited for Nano3G + simulator paths: `powermgmt.c`, Nano3G `power/powermgmt/adc/rtc`, hosted simulator power model (`uisimulator/common/powermgmt-sim.c`), and hosted RTC (`firmware/target/hosted/rtc.c`)
- [x] Simulator power model extended with explicit toggles for USB power, main charger power, charging enable, and sleep-state discharge behavior (all simulator-only)
- [x] Simulator key controls added for power state simulation in SDL frontend (`F6` main charger toggle, `F7` charging enable toggle, `F8` sleep-state toggle) while retaining existing USB key controls
- [x] Debug observability added: new debug menu entry `Power state screen` showing power input flags, charger state, battery voltage/level, sleep timer state, and RTC readout; includes simulator control hints
- [x] Nano3G native power scaffold improved safely: `charging_state()` now derives from `power_input_status()` instead of always-false placeholder, with explicit note that hardware charge signal remains unknown
- [x] Nano3G RTC scaffold corrected to populate day-of-year via `set_day_of_year(tm)` so RTC reads are internally complete when RTC is later enabled
- [x] No unverified hardware register guessing introduced; all new active behavior is simulator-only or conservative scaffold logic
- [x] Native and simulator builds validated after changes

### Phase 9 implementation notes

- Simulator power model (`uisimulator/common/powermgmt-sim.c`):
  - Tracks independent simulation state:
    - USB online (`sim_power_set_usb_online`)
    - Main charger online (`sim_power_set_main_online`)
    - Charging enabled (`sim_power_set_charge_enabled`)
    - Sleep mode (`sim_power_set_sleeping`)
  - Charging only occurs when external power is online and charging is enabled.
  - Battery now discharges only when external power is absent; discharge is reduced in simulated sleep mode.
  - `power_input_status()` now reports composed bitflags (`POWER_INPUT_MAIN`, `POWER_INPUT_USB`, and charger-capable flags) from simulator state rather than a synthetic auto-plug cycle.
- Simulator control wiring:
  - New simulator control API declared in `uisimulator/common/sim_tasks.h`.
  - `sim_tasks.c` now synchronizes USB connect/disconnect events with the power model (`sim_power_set_usb_online`).
  - SDL key handler (`firmware/target/hosted/sdl/button-sdl.c`) adds:
    - `F6` toggle main charger online/offline
    - `F7` toggle charging enabled/disabled
    - `F8` toggle simulated sleep mode
  - Help text updated to expose new power controls.
- Debug screen (`apps/debug_menu.c`):
  - New entry: `Power state screen` (enabled for `IPOD_NANO3G` and `SIMULATOR`).
  - Shows power flags, input source presence, charging status, battery stats, sleep timer status, and RTC value (or disabled state).
  - In simulator, allows live control from debug screen:
    - OK: USB toggle
    - NEXT: Main charger toggle
    - PREV: Charging enable toggle
    - CONTEXT: Sleep-state toggle

### Simulated vs real (Phase 9 boundary)

- Simulated/validated now:
  - Deterministic external power model with explicit USB/main/charge/sleep state controls.
  - Power state visualization and manipulation from a dedicated debug screen.
  - Sleep impact represented as reduced discharge rate in simulator model.
- Real hardware intentionally deferred:
  - No claim of real Nano3G ADC scale correctness.
  - No claim of true PMU charge-state bit identity on Nano3G.
  - RTC remains config-disabled for Nano3G (`CONFIG_RTC 0`) pending hardware validation.

### Phase 9 test setup and results

- Build validation:
  - `build-sim-ipodnano3g`: `make -j$(nproc)` success.
  - `build-native-ipodnano3g`: `make -j$(nproc)` success.
- Runtime sanity:
  - Simulator launched with no-audio-device path and exercised power hotkeys (`F6/F7/F8/F11/F12`) without crash.
  - Command used: `./rockboxui --nobackground --root ./simdisk --audiodev "__invalid_device__"`.

### Open issues found during Phase 9 implementation

| Location | Severity | Issue |
|----------|----------|-------|
| `firmware/export/config/ipodnano3g.h:128` | High | `CONFIG_RTC 0` keeps Nano3G RTC path disabled pending hardware validation of PMU RTC behavior |
| `firmware/target/arm/s5l8702/ipodnano3g/adc-nano3g.c` | High | ADC is still fully stubbed (all-zero returns), so real battery telemetry remains unimplemented |
| `firmware/target/arm/s5l8702/ipodnano3g/power-nano3g.c` | Medium | `charging_state()` is now conservative scaffold based on input presence, not true hardware charge-current detection |
| `uisimulator/common/powermgmt-sim.c` | Low | Power model is intentionally synthetic for deterministic simulator testing and not a physical battery model |

---

## 18. Acceptance Criteria — Phase 10

- [x] USB thread/state flow audited (`firmware/usb.c`, `apps/main.c`, `apps/gui/usb_screen.c`) with focus on inserted vs powered-only behavior and host/exclusive-storage transitions
- [x] Simulator USB state handling aligned with Rockbox USB mode semantics (mass-storage host mode vs charge-only powered mode)
- [x] Simulator APIs added for USB state introspection/control so UI/debug paths can switch USB state deterministically
- [x] Simulator stubs now report USB insertion and powered-only state from simulator state instead of hardcoded false values
- [x] SDL simulator controls extended for USB host-mode state handling (`F10` toggle charge-only host mode)
- [x] Debug observability updated: `Power state screen` now displays and toggles simulator USB host mode
- [x] Native and simulator builds validated after USB state-handling updates

### Phase 10 implementation notes

- USB state controls and introspection:
  - `uisimulator/common/sim_tasks.h` adds:
    - `sim_trigger_usb_powered_only(bool enabled)`
    - `sim_usb_inserted(void)`
    - `sim_usb_powered_only(void)`
- Simulator USB event/state logic (`uisimulator/common/sim_tasks.c`):
  - Tracks three independent USB facets:
    - physical cable/power presence (`is_usb_inserted`)
    - host-connected mass-storage state (`is_usb_host_connected`)
    - charge-only powered mode (`is_usb_powered_only`)
  - `sim_trigger_usb(...)` now sends:
    - `SYS_EVENT_USB_INSERTED` always when cable inserted
    - `SIM_USB_INSERTED` queue event only for host-connected mass-storage mode
    - extraction events only when host-connected path was active
  - `sim_trigger_usb_powered_only(...)` can switch between charge-only and storage mode while cable remains inserted by emitting the corresponding inserted/extracted event transitions.
- Simulator stubs (`uisimulator/common/stubs.c`):
  - `usb_inserted()` now reflects simulator USB insertion state.
  - `usb_powered_only()` now reflects simulator charge-only state.
  - This makes app-level USB decision paths in `apps/main.c` and `firmware/usb.c` testable in simulator.
- SDL controls (`firmware/target/hosted/sdl/button-sdl.c`):
  - Added `F10` to toggle USB host mode (charge-only vs storage host mode).
  - Simulator help text updated accordingly.
- Debug screen integration (`apps/debug_menu.c`):
  - `Power state screen` now displays `sim usb host=storage/charge-only`.
  - `ACTION_STD_MENU` toggles host mode directly from the screen.

### Simulated vs real (Phase 10 boundary)

- Simulated/validated now:
  - Distinction between USB cable-power state and host-connected mass-storage state.
  - Charge-only USB behavior path testable without entering exclusive storage mode.
  - Repeatable USB state transitions from keyboard controls and debug UI.
- Real hardware intentionally deferred:
  - No new Nano3G-specific USB PHY/PMU register assumptions.
  - No claim that current Nano3G PMU USB event wiring is fully hardware-validated.

### Phase 10 test setup and results

- Build validation:
  - `build-sim-ipodnano3g`: `make -j$(nproc)` success.
  - `build-native-ipodnano3g`: `make -j$(nproc)` success.
- Runtime sanity:
  - Simulator launched and exercised USB + power controls (`F10/F11/F12/F6/F7/F8`) without crash.
  - Command used: `./rockboxui --nobackground --root ./simdisk --audiodev "__invalid_device__"`.

### Open issues found during Phase 10 implementation

| Location | Severity | Issue |
|----------|----------|-------|
| `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c` | High | Native USB detect/event path relies on PMU input handling but remains unvalidated on Nano3G hardware |
| `firmware/export/config/ipodnano3g.h:245` | Medium | `HAVE_BOOTLOADER_USB_MODE` remains disabled pending target-specific validation of safe bootloader USB flow |
| `uisimulator/common/sim_tasks.c` | Low | Simulator USB host/power model is intentionally synthetic and event-driven for deterministic testing |

---

## 19. Acceptance Criteria — Phase 11

- [x] Nano3G hardware touchpoints audited and classified in dedicated bring-up document (`docs/porting/ipodnano3g-hardware-bringup.md`)
- [x] Global safe bring-up guard enabled for Nano3G: `NAN03G_SAFE_BRINGUP 1`
- [x] Safe-mode enforcement added for Nano3G hardware-write paths (PMU/backlight/piezo/LCD init/audio I2S-DMA/USB PHY-clocks-IRQ)
- [x] Early boot trace logging added with persistent in-memory ring buffer and debug output
- [x] Unknown LCD panel path converted from raw infinite loop to Nano3G failsafe halt path
- [x] Panic/exception flow on Nano3G safe mode now halts safely without reboot loop (`system_exception_wait()` -> failsafe halt)
- [x] Dedicated hardware bring-up + DFU/recovery checklist doc created
- [x] Both builds validated after changes (`build-sim-ipodnano3g`, `build-native-ipodnano3g`)

### Phase 11 implementation notes

- Added Nano3G bring-up utility module:
  - `firmware/target/arm/s5l8702/ipodnano3g/bringup-nano3g.h`
  - `firmware/target/arm/s5l8702/ipodnano3g/bringup-nano3g.c`
  - Provides:
    - `nano3g_safe_mode_enabled()`
    - boot trace ring (`nano3g_boottrace_*`)
    - `nano3g_failsafe_halt()`
- Wired into build system:
  - `firmware/SOURCES` adds `target/arm/s5l8702/ipodnano3g/bringup-nano3g.c` under `IPOD_NANO3G`
- Safe-write gates introduced in key touchpoints:
  - PMU writes blocked in safe mode (`pmu_write*`, `pmu_wr*`)
  - Backlight PMU writes blocked in safe mode
  - Piezo hardware start/stop/tone writes blocked in safe mode
  - LCD controller init path skipped in safe mode (`lcd_init_device()` early-return)
  - USB target clock/IRQ/enable/init writes blocked in safe mode
  - PCM/I2S/DMA hardware programming blocked in safe mode on Nano3G
- Boot and exception trace points:
  - `system_preinit()` logs stage progression (`syscon/gpio/i2c/pmu/miu`)
  - `system_init()` and key Nano3G subsystems log init stages
  - `system_exception_wait()` routes to Nano3G failsafe halt in safe mode

### Simulated vs real (Phase 11 boundary)

- Simulated/validated now:
  - Safe-mode gating compiles and runs in simulator context without changing simulator-first workflow.
  - Nano3G bring-up instrumentation exists even when storage/USB/audio are unavailable.
- Real hardware intentionally deferred:
  - NAND remains stubbed (read-zero/write-denied).
  - Codec/hardware audio path remains scaffold-only.
  - LCD/USB real bring-up writes remain blocked under safe mode.

### Open issues after Phase 11

| Location | Severity | Issue |
|----------|----------|-------|
| `firmware/target/arm/s5l8702/ipodnano3g/nand-nano3g.c` | High | Real NAND controller + FTL still unimplemented; storage remains synthetic |
| `firmware/target/arm/s5l8702/ipodnano3g/cscodec-nano3g.c` | High | Real Nano3G codec identity/power/reset/clock programming still unknown |
| `firmware/target/arm/s5l8702/ipodnano3g/adc-nano3g.c` | High | ADC channels and calibration remain stubbed/unvalidated |
| `firmware/target/arm/s5l8702/ipodnano3g/lcd-nano3g.c` | Medium | LCD ID probing is retained, but full panel validation on physical hardware still pending |
| `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c` | Medium | PMU register semantics still inherited/scaffolded; safe mode currently prevents risky writes |

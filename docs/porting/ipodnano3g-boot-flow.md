# iPod Nano 3G — Boot Flow & Developer Install Guide

**Status:** Phase 3 Documentation  
**Date:** 2026-04-14  
**Applies to:** `IPOD_NANO3G` target, S5L8702 SoC, `bootloader/ipod-s5l87xx.c`

---

## 1. Overview

The S5L8702 BOOTROM loads firmware exclusively from NOR flash using signed, AES-encrypted
IM3 images.  Apple controls the hardware AES key baked into each SoC — no image can be
signed without that key.  Rockbox therefore enters through an exploit-based path
(**wInd3x** / **NorBoot**) that allows running an unsigned DFU image on device, which
then installs the Rockbox bootloader into the NOR flash dualboot slot.

---

## 2. NOR Flash Memory Map

NOR flash is 1 MB (`FLASH_SIZE = 0x100000`).  The layout is identical to the iPod
Classic 6G (shared `norboot-target.h`):

```
1MB  ___________________
    |                   |
    |   flsh DIR        |  0x200 bytes at top (image directory)
    |___________________|  0xFFFE00
    |                   |
    |   File N ... 1    |  Optional extra files stored by OF
    |___________________|
    |                   |
    |   (Unused)        |
    |___________________|  0x28000  (approx, after RB install)
    |                   |
    |   ONB (decrypted) |  Original NOR Boot, moved here after RB install
    |   ~128 KB         |  starts at NORBOOT_OFF + BLSIZE
    |___________________|  (variable, e.g. ~0x28000)
    |                   |
    |   RB Bootloader   |  Installed at NORBOOT_OFF (replaces ONB primary slot)
    |   ~100 KB         |
0x8000 __________________|  NORBOOT_OFF
    |                   |
    |   SysCfg          |  Device serial number, HW revision, codec ID, etc.
    |___________________|
  0 |___________________|
```

Key constants (from `firmware/target/arm/s5l8702/norboot-target.h`):

| Constant         | Value      | Meaning                              |
|------------------|------------|--------------------------------------|
| `NOR_SZ`         | `0x100000` | Total NOR flash size (1 MB)          |
| `NORBOOT_OFF`    | `0x8000`   | Primary boot image offset (32 KB)    |
| `NORBOOT_MAXSZ`  | `0x20000`  | Maximum boot image size (128 KB)     |
| `SPI_PORT`       | `0`        | SPI controller port for NOR access   |
| `DIR_OFF`        | `0xFFFE00` | flsh directory offset (1MB - 0x200)  |

### IM3 Image Format

Every bootable image is wrapped in an IM3 container:

- **Header** (0x800 bytes): magic `"8702"`, version, encryption type, data size,
  HMAC-SHA1 info signature, and a decrypted-data HMAC-SHA1 body signature.
- **Body**: AES-CBC encrypted firmware payload (decrypt key derived from hardware AES key).

The BOOTROM verifies both signatures before executing the body.  `im3_read()` in
`norboot-s5l8702.c` replicates this validation in Rockbox firmware.

---

## 3. Full Boot Sequence with Timing

The following timeline is documented in `bootloader/ipod-s5l87xx.c:309–346`:

```
t0 = 0 ms
  S5L8702 BOOTROM reads NOR at offset NORBOOT_OFF (0x8000).
  - IM3 header (0x800 bytes) → IRAM1_ORIG
  - IM3 body (decrypted RB bootloader, ~100 KB) → IRAM0_ORIG
  Time to load: ~200–250 ms.
  Execution transferred to IRAM0_ORIG.

  RB bootloader immediately relocates itself from IRAM0_ORIG to
  IRAM1_ORIG+0x800, preserving the IM3 header (which contains the
  NOR offset of the backed-up ONB — needed for dualboot).

t1 = ~250 ms
  usec_timer_init(), i2c_preinit(0) executed.

  Hibernation check: if PMU is in hibernation state,
    im3_read(NORBOOT_OFF + bl_nor_sz, ...)  loads ONB (~128 KB, ~120 ms)
    and jumps to it at SPI clock 27/2 = 13.5 MHz.
    ONB restores iPod from hibernation state (SDRAM self-refresh).
  If not hibernated:
    system_preinit(), memory_init(), bss_init(), system_init(),
    kernel_init(), i2c_init(), power_init(), enable_irq(),
    [serial_setup() if HAVE_SERIAL], button_init().

t2 = ~650 ms
  User button selection window (waits until USEC_TIMER >= 400000 µs).
  Button combinations checked:

    MENU alone          → launch ONB (boots OF)
    SELECT + LEFT       → launch ONB (diagnostic/disk mode)
    SELECT + PLAY       → launch ONB (disk mode)
    SELECT + MENU       → wait for release, then re-read buttons
                          (prevents DFU entry from spinning HDD)

  If no special button: continue to LCD init.

t3 = ~700–900 ms
  lcd_init() — NOTE: there is an open TODO at bootloader/ipod-s5l87xx.c:836:
    "TODO: see if removing this causes the nano3g LCD to initialize properly"
    The development build adds a 1-second sleep here plus piezo beeps
    (one per lcd_type+1) before lcd_update(); this was a debugging aid.
  lcd_set_foreground/background, lcd_clear_display(), font_init(),
  lcd_setfont(FONT_SYSFIXED), lcd_update(), backlight_init().

  Display shows: "Rockbox boot loader" + version string.

t4 = ~2600–2800 ms
  [Battery check for ATA targets — NAND targets skip this]
  storage_init(), filesystem_init().
  Hold switch check → if locked, launches ONB (boots OF).
  [SELECT+RIGHT → USB mode if HAVE_BOOTLOADER_USB_MODE]
  disk_mount_all().
  load_firmware() reads /.rockbox/rockbox.ipod from NAND into DRAM_ORIG.

t5 = ~2800–3000 ms
  disable_irq(), commit_discard_idcache().
  Branch to rockbox.ipod entry point at DRAM_ORIG.
  Rockbox main firmware begins execution.
```

---

## 4. Dualboot Mechanism

Source: `utils/mks5lboot/dualboot/dualboot.c`

### Pristine NOR (stock)

```
0x8000  [ONB — encrypted IM3, ~128 KB]
```

### Rockboxed NOR

```
0x8000          [RB bootloader — decrypted IM3, ~100 KB]
0x8000+BLSZ     [ONB — decrypted copy, original moved here]
```

### Install process (`dualboot.c main()` — install variant)

1. Read IM3 at `NORBOOT_OFF`.  Identify firmware by decrypting and comparing its
   HMAC hash against a table of known OF hashes.
2. If the image is already a RB bootloader (not recognized as OF):
   - The ONB must be located at `NORBOOT_OFF + old_bl_nor_sz` (previous install).
   - Read and verify it.
3. Compute `bl_nor_sz` = size of new RB bootloader (NOR-sector aligned).
4. Safety check: `flsh_get_unused()` must have enough space for `bl_nor_sz`.
5. Write decrypted ONB to `NORBOOT_OFF + bl_nor_sz`.
6. Write new RB bootloader to `NORBOOT_OFF`.
7. On any write failure: restore ONB to `NORBOOT_OFF` (encrypt it back first).

### Uninstall process (`dualboot.c main()` — uninstall variant)

1. Read IM3 at `NORBOOT_OFF`.  If it is not an RB bootloader: done (happy exit).
2. ONB should be at `NORBOOT_OFF + bl_nor_sz`.  Read and verify.
3. Re-encrypt ONB, write back to `NORBOOT_OFF`.
4. Erase the NOR blocks that held the now-removed RB bootloader.

### Boot-time ONB launch (`bootloader/ipod-s5l87xx.c:254–298`)

```c
static int launch_onb(int clkdiv)
{
    // Sets SPI clock divider
    // Reads ONB IM3 from NORBOOT_OFF + im3_nor_sz(hinfo) into IRAM0_ORIG
    // Destroys exception vector table — must call only once!
    // Calls eint_init() to disable external interrupts
    // Branches to IRAM0_ORIG
}
```

Called at two points:
- **t1**: if PMU is hibernated — `launch_onb(1)` (27/2 = 13.5 MHz)
- **t2**: if button combination requests OF — `kernel_launch_onb()` which calls
  `launch_onb(3)` (54/4 = 13.5 MHz) with IRQs disabled/re-enabled around it

---

## 5. wInd3x / NorBoot Install Path

The exploit used to gain initial write access to NOR flash is **wInd3x**, available at:

> https://github.com/freemyipod/wInd3x

wInd3x exploits a vulnerability in the S5L8702 DFU (Device Firmware Upgrade) mode to
execute an unsigned DFU payload in IRAM.  That payload is built from the Rockbox source
tree via `utils/mks5lboot/`.

### Prerequisites

- Python 3 with `libusb` bindings (`pyusb`)
- A Linux or macOS host (Windows also supported via wInd3x)
- USB cable (data-capable, not charge-only)

### Steps (outline — verify against current wInd3x docs)

1. Put the iPod into DFU mode:
   - Hold SELECT+MENU until device reboots, then immediately hold SELECT+PLAY.
   - Device screen goes dark and USB enumerates as Apple DFU.

2. Run `wInd3x` to exploit DFU and upload the Rockbox dualboot-installer DFU image:
   ```
   wInd3x -m ipod3g install path/to/dualboot-installer.dfu
   ```

3. The installer DFU runs on-device, installs the RB bootloader into NOR, and
   reboots (watchdog reset).

4. On next boot, device runs the RB bootloader, which loads `rockbox.ipod` from
   NAND.  Hold MENU during boot to go back to the Apple OF.

### Building the DFU installer

```bash
cd utils/mks5lboot
make
# Produces: dualboot-installer.dfu, dualboot-uninstaller.dfu
```

The `mks5lboot` tool embeds the compiled RB bootloader binary into the DFU
installer image, signs everything appropriately for the DFU upload step.

---

## 6. SysCfg — Device Configuration in NOR

The `SysCfg` (System Configuration) structure lives at NOR offset 0 and stores
device-specific metadata.  Relevant fields for porting:

| Tag    | Meaning                          |
|--------|----------------------------------|
| `SrNm` | Device serial number             |
| `FwId` | Firmware ID (7 hex digits)       |
| `HwId` | Hardware ID (8 hex digits)       |
| `HwVr` | Hardware version (affects LBA48) |
| `Codc` | Audio codec identifier string    |
| `SwVr` | Software (OF) version            |
| `Mod#` | Model number string              |
| `Regn` | Sales region bitmask             |

The **`Codc` tag is critical for the audio codec mystery**: reading it from a real
Nano 3G device would identify the actual codec, resolving the `WRONG CODEC!!!`
placeholder in `ipodnano3g.h`.

The dev-bootloader `Show SysCfg` menu option dumps this information over LCD.

---

## 7. Development Bootloader

Build with:
```
#define S5L87XX_DEVELOPMENT_BOOTLOADER
```
in the bootloader build (or pass `-DS5L87XX_DEVELOPMENT_BOOTLOADER` to the compiler).

This replaces the normal boot flow with an interactive development menu after LCD init.

### Dev Menu Items (Nano 3G)

| Item                    | Function                                                        |
|-------------------------|-----------------------------------------------------------------|
| LCD sleep/awake test    | Tests `lcd_sleep()` / `lcd_awake()` (if `HAVE_LCD_SLEEP`)      |
| PMU info                | Dumps all 128 PMU registers over LCD; toggles PMU regs 6/7 on Nano3G |
| GPIO info               | Dumps all GPIO bank registers (PCON/PDAT/PUNA/PUNB/PUNC)      |
| Show SysCfg             | Parses and displays NOR SysCfg entries (serial, HW ID, codec)  |
| Show bootloader hash    | Displays HMAC hash of primary + backup bootloaders              |
| Dump bootflash to UART  | Reads entire NOR (256 pages × 4096 bytes) → UART raw binary    |
| Launch OF               | Boots the original firmware (5-second countdown)                |
| Restart                 | Calls `system_reboot()`                                         |
| Power off               | Calls `power_off()`                                             |

Navigation: MENU/LEFT = up, PLAY/RIGHT = down, SELECT = activate.

### LCD init note (open issue)

`bootloader/ipod-s5l87xx.c:836`:
```c
// TODO: see if removing this causes the nano3g LCD to initialize properly
#ifdef S5L87XX_DEVELOPMENT_BOOTLOADER
    sleep(HZ);
    for (int i = 0; i < lcd_type+1; i++) {
        sleep(HZ/2);
        piezo_seq(alivelcd);
    }
#endif
```

This 1-second sleep was added as a workaround for suspected Nano 3G LCD initialization
timing issues.  It should be tested with and without the delay once hardware testing
is possible.  The `lcd_type` value and `lcd_id` (if `S5L_LCD_WITH_READID` is defined)
would be displayed immediately after.

### Enabling UART debug logging

In `firmware/export/config/ipodnano3g.h`:
```c
#ifdef BOOTLOADER
#if 0 /* Enable/disable LOGF_SERIAL for bootloader */
#define HAVE_SERIAL
#define ROCKBOX_HAS_LOGF
#define LOGF_SERIAL
#endif
```

Change `#if 0` to `#if 1` to enable serial LOGF output during bootloader execution.
This feeds `logf()` output through `uart-s5l8702.c`.  The `Dump bootflash to UART`
dev menu option also requires `HAVE_SERIAL`.

---

## 8. Hardware Developer Setup Requirements

To do any hardware testing on the Nano 3G the following is needed:

| Requirement             | Purpose                                          |
|-------------------------|--------------------------------------------------|
| iPod Nano 3G device     | Any hardware revision, any storage size          |
| USB cable (data)        | DFU mode, wInd3x exploit                         |
| Python 3 + pyusb        | Running wInd3x                                   |
| wInd3x tool             | Exploit-based DFU upload to install bootloader   |
| Linux/macOS host        | USB DFU enumeration (libusb works best on Linux) |
| UART adapter (optional) | Serial debug output (3.3 V logic level)          |
| Built RB bootloader     | `make` in a bootloader build directory           |
| Built `mks5lboot`       | Packages bootloader into DFU installer image     |

### UART pinout

The UART TX pin on the S5L8702 is accessible on the Nano 3G dock connector.
The exact pin mapping should be verified against the iPod Classic schematic or
community hardware documentation — it is the same dock pinout as other S5L8702
devices (iPod Classic 6G).

---

## 9. Known Nano 3G Boot Issues / Open Questions

| Issue | Location | Status |
|-------|----------|--------|
| LCD initialization may require timing delay | `bootloader/ipod-s5l87xx.c:836` | TODO — untested without delay |
| Real audio codec unknown | `ipodnano3g.h:141` | CS42L55 placeholder; `Codc` SysCfg entry needed |
| NAND storage completely stubbed | `nand-nano3g.c` | Blocks any real firmware test |
| RTC disabled | `ipodnano3g.h:128` | Chip unidentified |
| Battery curves from Nano2G | `powermgmt-nano3g.c` | Likely incorrect |
| `HAVE_BOOTLOADER_USB_MODE` disabled | `ipodnano3g.h:242` | USB mode not available in bootloader |

---

## 10. Acceptance Criteria — Phase 3

- [x] Boot sequence timeline documented with timestamps from source code
- [x] NOR flash memory map documented with constants and offsets
- [x] IM3 image format described (header/body, signatures, encryption)
- [x] Dualboot install/uninstall mechanism documented with NOR layout diagrams
- [x] wInd3x install path outlined (steps, prerequisites, build instructions)
- [x] SysCfg structure documented (including `Codc` tag significance for codec discovery)
- [x] Development bootloader features and navigation documented
- [x] LCD init TODO noted as open issue
- [x] UART debug logging enable instructions included
- [x] Hardware developer setup requirements listed
- [x] All open issues / known unknowns enumerated

---

*See also: `docs/porting/ipodnano3g-roadmap.md` for the full phase plan.*

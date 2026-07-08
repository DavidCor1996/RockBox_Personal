# Palm OS 3.0 on iPod 6G Rockbox

## Current status

`apps/plugins/palmos.c` is a Rockbox-side frontend harness for iPod 6G:

- discovers `/.rockbox/palmos/roms/Palm-OS-3.0-en.rom`
- falls back to `/home/david/Downloads/Palm-OS-3.0-en.rom` in the simulator
- creates `roms`, `cards`, and `state` directories
- persists frontend state
- provides a 160x160 Palm screen surface
- maps clickwheel controls into pointer, text, and menu modes

It does not yet contain a 68k Palm hardware core. The displayed Palm screen is still a harness, not a booted OS framebuffer.

## Host boot proof

The ROM in `/home/david/Downloads/Palm-OS-3.0-en.rom` was tested against CloudpilotEmu as the candidate Palm core.

Temporary verifier setup:

- cloned CloudpilotEmu to `/tmp/cloudpilot-emu`
- replaced the missing native network backend with a no-op local stub
- removed absent native network backend sources from the temporary Makefile
- built `cloudpilot-emu`
- built `/tmp/palmos_boot_probe`, which calls:
  - `util::initializeSession(rom, "PalmIII")`
  - `gSession->RunEmulation(10000)`
  - `EmHAL::CopyLCDFrame(frame, fullRefresh)`

Observed result:

```text
ROM info
================================================================================
Card version:          2
Card name:             PalmCard
Card manufacturer:     Palm Computing
Store version:         1
Company ID:            0
HAL ID:                0
ROM version:           0
ROM version string:
CPU:
Databases:             System AMX UIAppShell PADHTAL Library IrDA Library Net Library PPP NetIF SLIP NetIF Loopback NetIF MS-CHAP Support Network Address Book Calculator Date Book Launcher Memo Pad Preferences Security HotSync To Do List Digitizer General Formats ShortCuts Owner Buttons Modem Mail Expense
================================================================================
using device: PalmIII
frame step=3 cycles=40002 bpp=1 width=160 lines=160 bytes_per_line=20 checksum=0x545c9dc5
frame step=83 cycles=840154 bpp=1 width=160 lines=160 bytes_per_line=20 checksum=0xbeaec729
frame step=84 cycles=850154 bpp=1 width=160 lines=160 bytes_per_line=20 checksum=0x98b08651
summary cycles=850154 frame_changes=3 power=on cpu_stopped=no
```

This confirms the Palm OS 3.0 ROM/device pairing reaches live CPU execution and produces changing 160x160 1bpp LCD frames under Cloudpilot's PalmIII model.

## Rockbox integration target

The next implementation step is not control mapping; it is core integration.

Recommended path:

1. Vendor a PalmIII-only Cloudpilot core subset under `apps/plugins/palmos_core/`.
2. Keep only the DragonBall 68328 path needed by `PalmIII` and the Palm OS 3.0 ROM.
3. Drop desktop-only pieces: SDL, readline, curl, websocket/native network proxy, debugger CLI, external storage mounts, CLIE, HandEra, Acer, Palm VZ/SZ models, and color LCD devices.
4. Add a small C ABI used by `palmos.c`:

```c
struct palmos_frame {
    const unsigned char *bits;
    int width;
    int height;
    int bpp;
    int bytes_per_line;
};

int palmos_core_init(const unsigned char *rom, unsigned int rom_size);
int palmos_core_run(unsigned int cycles);
int palmos_core_get_frame(struct palmos_frame *frame);
void palmos_core_pen(int x, int y, int down);
void palmos_core_key(unsigned int palm_key);
void palmos_core_shutdown(void);
```

5. Either teach the Rockbox plugin build to compile/link the required C++17 objects, or convert the reduced subset to C-compatible build rules.
6. Replace the placeholder `palm_core_*` functions in `apps/plugins/palmos.c` with the ABI above.

## Size risk

The full Cloudpilot native link pulls in more than 100 emulator objects and many device models. A direct import is too large for an iPod 6G plugin. The practical target is a PalmIII-only reduction validated against this ROM first, then performance tuning on the simulator and hardware build.

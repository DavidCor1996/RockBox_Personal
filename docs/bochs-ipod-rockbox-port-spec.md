# Bochs iPod Rockbox Port Spec

## Purpose

Evaluate and specify a first Bochs-based Rockbox plugin capable of booting a
small DOS/Windows 3.1 disk image on iPod Classic 6G/7G hardware. The first goal
is a proof of concept, not a polished desktop environment.

## Recommendation

Bochs is technically portable enough to try, but it is a high-risk port for
Rockbox because it is a large C++ PC emulator normally built with autoconf and
desktop host services. The first milestone should be a stripped, static,
no-sound, no-network, VGA-only Bochs plugin for iPod 6G. If that cannot boot DOS
at an acceptable speed, stop the Bochs path and switch to DOSBox or PCE/ibmpc.

## Target

Initial target:

- Rockbox `ipod6g` only.
- Apple iPod Classic/6G, S5L8702 ARM target.
- 64 MB RAM.
- 320x240 RGB565 LCD.
- Clickwheel with absolute wheel position.
- 3 MiB plugin buffer.
- Additional runtime memory from `plugin_get_audio_buffer()` after stopping
  audio playback.

Do not target iPod Video/5G first. It has the same 320x240 LCD but a smaller
1 MiB plugin buffer and weaker practical headroom for a desktop-class emulator.

Local references:

- `firmware/export/config/ipod6g.h`
- `firmware/export/config/ipodvideo.h`
- `apps/plugin.c`
- `apps/plugin.h`
- `apps/plugins/rockboy/rockboy.c`
- `apps/plugins/pokemini/pokemini_rockbox.c`
- `apps/plugins/plugins.make`

## Upstream Facts

Bochs is a portable IA-32/x86 PC emulator written in C++. It emulates x86 CPU,
common PC I/O devices, memory, and BIOS, and can run DOS and Microsoft Windows
guests. It is LGPL-2.1 licensed.

Bochs can be configured for different x86 CPU levels. The useful minimum for
Windows 3.1 is 386-class emulation. Bochs supports 386, 486, Pentium, and later
CPU families, but later CPUs and optional devices are unnecessary for this port.

Bochs provides compile-time display library choices including X11, SDL, SDL2,
term, RFB/VNC, wx, and `nogui`. None is a direct Rockbox backend. A Rockbox
backend must be written.

Bochs configuration supports:

- `cpu: ... ips=...` for emulated instruction timing.
- `memory: guest=..., host=...` or `megs: ...`.
- `romimage` and `vgaromimage`.
- `boot: floppy`, `boot: disk`, or multiple boot choices.
- ATA disk images, including flat images.
- VGA and VBE.
- PS/2 mouse and keyboard style input.
- Optional SB16, ES1370, network, USB, PCI, Cirrus, Voodoo, debugger, and
  logging features.

External references:

- Bochs repository: https://github.com/bochs-emu/Bochs
- Bochs docs: https://bochs.sourceforge.io/doc/docbook/user/
- Features: https://bochs.sourceforge.io/doc/docbook/user/features.html
- Compile options: https://bochs.sourceforge.io/doc/docbook/user/compiling.html
- Runtime config: https://bochs.sourceforge.io/doc/docbook/user/bochsrc.html
- Setup requirements: https://bochs.sourceforge.io/doc/docbook/user/setup.html

## Guest Scope

Supported for prototype:

- MS-DOS compatible boot disk.
- Windows 3.1 or Windows for Workgroups 3.11 already installed into a hard disk
  image.
- VGA 640x480x16 or 640x480 mono/grayscale, scaled/panned to the iPod LCD.
- PS/2 mouse.
- Standard keyboard through small on-screen key picker and mapped shortcuts.

Out of scope for prototype:

- Windows 95/98.
- Windows NT.
- Linux guests.
- Sound.
- Networking.
- USB guest devices.
- CD-ROM installation from physical media.
- Printing.
- Save states.
- Dynamic recompilation.
- Full-screen 1:1 desktop readability.

## Legal And Asset Boundaries

Do not ship Microsoft DOS, Windows, or third-party BIOS assets that are not
redistributable. Bochs includes its own BIOS and LGPL VGABIOS assets, but the
Windows/DOS image must be supplied by the user.

Recommended user-owned layout:

```text
/.rockbox/bochs/
    bochsrc.txt
    BIOS-bochs-latest
    VGABIOS-lgpl-latest
    win31.img
    keymap.txt
```

The plugin should fail with clear messages if required files are missing.

## Port Shape

Preferred shape:

```text
apps/plugins/bochs/
    bochs.make
    SOURCES
    rockbox/
        rb_bochs_main.cc
        rb_display.cc
        rb_input.cc
        rb_files.cc
        rb_memory.cc
        rb_config.cc
    upstream/
        selected Bochs source files
```

Rockbox currently compiles plugins with C rules in `apps/plugins/plugins.make`.
Because Bochs is C++, the prototype needs one of these approaches:

1. Add C++ plugin compile/link rules for this plugin only.
2. Build Bochs as a static C++ archive with custom rules, then link the `.rock`
   with the C++ compiler.
3. Create a separate experimental build target outside normal `make zip`.

Option 2 is preferred because it keeps the C++ exception/RTTI/runtime choices
localized. Build with no exceptions and no RTTI if Bochs permits it:

```text
-fno-exceptions -fno-rtti -fno-threadsafe-statics
```

Verify whether the ARM cross toolchain has enough C++ runtime support. If not,
this becomes the first hard blocker.

## Feature Cut List

Compile or stub out:

- debugger and debugger GUI
- instrumentation
- GDB stub
- logging beyond a small ring buffer
- plugins/shared-library loading inside Bochs
- x86-64
- SMP
- VMX/SVM/AVX/3DNow
- PCI unless required by the selected VGA/IDE config
- Cirrus and Voodoo
- SB16 and ES1370
- gameport
- all networking
- raw serial
- USB UHCI/OHCI/EHCI/XHCI
- CD-ROM host drive access
- RFB/VNC, SDL, SDL2, X11, wx, term

Keep:

- 386 CPU level, possibly 486 only if Windows setup requires it
- FPU only if Windows guest/apps need it
- A20 support
- BIOS
- VGA/VGABIOS
- PIT/PIC/DMA/CMOS/RTC
- keyboard controller
- PS/2 mouse
- floppy image support for install/test
- ATA flat disk image support

Candidate configure intent for upstream exploration:

```text
--with-nogui
--disable-plugins
--disable-debugger
--disable-debugger-gui
--disable-gdb-stub
--disable-logging
--enable-cpu-level=3
--disable-smp
--disable-x86-64
--disable-vmx
--disable-svm
--disable-avx
--disable-sb16
--disable-es1370
--disable-gameport
--disable-ne2000
--disable-pnic
--disable-e1000
--disable-clgd54xx
--disable-voodoo
--disable-pci
--disable-usb
--disable-usb-ohci
--disable-usb-ehci
--disable-usb-xhci
```

This is a starting point, not expected to work unchanged in Rockbox. Some
Bochs code assumes a host OS, autoconf results, and standard C/C++ library
facilities.

## Runtime Configuration

Prototype `bochsrc.txt` target:

```text
display_library: rockbox
romimage: file=/.rockbox/bochs/BIOS-bochs-latest, options=fastboot
vgaromimage: file=/.rockbox/bochs/VGABIOS-lgpl-latest
cpu: model=386, count=1, ips=500000
memory: guest=4, host=4
vga: extension=vbe, update_freq=10
ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14
ata0-master: type=disk, path=/.rockbox/bochs/win31.img, mode=flat
boot: disk
mouse: enabled=1
clock: sync=none, time0=local
log: -
panic: action=fatal
error: action=report
info: action=ignore
debug: action=ignore
```

The initial `ips` value is intentionally conservative. Bochs uses IPS for
timing calibration; it is not only a speed display. Tune after measuring on
hardware.

## Memory Plan

Use `plugin_get_audio_buffer()` and stop playback. This mirrors large-memory
plugins already in the tree.

Minimum prototype budget:

- 4 MB guest RAM.
- 1 MB video/work buffers.
- 128 KB BIOS.
- VGABIOS image.
- Bochs heap/state.
- File cache no larger than 256 KB.

The plugin must not malloc from libc. Route allocations through a fixed Rockbox
arena:

- reserve a single arena from `plugin_get_audio_buffer()`;
- provide `operator new/delete` over that arena if C++ is used;
- fail early if the arena is too small;
- avoid fragmented general allocation.

Hard requirement:

- boot DOS with 4 MB guest memory before attempting Windows 3.1.

## Display Plan

Bochs VGA output must be adapted to Rockbox RGB565.

Prototype display modes:

1. `pan` mode: render 640x480 VGA into an offscreen buffer and display a
   320x240 viewport. Clickwheel/buttons pan the viewport when in pan mode.
2. `scale2` mode: nearest-neighbor 640x480 to 320x240. Text is small but the
   full desktop is visible.
3. `zoom` mode: 1:1 viewport for setup dialogs and text.

First implementation should support `scale2` and `pan`; defer smoother scaling.

VGA color handling:

- Convert indexed VGA pixels through Bochs palette to RGB565.
- Limit updates to dirty rectangles if Bochs exposes them through the display
  backend.
- Cap LCD refresh to 5-10 Hz for the first Windows milestone.

## Input Plan

The clickwheel controls the mouse cursor by default.

Default mode:

- wheel clockwise/counterclockwise: mouse X movement
- hold `Play` + wheel: mouse Y movement
- `Select`: left mouse button
- hold `Select`: drag
- `Menu` + `Select`: right mouse button
- `Left`: Escape
- `Right`: Enter
- `Menu` + `Play`: emulator menu / quit

Alternate mode:

- `Menu` toggles keyboard mode.
- wheel selects key from an on-screen strip.
- `Select` sends selected key.
- `Left`/`Right` switch key groups.

Must support a small fixed shortcut list:

- `Ctrl+Esc`
- `Alt+F4`
- `Tab`
- `Shift+Tab`
- `Enter`
- `Esc`
- arrow keys
- `Y`, `N`, and space

Auto-repeat must be slow enough for Windows dialogs. The plugin should disable
normal wheel UI events during emulation with `wheel_send_events(false)` and
restore them on exit.

## Auto Boot / Auto Login

Rockbox autostart:

- Add an `openplugin` entry pointing to `/.rockbox/rocks/apps/bochs.rock`.
- Parameter defaults to `/.rockbox/bochs/bochsrc.txt`.
- Use the same direct-start pattern already used by Rockboy tooling.

Guest autostart:

- Prepare the disk image outside Rockbox.
- `AUTOEXEC.BAT` should run Windows directly:

```bat
@ECHO OFF
PATH C:\DOS;C:\WINDOWS
SET TEMP=C:\WINDOWS\TEMP
WIN
```

For kiosk mode, put the desired app in the Windows Startup group or use a
custom Program Manager replacement. Do not make the Bochs plugin automate
Windows installation.

## Performance Expectations

Expect slow performance. Bochs emphasizes portability and accuracy over speed,
and its own docs show tens of MIPS on much faster desktop CPUs. A 216 MHz ARM9
iPod is likely to land far below original 386DX desktop responsiveness once VGA,
BIOS, timers, IDE, and mouse are included.

Acceptable proof-of-concept bar:

- DOS prompt responds to input.
- Windows 3.1 reaches Program Manager.
- Mouse movement is visible.
- Simple app launch is possible.

Non-goal:

- games, video, web, or audio.

Abort criteria:

- DOS boot takes more than 5 minutes from a flat disk image.
- Windows 3.1 Program Manager takes more than 15 minutes from DOS prompt.
- Cursor latency is consistently over 2 seconds after boot settles.
- Plugin binary cannot fit iPod 6G loader constraints without invasive global
  build changes.

## Milestones

1. Host-side shrink build

Build Bochs on Linux with the intended feature cuts and boot a DOS image using
`nogui` or a minimal test display. Record binary size and required source files.

2. Rockbox C++ toolchain probe

Build a trivial C++ plugin for `ipod6g` using no exceptions, no RTTI, and custom
`new/delete`. This proves whether Bochs can be linked as C++ in this tree.

3. Rockbox shell plugin

Create `bochs.rock` that parses a config path, allocates the large arena, opens
BIOS/VGABIOS/disk files, and exits cleanly.

4. Display backend

Implement a Rockbox Bochs display backend that can receive VGA updates and draw
static frames to LCD.

5. Input backend

Implement keyboard and PS/2 mouse event injection from clickwheel/buttons.

6. DOS boot

Boot a minimal DOS image. Validate keyboard input, disk reads, and display.

7. Windows 3.1 boot

Boot a preinstalled Windows 3.1 image. Validate mouse, dialogs, Program Manager,
and shutdown.

8. Direct-start integration

Add launcher/autostart support and a default config path.

## Validation Checklist

Simulator:

- plugin loads and exits without corrupting Rockbox state;
- missing asset errors are readable;
- no unbounded allocation after startup;
- USB/default event handler still works;
- display backend renders nonblank frames;
- input mode toggles work.

Hardware:

- boots DOS from disk image;
- no audio playback conflict after taking audio buffer;
- backlight timeout handling restored on exit;
- wheel events restored on exit;
- disk image writes persist;
- hard poweroff/reboot does not corrupt the Rockbox config.

## Main Risks

C++ plugin support:

Rockbox plugin build rules are C-oriented. Bochs is C++. This is the first risk
to resolve.

Binary size:

Even stripped down, Bochs may not fit within plugin constraints without overlay
work.

Runtime speed:

Bochs may boot but be too slow to use.

Host assumptions:

Bochs expects autoconf, file paths, timers, logging, display libraries, and
possibly C/C++ library features that Rockbox does not provide.

Mouse usability:

Clickwheel cursor control is feasible but must be tuned carefully to avoid
overshooting and to support drag operations.

Disk writes:

Windows writes often. The port needs conservative flush behavior to avoid disk
image corruption on low battery or forced exit.

## Decision Gate

Continue Bochs only if milestones 1-6 pass with tolerable speed. If the C++
plugin or DOS boot milestones fail, switch to:

- DOSBox for Windows 3.1 as a DOS/Win16-focused path.
- PCE/ibmpc for a smaller full-machine PC emulator path.
- PCE/macplus or Mini vMac for old Macintosh OS.

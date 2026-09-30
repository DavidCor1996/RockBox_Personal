# Mini vMac iPod Rockbox Optimization Spec

## Goal

Make the Mini vMac Rockbox plugin usable on iPod Classic 6G hardware after the
first successful boot-to-desktop milestone.

This pass targets practical interaction and clearer output on iPod Classic 6G
without changing the Mini vMac emulator core.

## Current Runtime Layout

```text
/.rockbox/rocks/viewers/minivmac.rock
/.rockbox/minivmac/vMac.ROM
/.rockbox/minivmac/disk1.dsk
/.rockbox/minivmac/disk2.dsk
```

Viewer associations:

```text
dsk,viewers/minivmac,6
img,viewers/minivmac,6
```

## Controls

Default mode is mouse mode and follows Desktop Mode's click-wheel pointer
model.

- Moving around the wheel steers the mouse in two dimensions with subpixel
  precision and bounded acceleration.
- Resting a finger on the wheel glides the pointer in the direction of that
  point on the ring.
- Select is the Macintosh mouse button; press, move, and release supports
  dragging.
- Left/Right provide held-direction horizontal fallback movement.
- The 30-pin remote uses Desktop Mode's mappings: Previous/Next move
  horizontally, remote Play/Select clicks, remote Menu/Stop goes back, and
  remote Up/Down moves vertically.
- Play release toggles the keyboard picker. Wheel chooses a key, Select posts
  it, and Play returns to pointer mode. Select + Menu remains an alternate
  keyboard shortcut.
- Menu release opens a Return to iPod confirmation. Select confirms; Menu or
  Play cancels. Holding Play also opens the confirmation.
- Holding Menu toggles the sharp viewport and full-frame overview while
  undocked. Docked output always uses the full-frame layout.
- The hold switch releases any active mouse press and suppresses pointer and
  button input.

One-shot controls consume only the first repeat event so holding Play or Menu
cannot rapidly toggle state.

## Timing

Mini vMac expects 60 emulated ticks per second. Rockbox on this target uses
`HZ == 100`, so timing must use fractional accumulation:

```text
accumulator += elapsed_rockbox_ticks * 60
advance = accumulator / HZ
accumulator %= HZ
```

The plugin must not force one emulated tick per Rockbox tick. That causes the
Mac to run around 100 Hz instead of 60 Hz and wastes CPU/battery.

If no emulated tick is due, the plugin should yield and wait for more Rockbox
ticks while still polling input regularly.

## Video

The default undocked display mode is a sharp native viewport:

- Mac screen: 512x342 mono.
- iPod LCD: 320x240.
- Mac viewport: 320x240, using the full panel rather than reserving a permanent
  status strip.
- Viewport follows the emulated mouse.
- Horizontal viewport origin is aligned to 8 pixels so one source byte maps to
  eight destination pixels.

Native viewport rendering should use a byte-to-8-pixels lookup table. This
reduces the hot path from one bit test per output pixel to one byte decode per
8 output pixels.

The undocked full-frame overview preserves the Macintosh aspect ratio at
320x214 and centers it vertically. It uses a precomputed 2x2 supersampling map
and a five-level grayscale result instead of nearest-neighbor monochrome, which
keeps small Macintosh text and diagonal edges legible.

When a qualified iPod 6G video dock is active, Mini vMac uses all 320x240
Rockbox source pixels for the complete 512x342 Macintosh frame. The 6G DCP750
driver then scales that complete source to its qualified 648x432 TV viewport;
Mini vMac must not allocate or address the TV raster directly. Because the two
stages have complementary aspect ratios, the final docked image is nearly an
exact 3:2 Macintosh image and uses the entire output viewport.

The Applications launcher passes the current 6G dock state into Mini vMac, and
the plugin subscribes to `SYS_EVENT_VIDEOOUT_CHANGED` so plugging or unplugging
the dock changes layouts without restarting the emulator.

## Immediate Implementation

1. Keep 60 Hz timekeeping with fractional tick accumulation.
2. Use Desktop Mode's two-dimensional click-wheel pointer and dock remote
   mappings.
3. Use the full 320x240 panel for the native viewport.
4. Supersample the full-frame overview into grayscale using precomputed maps.
5. Switch automatically between the undocked 320x214 overview and docked
   320x240 source layout.
6. Rebuild and validate the iPod 6G hardware and simulator targets locally.
7. Do not deploy to the physical iPod until explicitly requested.

## Deferred Work

- Dirty-rect LCD updates using Mini vMac's changed rectangle.
- Settings file for mouse speed, disk write protection, and default view mode.
- Better keyboard entry UI.
- Optional disk read-only mode for system disks.
- Profiling gate that records frame time and emulated tick rate on hardware.

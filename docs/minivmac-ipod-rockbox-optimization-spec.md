# Mini vMac iPod Rockbox Optimization Spec

## Goal

Make the Mini vMac Rockbox plugin usable on iPod Classic 6G hardware after the
first successful boot-to-desktop milestone.

This pass targets practical interaction and CPU reduction without changing the
Mini vMac emulator core.

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

Default mode is mouse mode.

- Wheel: move mouse on current axis.
- Play: toggle wheel axis between X and Y.
- Left/Right: nudge mouse horizontally.
- Select: mouse button.
- Menu: toggle sharp native viewport and fit overview.
- Select + Menu: toggle keyboard picker.
- Menu + Play: exit plugin.

One-shot controls must ignore `BUTTON_REPEAT`; holding Play or Menu must not
rapidly toggle modes.

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

The default display mode is a sharp native viewport:

- Mac screen: 512x342 mono.
- iPod LCD: 320x240.
- Mac viewport: 320x214, leaving status text below.
- Viewport follows the emulated mouse.
- Horizontal viewport origin is aligned to 8 pixels so one source byte maps to
  eight destination pixels.

Native viewport rendering should use a byte-to-8-pixels lookup table. This
reduces the hot path from one bit test per output pixel to one byte decode per
8 output pixels.

Fit overview remains available, but it is secondary. It may use precomputed
X/Y maps to avoid per-pixel division.

## Immediate Implementation

1. Add a Mini vMac Rockbox optimization spec.
2. Fix 60 Hz timekeeping with fractional tick accumulation.
3. Ignore repeat events for Play/Menu/Select+Menu toggles.
4. Precompute fit-mode X/Y maps.
5. Add native-mode mono byte lookup table.
6. Rebuild ARM plugin and push to mounted iPod.

## Deferred Work

- Dirty-rect LCD updates using Mini vMac's changed rectangle.
- Settings file for default axis, mouse speed, disk write protection, and
  default view mode.
- Better keyboard entry UI.
- Optional disk read-only mode for system disks.
- Profiling gate that records frame time and emulated tick rate on hardware.

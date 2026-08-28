# iPod Classic 6G/7G to Philips DCP750 bench test

This procedure is for the opt-in composite-video diagnostic in the custom
Rockbox `ipod6g` build. It does not apply to an iPod touch. Do not connect or
deploy this firmware to an iPod touch.

This is the procedure; the authoritative physical observations and current
qualification build are recorded in
[`ipod6g-dcp750-videoout-results.md`](ipod6g-dcp750-videoout-results.md).

## Before the DCP750 arrives

- Keep the known-good stock or current Rockbox firmware available for recovery.
- Use an iPod Classic 6G, 6.5G, or 7G with a charged battery.
- Normal settings and boot paths force video output off. Do not bypass this
  guard or add playback activation until the staged diagnostic passes on the
  physical dock.
- Do not add a playback hook until every diagnostic stage below has been
  observed on the real dock.
- The prepared firmware is `build-hw-ipod6g/rockbox.ipod`.
- Calculate and record the exact firmware checksum immediately before deploy:
  `sha256sum build-hw-ipod6g/rockbox.ipod`.
- Preserve the separate known-good recovery image. Run
  `tools/ipod6g_dcp750_readiness_gate.sh` and do not deploy if it fails.

## Firmware deployment

The Classic must be identified unambiguously before deployment. Disconnect any
iPod touch first. A DFU device is not a mounted Rockbox volume, so do not copy
the firmware while the Classic remains in DFU.

For a complete 6G package deployment, use:

```sh
tools/deploy_ipod6g_preserve_database.sh
```

The deployment must preserve the tag database, keep `tagcache_autoupdate`
enabled, install `rockbox.ipod` at both the volume root and
`.rockbox/rockbox.ipod`, verify both against the local build, and sync before
ejecting.

## First connection

1. Power off the DCP750 and the Classic.
2. Seat the Classic directly in the DCP750 dock without an adapter or extension.
3. Power on the DCP750 and select its iPod source.
4. Boot Rockbox normally and confirm its local LCD and controls work.
5. Before touching video, enable `Accessory Power Supply`, `Line Out`, and
   automatic `Serial Bitrate` in Rockbox. Play a music track and verify the
   DCP750 speakers, charging state, remote buttons, and physical clickwheel.
   Then open Live TV and verify Previous/Next changes exactly one channel and
   Play opens the full guide. In the guide, verify Previous/Next taps change
   channel rows, holding them moves through time, and Play tunes the selection.
   Test any transmitted Menu, Select, Up, and Down events too. Open Desktop
   Mode and verify Previous/Next moves focus and Play opens the focused item.
   Keep the clickwheel usable in both applications.
6. Stop audio and video playback before opening the diagnostic.
7. Open `System` > `Debug (Keep Out!)` > `Test composite video`.

The DCP750 receives analog codec line-out directly through the 30-pin connector;
it does not use the laptop USB-audio bridge. The slight choppiness heard in the
RockPod laptop test therefore does not predict dock audio performance. Treat
clean dock audio as its own required observation rather than an assumption.

The diagnostic is manual and staged. Follow its on-screen controls and wait at
least five seconds after each change. An active stage shuts itself off after 60
seconds, so record the observation and advance or exit before that deadline:

| Stage | Output | Required observation |
| --- | --- | --- |
| 0 | Off | DCP750 remains in its normal no-video state. |
| 1 | NTSC sync | DCP750 should lock rather than roll or repeatedly reacquire. |
| 2 | Solid blue | A stable, edge-to-edge blue field should appear. |
| 3 | Color bars | Bars should be stable and colors should be distinct. |
| 4 | Static framebuffer | The Rockbox framebuffer should appear at the expected aspect and orientation. |
| 5 | Manual refresh | UI changes should appear only when refresh is requested. |

Press `MENU` to exit. Exiting disables the encoder, router, and compositor,
then restores the exact SVID clock, power-gate bits, and dock-pin function that
were present before the test.

## Live playback qualification (not enabled yet)

The persistent setting and Quick Settings activation path are deliberately
locked off after a freeze was observed from direct activation. After stages 0
through 5 pass on the physical dock, record the results before changing that
guard. A later qualification build can then test:

1. Exit the diagnostic and open the LCD display settings.
2. Enable `Composite Video Output` in that qualification build.
3. Play one MPEGPlayer clip and one OpenH264 clip with continuous motion.
4. Confirm moving video, paused frames, controls, and volume overlays all appear.
5. Let each clip run for at least ten minutes and watch for tearing, stale frames,
   audio underruns, resets, temperature changes, or loss of sync.
6. Stop playback, disable `Composite Video Output`, and confirm the DCP750 loses
   the generated signal cleanly.
7. Re-enable it, reboot once, and confirm the saved setting restores output.
8. Power off and confirm the DCP750 loses the generated signal without rolling
   or repeatedly reacquiring.

Before every firmware deployment, run the linked-image gate:

```sh
tools/ipod6g_videoout_static_gate.sh
```

## Stop conditions

Exit the diagnostic immediately if any of these occurs:

- The Classic resets, freezes, becomes unusually warm, or loses local LCD input.
- The DCP750 repeatedly loses sync or shows a rolling image at stage 1.
- The image is stable but monochrome, badly clipped, or geometrically corrupt.
- Audio playback or the Rockbox playback buffer changes unexpectedly.
- `MENU` does not return to the debug menu promptly.

If normal exit fails, power off the DCP750 and reset the Classic. Do not repeat
the test until the symptom and last successful stage are recorded.

## Result record

Record these items before changing code:

- Exact Classic model/capacity and Rockbox version from `rockbox-info.txt`.
- DCP750 model suffix and region.
- Result of each stage from 0 through 5.
- Whether the DCP750 was charging the Classic.
- Whether preflight line-out audio was clean and the stopped diagnostic emitted
  no clicks, underruns, or unexpected sound.
- A photo of stages 2, 3, and 4 when they are visible.
- Any reset, loss of sync, color error, cropping, or aspect-ratio error.

The persistent `Composite Video Output` setting defaults to off and currently
fails closed if an older configuration requests Auto or On. Quick Settings
points to the staged diagnostic but cannot start the hardware. The diagnostic
mirrors the existing LCD framebuffer without taking ownership of playback
memory. Live playback qualification is complete only after the guard is lifted
in a dedicated build and both direct-YUV player checks also pass.

# P16: restore audio hardware after retained wake

P16 is a locally built test candidate. It has **not been deployed or hardware
qualified**. The current device is a Nano 3G; deployment requires the user's
approval on the intended Classic 6G/7G.

## Report and diagnosis

The user reports that P15 repeatedly returns to the retained screen and music
position, but there is no sound and the progress bar stays stationary.
Pressing Play/Pause twice does not recover playback. The exact cycle count,
other wake modes, and battery consumption have not been established.

P15 restores the digital I2S/DMA side and sample-rate register, then uses the
codec's ordinary idle power-up. That assumes its other registers survived.
The target's PMU setup powers CS42L55 from LDO4 and does not retain LDO4 in
Standby. A reset CS42L55 loses its master-clock configuration and output
settings. Since this chip supplies the serial audio clocks, that mismatch is a
plausible explanation for both silence and stationary playback. Hardware
confirmation of this diagnosis is still required.

## Change

- Save the complete writable CS42L55 register bank before suspend mute and PDN.
  Skip read-only, clear-on-read status. Refuse entry if any read fails.
- After digital hardware and the scheduler return, reset the codec, apply the
  same required initialization values as cold startup, and restore the saved
  clock, routing, volume and DSP register settings. Retained software tone/DSP
  bookkeeping stays intact.
- Configure while powered down and muted at -60 dB, allow reset and amplifier
  startup time, verify key clock/output settings, then restore the saved mute
  and power policy. All new register transfers check their return status.
- Keep physical DMA blocked until this completes. A partial restore holds the
  codec in hardware RESET, records failure 21 and takes the existing failed-wake
  shutdown path. Its block is separate from USB audio's ownership flag.
- Keep the existing paired mixer lock and channel state. This adds no transport
  Play command, so paused/stopped transport remains paused/stopped. The original
  pre-entry rollback remains available when Standby was never entered.

The cold-start initialization table is shared without changing its values or
order. Legacy `cscodec_read` now initializes its fallback byte; the retained
save/restore exclusively uses the checked interface.

## Evidence

The pinned Apple RetailOS 2.0.4 image passes its existing machine-code audit
(`apple-audit.log`). Its output backend opcode 8 at 0x081e9904 carries a lock;
opcode 9 at 0x081e99bc restores configuration before queue rebuilding and unlock.
The latter calls configuration dispatcher 0x08091b98 at 0x081e9a04 or 0x081e9a38.
The dispatcher registration and complete Apple codec implementation have not
been recovered here. P16's codec snapshot/replay, error policy and readback are
Rockbox adaptations, not a claim of an exact Apple codec routine transplant.

Primary image: [RetailOS 2.0.4 archive](https://github.com/iEMU-Android/iPod_35.2.0.4),
SHA-256 `f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4`.

The manufacturer describes register reset on rail loss, master-mode clock
output, and the required initialization/startup sequence in sections 4.8,
4.10–4.13 of the [Cirrus CS42L55 datasheet](https://media.digikey.com/pdf/Data%20Sheets/Cirrus%20Logic%20PDFs/CS42L55.pdf).
P16 waits at least 1 ms with RESET low, at least 500 ns before register writes,
and at least 75 ms for amplifier startup; those waits occur with the scheduler
live. The analog volume code is taken from sections 6.21–6.22.

## Local validation

- Clean iPod 6G application build and matching PictureFlow build: PASS.
- Final linked-image gate with the preserved ABI-12 R12 bootloader: PASS.
  Includes codec save-before-mute, restore-after-tick-repair, combined
  display/video/audio success, hardware-start block, and prior retained memory,
  media lock, storage, input, interrupt and I2C/ADC checks.
- Ten host tests: PASS. The new cases compile the actual codec driver and PCM
  start/finish functions with AddressSanitizer/UndefinedBehaviorSanitizer.
  They cover 12 rail-loss cycles, active/idle settings, 44.1/48-kHz rates,
  asymmetric volumes, tone/DSP state, startup ordering, every one of 40 snapshot
  read failures, every one of 61 restore write failures, all seven readback
  failures, an acknowledged-but-missing clock write, and DMA inhibition that
  preserves USB audio ownership. These are models, not measurements on an iPod.
- Existing notification gate: PASS, 39,485 bytes within its 39-KiB budget.
- Changed-source whitespace check: PASS. Existing unrelated build warnings
  remain; no new warning in the audio/hibernate implementation.

Version: `04984f554eM-p16-260922`.
Firmware SHA-256: `275cef83e50cdfbe3c0b365f8de94e70a7d0cdf5e19355f3d25e3fef8ba5c2b1`.

See [test steps](TEST-STEPS.md), `manifest.json`, and
`source-changes-from-p15.patch`. The build uses the frozen P15 source snapshot
plus this change, avoiding unrelated concurrent work in the main tree. Source
changes and regression tests are also applied to the main Rockbox tree; its
independent version file is preserved. Normal firmware, databases, codecs,
configurations and the upstream/TV-out slots have not been changed on any iPod.

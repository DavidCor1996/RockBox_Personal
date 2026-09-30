# P15 local qualification — 22 September 2026

P15 was deployed with explicit user authorization to the identified Classic
as a named Rolo candidate, then verified and safely unmounted. The user now
reports repeated retained screen/position restoration, but no sound or progress;
Play/Pause twice also remains stuck. Exact cycle count and full qualification
matrix are not established. P16 addresses the codec register-loss hypothesis;
see the P16 validation record for the fix and pending hardware test.

## Firmware change completed

The unfinished P15 storage-admission, input-release, refusal rollback, and
bounded I2C work is present in the candidate. It retains the ABI-12 resume
contract, media-output lock across Standby, and lazy disk wake after the resume
event is committed.

The remaining production ADC wait was unbounded: its timeout was conditional
on a diagnostic monitor that production disables. Configuration writes,
completion reads, and data reads also ignored transport errors. `pmu_read_adc`
now checks each transfer, applies a wrap-safe 250-ms completion deadline in
production, sleeps between polls so lower-priority threads can run, and releases
its mutex on every outcome. Failure returns a prior completed sample for the
channel, or raw zero before its first success; failed data never updates the
cache. The legacy byte-valued `pmu_read` also initializes its return buffer so
an unsuccessful transfer cannot return uninitialized stack data.

The 250-ms ADC deadline, cached fallback, and best-effort cancellation are
Rockbox liveness adaptations. They are **not** claimed as recovered Apple ADC
policy. The NXP PCF50633 manual describes the conversion-ready bit and
microsecond-scale normal conversion, but does not establish Apple's software
deadline: https://www.freecalypso.org/pub/GSM/GTA02/PCF50633UM_6.pdf

## Evidence and limits

- RetailOS 2.0.4 machine-code audit: PASS, image SHA-256
  `f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4`.
  Source: https://github.com/iEMU-Android/iPod_35.2.0.4
- Two fresh application builds: PASS, identical `.ipod` and ELF bytes. GCC
  9.5.0, `ipod6g`, normal build, candidate define enabled explicitly.
- Final linked-image gate: PASS with the preserved R12 ABI-12 bootloader ELF
  and newly built PictureFlow ELF. Covers retained memory boundaries, service
  order, ATA ownership/admission, lazy demand wake, input neutrality, media
  pairing, rollback, and absence of production diagnostic hooks. Additional
  checks confirm production I2C/ADC timer bounds and checked cleanup.
- Notification gate: PASS, 39,485 bytes within the existing 39-KiB budget.
- Host tests: PASS, eight tests. Actual C ADC/I2C functions run with injected
  transport errors, stuck-ready states, timer wraparound, and repeated
  failure/recovery. A long I2C transfer verifies per-progress deadlines rather
  than a whole-transfer deadline. Six separate cases exercise the ARM
  control-flow checker.
- Build warnings exist in unrelated personal-tree code and plugin libraries;
  no new warning was emitted for the modified PMU source. A successful build
  and host tests cannot validate physical register behavior or battery drain.

The unfinished audit had several compiler-layout assumptions. It now resolves
negative global-data offsets, accepts the actual split-immediate GPIO encoding,
follows backward handler joins, checks both wheel receive blocks, and verifies
inlined notification rollback. Its local control-flow analysis tracks known
constants and zero predicates and treats unknown branches conservatively;
conditional `ble`/`bls` instructions are not misclassified as calls.

Application version: `04984f554eM-p15-260922`.

Application SHA-256:
`a2950d5b3257ce07d2b3a2789a0830efe00770d79a5b977fb9b207bbaa99e919`.

ELF SHA-256:
`da2b8dc711eaaba4570c9054213ee24147e055d9f58723241d9aea1e00b7ebd8`.

## Required physical qualification

Use the separately named P15 ROLO candidate only after approval and device
identification. Confirm the installed resume bootloader implements
`ipod6g-hibernate-abi12-record12`; this task did not install or modify a
bootloader. Keep normal firmware, its runtime, and the official-upstream
application slot intact. The candidate uses `/.rockbox/hibernate-rolo.cfg`.

1. Confirm the dark P15 / Apple Disk Barrier identity after ROLO.
2. With no music playing, hold Play to sleep, wake with a button, and release
   it. Confirm the same screen and selection return and the next independent
   button press works. Repeat at least five times without restarting firmware.
3. Repeat with music playing and paused. Confirm the same track and position,
   correct transport state, and no false Playback Stopped notification.
4. Repeat from different menu selections, WPS, and supported PictureFlow.
   After wake, exercise Database, Files, and artwork to trigger disk demand.
5. Check short Play versus long Play, wake-button release, Hold/unlock, and USB
   wake. Include multiple sleep durations. Record cycle number and first
   post-wake action if any failure occurs.

Do not claim complete Apple parity or release this as hardware-qualified until
that matrix passes. Power consumption and all wake sources remain hardware
measurements, not conclusions from the firmware audit.

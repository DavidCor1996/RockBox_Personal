# iPod Video 5G Warm Resume Plan

## Goal

Get the iPod Video 5G closer to Apple firmware behavior:

- immediate wake after a short power-button sleep
- optional deeper low-power idle after longer inactivity
- resume into the previous Rockbox session without a full cold boot

without increasing the risk of storage corruption, boot loops, or a device that
fails to wake reliably.

This document is intentionally stricter than the general suspend plan in
`docs/ipodvideo_5g_iflash_battery_plan.md`. The 5G power path is sensitive
enough that "it seemed to work once" is not a sufficient standard.

## What The Current Tree Already Has

### 1. Rockbox 5G suspend-in-place

`apps/misc.c` contains the current 5G-specific power-button path.

- `ipodvideo_try_suspend()` pauses playback, saves status, sleeps storage,
  sleeps the LCD, drops CPU frequency, and waits for a wake event.
- It does not implement a true warmboot or hibernate protocol.
- It is deliberately conservative and currently relies on live RAM state.

The code comment in `apps/misc.c` is explicit:

> The 5G Video path does not have a proven Rockbox-owned
> hibernate/warmboot implementation...

That is the central constraint for this work.

### 2. PMU standby / deep sleep entry

The iPod target already powers off through the PMU standby path:

- `firmware/target/arm/ipod/power-ipod.c`
- `firmware/drivers/pcf50605.c`

`power_off()` eventually calls `pcf50605_standby_mode()`, and the PMU driver
describes this as putting the iPod into a deep sleep. So the hardware-side
entry mechanism already exists.

### 3. RTC alarm wake hooks

The 5G target already advertises `HAVE_RTC_ALARM` in:

- `firmware/export/config/ipodvideo.h`

The RTC implementation exists in:

- `firmware/drivers/rtc/rtc_pcf50605.c`

That code already supports:

- `rtc_set_alarm()`
- `rtc_enable_alarm()`
- `rtc_check_alarm_started()`

So deeper low-power behavior with a timed wake is not blocked by a missing RTC
driver.

### 4. Bootloader RAM-resident Rockbox check

The iPod bootloader already knows how to detect a Rockbox image that is still
resident in DRAM:

- `bootloader/ipod.c`
- `firmware/target/arm/pp/crt0-pp.S`

The startup code writes a `Rockbox` signature near the beginning of the loaded
image, and the bootloader checks for that signature at `DRAM_START + 0x20`.
If present, it can jump back into the already-loaded Rockbox image instead of
reloading `rockbox.ipod` from disk.

This is the most promising hook for Apple-like warm resume on the 5G.

## The Key Technical Question

Does 5G PMU standby preserve Rockbox DRAM well enough for a controlled warm
resume?

If the answer is "yes", then Apple-like behavior is probably achievable with a
Rockbox-owned resume protocol.

If the answer is "no", then the project becomes much larger:

- full storage-backed hibernation image
- bootloader restore logic
- stricter integrity checks
- more write amplification on iFlash

That second path is possible in theory but should not be the first attempt.

## Working Hypothesis

The most realistic first implementation is:

1. Keep the current fast suspend for immediate wake.
2. For longer idle, enter PMU standby with a Rockbox warm-resume token armed.
3. On wake, let the bootloader detect a valid retained Rockbox image in DRAM.
4. Resume the existing Rockbox image instead of cold-loading from storage.

This would be much closer to the stock user experience than either:

- current pure RAM suspend forever
- or a forced full shutdown

## What This Project Must Not Do

Do not:

- assume DRAM retention without proving it on real 5G hardware
- rely on a magic token alone without validating the resident image
- skip storage sleep/flush ordering
- weaken low-battery cutoffs to make wakeups "look better"
- write repeated hibernation images to storage as a first attempt
- replace the current safe path until the warm-resume path is proven

## Stage 1: DRAM Retention Probe

This is the first stage because it is the gating question.

### Purpose

Determine whether the 5G can enter PMU standby and later come back with enough
valid DRAM state intact for the bootloader to identify a Rockbox image and
resume it safely.

### Desired properties

- No change to default user behavior.
- No dependence on filesystem writes during wake.
- No replacement of the current safe suspend path.
- Easy fallback to a normal cold boot if anything looks wrong.

### Probe requirements

The probe needs to answer all of these:

1. Is the `Rockbox` signature still readable after standby?
2. Is the retained image stable enough to pass at least a minimal integrity
   check?
3. Does the wake source matter?
   - button wake
   - charger insert
   - USB insert
   - RTC alarm
4. Does retention time matter?
   - 1 minute
   - 10 minutes
   - 1 hour
   - overnight

### Safe implementation direction

The probe should be implemented as an experimental path, not the default path.

Recommended shape:

1. Add an experimental boot reason / warm-resume token structure.
2. Place it in a location that survives PMU standby if DRAM survives.
3. Include:
   - magic
   - version
   - checksum
   - expected image base
   - optional timestamp or sequence number
4. On wake, the bootloader checks the token and the Rockbox signature.
5. If either check fails, it performs a normal cold boot.

### Abort conditions

Stop Stage 1 immediately if any of these happen:

- wake causes a boot loop
- resumed image behaves nondeterministically
- token survives but image contents are inconsistent
- wake reliability differs across repeated identical tests

If any abort condition is hit, warm resume should remain experimental only.

## Stage 2: Bootloader Warm Resume

This stage only starts if Stage 1 shows stable DRAM retention.

### Bootloader changes

`bootloader/ipod.c` would need a new branch before the normal Rockbox load:

1. Check for a valid warm-resume token.
2. Check for the resident Rockbox signature at `DRAM_START + 0x20`.
3. Validate any additional integrity marker.
4. If valid, jump into the resident Rockbox image.
5. If invalid, clear the token and continue with the normal boot path.

### Safety requirements

- One failed resume attempt must never brick the boot path.
- Cold boot must always remain available.
- Holding the normal Apple-firmware key path must still bypass resume.
- USB and charger boot behavior must stay predictable.

## Stage 3: User-Facing Sleep Policy

Only after warm resume is proven should the UI grow a policy like:

- `Sleep Only`
- `Sleep, then Deep Sleep`
- `Full Shutdown`

Recommended behavior:

- short press or normal power-off request enters immediate sleep
- optional delayed transition to deeper PMU standby
- wake from sleep is instant
- wake from deep sleep is slower but resumes session

This is the closest Rockbox equivalent to what Apple firmware appears to do.

## Stage 4: RTC-Assisted Deep Sleep

Once bootloader warm resume is stable, the RTC alarm path can be evaluated for:

- timed wake
- scheduled maintenance wake
- controlled transition testing

This is optional. It is not required for the basic stock-like experience.

## Storage And Battery Considerations

### iFlash

For an iFlash build, the risk profile is better than with the original HDD:

- no spinning disk to restart
- lower idle draw
- less concern about physical spin-up timing

But do not confuse that with "no resume risk". A bad warm-resume jump can still
leave the UI, codec state, or storage stack in a bad state.

### 3000 mAh battery

The large battery helps by giving more suspend margin. It does not prove the
power-state logic is correct.

Do not change:

- `battery_level_disksafe`
- `battery_level_shutoff`

as part of warm-resume work. Battery model calibration is a separate project.

## Validation Matrix

Warm resume should not be considered successful until all of these pass on real
5G hardware:

1. Wake after 1 minute standby
2. Wake after 10 minutes standby
3. Wake after 1 hour standby
4. Wake after overnight standby
5. Wake while paused
6. Wake while audio was playing before suspend
7. Charger insertion after standby
8. USB insertion after standby
9. Repeated suspend/resume cycles without reboot
10. Recovery after one intentionally invalid token

## Recommendation

The right next move is not "implement deep hibernate immediately".

The right next move is:

1. Add a DRAM-retention probe design.
2. Keep it strictly experimental.
3. Prove retained-image warm resume on real hardware.
4. Only then promote it into a user-facing sleep mode.

If Stage 1 fails, the fallback plan should be:

- keep `Suspend Shutdown Timeout = Never` for instant wake behavior
- optionally add a much longer delayed shutdown policy
- do not attempt storage-backed hibernation unless there is a strong reason

## Suggested Follow-Up Tasks

1. Define a safe retained-memory token layout for `ipodvideo`.
2. Identify a memory location that is both preserved across standby and safe to
   inspect early in boot.
3. Add bootloader-side logging or diagnostic state for warm-resume attempts.
4. Add an experimental build flag or hidden setting so normal users do not hit
   this path accidentally.
5. Test repeated suspend/wake cycles on the real 5G before any default changes.

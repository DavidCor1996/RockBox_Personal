# iPod Video 5G iFlash and 3000 mAh Battery Polish Plan

## Goal

Improve day-to-day 5G behavior for:

- iFlash storage
- a 3000 mAh replacement battery

without changing shutdown semantics, bootloader behavior, or voltage safety rules.

## Current Facts

### Battery capacity

The iPod Video target already supports a user battery-capacity setting up to `3000` mAh in `firmware/export/config/ipodvideo.h`.

That setting affects runtime estimation in Rockbox. It does not change the low-voltage safety cutoffs.

So for an aftermarket 3000 mAh battery:

- user setting: safe and useful
- generic target default change: not safe to assume
- voltage-table rewrite: not justified without measurement

### iFlash storage

The ATA path already does the important safe thing before shutdown:

- `firmware/drivers/ata.c`
- `ata_sleepnow()` flushes cache
- then sends `STANDBY IMMEDIATE`

That means the right next step is not to make storage power-down more aggressive. The right next step is to validate how your specific iFlash stack is identified and timed.

## Safe Polish Priorities

### 1. Keep the battery-capacity path user-specific

Recommended:

- leave the generic `ipodvideo` default alone
- use `3000 mAh` on-device for your unit
- improve docs so that replacement-battery users know this setting matters

Not recommended yet:

- lowering `battery_level_disksafe`
- lowering `battery_level_shutoff`
- changing the `percent_to_volt_*` tables

Those are voltage-model changes, not capacity changes.

### 2. Validate iFlash identify and transfer behavior

The most useful real-device checks are:

- SSD detected or not
- logical sector size
- physical sector size
- sector multiplier
- DMA mode
- reported spinup time

Why this matters:

- the PP5020 ATA path treats SSD-like devices differently for write behavior
- iFlash adapters and card combinations do not all report themselves the same way
- changing defaults blindly is more likely to regress odd adapters than improve them

### 3. Prefer measurement over target-wide tuning

Low-risk improvements should start with:

- documenting the debug values to capture
- comparing idle, scan, and playback behavior on your actual iFlash setup
- only then considering targeted source tweaks

## Good Source-Level Candidates

These are the best next code tasks once measurements exist:

1. Improve ATA debug visibility for iFlash users.
   Make sure the debug screen clearly exposes the fields that matter for SSD detection and sector-multiplier behavior.

2. Audit SSD detection in the PP5020 ATA path.
   If your iFlash setup is misclassified, fix the identify-data interpretation before touching DMA policy.

3. Revisit current-draw documentation for the Video target.
   `firmware/export/config/ipodvideo.h` still carries `FIXME` current-draw estimates. Better notes or measured values would help runtime modeling more than guessing at voltage tables.

4. Add measurement guidance for upgraded-battery users.
   A short doc or tooling note is safer than changing target defaults for everyone.

## Changes To Avoid Blindly

Do not change these without real discharge or storage data:

- `battery_level_disksafe`
- `battery_level_shutoff`
- `percent_to_volt_*`
- ATA timing constants
- ATA sleep ordering
- virtual sector defaults
- generic battery-capacity defaults for all iPod Videos

## Proposed Order

### Phase 1

1. Remove suspend and warm-resume experiment drift from the source tree.
2. Keep the stable 5G behavior on-device.
3. Capture ATA debug information from your iFlash configuration.
4. Confirm the on-device battery-capacity setting is `3000 mAh`.

### Phase 2

1. Tighten docs around upgraded batteries and iFlash expectations.
2. Check whether your adapter/card mix is being treated as SSD-like.
3. If classification looks wrong, fix that path before any performance tuning.

### Phase 3

1. Consider targeted runtime-model improvements only if you collect real discharge data.
2. Consider ATA-path tuning only if the debug data shows a clear misconfiguration.

## Practical Recommendation

The next useful work is not another suspend feature.

The next useful work is:

1. keep the stable suspend behavior
2. clean the tree
3. measure the iFlash path
4. document the 3000 mAh setup
5. only then tune anything source-level

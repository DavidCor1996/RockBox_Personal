# iPod Nano 3G Current Steering

Date: 2026-05-25

This note supersedes the older ad hoc Nano 3G handoff experiments for the
current tethered Rockbox bring-up.

## Current Direction

- Keep this tethered through wInd3x DFU while testing.
- Do not write NOR.
- Do not replace or patch the Apple boot path as a recovery mechanism.
- Do not use stale exact-sector tables from earlier `rockbox.ipod` builds.
- Do not continue one-function-at-a-time `main()` stepping except as a last
  resort. The next useful work is loader correctness and a clean `crt0` handoff.

## Known Good

- wInd3x Nano 3G support is enough for temporary unsigned DFU execution.
- FAT/root discovery is working well enough to find `/rockbox.ipod`.
- The patched native Nano 3G Rockbox image builds:
  - file: `build-native-ipodnano3g/rockbox.ipod`
  - size: `854812`
  - sha256: `a4d8fc899cf49e35552c2303e7605d0bf54170b44e37309b01a2acdde6066f5d`
  - first 8 bytes: `054207646e6e3367`
- LCD status polling was a real hang source. Nano 3G LCD status waits are now
  no-ops in both bootloader and native builds.
- The native image can be loaded far enough to reach manual `crt0`/`main`
  probes, so the payload is structurally plausible.

## Critical Correction

The old exact maps are stale.

Earlier handoff logs still showed the old Rockbox image:

- length: `861084`
- header/checksum: `052C3706`
- old physical regions such as `b=248/249/253/434/663/2844`

The current mounted file is different:

- length: `854812`
- header first8: `054207646e6e3367`

The current file header was found on NAND at:

- bank: `0`
- page: `246819`
- block: `1928`
- page offset: `35`

Any loader that still reports the old length/checksum or old exact table is
testing stale data. Treat that result as invalid even if it reaches
`Rockbox loaded`.

## Handoff Contract

Rockbox native firmware expects the normal S5L8702 `crt0` contract:

- entry at `0x08000000`
- IRQ/FIQ disabled at entry
- caches/protection unit handled consistently
- vectors copied to IRAM
- BSS/IBSS zeroed
- IRAM sections copied
- stacks/modes initialized before `main`

Calling arbitrary mid-functions is useful for probes only. It is not a stable
boot strategy.

## Next Work

1. Regenerate the exact sector map from the current `854812` image.
2. Prefer local dump analysis over screen transcription wherever possible.
3. Use a small fresh dump around the current header page first:
   - bank `0`
   - pages around `246819`
4. If the local dump proves the first page only, expand with targeted searches
   for missing sectors rather than a full slow scan.
5. Build the next wrapper only after the current image map is confirmed.
6. Remove or compile out intentional stop/probe loops before any handoff test.

## Fresh Local Dump Result

Fresh DFU read of bank `0`, pages `246816..246824` confirms the current
on-device file layout:

```
page 246816 b=1928 po=32: fs=1577,1578,1579
page 246817 b=1928 po=33: fs=1608..1611
page 246818 b=1928 po=34: fs=1640..1643
page 246819 b=1928 po=35: fs=0..3
page 246820 b=1928 po=36: fs=32..35
page 246821 b=1928 po=37: fs=64..67
page 246822 b=1928 po=38: fs=96..99
page 246823 b=1928 po=39: fs=128..131
page 246824 b=1928 po=40: fs=160..163
```

This proves the current image is not stored in simple logical sector order.
The observed pattern is a lane/stride layout: sequential physical pages around
the header advance logical file sectors by `+32` while each page contains four
512-byte slots. A loader that maps `page + 1` to the next logical sector will
read `fs=32` when it wanted `fs=4`.

Immediate consequence:

- exact maps must be sector-indexed, not page-increment predicted
- early sectors `fs=4..31` must be found separately
- old loader output showing `b=248` is not reliable for current image mapping

Additional checks:

- A full first8 scan for `fs=4` across pages `0..1048575` of banks `0..3`
  found no hit.
- A targeted check near the same header page plus larger page-space offsets also
  found no hit.
- Reading page `246819` via the normal BootROM helper at offset `0x800` does
  not expose `fs=4..7`; raw FIFO reads are currently not trustworthy because
  they return repeated ASCII `0x30` bytes even at offset `0`.

So the immediate blocker is now:

- either the current mounted file contents are not the same as the local
  `build-native-ipodnano3g/rockbox.ipod`, despite the header match
- or the current wInd3x Nano 3G NAND helper only exposes part of the physical
  storage layout and needs a corrected full-page/plane read path

Before another handoff attempt, verify the mounted `/rockbox.ipod` hash in disk
mode and fix the raw/full-page reader if the file hash still matches.

## Disk Mode Verification

Disk mode verification after syncing files:

```
build-native-ipodnano3g/rockbox.ipod
  sha256 a4d8fc899cf49e35552c2303e7605d0bf54170b44e37309b01a2acdde6066f5d

/rockbox.ipod
  sha256 a4d8fc899cf49e35552c2303e7605d0bf54170b44e37309b01a2acdde6066f5d
  size   854812

/.rockbox/rockbox.ipod
  sha256 a4d8fc899cf49e35552c2303e7605d0bf54170b44e37309b01a2acdde6066f5d
  size   854812
```

`/.rockbox/rockbox.ipod` was stale before this check and has been replaced with
the current build. A backup was left at:

```
/.rockbox/rockbox.ipod.pre-current-sync-20260525
```

`filefrag` reports both current files are contiguous:

```
/rockbox.ipod:
  4096-byte blocks 911886..912094
  512-byte partition-relative start 006F5070
  512-byte absolute host LBA 006FF0EE
  FAT first cluster 000DE2B2

/.rockbox/rockbox.ipod:
  4096-byte blocks 912304..912512
  512-byte partition-relative start 006F5D80
  512-byte absolute host LBA 006FFDFE
  FAT first cluster 000DE454
```

This is a major simplification. The current file is contiguous at the FAT/logical
storage layer, so a correct logical LBA reader should not need a hard-coded
per-sector physical exact map for the whole image.

If a later loader still reports:

```
flba=006E8136
```

that loader is using stale FAT/cluster information or a stale cached table. For
the current `/rockbox.ipod`, the expected FAT-relative start is `006F5070` and
the expected absolute host LBA is `006FF0EE` in 512-byte units.

## Do Not Do

- Do not branch through stale exact maps.
- Do not bypass image checksum and then trust the result.
- Do not patch storage/mount calls in Rockbox until the loaded image is proven
  to match the current file.
- Do not infer success from `Rockbox loaded` alone; require current header,
  current length, matching body checksum, then handoff.

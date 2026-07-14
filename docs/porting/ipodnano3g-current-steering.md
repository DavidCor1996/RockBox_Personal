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

## 2026-07-12 Native Handoff Checkpoint

The transient IRAM0 bootloader plus embedded native application chain now has
a confirmed hardware result.

- bootloader body: `30528` bytes
- native application body: `75208` bytes
- combined IMG1 body: `105736` bytes
- DFU image: `107792` bytes
- DFU sha256:
  `61a14abfb266b593ce68ca635e5b8660bded6d8f0587546e3dc598fb386c34b3`
- transport: upstream `wInd3x run`, temporary haxed DFU only
- persistent writes: none; no NAND or NOR path was entered

The bootloader copied the embedded native body from the original IRAM0 DFU
staging copy to `0x08000000`, verified the Rockbox model-seeded byte checksum,
cleaned/discarded caches, and branched to native Rockbox `crt0`.

Observed final screen: three stable red/green/blue horizontal bands. This is
the native application's explicit checkpoint after successful `crt0`,
`system_init()`, `kernel_init()`, I2C setup, LCD initialization, backlight
initialization, and framebuffer update. The automatic DRAM-persistence reboot
probe was disabled for this run, so the RGB screen remains held.

This closes the loader-correctness and clean-`crt0`-handoff blocker. The next
checkpoint should advance from the proven native pre-storage path; do not
return to stale exact-sector chainload experiments.

### Native IRQ and input result

A follow-up transient native probe enabled IRQ/FIQ, completed a one-second
kernel `sleep(HZ)`, initialized the S5L8702 click-wheel controller, and held a
cyan screen with a black/white half-second heartbeat. Hardware observation:

- heartbeat blink: confirmed
- Menu: red
- Right/forward button: green
- Play: magenta/purple
- Left/previous button: blue
- Select/center: white

This confirms the kernel tick interrupt, wheel-controller initialization, and
all five click-wheel buttons in native Rockbox. Rotation did not change the
first probe screen because rotation is posted to the button queue while that
probe only watched the direct button bitmask; it is not evidence of a failed
wheel sensor. The prepared follow-up reads the raw `new_wheel_value` updated by
the same ISR and maps clockwise/counterclockwise motion to yellow/orange.

The raw-wheel follow-up was also confirmed on hardware: clockwise motion
produced yellow and counterclockwise motion produced orange. Together with the
button result, this closes the native click-wheel input checkpoint.

### Native storage and filesystem result

All storage work in these transient probes remains read-only. Nano 3G NAND
program, erase, and sector-write entry points remain hard failure stubs.

The first native storage probe entered `storage_init()` at yellow and returned
success at a stable green screen with a flashing white bottom heartbeat. A
follow-up ran `filesystem_init()` and `disk_mount_all()` and reached cyan with
the same heartbeat, proving that the synthesized MBR/BPB path mounts a FAT
volume. The FTL map/context discovery used by these application probes is
bounded to the selected two-block map cluster and pages 0..7 of the known
context blocks; it no longer performs the old broad scans during startup.

A subsequent probe attempted to read the first eight bytes of both
`/rockbox.ipod` and `/.rockbox/rockbox.ipod`. The observed screen was visually
ambiguous between red and orange but retained its white heartbeat. Because the
immediately preceding identical storage/mount path had completed successfully,
the coherent interpretation is orange: the FAT volume mounted, but neither
firmware path was readable. Do not use color-only outcomes for the next
storage diagnostic.

Two read-only host OOB scans were then run through a temporary local wInd3x
extension. A scan of virtual block `0x025a` found no current root-range logical
OOB entries. A scan of physical page offset zero across all 8192 blocks and all
four banks likewise found no entries in logical range `0x1b000..0x1bfff`.
These negative results reject the old naive root candidate and the assumption
that the current root mapping must appear at physical page offset zero; they do
not exclude a later page in a scattered/log block.

The next transient image replaces colors with two columns of white blocks on
black. The left column probes host FAT LBA `0x13f`; the right probes root LBA
`0x3b2f`. Rows 1..6 encode success, no covering map entry, NAND read failure,
wrong OOB type, wrong OOB LPN, and other failure. Row 7 additionally marks
plausible sector content. The image is read-only and contains:

- bootloader body: `25280` bytes
- native application body: `100512` bytes
- combined IMG1 body: `125792` bytes (within the 128 KiB IRAM0 window)
- DFU image: `127840` bytes
- DFU sha256:
  `6e804a203b16df2096fd025f7151c0d63ee9a574e349000bd33844aa971e166e`
- artifact: `/tmp/nano3g-native-fat-root-probe-20260712.dfu`

Hardware reported one white rectangle in row 3 of each column, at the same
height. Therefore both target LBAs had covering map entries, but the normal
FMC path failed when it tried to read each translated physical NAND page. This
rules out missing map coverage as the immediate file-open blocker. The next
probe retries only those two selected pages through the already-proven local
read helper, which reinitializes the target NAND bank; it does not widen the
scan or write persistent storage.

Prepared local-read retry image:

- bootloader body: `25280` bytes
- native application body: `100584` bytes
- combined IMG1 body: `125864` bytes
- DFU image: `127920` bytes
- DFU sha256:
  `70812ddc95683cdffc0eee4a2494091a18541199ec960c7c7096981f987a42b7`
- artifact: `/tmp/nano3g-native-fat-root-local-read-probe-20260712.dfu`

The local-bank retry produced the same hardware result: row 3 for both FAT and
root. Per-bank BootROM reinitialization therefore does not repair either
translated page. A follow-up now renders the selected direct-read traces as
hexadecimal digits, with FAT above the horizontal divider and root below. For
each half, line 1 is `map-index vblock status`, line 2 is `lpn0`, and line 3 is
`page-offset bank physical-block physical-page-offset`.

Prepared hex-trace image:

- bootloader body: `25280` bytes
- native application body: `101232` bytes
- combined IMG1 body: `126512` bytes
- DFU image: `128560` bytes
- DFU sha256:
  `75e7d9e2c87b3d2e049b016432b287a1daded650f7705f5c76b91870589beb75`
- artifact: `/tmp/nano3g-native-fat-root-hex-trace-20260712.dfu`

Hardware transcription of that screen:

```text
FAT:  j=ffff v=ffff status=2 l0=ffffffff po=fff
root: j=0294 v=00da status=2 l0=00003201 po=24b
```

Status 2 is definitive `nocover`. The earlier row-only result was therefore a
visual row-count ambiguity, not a physical NAND read failure. The root target
also shows that the mounted direct view currently has sector base zero: for
host `0x3b2f`, `(0x3b2f - 0x3201) / 4 == 0x24b`. This means the raw-disk BPB
addresses used by that probe are not the LBAs requested after the fallback
synthesized MBR advertises partition start `0xa07e`.

The next probe instruments the actual opens and first eight-byte reads of
`/rockbox.ipod` and `/.rockbox/rockbox.ipod`. Each half reports file result
(`1` open failure, `2` read failure, `3` success), first FTL failure status,
host LBA, map index/vblock, l0, and page offset. This avoids any inferred LBA.

Prepared file-read trace image:

- bootloader body: `25280` bytes
- native application body: `101136` bytes
- combined IMG1 body: `126416` bytes
- DFU image: `128464` bytes
- DFU sha256:
  `2db4f3731ec17af2ab7c956d6fafa855d62c236fcabeb912798c41aac1efae7b`
- artifact: `/tmp/nano3g-native-file-read-hex-trace-20260712.dfu`

Hardware transcription:

```text
/rockbox.ipod:          file=2 status=5 lba=00077db6
                        j=0135 v=0076 l0=00077c01 po=06d
/.rockbox/rockbox.ipod: file=1 status=0 (no FTL failure recorded)
```

Thus the synthesized root successfully opens `/rockbox.ipod`, but its first
eight-byte data read fails at host LBA `0x77db6`. The map entry covers the LBA,
but the decoded page has an unexpected OOB LPN. The `.rockbox` path is absent
from the current synthesized root and fails before any FTL read error.

For `v=0x76`, the unremapped physical block is
`0x76 + syshyperblocks(0x1a9) = 0x21f`. Page offset `0x6d` selects bank 1,
physical page offset `0x1b`. Because page zero used to establish `l0` is on
bank 0, a bank-1-only VFL block remap coherently explains the mismatch. The
next probe checks only the final 23 physical replacement blocks
`0x1fe9..0x1fff` at bank 1/page `0x1b`, caches an exact OOB-LPN match, and
retries the same file read. No program or erase command is reachable.

Prepared bounded-remap image:

- bootloader body: `25280` bytes
- native application body: `101700` bytes
- combined IMG1 body: `126980` bytes
- DFU image: `129040` bytes (12 bytes of IMG1 alignment padding)
- DFU sha256:
  `fbcdde7eab5c3bbe4d7a1e4a9ccb79e13fa9aa501ba74dc63e6bcfe0e043996b`
- artifact: `/tmp/nano3g-native-file-remap-probe-20260712.dfu`

The bounded replacement scan did not find an exact OOB match. Hardware stayed
at file result 2/status 5 and reported physical block `0x021f`; all other trace
fields were unchanged. Therefore the current `v=0x76` map value is not fixed by
the simple final-23-block VFL replacement model.

The exact target is now sufficiently constrained for a host-side scan. Host
LBA/internal 512-LPN `0x77db6` can occupy any of four slices, so the page OOB
raw LPN must be one of `0x0efb66`, `0x0efb68`, `0x0efb6a`, or `0x0efb6c`.
The failed decode selected bank 1 and physical page offset `0x1b`. Scan that
single bank/page offset across physical blocks before widening either axis.

That read-only host scan completed across all 8192 physical blocks at bank 1,
page offset `0x1b`, with zero read failures and zero OOB-LPN matches. The
manifest is
`tmp/n3g-live-file-lba77db6-po1b-scan/manifest.txt`. This rules out both the
unremapped block and every possible same-bank/same-page-offset replacement;
the remaining error is in the logical-page to NAND bank/page-offset decode (or
in a missing scattered/log-block mapping), not a simple physical-block remap.

Critical host-tool correction: that negative scan, and the two preceding
`readextra-pages` scans, are invalid. The temporary wInd3x extension asked the
BootROM to place page data at `0x22000100` and OOB at `0x22000000`, but returned
`0x22000100 + 0x800` for OOB reads. It therefore scanned unrelated SRAM, which
was visibly identical for every requested bank/page. The local extension now
returns `0x22000000` for offset `0x800`; none of the old zero-hit manifests may
be used as NAND evidence.

The corrected OOB reader immediately resolves the `v=0x76` example. At
physical block `0x21f`, bank 0, physical page offset `0x6d`, the OOB raw LPN is
`0x000efb6a`; `raw >> 1 == 0x77db5`, so its second 512-byte slice covers the
requested `0x77db6`. This confirms that a direct-map delta already expressed
in 512-byte sectors must be passed to `ftl_n3g_decode_page()` as
`delta & ~3`, not divided by four a second time. The old decode selected bank
1/page `0x1b`; the corrected decode selects bank 0/page `0x6d`.

The body at that corrected page does not contain a Rockbox header. That is
expected because the native probe's synthesized root entry was still the stale
cluster `0xd44b`/size `861092` entry. The disk-mode source of truth is cluster
`0xde2b2`, size `854812`, first host LBA `0x006ff0ee`. The next native probe
updates only the synthetic diagnostic directory/FAT entry to that documented
current placement, applies the double-division fix, and displays the actual
eight bytes read plus the resolved physical page.

Prepared current-file mapping probe:

- bootloader body: `25280` bytes
- native application body: `101936` bytes
- combined IMG1 body: `127216` bytes
- remaining IRAM0 body margin: `3856` bytes
- application model-seeded byte checksum: `0x00a3c1ff`
- DFU image: `129264` bytes
- DFU sha256:
  `7eb1e5008dccffd61e495394ee0416262dbe1a6a3b533341e7779169fbab9087`
- artifact: `/tmp/nano3g-native-current-file-map-probe-20260712.dfu`

The display keeps the four-line root trace in the top half. The lower half
shows the actual first eight file bytes as two eight-digit rows, then physical
page offset, bank, and the `.rockbox` file result/status. A successful current
header should begin `05420764 6E6E3367` for the documented 854812-byte image.

Hardware result:

```text
/rockbox.ipod: file=2 status=2 pb=ffff lba=006ff0ee
               j=0765 v=0488 l0=000f1e01 po=2ec
header:        ffffffff ffffffff
physical:      pp=fff bank=f
/.rockbox:     file=1 status=0
```

The current documented cluster/LBA is now requested exactly, but no loaded
base-map or parsed scattered-log entry covers it. `j=0x765` is merely the
nearest lower readable base entry retained for diagnostics; its displayed
`po=0x2ec` is the low three hex digits of a much larger non-covering delta.
There was no NAND-page attempt (`pb/pp/bank` all erased sentinels).

For LBA `0x006ff0ee`, the logical 2 KiB page is `0x001bfc3b`, logical block
`0x0dfe`, page `0x03b`. The current native context search is restricted to
pages 0..7 of four hard-coded context blocks, whereas the original Whimory
mount searches backward through the whole 512-page control hyperblock for the
newest type-`0x43` context. The next host-side step is therefore a corrected
OOB scan of every page/bank in those context blocks, followed by a body read of
the newest type-`0x43` page and its `ftl_map_pages[]`/scattered-log state.

The unrestricted corrected OOB scan supersedes that hard-coded-context
assumption. It found three dense type-`0x44` map generations at bank 0:

- physical block `0x14c2`, page 0, USN `0xfffc8ebe`
- physical block `0x1816`, page 0, USN `0xfffc8ed8`
- physical block `0x1b04`, page 0, USN `0xfffc8ef2`

The USN counts down, so the first entry is the newest of those three. Their
control/map-pair separation is consistently 4096 blocks. Scanning the paired
control blocks exposed a still newer interleaved generation with USN
`0xfffc8e3c`. Its pages are:

- type `0x43` context: block `0x04c3`, page `0x13`
- type `0x46`: block `0x04c3`, page `0x10`
- type `0x49` overlay: block `0x14c2`, page `0x10`
- type `0x44` dense map: block `0x14c3`, page `0x10`
- type `0x45` tables: blocks `0x04c2`, `0x04c3`, `0x14c2`, and
  `0x14c3`, at the indexed pages recorded in the scan manifests

The complete corrected scans are in
`tmp/n3g-live-meta-1218-1219/manifest.txt` and
`tmp/n3g-live-meta-5314-5315/manifest.txt`. Captured bodies include:

- `tmp/n3g-current-context-b1219-p19.bin`, sha256
  `9cfd0d7c5f22eca0aadf3b1a82cfe0c7454fadf0ba01c47e97cca4320763c0ab`
- `tmp/n3g-current-map-b5315-p16.bin`, sha256
  `9db204d3c7fe158288972cbc55a4991984f913d5d293d67b240c7c8ffc74ea69`
- `tmp/n3g-current-overlay-b5314-p16-t49.bin`, sha256
  `a3ebf9a65a768c2c4c0ef05a550c7b5a8c834c9725627d72c0aabcbb56fc2df7`

Nano 3G's context log-entry array begins at body offset `0x1a4`, not at the
Nano 2G structure's `0x130`. Parsing at `0x1a4` yields 17 entries in the newest
context and active logical blocks including `0x50`, `0x52`, `0x53`, `0x723`,
`0x745`, and `0x746`. The corresponding type-`0x45` page tables have counts
consistent with these entries. The native context parser still needs this
format correction before scattered-log replay can be considered valid.

Capturing OOB for all entries in the newest dense map produced 1010 readable
entries. Its base-map LPN range is `0x800..0xf1e00`, so it cannot directly
cover host value `0x006ff0ee`. However, the documented disk-mode file address
divided by eight is `0x000dfe1d`, and that value is covered exactly:

```text
j=007b v=0722 l0=000dfe01 delta=01c
j=0330 v=0721 l0=000dfe00 delta=01d
```

The first candidate decodes to bank 0, physical block `0x08cb`, page `0x07`.
Its corrected OOB raw LPN is `0x001bfc3a`, for
`raw >> 1 == 0x000dfe1d`. This is direct physical evidence that the file
address used by the mounted FAT view has an additional eight-to-one,
4096-byte volume-sector scale. Host BootROM body reads at both candidates
returned erased bytes, but that path has not yet been shown equivalent to the
already working native local-bank read.

The next transient probe therefore applies `host_lpn >> 3` only to the eight
sectors beginning at the known file LBA and performs the selected page read
through the native local NAND path. It is diagnostic-only and keeps all NAND
write entry points disabled.

Prepared and uploaded native divided-LBA local-read probe:

- bootloader body: `25280` bytes
- native application body: `101952` bytes
- combined IMG1 body: `127232` bytes
- remaining IRAM0 body margin: `3840` bytes
- application model-seeded byte checksum: `0x00a3cafa`
- DFU image: `129280` bytes
- DFU sha256:
  `370f013ef905a21d88923ce5cf2036e076cb871dbf79224bf81fe3776de2afd4`
- artifact: `/tmp/nano3g-native-div8-localread-probe-20260713.dfu`

The screen result is pending transcription. Its lower two rows should be
`05420764` and `6e6e3367` if the native page and within-page selection both
match the disk-mode firmware header.

Hardware result:

```text
/rockbox.ipod: file=3 status=1 pb=08cb lba=006ff0ee
               j=007b v=0722 l0=000dfe01 po=01c
header:        ffffffff ffffffff
physical:      pp=007 bank=0
/.rockbox:     file=1 status=0
```

The OOB type/LPN checks and FAT file read now succeed, proving the `/8`
address scale, `j=0x7b` map selection, bank 0, block `0x08cb`, page `0x07`,
and file-sector slot. The all-FF body does not yet prove an erased page:
`nand_read_page()` uses the BootROM-transfer implementation and reports
success for this body, so its error-only local-path fallback did not run. This
is the same transfer family that returned all FF in the host probe while
returning coherent OOB.

The next image forces only logical page `0x000dfe1d` through
`nano3g_nand_diag_local_read()` even when the BootROM-transfer call reports
success. All other behavior remains unchanged and read-only.

Prepared forced-local-read probe:

- bootloader body: `25280` bytes
- native application body: `101984` bytes
- combined IMG1 body: `127264` bytes
- remaining IRAM0 body margin: `3808` bytes
- application model-seeded byte checksum: `0x00a3de74`
- DFU image: `129312` bytes
- DFU sha256:
  `17c1c7220cf99c87104570ada983d6e0e1a85ed987fbe1e2ccb438d1f71d53d9`
- artifact: `/tmp/nano3g-native-div8-forced-localread-probe-20260713.dfu`

The forced-local screen was identical to the preceding result, including
`file=3`, the exact `j=0x7b`/`v=0x722` mapping and OOB checks, physical
block/page `0x08cb/0x07`, and two all-FF body rows. This is expected from a
code-path audit: `nano3g_nand_diag_local_read()` delegates an offset-zero
full-page request to `n3g_read_page_bootrom_xfer_bank()`, so the nominally
forced path was not actually an independent body-transfer implementation.

Before changing the controller sequence, the next read-only host check should
re-read the independently known current header location (bank 0, physical
block 1928, page 35; absolute physical page 246819) with the corrected wInd3x
body/OOB helper. Comparing its body and current corrected OOB with block
`0x08cb`/page `0x07` will distinguish a controller-transfer problem from a
stale dense-map/base-page selection. The existing
`tmp/n3g-known-header-p246819.oob` cannot answer this: it predates the
`readextra-pages` return-offset correction and contains the invalid SRAM
window.

The corrected comparison shows that the May physical header location is now
stale: bank 0, block 1928, page 35 returns an all-FF 2048-byte body and erased
OOB. The file has moved since that earlier capture; this is not evidence that
the current body reader is globally broken.

A complete read-only body dump of current mapped physical block `0x08cb` on
bank 0 is saved at
`tmp/n3g-header-compare-20260713/pb2251-bank0.body`. It contains many non-erased
pages, but page 7 is all FF and the current firmware header does not occur
anywhere in the block. Corrected OOB samples show a coherent user-page
sequence with USN `0x0000f795`:

```text
bank 0 block 08cb page 00 raw=001bfc02 type=40
bank 0 block 08cb page 07 raw=001bfc3a type=40
bank 0 block 08cb page 1b raw=001bfcda type=40
bank 0 block 08cb page 1c raw=001bfce2 type=40
```

The crucial correction is that shifting the OOB word right by one discards a
live lane bit. The current file's first host sector translates without that
loss as:

```text
host=006ff0ee
raw page key = host >> 2 = 001bfc3b
512-byte slot = host & 3 = 2
```

The selected bank-0 page is raw `0x001bfc3a`, exactly the neighbouring key.
Map entry `j=0x07b`, `v=0x0722` begins at bank-0 raw `0x001bfc02`; the full
target selects its next interleaved lane, bank 1, physical block `0x08cb`,
page 7, virtual page offset `0x1d`, and slot 2. Host BootROM reads of banks
1..3 currently return zero body/OOB, so the next probe performs this selection
inside the initialized native environment and displays the actual slot even
if its standalone OOB view is unavailable.

Prepared and uploaded current raw-lane bank-1 probe:

- bootloader body: `25280` bytes
- native application body: `102344` bytes
- combined IMG1 body: `127624` bytes
- remaining IRAM0 body margin: `3448` bytes
- application model-seeded byte checksum: `0x00a48e0e`
- DFU image: `129680` bytes
- DFU sha256:
  `e497d2ebfd941616437bea948461a7aaea3ca43f9122a64e0b0b558c7f6fa234`
- artifact: `/tmp/nano3g-native-current-raw-lane-bank1-probe-20260713.dfu`

Hardware result:

```text
/rockbox.ipod: file=3 status=1 pb=08cb lba=006ff0ee
               j=007b v=0722 rawbase=001bfc02 po=01d
header:        ffffffff ffffffff
physical:      pp=007 bank=1 oob-valid=1
/.rockbox:     file=1 status=0
```

The dedicated bank-1 destination buffer was prefilled with `0xff` and remained
unchanged, so the discarded low raw-key bit is not a readable second NAND bank
through the current BootROM/native transfer primitive.  The next read-only
test must preserve bank 0 and inspect whether that bit selects a second 2-KiB
data half (or plane) that the existing host command never retrieves.

### Direct no-PMU bank-reset discriminator

A direct pre-storage no-PMU survey initially displayed:

```text
0 0 0 FF 004
001316FF
EE48F700
1 1 0 FF 000
FFFFFFFF
FFFFFFFF
2 1 0 FF 004
000316FF
EE48F700
```

The two nominal target reads returned the same four-word controller residue,
while the bank-1 page-zero control was erased.  This is not evidence for or
against a second lane: the diagnostic helper bypassed `storage_init()` and,
unlike the normal NAND path, had not run `n3g_rom_init_bank()` before the local
FMC transfer.  The identical `0xEE48F700` residue exposed that missing bounded
bank reset rather than NAND page contents.

`nano3g_nand_diag_bank_read()` now performs the local bank reset immediately
before its two read attempts.  It still makes no PMU calls and remains strictly
read-only.  The revised survey samples a known-live bank-0 control (block
`0x08cb`, page 0), the bank-0 target reference (block `0x08cb`, page 7), and the
same target through bank 1.  The first control should identify a functioning
local transfer with OOB `0x001bfc02`, type `0x40`, and body beginning with CPU
word `0xEDAEF4C4`.

Prepared initialized no-PMU survey:

- bootloader body: `25280` bytes
- native application body: `72572` bytes
- combined IMG1 body: `97852` bytes
- application model-seeded byte checksum: `0x0073d2dc`
- combined IMG1 sha256:
  `d896e278d186b13423adb4799b7d8c3ff209221868a3bcfec54cf9a1c73c728d`
- DFU image: `99904` bytes
- DFU sha256:
  `521465953f85b52cfbf5f838ac6332be12b2f66891fc2b0f3dc14ec5aae7f376`
- artifact:
  `/tmp/nano3g-native-no-pmu-reset-bank-survey-20260713.dfu`

Hardware result (the hand-drawn hexadecimal `B`/`A` glyphs were transcribed as
`6`/`R`; the values below are normalized against the exact displayed glyph
set and the independently captured bank-0 page):

```text
sample 0: bank=0 rc=0 type=40 nontrivial=111
          oob=001bfc02 first=edaef4c4
sample 1: bank=0 rc=0 type=40 nontrivial=000
          oob=001bfc3a first=ffffffff
sample 2: bank=1 rc=0 type=40 nontrivial=000
          oob=001bfc3b first=ffffffff
```

The control is an exact match for the host capture: `0x111` nontrivial words
and first CPU word `0xEDAEF4C4`.  The reset therefore fixes the local FMC data
path, and chip-select bank 1 returns the exact odd raw key requested.  The two
target bodies are all FF.  This does not yet distinguish a genuinely erased
bank-1 page from an FMC path that transfers bank-1 OOB but not bank-1 body,
because no independently live bank-1 body has been observed.

The next survey scans all 128 pages of physical block `0x08cb` on bank 1 and
reports the first non-erased body, while retaining the known-live bank-0 page
and exact bank-1 target as controls.  Its top rows are `sample bank rc page
type count`; if no bank-1 body is found, the scan row uses page `ff` and its
count is the number of coherent type-`0x40/0x41` OOB pages.

Prepared bank-1 body discriminator:

- bootloader body: `25280` bytes
- native application body: `72772` bytes
- combined IMG1 body: `98052` bytes
- application model-seeded byte checksum: `0x00741c6c`
- combined IMG1 sha256:
  `6e5c8ed8bde110438b241a2a9024dc40f33fdef9138590d7929b95d79a47bd83`
- DFU image: `100112` bytes
- DFU sha256:
  `7a286917585e2e7f4f38d99afd2f7ff06253074ba7e2bf4e631bfb6e5a69abb7`
- artifact: `/tmp/nano3g-native-bank1-body-scan-20260713.dfu`

Hardware result:

```text
sample 0: bank=0 rc=0 page=00 type=40 nontrivial=111
          oob=001bfc02 first=edaef4c4
sample 1: bank=1 rc=0 page=00 type=40 nontrivial=0f1
          oob=001bfc03 first=f6d8ffff
sample 2: bank=1 rc=0 page=07 type=40 nontrivial=000
          oob=001bfc3b first=ffffffff
```

Bank 1 has a healthy body path: page 0 returns coherent adjacent OOB
`0x001bfc03` and 241 nontrivial words.  The exact bank-1 target page is
therefore genuinely all FF, not an FMC chip-select transfer failure.  The
retained disk-mode extent/LBA `0x006ff0ee` no longer names the current file
body.  This agrees with the independently observed disappearance of the May
header at physical block 1928/page 35.  Do not continue deriving physical
addresses from that stale extent.

The next source-of-truth step is a fresh, read-only Apple Disk Mode capture of
the live partition, current `/rockbox.ipod` and `/.rockbox/rockbox.ipod`
hashes, FAT directory clusters, and extents.  Only after that capture should a
new NAND target be selected.

### Live Disk Mode geometry correction

The 2026-07-13 capture was taken after remounting the automatically mounted
volume read-only.  It is saved in `tmp/n3g-diskmode-live-20260713/`.

Live geometry and file state:

```text
whole-device logical/physical sector: 4096 / 4096 bytes
MBR partition 0: type=0b start=0000003f size=000e7f81
BPB: bps=4096 spc=1 reserved=32 fats=2 fatsz=927 root=2
/rockbox.ipod:          cluster=000de2b2 size=854812 extent=911886..912094
/.rockbox/rockbox.ipod:                  size=854812 extent=912304..912512
both sha256: a4d8fc899cf49e35552c2303e7605d0bf54170b44e37309b01a2acdde6066f5d
```

The file and its extent did not move.  The old conversion to absolute
512-byte LBA `0x006ff0ee` was wrong: it added stale value `0x0000a07e` as a
512-byte partition start even though the live device exposes 4096-byte sectors
and the MBR start is `0x3f` of those sectors.  The corrected first-file address
is:

```text
partition-relative 4096-byte sector = 911886 = 0x000dea0e
absolute 4096-byte sector           = 0x000dea0e + 0x3f = 0x000dea4d
absolute 512-byte LBA               = 0x000dea4d * 8 = 0x006f5268
raw 2-KiB page key                  = 0x006f5268 >> 2 = 0x001bd49a
512-byte slot                       = 0
```

The captured current dense map covers the corrected key through entry
`j=0x269`, `v=0x027d`.  Its page-zero OOB is `0x001bd400`, so
`l0=0x000dea00` and the corrected 4096-byte key has delta `0x4d`.  The primary
physical block is `0x027d + 0x01a9 = 0x0426`; its second-plane candidate is
`0x1426`.  A new no-PMU read-only survey searches every bank/page of only those
two blocks for exact OOB key `0x001bd49a` and reports its body.  The expected
CPU word for the file's first four bytes is `0x64074205`.

Prepared corrected-geometry raw-key survey:

- bootloader body: `25280` bytes
- native application body: `72820` bytes
- combined IMG1 body: `98100` bytes
- application model-seeded byte checksum: `0x00744e7c`
- combined IMG1 sha256:
  `94ea50f9dfcde564f97fef96e1202a40faa01fbf398e0bc895b7cd7293169fef`
- DFU image: `100160` bytes
- DFU sha256:
  `a7c54c552a161d2eed59ee2e69384442550d75d98931753fd8a8c132ced88f83`
- artifact: `/tmp/nano3g-native-corrected-4k-raw-scan-20260713.dfu`

Hardware result (normalized from the compact hand-drawn display):

```text
sample 0: bank=0 rc=0 page=00 type=40 nontrivial=111
          oob=001bfc02 first=edaef4c4
sample 1: no exact key in block 0426; coherent user pages=100
          requested=001bd49a last-bank-rc=d
sample 2: no exact key in block 1426; coherent user pages=100
          requested=001bd49a last-bank-rc=d
```

Both physical blocks expose `0x100` valid user pages across the working bank
0/1 paths, but neither contains exact raw key `0x001bd49a`.  Controller banks
2 and 3 finish with `rc=-3` (displayed as low nibble `d`), so treating the
missing raw-key `+2` lane as another controller bank is wrong.  The already
observed physical-block bit 12 changes the raw lane by `+4`; physical-block
bit 11 is therefore the bounded candidate for the missing `+2` lane.

The next read-only native probe directly samples bank 0, page `0x13` from
physical blocks `0x0c26` (bit 11) and `0x1c26` (bits 11 and 12), retaining the
known-live block `0x08cb`, page 0 control.  An exact hit should report OOB
`0x001bd49a` and begin with CPU word `0x64074205`.

Prepared physical-bit-11 plane probe:

- bootloader body: `25280` bytes
- native application body: `72596` bytes
- combined IMG1 body: `97876` bytes
- remaining IRAM0 body margin: `33196` bytes
- application model-seeded byte checksum: `0x0073f18d`
- application sha256:
  `baaee7a77b3372d191159624dfe6c21d70710bf4e054b7ae6fee237f29223725`
- combined IMG1 sha256:
  `f47dd858e17cc746c695df2fc9cea7f52347e92e22441f1ae6ea647d185802ff`
- DFU image: `99936` bytes
- DFU sha256:
  `f0b617afe6069c731b95ae85efbd7bcdceab11c872dbb35b3d061b0482819bfe`
- artifact: `/tmp/nano3g-native-bit11-plane-probe-20260713.dfu`

The bootloader/app concatenation, model checksum, complete DFU payload, and
128-KiB staging bound were verified byte-for-byte before hardware use.

Hardware result:

```text
sample 0: bank=0 rc=0 page=00 type=40 nontrivial=111
          oob=001bfc02 first=edaef4c4
sample 1: bank=0 rc=0 page=13 type=40 nontrivial=200
          oob=000a1098 first=07651bc2
sample 2: bank=0 rc=0 page=13 type=40 nontrivial=200
          oob=000a109c first=9641a76c
```

Physical block bit 11 does not select the missing lane.  Blocks `0x0c26` and
`0x1c26` are both healthy, but contain a different logical region; bit 12
again changes that region's raw lane by four.

The captured page-zero OOB table provides the missing discriminator.  Across
all 1010 valid current map entries, physical-block parity predicts the page-0
raw residue exactly: even decoded physical blocks have raw residue zero and
odd decoded physical blocks have raw residue two.  There are no exceptions.
Together with the already proven bit-12 `+4` plane, the physical hyperblock
lanes are therefore split across the adjacent block (`pblock ^ 1`) rather
than block bit 11.  For the even base block `0x0426`, raw delta `0x9a` selects
adjacent odd block `0x0427`, page `0x13`; controller bank 0 should have raw
`0x001bd49a` and bank 1 should have `0x001bd49b`.

The next transient read-only probe samples those two exact pages and retains
the known-live block `0x08cb` control.  A successful first half should begin
with CPU word `0x64074205`, the current firmware header bytes.

Prepared adjacent-odd-block probe:

- bootloader body: `25280` bytes
- native application body: `72604` bytes
- combined IMG1 body: `97884` bytes
- remaining IRAM0 body margin: `33188` bytes
- application model-seeded byte checksum: `0x00740098`
- application sha256:
  `e4a0e6d692c6f0816716cbd1d606dba59fb88c1a9109417c1692b6e090052ec4`
- combined IMG1 sha256:
  `5b4dd1e5c4dface53dc531a8fc541b4562b34fa1e7a6939fe19f4859743a88aa`
- DFU image: `99936` bytes
- DFU sha256:
  `c058f525d21d3d9e30bb11e31339399dfdd98d7548b4c1db4c672862ec91236f`
- artifact:
  `/tmp/nano3g-native-adjacent-odd-block-probe-20260713.dfu`

The chained body, model checksum, DFU payload, and staging-window bound were
verified byte-for-byte.

Hardware result:

```text
sample 0: bank=0 rc=0 page=00 type=40 nontrivial=111
          oob=001bfc02 first=edaef4c4
sample 1: bank=0 rc=0 page=13 type=40 nontrivial=200
          oob=001bd49a first=10ff109c
sample 2: bank=1 rc=0 page=13 type=40 nontrivial=200
          oob=001bd49b first=cb1cff16
```

This proves the complete physical lane mapping: the adjacent odd block is the
missing `+2/+3` half, and both exact absolute-address raw keys are readable
with healthy bodies.  Neither body is the current firmware header, however.
The remaining error is therefore not NAND addressing or a failed body path;
it is the logical namespace used to convert the live Disk Mode extent.

`filefrag` reported partition-relative 4096-byte block `0x000dea0e`.  Adding
the live MBR start `0x3f` selected the just-proven non-header pages.  The
directly testable correction is that the NAND OOB namespace addresses the FAT
volume before Disk Mode's MBR offset: `0x000dea0e * 2 = 0x001bd41c` for the
first 2-KiB half.  Relative to the mapped raw base `0x001bd400`, delta `0x1c`
selects block `0x1426`, page 3, bank 0; bank 1 should carry consecutive key
`0x001bd41d`.  The next read-only probe samples exactly those two pages.  The
bank-0 body should begin `0x64074205` if this namespace correction is right.

Prepared partition-relative header probe:

- bootloader body: `25280` bytes
- native application body: `72604` bytes
- combined IMG1 body: `97884` bytes
- remaining IRAM0 body margin: `33188` bytes
- application model-seeded byte checksum: `0x0073ff3f`
- application sha256:
  `172d3c75b679cc50d5bcbec1f2bacc35e6d46facbf1585bfaca08535540da8a0`
- combined IMG1 sha256:
  `b3b5ac72bb2a6d5c29f6300d4c03af706126c60b34ecf881a1ba8d9750f55a58`
- DFU image: `99936` bytes
- DFU sha256:
  `9250f34b4b330aa219ca1bc930817fc27e4f69b0c30244ad7899f981c0192ac9`
- artifact:
  `/tmp/nano3g-native-partition-relative-header-probe-20260713.dfu`

The bootloader/app concatenation, model checksum, complete DFU payload,
zero-valued wrapper padding, and 128-KiB staging bound were verified
byte-for-byte before hardware use.

Hardware result:

```text
sample 0: bank=0 rc=0 page=00 type=40 nontrivial=111
          oob=001bfc02 first=edaef4c4
sample 1: bank=0 rc=0 page=03 type=40 nontrivial=1ff
          oob=001bd41c first=00000002
sample 2: bank=1 rc=0 page=03 type=40 nontrivial=200
          oob=001bd41d first=4fa400ec
```

Both partition-relative raw keys are exact and both body paths are healthy,
but this physical copy does not contain the current file header.  The scaling
is now unambiguous: OOB LPNs are logical 2-KiB page numbers, so one FAT
4096-byte sector is its consecutive even/odd pair.  The remaining selection
problem is FTL generation/replay, not sector-size conversion.

The proven hardware layout also corrects the inherited Nano 2G geometry.
A Nano 3G logical hyperblock has eight lanes (two chip selects, adjacent-block
`+2/+3`, and block-bit-12 `+4..+7`) and 128 rows, hence `0x400` logical 2-KiB
pages.  Raw target `0x001bd41c` is logical block `0x6f5`, page `0x01c`.
The first half of the captured type-`0x44` block map contains a candidate
whose page-zero OOB is `0x001bd400` and USN `0x0000f788`, but its target body
is stale.  The current type-`0x43` context exposes consecutive map-page
pointers `0x00098486` and `0x00098487`; the second map half is expected on
the bank-1 lane corresponding to the captured bank-0 type-`0x44` page.  It
must be captured and replayed before selecting another data copy.

The direct DFU/BootROM attempt to capture bank 1, physical block `0x14c3`,
page `0x10` returned a 2048-byte all-zero body and an OOB prefix of twelve
zero bytes.  The matching bank-0 OOB remains coherent:

```text
bank 0: usn=fffc8e3c index=0000 type=44
bank 1: 000000000000000000000000 (host BootROM failure)
```

This repeats the already characterized host bank-1 limitation; it does not
describe NAND contents.  The next native probe reads that bank-1 map page,
extracts presumed second-half entry `0x2f5` for logical block `0x6f5`, and
follows it through the eight-lane physical decoder to raw target
`0x001bd41c`.  Its middle sample displays the selected vblock in the usual
three-digit count field and OOB word 1 on the last row.

Prepared native map-half/direct-entry probe:

- bootloader body: `25280` bytes
- native application body: `72692` bytes
- combined IMG1 body: `97972` bytes
- remaining IRAM0 body margin: `33100` bytes
- application model-seeded byte checksum: `0x0073dcad`
- application sha256:
  `069893ccac79caaaa322b757d96b10f61055d3d317cd8c428a4f4fe6f7cb916c`
- combined IMG1 sha256:
  `1d82f6cdcb620ea87b07e302b12bbed4fc78a75dfa81c3c5d6d35af38058160b`
- DFU image: `100032` bytes
- DFU sha256:
  `2e6180f3699136c6b1bd7654d2d4baf068376158e8d5104a3545a3c8570a7272`
- artifact: `/tmp/nano3g-native-map1-direct-entry-probe-20260713.dfu`

The chained body, model checksum, complete DFU payload, zero-valued wrapper
padding, and staging-window bound were verified byte-for-byte.

Hardware result (the map USN is normalized against the exact bank-0 OOB; the
small display's `8E` glyphs were transcribed as `0B`):

```text
sample 0: bank=0 rc=0 page=00 type=40 nontrivial=111
          oob=001bfc02 first=edaef4c4
sample 1: bank=1 rc=0 page=10 type=44 selected-v=213
          usn=fffc8e3c index-word=ffff0001
sample 2: bank=0 rc=0 page=03 type=40 nontrivial=200
          oob=0002841c first=d0d7c3f1
```

The second type-`0x44` map half is fully readable through the native bank-1
path and its index word is exactly one.  Its local entry `0x2f5` contains
vblock `0x0213`, but following that entry reaches logical OOB `0x0002841c`,
not `0x001bd41c`.  This directly proves the type-`0x44` body is pool ordered;
logical block number is recovered from each candidate vblock's page-zero OOB,
not from the entry's array position.

The next bounded read-only probe loads this bank-1 map half, scans its valid
vblocks' page-zero OOB for logical hyperblock base `0x001bd400`, chooses the
highest user-data USN among matches, and follows that vblock to lane 4, row 3.
Its middle group encodes `match-count`, `index-high`, `index-low`, and
`vblock` on the first row, followed by winning raw LPN and USN.

Prepared native map-half reverse-scan probe:

- bootloader body: `25280` bytes
- native application body: `73204` bytes
- combined IMG1 body: `98484` bytes
- remaining IRAM0 body margin: `32588` bytes
- application model-seeded byte checksum: `0x0074aeb1`
- application sha256:
  `23fdca66c90399a1bd381fd10101d396b4187c4e79613cc7f33070fb4eebf79e`
- combined IMG1 sha256:
  `e5a588124f5bc3540a2c7d823bbb43b415e6874421862ee4a4ac6dd4885b80ef`
- DFU image: `100544` bytes
- DFU sha256:
  `026eca29097060ebfab609498dec697b8cc561655d1e46516626250064bd20cf`
- artifact: `/tmp/nano3g-native-map1-reverse-scan-probe-20260713.dfu`

The chained body, model checksum, complete DFU payload, zero-valued wrapper
padding, and staging-window bound were verified byte-for-byte.

Hardware result:

```text
sample 0: bank=0 rc=0 page=00 type=40 nontrivial=111
          oob=001bfc02 first=edaef4c4
scan:     matches=1 rc=0 index=047 v=027e
          page0-raw=001bd402 usn=0000f788
sample 2: bank=0 rc=0 page=03 type=40 nontrivial=1ff
          oob=001bd41c first=00000002
```

The second map half contains the paired odd-block entry for the same base
generation: vblock `0x027e`, physical block `0x0427`, page-zero raw
`0x001bd402`, USN `0x0000f788`.  Following it correctly converges on the same
stale raw page as the even candidate.  There is no newer base-map generation
for raw hyperblock `0x001bd400` in this map half.

The address premise for that scan was nevertheless incomplete.  Earlier
current hardware/replay work already proved an anchor that is stronger than
assuming the FAT volume begins at raw LPN zero:

```text
Disk Mode absolute 4096-byte LBA 0000003f -> OOB raw 000140fc
raw base at absolute LBA zero             ->         0001407e
```

Therefore the deterministic Disk Mode conversion is
`raw = 0x0001407e + 2 * absolute_4K_LBA`.  For the current file extent this is:

```text
absolute 4-KiB LBA = 000dea0e + 0000003f = 000dea4d
first raw page      = 0001407e + 2 * 000dea4d = 001d1518
second raw page     = 001d1519
```

Raw `0x001d1518` is hyperblock base `0x001d1400`, offset `0x118`, hence lane
0 and row `0x23`.  The row exactly agrees with the independently observed May
header at physical page offset 35; only its current vblock must be recovered.
The next reverse scan uses this corrected base and follows its newest map-half
match to bank 0, row `0x23`.

Prepared corrected-offset header probe:

- bootloader body: `25280` bytes
- native application body: `73188` bytes
- combined IMG1 body: `98468` bytes
- remaining IRAM0 body margin: `32604` bytes
- application model-seeded byte checksum: `0x0074a2cd`
- application sha256:
  `81476cc96dc934d942a3cd5e74209f90194b1d6905789bf2a73ee172feb7df32`
- combined IMG1 sha256:
  `799c9f5cfd940166881a709ffd810c2e80368c59c062679bf68fd5ba4b6de853`
- DFU image: `100528` bytes
- DFU sha256:
  `58e6cde82c7a0c841d023228659873596a459aa95cdf24cd97437f2d13d73ec5`
- artifact:
  `/tmp/nano3g-native-offset-corrected-header-probe-20260713.dfu`

The bootloader/application concatenation, model checksum, complete DFU
payload, wrapper header and zero padding, and 128-KiB staging bound were
verified byte-for-byte.  This read-only probe reverse-scans the second current
type-`0x44` map half for raw hyperblock `0x001d1400`, selects its highest-USN
match, and follows the paired even physical block to bank 0, row `0x23`.

Hardware result:

```text
sample 0: bank=0 rc=0 page=00 type=40 nontrivial=111
          oob=001bfc02 first=edaef4c4
scan:     matches=0 rc=0 index=ffff v=ffff
          page0-raw=ffffffff usn=00000000
sample 2: bank=0 rc=0 page=23 type=ff nontrivial=000
          oob=ffffffff first=ffffffff
```

The second current map half has no page-zero candidate for raw hyperblock
`0x001d1400`.  The sample-2 erased result is only the deliberate fallback
block used when the scan has no winner.  An offline pass over the already
captured first-half body and its 1024 page-zero OOB records likewise finds
zero `0x001d1400` candidates, so repeating the same base-map lookup in the
other half is not useful.

The newest context supplies a more specific lead.  At its Nano-3G log-array
offset `0x1a4`, entry 6 names logical hyperblock `0x0745`, exactly
`0x001d1400 / 0x400`, with `pagesused=0x400`,
`pagescurrent=0x2ba`, and sequential flag 1.  The matching current
type-`0x45` table at index 6 has 698 valid entries and maps logical offset
`0x118` to physical offset `0x118`.  Therefore the exact target remains
bank 0, physical page offset `0x23`; only the owning physical block is
unknown.

The next authoritative read-only step is a DFU-side OOB scan of bank 0,
page offset `0x23`, across all physical blocks for exact raw key
`0x001d1518`.  A hit will be body-dumped immediately and compared with the
expected first CPU word `0x64074205`.  This avoids assuming that a current
scattered/committed block must still be discoverable from page zero of either
dense-map half.

That host-side scan completed with exactly one match:

```text
bank=0 physical-block=0x0dfe page=0x23
raw=0x001d1518 usn=0x0000fddd type=0x40
body[0:8]=054207646e6e3367
CPU first word=0x64074205
```

The captured page body is the exact live `nn3g` firmware header.  Its SHA-256
is `4e498aeedfc0668594c237665191616308554695a0f9f27620b8da55cab517dc`;
the matching OOB SHA-256 is
`dcdac9677b1570b24e870fe80d0ef13363dfa63608b88913469de2b1ba9ac9aa`.
The captures are under
`tmp/n3g-live-file-raw1d1518-row23-scan/`.  A separate page-zero OOB read of
the same block reports raw `0x001d1400`, USN `0x0000fddd`, and type `0x40`
(SHA-256
`1e1efb80df0909cf21a65619aa29459c45c1380e2c11a1fb5cd350f5e2b41b8a`).
With the VFL block offset `0x1a9`, this is vblock `0x0c55`; it is absent from
both captured current base-map halves, consistent with the active log entry
being represented by its type-`0x45` table instead.

The context/table evidence now gives a deterministic read-only decoder for
the complete live 854812-byte file.  Its 418 NAND pages occupy logical page
offsets `0x118` through `0x2b9`, exactly ending at the type-`0x45` table's
698-entry boundary `0x2ba`.  For a 512-byte file sector, the decoder is:

```text
logical page  = 0x118 + file-sector / 4
lane          = logical-page & 7
row           = logical-page >> 3
bank          = lane & 1
physical block= 0x0dfe + ((lane >> 1) & 1) + ((lane & 4) ? 0x1000 : 0)
raw           = 0x001d1400 + logical-page
512-byte slot = file-sector & 3
```

The transient native storage path now gives this exact current extent,
starting at synthetic host LBA `0x006ff0ee`, priority over the older stale
physical-map fallback.  Each newly selected 2-KiB page is read with
`nano3g_nand_diag_bank_read()`, accepted only when its OOB raw key is exact
and its type is `0x40` or `0x41`, then served from a dedicated page cache.
There are no NAND program, erase, or metadata-update operations in this
path.

The application probe streams `/rockbox.ipod` in 512-byte chunks and displays
the total bytes, firmware header checksum, independently accumulated model
checksum, first header word, and model tag.  A root result of `3` is emitted
only for exactly 854812 bytes, model tag `nn3g`, and equal checksums.  The
expected successful display values are:

```text
root result:       3
bytes:             000d0b1c
header checksum:   05420764
computed checksum: 05420764
header bytes:      05420764
model bytes:       6e6e3367
```

Prepared full-current-file checksum probe:

- bootloader body: `25280` bytes
- native application body: `102484` bytes
- combined IMG1 body: `127764` bytes
- remaining 128-KiB IRAM0 staging margin: `3308` bytes
- application model-seeded byte checksum: `0x00a4d5b8`
- bootloader SHA-256:
  `ecd49deac8e4668e57885713c56484ef3647c52f34941f27a16ad197c2c1d2b2`
- application SHA-256:
  `77e3cd2d7bf4982a91cafe00538aea7ccda9f0f97d3a67ec247a48ef77edb985`
- combined IMG1 SHA-256:
  `a02339df8e9d8a55cace3a60cbe8cff03806ef0be1aeb3a20564aaa082ee5711`
- DFU image: `129824` bytes
- DFU SHA-256:
  `59740cb6508409a5df2b7bed71d9b029955f9fac7d0d579a095ac50da8e5f1a1`
- artifact:
  `/tmp/nano3g-native-current-file-full-checksum-probe-20260713.dfu`

The bootloader prefix and application suffix were compared byte-for-byte.
The DFU wrapper has the expected `87021.0`/`0x02` signature, three identical
little-endian payload lengths of `127776`, zero header padding, an exact
127764-byte combined body at offset 2048, and the required twelve-byte zero
suffix.

Hardware result:

```text
root/dot result:   6 1
bytes:             000d0b1c
parsed checksum:   64074205
computed checksum: 05420764
header bytes:      05420764
model bytes:       6e6e3367
root/dot trace:    0 0
```

This is a complete successful file read with a diagnostic byte-order bug, not
a content mismatch.  `tools/scramble -add=nn3g` writes its checksum in big
endian order, and the normal loader correspondingly uses `load_be32()`.  The
probe had parsed `05 42 07 64` as little endian `0x64074205`; its independently
accumulated body checksum is the correct `0x05420764`, identical to the header
bytes interpreted in their defined big-endian order.  The full byte count,
model tag, header, and checksum therefore prove all 1670 file sectors were
read coherently.  The diagnostic parser has been corrected to big endian.
The dot-file open result `1` is expected because this bounded synthesized FAT
view intentionally exposes only the root file.

With loader correctness now established, the next transient boot no longer
embeds an application or enters any hard-coded function offset.  The
bootloader mounts the same read-only synthesized view, lets the normal
`load_firmware()` path strip the eight-byte header and verify the big-endian
checksum, then performs the already-proven clean native contract: disable
IRQ, clean/discard caches, and branch to the loaded body's entry at
`0x08000000`.

Prepared read-only disk-load/clean-crt0 image:

- bootloader body: `73152` bytes
- remaining 128-KiB haxed-DFU staging margin: `57920` bytes
- bootloader SHA-256:
  `ab338275260109d3e87c9c02a1143fbdd2d0a94c0e5e4fa5ab720ac0da5177e7`
- DFU image: `75200` bytes
- DFU SHA-256:
  `78f17c5fba3d7875ce1ee9c82d6b28575dcc59088d3976a812eba1353dfc993e`
- artifact:
  `/tmp/nano3g-native-readonly-disk-clean-crt0-20260713.dfu`

The wrapper signature, three `73152`-byte length fields, zero padding, exact
bootloader body at offset 2048, and zero tail were verified byte-for-byte.
Only NAND reads are reachable; program, erase, sector write, NOR write, and
persistent boot-path changes remain excluded.  Immediately before handoff the
screen reports `N3G DISK VERIFIED`, `854804 BODY BYTES`, and
`CLEAN CRT0 IN 2 SEC`.

Hardware reached that exact three-line verification screen and it remained
visible after the two-second handoff delay.  This proves the normal loader
opened the current file, read its 854804-byte body, and accepted checksum
`0x05420764`.  The unchanged framebuffer after the deterministic branch is
consistent with the disk-installed May image stopping during early startup,
before its first display update.

That installed image predates the 2026-06-16 Nano-3G `crt0.S` correction which
omits `memory_init()` on this target.  The old image's exact memory-init entry
is already documented by the prior image analysis at body offset `0x78160`.
The next transient loader therefore scans only the first `0x1000` bytes of the
checksum-verified DRAM body, decodes ARM `BL` targets, and requires exactly one
branch to `image + 0x78160`.  On one match it replaces only that call in the
temporary DRAM copy with an ARM NOP, which is semantically identical to the
current source guard, then enters the image normally at offset zero.  Zero or
multiple matches stop on a diagnostic screen without patching or branching.

Prepared read-only disk-load/old-crt0-memory-skip image:

- bootloader body: `73504` bytes
- remaining 128-KiB haxed-DFU staging margin: `57568` bytes
- bootloader SHA-256:
  `6f2b81d3b159995b9def9b4481396b009d0fc9d794b9a1390ce9faf16072096f`
- DFU image: `75552` bytes
- DFU SHA-256:
  `71c297c4cfb878ee032fa7720fc79fc673081f31dfef5d38130acd4dbbcf9042`
- artifact:
  `/tmp/nano3g-native-readonly-disk-crt0-memskip-20260713.dfu`

The wrapper signature, all three `73504`-byte length fields, zero padding,
exact body at offset 2048, and zero tail were verified byte-for-byte.  The
patch is RAM-only; NAND/NOR program, erase, and sector-write paths remain
unreachable.

Hardware stopped safely at the pre-patch guard:

```text
N3G CRT0 SCAN STOP
MATCHES 0
OFF FFFFFFFF
WORD FFFFFFFF
```

Thus the exact call is not present in the initial `0x1000`-byte window; no
word was modified and no image entry was taken.  The startup/init input
sections in this older full build are not guaranteed to reside in that small
prefix.  The next version scans the complete already checksum-verified
854804-byte DRAM body, while retaining the same exact decoded target and
one-match requirement.  On a stop it additionally reports branches whose
decoded targets fall in `image + 0x77000..0x78fff`, so a shifted historical
symbol can be diagnosed without broadening the patch criterion.

Prepared full-body exact-branch scan image:

- bootloader body: `73632` bytes
- remaining 128-KiB haxed-DFU staging margin: `57440` bytes
- bootloader SHA-256:
  `c9701878623dedfe5b6a6b56a8f4c9dfb8db718f9e6cdfc4769aa60a9e290187`
- DFU image: `75680` bytes
- DFU SHA-256:
  `6b314a57213a04861e9fb0000e83144e9ead7ad3b68f63e3484820c5c2b08701`
- artifact:
  `/tmp/nano3g-native-readonly-disk-crt0-memskip-fullscan-20260713.dfu`

The payload, wrapper fields, padding, and zero tail were again verified
byte-for-byte.  All changes remain confined to the transient DRAM copy.

Hardware stopped at the full-body guard with:

```text
N3G CRT0 SCAN STOP
MATCHES 0
OFF FFFFFFFF
WORD FFFFFFFF
NEAR 150
00080ADC 000782D4
```

No word was patched and the image was not entered.  The full verified body
contains no ARM `BL` whose decoded target is body offset `0x78160`, proving
the historical hard-coded entry is not the installed image's `memory_init()`
address.  The final row is diagnostic only: it is the last of 150 calls in
the entire image whose target lies in `0x77000..0x78fff`, not evidence that
the call at `0x80adc` is part of crt0.  Broadening the RAM patch to any of
those calls would therefore be unsafe.

The next step is host-side reconstruction of the exact installed binary,
before any further handoff image is built.  The read-only
`--dump-current-rockbox` mode in `tools/nano3g_wmount_dfu_dump.py` groups the
proven 418-page mapping into its eight physically contiguous lanes, reads
each body and OOB area, and rejects any page whose raw key or type differs
from the expected mapping.  It reassembles file order only after all OOB
checks pass, then requires all of the following before naming the result
`rockbox.ipod`:

```text
size:              854812
first eight bytes: 054207646e6e3367
header checksum:   equal to 117 + sum(body bytes), modulo 2^32
sha256:            a4d8fc899cf49e35552c2303e7605d0bf54170b44e37309b01a2acdde6066f5d
```

The temporary local wInd3x helper also has a Nano-3G-only `nand readwide`
command.  It uses the same BootROM NAND read helper and only the USB control
IN reply, but returns one already-read 2048-byte body per request instead of
re-reading the NAND page for each 64-byte slice.  The ordinary slow `nand
read` remains available as a fallback.  Neither path contains NAND program,
erase, metadata update, NOR write, or sector-write operations.

### Exact-file host dump checkpoint

The first `--dump-current-rockbox` run is captured under
`tmp/n3g-current-rockbox-exact-20260713/`.  Lane 0 completed and begins with
the expected live firmware header:

```text
lane 0: bank=0 pblock=0x0dfe rows=0x23..0x57 pages=53
body sha256: 72f94bffda9e0d0b584322641f6d9807f6b8b20e0acdcb231dcc4e24721456c4
oob  sha256: c6b2088838580c6dcdc35e27809f501cbab96c4ce5b6735177db4b7d94ce9f4f
first bytes: 054207646e6e3367
```

Lane 1 returned zero body data and invalid OOB (`raw=0`, `type=0`) at its
first page.  The manifest stopped at:

```text
N3GR_FAIL lane=1 fp=1 reason=oob raw=00000000 expected=001D1519 type=00
```

This reproduces the already characterized host BootROM bank-1 limitation;
it does not invalidate the native bank-1 reads or the proven live-file
mapping.  Do not accept or reassemble the partial host capture.

A follow-up attempt to test whether the generic DFU memory dumper could be
used after a native read stage blocked in its first 64-byte transfer.  The
stale `wInd3x-extra` process was terminated, but a fresh bounded control dump
and a one-page `nand readwide` control also blocked.  A host USB reset did not
recover the endpoint and removed the device from enumeration.  No NAND/NOR
write, program, erase, or persistent boot-path operation was issued.  The
next session must begin with a physical Nano reset/reconnect into `05ac:1223`
DFU mode, then use a bounded native USB-return path (or another native-visible
transport) for lanes 1, 3, 5, and 7 rather than repeating the host BootROM
bank-1 dump.

### Disk-Mode exact-image recovery and corrected handoff diagnosis

The bounded native USB-return experiment loaded and checksum-verified the live
disk file and reached:

```text
N3G DISK VERIFIED
854804 BODY BYTES
USB READBACK START
```

The native USB controller did not re-enumerate on the host from that execution
context, so no readback frames were transferred.  The Nano was physically reset
into Apple Disk Mode (`05ac:1262`) without issuing a NAND/NOR write, program, or
erase operation.  Disk Mode exposed both firmware copies directly.  They were
read and compared while the volume was mounted read-only:

```text
/rockbox.ipod:          854812 bytes
/.rockbox/rockbox.ipod: 854812 bytes
sha256 (both):          a4d8fc899cf49e35552c2303e7605d0bf54170b44e37309b01a2acdde6066f5d
```

The exact local readbacks are under
`tmp/n3g-installed-exact-20260713/`.  Their stripped 854804-byte body is
`rockbox.bin`, SHA-256
`106f7e33d4124f725d74826d5bbfb3e8e029d073d6d10db2d0f8958a36c1b46e`.

Disassembling this exact body corrects the earlier historical-offset premise:
its `crt0` already omits `memory_init()`.  The link map preserved in commit
`cc86cc4c15` matches the exact image's code addresses, including `main` at
`0x08005d78`, `system_init` at `0x08077c2c`, `lcd_init_device` at
`0x080797c8`, and `nano3g_safe_mode_enabled` at `0x0807eec0`.  The last
function returns true unconditionally.  Consequently `lcd_init_device()`
takes its safe-mode early return and leaves LCD updates disabled.  The old
clean entry therefore could have reached the application's terminal
pre-storage checkpoint while leaving the bootloader framebuffer unchanged;
that screen was not proof of a CPU stop.

The next loader operates only on the checksum-verified DRAM copy of this exact
image.  It requires the exact body length, entry/main/safe-mode signature words,
and all seven original instruction words before changing anything.  It then
applies these narrowly bounded RAM-only changes:

- skip application CPU-frequency changes and `cpu_boost()`, retaining
  `i2c_init()`;
- skip `power_init()` and application IRQ/FIQ enable, entering `lcd_init()`;
- preserve IRAM1 and the BootROM clock/PMU state in `system_init()`, matching
  the already-proven `NANO3G_NATIVE_PRESTOR_ONLY` contract;
- bypass only the LCD safe-mode early return while leaving the global safe-mode
  policy true; and
- after `settings_reset()`, branch to the image's own terminal
  `N3G_NATIVE_PRESTOR` display checkpoint before USB or storage initialization.

Any guard mismatch displays `N3G PATCH GUARD STOP` and does not enter the
image.  The disk file and all persistent media remain untouched.

Prepared exact-image pre-storage RAM-patch test:

- bootloader body: `73696` bytes
- remaining 128-KiB haxed-DFU staging margin: `57376` bytes
- bootloader SHA-256:
  `2f37a33b007f7b16c2dba24169aba2304bf83dd44c778399dd1ab3dabfdd6d21`
- DFU image: `75744` bytes
- DFU SHA-256:
  `895d4f2115148b5a1bcad45ad52611d2992f46d80d0305bb6b6b6eb42e4f401b`
- artifact:
  `/tmp/nano3g-native-installed-prestor-ram-patch-20260713.dfu`

The Nano-3G `87021.0` format-2 wrapper, three `73696`-byte length fields,
2024 bytes of zero header padding, exact body at offset 2048, total length, and
absence of a tail were verified.  The device is now connected in Disk Mode and
the volume is explicitly read-only.  The next required action is a physical
reset into `05ac:1223` DFU mode, followed by the transient upload.  A successful
test first shows the loader's four-line two-second guard screen and then:

```text
N3G_NATIVE_PRESTOR
handoff ok
storage skipped
```

The device was then confirmed in standard DFU mode as `05ac:1223`, and
`wInd3x-extra run` successfully entered haxed DFU, parsed the image as Nano 3G,
received `dfuMANIFEST`, and reported `Image sent` at 2026-07-13 14:56 local
time.  The transient image is now running; the stopping point is the required
physical screen inspection.  No persistent write path was enabled or invoked.

Hardware reported the expected final image-owned checkpoint:

```text
N3G_NATIVE_PRESTOR
handoff ok
storage skipped
```

This proves that the exact disk image passed all RAM-patch guards, entered its
own `crt0`, completed the preserved-clock `system_init()`, core allocator,
kernel, filesystem, I2C, LCD, font, and settings-reset path, and performed a
native framebuffer update.  It also confirms that the prior unchanged
bootloader framebuffer was caused by the image's LCD safe-mode early return,
not evidence that the clean entry failed.

The next bounded test retains only five RAM patches: skip the application's
later CPU-frequency transition, preserve IRAM1/BootROM clocks, skip PMU
preinit, and bypass the LCD safe-mode early return.  It no longer branches
early to the terminal checkpoint.  The exact image will run its original
remaining pre-storage sequence, including `power_init()`, IRQ/FIQ enable,
logo/language/serial/RTC/ADC, USB, backlight, buttons, power management, piezo,
and GUI initialization before naturally reaching the same terminal screen.
The image's global safe mode remains true, so PMU and piezo writes are blocked;
the existing terminal still precedes `storage_init()`.

Prepared full original pre-storage-path test:

- bootloader body: `73664` bytes
- remaining 128-KiB haxed-DFU staging margin: `57408` bytes
- bootloader SHA-256:
  `8aa9c52ce063019d210c3889ab6a04c1809ee0e25b3e951fc41def91b500bbdc`
- DFU image: `75712` bytes
- DFU SHA-256:
  `4cf7c40437bd21573b23c48d67b6db07232ad3b81f9f7f13a066b6c7f982d5da`
- artifact:
  `/tmp/nano3g-native-installed-full-prestor-ram-patch-20260713.dfu`

The format-2 wrapper, all three `73664`-byte length fields, 2024-byte zero
header padding, exact bootloader body at offset 2048, total length, and absence
of a tail were verified.  The next stopping point is another physical reset to
standard DFU mode; the currently running image leaves the old DFU USB identity
visible but does not provide a live BootROM control endpoint.

The fresh standard-DFU connection enumerated as a new `05ac:1223` device, and
the full pre-storage artifact was uploaded at 2026-07-13 15:04 local time.
`wInd3x-extra` reported haxed DFU running, parsed the image as Nano 3G, received
`dfuMANIFEST`, and reported `Image sent`.  Transient libusb interrupted-event
warnings did not abort the transfer.  The stopping point is the physical screen
result from the now-running image.

Hardware displayed the Rockbox boot logo with incorrect-looking colors and did
not replace it with the terminal pre-storage text.  The hardware version of
`show_logo_boot()` has no timed sleep, so this proves execution reached the
logo update but not the later terminal framebuffer update; it does not by
itself identify which subsequent initializer stopped progress.

The most specific difference from the earlier successful path is
`power_init()` before IRQ/FIQ enable.  In this exact safe-mode image,
`pmu_init()` registers the PMU external-interrupt path while safe mode turns
the PMU mask and event-ack writes into no-ops.  An already asserted PMU source
can therefore be re-registered without being cleared and starve normal
progress once IRQs are enabled.  The next test changes only that dimension:
it replaces the exact `power_init()` call with an ARM NOP, retains the
immediately following IRQ/FIQ enable, and otherwise runs the same complete
pre-storage tail.  All six RAM patch sites and original words remain guarded.

Prepared full pre-storage/no-`power_init()` discriminator:

- bootloader body: `73664` bytes
- remaining 128-KiB haxed-DFU staging margin: `57408` bytes
- bootloader SHA-256:
  `63feec71e92aea62733b2af4795749f37e46f6a9e2cbf95da6c5d746234ad70b`
- DFU image: `75712` bytes
- DFU SHA-256:
  `5b93085e788b1f089db5e8030d295f470d4ea8c35798a69f7d02eda8137b9f29`
- artifact:
  `/tmp/nano3g-native-installed-full-prestor-no-powerinit-20260713.dfu`

The wrapper fields, zero header padding, exact body at offset 2048, total
length, and absence of a tail were verified.  This remains transient,
read-only, and stops before `storage_init()`.

The device re-enumerated as a fresh standard-DFU instance and the no-power-init
artifact was uploaded at 2026-07-13 15:09 local time.  Haxed DFU entry,
Nano-3G image parsing, `dfuMANIFEST`, and `Image sent` all completed; the
non-fatal interrupted-event warnings were the same as the preceding transfer.
The stopping point is its physical screen result.

Hardware showed the Rockbox logo briefly, then replaced it with the
`Nano3G failsafe halt` screen.  This proves that removing `power_init()` let
execution advance far enough to render a later safety stop; the previous
persistent-logo result was therefore tied to the PMU-init/IRQ interaction.
The failsafe renderer places the exact reason on row 1 and recent boottrace
entries below it.  Those remaining rows must be transcribed before selecting
the next RAM-only discriminator; the three possible failsafe callers in this
exact image are `system_exception_wait`, `unknown LCD panel`, and
`power_off in safe bringup`.

The complete screen transcription was:

```text
Nano3G failsafe halt
power_off in safe bringup
system_init
lcd_init_device
lcd detect start
lcd detect ok
lcd_init_device done
power_off
Failsafe halt
power_off in safe bringup
```

This is definitive.  Removing `power_init()` repaired the earlier stop, and
normal call order proves the image returned from logo, language, serial, RTC,
ADC, USB, backlight, and button initialization before `powermgmt_init()`
requested shutdown.  That request is expected when its prerequisite power/PMU
state was deliberately omitted; safe mode then correctly blocked the shutdown.
The next image NOPs the exact `powermgmt_init()` call as the paired half of the
power-path exclusion, while retaining IRQ/FIQ and every other pre-storage
initializer.

Prepared full pre-storage/no-power-path test:

- bootloader body: `73696` bytes
- remaining 128-KiB haxed-DFU staging margin: `57376` bytes
- bootloader SHA-256:
  `25d181422ef0e769155c27d591d3dd4bb8743ababa7e6e59512bcdf0a095defc`
- DFU image: `75744` bytes
- DFU SHA-256:
  `b38339833235773923d51ba2a4d417bf141aa889a887c6031750b0ee2db18213`
- artifact:
  `/tmp/nano3g-native-installed-full-prestor-no-power-path-20260713.dfu`

The seven exact patch guards, wrapper fields, zero header padding, body at
offset 2048, total length, and absence of a tail were verified.  The test is
RAM-only, retains the global safe-mode policy, and cannot enter storage.

The fresh `05ac:1223` instance received this artifact at 2026-07-13 15:16
local time.  Haxed DFU entry, Nano-3G parsing, `dfuMANIFEST`, and `Image sent`
completed successfully; repeated interrupted-event warnings were non-fatal.
The stopping point is the physical screen result.

Hardware reached the natural terminal checkpoint:

```text
N3G_NATIVE_PRESTOR
handoff ok
storage skipped
```

This closes the exact image's complete original pre-storage sequence under the
bounded no-power contract.  In particular, the image returned from USB,
backlight, buttons, piezo, all four GUI initializers, and the final framebuffer
update with IRQ/FIQ live.  Only the paired `power_init()`/`powermgmt_init()`
path was excluded.

The compiler removed the source after this unconditional Nano-3G terminal
loop, so there is no fall-through storage block to expose with a one-word
branch.  The next RAM-only test repurposes the terminal's 36-byte loop/literal
area into a tiny fixed-address stub.  It displays `N3G STORAGE ENTER`, calls
the exact image's existing `storage_init()` once, then deliberately enters the
existing failsafe renderer with reason `storage ok` or `storage fail`.  In this
test the `Nano3G failsafe halt` heading therefore means the bounded probe has
finished, not that an exception occurred.

Before image entry, the loader now verifies the exact signatures of
`storage_init()`, `nano3g_failsafe_halt()`, and every persistent NAND gate:
`nand_write_page()`, `nand_write_page_start()`,
`nand_write_page_collect()`, and `nand_block_erase()` must each remain the
two-instruction `return 1` stub, while `nand_write_sectors()` must remain the
two-instruction `return -1` stub.  Fourteen instruction/literal words and all
three repurposed strings are exact-match guarded before any RAM byte changes.

Prepared exact-image storage-entry test:

- bootloader body: `74272` bytes
- remaining 128-KiB haxed-DFU staging margin: `56800` bytes
- bootloader SHA-256:
  `387dd1cddb78871c0e95cd713b6d28228bbafbbd39f635f548cf10aaa606ef72`
- DFU image: `76320` bytes
- DFU SHA-256:
  `e7ffbaee861bc579a9a757af8c69da2bd3ea2b43dbb5287625e2c00ff31d0712`
- artifact:
  `/tmp/nano3g-native-installed-storage-entry-ram-patch-20260713.dfu`

Both injected ARM calls decode back to the mapped `storage_init()` and
failsafe addresses, and both conditional literal loads resolve to the guarded
result strings.  The format-2 wrapper fields, 2024-byte zero header padding,
exact body at offset 2048, total length, and absence of a tail were verified.
No program, erase, sector-write, NOR-write, or persistent boot-path operation
is reachable.

The device re-enumerated as a fresh `05ac:1223` instance and received the
one-shot storage-entry artifact at 2026-07-13 15:27 local time.  Haxed DFU,
Nano-3G parsing, `dfuMANIFEST`, and `Image sent` completed successfully.  The
stopping point is the physical screen: `N3G STORAGE ENTER` means the old
image's storage initializer has not returned, while a failsafe screen with
reason `storage ok` or `storage fail` is its bounded completion result.

Hardware remained on the miscolored Rockbox logo.  Post-run disassembly found
that this result invalidates the first storage-entry probe: words
`0x5e68..0x5e78`, treated as spare terminal literals by that probe, are also
loaded earlier by main as the CPU-frequency constant, global-status pointer,
language pointers, and statusbar pointer.  Replacing them corrupted the
pre-storage path before the injected storage call could run.  The change was
confined to the checksum-verified DRAM copy and is cleared by reset; no storage
initializer or persistent write path was reached.  Do not reuse artifact
`e7ffbaee861bc579a9a757af8c69da2bd3ea2b43dbb5287625e2c00ff31d0712`.

The corrected stub occupies only the original terminal instructions at
`0x5e38..0x5e64` and leaves the entire `0x5e68..0x5e84` literal pool
byte-for-byte intact.  It uses the original first-line setup, directly calls
the mapped LCD update, sleeps one tick-second, calls `storage_init()`, selects
the existing success/failure string pointers, and enters the mapped failsafe
renderer.  All four injected branch targets and both conditional literal
addresses were independently decoded back to their exact destinations.

Prepared corrected code-only storage-entry test:

- guarded instruction words: `19`
- guarded/replaced result strings: `3`
- bootloader body: `74336` bytes
- remaining 128-KiB haxed-DFU staging margin: `56736` bytes
- bootloader SHA-256:
  `6bcdd286845640ea95691bd874133f47b06c9a298fe76dedb218e5db8eadda82`
- DFU image: `76384` bytes
- DFU SHA-256:
  `d05a7bf091e4cb7b48d23d63b3f69685b7c77ffbd42d6d82fe747d145872ec09`
- artifact:
  `/tmp/nano3g-native-installed-storage-entry-code-only-20260713.dfu`

The exact persistent-write stub guards remain in force.  The wrapper fields,
2024-byte zero padding, exact body at offset 2048, total length, and absence of
a tail were reverified.  This artifact supersedes the failed literal-pool
version and remains transient/read-only.

The corrected artifact was uploaded to a fresh `05ac:1223` instance at
2026-07-13 15:33 local time.  Haxed DFU entry, Nano-3G parsing,
`dfuMANIFEST`, and `Image sent` completed without a transfer error.  The
stopping point is its physical screen result.

Hardware displayed:

```text
N3G STORAGE ENTER
```

This is the exact injected checkpoint immediately before the call to the
installed image's `storage_init()`.  It proves that the corrected code-only
stub preserved the complete pre-storage path and entered the old storage
initializer; that initializer had not returned at the time of observation.

Source for the matching `cc86cc4c15` image explains why this is not a useful
long-running test.  Its application `ftl_init()` first enters the historical
`ftl_n3g_winpod_mount()`, whose fallback searches as many as `0x400` map
entries and every page represented by `ppb`, with additional broad discovery
passes afterwards.  It predates the bounded map-cluster and context-page
limits already proved by the current source's native storage and full-file
checksum probes.  Waiting for that obsolete scan cannot distinguish a hard
stop from hundreds of thousands of slow NAND reads and adds no new coverage.
The image remains RAM-only and all program/erase/sector-write guards were
verified before entry.

The historical scan later completed and the injected terminal reported:

```text
Nano3G failsafe halt
storage ok
```

This is the probe's intentional completion path, not an exception.  The exact
installed image returned zero from `storage_init()` after running its complete
original pre-storage sequence with only `power_init()` and `powermgmt_init()`
excluded.  The long delay was therefore the obsolete broad FTL search, not a
CPU hang.  This closes the combined exact-image pre-storage plus storage-init
checkpoint; the next image should use the current bounded storage source
instead of repeating that historical scan.

### Full current-source safe-boot candidate

The exact-image result permits the two independently proved contracts to be
combined in source rather than with more fixed-address patches.  The current
Nano 3G application now builds the normal full `main()` path with these
bring-up constraints:

- preserve BootROM clocks and IRAM1 and omit PMU preinitialization;
- make runtime CPU-frequency and AHB-boost requests no-ops;
- omit the paired `power_init()` and `powermgmt_init()` calls;
- retain IRQ/FIQ, serial, RTC, ADC, USB, backlight, button, piezo, and GUI
  initialization;
- remove the historical `N3G_NATIVE_PRESTOR` terminal loop;
- enter the current bounded, read-only `storage_init()` path; and
- display `N3G FULL STORAGE OK` after storage returns and `N3G FULL INIT OK`
  immediately before entering the normal Rockbox UI.

The safe-mode NAND program/erase/page-write functions are unreferenced and
garbage-collected from this application.  The linked sector-write entry point
is still exactly `mvn r0,#0; bx lr` (`return -1`).  Disassembly also confirms
that `set_ahb_boost()` and `set_cpu_frequency()` are each a single `bx lr`,
that no `power_init()` or `powermgmt_init()` symbol/call is linked, and that
the full pre-storage sequence calls `storage_init()` at `0x08005f30`.

Prepared full current-source candidate:

- firmware file: `881396` bytes
- stripped body: `881388` bytes
- model/checksum header: `05673eef6e6e3367`
- recomputed model-seeded body checksum: `0x05673eef`
- firmware SHA-256:
  `bb51649118740cc8c70a55d1a91474d454c53a8306ff7bf206c6801202c6b333`
- body SHA-256:
  `ad032f1587f04dd0f88c159f05abb65f21e9ca8174db357fccdaf19692061d1e`
- artifacts: `tmp/n3g-full-safe-20260713/`

At 881388 bytes the application cannot share the 128-KiB haxed-DFU staging
window with a chainloader.  The next test therefore remains tethered but must
stage this file through Apple Disk Mode, copying the exact candidate to both
`/rockbox.ipod` and `/.rockbox/rockbox.ipod` and verifying all three hashes.
After that one file update, rediscover the new root-file physical extent and
build a checksum-guarded transient DFU disk loader; do not alter NOR or the
Apple boot path.

Disk Mode deployment completed on the owned Nano 3G (`05ac:1262`, serial
`000A27001AF57313`) at 2026-07-13 15:59 local time.  Before replacement, both
device copies were saved under
`tmp/n3g-pre-full-safe-backup-20260713/`; each backup is 854812 bytes with
SHA-256
`a4d8fc899cf49e35552c2303e7605d0bf54170b44e37309b01a2acdde6066f5d`.
The candidate was then copied to both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`.  The local artifact and both on-device files are
881396 bytes and independently read back with SHA-256
`bb51649118740cc8c70a55d1a91474d454c53a8306ff7bf206c6801202c6b333`.
After `sync`, a clean unmount, and a read-only remount, both hashes still
matched; the volume was cleanly unmounted again.  An initial kernel warning
recorded that Apple Disk Mode had not previously unmounted the FAT volume
cleanly, but the verified write/read cycle produced no later warning and no
unrelated volume data was changed or repaired.

The transient loader now rejects every image except this candidate before
entry.  It requires the exact 881388-byte stripped-body length, the
model-seeded additive checksum `0x05673eef`, an independent whole-body FNV-1a
fingerprint `0x9ecd7f8f`, and four fixed instruction signatures.  On success
it performs a clean application crt0 entry with no RAM patching.  The loader
still needs the candidate's newly allocated physical extent before it can be
built and sent; extent discovery remains read-only and tethered.

Fresh DFU discovery resolved that extent without trusting the stale file
copy.  The old raw key `0x001d1518` still has exactly one NAND match and still
contains the previous `05420764` image, proving it is a retained obsolete
generation.  The live FAT root page was then recovered from corrected raw key
`0x00014fb8` at bank 0, physical block `0x0dd4`, row `0x77`; its short entry
now reports:

```text
first cluster: 0x000de526
file size:     881396
```

The corresponding FAT page, raw `0x0001482e` at bank 0/block `0x1ac1`/row
`0x05`, proves an exact contiguous 216-cluster chain from `0xde526` through
`0xde5fd`, followed by EOC.  Combining that authoritative allocation with
the previously proved Disk-Mode-to-OOB anchor gives the first file page as
raw `0x001d1a00`.  The all-block scan found exactly one match:

```text
bank=0 physical-block=0x0974 row=0x40 raw=0x001d1a00 type=0x40
body[0:8]=05673eef6e6e3367
```

The captured 2048-byte body matches the local candidate byte-for-byte.  The
adjacent-block/bit-12 lane checks also matched local file pages 2, 4, and 6
exactly at blocks `0x0975`, `0x1974`, and `0x1975`; the final partial page at
block `0x1975`, row `0x75`, raw `0x001d1bae` matches all 756 meaningful tail
bytes.  Thus the new deterministic decoder is:

```text
synthetic file LBA = 0x0070048e
raw base           = 0x001d1800
logical page 0     = 0x200
base physical block= 0x0974
file pages         = 431 (0x200..0x3ae)
```

It retains the proved eight-lane bank/adjacent-block/bit-12 formula.  Source
synthetic root/FAT/file constants and the host dump constants now describe
this exact allocation.  The transient bootloader rebuilt successfully with
only the pre-existing unused diagnostic warnings.  Its only linked NAND
write API remains the exact `return -1` two-instruction stub; page program,
page collect/start, block erase, and NOR write functions are not linked.

Prepared clean full-image loader:

- bootloader body: `73600` bytes
- bootloader SHA-256:
  `74714ae51bfceec2ef9beac52fd781bc7870ea55b025b90b59cec156679ddb4a`
- DFU image: `75648` bytes
- DFU SHA-256:
  `55214cd39f22712acfc11d5fc97685e7d1cc6f1e79aaab14484c0f808e766e11`
- artifacts: `tmp/n3g-full-safe-loader-20260713/`

The Nano-3G format-2 wrapper has three exact 73600-byte length fields, zero
padding through offset 2048, the exact bootloader body, and no tail.  Before
entry the loader requires the deployed body length/additive checksum/FNV and
fixed signatures, then enters its unmodified crt0.  No persistent boot path,
NOR operation, NAND program, or NAND erase is reachable.

The clean loader was uploaded to a fresh standard-DFU instance at 2026-07-13
16:53 local time.  Haxed DFU entry, Nano-3G parsing, `dfuMANIFEST`, and
`Image sent` completed successfully.  Hardware first displayed the Rockbox
boot logo with incorrect colors.  The logo remained visible long enough to
look like a stop, but the current bounded storage path eventually completed
and the application reached its exception failsafe.  The complete screen was:

```text
Nano3G failsafe halt
system_exception_wait
system_init
lcd_init_device
lcd detect start
lcd detect ok
lcd_init_device done
storage_init ok
pcm init skipped (safe)
system_exception_wait
Failsafe_halt
system_exception_wait
```

This proves the clean, unpatched application returned from `storage_init()`,
mounted the volume, and returned from the safe-mode PCM initializer.  In exact
main disassembly the first unconfirmed call is therefore `dsp_init()` at
`0x08060a4`, although an asynchronous exception remains possible.  The generic
failsafe screen cannot identify the fault because `UIE()` first renders the
exception type, faulting PC, FSR/FAR, and backtrace, then
`system_exception_wait()` immediately replaces that display with the failsafe
renderer.

A pre-storage terminal loader was built while the delayed run still appeared
to be stopped at the logo, but it was never uploaded and was superseded by the
screen above.  Its artifacts under
`tmp/n3g-full-safe-prestorage-loader-20260713/` are retained only as an audit
record and must not be used for the next run.

The next transient loader preserves the native `UIE()` screen.  After the same
full-body length, additive-checksum, FNV-1a, and fixed-word guards, it verifies
the exact candidate instructions at `system_exception_wait()+0x0c` and
`+0x20`.  It changes only the DRAM word at application offset `0x000803b4`
from `0xeb001c9d` (call `nano3g_safe_mode_enabled`) to `0xea000003` (branch to
the existing terminal loop at `0x080803c8`).  The preceding boottrace call is
retained, but the generic failsafe call becomes unreachable, leaving the
already-rendered exception details on screen.  No deployed file is changed.

Prepared exception-screen-preserving loader:

- guarded application RAM words: `2`
- changed application RAM words: `1`
- bootloader body: `73664` bytes
- bootloader SHA-256:
  `f0256a7a92a332b9f7e2b07dbce6279d537b0853a63d32474d9ed91b93a23db8`
- DFU image: `75712` bytes
- DFU SHA-256:
  `f0acea718962a52f4f017b4c64b6ca2cb2a52599cafc91fb20f3c034e5e0e163`
- artifacts: `tmp/n3g-full-safe-exception-screen-loader-20260713/`

The format-2 wrapper has three exact `73664`-byte length fields, 2024 bytes of
zero padding after its 24-byte header, the exact loader body at offset 2048,
and no tail.  The only linked NAND write entry remains the exact two-word
`return -1` stub; NAND page program/block erase and NOR write/erase functions
are not linked.  The next stopping point is a physical reset to standard DFU.

The owned Nano re-entered genuine BootROM DFU with serial
`87020000000001`.  The exception-screen-preserving artifact was uploaded at
2026-07-13 17:09 local time.  `wInd3x-extra` reported haxed DFU running,
parsed the wrapper as Nano 3G, received `dfuMANIFEST`, and completed with
`Image sent`; the intervening libusb interrupted-event messages were non-fatal.
The stopping point is the eventual physical exception screen.  Unlike the
preceding run, it should remain on the native `UIE()` diagnostic instead of
being replaced by `Nano3G failsafe halt`.

Hardware retained the native panic screen:

```text
*PANIC* (ece6732bdeM-260713)
dc_writeback_callback() - Could not write sector 41094 (error -1):
pc:0808c8c0
sp:001af690
A: 0808be34

bt end
```

The two high address bytes above normalize the visually ambiguous user
transcription against the exact ELF.  `0x0808c8c0` is the return address after
`dc_writeback_callback()` calls `panicf()`, and `0x0808be34` is the return
address in `dc_commit_all()`.  Sector 41094 (`0xa086`) is the synthetic FAT32
FSInfo sector at partition start `0xa07e` plus eight 512-byte sectors.  The
application therefore returned from `dsp_init()`; the stop is the intended
global NAND-write guard returning `-1` when `settings_load()` enters its
boot-time temporary-settings promotion path.  No NAND write occurred.

The next loader keeps that write guard and transiently patches the guarded
DRAM copy to continue through a read-only boot.  In addition to preserving
the native panic screen, it:

- branches over both temporary-settings promotion blocks while retaining the
  existing `config.cfg`, `.resume.cfg`, and `fixed.cfg` reads;
- skips tagcache-state removal, dircache persistence/status save, tagcache
  initialization/scanning, playback-log allocation, and `playername.txt`
  creation; and
- retains normal playlist/tree/audio/UI initialization and the exact global
  `nand_write_sectors()` `return -1` stub.

All seven changed application words are independently guarded before any word
is changed.  Compiled-loader disassembly confirms the stores at offsets
`0x1daf4`, `0x60f0`, `0x60fc`, `0x614c`, `0x6220`, `0x6224`, and `0x803b4`.
The candidate bytes at those offsets still match the expected instructions.
NAND page-program/start/collect, block-erase, and NOR write/erase sections are
not linked.

Prepared read-only-continuation loader:

- bootloader body: `73888` bytes
- bootloader SHA-256:
  `79457b154843e50edb5987707d8b042f38475428725985aee1285a76fc26cdda`
- DFU image: `75936` bytes
- DFU SHA-256:
  `7b599cb02c4c2da7645af5a4766ab27d542a20c65d7ea3376e5d4ca750adcc4a`
- artifacts:
  `tmp/n3g-full-safe-readonly-continuation-loader-20260713/`

The format-2 wrapper has three exact `73888`-byte length fields, zero padding
from its 24-byte header through offset 2048, the byte-identical loader body at
offset 2048, and no tail.  The device was already back in genuine BootROM DFU
with serial `87020000000001`, so no further physical reset was needed.  The
artifact was uploaded at 2026-07-13 17:27 local time; haxed DFU entry, Nano-3G
parsing, `dfuMANIFEST`, and `Image sent` all completed successfully.  The
stopping point is the physical screen after the read-only continuation enters
the full application.

Hardware completed the deliberately slow storage boot and reached the normal
Rockbox root menu using the fallback theme.  The display colors remain
incorrect, but there was no panic and the global NAND-write failure guard did
not fire.  This proves the guarded read-only path returned through settings,
playlist, tree, filetype, shortcuts, audio-core, accessory, skin, list, and
tree initialization and entered `root_menu()`.  Full current-source Rockbox UI
startup on the owned Nano 3G is therefore confirmed; theme selection and LCD
color ordering remain separate follow-up issues.

### LCD color discriminator

The fallback theme is an expected consequence of the current synthetic FAT
view, not evidence of a second skin-engine failure.  Its synthetic root sector
contains only `/rockbox.ipod`; it does not expose `/.rockbox/config.cfg`, the
`/.rockbox` directory, `cabbiev2.wps`, fonts, backdrops, or other theme assets.
Rockbox therefore keeps the default `cabbiev2` setting, cannot open its WPS,
and falls back to the compiled skin.

The remaining LCD fault is now isolated with a single transient screen instead
of another full storage boot.  The visibility-only bootloader displays the
four-byte panel ID and panel type, then six columns labelled `R G B C M Y` in
four rows:

- `A`: normal RGB565;
- `B`: byte-swapped RGB565;
- `C`: red/blue-swapped RGB565; and
- `D`: red/blue plus byte swap.

Prepared read-only LCD color probe:

- bootloader body: `30816` bytes
- remaining 128-KiB haxed-DFU staging margin: `102256` bytes
- bootloader SHA-256:
  `9a0ed3b7e05d66efdd417721968eafe2ac0e5e2d82eeb4a64426e5388f92387b`
- DFU image: `32864` bytes
- DFU SHA-256:
  `84185fe75ecf8fe4d9ebea369095f91517551db8e689751c72ee2332bdb401cc`
- artifacts: `tmp/n3g-lcd-color-probe-20260713/`

The format-2 wrapper has three exact `30816`-byte length fields, zero padding
from its 24-byte header through offset 2048, the byte-identical body at offset
2048, and no tail.  Link and disassembly inspection show the probe loops on the
rendered screen before IRQ enable or storage entry; storage mount and NAND/NOR
write or erase routines are not linked.  The next stopping point is a physical
reset from the running Rockbox menu into genuine `05ac:1223` BootROM DFU,
followed by this transient upload and one screen inspection.

The owned Nano re-entered genuine BootROM DFU with serial `87020000000001`.
The verified probe was uploaded at 2026-07-13 17:54 local time;
`wInd3x-extra` reported haxed DFU running, parsed the image as Nano 3G,
received `dfuMANIFEST`, and reported `Image sent`.  The stopping point is the
panel-ID line and the visible color sequence in rows `A` through `D`.

Hardware reported panel ID `00 58 91 71`, detected as `TYPE 4` (`58xx`), and
row `A` displayed the intended red, green, blue, cyan, magenta, and yellow in
the labelled order.  This rules out both an RGB565 byte swap and a red/blue
channel swap on this unit.  The current P8b/high-byte-then-low-byte frame path
is correct for saturated colors; the earlier boot-splash appearance must be
treated separately from color ordering.

The exact full-safe ELF embeds
`apps/bitmaps/native/rockboxlogo.320x98x16.bmp` as a 320x240 dark-blue/gray
Apple-logo-and-`iPod` splash.  It is not the usual Rockbox wordmark despite
the source filename.  The generated RGB565 bytes in the deployed image match
that bitmap, so the unusual splash palette is now an asset/content issue, not
an LCD channel-order defect.  The one-shot color-probe configuration is
disabled again for subsequent bootloader builds; its implementation and
verified artifact remain available for regression use.

The next storage target can be derived without another broad map guess.  The
captured live root directory places `/.rockbox` at FAT cluster `0x000d9efc`.
With the verified 4096-byte FAT geometry, Disk Mode partition start `0x3f`,
and raw anchor `raw = 0x0001407e + 2 * absolute_4K_LBA`, its first directory
half is exact raw page `0x001c8dac`: logical hyperblock `0x0723`, offset
`0x1ac`, lane 4, bank 0, row `0x35`.  The older current context also names
logical hyperblock `0x0723` as an active log entry, which explains why the
dense-map-only reader cannot expose this directory.

A bounded host OOB scan was staged for bank 0, row `0x35`, exact raw key
`0x001c8dac`.  The apparent `05ac:1223` enumeration left by the visibility
probe did not service the first NAND control request: three 64-block groups
timed out before the scan was interrupted, and no body or OOB file was
produced.  This is the expected stale BootROM USB descriptor while the probe
loops before USB initialization, not a NAND result.  A physical reset into a
fresh responsive BootROM DFU instance is required before retrying the exact
read-only scan.

### Read-only Cabbiev2 storage view

After a physical reset, the owned Nano returned as responsive BootROM DFU
device `05ac:1223`, serial `87020000000001`.  Exact OOB-key searches then
resolved the small directory and skin working set without a general FTL
guess:

- `/.rockbox`, cluster `0xd9efc`, begins at raw page `0x001c8dac`;
  bank 0 physical block `0x1ac6`, row `0x35`, was its only exact type-`0x40`
  hit.  The captured page SHA-256 is
  `b1163e8cce603e6ebec679f7ff63858cc0640483d0b9fd5acaba526154bf5a43`.
- `/.rockbox/wps`, cluster `0xdad13`, begins at raw page `0x001ca9da`;
  bank 0 physical block `0x0e39`, row `0x3b`, was its only exact hit.  The
  captured page SHA-256 is
  `cc0a210a7cf4c7879aefeca23a9148ecbab37759fee181bc778c5443993e0760`.
- `/.rockbox/wps/cabbiev2`, cluster `0xdb06d`, begins at raw page
  `0x001cb08e`; the derived hyperblock mapping read it at bank 0 physical
  block `0x16a1`, row `0x11`.  The page SHA-256 is
  `27254f6d306d5385619eb3cb59586a49d59f30e258b036d569ef712c8d6ea527`.
- `cabbiev2.wps`, cluster `0xdb0c0`, begins at raw page `0x001cb134`;
  bank 0 physical block `0x16a0`, row `0x26`, was its exact hit.  Its first
  1685 bytes match the repository's installed Cabbiev2 WPS byte for byte,
  SHA-256
  `3169b66f8d4e9291de760ce01ef7b3ca941cf23c1ab15fde39e3b2425a2e5789`.

The Disk Mode relation is now verified at all four anchors:

`raw = 0x0001407e + 2 * absolute_4096_byte_LBA`

The Cabbiev2 directory proves that its nine files occupy a contiguous logical
region from cluster `0xdb06e` through `0xdb0c0`, all inside raw hyperblock
`0x001cb000`.  The individual FAT chains nevertheless terminate at each
file's true final cluster: backdrop `0xdb0a6`, lock `0xdb0a8`, battery
`0xdb0b0`, volume `0xdb0b6`, shuffle `0xdb0b7`, repeat `0xdb0b9`, playmode
`0xdb0bc`, progress bar `0xdb0bf`, and WPS `0xdb0c0`.

The safe application's controlled FAT view now exposes only the required
tree.  FAT entries 0 and 1 contain the required FAT32 reserved values; root,
directory, firmware, and each theme-file chain have explicit end markers.
The mutable `/.rockbox` directory is synthesized with only `.`, `..`, and
`wps`, so the mandatory later replacement of `/.rockbox/rockbox.ipod` cannot
invalidate this view by relocating its NAND page.  The WPS and Cabbiev2
directories plus all Cabbiev2 payload pages remain exact, checksum-guarded,
read-only native reads.

A sparse host FAT32 replay of this exact overlay was accepted by mtools.
`mdir` resolved `/.rockbox/wps/cabbiev2.wps` and every referenced bitmap, and
an `mcopy` readback of the WPS produced the exact SHA-256 above.  `fsck.fat`
also recognized the 4096-byte-sector geometry and all intended Cabbiev2
chains.  It reports the other entries retained in the captured WPS directory
as unallocated, as expected: those unrelated skins are deliberately outside
this bounded view.

The final safe theme build is in
`tmp/n3g-full-safe-theme-20260713/`:

- `rockbox.ipod`: 895256 bytes, SHA-256
  `42b96284924e6ce625c728de85efda643d1af87c033401bb25b819f89317b2f3`;
- `rockbox.bin`: 895248 bytes, SHA-256
  `c0200012286fe62012dae819d65c99ace2eb7f0726c3ad39b6e1f5a30d0fc8d7`;
- ELF SHA-256
  `6dc1fce987223b81a59d4ca76b11fb045cb751eb5676d032a6b360340f83a4ff`;
- map SHA-256
  `cf4cb10ef16c52fe52d4d811bc4cdb8cb390469b9e3f4131cf85f71f2239d8c8`.

The `nn3g` wrapper's stored big-endian checksum `0x05827bfc` equals model
seed 117 plus every body byte, and its body is byte-identical to
`rockbox.bin`.  Link and disassembly inspection again show all NAND page
program and block erase entry points returning failure, sector writes
returning `-1`, and no linked NOR write or erase routine.  The image is too
large for the 128-KiB haxed-DFU staging area.  Its next stopping point is
therefore a physical transition from responsive BootROM DFU into Apple Disk
Mode.  Deployment must replace and checksum-verify both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`, then rediscover the new firmware extent before a
checksum-guarded transient loader can be built.

Apple Disk Mode enumerated as `05ac:1262`, serial
`000A27001AF57313`, with its FAT32 volume mounted at
`/run/media/david/DAVID_S IPO`.  Before writing, both installed firmware
copies were preserved under
`tmp/n3g-pre-theme-deploy-20260713-1844/`; each retained the prior SHA-256
`bb51649118740cc8c70a55d1a91474d454c53a8306ff7bf206c6801202c6b333`.

The audited 895256-byte image was then copied to both required paths.  The
local artifact, `/rockbox.ipod`, and `/.rockbox/rockbox.ipod` all produced
SHA-256
`42b96284924e6ce625c728de85efda643d1af87c033401bb25b819f89317b2f3`
before `sync`.  After unmounting and remounting the volume read-only, both
device copies produced the same checksum again.

The FAT allocator gave both copies single, adjacent extents:

- `/rockbox.ipod`: clusters `0xde6d6..0xde7b0`, Disk Mode 512-byte LBAs
  `0x006f7388..0x006f7a5f`;
- `/.rockbox/rockbox.ipod`: clusters `0xde7b1..0xde88b`, Disk Mode LBAs
  `0x006f7a60..0x006f8137`.

`mshowfat` and root-authorized `hdparm --fibmap` independently reported the
same 219-cluster/1752-sector contiguous extents.  A final read-only
`fsck.fat` found no lost chains, cross-links, directory faults, or FAT-copy
disagreement; it reported only the volume's pre-existing dirty bit and left
the filesystem unchanged.  The root file's first exact raw key is therefore
`0x001d1d60`, raw-hyperblock offset `0x160`, bank 0, row `0x2c`; all 438
2-KiB firmware pages remain within raw hyperblock `0x001d1c00`.

The next stopping point is genuine BootROM DFU.  A bounded bank-0 OOB search
at row `0x2c` can resolve the new hyperblock's physical base, after which the
new body checksum/guard offsets and transient read-only loader can be built
without another persistent device write.

### Theme-image physical map and transient continuation

Fresh responsive BootROM DFU resolved the root firmware header at the sole
bank-0, row-`0x2c` type-`0x40` match for raw key `0x001d1d60`: physical block
`0x02f4`.  The captured first 2048 bytes exactly match the deployed
`rockbox.ipod`, including its `05 82 7b fc 6e 6e 33 67` header.  Bounded reads
at the three other bank-0 lanes then verified the complete physical-plane
formula:

- lane 0: bank 0, block `0x02f4`, raw `0x001d1d60`;
- lane 2: bank 0, block `0x02f5`, raw `0x001d1d62`;
- lane 4: bank 0, block `0x12f4`, raw `0x001d1d64`;
- lane 6: bank 0, block `0x12f5`, raw `0x001d1d66`.

All four exact pages have row `0x2c`, type `0x40`, and index `0xfe14`.
Together with the already proven adjacent-bank layout, the odd lanes are bank
1 at the corresponding blocks.  A full host BootROM bank-1 OOB search over
physical blocks `0x0000..0x1fff` returned no valid matching OOB page, exactly
reproducing the previously documented host bank-1 read limitation; it is not
a native-controller mapping result.  The transient loader uses
`nano3g_nand_diag_bank_read()`, whose native bank-1 path was already proven on
this device.

The current-file reader now targets synthetic host LBA `0x0070120e`, file
page `0x160`, raw hyperblock `0x001d1c00`, and base physical block `0x02f4`.
Compiled disassembly independently confirms the lane/bank/block arithmetic,
raw-key/type checks, and fail-closed zeroing path.  The exact application
guard requires body length `895248`, model checksum `0x05827bfc`, and FNV-1a
`0x83d7769e`; every guarded instruction and all seven transient read-only
patch targets match the preserved ELF and binary.

Prepared full-safe theme read-only-continuation loader:

- bootloader body: `74816` bytes;
- remaining 128-KiB haxed-DFU staging margin: `56256` bytes;
- bootloader SHA-256:
  `98c98d5bfb6ac2c6d5aac295b77f56b159d3db4a376ea61c77271a1c180b303c`;
- DFU image: `76864` bytes;
- DFU SHA-256:
  `6c2dbaf7bb6a12eddd19a5b21bd6f3ef12b55dbc21d499678b77173b6d52b5fe`;
- artifacts:
  `tmp/n3g-full-safe-theme-readonly-continuation-loader-20260713/`.

The Nano-3G format-2 wrapper has three exact `74816`-byte length fields,
2024 zero bytes between its header and body, the byte-identical loader body at
offset 2048, and no tail.  The loader links only the `nand_write_sectors()`
`return -1` stub; page program, program-start/collect, block erase, and NOR
write/erase routines are absent.  The application retains its independently
verified `return -1` sector-write stub and `return 1` page-program/block-erase
stubs, with no linked NOR write/erase path.

The owned Nano was still in genuine BootROM DFU (`05ac:1223`, serial
`87020000000001`).  The verified artifact was uploaded at 2026-07-13 19:11
ADT.  Haxed DFU entry, Nano-3G wrapper parsing, `dfuMANIFEST`, and `Image sent`
all completed successfully; the intervening libusb interrupted-event message
was non-fatal.  The stopping point is the eventual physical screen after the
deliberately slow exact-file read and read-only application entry.

### Forced iPodJS and one-transfer read test

The continuation reached the normal fallback Rockbox root menu after a long
delay.  Its boot splash retained the unusual colors, and iPodJS was not
active.  This result does not show that the Cabbiev2 payload failed: the
default `wps_file` is `cabbiev2`, but the default status-bar skin is `-`, and
the WPS is only exercised by the Now Playing screen.  The root menu therefore
was expected to retain its normal Rockbox appearance.

The bounded synthetic `/.rockbox` directory also intentionally exposes only
`.`, `..`, and `wps`.  It hides the live `config.cfg` and the `ipodjs` asset
directory.  Consequently `settings_reset()` retained the compiled
`UI_ENGINE_ROCKBOX` default and no saved iPodJS selection could be loaded.
This explains the reported fallback menu without implicating the iPodJS
renderer itself.

The next transient loader makes one additional, exact, checksum-guarded DRAM
patch.  It verifies the complete six-word `settings[]` record for
`global_settings.ui_engine` at body offsets `0x000a0e1c..0x000a0e30`, then
changes only its default word at `0x000a0e28` from Rockbox (`0`) to iPodJS
(`1`).  The installed firmware and configuration remain unchanged.  This
allows the compiled native renderer and its code-generated gradients to be
tested even while the real config and optional image assets remain outside
the synthetic filesystem view.

The exact-file reader formerly repeated all 438 NAND-page transfers solely
to prime and then capture the stage-spare registers.  The new reader repeats
only the first completed page on each bank, records that bank as primed, and
uses one transfer for every subsequent page.  Returned raw key/type metadata
is still validated on every page, and the complete 895248-byte application
still must pass its model checksum, FNV-1a, length, and instruction guards
before any patch or entry.  A bank reset clears the corresponding primed bit.

Prepared forced-iPodJS, faster read-only loader:

- bootloader body: `75040` bytes, SHA-256
  `e6d9106bc44d238130b0aa0cbd006c61acd88a30aef9c4dfe3d395c957bf867f`;
- remaining 128-KiB haxed-DFU staging margin: `56032` bytes;
- DFU image: `77088` bytes, SHA-256
  `8d8a2951f616e2e4190d1d331aa59d4518a78cb83a4c1703aa7e308e34d85f44`;
- artifacts:
  `tmp/n3g-full-safe-ipodjs-fast-readonly-loader-20260713/`.

The format-2 wrapper contains three exact `75040`-byte length fields, 2024
zero padding bytes, the byte-identical body at offset 2048, and no tail.
Disassembly confirms the eight guarded DRAM patches, the per-bank spare-prime
branch, the full application checksum gate, and the loader's only linked
write entry as the two-instruction `nand_write_sectors()` failure stub.
Page program, block erase, and NOR write/erase routines are absent.  The next
stopping point is a physical reset from the running menu into responsive
BootROM DFU, followed by the transient upload and observation of boot time,
the iPodJS root surface, and its procedurally generated colors.

The owned Nano entered responsive genuine BootROM DFU as `05ac:1223`, serial
`87020000000001`.  The verified forced-iPodJS artifact was uploaded at
2026-07-13 19:34 ADT.  Haxed DFU entry, Nano-3G wrapper parsing,
`dfuMANIFEST`, and `Image sent` completed successfully; the two libusb
interrupted-event messages were non-fatal.  The stopping point is the first
application screen and a comparison of its boot delay with the previous
two-transfer-per-page load.

### Similar-device boot-path correction

The hardware result was unchanged: the boot remained extremely long, the
splash palette still looked wrong, and the application eventually entered
the generic fallback Rockbox menu instead of iPodJS.  That result exposed two
separate application paths which the transient settings patch and loader-only
NAND optimization could not affect.

The native 320x240 iPodJS dashboard in `apps/root_menu.c` was compiled only
for `IPOD_VIDEO` and `IPOD_6G`.  Nano 3G therefore always reached the generic
`do_menu()` branch even when `global_settings.ui_engine` was forced to
`UI_ENGINE_IPODJS`.  The deployed ELF contained only the non-native
`root_menu_ipodjs_native_screen_active()` stub and no dashboard/WPS renderer.
Nano 3G has the same 320x240, 16-bit color UI geometry and scroll-wheel input
class as Video/Classic, so every native dashboard/WPS compile and dispatch
guard now includes `IPOD_NANO3G`.  The new ELF contains the real native-screen
entry at `0x0802432c` and `ipodjs_video_wps()` at `0x08021628`.  Nano 3G also
uses `UI_ENGINE_IPODJS` as its compiled default; other targets retain their
existing Rockbox default.

The dominant boot delay was in application `ftl_init()`.  The Nano 3G WMOUNT
path searched 3584 physical blocks at four candidate pages each, performing
14336 raw OOB reads before mounting.  None of that discovery output is used by
the current read-only synthetic view, which already carries exact mappings for
the MBR/FAT/root, firmware extent, and bounded theme payload.  In comparison,
Nano 2G opens indexed VFL/FTL metadata instead of rediscovering a map by raw
sweep, and the Nano 3G transient bootloader already uses the constant-time
`ftl_n3g_physrb_mount()` path.  The exact-read application now uses that same
mount directly.  Its own NAND reader also contains the proven per-bank spare
priming optimization, so only the first completed page on each bank is
repeated; subsequent pages use one transfer.

The color result was still confounded by two Nano-3G-only differences.  The
application replayed the complete detected-panel power/gamma initialization
after the bootloader had already initialized it.  Both the original Nano 3G
port and the working iPod 6G restrict that sequence to the bootloader, so the
application now preserves the handoff and configures only its interface/DMA
state.  Also, the repository's shared file named
`rockboxlogo.320x98x16.bmp` is actually custom 320x240 Apple/iPod artwork.
Nano 3G now selects the canonical upstream 320x98 Rockbox splash, SHA-256
`5dd329e99beb8178e21013ea9c9ca65c4a4f563d2e0485de8fdecb8086200857`,
while Video/Classic keep the custom shared asset.  This makes the next color
observation comparable to the original port.  The previous saturated-color
probe already proved row A, ruling out RGB565 byte and red/blue channel swaps.

The prepared application is in
`tmp/n3g-full-safe-ipodjs-exactmount-20260713/`:

- `rockbox.ipod`: 842540 bytes, SHA-256
  `0091804ea11a52b855c570edf82b661e25f787cd068680c26337674cf4ff1a59`;
- `rockbox.bin`: 842532 bytes, SHA-256
  `fcde49f6bc1db6b8c21a70cc3b7a28e14ee7b83e3f09c332034f5ba0f9e98680`;
- ELF SHA-256
  `dc9ec39b1a23ad8516964b765c5983fdea3c94dd848ddb81a84db5a5f9a8dce2`;
- map SHA-256
  `238e0e33d6f649b07dd1f4eff555e191c845f9166df48ebdafd7374815ab7825`.

The `nn3g` stored checksum `0x0550c12a` equals model seed 117 plus every
body byte, the wrapper body is byte-identical to `rockbox.bin`, and body
FNV-1a is `0xef4c6f36`.  Disassembly confirms that sector writes return `-1`,
all page-program/program-collect/block-erase entry points return failure, and
no NOR write/erase routine is linked.  The two Nano 3G replay suites pass all
18 tests.  A later clean-build attempt encountered an unrelated `tagcache.c`
edit made after this artifact; the successful `make bin` artifact and every
Nano-3G object above predate that unrelated compile failure.

This image changes size and cannot be loaded through the 128-KiB DFU staging
area.  The next stopping point is Apple Disk Mode.  Deploy the audited image
to both `/rockbox.ipod` and `/.rockbox/rockbox.ipod`, verify both hashes
against the local artifact before sync/eject and again after read-only
remount, then rediscover the new root-file extent before preparing a new
checksum-guarded transient loader.

### Exact-mount image deployment

Two connected iPods used the same `DAVID'S IPO` volume label during this
handoff.  The first auto-mounted device was the 477-GiB iPod 6G, serial
`000A27002101824D`; it was identified by size and serial before any write
succeeded and remained unchanged.  The Nano 3G subsequently enumerated as
Disk Mode `05ac:1262`, 3.6 GiB, serial `000A27001AF57313`.  Device identity,
not the shared volume label, is now the deployment discriminator.

Both existing Nano firmware copies were preserved in
`tmp/n3g-pre-exactmount-deploy-20260713-2002/`; each is 895256 bytes with
SHA-256
`42b96284924e6ce625c728de85efda643d1af87c033401bb25b819f89317b2f3`.
The audited 842540-byte image then replaced both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`.  The local artifact and both device paths matched
SHA-256
`0091804ea11a52b855c570edf82b661e25f787cd068680c26337674cf4ff1a59`
before `sync`.  After unmount and explicit read-only remount, both device
copies matched the same hash again.

The files were truncated in place.  The previously verified root extent
therefore retains first cluster `0xde6d6` and host LBA `0x0070120e`; the new
length consumes 206 contiguous 4096-byte clusters through `0xde7a3`, or 412
NAND pages.  Its expected first raw key remains `0x001d1d60` at row `0x2c`.
This is deliberately marked provisional until an exact DFU OOB hit returns a
body whose first bytes are `05 50 c1 2a 6e 6e 33 67` and whose payload matches
the new artifact.  The next stopping point is a physical reset into genuine
BootROM DFU for that bounded read-only scan.

### Exact-mount overwrite journal audit

Fresh BootROM DFU proved that the sole bank-0 row-`0x2c` page with raw key
`0x001d1d60` is still physical block `0x02f4`, but its body begins with the
previous theme image header `05 82 7b fc 6e 6e 33 67`.  An all-block scan found
no second match.  The in-place Disk Mode overwrite therefore did not replace
the previously documented constant physical extent.

The post-overwrite control journal contains three newer type-`0x43` contexts,
in decreasing-USN order:

- block `0x04c2`, page 3, USN `0xfffc8dba`;
- block `0x04c3`, page 6, USN `0xfffc8da0`; and
- block `0x14c2`, page 9, USN `0xfffc8d86` (newest).

The middle context names logical hyperblock `0x0747`.  Its current type-`0x45`
page is block `0x14c2`, page 4, index 4, SHA-256
`62cf1bca28c61aa04d5a3e070cb340c55e74f29e6b7513f634e0b344b116443f`.
It has 672 non-erased entries: logical offsets `0x160..0x3ff` contain values
`0x000..0x29f`.  This exactly records a sequential rewrite beginning at the
root file's provisional first page, but neither possible direction of that
table resolves a live header: complete bank-0 scans at physical offsets zero
and `0x2c0` found no `0x001d1d60` page.

The newest context has advanced to logical hyperblocks `0x0748` and `0x0749`.
Its bank-0 type-`0x44` half is block `0x04c2`, page 7.  Page-zero OOB capture
for all 1023 valid entries finds only the prior odd-lane generation for the
root hyperblock (`v=0x014c`, physical block `0x02f5`, raw `0x001d1c02`, user
USN `0x0000fe14`).  A complete primed bank-0 row-zero index likewise finds
only the old four-lane base at blocks `0x02f4/0x02f5/0x12f4/0x12f5`.
Scanning every row of those blocks and the neighbouring current-USN
hyperblocks finds no additional `0x001d1d60` page.

The DFU dump helper now repeats the first requested OOB page after controller
setup and discards the priming result, matching the native Nano 3G spare-path
rule.  This closes the possibility that a chunk-boundary page was omitted;
the 18 offline replay tests still pass.

Do not build a continuation loader against `0x02f4`: it would load the old
theme application again.  The next authoritative checkpoint is a fresh Apple
Disk Mode read after this power cycle.  If Disk Mode still returns the new
SHA-256, delete and recreate both required firmware paths rather than
truncating them in place, sync and unmount, then rediscover their new extents.

### Exact-mount delete-and-recreate deployment

Fresh Apple Disk Mode again returned the exact-mount image from both required
paths.  Two iPods were mounted under the same volume label, so every write was
guarded by device identity: the selected 3.6-GiB Nano 3G was serial
`000A27001AF57313` at `/dev/sda1`; the 477.3-GiB iPod 6G, serial
`000A27002101824D` at `/dev/sdb1`, was excluded and left unchanged.

Both Nano copies were backed up under
`tmp/n3g-pre-recreate-deploy-20260713-211305/`; the root and `/.rockbox`
backups are each 842540 bytes with SHA-256
`0091804ea11a52b855c570edf82b661e25f787cd068680c26337674cf4ff1a59`.
The installed files were first renamed in place so their old clusters remained
allocated while two genuinely new paths were created.  The local artifact,
`/rockbox.ipod`, and `/.rockbox/rockbox.ipod` matched that same SHA-256 before
sync, after an explicit read-only unmount/remount, and again before the final
sync.  Only then were the two temporary on-device names removed.  The Nano
volume is synced and unmounted; the repository backups remain.

A root-directory entry and FAT1 captured directly from the unmounted Nano
prove that `/rockbox.ipod` now starts at cluster `0x000dea28`, not the stale
`0x000de6d6`.  Its 206-cluster chain is contiguous through `0x000deaf5`.
With this volume's 4096-byte sectors, one sector per cluster, first data sector
1886, and 63-sector partition start, the new extent is:

- partition-relative 4096-byte LBAs `0x000df184..0x000df251`;
- Disk Mode 512-byte LBAs `0x006f8e18..0x006f9487` (including final cluster
  padding);
- synthetic host LBAs `0x00702c9e..0x0070330d`;
- first raw key `0x001d2404`, raw hyperblock `0x001d2400`, file-page offset
  `0x004`, bank 0, row 0;
- 412 2-KiB data pages ending at raw key `0x001d259f`.

The read-only capture is in
`tmp/n3g-post-recreate-diskmode-20260713-211305/`.  FAT1 SHA-256 is
`3c20c59be3dee3aac07c550b85bf414b33f46bb43833f7ba4ab05a8e9d9996b1`;
the captured root directory cluster SHA-256 is
`19355fe423e643ab8f9639488db21cc803e751d3681c80a832cb93434db30d76`.

The previous full row-zero index saw raw-hyperblock lanes `0x001d2400` and
`0x001d2402` at physical blocks `0x03b8/0x03b9`, with the corresponding
lane-4/6 blocks at `0x13b8/0x13b9`.  That makes block `0x13b8`, row 0 a useful
first probe for raw key `0x001d2404`, but it is not yet authoritative after
the new write.  Do not update or upload the exact reader until fresh BootROM
DFU returns the expected header `05 50 c1 2a 6e 6e 33 67` and the reconstructed
842540-byte image matches the audited SHA-256.

### Delete-and-recreate journal resolution

Fresh BootROM DFU rejected the provisional block `0x13b8`: its row-zero OOB
still names raw key `0x001d2404`, but user USN `0xfe1f` and its unrelated body
identify a stale generation.  A complete primed bank-0 row-zero index found
the current header instead at block `0x0788`, row zero, user USN `0xfe23`.
Its first 2048 bytes are byte-identical to the audited exact-mount image,
including header `05 50 c1 2a 6e 6e 33 67`.

The recreated extent is a scattered FTL log rather than a contiguous
hyperblock.  Full row scans of blocks `0x0788`, `0x0789`, `0x1788`, and
`0x1789` resolve every one of the 206 even raw keys from `0x001d2404` through
`0x001d259e`, with no missing or duplicate keys.  Every accepted entry has
user USN `0xfe23` and type `0x40`.  The adjacent odd raw key uses bank 1 at
the same physical block and row, matching the already-proven Nano-3G lane
pairing; BootROM's host bank-1 body path remains unsuitable, so the transient
loader uses the native bank reader.  Twenty-nine captured bank-0 body pages,
including the header, compare exactly with their corresponding local image
pages; the complete application checksum, FNV-1a, length, model tag, and
instruction guard must still pass before image entry.

The exact pair table contains 206 physical-page values and covers all 412
2-KiB application pages.  During the pre-upload audit, the synthetic root
directory was also found to retain the prior cluster `0xde6d6`.  It now names
the recreated cluster `0xdea28`, its FAT chain runs through `0xdeaf5`, and the
derived first host LBA `0x00702c9e` is identical to the scattered reader's
start.  The stale-root build was never uploaded.

Prepared scattered-log, read-only continuation loader:

- bootloader body: `75712` bytes, SHA-256
  `6934a88d7e15802184e60ae41e28525da37b8aa38e9c44976cf70d4d7438dda6`;
- remaining 128-KiB haxed-DFU staging margin: `55360` bytes;
- DFU image: `77760` bytes, SHA-256
  `89af6036dbf34d55eeaa5fec5f76a092ce18578df5ccfb0347a8811edc8f40bb`;
- artifacts:
  `tmp/n3g-full-safe-ipodjs-scattered-readonly-loader-20260713/`.

The `87021.0` format-2 wrapper has three exact `75712`-byte length fields,
2024 zero header-padding bytes, the byte-identical bootloader body at offset
2048, and no tail.  A static gate matched the compiled 206-entry table to the
four OOB manifests, decoded the compiled root entry and FAT boundary, checked
all application guard words and checksums, and confirmed that the loader's
only linked NAND write entry is the two-instruction `return -1` stub.  Page
program, block erase, and NOR write/erase entry points are absent.  Both
Nano-3G replay suites still pass all 18 tests.

The owned Nano enumerated as responsive genuine BootROM DFU `05ac:1223`,
serial `87020000000001`.  The verified artifact was uploaded at 22:07 ADT on
2026-07-13.  Haxed DFU entry, Nano-3G image parsing, `dfuMANIFEST`, and
`Image sent` all completed successfully; the intervening libusb interrupted
event messages were non-fatal.  The stopping point is the required physical
screen observation of the checksum-guard screen, boot timing, splash colors,
and first application UI.  No persistent write path was enabled or invoked.

Hardware reported that this build is "a lot better and faster."  That closes
the scattered-file lookup and synthetic-FAT correction: the complete current
image passed its checksum guards and the exact-mount application removed the
dominant boot delay.  Display colors remain "very wrong," however.  The color
fault is therefore independent of the old application mount sweep and stale
firmware extent.  The next bounded discriminator should operate only on LCD
pixel packing/transfer state while retaining this now-proven storage path.

### Native P9 LCD transfer discriminator

The later canonical Rockbox splash still had very wrong colors, so the prior
asset explanation is superseded.  The saturated row-A probe ruled out only
coarse byte and red/blue swaps; saturated primaries cannot validate the
controller's intermediate-bit expansion.

Comparison with the original Nano 3G port and the shared Nano 4G/iPod 6G LCD
path found one Nano-3G-only workaround in the current tree.  Commit
`9c076ce768` had replaced the target's declared 9-bit frame mode, 16-bit DMA,
and native RGB565 words with `LCD_MODE_P8b`, 8-bit DMA, and software
high-byte/low-byte streaming.  The original Nano 3G contract is
`LCD_MODE_P9` (`0x81100db8`), described in-tree as two panel transfers per
RGB565 pixel.  Nano 4G and iPod 6G likewise leave pixel serialization to the
LCD controller and feed it 16-bit source words.  The P8b workaround can retain
the identities of saturated primaries while losing the 9-bit transfer mode's
intermediate color-bit placement, which matches the hardware symptom.

The working tree now restores the original P9/16-bit/native-word path.  A
visibility-only bootloader renders a 16-step neutral grayscale ramp, separate
red/green/blue ramps, and six muted RGBCMY midtones, then loops before IRQ
enable or any storage entry.  Prepared artifact:

- bootloader body: `30944` bytes, SHA-256
  `22b08d6df9cf8fa20a19b28fe3912d545acb722128198e1f0c1483b1dc857ef3`;
- remaining 128-KiB haxed-DFU staging margin: `100128` bytes;
- DFU image: `32992` bytes, SHA-256
  `4d4769faf00088c45738367015a3c9b7f77b53bea45b97f95ace501d8a048b3c`;
- artifacts: `tmp/n3g-lcd-p9-ramp-probe-20260713/`.

The `87021.0` format-2 wrapper has three exact `30944`-byte length fields,
2024 zero padding bytes, the byte-identical body at offset 2048, and no tail.
Disassembly shows the permanent probe loop at `0x22021138`; storage init and
NAND/NOR write or erase symbols are absent.

The owned Nano was already in responsive genuine BootROM DFU `05ac:1223`,
serial `87020000000001`, state 2.  The audited probe was uploaded at 22:27 ADT
on 2026-07-13.  Haxed DFU entry, Nano-3G parsing, `dfuMANIFEST`, and
`Image sent` completed successfully.  The owner reported that all colors on
the P9 probe looked good.  This hardware-validates the neutral grayscale,
RGB ramps, and muted midtones, and isolates the bad-color regression to the
temporary P8b/8-bit/software-byte-stream path.  The validated
P9/16-bit/native-RGB565 path is now promoted to the full application build;
the probe remains in source as a disabled diagnostic.

### P9 full-application promotion

The promoted full application is archived at
`tmp/n3g-full-safe-ipodjs-p9-20260713/`.  Its `843436`-byte `rockbox.ipod`
has SHA-256
`5d38e548cafa92e43f6afe59f86af5e29925e63d6f719e0ff7d6f1d26f240085`.
The model-117 checksum `0x0550a794` matches the header, and the scrambled
body is byte-identical to the `843428`-byte `rockbox.bin` (SHA-256
`8051c0df02bcceee5bcf8cb8391c29480a0621ca2db160349357db552e66f082`).
Both Nano 3G replay suites still pass all 18 tests.  The application target
itself builds successfully; the wider all-plugin target later stops in the
unrelated Puzzles packaging step because its `sgt-blackbox.map` output
directory is absent.

The archived image retains the constant-time exact physical read-only mount,
hard-failed NAND writes/erase, native iPodJS default, and the workspace's
pending native-plugin read retry.  The color-probe flag is disabled.  It was
deployed in Apple Disk Mode to both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod` on the owned Nano serial `000A27001AF57313`; both
copies matched the local SHA-256 before sync and again after a read-only
remount.  Direct Disk Mode block readback also reproduced both complete files
and found only the expected 340 zero bytes in each final allocation cluster.
The 477-GiB iPod 6G serial `000A27002101824D` remained explicitly excluded.

### P9 deployed-image exact map and loader

The root copy now occupies FAT32 clusters `0xdebd0..0xdec9d`, 206 contiguous
4-KiB clusters for the 843436-byte image.  Its first synthetic 512-byte host
LBA is `0x007039de`; its 412 native 2-KiB pages use raw keys
`0x001d2754..0x001d28ef`.

Fresh BootROM DFU OOB scans resolved the extent in two log generations.  Raw
keys below `0x001d2800` use user USN `0xfebf`: rows 106 through 127 of
physical blocks `0x0f20`, `0x0f21`, `0x1f20`, and `0x1f21`.  Keys from
`0x001d2800` through the end of the root file use USN `0xfeb9`: rows 0 through
29 of blocks `0x0238`, `0x0239`, `0x1238`, and `0x1239`.  The bank-0 scans
cover all 206 even raw keys without gaps or duplicates.  Each adjacent odd
key remains the bank-1 member of the same physical-page pair, as established
by the earlier native-reader hardware tests.

The exact reader, synthetic root entry, and synthetic FAT boundary now encode
that deployment.  The loader also guards the complete immutable application
body (`843428` bytes, model checksum `0x0550a794`, FNV-1a `0xf1a098ec`) and
all 12 application instructions used to prove entry and apply the seven
read-only continuation words.  In particular, the relocated NAND page-write
stub is guarded at body offset `0x00093ae4`; the loader's only linked NAND
write entry remains the two-instruction `return -1` sector stub, with page
program, block erase, and NOR write/erase routines absent.

Prepared transient loader:

- bootloader body: `75712` bytes, SHA-256
  `dce513d11ab37d5b245d2df5f513d5ba51c60d27dd33c62f129b071eae043e11`;
- remaining 128-KiB haxed-DFU staging margin: `55360` bytes;
- DFU image: `77760` bytes, SHA-256
  `5749fb3fe1547a3e329479ff5bffc1a17764257db32fbcb5886ac8fc057fa370`;
- artifacts:
  `tmp/n3g-full-safe-ipodjs-p9-exactmap-loader-20260713/`.

The `87021.0` format-2 wrapper has three exact `75712`-byte length fields,
2024 zero padding bytes, the byte-identical body at offset 2048, and no tail.
The compiled 206-entry table matches the formulas derived from all eight
physical blocks and both OOB manifests.  Both Nano 3G replay suites pass all
18 tests.  The owned Nano was in responsive genuine BootROM DFU
`05ac:1223`, serial `87020000000001`; the artifact was uploaded at 23:12 ADT
on 2026-07-13.  Haxed DFU entry, Nano-3G image parsing, `dfuMANIFEST`, and
`Image sent` all completed successfully.  The owner reported that the result
"looks great."  This hardware-validates the new exact extent, full
application entry, and P9 display path together.  Music playback and
systematic plugin loading remain separate, unproven bring-up stages.

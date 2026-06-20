# iPod Nano 3G NAND diagnostics

This file records confirmed findings from the Nano 3G NAND bring-up work.

## Confirmed observations

- The S5L8702 FMC base used by the current diagnostics is `0x38a00000`.
- The Nano 3G BootROM NAND page-read helper at `0x20009910` hangs when called from the relocated Rockbox bootloader context.
- The BootROM helper calls used by wInd3x for I-cache disable, clockgate enable, and NAND GPIO setup also hang when called from the relocated Rockbox bootloader context.
- `nand_device_init()` must not pre-init all four banks during the diagnostic path; bank init is deferred to the single targeted read.
- Local bank reset succeeds after waiting for `FMCSTAT` bank-ready bits `0x1000 << bank`.
- The previous reset wait on `0x800000` timed out even though `FMCSTAT` showed `0x000c3000`.
- With bank-ready reset, bank 0 init reports `init_rc=0`, `stage=0`, `reset_stat=000c3000`.
- Local FMC `READID` and page FIFO reads work in build `20260503p` when Nano 3G bootloader PMU preinit is skipped and no PMU-write variants run before the page sample.
- Nano 3G spare/OOB metadata is exposed through FMC stage registers
  `FMSTAGE0..2` after a page transfer. It is not available by reading NAND
  column `0x800`; that local column read returns fake `0x30303030` bytes.
- FMCTRL0 timing/bank-select variants tested so far all returned zero IDs.
- PMU NAND-related register variants tested so far all returned zero IDs.
- GPIO mux profile variants tested through `N3G_GPIOVAR` also returned zero IDs.
- Bootloader-side no-setup `N3G_DFU_STATE_ID` and local `READID` paths returned zero IDs after DFU upload, so the working wInd3x NAND state is not surviving into the relocated Rockbox image.
- wInd3x DFU-mode `nand identify 0`, run outside Rockbox, successfully reads JEDEC manufacturer `0x2c` and device bytes `0xd5 0xd5 0xa5`.
- wInd3x DFU-mode `nand read 0 ... 0 1` successfully reads bank 0 page 0 as non-zero/non-erased data; first 0x1c bytes are `0xff`, followed by bytes beginning `58 64 c6 84 bb b4 b0 22`.
- wInd3x DFU-mode `nand readextra 0 0 ... 1` successfully reads the 0x40-byte auxiliary/spare buffer as non-zero/non-erased data.
- DFU-side `nand state 0` reports `id=a5d5d52c`, `fmctrl0=00043803`, `fmcstat=08041008`, `pcon8=22222222`, and `pcon9=00020002`.
- DFU-side grouped state capture additionally reports `pwr0=2007eae2`, `pwr1=00018ff7`, `clk0=00201000`, `clk1=00404101`, `misc=00000000`, `fmctrl1=00000302`, `fmanum=00000000`, and `fmdnum=00000007`.
- The matching DFU `fmctrl0`, `pcon8`, and `pcon9` values mean the current Rockbox bank-select/timing word and main GPIO mux values are not the remaining mismatch.
- The DFU `misc=00000000` value contradicts the previous Rockbox diagnostic guess that forced `N3G_PLATFORM_MISC` low bits to `1`.
- wInd3x clears command/address status bits (`0x2`, `0x4`) but leaves the data-ready bit (`0x8`) set before FIFO flush/read. The Rockbox diagnostic had been clearing `0x8`; build `20260503d` changes data-ready waits to leave `0x8` latched.
- Build `20260503d` still returned broken NAND access on-device.
- Build `20260503g` was screen-compact and still returned all-zero IDs/data on-device.
- Build `20260503h` preserved BootROM-style clocks/syscon and still returned all-zero IDs/data on-device.
- Build `20260503i` added a CP15 I-cache/D-cache disable variant around a local `READID`; the device still reported all-zero IDs/data.
- Build `20260503j` matched DFU `misc=0` and removed broad power-mask clears; the device still reported all-zero IDs/data.
- Build `20260503k` showed NAND access works at loaded-image entry after minimal setup: entry `r0=00000000`, `r1` read as the real NAND ID (`a5d5d52c`, seen on-screen as `65d5c52c`/similar), `rc=-1/0`, `st=00000000/08041008`. The later storage-path probe still returned zeros, so a bootloader initialization step after entry breaks or hides the working FMC path.
- Build `20260503l` showed staged probes ran at all stages (`seen=000003ff`), but only stage 0 succeeded (`ok=00000001`, first bad stage 1). The last failed probe had `rc=-1` and `st=08041008`, meaning the data-ready bit from the previous successful read remained latched and the next `READID` command never completed.
- Build `20260503m` fixed the stale data-ready latch enough for stage 1 to succeed (`ok=00000003`). The first bad stage moved to stage 2, immediately after `system_preinit()`. The failed read completed (`rc=0`) but returned `00000000`, so the remaining break is inside `system_preinit()`, not stale status handling.
- Build `20260503n` expanded `system_preinit()` substages and showed NAND ID reads survive through stage 13 (`ok=00003c03`) but fail at stage 14, after `pmu_preinit()`. This identifies Nano 3G PMU preinit as the step that turns real NAND ID reads into zero FIFO data.
- Build `20260503o` skipped `pmu_preinit()` and all staged ID probes succeeded (`seen=0000ffff`, `ok=0000ffff`, no bad stage). The final staged ID remained real NAND ID data (`last=65d5c52c` on screen, equivalent to the expected `a5d5d52c` byte ordering). This confirms the guessed Nano 3G PMU preinit writes are the NAND-breaking step.
- Build `20260503p` kept the PMU preinit bypass and removed the active PMU-write variant loop before page sampling. The device reported `N3G_NAND_ACCESS_FIXED`, confirming real Rockbox-side NAND data access.

## Current diagnostic direction

The active diagnostic avoids the hanging BootROM page-read helper and uses local FMC command/FIFO paths only. Build `20260503i` keeps the compact screen output, preserves BootROM clock/syscon setup, and adds one CP15 cache/MMU-control variant. It suppresses per-variant and register spam and prints only:

- `N3G_BUILD 20260503i`
- `N3G_MIN_START`
- `N3G_IDS`
- `N3G_ID2`
- `N3G_ID3`
- `N3G_ID4`
- `N3G_SMP`
- `N3G_DAT`
- `N3G_CNT`
- `N3G_NAND_ACCESS_FIXED` or `N3G_NAND_ACCESS_STILL_BROKEN`
- `N3G_MIN_DONE`

The wInd3x control tests confirm the NAND hardware and BootROM DFU payload path can read both ID and page data. The remaining issue is reproducing the required low-level state from inside the relocated Rockbox bootloader context without calling BootROM helpers that hang there.

The next diagnostic step is DFU-side state capture using the local wInd3x `nand state` command. It runs the known-good BootROM NAND init and identify path, then reports the ID plus a trimmed register group as `N3G_DFU_STATE ...` lines. The first full register payload was too large for the Nano 3G DFU exploit buffer (`1204 > 1024`), so the command now accepts small groups:

- `nand state 0 0`: ID, `FMCTRL0`, `FMCSTAT`, `PCON8`, `PCON9`.
- `nand state 0 1`: ID, power gates, clock gates.
- `nand state 0 2`: ID, platform misc and core FMC transfer count/control registers.
- `nand state 0 3`: ID, current FMC command/address registers.

Build `20260503f` added clock-divider ID variants. Build `20260503h` goes earlier and prevents the Nano 3G bootloader from running Rockbox `syscon_preinit()`/`clocking_init()`, so the NAND probe runs closer to the BootROM/wInd3x clock state.

Build `20260503i` tests whether the Rockbox CP15 cache-control state affects FMC FIFO reads by temporarily disabling I-cache and D-cache around one local `READID`, then restoring CP15 control. It did not recover NAND access.

Build `20260503j` changes the Nano 3G FMC setup to better match captured DFU state: it forces the platform misc low bits to `0` instead of `1`, and stops clearing broad `PWRCON(0)`/`PWRCON(1)` masks. The diagnostic still explicitly enables only the NAND and NAND ECC clock gates before local ID/page reads.

Build `20260503k` adds an early-entry bounded `READID` probe before `system_preinit()`, `memory_init()`, and BSS clearing. It stores the result in initialized data and later prints:

- `N3G_ENT r0=<entry_id_before_setup> r1=<entry_id_after_min_setup> c=<entry_state>`
- `N3G_ENT2 rc=<before_rc>/<after_rc> st=<before_stat>/<after_stat>`

This determines whether NAND access is already broken at loaded-image entry or breaks during Rockbox initialization.

Build `20260503l` adds staged bounded `READID` probes after each major bootloader initialization step while reapplying the same minimal NAND setup each time:

- stage 0: after `i2c_preinit(0)`
- stage 1: after hibernation/ONB check
- stage 2: after `system_preinit()`
- stage 3: after `memory_init()`
- stage 4: after `bss_init()`
- stage 5: after `system_init()`
- stage 6: after `kernel_init()`
- stage 7: after `i2c_init()`
- stage 8: after `power_init()`
- stage 9: immediately before `storage_init()`

It prints `N3G_STG seen=<mask> ok=<mask> bad=<stage>` and `N3G_STG2 last=<id> rc=<rc> st=<stat>` to identify the first initialization step where the recoverable `READID` path stops working.

Build `20260503m` changes the local direct-FMC read paths to acknowledge the data-ready status bit (`0x8`) only after the FIFO has been read. It also clears any stale data-ready bit before starting a new direct `READID`. This preserves the wInd3x ordering for the active transfer while preventing one successful read from poisoning the next command.

Build `20260503n` expands the same `N3G_STG` mask with system-preinit substages:

- stage 10: after preserved-clock/syscon step
- stage 11: after `gpio_preinit()`
- stage 12: after `i2c_preinit(0)` inside `system_preinit()`
- stage 13: after `pmu_is_hibernated()`
- stage 14: after `pmu_preinit()`
- stage 15: after `miu_preinit()`

The same compact `N3G_STG` and `N3G_STG2` lines identify the first substep that turns real NAND ID reads into zero data.

Build `20260503o` skips `pmu_preinit()` in the Nano 3G bootloader diagnostic path. This is a read-only NAND bring-up test to confirm whether bypassing the guessed PMU LDO/voltage writes keeps direct FMC reads alive through `storage_init()`.

Build `20260503p` keeps the PMU preinit bypass and also removes the old active PMU-variant ID loop from the compact diagnostic path. Those PMU variant writes are now known unsafe for NAND bring-up and could poison the following page-read sample. The active diagnostic is back to read-only discovery for NAND/FTL purposes.

Build `20260503q` returns to Whimory metadata discovery now that raw NAND reads are real. It performs a compact read-only OOB scan over the end of NAND:

- reads only the spare/OOB area from page 0 of each sampled block
- scans all banks, max `2048` bank/block reads, newest/end blocks first
- prints progress as `N3G_OOB_SCAN_PROGRESS block=<blk>`
- prints only known Whimory types `0x40`, `0x41`, `0x43`, `0x44`, `0x46`, `0x47`, `0x80`
- prints `N3G_FTL_CTX_CANDIDATE`, `N3G_FTL_MAP_CANDIDATE`, type counts, and `N3G_OOB_SCAN_DONE`

It does not run devinfo scan, mapping decode, sector reads, or PMU-write tests.

Build `20260503q` found no known Whimory OOB type counts and reported `N3G_OOB_CONTEXT_NOT_FOUND`.

Build `20260503r` keeps the OOB-only, read-only scan but no longer assumes the marker is on page 0 of each block. It scans the first `4` pages per block in the same end-of-NAND direction, with the same hard limits: max `2048` OOB reads and max `64` hits printed.

Build `20260503r` also found no known Whimory OOB type counts and reported `N3G_OOB_CONTEXT_NOT_FOUND`.

Build `20260503s` stops broad scanning and verifies the OOB access path itself. It compares direct body reads against direct spare-column reads (`offset=0x800`) for pages `0`, `1`, `128`, and `1024` on each bank, printing compact `N3G_OOB_PATH ...` lines with body/OOB nonzero and nontrivial counts. This should show whether the direct spare-column path is returning real OOB bytes before any further metadata scan.

Build `20260503s` output was too wide to reliably read on the device screen, but it confirmed bank 0 and bank 1 body reads have nonzero/nontrivial data. Build `20260503t` keeps the same bounded sample and compresses it to per-bank summary lines:

- `N3G_OOB_PATH_BANK b=<bank> bh=<body_hits> oh=<oob_hits> bf=<body_fail> of=<oob_fail>`
- `N3G_OOB_PATH_FIRST b=<bank> bp=<first_body_page> op=<first_oob_page> o0=<last_oob_w0> o1=<last_oob_w1>`

Build `20260503t` showed OOB hits are real for at least some entries (`bh=4`, `oh=4`), while later entries can have `oh=0`. This means direct spare reads work, but known Whimory type markers are not at the previously assumed OOB byte offset/region.

Build `20260503u` probes OOB layout on the same tiny bounded sample. For each bank it prints:

- `N3G_OOB_LAYOUT b=<bank> p=<sample_page> tpos=<first_known_type_byte_offset> t=<type> mask=<offset_mask>`
- `N3G_OOB_WORDS b=<bank> w0=<...> w1=<...> w2=<...> w3=<...>`

This identifies whether marker-like bytes exist elsewhere in the 64-byte spare area before changing the broad scanner.

Build `20260503u` reported bank 0 and bank 1 sample page 0 with spare words
`0x30303030`, no known marker byte (`tpos=0xffffffff`), and banks 2/3 with no
sample page. This does not match the known-good DFU `readextra` behavior; the
diagnostic had been reading column `0x800` through the local FIFO path, while
wInd3x reads the BootROM's separate 0x40-byte auxiliary/extra buffer at
`0x22000000`.

Build `20260503v` changes `nano3g_nand_diag_local_read(..., offset=0x800, ...)`
to use the BootROM page-read helper's extra buffer, matching wInd3x
`nand readextra`. The active probe remains the compact OOB layout probe; the
goal is to verify real spare bytes before returning to broad OOB type scans.

Build `20260503v` froze while grabbing the OOB layout. This means the BootROM
extra-buffer helper is not safe to call from the loaded Rockbox bootloader
context, even though the same helper path works from the tiny DFU/wInd3x
payload.

Build `20260503w` backs the bootloader diagnostic away from BootROM extra
reads. It restores the non-hanging local spare-column read, limits the layout
probe to banks 0 and 1 and pages 0/1, and prints
`N3G_OOB_BOOTROM_EXTRA_UNSAFE` plus
`N3G_OOB_LAYOUT_DONE result=local_column_only` so the screen has a clear final
verdict instead of freezing mid-probe.

Build `20260503w` confirmed the local spare-column path is not real OOB: banks
0 and 1 returned `0x30303030`, no known marker bytes, and
`result=local_column_only`. The correct spare source remains the BootROM
auxiliary buffer, but that path must be used from DFU/wInd3x rather than from
the loaded Rockbox bootloader.

The local wInd3x tree now has `nand scanextra [start-block] [block-count]`.
It runs from DFU, reads the BootROM extra buffer for the first page of end-region
blocks, prints compact `N3G_DFU_OOB_HIT` lines for Whimory types
`0x40/0x41/0x43/0x44/0x45/0x46/0x47/0x80`, prints per-type counts, and stops at
`2048` reads or `64` printed hits.

`scanextra` now also accepts an explicit page-offset list and bank list:

- `@127` means only page offset 127.
- `@4,5,6` means those exact page offsets.
- The optional fourth argument limits banks, for example `0`.

The DFU-side scanner originally checked all spare bytes and produced false
`0x43` candidates from byte offset 4. Real user-page markers were consistently
at byte offset 9 (`usn=ffff40ff`), so `scanextra` now treats offset 9 as the
OOB type byte.

DFU scan results:

- `scanextra 8191 512`: last 512 blocks, page 0, found `type=40 count=330`,
  no `0x43/0x44/0x80`.
- `scanextra 8191 128 4`: last 128 blocks, pages 0-3, found no known types.
- `scanextra 8063 128 4`: blocks 8063-7936, pages 0-3, found only
  `type=40 count=312`.
- `scanextra 7679 512`: blocks 7679-7168, page 0, found only
  `type=40 count=504`.
- `scanextra 7167 512`: blocks 7167-6656, page 0, found
  `type=40 count=504` and one real block-map marker:
  `type=44 bank=0 block=6916 page=885248 off=9 first=fffccbe2,ffff0000`.
- `scanextra 6975 128 4`: blocks 6975-6848, pages 0-3, confirmed the same
  `type=44` marker at bank 0 block 6916 page 885248 and no nearby `0x43` or
  `0x80`.
- `scanextra 6916 1 128`: all 128 pages of bank/all-bank block 6916 found only
  the one `0x44` page at page 885248.
- `scanextra 6975 128 @4..31 0`, `@32..63 0`, `@64..95 0`, and `@96..127 0`
  found only `0x40` user data in the surrounding bank-0 range; no additional
  map/context/VFL metadata pages.

Later DFU scans identified the missing context pages. `scanmap` scans one page
offset across bank 0 and reports only compact metadata hits. A full page-offset
0 scan found three block-map generations and three erase-counter generations:

- current `usn=fffccbe2`: `type=44` at block `6916`, page `885248`; matching
  `type=46` at block `2820`, page `360960`.
- older `usn=fffccc16`: `type=44` at block `6166`, page `789248`; matching
  `type=46` at block `2070`, page `264960`.
- older `usn=fffccbfc`: `type=44` at block `5314`, page `680192`; matching
  `type=46` at block `1218`, page `155904`.

The map and erase-counter blocks are paired with a physical block delta of
`4096`. The current FTL context was then found by scanning all page offsets of
block `2820`: `type=43` at block `2820`, page `360963`, with
`usn=fffccbe2`.

`type=45` is now known to be a real Nano 3G Whimory metadata/control page type.
It was originally hidden by the known-type filter. Current `0x45` pages are:

- block `2820` page offsets `1`, `2`: indexes `0002`, `000a`.
- block `2821` page offsets `1`, `2`: indexes `0004`, `000c`.
- block `6916` page offsets `1`, `2`: indexes `0006`, `000e`.
- block `6917` page offsets `0`, `1`, `2`: indexes `0000`, `0008`, `0010`.

Targeted all-bank scans over blocks `6917..6916` and `2821..2820`, page offsets
`0..3`, found these metadata pages only on bank 0.

The `0x44` page body at bank 0 block 6916 page 885248 is a dense 1024-entry
little-endian `uint16_t` table. The extra data decodes as:

- `usn=fffccbe2`
- `idx=0000`
- `type=44` at byte offset 9
- first words `fffccbe2,ffff0000`

This is a real block-map page, but not enough by itself to reconstruct the
whole FTL map.

Additional DFU tools added:

- `nand findlpn [lpn] [start-block] [block-count] [pages|@page-list] [banks]`
  scans OOB user markers. The OOB raw field advances by `8` per 2048-byte NAND
  page; for Rockbox's 2048-byte logical sector model, `raw >> 3` identifies the
  logical page number. For 512-byte sub-sector reasoning, `raw >> 1` advances by
  four sectors per NAND page.
- `nand findsig [start-block] [block-count] [pages|@page-list] [banks]` reads
  only the body chunk containing offset 510 and looks for an MBR-style
  `55 aa` signature.
- The experimental `nand fastsig` device-side scan loop timed out and left DFU
  unable to accept normal commands until a manual DFU reset. It has been removed
  from the local wInd3x CLI/source; do not reintroduce it without first building
  a bounded single-page payload test.

Findings from the new tools:

- Earlier `findlpn 0` hits at bank 0 blocks 7939/7938 had raw OOB values
  `00000006` and `00000004`, but their page bodies were repeated test-looking
  patterns (`03 00 00 00 ...` and `02 00 00 00 ...`), not an MBR.
- `findsig 8191 8192 @0` found no `55 aa` signature on page offset 0 in the
  upper 2048 blocks scanned.
- `findsig 7950 32 128 0` found no `55 aa` signature in all pages of the
  small region around the raw low-LPN pages.
- `fastsig 0 1016192 128` timed out and put the DFU connection into a bad USB
  state (`LIBUSB_ERROR_OTHER` on later DFU scans/reset attempts).

The current FTL context body was dumped to
`tmp/n3g-ftlctx-b2820-p360963.bin` and its extra buffer to
`tmp/n3g-ftlctx-b2820-p360963.extra`. Important decoded fields:

- context extra: `e2 cb fc ff ... ff 43 ff ff`, so `usn=fffccbe2` and
  `type=43` at OOB byte offset `9`.
- context body starts with `usn=fffccbe2`, `nextblockusn=0000de0e`,
  `freecount=19`, `nextfreeidx=1`.
- the body contains vpage-like values `0x00160800..0x00160818`, including
  `0x00160800/00160801` near the erase-counter page area and
  `0x00160804/00160805` near the block-map page area, but direct Nano2G-style
  VFL translation does not yet map these to the observed physical pages without
  the Nano 3G VFL/remap state.

The current `0x44` block-map page body was dumped to
`tmp/n3g-map-6916-page885248.bin` and its extra buffer to
`tmp/n3g-map-6916-page885248.extra`. The body is a dense 1024-entry little-endian
`uint16_t` table with `min=2`, `max=1958`, no `0xffff`, no zero entries, and
1024 unique values. A simple Nano2G-style map read using `map[0]=0x0781`,
`ppb=512`, and `syshyperblocks=425` predicted bank 0 page `300288`, but that
page's OOB raw field is `0008e800`, not logical page zero. This means the
remaining blocker is Nano 3G VFL/remap interpretation or a Nano 3G-specific map
layout, not lack of real NAND data.

`nand identify 0` now prints the raw ID bytes as
`N3G_DFU_ID_RAW 2c d5 d5 a5 00 00 00 00 00 00 00 00 00 00 00 00`.
The little-endian ID is `0xa5d5d52c`. The Nano 2G NAND table already contains
an exact matching row:

- `{0xA5D5D52C, 8192, 7744, 0x80, 7, 3, 2, 2, 1}`

The Nano 3G table previously used guessed IDs only and hardcoded table index 0.
`nand-nano3g.c` now includes the exact `0xa5d5d52c` row first and selects the
device row by reading the actual ID once, falling back to that first row if the
ID read fails.

Additional map interpretation checks failed:

- Treating the `0x44` table as `logical_block -> physical_block` does not make
  OOB logical-page markers line up with the requested logical block.
- Treating the table inversely as `physical_block -> logical_block` also does
  not line up.
- Segment/high-block offset tests for `map[0]` across pages `245888`,
  `300288`, `507904`, `562304`, `770048`, `824320`, and `1032192` did not
  produce logical page zero.
- Page-offset-0 `scanmap` over banks `1`, `2`, and `3` found no metadata hits,
  so the current page-offset-0 metadata generation is bank-0 anchored.

The current `0x45` metadata/control pages have valid OOB markers, but their page
bodies mostly read as erased (`0xff`). They are not additional dense map pages;
the dense body found so far is still the current `0x44` block-map page.

BootROM disassembly of helper `0x20009910` confirmed the DFU extra-buffer
behavior: the helper performs the normal page transfer, then stores
`FMSTAGE0`, `FMSTAGE1`, and `FMSTAGE2` into the caller's extra pointer. Rockbox
now mirrors that behavior in the local read path by synthesizing the first three
spare words from the stage registers after `n3g_internal_page_xfer_bank()`.
Build `20260503y` changes the compact bootloader diagnostic to print
`N3G_OOB_STAGE_REGISTER_SPARE` and
`N3G_OOB_LAYOUT_DONE result=stage_registers`.
Build `20260503z` also reads the known current metadata pages directly through
that spare path and prints `N3G_OOB_KNOWN` / `N3G_OOB_KWORDS` for:

- block `6916`, page offset `0`, expected type `0x44`
- block `2820`, page offset `3`, expected type `0x43`
- block `2820`, page offset `0`, expected type `0x46`

On-device `20260503z` still printed zero stage words for the known metadata
pages. Comparing against BootROM helper `0x20009910` showed Rockbox was restoring
`FMCTRL0` to idle before reading `FMSTAGE0..2`, while BootROM reads the stage
registers first and restores `FMCTRL0` afterward. Build `20260504a` captures
`FMSTAGE0..2` immediately after the page transfer completes and only then
returns the controller to idle.

On-device `20260504a` still showed zero stage words. A closer BootROM comparison
found the missing extraction step: before reading `FMSTAGE0..2`, BootROM writes
`FMSTAGECMD=0x5140`, writes `FMSTAGECTRL=2`, waits for bit `2` to clear, and
then copies `FMSTAGE0..2`. Build `20260504b` adds that bounded extraction step.

On-device `20260504b` produced nonzero stage words for known metadata pages, and
`w0=00fccbe2` confirmed the current USN is present. However the stage words did
not match the DFU extra-buffer layout (`type` still decoded as `0x00`). Another
BootROM loop difference was found: after each `0x8` stage/data-ready wait inside
the page-transfer loop, BootROM acknowledges `FMCSTAT=0x8` before continuing.
Build `20260504c` mirrors those two acknowledgements.

On-device `20260504c` matched the DFU extra-buffer layout for known metadata:

- block `6916`, page offset `0`: `w0=fffccbe2`, `w1=ffff0000`,
  `w2=ffff44ff`, type `0x44`.
- block `2820`, page offset `3`: `w0=fffccbe2`, `w1=ffffffff`,
  `w2=ffff43ff`, type `0x43`.
- block `2820`, page offset `0`: `w0=fffccbe2`, `w1=ffff0000`,
  `w2=ffff46ff`, type `0x46`.

This confirms Rockbox-side physical page body and spare/OOB reads are now
working. Build `20260504d` switches the compact bootloader diagnostic from
layout proof to metadata location over two bounded known ranges:
`7167..6656` and `3071..2560`, page offsets `0..3`, bank 0 only. It reports
`N3G_META_HIT`, `N3G_META_COUNT`, and `N3G_META_LOCATE_DONE`.

On-device `20260504d` found the current metadata using Rockbox-side spare/OOB:

- type `0x44` block map: block `6916`, page offset `0`, USN `fffccbe2`
- type `0x46` erase counters: block `2820`, page offset `0`, USN `fffccbe2`
- type `0x43` FTL context: block `2820`, page offset `3`, USN `fffccbe2`

The long `N3G_META_COUNT` lines were clipped on the iPod display before the
page field was visible. Build `20260504e` replaces those with short
`N3G_META_BEST t=.. b=.. p=.. u=.. n=..` lines and adds compact body summaries:
`N3G_CTX` for the FTL context header, `N3G_MAP` for the dense 16-bit block map
range, and `N3G_ERS` for the erase-counter/control page first words.

On-device `20260504e` confirmed:

- `N3G_CTX b=2820 p=3 u=fffccbe2 nx=0000de0e f=19 i=1`
- `N3G_MAP b=6916 p=0 mn=0002 mx=07a6 z=0 ff=0`
- the erase-counter page body still reads as `30303030` through this diagnostic
  path, so it is not useful yet.

Build `20260504f` keeps the same bounded locator and adds short FTL-context
pointer dumps: `N3G_CXT_MAP0`, `N3G_CXT_MAP1`, `N3G_CXT_ERA0`, and
`N3G_CXT_CTL`. This should show how the current physical metadata pages map to
Whimory's stored virtual page numbers before changing any mount logic.

On-device `20260504f` showed the context body is Nano3G-specific rather than a
straight Nano2G `struct ftl_cxt_type`:

- `N3G_CXT_ERA0 00160800,00160801,...`
- `N3G_CXT_CTL 0016,080b,0016 cp=0016080c cl=00160800`

Values shaped like `001608xx` decode naturally as page references:
`block = (raw >> 8) / 2`, `page = raw & 0xff`; for example `00160800`
decodes to block `2820`, page `0`, matching the located `0x46` page. Build
`20260504g` adds `N3G_REFSCAN` and `N3G_REF` lines that scan the context body
for these page-reference-shaped words, decode them, and read each referenced
page's OOB type/index. It also reports whether the physical map page block
`6916` appears as encoded reference `00360800` in the context body.

On-device `20260504g` showed the reference decode is valid for the control
block but too noisy when every erased page is printed. Useful hits included:

- `00160800` -> block `2820`, page `0`, type `0x46`
- `00160801` -> block `2820`, page `1`, type `0x45`

The surrounding `00160806..00160816` references pointed at erased pages
(`type=0xff`), so build `20260504h` now only prints reference hits whose OOB
type is a known metadata type (`0x43..0x47` or `0x80`). It also adds `N3G_MAPT`
lines for the first eight entries of the found `0x44` block map, applying the
current Nano2G VFL formula (`physical = map[i] + syshyperblocks`) and printing
the resulting OOB type and logical page number. This checks whether the located
map page can already address user data with the existing read formula.

On-device `20260504h` showed the first map entries do point to real user-data
pages with OOB type `0x40`, but their logical page numbers do not match the map
entry index:

- `map[0]=0781` -> physical block `2346`, type `0x40`, lpn `0008e800`
- `map[1]=066b` -> physical block `2068`, type `0x40`, lpn `0004ac00`
- later entries similarly point to valid user pages with unrelated LPNs.

This means the `0x44` page is not a simple `logical block index -> physical
block` table in stored order. Build `20260504i` keeps the first-eight
`N3G_MAPT` samples and adds a bounded inverse pass over all 1024 entries in the
page. It prints low logical-block hits as `N3G_MAPLOW`, then compact totals:
`N3G_MAPSCAN u=.. eq=.. minlb=.. j=.. v=.. l=..` and
`N3G_MAPLPN0 j=.. v=..`.

On-device `20260504i` found no LPN zero in the current map page, but did find
low logical blocks:

- `j=5`, `v=0405`, logical block `8`, page offset `0`, LPN `00001000`
- `j=27`, `v=057d`, logical block `6`, page offset `0`, LPN `00000c00`
- `j=569`, `v=01d4`, lowest seen logical block `4`

This suggests the visible filesystem may start at an FTL logical offset rather
than LPN zero, or the first map page is still a segment with reserved low
logical blocks absent. Build `20260504j` adds one focused scan of the best low
mapped block (`min_v`) across its pages. It prints `N3G_LOWPG` for low LPNs
under `0x2000`, including body word zero and the `0x1fe` signature, and
`N3G_MBR_CAND` if a `0xaa55` signature appears.

On-device `20260504j` found real very-low LPNs in the low mapped block, but no
`0xaa55` signature in the first printed candidates. Examples included LPNs
`2`, `0x0a`, `0x0b`, `0x12`, `0x13`, `0x1a`, and `0x1b`. Build `20260504k`
therefore widens only the low-LPN check: it remembers up to eight low map
candidates from `N3G_MAPLOW`, scans each candidate hyperblock for LPNs under
`64`, prints those as `N3G_LHIT`, and emits `N3G_LBEST` with the lowest LPN
seen. It still avoids a broad NAND scan and only follows blocks already found
through the current `0x44` map page.

On-device `20260504k` still showed the current `0x44` page segment bottoms out
around logical block 4 (`LBEST l=00000800`). That means the page is real and
usable, but it is not the segment containing LPN zero. Build `20260504l`
reduces the low-page output and scans only the known metadata blocks and their
adjacent blocks (`2820/2821` and `6916/6917`) for `0x44` and `0x45` pages.
For each candidate page it treats the body as a 16-bit map segment and reports:
`N3G_SEGMAP t=.. b=.. p=.. ix=.. u=.. min=.. j=.. v=..`. The final
`N3G_SEGBEST` line identifies which candidate segment reaches the lowest LPN.

After sending `20260504l`, the device still displayed `20260504k`-only output
(`N3G_LFIND`), while the raw DFU file on disk contained `20260504l` and no
`N3G_LFIND` string. Build `20260504m` is a deployment proof payload: it prints
only `N3G_BUILD 20260504m` and `N3G_EXEC_PROOF_M`, then returns immediately.
Use it to confirm that the just-built raw DFU payload is actually executing
before continuing the map-segment diagnostics.

On-device `20260504m` printed the proof line, confirming raw DFU execution is
working. Build `20260504n` restores the map-segment diagnostic and adds
`N3G_EXEC_SEG_N` before storage init so the next run is easy to distinguish
from stale output.

On-device `20260504n` showed the known `0x45` pages still have no user-map
entries (`u=0`). The only dense map segment found in the known metadata blocks
is still `0x44` at block `6916`, page `0`; it reaches low LPNs but not LPN
zero. Build `20260504o` removes the noisy locator/ref/map output and runs only
a compact segment/signature probe over `2820`, `2821`, `6916`, and `6917`,
pages `0..3`. It prints `N3G_SEG2` summaries, `N3G_SEG2_BEST`, and reads the
body of the lowest mapped page to report `N3G_SEG2_BODY l=.. sig=.. w=..`.
This checks whether the lowest mapped LPN is actually an MBR-like sector and
whether Rockbox should mount from a nonzero logical base offset.

On-device `20260504o` showed a contradictory clipped result: the `0x44` segment
line appeared to report `min=00000000`, but `SEG2_BEST` still reported
`l=00000800`. Build `20260504p` replaces the segment scan with one direct
authoritative probe of map page `6916:0`. It prints `N3G_DMAP_SUM` with the
exact minimum LPN and whether an exact LPN zero entry exists, then reads the
minimum page body as `N3G_DMIN` and, if present, exact LPN zero as
`N3G_DZERO`.

On-device `20260504p` directly confirmed map page `6916:0` has no exact LPN
zero in its page-0 checks: `z=0`, with the minimum page-0 LPN reported as
`00000800` and body signature `f833`. Build `20260504q` keeps the direct map
probe and adds a focused `N3G_DLOW` scan: it takes the low candidate vblocks
already found from `6916:0`, scans all pages within those vblocks for LPNs
below `64`, reports exact low hits (`N3G_DLOW_HIT`), per-vblock summaries
(`N3G_DLOW_V`), and the lowest page/body (`N3G_DLOW_BEST`,
`N3G_DLOW_BODY`). If exact LPN zero exists in those candidates it prints
`N3G_DLOW_ZERO`.

On-device `20260504q` reported no LPNs below `64` in the low candidate vblocks
(`N3G_DLOW_V ... h=0`). That filter was too narrow because the direct map
already showed the lowest page-0 LPN is `0x800`. Build `20260504r` changes
`N3G_DLOW` to count all user pages, count low pages under `0x2000`, track the
minimum LPN across every page in each candidate vblock, and print
`N3G_DLOW_MIN` with the body signature for each candidate that has low pages.

On-device `20260504r` showed the current map page's low candidates are coherent
low logical ranges, not LPN zero:

- `v=01d3` starts at LPN `00000800`
- `v=057d` starts at LPN `00000c00`
- `v=0405` starts at LPN `00001000`
- `v=04eb`/`04ec` start around LPN `00001c00`

The body signatures at these minima were not `0xaa55`. Build `20260504s`
suppresses the individual low-LPN hit lines and adds `N3G_DBOOT`, a bounded
body-signature scan across these map-derived low vblocks only. It reports pages
with `0xaa55`, `MSDOS`, or `FAT` markers and finishes with
`N3G_DBOOT_DONE hits=..`.

On-device `20260504s` found no `0xaa55`, `MSDOS`, or `FAT` markers when
checking only the first 512-byte sector of each 2048-byte NAND page
(`N3G_DBOOT_DONE hits=0`). Build `20260504t` expands that same bounded
map-derived scan to check all four 512-byte slots inside each page. It prints
`N3G_DBOOT4` hits with slot index `q=0..3`, signature, and marker tag, then
finishes with `N3G_DBOOT4_DONE hits=..`.

On-device `20260504t` also found no boot/FAT markers in any of the four
512-byte slots (`N3G_DBOOT4_DONE hits=0`). The OOB LPN map is coherent, but the
user page bodies look patterned rather than filesystem-like. Build
`20260504u` adds a body-path comparison for the known low page `v=01d3`,
page offset `0` (LPN `0x800`): it reads the same physical page through the
Rockbox local transfer and through the BootROM helper, compares body words,
prints both signature sets, and prints both spare/OOB views. This checks
whether the local page-transfer loop is returning the same transformed body as
the BootROM helper.

On-device `20260504u` stopped after `N3G_CMP_PAGE`, before any read result was
printed. Build `20260504v` stages the compare probe so it prints
`N3G_CMP_BEFORE_LOCAL`, `N3G_CMP_AFTER_LOCAL`, `N3G_CMP_BEFORE_OOB`, and
`N3G_CMP_AFTER_OOB`. It intentionally skips the BootROM helper call in this
build so the diagnostic always reaches a local-read verdict.

On-device `20260504v` confirmed the resolved physical page for `v=01d3`,
page offset `0`, has valid user OOB (`type=0x40`, LPN `0`), but the 2048-byte
body returned by the local transfer was all zero. Build `20260504w` extends
that exact-page probe to compare the local full-page DMA read, a small FIFO
read at column zero, and the normal `nand_read_page()` wrapper. It reports
word samples and nonzero byte counts for each method to isolate whether the
zero body is caused by the full-page transfer path or by all body read paths.

On-device `20260504w` showed the body path split clearly: the local full-page
diagnostic read returned all zero, a small FIFO read returned nonzero bytes,
and `nand_read_page()` returned a nonzero body with valid user OOB. It also
reported `v=01d3`, page offset `0`, as OOB LPN zero, which contradicts earlier
OOB-only low scans that reported that same vblock as starting at LPN `0x800`.
Build `20260504x` adds an OOB read before any body reads and another OOB read
after the body reads for the same physical page. This checks whether the stage
spare/OOB extraction depends on read order or controller state.

On-device `20260504x` confirmed the OOB-only diagnostic path was stateful:
the first standalone OOB read returned zero/stale spare, while the later OOB
read after body transfers returned `type=0x40`, LPN `0x800`. The normal
`nand_read_page()` path returned the nonzero body and matching OOB. The Nano 3G
diagnostic OOB-only helper now performs a priming transfer and then a second
identical transfer before copying stage spare. Build `20260504y` reruns the
same compare with that OOB-only stabilization in place.

On-device `20260504y` confirmed the OOB-only stabilization works:
`N3G_CMP_AFTER_OOB0` and the later `N3G_CMP_AFTER_OOB` both reported
`type=0x40`, LPN `0x800` for `v=01d3`, page offset `0`. The local full body
read and `nand_read_page()` also agreed on nonzero body data. Build
`20260504z` returns to map discovery with the fixed OOB path: it scans page
offset `0` across two bounded metadata ranges (`7167..5120` and `3071..1024`)
for `0x44` map pages. For each hit it treats the body as a map segment and uses
the reliable `nand_read_page()` path to summarize user-count, min/max LPN, map
index, and mapped vblock.

On-device `20260504z` found three valid `0x44` map pages. The first/current
hit at block `6916`, page `0` is the important one: it reported `u=1015` and a
minimum LPN of `0`, while older map pages at blocks `6166` and `5914` reported
minimum LPN `0x800`. This proves the current map segment for LPN zero is
present and should be used for the first read-only mount attempt.

Build `20260504aa` replaces the map search diagnostic with a guarded Nano 3G
bootloader direct-mount path. It locates a `0x44` map page whose page-0 OOB
inverse scan reaches logical block zero, builds `ftl_map[logical_block] =
vblock` from user-page OOB LPNs, and routes `ftl_read()` directly through the
validated Nano 3G physical-page formula:

`abspage = (vblock + syshyperblocks) * ppb + page`

The direct read path remains read-only and verifies each read against user OOB
type `0x40/0x41` and matching LPN. It prints `N3G_DIRECT_MAP`,
`N3G_DIRECT_S0`, and `N3G_DIRECT_READY` before returning FTL init success.

On-device `20260504aa` did not mount. It found a large number of `0x44`
candidates in the broad ranges, but each candidate produced only a sparse
inverse map and none reliably mapped logical block zero (`m0=ffff`). That means
blind `0x44` scanning is polluted by old/sparse map segments and should not be
used for mount selection.

Build `20260504ab` stops the broad direct scan and tests only the known current
map page found earlier: block `6916`, page `0`. It prints `N3G_DIRECT_RAW`
with the map OOB USN/index plus raw page-0 LPN min/max before attempting the
sector-zero direct-read proof.

On-device `20260504ab` showed `nand_read_page()` is still not a reliable way to
read metadata spare for block `6916`, page `0` in the direct-mount path:
`N3G_DIRECT_FAIL t=8f ix=faff u=ffa1fcff`. The same page was proven earlier by
the stabilized local read path as `type=0x44`, so build `20260504ac` changes
only the metadata map-page read to use `nano3g_nand_diag_local_read()` for body
and spare. User-page OOB/body reads remain on the normal `nand_read_page()`
path that matched the body comparison probe.

On-device `20260504ac` read the known map page correctly:
`type=0x44`, `idx=0`, `usn=fffccbe2`, and `ru=1015`. However, sampling only
page offset zero still produced no logical block zero (`cnt=745`, min logical
block `4`). Build `20260504ad` keeps the same current map page and adds a
bounded LPN0 finder: if page-offset-zero inversion misses map0, it scans page
offsets inside the mapped vblocks until it finds any user OOB with LPN below
`ppb`, stopping as soon as LPN zero is found. It records that exact vblock/page
offset for the sector-zero proof.

On-device `20260504ad` completed the full mapped-vblock page-offset scan:
`scans=524288`, but found only two pages below one hyperblock and still did not
find LPN zero. This disproves the assumption that map page `6916:0` contains
sector zero anywhere in its mapped vblocks. Build `20260504ae` reruns the same
bounded scan with far less progress output and prints the exact low-LPN hits as
`N3G_L0_HIT`, including body signatures. The purpose is to identify what those
two low pages are before choosing the next map-search strategy.

On-device `20260504ae` completed the scan and showed the two low hits are
`LPN=0x000000ff` and `LPN=0x000001ff`, from map entries `j=359`/`v=04e5` and
`j=484`/`v=01b2`. The scan finished with `scans=524288`, `hits=2`, and no LPN
zero. Those are tail pages of logical block zero, not sector zero. Build
`20260504af` changes direction from mounting to a concise multi-map low-LPN
search. It checks the three known `0x44` map pages (`6916`, `6166`, `5914`),
prints low hits below `ppb` as `N3G_MLOW_HIT`, and stops immediately if any
map owns LPN zero.

On-device `20260504af` confirmed both real `0x44` map pages behave the same:
block `6916` with USN `fffccbe2` and block `6166` with USN `fffccc16` each
reported the same low tail-page hits (`0xff` and `0x1ff`) and no LPN zero. The
previous block `5914` did not read back as a `0x44` map in this path. The next
hypothesis is that the missing early pages are represented by `0x45`
continuation/log pages near the current context, not by the dense `0x44` map.
Build `20260504ag` probes known nearby `0x45` pages and prints compact body
shape summaries rather than scanning user data again.

On-device `20260504ag` showed all known nearby `0x45` pages have current OOB
metadata but erased bodies (`FFFFFFFF` words). They are not the missing map
bodies. Build `20260504ah` now scans the first 16 pages of each block in the
two known metadata ranges for `0x44` and `0x45` OOB markers. This should reveal
whether additional map pages live at nonzero page offsets that earlier page-0
searches missed.

On-device `20260504ah` showed only page-zero `0x44` maps in the scanned
metadata ranges; all other hits were `0x45` marker pages with erased-looking
bodies. There is no hidden nonzero-page `0x44` map segment in those ranges.
Build `20260504ai` switches to a direct physical OOB search for low user LPNs:
it scans all four banks across four 2048-block bands, prints user pages with
`LPN < 0x200`, and stops immediately if `LPN 0` is found. This is bounded and
prints a final `N3G_LPN0_DONE` verdict.

Partial `20260504ai` output showed the direct OOB search was dominated by
`0xff` and `0x1ff` tail-page hits, with no useful sector-zero signal before
the output became noisy. Build `20260504aj` stops broad scanning and hard-checks
the known-good physical candidate instead: bank `0`, physical page `114176`
(`pb=892`) and nearby page offsets `-4..+8`. It prints OOB LPN/type plus body
signatures as `N3G_ROOM892`.

On-device `20260504aj` showed the normal `nand_read_page()` spare path does not
currently reproduce the earlier clean `type=0x40`, `LPN=0x800` result for
physical block `892`, page offset `0`; the visible type bytes were inconsistent
(`00`, `88`, `01`, `10`, `ff`, etc.). Build `20260504ak` repeats the same
focused room-892 probe with short lines and prints both OOB sources for each
offset: `N3G_R892N` from normal `nand_read_page()` and `N3G_R892L` from the
stabilized local OOB read.

On-device `20260504ak` showed normal and local OOB agree around room 892, but
the OOB does not decode as a clean Whimory user page at bank `0`, physical page
`114176`; examples included `l=002100ff` and type bytes like `00`/`a8`. Build
`20260504al` now probes the remembered vblock directly (`v=01d3`) through the
current Nano 3G interleave formula for logical page offsets `0..31`, printing
the resolved bank/physical page and both normal/local OOB decodes. This avoids
confusing adjacent physical pages with adjacent logical pages.

Partial `20260504al` output showed the expected `0x800` signal is present but
byte-shifted: `po=14`, `bank=2`, `pb=892`, `pg=3` reported raw spare word
`000800ff`. Interpreting spare word 0 as `[tag byte][24-bit LPN]` gives
`000800ff >> 8 = 0x800`. Build `20260504am` narrows to offsets `12..15` and
prints raw spare words plus this shifted-LPN decode (`sl=...`) to confirm the
Nano 3G user-spare layout.

On-device `20260504am` showed the stabilized local OOB helper fails on the
expected bank-2 page (`po=14`, `rc=-3`), while the earlier normal
`nand_read_page()` path succeeded there. Build `20260504an` therefore prints
both normal and local raw spare words for offsets `12..15`, including shifted
LPN decode from the normal spare path.

On-device `20260504an` ruled out the shifted-LPN interpretation for the
remembered `v=01d3` candidate: the normal path at `po=14`, `bank=2`, `pb=892`,
`pg=3` reported `sl=00000000`, not `00000800`. The previous `000800ff` value
was therefore not a stable decode. Build `20260504ao` goes back to the source
of that candidate: map page `6916:0`, entry `j=744`, then prints the raw OOB
words for the first 16 resolved physical pages of that exact vblock.

On-device `20260504ao` showed map entry `j=744` is indeed `v=01d3`, but it
does not own low logical pages in its first 16 offsets. More importantly, this
reframes earlier `MLOW` results: values like `w0=000000ff` and `w0=000001ff`
are not tail-page LPNs. Nano 3G user spare appears to store a low marker byte
in `w0[7:0]` and the LPN in `w0 >> 8`. Therefore previous hits with
`l=000000ff` and `l=000001ff` are shifted LPN `0` and `1`. Build
`20260504ap` uses this shifted-LPN decode to build a low-page table from map
page `6916:0` and prove a sector-zero read through the exact mapped vblock and
page offset.

On-device `20260504ap` showed why that generalization was unsafe: shifted-LPN
normalization accepted `j=0/v=0781`, but the known-good anchor is still
`j=744`, `v=01d3`, physical block `892`, physical page `114176`, OOB
`type=0x40/LPN=0x800`, body signature `f833`. Build `20260504aq` is therefore
anchor-only. It forces map entry `j=744`, evaluates only page offset `0`, and
prints local OOB before/after, local body, and normal `nand_read_page()` body
and spare. No generalized decoder is accepted until this anchor reproduces.

On-device `20260504aq` reproduced the anchor cleanly:

- map `6916:0`, entry `j=744` is `v=01d3`
- decoded page offset `0` resolves to bank `0`, physical block `892`, page `0`
- local OOB before body: `type=0x40`, `lpn=00000800`
- local body: signature `f833`, first words `888839d3,8888e9df`
- normal `nand_read_page()` body/spare matches: `type=0x40`, `lpn=00000800`
- final verdict: `N3G_A744_DONE ok=1`

This is the first stable Nano 3G FTL read proof. Generalization must preserve
this anchor and should use the normal/local page read path that reproduced it,
not broad shifted-LPN guesses that admit false candidates.

Build `20260504ar` attempts the first read-only direct mount from the stable
anchor. It treats Rockbox sector `0` as FTL LPN `0x800`, builds an inverse map
from the current `0x44` map page using page-0 OOB LPNs, proves sector 0 through
`j=744/v=01d3`, then returns FTL init success if that proof read succeeds.

On-device `20260504ar` was the first mount-like path, but the inverse map still
used only page-0 OOB and picked `v=01d4` for host sector zero; the proof read
returned LPN `0x802` and failed. Build `20260504as` adds an exact page table for
host sectors `0..511` by scanning the current map page's vblocks for OOB LPNs
in `0x800..0x9ff`. `ftl_read()` now consults this exact page table before the
block-level fallback.

On-device `20260504as` reached `N3G_DMOUNT_MAP` and then appeared stuck because
the exact-page scan was silent and very large (`1024 * ppb` page probes). Build
`20260504at` removes that broad exact scan for now and seeds host sector zero
directly from the stable anchor (`map[744] == 01d3`, page offset `0`). It
proves only the anchored sector-zero read, then returns success if that read
passes.

On-device `20260504at` succeeded through the anchored direct mount proof:
`N3G_DMOUNT_S0 rc=0`, OOB `type=0x40`, LPN `00000800`, body signature
`f833,1d33`, then `N3G_DMOUNT_READY`. The bootloader then reported storage
sector size `2048` and `No partition found`. That means the FTL read path is
now good enough to return the anchored page, but the page exposed as storage
sector zero is not an MBR or FAT boot sector with a normal `0xaa55` signature at
offset 510. Build `20260504au` stops trying to mount and instead runs a compact
boot-signature probe over the proven map page candidates. It checks the four
512-byte signature slots inside each 2048-byte NAND page so we can distinguish
between a 512-byte-sector presentation issue and a remaining user-data decode
or scrambling issue.

On-device `20260504au` found no `0xaa55` signatures in the first few pages of
the anchor vblock: `N3G_BSIG_DONE hits=0`. The stable LPN `0x800` page remains
real (`type=0x40`, body `f833,...`), but it is not a normal MBR/FAT boot sector
image. Also note that the `BSIG` implementation reused `ftl_buffer` as both the
map body and page body, so only the first candidate was reliable. Build
`20260504av` copies the map body into `ftl_map` first, then performs a bounded
read-only format probe over the current map page. It checks compact signatures
for WinPod (`0xaa55`, FAT strings) and MacPod (`ER`, `PM`, `H+`, `HX`) and
prints a single `N3G_FORMAT_GUESS macpod|winpod|unknown` verdict.

On-device `20260504av` reported `N3G_FORMAT_GUESS macpod mbr=0 apm=1 hfs=1
fat=0`. This confirms the current Nano 3G disk contents are MacPod/APM/HFS, not
WinPod/MBR/FAT. The earlier `No partition found` after `N3G_DMOUNT_READY` is
therefore expected from the Rockbox bootloader's FAT/MBR-oriented mount path and
does not invalidate the NAND/FTL read proof. For Rockbox boot testing, the next
practical step is to restore/reformat the iPod as Windows/FAT, then rerun the
same guarded Nano 3G FTL path and expect MBR/FAT signatures instead of APM/HFS.

After a Windows iTunes restore, `20260504av` produced a mixed verdict:
`mbr=1 apm=1 hfs=1 fat=0`. This is plausible after NAND FTL history: stale
MacPod-looking pages can remain readable, so classification must prioritize the
current logical page zero instead of any old signature anywhere in mapped user
pages. Build `20260504aw` is a stricter read-only MBR probe. It finds pages
whose OOB LPN is exactly `0`, reports the `0xaa55` slot, parses the four MBR
partition entries, then probes each partition start using both direct LBA and
`LBA/4` interpretations to account for 512-byte MBR sectors inside Rockbox's
2048-byte NAND page sectors.

On-device `20260504aw` flooded the screen with `N3G_MBR_L0` because the
restored NAND still has many historical pages whose OOB LPN is `0`. Build
`20260504ax` keeps the LPN0 count internally but suppresses those per-page
prints. It should only show progress, `N3G_MBR_HIT`, `N3G_MBR_PART`,
`N3G_MBR_FAT`, and the final `N3G_FORMAT_GUESS`.

The post-restore strict probe confirmed `N3G_FORMAT_GUESS winpod` with
`mbr=1`, `hfs=0`, and `apm=0`. Build `20260504ay` turns that into the next
read-only mount attempt. It finds the real MBR page by `LPN=0 + 0xaa55`, parses
the partition entry, proves the FAT boot page using both direct MBR LBA and
`LBA/4`, then synthesizes sector 0 with the partition start/size adjusted to
Rockbox's 2048-byte NAND sectors. This avoids writing anything to NAND while
letting `disk.c` see an MBR layout in the sector units it expects.

On-device `20260504ay` found the restored WinPod MBR:
map USN `fffccb2c`, `N3G_WMOUNT_MBR j=992 po=0 slot=0`, partition 0
`type=0x0c start=0x3f size=0xe7f81`. It also printed a stale/invalid partition
1 (`type=0x8f size=0`), which must be ignored. Build `20260504az` now selects
only FAT32 partition entries (`0x0b`/`0x0c`) with nonzero start and size,
prefers `0x0c`, keeps partition start/size unchanged at `0x3f/0xe7f81`, and
makes the partition boot-sector read a diagnostic rather than a hard stop once
the valid `0x0c` partition exists.

On-device `20260504az` showed the current MBR entry bytes as partition 0
`type=0x0c start=0xa07e size=0xe7f81` and partition 1 `type=0x8f start=0x3f
size=0`. The valid entry is still partition 0; the earlier `0x3f` start was
from the stale zero-size slot. The payload then stalled after `USEPART` because
it still performed a broad partition boot-sector search. Build `20260504ba`
removes that full-page scan. It scans only page 0 of each map entry to find the
vblock whose first OOB LPN belongs to `part_start / ppb`, seeds that block into
`ftl_map`, and then lets the normal direct read path test `part_start` exactly.

On-device `20260504ba` parsed the synthetic MBR successfully and Rockbox later
reported `P0 T0C`, so `disk.c` now sees the partition table. The partition boot
sector still failed: `N3G_WMOUNT_WARN nobmap lb=80 st=0000a07e` followed by
`N3G_WMOUNT_SP/SF rc=-6`. Build `20260504bb` adds a bounded exact lookup for
only the required page offset (`part_start % ppb`) across the 1024 map entries.
This is at most 1024 reads with progress every 128 entries, and should seed
logical block 80 if any map entry has OOB LPN `0x0000a07e` at that page offset.

On-device `20260504bb` still reached `N3G_WMOUNT_READY` and proved the MBR
read (`N3G_WMOUNT_S0 rc=0 sig=aa55`) but could not find the partition boot
sector: exact lookup for `st=0x0000a07e` did not find a matching user-page OOB
LPN. Build `20260504bc` keeps the scan bounded and tests only four target
interpretations for that same partition start: exact, 4-sector aligned base,
`start/4`, and `start/2`. It prints one compact `N3G_WMOUNT_TGT` line per
interpretation with the candidate LPN, slot, map index, page offset, boot
signature, BPB bytes-per-sector, and FAT-string detection. This should tell us
whether the remaining failure is a partition-sector unit mismatch or a missing
map segment for logical block 80.

On-device `20260504bc` showed all four direct target interpretations failed:
exact `0x0000a07e`, aligned-base `0x0000a07c`, `start/4` (`0x0000281f`), and
`start/2` (`0x0000503f`) did not resolve to an obvious partition boot sector.
It then reached the existing exact page-offset sweep with `po=126`, matching
`0x0000a07e & 0x7f`. Build `20260504bd` instruments that sweep. For each
non-empty map entry read at `po=126`, it prints `N3G_WMOUNT_XC` with the map
index/value, page-0 logical range, computed bank/physical block/page, OOB
type/LPN, first body words, `AA55` status at `0x1fe`, BPB bytes-per-sector, and
FAT string detection. If any candidate has `AA55`, the scan stops immediately
and that `j/v/po` is installed as the partition boot-sector mapping for the
read-only direct path.

On-device `20260504bd` still produced only `N3G_WMOUNT_XPROG` progress through
`j=896`; there were no `N3G_WMOUNT_XC` candidate lines, meaning `po=126` did
not expose valid user-page OOB entries for the partition boot sector. Build
`20260504be` stops treating `po=126` as the only possible offset. It adds a
bounded, content-driven candidate search over mapped user vblocks: target
window `0x0000a07e +/- 0x200`, rounded targets around `0xa000`, `0xa040`,
`0xa078`, `0xa07c`, and `0xa080`, and full body validation by `AA55`, BPB
bytes-per-sector, BPB jump/fat count sanity, and FAT string detection. It
prints only candidate pages (`N3G_WMOUNT_CAND`) rather than every read. If a
FAT BPB candidate is found, `N3G_WMOUNT_CMAP` installs that `j/v/po/slot` as a
read-only boot-sector override and stops the search immediately.

On-device `20260504be` was still too noisy because range-only candidates
overflowed the Nano screen. Build `20260504bf` keeps the same bounded
content-driven search, but suppresses range-only candidate lines. It prints at
most 12 `N3G_WMOUNT_CAND` lines, and only for pages with an `AA55` signature or
stronger boot-sector score. The final `N3G_WMOUNT_CDONE`/`CMISS` lines retain
the internal candidate/read counts so we still know whether the search ran.

On-device `20260504bf` still overflowed the screen because weak `AA55` pages
are common in restored NAND history. It did not show a usable `CMAP`; the
partition table still mounted only as synthetic MBR and the FAT boot-sector read
failed. Build `20260504bg` suppresses weak `AA55` pages entirely. It prints at
most 8 short `N3G_WMOUNT_CB` lines and only when a page has BPB-like fields or
FAT strings. It also removes the old exact `po=126` fallback after the content
scan, so the final evidence is just `CDONE` counters and, if found, `CMAP`.

On-device `20260504bg` reported `N3G_WMOUNT_CDONE r=524288 in=96 aa=12 bpb=0
fat=0`. That means the bounded scan found 12 pages with `AA55` signatures, but
none had sane BPB/FAT32 fields. Build `20260504bh` dumps only those 12 `AA55`
pages. For each hit it prints `j/v/po/slot/pb/pp`, OOB type/LPN, first 16
bytes, BPB bytes `0x0b..0x24`, FAT32 label bytes `0x52..0x59`, signature bytes,
and a compact reason mask. The BPB validator now checks all four 512-byte slots
inside each 2048-byte page, requires a valid bytes-per-sector value, power-of-2
sectors-per-cluster, nonzero reserved sectors, sane FAT count, total sectors
near the partition size, FAT size/root-cluster sanity, and `FAT` string
detection. `AA55` alone is never accepted as a boot sector.

On-device `20260504bh` showed the `AA55` candidates have random or zero BPB
fields (`bps=0`, `bps=44932`, `spc=117`, etc.), so they are stale/random
signatures and not the FAT boot sector. Build `20260504bi` stops the broad
body-signature scan. It loads all current `0x44` map pages near the active map
block (`6916..6919`) with the active USN, reports `N3G_WMOUNT_MPAGE/MLOAD`,
then reruns the exact OOB lookups for `st=0xa07e`, aligned base, `st/4`, and
`st/2` across the merged map entries. If a target has `AA55`, it prints
`N3G_WMOUNT_TBPB`; if the BPB/FAT fields are sane, it installs that target with
`N3G_WMOUNT_TMAP`. The old broad content scan is disabled for this build.

On-device `20260504bi` still loaded only one current map page:
`N3G_WMOUNT_MPAGE i=0 b=6916 p=0` and `N3G_WMOUNT_MLOAD n=1 ent=1024`. The
exact partition-start target lookups all missed, so logical block 80 is not
represented by the direct `0x44 idx=0` table that contains the MBR. Build
`20260504bj` also scans nearby `0x45` metadata/log-map pages in blocks
`6916..6919` pages `0..7`. Each `0x44/0x45` body is treated as a candidate
1024-entry map page for the four partition-start interpretations. It prints
compact `N3G_WMOUNT_LHIT` lines for exact OOB target matches and installs a
valid BPB/FAT target as `N3G_WMOUNT_LMAP`. This avoids another broad body
signature scan while testing whether the FAT boot mapping lives in the adjacent
`0x45` map/log pages.

On-device `20260504bj` showed the adjacent `0x45` pages were visible but did
not produce target hits. Build `20260504bk` changes map selection itself. It
scans known metadata bands for `0x44 idx=0`, ranks candidates by whether their
map body reproduces the known WinPod MBR near `j=982`, then by USN, and uses
the selected map cluster instead of assuming block `6916`. It also prints
`N3G_WMOUNT_MENT` entries around the MBR map index and the expected partition
logical block so we can see the parsed map values and page-0 OOB LPNs before
target lookup.

On-device `20260504bk` proved the working MBR map entry has a nonzero logical
base: `N3G_WMOUNT_MENT mbr j=982 v=056B t=40 l0=00000002`. The FAT partition
entry remains WinPod/FAT32-LBA, `type=0C start=0000A07E size=000E7F81`, so the
partition boot-sector lookup should test `mbr_l0 + start = 0000A080`, not only
raw `0000A07E` or the old `part_start / ppb` map window. Build `20260504bl`
computes the MBR map entry's page-0 OOB LPN, adds an explicit `mbrbase` target,
and dumps that target's `j/v/po`, OOB type/LPN, first body words, signature
bytes, BPB fields, and FAT/OEM string words. The noisy direct logical-block
fallback scan is disabled for this build so failures stay readable.

On-device `20260504bl` showed the selected MBR map entry's page-0 OOB LPN is
not stable as an MBR base (`j=982` reported `l0=0007A800` on this run), and the
direct target lookup still missed. Build `20260504bm` changes the lookup model:
each map entry is treated as a logical range derived from its page-0 OOB LPN,
with a 2048-byte NAND page covering four 512-byte sectors:
`po=(target_lpn-lpn0)/4`, `slot=(target_lpn-lpn0)&3`. It probes this range
model for the partition start `0000A07E` and the experimental `mbrbase` target,
printing only compact `RLOOK/RHIT/RB/RMAP/RDONE` lines.

On-device `20260504bm` produced range hits for both `exact` and `mbrbase`, but
none passed BPB/FAT validation, so `fat_j/fat_po` were left as `FFFFFFFF` and
the final partition-start read still failed. Build `20260504bn` preserves the
best rejected range candidate for diagnostics. It prints `RSEL/RREJ` when range
hits exist but are rejected, and prints `SEL/SEL2` immediately before the final
partition boot-sector read. `MLOAD` also now prints `MVALID`, because
`max=0` only means the highest loaded metadata page index is zero, not that the
map page body is empty.

On-device `20260504bn` selected a rejected range hit (`j=61 v=0334 po=287`) as
diagnostic state; its body signature was `0000` and BPB reason was `77F`, so it
was not a real FAT boot sector. Build `20260504bo` fixes selection semantics:
range hits are all read and validated, each rejection gets a compact `RREJ`
line, and only an `AA55` + sane BPB/FAT candidate installs `fat_j/fat_po`. If
no candidate passes validation, the final path prints `N3G_WMOUNT_NOBOOT`
with rejection counts and skips the partition-start `SF` read.

The next observed screen still showed the old `RSEL/PREJ/SEL ok=0/SF` sequence,
so it was not executing the BO control-flow despite the BO raw DFU being sent.
Build `20260504bp` adds an unmistakable `N3G_EXEC_WMOUNT_BP_NOSEL0` marker,
disables the old direct `TGT` fallback loop for this diagnostic, and leaves only
the validated range lookup. A failing run should end at counted `NOBOOT`, not
`SEL ok=0` and not `SF`.

On-device `20260504bp` confirmed the exact/mbrbase range hits are not FAT boot
sectors: the hits at `j=61` and `j=462` have `sig=0000` and are rejected. Build
`20260504bq` keeps MBR/partition detection unchanged and adds a constrained
content-validation pass over mapped user pages. It checks all four 512-byte
slices inside each 2048-byte page, rejects `sig=0000`, and only prints compact
`BCAND/BBPB/BSTR` lines for slices with `AA55` plus BPB fields that are close to
valid. Only `AA55` + sane BPB/FAT installs a partition boot-sector mapping.

On-device `20260504bq` reported `fat=1` but `bpb=0` and printed no close BPB
candidate. That means the FAT-string-like slice was not the best-scored slice
for its page. Build `20260504br` prints FAT-string hits directly, independent
of per-page best score. It dumps `FCAND/FOOB/F0/F1/F2/FR/FB/FSH`, including
the physical location, OOB type/LPN, 512-byte slice index, first 16 bytes, BPB
bytes `0x0B..0x24`, FAT string bytes `0x52..0x59`, signature bytes, compact
BPB failure bits, and BPB reason masks for shifts `+0/+1/+2/+4`. FAT alone is
still not accepted as a boot-sector mapping.

The next pasted screen still showed the pre-BR `CDONE` format without `fp=...`,
so it was not the BR payload output. Build `20260504bs` keeps the BR FAT-string
dump but removes the noisy `WARN/CSTART/CROUND/CPROG` progress lines and uses
the unique marker `N3G_EXEC_WMOUNT_BS_FATONLY`. The FAT-focused lines should be
visible directly as `FCAND/FOOB/F0/F1/F2/FR/FB/FSH`.

On-device `20260504bs` showed the scan starts at `FSCAN` and the in-range OOB
hits `j=61` and `j=462` remain rejected with `sig=0000`. Build `20260504bt`
adds an in-range slice print path that is independent of the page's best-scored
slice. For any page whose OOB LPN is in `st +/- 0x200`, each 512-byte slice is
validated and compact `ICAND/IBPB/ISTR` lines are printed if it has `AA55`, a
sane bytes-per-sector value, a FAT string, or partially sane BPB fields. The
summary `CDONE` now includes `ip=<printed in-range candidates>`.

Build `20260504bu` changes FSCAN again to use decoded map-entry ranges directly
instead of scanning every page and then checking page OOB. For each non-empty
map entry, it reads page-0 OOB as `l0` and only scans `po` values where
`l0 + po*4 + slice` overlaps `00009E7E..0000A27E`. Every 512-byte slice is
checked for `AA55`, sane bytes-per-sector, power-of-two sectors-per-cluster,
nonzero reserved sectors, sane FAT count, and FAT/FAT32 string. Slices passing
at least one check print `VCAND/VBPB/VSTR`; the final `F2DONE` line reports
range count, reads, printed candidates, per-rule hit counts, and whether a
validated boot-sector mapping was installed.

On-device `20260504bu` showed `j=61 v=0384` can produce an `AA55` slice, but
its BPB is impossible (`bps=53456`, `spc=208`, `rs=53456`), so that map entry is
not the partition boot sector. Build `20260504bv` marks `j=61` as known-bad for
the current WMOUNT search and skips it in both exact range lookup and FSCAN2.
FSCAN2 now only prints slices with sane bytes-per-sector or a FAT/FAT32 string;
random `AA55` alone is counted but no longer printed.

On-device `20260504bv` removed `j=61` from exact lookup and left only another
bad exact hit, `j=462 v=0383 po=287 sig=0000`. Build `20260504bw` skips both
`j=61` and `j=462` in exact/mbrbase RLOOK, widens FSCAN2 to
`0000A07E +/- 0x4000`, and includes physical `pb/pp` in printed `VCAND` lines.
FSCAN2 still allows any map entry, including known-bad exact hits, to print if
it later shows sane bytes-per-sector or a FAT/FAT32 string.

Build `20260504bx` reduces the FSCAN2 output to compact candidate-table rows.
Only slices with `AA55`, sane bytes-per-sector, or a FAT/FAT32 string print.
Known-bad exact map entries `j=61` and `j=462` are suppressed unless they show
sane bytes-per-sector or FAT. Each printed candidate uses `CTAB/CTAB2/CTAB3`
with `j/v/po/slice/l0/pb/pp`, `sig/bps/spc/reserved/nfats/fatsz/fat/reason`,
and FAT string bytes. `F2DONE` now directly reports `r`, candidate count,
`aa`, `bpb`, `fat`, and selected `j/po/slice`; `F2CNT` carries the remaining
rule counters.

On-device `20260504bx` narrowed the scan to 24 candidates and one FAT-string
candidate, but no valid BPB (`aa=1 bpb=0 fat=1`; the visible FAT-ish candidate
had invalid `bps=35` and `spc=156`). Build `20260504by` keeps the compact table
and adds a focused `FDET` dump for FAT-string candidates only: location/OOB,
first 16 bytes, BPB bytes `0x0B..0x24`, FAT string area `0x52..0x59`,
signature bytes, and `FSH` BPB parse attempts at byte shifts `-8..+8` around
the 512-byte slice start. FAT alone is still diagnostic and is not accepted.

On-device `20260504by` still found one FAT-string-like candidate but the BPB
was invalid (`bps=35`, `spc=156`) and no boot sector was selected. Build
`20260504bz` keeps MBR and partition parsing unchanged, stops printing CTAB rows
for FAT-only garbage, and prints CTAB only for `AA55` or sane bytes-per-sector
candidates. FAT-string hits now get focused detail only: `FDET/FOOB/FD0/FD1/FD2`,
a raw BPB-adjacent window `FBW0/FBW1`, and `FSH` BPB parse attempts for byte
alignments `-16..+16`. The compact summary keeps candidate, AA55, FAT, valid
BPB, selected `j/po/slice`, and rejection reason counts visible.

On-device `20260504bz` still reported only one FAT-string-like candidate and no
valid BPB (`bps=35`, `spc=156`), while the broader CTAB-style rows were still
too noisy on screen. Build `20260504ca` disables CTAB output for this pass,
widens the map-entry LPN range to `part_start +/- 0x8000`, and focuses only on
FAT-string candidates. It prints the FAT candidate location, OOB, body/BPB/FAT
bytes, a wider BPB-adjacent raw window (`-32..+32`), BPB shifts `-16..+16` for
the assumed slice, then checks all four 512-byte slices with those shifts and
prints the first `FSANE` alignment whose bytes-per-sector is 512/1024/2048/4096.
If none exists, it prints `FREJ ... no_sane_bps` and keeps scanning the widened
logical range.

On-device `20260504ca` still showed the old-looking `FSCAN/F2DONE/F2CNT`
screen text, but the raw DFU contains only the `FSCAN2`/new-counter strings; the
device display can truncate labels enough to make them ambiguous. Build
`20260504cb` switches this pass to unique `FXSCAN/FXDONE/FXCNT` labels, widens
the FSCAN2 map-entry range again to `part_start +/- 0x10000`, and expands the
FAT-candidate alignment diagnostic. FAT candidates now print raw bytes around
the assumed 512-byte sector start at `-32..+64`, parse BPB shifts `-32..+32`,
include FAT-size and `ok=<sane_bps>` on each `FSH` line, test all four 512-byte
slices with those shifts, and print `FSANE` for the first alignment whose
bytes-per-sector is 512/1024/2048/4096. If no sane-BPS alignment exists, `FREJ`
records that candidate as rejected and `FXCNT` reports the reject count.

On-device `20260504cb`/`20260504cq` output kept the valid MBR and Windows partition 0
but still found no boot sector. The widened `FXSCAN` found one `AA55` candidate,
zero FAT-string candidates, and no valid BPB (`bps=69` was visible in the rule
counters, which is not a valid bytes-per-sector value). Build `20260504cr`
therefore stops relying on FAT-string candidates for the detailed dump. It adds
an AA55-candidate path that prints `ADET/AOOB`, dumps a 96-byte BPB window
around the assumed BPB area as `AW d=-32..+48`, searches that window for
little-endian bytes-per-sector values `0x0200/0x0400/0x0800/0x1000`, and if one
is found prints `ABPS/ABPB` with the parsed BPB from the inferred sector base.
If no such value exists in the AA55 candidate window, it prints
`AREJ ... no_bps_magic` and keeps MBR/partition parsing unchanged.

On-device `20260504cr` proved the lone `AA55` candidate in the widened scan was
the MBR itself: `j=982 v=056B po=0 s=0 l0=00000000`, with `t=40 l=00000000`
and no FAT BPB (`bps=0`). Build `20260504cs` separates the two concepts in the
log. It prints `N3G_WMOUNT_MBR_OK` for the confirmed disk LBA 0 mapping, enables
the explicit boot-sector target probes again with `BSTGT/BSD/BSS/BSBPB`, and
prints `BOOTSECTOR_OK` only if a candidate has `55AA` plus a sane FAT BPB. The
broad `FXSCAN` now skips the confirmed MBR map entry (`j=982`) so the MBR is no
longer considered as a partition boot-sector candidate.

On-device `20260504cs` selected the older map page `b=6166 p=0 u=FFFCC98C` and
found a much more useful covering entry: `j=61 v=0194 l0=0000A07C`, exactly two
sectors before partition start `0000A07E`. The direct mbrbase probe still read
zeros at its single derived slice, so build `20260504ct` adds a targeted
covering-entry probe. It finds map entries whose page-0 OOB LPN is within
`target +/- 4`, logs `COV_ENTRY`, then explicitly reads `po=288..292` and all
four 512-byte slices for each page. Each slice prints `COVP` with OOB type/LPN,
signature, bytes-per-sector, FAT-string presence, and BPB rejection mask.
`BOOTSECTOR_OK cov` is printed only if a slice has `55AA` plus a sane FAT BPB.

On-device `20260504ct` did not find a boot sector; its covering probe returned
`e=0 r=0 aa=0 sane=0`, so the broad BPB/FAT scans remain uninformative. Build
`20260504cu` stops all broad boot-sector scans for this pass and tests direct
LBA translation only. After `MBR_OK`, it computes `mbr_l0 + p0_start`, searches
the currently selected map for entries whose OOB LPN range covers that disk
LBA, and prints either the selected path (`DLOOK/DCV/DSEL/DPHY/DOOB`) plus exact
sector bytes (`D0/D1/DBPB/DFAT/DSUM`), or the nearest map ranges (`DNEAR`) if no
covering entry exists. The goal is to understand the map lookup path before any
more BPB scanning.

Build `20260504cv` switches from live-only probing to a bounded offline replay
workflow. The live device still prints compact status lines:

- `N3G_WMOUNT_MBR_OK`: confirmed disk LBA 0 / MBR mapping. The stable observed
  mapping is `j=982 v=056B po=0 l0=00000000 sig=AA55`.
- `N3G_WMOUNT_PART_OK`: selected Windows partition 0, type `0C`, start
  `0000A07E`, size `000E7F81`.
- `N3G_WMOUNT_BOOT_CAND`: candidate pages nearest the partition-start LBA.
- `N3G_WMOUNT_NOBOOT offline_dump`: no live boot-sector mount is attempted in
  this dump build.

After `MBR_OK`/`PART_OK`, the diagnostic emits parseable raw dump records:

- `N3GD_BEGIN ...`: one dumped page, with `kind=map|mbr|cand`, `j`, `v`, `po`,
  `l0`, target LBA, bank, physical block/page, read return code, OOB type, and
  OOB logical page.
- `N3GD_BODY id=<n> off=<offset> <16-byte-hex>`: raw 0x800-byte page body.
- `N3GD_OOB id=<n> off=<offset> <16-byte-hex>`: raw spare/OOB bytes.
- `N3GD_END id=<n>` and `N3GD_DONE pages=<n>` delimit the capture.

The dump set is intentionally bounded to the selected map page, the confirmed
MBR page, and the nearest candidate pages around disk LBA `0xA07E`. This gives
the host tools exact bytes for map lookup and BPB validation without rebuilding
and reflashing for every hypothesis.

Host replay tool:

```sh
tools/nano3g_ftl_replay.py captured-log.txt --target-lba 0xA07E
```

The replay tool parses `N3GD_*` records, verifies the MBR and partition table,
then tests all four 512-byte slices inside each candidate page with BPB shifts
`-32..+32`. It accepts only a sane FAT boot sector: signature `55AA`,
bytes-per-sector in `512/1024/2048/4096`, power-of-two sectors per cluster,
nonzero reserved sectors, sane FAT count, nonzero FAT size, and a FAT/FAT32
string. Recent `AA55` or FAT-ish candidates with invalid bytes-per-sector
values such as `35`, `57`, or `69` remain explicit rejects.

Replay unit test:

```sh
python3 tools/test_nano3g_ftl_replay.py
```

The test uses synthetic `N3GD_*` dumps to prove that LBA 0 resolves to the
known MBR shape (`j=982/v=056B/po=0`), partition 0 parses as type `0C` with
start `0xA07E`, and an `AA55` sector with invalid `bps=35` is rejected.

Important DFU deployment note: `mks5lboot --mkdfu-inst` builds a bootloader
installer and can end up handing off to an older installed bootloader. That is
why the device still showed an older `20260503U` screen after a successful
send. For diagnostics that must execute the just-built payload directly, use
`--mkdfu-raw build-bootloader-ipodnano3g/bootloader.bin ...` and send that raw
DFU image instead.

Host DFU capture result for `20260504cv`:

- Capture directory: `tmp/n3g-capture-cv/`.
- Captured selected map pages: `b=6166 p=0` and `b=6916 p=0`, with full map
  body and page-0 OOB/extras.
- Captured confirmed MBR page: `j=982 v=056B po=0 l0=00000000`.
- Captured all page-0 extras for the `b=6166 p=0` map so the host can decode
  every map entry's OOB logical page.
- Captured the exact covering candidate pages for disk LBA `0xA07E`, plus
  nearest map-entry pages around that target.

Replay of `tmp/n3g-capture-cv/replay-near.log` currently reports:

```text
REPLAY_MBR_OK j=982 v=056B po=0 slice=0
REPLAY_PART_OK type=0C start=0000A07E size=000E7F81
REPLAY_BOOT_CAND phase=near j=598 v=0769 po=0 l0=0000C000 slice=0 shift=2 sig=AA55 bps=170 ...
REPLAY_BOOT_REJECT phase=near j=598 po=0 slice=0 shift=2 reason=1B7
REPLAY_BOOT_CAND phase=near j=816 v=035B po=0 l0=0000C800 slice=0 shift=2 sig=AA55 bps=170 ...
REPLAY_BOOT_REJECT phase=near j=816 po=0 slice=0 shift=2 reason=1B7
REPLAY_BOOT_CAND phase=near j=740 v=035C po=0 l0=0000C802 slice=0 shift=2 sig=AA55 bps=170 ...
REPLAY_BOOT_REJECT phase=near j=740 po=0 slice=0 shift=2 reason=1B7
REPLAY_NOBOOT target=0000A07E covering=6 near=14 printed=3
```

The six map entries that actually cover LBA `0xA07E` did not contain an
interesting FAT BPB/AA55/FAT-string sector. The nearest AA55-like pages are not
valid FAT boot sectors either: they decode as shifted signatures with
`bps=170`, `spc=0`, no FAT string, and rejection mask `0x1B7`. The missing
piece is therefore still the Nano 3G disk-LBA-to-physical-page transform, not
BPB validation.

Host DFU BPB scanner:

```sh
python3 tools/nano3g_dfu_bpb_scan.py \
  --max-pages 512 \
  --window 0x30000 \
  --sweep-offsets \
  --progress 64 \
  --read-timeout 4
```

The scanner decodes `tmp/n3g-capture-cv/map-b6166-p0-body.bin` and
`tmp/n3g-capture-cv/map-b6166-entry0-extra.bin`, computes the same
`vblock/page -> bank/physical-page` transform used by the firmware
(`system_hyperblocks=425`, four banks, 128 pages per physical block, 512 pages
per hyperblock), reads bounded candidate pages through wInd3x, and validates
all 512-byte slices plus shifts with the host BPB rules. It now suppresses
AA55-only garbage by default; pass `--print-aa-only` to show those rows.

Observed 512-page DFU scan result:

```text
N3G_DFU_SCAN_START entries=1024 valid=1017 target=0000A07E pages=512
N3G_DFU_BOOT_CAND j=344 v=0263 po=4   l0=00009400 sig=0B9D bps=512  ... r=1B5
N3G_DFU_BOOT_CAND j=563 v=0288 po=0   l0=00009002 sig=1BDA bps=512  ... r=1B5
N3G_DFU_BOOT_CAND j=563 v=0288 po=128 l0=00009002 sig=023E bps=512  ... r=1B5
N3G_DFU_BOOT_CAND j=724 v=0287 po=288 l0=00009000 sig=01F4 bps=1024 ... r=1B1
N3G_DFU_BOOT_CAND j=523 v=0235 po=64  l0=00008C00 sig=FFC7 bps=512  ... r=185
N3G_DFU_SCAN_DONE read=512 hits=56 rejects=56 boot=0
```

The other printed hits in that run were repeated AA55-only false positives with
`bps=170`, `spc=0`, no FAT string, and `r=1B7`; those are now suppressed by
default. No valid FAT boot sector was found in the 512-page bounded host scan.

Because the MBR partition start is a 512-byte-sector LBA while the current NAND
read path returns 2048-byte pages, the host scanner also tested the derived page
target `0xA07E / 4 = 0x281F`. That scan used the same `b=6166` map and a
512-page cap:

```text
N3G_DFU_SCAN_START entries=1024 valid=1017 target=0000281F span=2048 pages=512
N3G_DFU_SCAN_DONE read=512 hits=16 rejects=16 aa_only_suppressed=16 boot=0
```

The hits were again invalid BPB fragments, for example sane-looking
bytes-per-sector values embedded in unrelated data (`bps=512/1024/2048/4096`)
but with impossible sectors-per-cluster/FAT-count/FAT-string fields. This rules
out the simple "`partition_start / 4`, then choose 512-byte slice 2" hypothesis
for the currently captured map state.

The alternate captured map generation `b=6916 p=0` was also expanded into
`tmp/n3g-capture-cv/map-b6916-entry0-extra.bin` and scanned. Its precise
covering-entry pass matched `b=6166`:

```text
N3G_DFU_SCAN_START entries=1024 valid=1017 target=0000A07E span=2048 pages=54
N3G_DFU_SCAN_DONE read=54 hits=0 rejects=0 aa_only_suppressed=1 boot=0
```

The wider `b=6916` 512-page pass produced the same reject family as `b=6166`
and no boot sector:

```text
N3G_DFU_SCAN_DONE read=512 hits=8 rejects=8 aa_only_suppressed=42 boot=0
```

Content-search mode was added to stop depending on the current LBA transform:

```sh
python3 tools/nano3g_dfu_bpb_scan.py \
  --content-search \
  --search-offsets 0 \
  --max-pages 1024 \
  --progress 128 \
  --read-timeout 4
```

The first run reached `j=488` and exposed a scanner partial-read bug; that is
now handled as `N3G_DFU_READ_SHORT` instead of crashing. Resuming from
`j=489` completed the page-offset-0 content search over the remainder of the
map:

```text
N3G_DFU_MBR_CAND j=982 v=056B po=0 l0=00000000 bank=0 pb=1812 pp=0 slice=0 parts=0C:0000A07E:000E7F81,3F:0000003F:0000A000,00:00000000:00000000,00:00000000:00000000
N3G_DFU_SCAN_DONE read=532 hits=54 rejects=54 aa_only_suppressed=10 boot=0
```

This confirms the content search can rediscover the known MBR from the map, but
there is no valid FAT boot sector at page offset 0 of any valid selected map
entry. Every BPB-like row in the content search was rejected for impossible
fields and no FAT/FAT32 string.

Content-search page offset 1 was also scanned across all valid selected map
entries:

```text
N3G_DFU_SCAN_START entries=1024 valid=1017 target=0000A07E span=2048 pages=1017 mode=content
N3G_DFU_SCAN_DONE read=1017 hits=0 rejects=0 aa_only_suppressed=0 boot=0
```

This is a clean negative result: page offset 1 produced no AA55, sane-BPS, or
FAT-string candidates at all.

Content-search page offset 2 was likewise clean:

```text
N3G_DFU_SCAN_START entries=1024 valid=1017 target=0000A07E span=2048 pages=1017 mode=content
N3G_DFU_SCAN_DONE read=1017 hits=0 rejects=0 aa_only_suppressed=0 boot=0
```

Content-search page offset 3 was also clean:

```text
N3G_DFU_SCAN_START entries=1024 valid=1017 target=0000A07E span=2048 pages=1017 mode=content
N3G_DFU_SCAN_DONE read=1017 hits=0 rejects=0 aa_only_suppressed=0 boot=0
```

Summary for page-offset content search:

- `po=0`: rediscovered the known MBR at `j=982/v=056B`, but no FAT boot
  sector; all BPB-like rows were rejected.
- `po=1`: no candidates.
- `po=2`: no candidates.
- `po=3`: no candidates.

The selected t=44 map can locate the MBR, but the restored FAT boot sector is
not present in the first four page offsets of any valid base-map entry. The
next likely missing layer is a t=45/log/scattered overlay rather than another
simple base-map page-offset transform.

Global metadata OOB index scan:

```sh
python3 tools/nano3g_dfu_bpb_scan.py \
  --scan-meta-pages \
  --meta-start-block 0 \
  --meta-end-block 8191 \
  --meta-start-page 0 \
  --meta-end-page 2 \
  --meta-types 0x44,0x45
```

The scan is OOB-only and found 30 `t=44/t=45` hits across NAND:

```text
N3G_DFU_MSCAN_HIT b=1218 p=1 t=45 idx=0004 u=FFFCC924
N3G_DFU_MSCAN_HIT b=1218 p=2 t=45 idx=000C u=FFFCC924
N3G_DFU_MSCAN_HIT b=1219 p=1 t=45 idx=0004 u=FFFCC924
N3G_DFU_MSCAN_HIT b=1219 p=2 t=45 idx=000C u=FFFCC924
N3G_DFU_MSCAN_HIT b=2070 p=1 t=45 idx=0002 u=FFFCC93E
N3G_DFU_MSCAN_HIT b=2070 p=2 t=45 idx=000A u=FFFCC93E
N3G_DFU_MSCAN_HIT b=2071 p=1 t=45 idx=0004 u=FFFCC93E
N3G_DFU_MSCAN_HIT b=2071 p=2 t=45 idx=000C u=FFFCC93E
N3G_DFU_MSCAN_HIT b=2820 p=1 t=45 idx=0002 u=FFFCC958
N3G_DFU_MSCAN_HIT b=2820 p=2 t=45 idx=000A u=FFFCC958
N3G_DFU_MSCAN_HIT b=2821 p=1 t=45 idx=0004 u=FFFCC958
N3G_DFU_MSCAN_HIT b=2821 p=2 t=45 idx=000C u=FFFCC958
N3G_DFU_MSCAN_HIT b=5314 p=0 t=44 idx=0000 u=FFFCC924
N3G_DFU_MSCAN_HIT b=5314 p=1 t=45 idx=0008 u=FFFCC924
N3G_DFU_MSCAN_HIT b=5314 p=2 t=45 idx=0010 u=FFFCC924
N3G_DFU_MSCAN_HIT b=5315 p=0 t=45 idx=0000 u=FFFCC924
N3G_DFU_MSCAN_HIT b=5315 p=1 t=45 idx=0008 u=FFFCC924
N3G_DFU_MSCAN_HIT b=5315 p=2 t=45 idx=0010 u=FFFCC924
N3G_DFU_MSCAN_HIT b=6166 p=0 t=44 idx=0000 u=FFFCC93E
N3G_DFU_MSCAN_HIT b=6166 p=1 t=45 idx=0006 u=FFFCC93E
N3G_DFU_MSCAN_HIT b=6166 p=2 t=45 idx=000E u=FFFCC93E
N3G_DFU_MSCAN_HIT b=6167 p=0 t=45 idx=0000 u=FFFCC93E
N3G_DFU_MSCAN_HIT b=6167 p=1 t=45 idx=0008 u=FFFCC93E
N3G_DFU_MSCAN_HIT b=6167 p=2 t=45 idx=0010 u=FFFCC93E
N3G_DFU_MSCAN_HIT b=6916 p=0 t=44 idx=0000 u=FFFCC958
N3G_DFU_MSCAN_HIT b=6916 p=1 t=45 idx=0006 u=FFFCC958
N3G_DFU_MSCAN_HIT b=6916 p=2 t=45 idx=000E u=FFFCC958
N3G_DFU_MSCAN_HIT b=6917 p=0 t=45 idx=0000 u=FFFCC958
N3G_DFU_MSCAN_HIT b=6917 p=1 t=45 idx=0008 u=FFFCC958
N3G_DFU_MSCAN_HIT b=6917 p=2 t=45 idx=0010 u=FFFCC958
N3G_DFU_MSCAN_DONE total=24576 hits=30
```

The OOB layout for metadata pages is now confirmed as:

- bytes `0..3`: USN
- bytes `4..5`: metadata index
- byte `9`: metadata type

Capturing the newest map-adjacent cluster `b=6916..6919` shows:

- `b=6916 p=0`: `t=44 idx=0000`
- `b=6916 p=1/p=2`: `t=45 idx=0006/000E`, erased bodies
- `b=6917 p=0`: `t=45 idx=0000`, body is a 16-bit sequential table
  `0..125` followed by `0xFFFF`
- `b=6917 p=1/p=2`: `t=45 idx=0008/0010`, erased bodies
- `b=6918/6919 p=0..7`: user `t=40` pages with LPNs around `0x00107Cxx`

Scanning the `b=6917 p=0` `t=45` body as a bare vblock table over page offsets
`0..3` produced no boot sector:

```text
N3G_DFU_SCAN_START entries=1024 valid=125 target=0000A07E span=2048 pages=500 mode=content
N3G_DFU_SCAN_DONE read=500 hits=11 rejects=11 aa_only_suppressed=3 boot=0
```

Capturing the newest control/log cluster `b=2820..2821` shows:

- `b=2820 p=0`: `t=46 idx=0000`
- `b=2820 p=1/p=2`: `t=45 idx=0002/000A`
- `b=2820 p=3`: `t=43` context
- `b=2821 p=0`: `t=49 idx=0000`, nonempty body
- `b=2821 p=1/p=2`: `t=45 idx=0004/000C`

The non-erased `t=45` bodies in this cluster look like page-offset tables, not
dense vblock maps: `idx=0002` has valid entries only at indices `894..1023`,
and `idx=0004` has valid entries at `952..1023`. The `t=49` body is the next
candidate for the scattered/log table tying those offset pages to scattered
vblocks and logical blocks.

Host-side replay and metadata tooling added:

```sh
python3 tools/nano3g_ftl_replay.py tmp/n3g-capture-cv/replay-near.log --target-lba 0xA07E
python3 tools/nano3g_meta_analyze.py tmp/n3g-capture-cv/meta-2820 tmp/n3g-capture-cv/meta-6166 tmp/n3g-capture-cv/meta-6916
```

The DFU BPB scanner can now write replayable raw body/OOB logs instead of
only screen summaries:

```sh
python3 tools/nano3g_dfu_bpb_scan.py \
  --content-search \
  --only-j 61,462 \
  --search-offset-range 280:300 \
  --dump-pages-log tmp/n3g-capture-cv/j61-j462-po280-300.log \
  --dump-all-pages
```

The focused `j=61/j=462` capture confirms the exact/mbrbase candidates are
not valid boot-sector locations and also explains why: the bank-0 user OOB
logical field advances as `l = l0 + 2 * po`. Examples:

```text
j=462 po=280 t=40 l=00009E30
j=462 po=284 t=40 l=00009E38
j=462 po=288 t=40 l=00009E40
j=61  po=280 t=40 l=00009E32
j=61  po=288 t=40 l=00009E42
```

For partition start `0xA07E`, `j=462/l0=0x9C00` would require
`po=(0xA07E - 0x9C00) / 2 = 575`, which is outside a 512-page block. Under
this corrected model, neither `j=61` nor `j=462` covers the partition boot
sector. A quick map-entry check over both captured map pages shows no base-map
entry with `l0` in `0x9C80..0xA07E`, i.e. no base map entry covers the
`0xA000` logical region with the observed `2 * po` layout. The earlier
`span=2048` searches were therefore too permissive and selected false
covering candidates.

`tools/nano3g_dfu_bpb_scan.py` now defaults to this corrected `span=1024`
model via `--logical-units-per-page 2`; the same target lookup now prints
`pages=0` instead of re-testing the false `j=61/j=462` candidates.

`tools/nano3g_meta_analyze.py` confirms the current non-empty offset tables:

```text
N3GM_T45_TABLE b=6167 p=0 idx=0000 table=0000 valid=126 lp_first=0   lp_last=125 po_min=0 po_max=125 seq=1
N3GM_T45_TABLE b=6917 p=0 idx=0000 table=0000 valid=126 lp_first=0   lp_last=125 po_min=0 po_max=125 seq=1
N3GM_T45_TABLE b=2820 p=1 idx=0002 table=0003 valid=130 lp_first=382 lp_last=511 po_min=0 po_max=129 seq=1
N3GM_T45_TABLE b=2821 p=1 idx=0004 table=0005 valid=72  lp_first=440 lp_last=511 po_min=2 po_max=73  seq=0
```

The important implication is that the missing partition boot-sector region is
likely in the scattered/log overlay rather than in the base `t=44` map. The
next step is to decode which logical base each populated `t=45` offset table
belongs to, using the `t=43` context references and the `t=49` body.

Update: host LBA vs OOB raw units
---------------------------------

The `0xA07E` partition start from the MBR is a 512-byte host-sector LBA, not
the raw OOB logical field used in the user pages. The observed user OOB field
advances by two raw units per bank-0 page offset in the focused
`j=61/j=462` capture, so the current scanner treats host LBA `0xA07E` as raw
target `0x140FC` (`host << 1`) for map-entry matching.

The DFU scanner and replay tool now log both values:

```text
host=0000A07E raw=000140FC
```

Running the corrected target lookup against both captured base maps finds no
covering `t=44` base-map entry:

```text
N3G_DFU_SCAN_START entries=1024 valid=1017 host=0000A07E raw=000140FC span=1024 pages=0
N3G_DFU_MAP_NEAR rank=0 j=701 v=05A0 t=40 l0=00014402 end=00014801 dist=774
N3G_DFU_MAP_NEAR rank=1 j=150 v=044C t=40 l0=00014802 end=00014C01 dist=1798
N3G_DFU_MAP_NEAR rank=2 j=291 v=0375 t=40 l0=00013000 end=000133FF dist=3325
```

A focused DFU dump of the nearest base-map entries was captured to:

```text
tmp/n3g-capture-cv/raw140fc-near.log
```

Replay of that dump still finds no boot sector:

```text
REPLAY_NOBOOT host=0000A07E raw=000140FC covering=0 near=12 printed=0
```

The nearest-entry dump confirms the base map itself skips over raw target
`0x140FC`; examples around the closest entries:

```text
j=701 po=0   l=00014402
j=701 po=288 l=00014642
j=150 po=0   l=00014802
j=347 po=0   l=00015000
```

Conclusion: the partition boot sector is not in the current base `t=44` map
page. The next useful work is decoding the overlay/log metadata (`t=45`/`t=49`
and `t=43` references), not further broad BPB scans of the base map.

`tools/nano3g_meta_analyze.py` is now also target-aware and prints both the
host and raw target:

```text
N3GM_START pages=80 host=0000A07E raw=000140FC
```

For the current `t=49` page it finds target-index hints but no direct raw
target hit:

```text
N3GM_T49_HINT name=host_512 value=0050 hits=281,361 count=2
N3GM_T49_HINT name=raw_512  value=00A0 hits=510 count=1
```

Those hints are not enough to read the boot sector yet, but they give the next
offline target for decoding the `t=49` structure.

Testing the most obvious `t=49` hint as a direct vblock did not locate the
boot sector. The row containing `raw_512=00A0` has nearby value `050E`, but
direct reads of `v=050E po=248..260` produced user OOB around `000CC1FA`, not
the target `000140FC`:

```text
N3GD_BEGIN ... v=050E po=252 ... t=40 ... l=000CC1FA
N3G_DFU_SCAN_DONE read=13 hits=0 boot=0
```

Other adjacent small fields from that row (`00E8`, `0101`, `0003`) were also
tested over `po=248..260`; they produced unrelated OOB logical ranges
(`0019C9FA`, `000EFDF8`, `000605F8`) and no valid BPB.

A new `--scan-oob-target` mode was added to search OOB/spare directly for the
raw target without reading page bodies unless the OOB logical value matches.
It is intentionally compact and now aborts after repeated DFU/no-file failures
instead of looping across the whole device on a broken session:

```text
N3G_DFU_OOB_TARGET_START bank=0 ... host=0000A07E raw=000140FC
N3G_DFU_OOB_TARGET_ABORT failures=2 reason=no_file
```

The first attempted full OOB pass left a stale `wInd3x` holder on the DFU USB
interface; it was identified with `fuser` and terminated. Subsequent reads
returned `ClrStatus` I/O errors, so the device likely needs a fresh DFU
session before the direct OOB target scan can be run for real.

After a fresh DFU session, the direct OOB target scan completed successfully.
Scanning bank 0 page 0 of each physical block, with a stride of 128 pages,
found 52 OOB entries near raw target `0x140FC`. The strongest discovery was
not the primary FAT boot sector, but a nested MBR-like sector:

```text
phy=403968 pb=3156 pp=0 t=40 l=0001407E ix=DE53
body sig=AA55 p0 type=0B start=0000003F size=000E7F81
```

This sector's relative partition start explains the earlier target:
`0x1407E + (0x3F << 1) = 0x140FC`.

The primary sector at raw `0x140FC` still appears absent/zero in the captured
material, but the FAT32 backup boot sector is valid:

```text
v=0AAC po=68 pb=3157 pp=17 t=40 l=00014108
sig=AA55 bps=4096 spc=1 rs=32 nf=2 fatsz=927 fs='FAT32   '
```

`0x14108` is `0x140FC + 12`, matching FAT32 backup boot sector number 6 when
OOB logical values are counted in 512-byte units. The current bootloader
diagnostic therefore keeps the restored Windows MBR unchanged, but if normal
partition-start lookup fails it validates `v=0AAC po=68` and presents that
backup boot sector for the partition-start read. It also synthesizes a minimal
read-only FSInfo sector at the BPB-derived location so `fat_mount()` can get
past the mandatory FSInfo signature check. This is a narrow mount probe, not a
general Nano 3G FTL decode.

Build `20260504cw` contains this backup-boot mount probe:

```text
N3G_EXEC_WMOUNT_CW_BACKUP_BOOT
N3G_WMOUNT_BKTRY ...
N3G_WMOUNT_BOOTSECTOR_OK backup ...
```

The test DFU image generated from that build is:

```text
tmp/nano3g-wmount-backup-boot.dfu
```

It was sent successfully with `mks5lboot --dfusend` after clearing DFU state
with `--dfureset`.

On-device `20260504cw` confirmed the narrow backup-boot mount path is usable:

```text
N3G_WMOUNT_MBR_OK j=982 v=056B po=0 s1=0 l0=00000000
N3G_WMOUNT_PART_OK p=0 t=0C st=0000A07E sz=000E7F81
N3G_WMOUNT_ENTRY v=0AAC po=68 rc=0 t=40 ...
N3G_WMOUNT_BOOTSECTOR_OK backup=0 po=68 s1=0 bps=512
N3G_WMOUNT_S0 rc=0 sig=AA55 part=0 t=0C st=0000A07E
N3G_WMOUNT_SF rc=0 j=4294967295 po=68 sig=AA55,...
N3G_WMOUNT_READY mbrj=982 mbrpo=0 xmap=1 st=0000A07E
Loading Rockbox...
Error!
Can't load rockbox.ipod:
File not found
```

This moves the failure point from NAND/FTL mount to the expected Rockbox file
load. The restored Windows/FAT volume was then mounted in Apple disk mode at
`/run/media/david/IPOD`; `build-native-ipodnano3g/rockbox.zip` was extracted to
the FAT volume and `build-native-ipodnano3g/rockbox.ipod` was copied to both
`.rockbox/rockbox.ipod` and root `/rockbox.ipod`. All three `rockbox.ipod`
copies matched SHA-256 `af9fd6b08a72dd94d2fe36fb050e26cab81951bbc80e49a8e45fd4eb80b29338`
before syncing.

After installing files, a repeat run of `20260504cw` regressed to
`N3G_WMOUNT_BACKUP_NOBOOT`: the same hard-coded backup candidate still read,
but its OOB logical tag no longer matched the earlier raw target. Build
`20260504cx` restores the useful behavior by treating the known backup sector
as a body-validated mount bridge: `v=0AAC po=68 s1=0` is accepted when the
sector body has `AA55` and a sane BPB bytes-per-sector value, independent of
the stale OOB raw value. The bootloader also now prints:

```text
N3G_LOAD_PATH /rockbox.ipod
```

immediately before `load_firmware()` so Rockbox file-load failures can be
distinguished from NAND/FTL mount failures. The corresponding test DFU is:

```text
tmp/nano3g-wmount-backup-restore.dfu
```

Build `20260504cy` is the stop/revert point for the post-success WMOUNT
experiments. It disables the later broad target/FAT/BPB searches on the live
boot path and keeps only the proven sequence:

```text
N3G_WMOUNT_MBR_OK j=982 v=056B po=0 l0=00000000 sig=AA55
N3G_WMOUNT_PART_OK p=0 t=0C st=0000A07E sz=000E7F81
N3G_WMOUNT_BOOTSECTOR_OK backup=0 po=68 s1=0 bps=512
N3G_WMOUNT_READY ... xmap=1 ...
```

The hard regression guard is: after `MBR_OK` and `PART_OK`, `v=0AAC po=68
s1=0` must be selected when its body validates with `sig=AA55` and `bps=512`.
It must not fall through to `NOBOOT` while that candidate is valid. The
remaining issue is file loading from the mounted FAT volume; the bootloader
prints `N3G_LOAD_PATH /rockbox.ipod` immediately before opening the image.

```text
tmp/nano3g-wmount-known-backup-only.dfu
```

Build `20260504da` fixes a caller-state regression observed after the compact
known-backup-only build: the backup candidate could print `BACKUP_ACCEPT` but
still fall through to `NOBOOT`/`xmap=0`. The helper now prints
`BACKUP_ACCEPT` immediately before `BOOTSECTOR_OK`, and the caller reasserts
the selected state after any accepted backup:

```text
selected.backup = 0
selected.v = 0x0AAC
selected.po = 68
selected.s1 = 0
xmap = 1
```

After `BACKUP_ACCEPT`, `NOBOOT` is a regression. The test DFU for this guard is:

```text
tmp/nano3g-wmount-backup-accept-guard.dfu
```

Build `20260504db` moves the known backup guard to raw bytes instead of the
BPB validator output. It prints compact diagnostics for the exact candidate:

```text
N3G_WMOUNT_BKVAL sig=<raw> bps=<raw> spc=<parsed> rs=<parsed> nf=<parsed> fat=<parsed> r=<reason>
N3G_WMOUNT_BKFS fs=<word0>,<word1>
N3G_WMOUNT_BKREJ rc=<rc> sig=<raw> bps=<raw> r=<reason>
```

For this one proven candidate only, `sig=AA55` and raw `bps=512` force
`BACKUP_ACCEPT`/`BOOTSECTOR_OK` even if the newer BPB validator returns another
reason. The DFU image is:

```text
tmp/nano3g-wmount-raw-backup-guard.dfu
```

After Rockbox files were copied in Apple disk mode, the old hard-coded backup
candidate stopped being valid:

```text
N3G_WMOUNT_BKREJ rc=0 sig=9769 bps=1024 spc=57
```

That means `v=0AAC po=68 s1=0` was a pre-write physical clue, not a stable
mount invariant. Build `20260504dc` removes that candidate from the live mount
path and searches the current map for a FAT boot sector body after the valid
MBR/partition parse. It prints only compact boot-sector candidates:

```text
N3G_BOOTSECTOR_CAND j=<j> v=<v> po=<po> sl=<slice> sh=<shift> ...
N3G_BOOTSECTOR_BPB sig=<sig> bps=<bps> spc=<spc> rs=<reserved> nf=<fats> fz=<fatsz> root=<root_cluster> r=<reason>
N3G_BOOTSECTOR_OK ...
N3G_BOOTSECTOR_NOT_FOUND ...
```

The bootloader-side file open marker is now:

```text
N3G_LOAD_PATH path=/rockbox.ipod
```

The corresponding DFU image is:

```text
tmp/nano3g-wmount-current-boot-search.dfu
```

Build `20260504dd` fixes a misleading state report from the first current-boot
search build: `xmap=1` must mean `BOOTSECTOR_OK` actually selected a sector.
It also adds direct read diagnostics after the MBR is exposed:

```text
N3G_DREAD0 rc=<rc> sig=<sig> p0=<type> st=<start> sz=<size>
N3G_DREADP lba=0000A07E rc=<rc> sig=<sig> bps=<bps> spc=<spc> rs=<reserved> nf=<fats> fat=<fat> r=<reason>
N3G_DREADP_W w=<w0>,<w1>,<w2>,<w3>
```

These distinguish “partition scanner can read LBA 0 but cannot translate the
partition start” from “custom boot-sector scan failed but normal direct reads
are already good.” The DFU image is:

```text
tmp/nano3g-wmount-direct-read-test.dfu
```

Build `20260504de` disables the boot-sector candidate scanner entirely for the
live test path. The only new diagnostic is run from `bootloader/ipod-s5l87xx.c`
after `storage_init()` and before `filesystem_init()` / `disk_mount_all()`,
using the same public storage path as Rockbox partition code:

```text
storage_read_sectors(0, 1, buf)
storage_read_sectors(0x0000A07E, 1, buf)
```

It prints only:

```text
N3G_DIRECT_LBA0 rc=<rc> sig=<sig> p0t=<type> p0st=<start> p0sz=<size>
N3G_DIRECT_BOOT rc=<rc> sig=<sig> bps=<bps> spc=<spc> rs=<reserved> nf=<fats> fz=<fatsz> ts=<total> fatstr=<8 chars>
```

There should be no `N3G_BOOTSECTOR_CAND`, `N3G_BOOTSECTOR_BPB`, `FSCAN`, or
`FXSCAN` output in this build. The DFU image is:

```text
tmp/nano3g-storage-direct-only.dfu
```

Build `20260505a` keeps the compact public-storage diagnostic from
`20260504de`, but fixes the direct WMOUNT read path to resolve nonzero LBAs by
range rather than exact match. The current WinPod fixture is MBR/FAT32-LBA:

```text
p0 type=0C start=0000A07E size=000E7F81
```

Offline replay tests now model the observed covering entry behavior:

```text
l0=0000A07C covers lba=0000A07E with delta=2, po=0, slice=2
```

The helper shape is:

```text
find_entry_covering_lba(lba) -> entry + sector_delta
```

The expected live output remains compact:

```text
N3G_BUILD 20260505a
N3G_EXEC_WMOUNT_EA_RANGE_DELTA
N3G_DIRECT_LBA0 rc=0 sig=AA55 p0t=0C p0st=0000A07E p0sz=000E7F81
N3G_DIRECT_BOOT rc=0 sig=AA55 bps=512 ...
```

The offline test command was:

```text
python3 tools/test_nano3g_ftl_replay.py
```

The DFU image is:

```text
tmp/nano3g-wmount-range-delta.dfu
```

Build `20260505b` adds the next direct-read translation trace. The public
storage diagnostic now also probes `1`, `0x3F`, `0xA07C`, and `0xA080` so the
FTL shim prints the selected entry for each interesting LBA. The lookup trace
now includes:

```text
N3G_READ lba=<requested> key=<lookup> rc=<rc> j=<j> v=<v> l0=<base> span=<span> d=<delta> po=<po> s1=<slice> reason=<reason>
N3G_READ_PHY lba=<requested> pb=<physical_block> pp=<physical_page> bank=<bank>
```

The MBR entry is stored separately. If any nonzero LBA tries to reuse that
entry, the code prints and rejects it:

```text
N3G_BAD_MBR_REUSE lba=<lba> j=<mbr_j> v=<mbr_v> l0=<base> span=<span> d=<delta> po=<po> s1=<slice>
```

For the WinPod direct diagnostic, each requested disk LBA is translated as the
first 512 bytes of the returned buffer; adjacent 512-byte slices are not allowed
to turn a good target-sector read into `rc=-6`.

The DFU image is:

```text
tmp/nano3g-wmount-direct-lba-trace.dfu
```

Build `20260505c` stops live WMOUNT tuning and performs one offline replay dump
pass. After `MBR_OK` and `PART_OK`, it returns after exporting raw capture
records rather than continuing through boot-sector scans or mount attempts:

```text
N3G_BUILD 20260505c
N3G_EXEC_WMOUNT_EC_REPLAY_DUMP
N3GD_START profile=winpod_mbr_fat32 target=0000A07E ppb=512
N3GM_START selected_map_b=<block> selected_map_p=<page> mbr_j=982 ...
N3GM_TEST_LBA lba=00000000
N3GM_TEST_LBA lba=00000001
N3GM_TEST_LBA lba=0000003F
N3GM_TEST_LBA lba=0000A07C
N3GM_TEST_LBA lba=0000A07E
N3GM_TEST_LBA lba=0000A080
N3GM_ENTRY mapb=<block> mapp=0 j=<j> v=<v> l0=<base> span=<span> t=<type> po=<po> tgt=0000A07E d=<delta>
N3GD_BEGIN id=<n> kind=map|mbr|cand ...
N3GD_BODY id=<n> off=<offset> <16-byte-hex>
N3GD_OOB id=<n> off=<offset> <16-byte-hex>
N3GD_END id=<n>
N3GD_DONE pages=<n>
N3G_WMOUNT_DUMP_DONE offline_replay
```

The dump includes the selected map page plus known map candidates `6916/0`,
`6166/0`, and `6912/0`, the confirmed MBR physical page, and candidate pages
whose decoded logical ranges overlap `00009C00..0000A200` or cover the
partition start. Save the screen/serial capture as text and replay it with:

```text
python3 tools/nano3g_wmount_replay.py capture.txt
```

The replay harness is WinPod-specific for this profile: MBR signature at
`0x1FE`, partition entries at `0x1BE`, and FAT32 types `0B/0C`. It enforces
range containment for map lookup and rejects MBR-entry reuse for nonzero LBAs.
The unit tests are:

```text
python3 tools/test_nano3g_wmount_replay.py
python3 tools/test_nano3g_ftl_replay.py
```

The DFU image is:

```text
tmp/nano3g-wmount-replay-dump.dfu
```

Build `20260505d` narrows the replay dump to fixed targets only. It does not
dump nearest-score or arbitrary candidate pages. The required targets are:

```text
N3GD_DUMP_MAP b=6916 p=0
N3GD_DUMP_MAP b=6166 p=0
N3GD_DUMP_MAP b=6912 p=0
N3GD_DUMP_PAGE target=page_j982_v056B j=982 v=056B po=0
N3GD_DUMP_PAGE target=page_j61_v0384 j=61 v=0384 po=287
N3GD_DUMP_PAGE target=page_j61_v0194 j=61 v=0194 po=0
N3GD_DUMP_PAGE target=page_j462_v0383 j=462 v=0383 po=287
```

Every body/OOB chunk includes `target=<name>` and `off=<offset>` so a screen or
serial capture can be split back into binary fixtures. File writes are not used
in this path because the dump happens during storage init, before the Rockbox
filesystem layer is available.

The build marker is:

```text
N3G_BUILD 20260505d
N3G_EXEC_WMOUNT_ED_TARGET_DUMP
```

The LCD dump path is now retired. The practical transport is host-side DFU
capture with wInd3x, which writes binary fixtures directly on the computer:

```text
python3 tools/nano3g_wmount_dfu_dump.py --out tmp/n3g-dump --entry-oob-chunk 32
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump
```

The host dump writes:

```text
tmp/n3g-dump/manifest.txt
tmp/n3g-dump/n3g_wmount_manifest.txt
tmp/n3g-dump/map_b6916_p0.bin
tmp/n3g-dump/map_b6916_p0.oob
tmp/n3g-dump/map_b6916_p0.entries.oob
tmp/n3g-dump/map_b6166_p0.bin
tmp/n3g-dump/map_b6166_p0.oob
tmp/n3g-dump/map_b6166_p0.entries.oob
tmp/n3g-dump/map_b6912_p0.bin
tmp/n3g-dump/map_b6912_p0.oob
tmp/n3g-dump/map_b6912_p0.entries.oob
tmp/n3g-dump/page_j982_v056B.bin
tmp/n3g-dump/page_j982_v056B.oob
tmp/n3g-dump/page_j61_v0384.bin
tmp/n3g-dump/page_j61_v0384.oob
tmp/n3g-dump/page_j61_v0194.bin
tmp/n3g-dump/page_j61_v0194.oob
tmp/n3g-dump/page_j462_v0383.bin
tmp/n3g-dump/page_j462_v0383.oob
```

The first real binary replay confirms the MBR and partition table, but still
fails the boot sector:

```text
WMREPLAY_MBR_OK j=982 v=056B po=0 slice=0 sig=AA55
WMREPLAY_PART_OK p0t=0C p0st=0000A07E p0sz=000E7F81
LBA 0000A07E: ... j=61 v=0384 l0=00009C02 ... sig=0000 bps=0
WMREPLAY_BOOT_FAIL lba=0000A07E sig=0000 bps=0 ...
```

A host-side full-page-offset sweep of the two entries covering `0xA07E`
(`j=61/v=0384` and `j=462/v=0383`) found only false candidates with invalid
BPB fields, not a valid FAT boot sector. No new device build should be flashed
until the host replay finds a valid translation.

The replay harness now has a transform search mode:

```text
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump --search-boot --target-lba 0xA07E --try-transforms
```

It tests these raw logical interpretations while still requiring range
containment:

```text
host
host +/- 2
host * 2
host * 4
host / 2 when even
host / 4 when divisible
partition_start + relative_lba
partition_start - mbr_l0
partition_start +/- 2
```

It prints only near-valid boot candidates (`AA55`, sane bytes-per-sector, or a
FAT/FAT32 string) and passes only on a sane BPB. The current real fixture still
fails:

```text
WMREPLAY_MBR_OK j=982 v=056B po=0 slice=0 sig=AA55
WMREPLAY_PART_OK p0t=0C p0st=0000A07E p0sz=000E7F81
WMREPLAY_BOOT_NOT_FOUND lba=0000A07E transforms=10 covers=16 pages=20 missing_pages=6 cands=0
```

For diagnostics, `--print-covers` prints the covered entries and the exact
`delta/po/slice` selected for each transform, but that output is intentionally
off by default.

The replay harness can now use a disk-mode sector as ground truth. Once the
iPod is in Apple disk mode and the host sees it as the `iPod` USB disk, capture
the MBR sector and partition-start sector read-only:

```text
mkdir -p tmp/n3g-diskmode
sudo dd if=/dev/sdb of=tmp/n3g-diskmode/lba0.bin bs=512 count=1 skip=0 status=none
sudo dd if=/dev/sdb of=tmp/n3g-diskmode/lba_A07E.bin bs=512 count=1 skip=41086 status=none
```

On the current host the iPod appeared as `/dev/sdb` with its FAT volume mounted
at `/run/media/david/IPOD`; the external portable SSD was `/dev/sda`, so confirm
the device node before running `dd`. Raw `/dev/sdb` reads require sudo on this
host. The replay-side reference search is:

```text
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump --reference-sector tmp/n3g-diskmode/lba_A07E.bin
```

It scans every dumped NAND page body for an exact 512-byte match and near
matches, printing `WMREPLAY_REF_CAND` and `WMREPLAY_REF_OK` with the decoded
`j/v/po/l0`, body offset, slice, shift, signature, and BPB fields. This avoids
another live firmware flash: the next WMOUNT change should be derived from the
disk-mode sector fingerprint matched against the existing DFU dump.

After restoring/copying files in disk mode, the host-visible layout changed
from the earlier live WMOUNT logs:

```text
tmp/n3g-diskmode/lba0.bin:
  sig=AA55
  p0 type=0B start=0000003F size=000E7F81

tmp/n3g-diskmode/lba_3F.bin:
  sig=0000 bps=0

tmp/n3g-diskmode/sdb1_lba0.bin:
  sig=AA55 jump=EB3C90 bps=4096 spc=1 rs=32 nf=2
  total=950145 fatsz32=927 root=2 fatstr='FAT32   '
```

The existing DFU dump does not contain an exact match for either the current
raw LBA 0 sector or the current `/dev/sdb1` FAT boot sector:

```text
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump --reference-sector tmp/n3g-diskmode/lba0.bin
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump --reference-sector tmp/n3g-diskmode/sdb1_lba0.bin
```

Both report `WMREPLAY_REF_NOT_FOUND`, while the stale raw `0x3F` sector matches
slice 1 of the old MBR dump as all-zero data. Conclusion: `tmp/n3g-dump` was
captured from an older NAND state. Do not tune WMOUNT against that fixture for
the current restored WinPod. The next useful step is a fresh DFU dump from the
current restored disk state, then rerun the reference-sector search against
`sdb1_lba0.bin`.

Fresh DFU dump from the restored disk state found the current host-visible
sectors. A targeted OOB signature scan found:

```text
N3G_DFU_FINDSIG_HIT bank=0 block=1812 page=231936 raw=00000000 lpn=0 lpn512=0 type=40 usn=0000de4d
N3G_DFU_FINDSIG_HIT bank=0 block=1524 page=195072 raw=0001407e lpn=10255 lpn512=41023 type=40 usn=0000de94
```

Dumping block 1524 page 0 showed slice 0 is an exact match for the host raw
`/dev/sdb` LBA 0 sector. Its OOB logical value is `0x0001407e`, so current raw
disk LBA 0 is not Whimory LPN 0.

Additional targeted scans around vblock `0x044b` found the host FAT partition
boot sector:

```text
WMREPLAY_REF_OK file=tmp/n3g-diskmode/sdb1_lba0.bin target=scan_v044B_po128_l000140FC j=4294967295 v=044B po=128 l0=000140FC off=0000 slice=0 shift=0
```

Current restored WinPod sector anchors:

```text
raw disk LBA 0  -> v=044B po=0   OOB l=0001407E slice=0 sig=AA55 p0 type=0B start=0000003F
raw disk LBA 3F -> v=044B po=128 OOB l=000140FC slice=0 sig=AA55 bps=4096 spc=1 rs=32 nf=2 FAT32
```

The resulting transform for the host-visible raw disk view is
`oob_l = 0x1407e + 2 * host_lba`, but page placement is not linear: the boot
sector for host LBA `0x3f` is at `po=128`, not the simple arithmetic page
offset. The live diagnostic now uses a narrow overlay for these two proven
current sectors so `storage_read_sectors(0)` and `storage_read_sectors(0x3f)`
exercise the same path Rockbox partition/FAT code uses.

Updated offline replay result:

```text
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump-current \
  --current-winpod \
  --lba0-reference tmp/n3g-diskmode/lba0.bin \
  --boot-reference tmp/n3g-diskmode/sdb1_lba0.bin

WMREPLAY_CUR_MBR_OK target=page_b1524_p0 v=044B po=0 slice=0 l=0001407E sig=AA55 p0t=0B p0st=0000003F p0sz=000E7F81
WMREPLAY_CUR_BOOT_CAND target=scan_v044B_po128_l000140FC v=044B po=128 l=000140FC page_l=000140FC slice=0 sig=AA55 bps=4096 spc=1 rs=32 nf=2 fz=927 ts=950145 fatstr='FAT32   ' r=000
WMREPLAY_CUR_OK base=0001407E scale=2 boot_lba=0000003F boot_l=000140FC
```

The replay now models page containment for the current WinPod view: a 2048-byte
page covers four 512-byte slices at OOB logical values `l`, `l+2`, `l+4`, and
`l+6`. It also prefers the active vblock from the confirmed MBR page (`v=044B`)
over stale exact-OOB matches from adjacent vblocks. This fixes the previous
bad reuse where nonzero LBAs could select the MBR entry or a stale exact page.

Current BPB-derived read plan:

```text
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump-current \
  --current-plan \
  --lba0-reference tmp/n3g-diskmode/lba0.bin \
  --boot-reference tmp/n3g-diskmode/sdb1_lba0.bin

WMREPLAY_CUR_PLAN_BPB base=0001407E active_v=044B pstart=0000003F psize=000E7F81 bps=4096 scale=8 spc=1 rs=32 nf=2 fatsz=927 fsinfo=1 backup=6 root_cluster=2
WMREPLAY_CUR_PLAN boot  lba=0000003F raw=000140FC rc=0 ... sig=AA55 bps=4096 ... r=000
WMREPLAY_CUR_PLAN fat1  lba=0000013F raw=000142FC rc=-1 reason=missing
WMREPLAY_CUR_PLAN fat2  lba=00001E37 raw=00017CEC rc=-1 reason=missing
WMREPLAY_CUR_PLAN root  lba=00003B2F raw=0001B6DC rc=-1 reason=missing
```

Conclusion: the offline environment now proves MBR and FAT boot-sector reads
for the restored WinPod. The next missing data is not another boot-sector
heuristic; it is the backing pages for FAT and root-directory LBAs. Do not flash
another bootloader until those pages are captured and replayed. Use
`tools/nano3g_wmount_dfu_dump.py` range scans to fetch only OOB logical windows
around `0x142fc`, `0x17cec`, and `0x1b6dc`. The dump tool now accepts
`--scan-v-min/--scan-v-max` so a bounded vblock range can be scanned without
LCD hex output.

DFU transport note: for bootloader payloads that must show LCD output, use
`./utils/mks5lboot/mks5lboot --dfusend <payload.dfu>`. The old bounded context
trace payload displayed correctly through this path. Do not use `wInd3x run`
for LCD bootloader payload validation; it uploaded payloads but produced black
screen/no visible execution in this session.

## 2026-05-05 offline replay update

The replay harness now has an explicit regression for the range/delta bug that
caused nonzero reads to choose the wrong page offset. For a map entry with a
nonzero base page:

```text
entry: j=332 v=025A l0=0001B372 po0=440
target raw lba: 0001B6DC
delta: 874
correct page offset: po0 + delta/4 = 658
slice: 2
```

The old model used only `delta/4`, which selected `po=218` and could silently
fall back to an unrelated page. Replay now:

- computes `po_final = entry.po0 + delta / sectors_per_page`
- rejects missing exact pages instead of falling back to the first page with
  the same `j/v/l0`
- prefers actual OOB logical `l=` over requested dump hints `l0=`
- has 10 passing unit tests in `tools/test_nano3g_wmount_replay.py`

The latest verification commands:

```text
python3 -m py_compile tools/nano3g_wmount_replay.py tools/nano3g_ftl_replay.py tools/nano3g_wmount_dfu_dump.py
python3 tools/test_nano3g_wmount_replay.py
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump-current --lba 0x1B6DC --print-covers --ppb 512
```

The root-directory target still has no captured exact backing page:

```text
LBA 0001B6DC: rc=-1 reason=nopage j=332 v=025A l0=0001B372 span=2048 delta=874 po_base=440 po_final=658 slice=2
```

Dump attempts at `v=025A/po=218`, `po=438`, and `po=658` showed unrelated OOB
logical values, so `v` from the map table is not always a direct physical
vblock address under the simple `v + syshyperblocks` formula. The next offline
task is to find the physical page whose OOB logical is in the root window,
without broad unbounded scans.

Operational note: a broad physical OOB scan can keep a `wInd3x` child holding
the DFU interface. If that happens, `fuser -v /dev/bus/usb/<bus>/<dev>` shows
the holder. In this session the parent was inside the tool runner and respawned
children, so the safe recovery was:

```text
mv tmp/wInd3x-src/wInd3x tmp/wInd3x-src/wInd3x.hold
kill TERM on current fuser holder until the parent fails
mv tmp/wInd3x-src/wInd3x.hold tmp/wInd3x-src/wInd3x
```

Use that only for stuck host-side dump processes; it does not write to the iPod.

`tools/nano3g_wmount_dfu_dump.py` now has bounded physical scan controls:

```text
--scan-max-chunks <n>
--scan-progress-chunks <n>
```

A safety check over bank 0 stopped cleanly after 16 chunks:

```text
N3GD_PSCAN_PROG bank=0 chunk=0 pb=0 pp=0
N3GD_PSCAN_PROG bank=0 chunk=4 pb=16 pp=0
N3GD_PSCAN_PROG bank=0 chunk=8 pb=32 pp=0
N3GD_PSCAN_PROG bank=0 chunk=12 pb=48 pp=0
N3GD_PSCAN_STOP bank=0 chunks=16
N3GD_DUMP_DONE files=1 hits=0
```

## 2026-05-06 restored WinPod replay update

After restoring with Windows iTunes and adding Rockbox files, the disk-mode
fixture changed to a normal WinPod MBR/FAT32-LBA layout:

```text
host LBA0:          MBR sig=AA55 p0 type=0B start=0000003F size=000E7F81
/dev/sdb1 LBA0:     FAT32 boot sig=AA55 bps=4096 spc=1 rs=32 nf=2 fatsz=927
```

The current replay model that matches captured NAND is:

```text
raw_oob_l = mbr_oob_l + 2 * host_lba
mbr_oob_l = 0001407E
boot lba  = 0000003F
boot_oob_l = 000140FC
```

Verified replay command:

```text
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump-current \
  --current-winpod \
  --lba0-reference tmp/n3g-diskmode/lba0.bin \
  --boot-reference tmp/n3g-diskmode/sdb1_lba0.bin
```

Expected result:

```text
WMREPLAY_CUR_MBR_OK ... v=044B ... l=0001407E sig=AA55 p0t=0B p0st=0000003F
WMREPLAY_CUR_BOOT_CAND ... l=000140FC sig=AA55 bps=4096 spc=1 ...
WMREPLAY_CUR_OK base=0001407E scale=2 boot_lba=0000003F boot_l=000140FC
```

For later filesystem sectors the active physical vblock can be encoded with
high flag bits in the map entry. Example:

```text
map b=6912: j=110 v=1CA5 l0=0001B404
physical vblock = v & 0x0FFF = 0CA5
physical pblock = 0CA5 + syshyperblocks(425) = 3662
```

A bounded first-page physical OOB scan found the continuation:

```text
N3GD_PSCAN_HIT bank=0 pb=3662 pp=90 t=40 l=0001B6D0
N3GD_PSCAN_HIT bank=0 pb=3662 pp=91 t=40 l=0001B6D8
N3GD_PSCAN_HIT bank=0 pb=3662 pp=92 t=40 l=0001B6E0
```

Firmware-side fixes made from replay:

- mask high vblock flags in `ftl_n3g_decode_page()` with `vblock &= 0x0FFF`
- present storage reads as 512-byte sectors, not 2048-byte buffer strides
- translate host sectors as `raw = mbr_base_lpn + 2 * host_lba`
- for map-derived reads, compute the provisional page as `(raw - entry_l0)/2`
  and then derive the final 512-byte slice from the actual OOB `l` value
- keep the confirmed boot sector as a special direct read target when found

Build verification passed:

```text
python3 -m py_compile tools/nano3g_wmount_replay.py tools/nano3g_wmount_dfu_dump.py tools/nano3g_ftl_replay.py
python3 tools/test_nano3g_wmount_replay.py
make -C build-bootloader-ipodnano3g
./utils/mks5lboot/mks5lboot --mkdfu-inst build-bootloader-ipodnano3g/bootloader-ipodnano3g.ipod tmp/nano3g-wmount-sectorfix.dfu
```

## 2026-05-06 A07E bank/slice correction

Clearer live output showed the logical key transform was already correct:

```text
lba=0000A07C key=000140F8
lba=0000A07E key=000140FC
lba=0000A080 key=00014100
```

The failure was physical page selection, not a key offset bug:

```text
A07C -> j=150 v=044C pb=1525 pp=15 bank=0 rc=0
A07E -> j=150 v=044C pb=1525 pp=15 bank=2 rc=-6
A080 -> j=150 v=044C pb=1525 pp=16 bank=0 rc=0
```

Targeted DFU dump for `j=150/v=044C` confirmed that only the bank0 pages
carry the real Whimory OOB metadata for this family:

```text
a07x_v044C_po60 bank=0 pb=1525 pp=15 t=40 l=000140F8
a07x_v044C_po61 bank=1 pb=1525 pp=15 t=00 l=00000000
a07x_v044C_po62 bank=2 pb=1525 pp=15 t=00 l=00000000
a07x_v044C_po63 bank=3 pb=1525 pp=15 t=00 l=00000000
a07x_v044C_po64 bank=0 pb=1525 pp=16 t=40 l=00014100
```

Replay now has a focused check:

```text
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump-current --current-a07x-bank-test
```

Key result:

```text
WMREPLAY_A07X lba=0000A07E key=000140FC ... naive_po=62 naive_bank=2 naive_t=00 ...
  fixed_po=60 fixed_bank=0 fixed_t=40 fixed_l=000140F8 pb=1525 pp=15 slice=2
```

Firmware fix:

```text
entry->po = (delta >> 1) & ~3u;
```

This selects the containing bank0 page, then the existing OOB-derived logic
computes the 512-byte slice from `lpn - spare.user.lpn`. It should turn the
previous `A07E rc=-6 reason=type/lpn` path into a real read from `pb=1525
pp=15 slice=2`.

Do not treat this alone as storage-complete. The current dump contains two
different MBR-like sectors:

```text
stale map MBR:    page_j982_v056B sig=AA55 p0 type=0C start=0000A07E
restored WinPod:  page_b1524_p0   sig=AA55 p0 type=0B start=0000003F
```

The restored WinPod path has a verified FAT32 boot sector:

```text
page_b1524_p0              l=0001407E sig=AA55 p0 start=0000003F
scan_v044B_po128_l000140FC sig=AA55 bps=4096 spc=1 rs=32 nf=2 FAT32
```

The stale `A07E` path no longer points at a valid BPB after the disk-mode file
copy. Therefore the next storage-completeness gate is MBR selection: WMOUNT
must select or synthesize the current WinPod MBR/start `0000003F`, or perform a
real boot-sector search and expose coherent sectors for FAT/root reads. Do not
push another build only for the bank0/slice fix.

Disk-mode capture provided the important current WinPod ground truth:

```text
MBR p0 type=0B start=0000003F size=000E7F81
partition boot BPB: bps=4096 spc=1 reserved=32 nfats=2 fatsz=927 root_cluster=2
```

Replay validates the current MBR and boot-sector match:

```text
WMREPLAY_CUR_MBR_OK target=page_b1524_p0 v=044B po=0 l=0001407E sig=AA55 p0t=0B p0st=0000003F
WMREPLAY_CUR_BOOT_CAND target=scan_v044B_po128_l000140FC sig=AA55 bps=4096 spc=1 rs=32 nf=2 FAT32
```

This means the old `0xA07E` partition-start assumption is stale. The current
disk is a WinPod/MBR/FAT32-LBA volume whose host partition starts at `0x3F`.
The next unresolved issue is not BPB detection; it is coherent FAT/root sector
translation after the boot sector.

The replay currently proves that simple `raw_l = mbr_l + 2 * lba` only works
for the early MBR/boot/reserved area. Later FAT/root sectors require the real
FTL map/log decode or a disk-mode reference match:

```text
LOG=4096 plan: fat1 lba=0000005F raw=0001413C
LOG=4096 plan: root lba=0000079D raw=00014FB8
LOG=2048 plan: root lba=00000EFB raw=00015E74
LOG=512  plan: root lba=00003B2F raw=0001B6DC
```

Captured pages at `raw=00015000` contain Rockbox language file text, proving
Rockbox files exist on NAND, but that page is not a root directory sector. A
bogus manifest path was found and fixed: `map_b6912_p0` is `type=40` user data,
not a map page, and must not be decoded as `N3GM_ENTRY` map state.

Current offline blocker:

```text
MBR_OK and BOOT_OK pass.
FAT/root directory lookup is not proven.
Need disk-mode reference sectors for FAT/root, then match those bytes to NAND
pages and derive the post-boot FTL mapping.
```

Disk-mode sectors to capture next:

```text
/dev/sdb  LBA0, 512 bytes
/dev/sdb1 sector 0, 4096 bytes  (boot sector)
/dev/sdb1 sector 32, 4096 bytes (first FAT sector)
/dev/sdb1 sector 1886, 4096 bytes (root cluster 2 if BPB is literal)
```

Do not flash until replay can read a current root directory sector and locate
`rockbox.ipod` or `.rockbox` through the same model the firmware will use.

## Nano 3G dualboot / Apple firmware recovery

The online Rockbox `mks5lboot` README marks iPod Nano 3G support as WIP. It
documents the intended dualboot controls:

```text
SELECT+PLAY -> original firmware disk mode
MENU        -> original firmware
Hold locked -> original firmware
```

It also warns that `--single` destroys the original Apple NOR boot image. The
local source explains the current failure mode: `identify_fw()` is stubbed for
`IPOD_NANO3G`, so the stock Nano 3G uninstaller treats every valid IM3 as
"not Rockbox" and exits without uninstalling. The same missing identification
can make repeated normal installs preserve the current Rockbox image as the
adjacent "OF" backup, overwriting the real Apple backup after the first install.

Observed behavior after a rebuilt uninstaller:

```text
fixed uninstaller DFU sent successfully
device still boots straight to Rockbox bootloader
MENU / SELECT+PLAY / Hold paths do not reach Apple firmware
```

Interpretation: the adjacent backup image restored by the fixed uninstaller is
also Rockbox, or the Apple/ONB image is otherwise missing/unusable. Do not keep
retrying the uninstaller as a recovery mechanism. Use iTunes restore if Apple
firmware/disk mode is required, or continue with Rockbox storage bring-up from
DFU. The installer/uninstaller source has been patched locally so Nano 3G
future updates prefer an adjacent valid backup and the uninstaller actually
attempts to restore it, but this cannot recover an Apple image that has already
been overwritten.

Verification:

```text
python3 tools/test_nano3g_wmount_replay.py
python3 -m py_compile tools/nano3g_wmount_replay.py tools/nano3g_wmount_dfu_dump.py tools/nano3g_ftl_replay.py
make -C build-bootloader-ipodnano3g
./utils/mks5lboot/mks5lboot --mkdfu-inst build-bootloader-ipodnano3g/bootloader-ipodnano3g.ipod tmp/nano3g-wmount-bank0-slice.dfu
```

## Current restored WinPod sector-size fix

After restoring again through Apple disk mode, the current device is a clean
WinPod/MBR/FAT32-LBA volume:

```text
N3GDM_MBR sig=AA55 p0t=0B p0st=0000003F p0sz=000E7F81
N3GDM_BPB sig=AA55 bps=4096 spc=1 rs=32 nf=2 fatsz=927 root=2 fsinfo=1 backup=6
```

The mounted FAT volume contains both expected loader paths:

```text
/rockbox.ipod
/.rockbox/rockbox.ipod
```

Replay with the current NAND dump and disk-mode references now validates the
current MBR and FAT boot sector:

```text
WMREPLAY_CUR_MBR_OK target=page_b1524_p0 v=044B po=0 slice=0 l=0001407E sig=AA55 p0t=0B p0st=0000003F p0sz=000E7F81
WMREPLAY_CUR_BOOT_CAND target=scan_v044B_po128_l000140FC v=044B po=128 l=000140FC page_l=000140FC slice=0 sig=AA55 bps=4096 spc=1 rs=32 nf=2 fz=927 ts=950145 fatstr='FAT32   ' r=000
WMREPLAY_CUR_OK base=0001407E scale=2 boot_lba=0000003F boot_l=000140FC
```

The code-level mismatch found at this point was the Nano 3G config advertising
`SECTOR_SIZE 2048` while the WMOUNT/direct FTL reader returns 512-byte slices
and stores each requested sector at `i << 9`. This corrupts the storage contract
above the NAND layer. `firmware/export/config/ipodnano3g.h` now exposes
`SECTOR_SIZE 512` so Rockbox's disk/FAT layer uses the same sector unit as the
MBR and WMOUNT reader. FAT's BPB scaling handles the 4096-byte FAT32 sector size
on top of the 512-byte storage sectors.

Verification after the change:

```text
python3 -m py_compile tools/nano3g_wmount_replay.py tools/nano3g_wmount_dfu_dump.py tools/nano3g_ftl_replay.py tools/nano3g_diskmode_capture.py
python3 tools/test_nano3g_wmount_replay.py
python3 tools/nano3g_wmount_replay.py tmp/n3g-dump-current --current-winpod --lba0-reference tmp/n3g-diskmode-current/disk_lba0_512.bin --boot-reference tmp/n3g-diskmode-current/part_lba0_boot.bin
make -C build-bootloader-ipodnano3g
```

The old current NAND dump still does not prove later FAT/root reads because it
does not contain the backing pages for those LBAs:

```text
WMREPLAY_CUR_MAP fat1 ... reason=nopage
WMREPLAY_CUR_MAP root ... reason=nopage
```

This is dump coverage, not a disk-mode file-placement problem. Disk mode shows
the root directory has `.rockbox` and `rockbox.ipod`.

## Current restored WinPod FSInfo fix

After the `SECTOR_SIZE 512` fix, the next FAT mount blocker is the FAT32
FSInfo sector. The restored WinPod BPB reports `bps=4096`, `fsinfo=1`, so
Rockbox scales the FSInfo location to host LBA `0x47`
(`partition_start 0x3f + fsinfo * 8`). The current raw NAND dump has a stale or
unreadable sector there:

```text
WMREPLAY_CUR_PLAN fsinfo_raw lba=00000047 raw=0001410C ... sig=0000 bps=0 ... r=77F
```

Rockbox `fat_mount()` treats a missing or bad FSInfo signature as fatal before
it reaches FAT/root-directory reads. The Nano 3G WMOUNT path now validates the
real FAT boot sector first, then synthesizes the minimal FSInfo sector expected
by `fat_mount()` for that one LBA:

```text
WMREPLAY_CUR_PLAN fsinfo_synth lba=00000047 raw=0001410C rc=0 sig=41615252 free=00000000 next=00000002 trail=AA550000
```

Firmware side effect:

```text
N3G_WMOUNT_BOOT_OK ... bps=4096 spc=1 rs=32 nf=2 fs=00000047
N3G_WMOUNT_READY ... xmap=1 ... fs=00000047
```

This does not hardcode a boot-sector page. It derives FSInfo from the current
BPB and only enables the synthetic sector after the FAT32 boot sector validates.
The synthetic free-count is finite (`0`) rather than `ffffffff`; Rockbox treats
`ffffffff` as "recalculate free space now", which can force a broad FAT scan in
the bootloader before the mount is usable.
Do not re-enable the broad boot-sector scanners for this restored WinPod path;
the remaining live test is whether FAT/root reads progress far enough to open
`/rockbox.ipod`.

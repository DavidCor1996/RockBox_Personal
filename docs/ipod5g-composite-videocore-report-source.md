# iPod Video 5G composite and VideoCore recovery — report source

Date: 2026-09-04  
Target: iPod Video 5G/5.5G (`ipodvideo`, PP5022 + BCM2722)  
Scope: the glitching composite Rockbox menu and the `Apple video core time out`
that occurs before H.264 playback begins. This report supersedes the loader and
resident-composite conclusions in
`docs/ipod5g-dock-undock-video-report-source.md`; its output-policy and audio
lifecycle findings remain useful.

## Executive result

The two hardware symptoms have separate, source-backed causes.

1. The composite implementation used a 720x480 diagnostic BMP as if it were a
   persistent display surface. On the 5G resident VideoCore firmware, command 3
   only arms NTSC output. The live 320x240 BGR24 surface is at `0xc0000000`.
   Reissuing the mode-set disrupts TV synchronization; failing to refresh that
   live surface leaves a stale boot/menu image. Unpaced host-port writes can
   corrupt the surface as horizontal lines.
2. The H.264 loader wrote all 201,376 bytes of the retail `vmcs.bin` as an
   unpaced CPU halfword stream. Apple aligns the buffer, transfers the largest
   256-byte-aligned span through the PP5022's *second* DMA controller, and uses
   CPU halfwords only for the small tail. The device's last status stopped at
   `phase=probe`, `driver_stage=start`, proving the failure happens inside
   `bcm2722_video_start()` before service discovery or MP4 sample parsing.

The first hardware build using that DMA layout completed every DMA command but
still stopped at `driver_stage=start`; its composite mirror acquired the right
320x240 mode but remained cropped by panel overscan and tore badly during the
moving right-side menu preview. DMA completion therefore was not sufficient
evidence that the copied VideoCore image was intact.

The current qualification build adds RetailOS's own optional word-for-word
upload verification branch before launch, and records each start-mailbox step
separately. It also replaces direct per-four-pixel live writes with Rockbox's
PP5022 assembly streamer, holds the CPU/bus boost only during the copy, and
software-scales the mirror into the same centered 90% NTSC safe area already
qualified on the 6G. The 5G hardware build passes. Simulator playback sustained
30 seconds of the user's H.264/AAC test file without an audio cutoff, but the
simulator cannot execute or validate the physical BCM2722.

## Evidence provenance

### Apple firmware

The device-matched Apple iPod 5G 1.3 updater is available from Apple's update
server:

- [Apple iPod 5G 1.3 IPSW](https://secure-appldnld.apple.com/iPod/SBML/osx/bundles/061-2965.20080313.R45jT/iPod_13.1.3.ipsw)
- IPSW SHA-256:
  `66aad071f960061dcfbdfe69773a698a59b9635c18ba9cb4478f57fd69306cb7`
- extracted OSOS SHA-256:
  `f7b7670bafd1d6ac45effa00b4d314550c030218a953468cfc6057d6d80e6ea7`
- OSOS load base used for the addresses below: `0x10000000`

No Apple binary is added to this repository. The device receives VideoCore
runtime files through `tools/install_ipod5g_videocore.py` from its own Apple
resource image.

### Rockbox and measured prior art

- [Rockbox PP5020 register definitions](https://github.com/Rockbox/rockbox/blob/master/firmware/export/pp5020.h)
  name the primary DMA controller registers and all command/status bits.
- [Rockbox PP502x PCM DMA driver](https://github.com/Rockbox/rockbox/blob/master/firmware/target/arm/pp/pcm-pp.c)
  independently demonstrates `size - 4`, cache commit plus uncached aliasing,
  and read-to-clear DMA completion status.
- [Archived Rockbox FS#9787](https://web.archive.org/web/20240228164605/https://www.rockbox.org/tracker/task/9787)
  and its [original patch attachment](https://web.archive.org/web/20240228164605id_/https://www.rockbox.org/tracker/task/9787?getfile=18536)
  document working 5G composite mirroring on physical hardware.
- The Broadcom [BCM2722 product brief](https://pdf.dzsc.com/88888/2008121101925301.pdf)
  confirms the chip family supplies multimedia decode, scaling, display, and
  TV-output functions; it does not publish this private host protocol.

## Composite path reconstruction

### What Apple diagnostics does

In the device-matched diagnostic image, `0x1000ebc4`:

1. writes the encoded command to internal `0x1f8`;
2. routes commands 2, 3, and 4 to BMP staging address `0xc0200000`;
3. uploads the BMP;
4. writes `0x31` to `BCM_CONTROL`;
5. polls command completion; and
6. reads the signed status at `0x1fc`.

Its BMP uploader at `0x1000e564` polls `BCM_CONTROL & 2` before each burst and
never sends more than 16 bytes without another ready check. That pacing remains
in the one-time command/BMP path. It is not evidence that the continuously
changing live framebuffer should use the same slow per-burst CPU loop: the
physical FS#9787 mirror streams that surface through the 32-bit host port.

### What makes it a live display

FS#9787 supplies the missing runtime observation:

- a 320x240, 24-bit command-3 BMP arms NTSC output;
- the displayed live framebuffer is `0xc0000000`, not the BMP staging address;
- its format is packed BGR24;
- RGB levels are mapped to studio swing 16..235;
- direct writes update the attached television without repeating command 3;
- touching 20 scanlines after the framebuffer flushes the VideoCore-side
  cached window; and
- a 720x480 mode-set selects a different output mode and is not the Rockbox LCD
  mirror surface.

The previous implementation therefore combined two incompatible operations:
it armed a 720x480 diagnostic mode once, then continued to update only the
ordinary LCD buffer. The TV retained the arm-time image at the wrong geometry.
Repeated or unpaced attempts produced the reported signal reacquisition and
horizontal corruption.

### Corrected resident behavior

`firmware/target/arm/ipod/video/lcd-video.c` now:

- arms command 3 once with the exact 320x240 BMP header/image sizing from the
  working FS#9787 implementation;
- writes live Rockbox rectangles into `0xc0000000` in BGR24;
- uses the measured 5-bit/6-bit to 16..235 level tables;
- maps the 320x240 source into a centered 288x216 safe area with 16-pixel side
  and 12-line top/bottom borders, exactly 90% on both axes like the qualified
  6G output;
- precomputes the source maps and uses one 960-byte line workspace (1,840 bytes
  total including maps), with no core/audio-buffer allocation;
- sends completed BGR rows through the existing hand-optimized PP5022 assembly
  writer instead of polling and calling C code for every four pixels;
- keeps partial rectangles partial after scaling and holds a normal CPU boost
  reference only while the synchronous mirror transfer runs;
- preserves the iPodJS overlay composition path;
- retains Apple-style at-most-16-byte pacing for the command/BMP upload;
- touches 20 post-frame scanlines to expose updates; and
- does not resend command 3 or command 14 during normal LCD updates.

Command 14 is not a per-frame prerequisite. Apple diagnostics issues it in a
separate standard-change sequence after a display power transition; using it
as an update operation would needlessly disturb the output.

## Retail VideoCore/H.264 loader reconstruction

### Exact Apple call path

RetailOS function `0x10287698` is the flat-image uploader used for the
201,376-byte resource `vmcs.bin`. The MPlayer caller passes its final flag as
zero. Therefore the optional eight-byte mutation at VideoCore offset `0x200`
belongs to another loader mode and must not be applied to this image.

The warm player path is:

1. establish the VideoCore host bootstrap/alternate-port condition;
2. upload the flat image at VideoCore internal address zero;
3. clear command `0x1f8`;
4. write `0xc0000000` to `0x10000c00` and wait for bit 0;
5. clear that register;
6. write `0xa5a50002` to `0x10000400`;
7. wait for command `0x1f8` to become nonzero; and
8. read the runtime header/service directory rooted at `0x1f0`.

The earlier report's cold-power-cycle conclusion is retracted. The actual
MPlayer call selects the warm flat-image path; adding an invented reset can
destroy the precondition Apple relies on.

### Exact Apple transfer method

The image writer at `0x10287be8`:

1. emits one CPU halfword only if needed to make the source word-aligned;
2. takes `length & ~0xff` as the DMA bulk span;
3. splits the bulk into at most 64 KiB chunks; and
4. emits the remaining tail as CPU halfwords.

For the 201,376-byte `vmcs.bin`, this is 201,216 DMA bytes followed by a
160-byte CPU tail: three 64 KiB chunks plus `0x1200`, then `0xa0`.

RetailOS programs an undocumented second PP502x DMA instance:

| Field | Apple value |
|---|---:|
| master control | `0x60008000`, bit 31 enabled |
| channel 0 command | `0x60009000` |
| channel 0 status | `0x60009004`, read-to-clear |
| RAM address/config | `0x60009010` / `0x60009014` |
| peripheral address/config | `0x60009018` / `0x6000901c` |
| RAM config | `0x22000000` |
| peripheral config | `0x26000000` |
| peripheral port | fixed `0x30000000` (`BCM_DATA32`) |
| completion interrupt | PP5022 line 27 |

For a 64 KiB chunk, the measured command is `0xcc00fffc`:
`START | INTR | RAM_TO_PER | SINGLE | (0x10000 - 4)`. This is the same
register format as Rockbox's documented controller at `0x6000a000`/
`0x6000b000`; Apple instantiates the driver for both controllers.

Rockbox has no IRQ-27 handler for the second instance. The loader
temporarily masks line 27, keeps Apple's exact `INTR` command bit, polls the
`START` bit to completion, consumes the read-to-clear status, and restores the
prior interrupt mask. It also commits the cache and supplies the DMA engine an
uncached source alias, matching Rockbox's established PP502x DMA rules.

The first physical run proved only that those DMA commands completed; launch
still timed out. The current qualification build therefore enables Apple's
optional verification loop at `0x102877cc..0x10287838`, comparing all 50,344
little-endian words before launch. A mismatch is reported by 64 KiB chunk. If
verification passes, the two formerly combined waits are reported distinctly
as `Apple start mailbox arm timed out` (bit 0 at `0x10000c00`) or
`Apple VideoCore ready timed out` (non-zero command at `0x1f8`). Their bound is
10 seconds; Apple itself waits without a timeout.

## Why the competing explanations do not fit

| Explanation | Evidence against it | Result |
|---|---|---|
| Bad cable/accessory authentication | The TV now acquires a signal and shows recognizable Rockbox pixels. | Not the cause of the corrupt geometry or loader timeout. |
| PAL/NTSC preference only | A standard mismatch can disturb sync, but cannot explain a frozen arm-time menu plus a loader stopping at `driver_stage=start`. | Secondary setting, not either root cause. |
| Bad H.264 profile/container | The recorded failure occurs before VideoCore services are available and before a sample is submitted. | Cannot cause this start timeout. |
| Missing eight-byte patch at `0x200` | MPlayer passes flag zero to the flat-image uploader. | Correctly omitted. |
| Retail image missing/corrupt on disk | The image size/readback had already been verified, while the old upload method diverged from Apple's DMA path. | File presence alone was insufficient. |
| Cold-reset needed before upload | The decompiled MPlayer caller selects the warm path. | Prior conclusion retracted. |
| One-shot 720x480 BMP is the screen | FS#9787 identifies `0xc0000000` as the live 320x240 framebuffer. | Direct cause of stale/wrong-resolution output. |

## Verification performed

- `rockpod/tests/test_ipod5g_composite_source.py`: 8 tests passed.
- `build-hw-ipodvideo`: incremental 5G hardware build passed.
- `build-hw-ipodvideo/rockbox.zip`: packaging passed.
- Linked ARM disassembly contains `0x22000000`, `0x26000000`, fixed
  `0x30000000`, `0xcc000000` command bits, cache commit, uncached aliasing, and
  polling of DMA `START`.
- The user's Downloads H.264/AAC MP4 passed a 30,016 ms sustained simulator
  playback gate with 6,758,400 PCM bytes produced.
- The general 5G simulator-first script is blocked by an unrelated pre-existing
  missing generated dependency:
  `build-sim-video-5g/snes/snes_regs.h`. This does not affect the successful
  5G hardware link or the independent H.264/AAC gate.
- Current qualification `rockbox.ipod` SHA-256:
  `752b2297affef3a659ff66e5143a84c9d9d4f221eb97bcf9c306548f9f4ecf84`.
  That exact image was installed and read back successfully from both required
  firmware paths on the serial-verified 5G
  (`500000000000A270015897630`, `/dev/sda2`). The separately attached 6G was
  not a deployment target.

The simulator result covers MP4 demux, AAC output, PCM continuity, and plugin
lifecycle. It does **not** claim that the simulated host executes BCM2722 code.

## Evidence gaps and hardware decision gate

| Claim to validate | Current confidence | Required physical observation |
|---|---:|---|
| Rockbox menu is stable and fully inside the display | Medium until the new safe-area/assembly path is observed | Enable Composite On and leave menu active for at least 30 seconds. |
| Moving right-side preview and overlays mirror without lines | Medium; the old per-pixel path failed physically and has now been replaced | Navigate the main menu and watch the moving right pane for at least 30 seconds. |
| Retail `vmcs.bin` is intact in VideoCore SRAM | Unknown until the new Apple verification branch runs | Launch the test M4V and inspect `driver_stage=verify` or the later exact start sub-error. |
| Retail `vmcs.bin` reaches service discovery | Medium-low after the previous `start` timeout | Confirm status progresses to `service-discovery`. |
| H.264 playback works on BCM2722 | Medium until device run | Play at least 60 seconds, both LCD and composite as applicable. |
| Audio remains continuous and synchronized | Simulator-supported, hardware unverified | Listen for >60 seconds and test pause/resume/exit. |
| Software MPEG YUV blits mirror perfectly | Medium; normal backing-buffer mirror path is retained | Play an MPEG counterpart and compare framing/overlays. |

No commit should be made until the first three physical checks pass. A hardware
failure must be classified by the recorded `driver_stage`; repeating blind
power-cycle or mount-repair experiments would discard the diagnostic value of
this pass.

## Claim-source ledger

| Claim | Primary evidence | Corroboration |
|---|---|---|
| Composite command wrapper and <=16-byte FIFO pacing | Apple diagnostic functions `0x1000ebc4`, `0x1000e564` | Current source-level pacing assertions |
| 320x240 arm, live FB `0xc0000000`, BGR24, 16..235, 20-line flush | Archived FS#9787 and attachment | Implemented conversion tables match attachment byte-for-byte |
| Retail loader sequence and flag-zero caller | Apple RetailOS `0x10287698` and MPlayer call site | Runtime header/service reads at `0x1f0` |
| DMA bulk/tail split | Apple RetailOS writer `0x10287be8` | Exact 201,216 + 160 byte calculation |
| Second DMA register map and command | Apple RetailOS DMA driver object and measured stores | Upstream `pp5020.h` field definitions |
| Cache/uncached DMA source requirement | Apple/Rockbox PP502x implementation behavior | Upstream `pcm-pp.c` |
| Failure precedes MP4 parsing | Device `video5-last-status.txt`: `phase=failed`, `driver_stage=start` | Driver control flow |
| Sustained host audio path | `tools/h264_audio_sim_gate.py` result | 30,016 ms / 6,758,400 PCM bytes |

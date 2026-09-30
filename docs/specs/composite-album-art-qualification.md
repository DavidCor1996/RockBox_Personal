# 640x480 composite artwork qualification candidate

2026-09-12. Status: builds and host checks passed; full-height static chart observed,
fine-line shimmer reported; remaining physical gates pending.
This is the first hardware gate for `composite-album-art-quality.md`, not an
album-art renderer or a completed artwork sync.

## New reference evidence

Recovered the reference from the public mirror already cited in the local
hibernate audit: https://github.com/iEMU-Android/iPod_35.2.0.4 . The decrypted
wrapped image SHA-256 is
`f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4`.
The existing full hibernate audit passes against it. The image is held in the
local analysis directory, not redistributed with this change.

The old VP probe mapped the entire body at 0x08000800. The later hibernate and
ARM xref audits establish the actual startup relocation: body 0..0xaed8 to
IRAM, body 0xaed8..0xa1ab60 to DRAM starting 0x08000000, and final initialized
data to 0x08a0fc88. Updated the probe to use those segments, accept only the
recorded wrapped/body hashes, and reject an instruction-budget exhaustion.

Verified runtime routines in that exact image:

- 0x0815e19c: auto-geometry (body offset 0x169074).
- 0x0815ec2c: writes caller image width/height to VP+0x3c/+0x40 and stores
  dimensions and format in the layer object.
- 0x0815e86c: for layer 5, format 8, writes descriptor slots 0/2/1 to
  VP+0x28/+0x2c/+0x30, clears +0x34, sets spans to image width/half-width,
  and plane mode to 1.
- 0x0815ddc4..0x0815ddf0: explicit-destination ratio formulas are
  `((source_width << 12) / destination_width) >> 3` and the corresponding
  height formula shifted by 4. This supplies the existing overscan viewport.

The new `tools/ipod6g_stock_vp_gate.py` executes the first three routines using
Unicorn and checks actual register writes for native and larger input. It also
checks rejection of a corrupted image. Results are in
[stock-gate.json](composite-art-qualification/stock-gate.json).

| Source | Auto destination | Auto H/V | Image dimensions | Y/C stride |
| --- | --- | --- | --- | --- |
| 320x240 | 720x480 | 227/128 | 320x240 | 320/160 |
| 640x480 | 720x480 | 455/256 | 640x480 | 640/320 |

These are executed software contracts with injected frame dimensions and
initialized state, not a captured RetailOS 640x480 playback session or proof
of analog hardware behavior. Together with the existing physical scan-state
correction, they support one isolated 640x480 test. The newer correct map
reproduces the old native ratio result; it does not retroactively validate all
historical flat-map analyses.

## Candidate behavior

`IPOD6G_VIDEOOUT_HIRES_TEST` adds Stage 6 to the existing core debug diagnostic.
It is absent from normal builds. No normal mirror geometry changes.

Stage 6 disables the prior layer before writing a single 460,800-byte static
YUV420 frame into the existing 1,228,800-byte target-owned allocation. It yields
between strips, cache-cleans the full frame, then publishes private format-8
pointers and geometry with the minimal register section masked. It preserves
existing reset/clock handling and mixer SD scan selection. No live frame is
modified while it is scanned. No playback buffer or allocator is used.

Source/image: 640x480; Y/C strides: 640/320. Destination: `(36,24) 648x432`.
H/V: 505/284, calculated using the verified explicit-destination formulas.
Plane mode: 1, fourth descriptor zero, qualified Y/Cb/Cr ordering retained.
Mirroring stays disabled for the static chart. Stage 6 times out after 60 seconds
back to the native Stage 4 grid; SELECT also returns to Stage 4. MENU exits the
diagnostic and restores the previously armed output mode. The test build also
cleans up before handing a USB connection event to the normal handler.

The chart is generated directly at 640x480, not enlarged from an LCD image.
Rows 00–15 identify the complete vertical extent. Color blocks establish color
order; the green circle tests aspect. Right-hand panels compare one- and
two-pixel vertical and horizontal stripes. Composite filtering may blur the
one-pixel stripes; their mere visibility is not a mandatory quality claim.

[Reference chart](composite-art-qualification/chart.png)

## Validation completed

- Native 6G test build with isolated `/.rbtv` runtime, including matching codecs.
- Native build without the test flag; high-resolution symbol absent.
- Simulator core build succeeds without the target-only diagnostic.
- Existing exhaustive RGB565 conversion gate passes all 65,536 colors.
- Host chart generator runs with AddressSanitizer and UndefinedBehaviorSanitizer;
  full chart visually inspected.
- Stock emulator tests pass at both resolutions; modified input rejected.
- ARM diagnostic function has 56 bytes of its own stack frame; chart helper
  saves 24 bytes. This is not a total stack bound across existing callees.
- No allocation, PCM, playlist, audio-stop or artwork-slot-update call in the
  new compiled diagnostic path.
- Native source and touched-file whitespace checks pass. Existing unrelated
  compiler warnings remain in this working tree; none originates in new code.

| ELF measurement | Flag off | Test flag on | Change |
| --- | ---: | ---: | ---: |
| text | 2,944,692 | 2,946,012 | +1,320 |
| data | 11,032 | 11,032 | 0 |
| BSS | 9,026,564 | 9,026,564 | 0 |
| target framebuffer | 1,228,800 | 1,228,800 | 0 |

A first baseline language generation produced a header-only Bulgarian file;
regenerating that build artifact serially restored the same language-buffer
size and made the BSS comparison meaningful. No language source was changed.
An unrelated iAP thread-stack change arrived in the shared tree during the
work; both native builds were refreshed to include it before the final memory
comparison. It is not part of this diagnostic change.

Candidate firmware SHA-256:
`d79722bb15cccbd9d1eacaa2b6957ed505e671d7d2d3214ee69ae9ae3656f7d1`.

Build directory: `build-composite-art-qualification`.
Reproduction:

```sh
mkdir -p build-composite-art-qualification
cd build-composite-art-qualification
../tools/configure --target=ipod6g --type=n --rbdir=/.rbtv
make -j8 EXTRA_DEFINES=-DIPOD6G_VIDEOOUT_HIRES_TEST bin codecs
```

Reference check (requires Unicorn in the selected Python environment):

```sh
python tools/ipod6g_stock_vp_gate.py /path/to/osos.fw.decrypted
```

## Physical test and next gate

Use the isolated `/rockbox-tvout-test.ipod` Rolo image and `/.rbtv` runtime.
Never install this diagnostic as either personal firmware copy. Its runtime
plugins may belong to an older test build; this qualification uses the core
Debug screen and freshly matched audio codecs only.

1. Boot the normal firmware and select `/rockbox-tvout-test.ipod` in Files.
2. Dock in the DCP750. Open System → Debug (Keep Out!) → Test composite video.
3. SELECT progresses through sync, white, native bars, native grid and native
   live UI. Confirm Stage 4 still shows its complete grid and correct colors.
4. SELECT once more for Stage 6. Confirm its LCD diagnostics show image and
   source 640x480, H/V 505/284 and mixer config 0x12.
5. Report whether rows 00 through 15 and the bottom border all appear, whether
   the circle is round, whether colors match, and whether either half repeats,
   clips or flickers. A photo of the entire TV and iPod diagnostics is useful.
6. SELECT returns to the native grid; MENU exits. Verify ordinary UI mirroring
   resumes. Reboot normally to return to the personal installation.
7. If the static pattern passes, repeat with music playing through the core
   Files browser, checking audio during entry/exit. Do not launch older test
   runtime plugins as part of this stage.

A passing image only opens the next gate: a bounded TV artwork compositor,
field-safe updates and an A/B quality comparison. It does not yet justify bulk
artwork sync or a production geometry switch. If it fails, retain the native
fallback and record the exact pattern/readbacks before further experiments.

## Installation verification

The final candidate was copied to `/rockbox-tvout-test.ipod` and
`/.rbtv/rockbox.ipod`, with 43 matching codecs in `/.rbtv/codecs`.
Both firmware copies match the candidate hash above. All 71 protected personal
firmware, configuration, codec and personal/test database files retained their
checksums. Existing empty scanner scratch files were preserved too.
The device was flushed and left mounted for the user to safely eject.

The original isolated test files are backed up at
`/tmp/composite-art-qualification/device-backup-1789257585140592054`.
The final write/preservation manifest is
[deployment.json](composite-art-qualification/deployment.json).
A physical photo now shows the complete numbered chart. The user reports
apparent motion in the fine horizontal-line panel below the circle. See the
physical observation in `../ipod6g-dcp750-videoout-results.md`; temporal
stability, aspect and active-playback checks remain open.


The user subsequently confirmed that motion is confined to the fine stripe
region; numbers, colors and the circle remain stable. The next isolated
artwork/video candidate is documented in [composite-art-enhanced-test.md](composite-art-enhanced-test.md).

# Composite artwork and video enlargement candidate

The user confirmed the 640×480 chart is stable in its large features; motion is
confined to the fine horizontal stripe stress pattern. This qualifies proceeding
with a separate experimental build, not a claim that moving video has passed.

## Implemented path

`VIDEOOUT_ENHANCED_TEST` selects a 640×480 YUV420 surface using the stock-probed
private format-8 descriptor layout and the already tested 648×432 TV viewport.
Normal builds retain the existing 320×240 path. RGB565 LCD updates duplicate each
pixel twice in each direction, preserving UI edges. Decoded YUV420 updates use
bounded 2× bilinear enlargement of each plane. Source rectangle edges are
replicated; no source padding is assumed. Band boundaries can therefore have a
small interpolation discontinuity and need checking with real video overlays.
This enlarges frames already decoded for the LCD; it does not recover detail
lost during earlier video encoding/downsampling or add a 640×480 decoder.

The current native iPodJS WPS projects its playback-owned 128px cover into a
136×136 slanted image plus a 50px reflection. A host-prepared `cover.tvart` beside
each album's music supplies that pane at 272×372. Source covers remain unchanged.
The source library has 314 audio-containing directories, all with covers, plus
six cover-only directories. The prepared 320 sidecars occupy 48,573,440 bytes.
Most source covers are 320×320; this path preserves their existing detail instead
of enlarging the 128px playback thumbnail. No online replacement or AI-generated
art is used.

Version 1 is `TVART001`, little-endian uint32 payload length and FNV-1a checksum,
then 101184 Y, 25296 Cb and 25296 Cr bytes, limited-range BT.601. A vertical
(1,2,1)/4 filter is applied before chroma subsampling to reduce interlace twitter.
The host tool saves filtered/unfiltered PNG previews for comparison. Physical
comparison and subjective sharpness remain pending.

## Ownership and failure behavior

Two scanout frames consume 921600 bytes of the existing 1228800-byte target
allocation. The 151776-byte artwork and 50592-byte native comparison pane occupy
the unused tail; their combined end is 1123968. No core allocation, playback
shrink callback, extra album-art claim, mixer change or playlist change occurs.

The WPS draw hook only publishes a track path and binds cached native pixels.
The idle service waits one second, requires an empty input queue, no Hold,
usable/non-committing tagcache and an active output. Each service reads at most
four 4096-byte chunks, checking input and yielding between reads. It never scans
directories or decodes a bitmap. Track changes invalidate the cached pane;
leaving WPS closes the descriptor. Missing, truncated, corrupt or interrupted
files fall back to the ordinary LCD image. Failed files are not repeatedly
opened during the same request.

All cache writes/publication and LCD composition use the LCD mutex. On each
RGB update, the compositor first checks every native pixel in the overlap
against the bound pane. A modal, Hold screen, text page or other changed pixels
wins over the optional artwork. Only a matching intersection is replaced with
high-resolution pixels, including odd dirty rectangles. Mode transitions that
reuse the scanout allocation invalidate the artwork. This first candidate hooks
the native iPodJS WPS only, not arbitrary user skins or browser thumbnails.

## Verification

- Clean iPod 6G build, codecs and matching MPEG player succeeded.
- Normal simulator build and focused WPS regression passed: page cycling,
  pause/seek/resume, playlist identity, source-list return and exact native note
  pixels.
- `tools/tests/composite_enhanced_gate.py` compiles actual target functions under
  ASan/UBSan: interpolation reference comparisons, plane/edge boundaries, dirty
  intersections, cache rejection and invalidation.
- `tools/tests/composite_art_service_gate.py` compiles the production idle
  service with host shims: settle/input/Hold/commit gating, checksum failures,
  I/O errors, track changes and descriptor cleanup. In this execution sandbox,
  LeakSanitizer's ptrace restriction required `ASAN_OPTIONS=detect_leaks=0` for
  that run; AddressSanitizer and UBSan remained enabled, and descriptor counts
  are asserted explicitly. The tested code performs no heap allocation.
- Clean baseline ELF: text 2944788, data 11032, BSS 9026564 bytes. Candidate:
  text 2946172, data 11036, BSS 9031236. Delta: +1384/+4/+4672. Reserved scanout
  stays 1228800 bytes. ARM own stack frames: draw 24, idle service 32,
  RGB expansion with inlined compositor 64, byte scaler 48, pane binding 40
  bytes, excluding callees.
- Matching MPEG player: target 71, API 288, load 0x0bc4c000,
  end 0x0be5d0e0; fits the 0x2f0000-byte plugin reservation.

The first navigation harness attempt could not copy a pre-existing recursive
simdisk asset directory. The isolated `/tmp/qs-fixture` source avoided that
unrelated fixture problem. Its old WPS/backlight configuration then produced a
blank WPS; the rerun uses the current WPS and an always-on test backlight, as the
focused WPS gate does. Final navigation/deployment results are appended below.

## Physical test

Use the isolated `/rockbox-tvout-test.ipod` Rolo image and `/.rbtv` runtime.
The normal firmware and its runtime/database are preserved. The test runtime
includes current Apple UI assets and WPS, plus its existing configuration with
explicit iPodJS and always-on backlight settings for this test. Database autoupdate
remains enabled; the deploy verifies both normal and test database checksums.

1. Start a database album while docked. Leave WPS idle about three seconds;
   the TV cover should become sharper while the iPod LCD stays the same.
2. Change albums, cycle WPS pages, use Hold, then return through Albums/Artists
   and rapidly press Menu to Home. Music and the database must remain usable.
3. Open a known MPEG video with the matching test MPEG player. Check smooth
   motion/audio sync, pause/resume, volume overlays and seeking. Exit and start
   music from both Database and Files. Repeat the music/video switch.
4. Report any flicker beyond very fine detail, frame drops, stale art or audio
   interruption. A successful static chart alone cannot settle these checks.

Video quality and CPU headroom remain hardware checks; no frame-rate
improvement is claimed. The user subsequently requested installation into the
normal build; see the normal-runtime deployment below.

## Final navigation result

The corrected fixture passed the full navigation trace gate: 2422 records,
continuous `/Music/test.mp3` playback and playlist identity `(1,1)`, ten complete
hierarchy cycles and twenty rapid Albums/Artists switches. Process FDs were 17
at baseline and after each of the ten cycles. Thirteen playing Home samples
reported 12 Rockbox open files; core available/allocatable remained zero (the
playback arena owns the remaining simulator core memory), without a downward
trend or a recovery screen. The static target memory audit and host loader gate
provide the separate evidence for the new hardware-only cache path.

Candidate firmware SHA256:
`d944eccf92d5356de081ff3681d726e84261197edfee5443e3c8f39275e63c5f`.
Physical moving-video/audio and artwork acceptance remain pending.

The matching OpenH264 player wrapper was also rebuilt for this core (target 71,
API 288, load 0x0bc4c000, end 0x0bcb3eb4) and passed the plugin address/buffer
check. Its decoder continues to use the existing core video path; no decoding
resolution or audio lifecycle was changed. The test package includes the
14-Adobe-Helvetica-Bold font referenced by the WPS declaration.

Reproduction:

```sh
mkdir -p build-composite-enhanced-test
cd build-composite-enhanced-test
../tools/configure --target=ipod6g --type=n --rbdir=/.rbtv
make -j8 EXTRA_DEFINES=-DVIDEOOUT_ENHANCED_TEST bin codecs
```

The mounted writes are recorded separately in
`/tmp/composite-art-qualification/enhanced-deployment.json`; replacement backups
are under the directory recorded there. That initial isolated deployment left the personal firmware unchanged. The
subsequent user-requested normal-runtime deployment below supersedes the
original normal-boot rollback route; the previous firmware is now in its
explicit device backup.

Final deployment verified 1824 files, including 320 sidecars, 43 codecs,
two matching video players and both isolated firmware copies. All 390
preserved-file checksums matched. The device was synced and remains mounted.
See [deployment manifest](composite-art-qualification/enhanced-deployment.json).


## User-requested normal-runtime deployment

The user explicitly requested the change in their real build. A clean normal
`/.rockbox` build was produced in `build-composite-personal` with
`EXTRA_DEFINES=-DVIDEOOUT_ENHANCED_TEST` (also retained in that build's Makefile).
The complete package contains 222 plugins, all verified as target 71, API 288,
with the expected load address and bounds. Archive CRC and RetailOS asset/font
verification passed. The normal ELF contains the artwork service and 2× scaler.

Normal firmware SHA256:
`beb8129bb9aec3c1fc8bf379106a417fa52b1c1c4003b3280130b0c22bce5830`.

Installation used `tools/deploy_ipod6g_preserve_database.sh`. Both normal
firmware locations match the build. All 9590 package-file hashes match;
331 preserved database/artwork files match, and personal settings retain their
values with database autoupdate enabled. The script validated 2929 music paths
and preserved all 11 live tagcache files byte-for-byte. It quarantined transient
transaction files and installed its verified database recovery snapshot.

The prior 9592 replaceable runtime files are backed up at
`build-composite-personal/device-backup-1789260538383279854`.
The full checksum record is `build-composite-personal/deployment.json`.
Normal reboot now selects the enhanced firmware. Moving-video quality, audio
sync, dock artifact behavior and native navigation acceptance still need the
user's physical check; these are not claimed as passed by a simulator build.

The guarded normal deployment completed successfully, including app inventory
and final sync. The iPod remains mounted for safe ejection by the user.

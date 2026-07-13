# SNES Lite milestone test report

Date: 2026-07-12 (America/Halifax)

Status: simulator/build/RockPod gates passed; physical iPod gameplay validation
is still required. SNES Lite remains experimental.

## Build and package

- iPod Classic 6G hardware `make rocks`: passed, including the existing plugin
  set.
- iPod Classic 6G `make zip`: passed.
- Simulator `make rocks`: passed.
- Hardware artifact:
  `build-hw-ipod6g/apps/plugins/snes_lite/snes_lite.rock`.
- `rockbox.zip` contains both `snes_lite.rock` and the updated
  `rockboy_launcher.rock`.

The imported legacy C core emits upstream warnings, including old aliasing and
high-resolution tile assumptions. They are documented technical debt and one
reason this build must remain experimental. The milestone does not promise
hires-game support.

## Killer Instinct simulator and optimization tests

Test ROM: user-provided `Killer Instinct (USA) (Rev 1).sfc` from Downloads.

- Size: 4,194,304 bytes
- SHA-256: `7a5261f1a5e84b67483c79fb002ce1539f2360f88333bda60f12e617d86e0def`
- Header: `KILLER INSTINCT`, HiROM, map `0x31`, cartridge type `0x00`
- Special chip preflight: none detected
- Balanced/fullscreen/audio-off gate: 300 loops in 501 ticks, 60 emulation FPS,
  210 rendered frames, 90 intentionally skipped, `status=0`.
- Fast/fullscreen/audio-on gate: 180 loops in 302 ticks, 60 emulation FPS,
  90 rendered frames, 90 intentionally skipped, `status=0`.
- Plugin arena: 63,427,376 bytes available; 12,952,112 bytes used after core
  start in the balanced test. This is 7,520,256 bytes less than the first-pass
  core allocation.
- The audio-on simulator run completed without a lifecycle crash. SDL dummy
  audio reported 11 dropped blocks and 6 underruns, so it validates safe mixer
  integration but not audible hardware quality.

This establishes load/init/frame-loop/clean-exit behavior. It does not establish
real-time iPod performance, physical controls, audible output, or complete game
playability.

The optimized frontend now uses hardware CPU boost only while gameplay is
running, releases it in the menu and on cleanup, paces the emulation loop to 60
Hz, uses adaptive frameskip, renders directly to the framebuffer, and defaults
to the low-cost 320x240 fullscreen scaler. Audio remains off by default and is
optional through the Rockbox primary PCM mixer.

## RockPod

- Focused suite: 66 passed.
- Mock-device sync: passed; second diff showed all ten scoped items unchanged.
- Selected-ROM plan included the emulator, Games launcher, config, console
  index, ROM, fallback cover, global game index, and SNES manifest.
- Plan contained no Music or Videos paths and no ROM removals.

## Physical deployment

Mounted target: `/run/media/david/DAVID_S IPO` (`/dev/sda1`, VFAT). It was
writable and had approximately 301 GiB free. RockPod applied the reviewed plan
with no copy failures and retained rollback manifests under
`rockpod/.backups/games/snes-lite-gate/device/`.

The optimization deployment updated only `snes_lite.rock`,
`rockboy_launcher.rock`, and `snes_lite.cfg`; the subsequent RockPod inventory
diff classified all ten scoped SNES items as unchanged. Deployed checksums were
re-read from the device and matched the local plugin, launcher, and ROM. The
existing console and game indexes were merged, not
replaced wholesale; an existing Secret of Mana SNES entry remained present.
No `rockbox.ipod` firmware image was deployed, so the dual-firmware-copy rule
was not invoked.

## Required hands-on follow-up

After safe eject and reboot, open Games -> Super Nintendo and test Killer
Instinct fullscreen rendering, click-wheel mapping, CPU-boosted performance,
menu/quit, and return to Games. In the emulator menu, Menu/Play and wheel
back/forward move selection and Select activates the highlighted command.
Changing Audio takes effect on the next launch. SRAM
must be validated with a title that actually writes battery-backed SRAM; Killer
Instinct alone is not an adequate SRAM persistence test. Test a known
SuperFX/SA-1 cartridge separately and confirm the named warning. Do not mark
SNES Lite stable or broadly compatible until these hardware checks pass.

## Audio, controls, and menu pass

The follow-up build replaced the original 32,040 Hz/fixed-block path with a
16-block low-jitter queue clocked from the mixer's actual hardware frequency.
Core frame pacing now uses the reported NTSC/PAL refresh rate. Short underruns
fade the last sample to silence, automatic frameskip receives live queue
occupancy, and Auto/Best sound enables interpolation plus the core's low-pass
filter. Economy requests 22.05 kHz and disables those two quality costs.

Killer Instinct simulator gates passed with `status=0` in both modes:

- Best: 600 loops in 1,002 ticks, actual mixer/core rate 32,000 Hz.
- Economy: 300 loops in 501 ticks, actual mixer/core rate 22,050 Hz, zero
  dropped blocks in the SDL dummy backend.

SDL dummy audio still reported synthetic underruns, so these gates validate
clocking, buffering, lifecycle, and clean exit—not audible quality. The prior
device log had sound disabled and therefore supplied no useful audible-path
baseline. Physical transition-matrix testing is still required.

The emulator menu now uses an iPodJS-style light header, white list, blue glass
selection, right-aligned values, and footer hints. Six mappings now alter the
actual SNES buttons rather than only changing a stored label. RockPod title and
genre rules generated these profiles for every installed ROM:

- DKC3: Platformer (0)
- Final Fantasy III: RPG (1)
- Killer Instinct: Fighting (3)
- NHL '94: Sports (5)
- Secret of Mana: Action (2)
- Super Mario Kart: Racing (4)

The focused RockPod/plugin suite passed 67 tests. The full RockPod suite passed
820 tests and reported 26 failures in unrelated album-list, wallpaper,
inventory, theme, and general music/video sync tests; none touched files in
this scoped change. Hardware plugin build and `make zip` passed.

RockPod deployed eight changed files with no failures: the plugin, global
defaults, and six per-game configs. A final one-file plugin refresh corrected
the next-launch Sound setting lifecycle. The post-deploy diff classified all
eleven scoped runtime items unchanged. Local and device plugin SHA-256 both
equal
`23b16d15e0affcb8a011e9f264812d789f425f3960fe43b5d93e9b1f8ee6aacd`.
The reviewed plan contained no Music or Videos paths and copied no ROMs.

## Upstream optimization research pass

The imported commit remains the current `libretro/snes9x2002` head as of this
test. Upstream documentation reports 59.9227434043 Hz NTSC and 50.3197396536 Hz
PAL timing, matching the frontend's measured 59,923 milli-Hz result. The 2019
input-latency fixes are already present: input is polled before emulation and a
frame runs VBlank-to-VBlank.

The useful missing upstream configuration was the ARM926-class path used by
the Miyoo target: ARM graphics assembly with the C CPU/SPC cores and
`FAST_ALIGNED_LSB_WORD_ACCESS`. Native iPod 6G builds now use that combination;
the simulator stays on the portable renderer. The linked hardware map confirms
`ppu.o`, `rops.o`, `gfx16.o`, and `tile16.o`, with no portable `ppu_.o`, `gfx.o`,
or `tile.o` in the resulting plugin. The main CPU and SPC700 assembly cores
remain deliberately disabled.

The frontend fullscreen scaler now performs 160 aligned 32-bit stores instead
of 320 halfword stores per output line and reuses the previous output line for
the 16 vertical duplicates. Hardware plugin size decreased from approximately
638 KiB to 622 KiB. Physical FPS/audio measurement is required before claiming
a real-device speedup.

RockPod deployed this as a one-file plugin overwrite with no media paths and a
rollback manifest under `rockpod/.backups/games/snes-lite-arm-opt/device/`.
The final local/device SHA-256 is
`d4829502b22a21d5696d3306b24655847dacb85c76e0fbac0081cb30cb744a85`;
the post-deploy diff classified all eleven scoped items unchanged.

### Hardware regression and rollback

Real iPod testing rejected this optimization: games displayed black bars and
became barely playable. The ARM renderer, aligned-fetch flags, packed scaler,
and repeated-line shortcut were therefore removed from the source tree. The
known-good device plugin is preserved in the rollback manifest with SHA-256
`23b16d15e0affcb8a011e9f264812d789f425f3960fe43b5d93e9b1f8ee6aacd`.
It must be restored when the iPod is next connected; the device was not present
when the regression was reported.

## Color-correct portable optimization pass

The failed ARM renderer remains disabled. The safe follow-up keeps the
hardware-tested portable core and corrects its display conversion instead:
PocketSNES carries the SNES five-bit green channel in RGB565 bits 6..10, with
bit 5 reserved for color math. The frontend now replicates green's high bit
into bit 5 during blit, producing the full standard RGB565 range expected by
the iPod 6G framebuffer. A compiled startup self-test checks every pixel in a
256-to-320 reference line before loading the ROM.

Fullscreen rendering also copies only the 16 output lines that the exact
224-to-240 vertical mapping duplicates. Automatic frameskip now derives its
slow/fast thresholds from the core's reported NTSC or PAL refresh rate rather
than hard-coding 60 Hz assumptions. Balanced mode starts at frameskip zero and
adapts upward only if measured emulation speed requires it.

The identical 600-loop Killer Instinct simulator gate changed from 511 drawn /
90 skipped in 1,001 ticks to 601 drawn / 0 skipped in 1,002 ticks. Fast, Max,
and Quality-with-audio gates also completed with `exit status=0`. SDL dummy
audio reported artificial queue pressure, so audible quality and ARM runtime
performance still require real-device testing.

### Hardware rejection and corrected frameskip policy

Physical testing rejected the green-bit expansion and revealed a separate,
severe frameskip bug. A Secret of Mana run maintained 60 emulation fps for 643
loops, but audio-occupancy frameskip sent only 20 frames to the LCD and skipped
623, yielding roughly 2 drawn fps. Audio recorded no dropped blocks and one
underrun, confirming that the sustained video loss was policy rather than CPU
load.

The frontend now preserves the core's declared RGB565 pixels byte-for-byte and
never uses audio occupancy as a reason to suppress video. Auto frameskip uses
measured NTSC/PAL emulation speed and selects disabled or a bounded fixed
interval. The failed `rgb565_green6` build remains available only as rollback
evidence and must not be described as color-correct.

Killer Instinct also had a stale on-device `performance_preset=2`, which
started at fixed frameskip 2 and disabled the core's transparency path. The
frontend no longer permits any performance preset to disable transparency or
background color math, and RockPod's current Balanced/Fighting configuration
replaces that stale file.

The corrected 600-loop Killer Instinct fullscreen/audio-on simulator gate
reported 601 rendered callbacks, zero skipped frames, 1,016 ticks, and 58
measured emulation/draw fps in the SDL dummy-audio environment. This directly
removes the 31-of-32 suppression failure; only physical iPod timing can prove
whether the fully rendered portable core itself sustains the target rate.

## Staged ARM926 optimization and profiling pass

This pass is staged locally and was explicitly not copied to the iPod. Native
hardware builds add LTO, `FAST_ALIGNED_LSB_WORD_ACCESS`, and
`-fno-unroll-loops`, matching the safe CPU-side flags used by upstream's
ARM926/Miyoo target while continuing to link the portable `ppu_.o`, `gfx.o`,
and `tile.o` renderer. The rejected ARM graphics modules remain absent.

The fullscreen RGB565 scaler uses aligned paired 32-bit writes only when the
destination permits it. At startup, the optimized output is compared across a
complete scanline with both the reference scaler and the exact nearest-neighbor
mapping. Core, scaler, and blocking LCD-update tick totals are recorded in the
exit log. The makefile now makes every frontend object depend on its flag file,
preventing stale mixed-LTO builds.

The clean 600-loop Killer Instinct fullscreen/audio-on simulator gate rendered
601 callbacks, skipped zero, completed in 1,006 ticks, and reported 59 emulated
and drawn fps. Its coarse profile attributed 994 ticks inside `retro_run`, 3 to
scaling, and 629 to LCD update work included within that core callback. The
hardware plugin is 542 KiB with SHA-256
`acb0d4ff0c2abef384efae4265591d610287d731d64f887609ca17580810e29e`.
Both hardware packaging and 65 scoped tests passed. Real iPod performance is
unmeasured for this staged artifact.

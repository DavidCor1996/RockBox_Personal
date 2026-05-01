# Rockboy Performance and Audio Notes

## Scope

Target: Rockboy on iPod Video/5G, with compatibility preserved for other
Rockbox targets.

Touched areas:

- Profiling counters and `profile.log` output.
- Rockbox PCM submit buffering.
- Sound mixer inner-loop work.
- Frame pacing and autoskip behavior.

## Phase 1: Profiling

The existing Rockboy profiler now records:

- Frame time.
- CPU emulation time.
- LCD render time.
- Scale time.
- Blit/update time.
- Audio mix time.
- PCM submit wait time.
- PCM underrun count.
- Rendered frame count.
- Skipped frame count.
- Save-state time.

`profile.log` is still appended under `/.rockbox/rockboy/profile.log` when
the Rockboy profile mode is set to `Overlay + Log`.

Each log row is a single run summary with average and peak tick values for
the timed counters.

## Phase 2: PCM Buffering

The Rockbox PCM submit path now uses a four-slot software ring plus the
existing hardware callback buffer. This keeps one ring slot free for the
mixer while up to three completed buffers are queued for playback.

Behavioral notes:

- `rockboy_pcm_submit()` still accepts a filled `pcm.buf` and returns after
  the buffer has been queued.
- It only waits when the ring is full.
- Callback underruns are filled with silence and counted by the profiler.
- The existing sample-rate selection is unchanged, including the 11 kHz path
  on targets that expose it and the 44.1 kHz fallback elsewhere.

## Phase 3: Sound Mixer

`sound_mix()` now caches repeated mixer state once per call:

- Stereo balance.
- Sound quality step.
- Output gain multipliers.
- Digital-output values.

Each channel returns early when it is fully disabled. Envelope, length, sweep,
wave, and noise behavior remains tied to the existing channel state and update
rules.

No user-facing audio quality setting was added; the safe mixer cleanup did not
require splitting behavior into Accurate/Balanced/Fast modes.

## Phase 4: Frame Pacing

The old ten-tick `framesin < 6` / `framesin > 6` autoskip adjustment has been
replaced with deadline-based pacing at 60 fps.

Behavioral notes:

- Audio generation still runs every emulated frame and is not skipped.
- The loop yields when ahead of the frame deadline.
- Frameskip rises based on measured lateness.
- Frameskip decays slowly after stable frames to avoid oscillation.
- Frameskip is clamped to `options.maxskip`.

## Verification

Build:

- Passed: `TMPDIR=/home/david/Documents/RockBox_Personal-master/tmp/build-tmp make -B -C build-sim-video-5g /home/david/Documents/RockBox_Personal-master/build-sim-video-5g/apps/plugins/rockboy/rockboy.rock`
- Note: `/tmp` was full, so `TMPDIR` was redirected into the workspace.

ROM availability found locally:

- `build-sim-video-5g/simdisk/gameboy/Tetris (World) (Rev 1).gb`
- `build-sim-video-5g/simdisk/gameboy/Pokemon - Red Version (USA, Europe) (SGB Enhanced).gb`
- `build-sim-video-5g/simdisk/gameboy/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc`
- `/home/david/Documents/RockBox_Personal-master/Pokemon - Crystal Version (USA).gbc`

Runtime:

- Pending: ROM runtime verification for Tetris, Pokemon Red, Pokemon Crystal,
  and Zelda Oracle.
- Pending: `profile.log` verification.
- Pending: audio crackle, runaway frameskip, visual corruption, and clean exit
  checks.

The first simulator autostart attempt used the wrong open-plugin key and
opened a different plugin. The simulator config and Rockboy options were
restored afterward. The root-menu loader uses the `Start Screen` key for
`start in screen: plugin`.

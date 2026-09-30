# Vortex retail compatibility

This port runs the original full game's executable and assets. It is not yet
verified as retail-identical: rendering, complete progression, and physical
iPod timing remain acceptance work.

## Reproducible game identity

- GUID: `12345`; build: `2563290`; Platform ID: `1`.
- Decrypted executable: 414,600 bytes.
- SHA-256: `6b89c1f3d8ce439a6352d9b577c82a2da4a094df31baac031d840b5b547bf06b`.
- Preservation catalog: https://github.com/Olsro/ipodclickwheelgamespreservationproject
- Full package used for this checkpoint: https://archive.org/download/ipodclassicgames/12345.zip
- Behavioral cross-reference: https://github.com/DogParty/iPodClickWheelRecomps/tree/main/Vortex

The GitHub preservation release contains encrypted executable data. The legacy
archive supplied the decrypted executable matching the existing port's hash.
Proprietary files remain in ignored local storage, outside the source patch.
The recompilation project's GPLv3 code was not copied into this GPLv2 tree;
its recovered ABI and timing observations were used as references.

## September 2026 changes

- Correct ARM signed long multiply, multiply flags, and ADC/SBC/RSC flag
  boundaries. Synthetic tests exercise the actual single-step and batched
  interpreter against integer reference calculations.
- Pace Vortex at 30 event frames per second, matching the reference runtime.
  Preserve real elapsed time on hardware and avoid short catch-up frames.
  Deterministic simulator input retains its original elapsed-time schedule.
- Exercise the batched interpreter in the simulator's main event entry.
  Host throughput is not an estimate of iPod performance.
- Stream the original three music tracks from prepared 44.1 kHz stereo PCM
  sidecars. A fixed 32 KiB ring feeds the existing playback mixer; file I/O
  happens on the VM thread. Resample the game's 27 kHz effects for mixing at
  44.1 kHz. Stop the mixer before closing music and restoring its frequency.

Preparation requires FFmpeg on the host, after importing the game:

```sh
python3 -m tools.ipodgames.prepare_music /path/to/games/12345/game.igame
```

This keeps the original AAC assets and adds `a.m4a.pcm`, `b.m4a.pcm`,
`c.m4a.pcm`, and a hash manifest. The sidecars must accompany the imported
game on the device. Missing sidecars leave effects available but music silent;
the runtime log records a music file failure. Music uses approximately
176,400 bytes of disk space per second, without retaining whole tracks in RAM.

## Verified scope

The isolated simulator disk completes first-run name entry, the circular menu,
New Game, tunnel transition, paddle movement/firing, pause, and Save & Exit.
At 30 Hz the script exits after 1,120 events without an ARM fault. Four successful
writes total 21,356 bytes, including an 18,988-byte `quicka` save. Three music
tracks register and playback starts; the initial batched run consumed 527,360
stereo frames with zero reported underruns or file failures. This validates
buffer delivery, not audible quality or stock synchronization.
The single-step run produces byte-identical final framebuffer data and all
four save files compared with the batched run.

The simulator and iPod 6G plugin compile. Sixteen host tests pass, covering
the import tools, ARM arithmetic, and music ring wrap, partial tails, repeat,
pause/resume, switching, invalid input, and file closure. This is not a clean
whole-firmware qualification or an iPod 5G build result.

Example isolated simulator gate (use a fresh test save directory):

```sh
python3 tools/ipodgames_sim_gate.py --simdisk /path/to/test-disk \
  --metadata /path/to/test-disk/.rockbox/ipodgames/games/12345/game.igame \
  --generic --vortex-play --frames 1200 --hardware-path --require-music \
  --stop-after-return
```

The final flag stops the headless host only after `plugin_load` returns
successfully. It does not test the simulator's subsequent poweroff path.

## Remaining work before a full-stock claim

1. Compare two-sampler masks, depth ordering, tunnel backgrounds, menu layering,
   and name-editor textures against captured retail frames. Three packaged
   textures have suspicious format/size combinations; do not rewrite their
   headers without verifying how retail interprets them.
2. Verify music stop/pause, volume, shuffle, repeat semantics, track transitions,
   and synchronization against retail. Only observed registration/play/repeat
   paths have been integrated; ring pause/resume tests do not establish the
   guest's pause ABI. No user playlist is substituted for game music.
3. Exercise every level, power-up, scoring/life transition, unlock, game-over,
   final completion, and save/relaunch/resume path. A first-level save is not
   evidence of full-game progression or save compatibility with retail.
4. Measure real iPod frame times, wheel latency, audio underruns, and memory
   headroom during the busiest scenes. Qualify Database/Files playback into
   and out of the game, repeated launch/exit, and error paths on hardware.

No device deployment or physical gameplay qualification was performed for
this checkpoint. Keep the older bring-up report as historical evidence, not
as evidence that this newer binary has been tested on a device.

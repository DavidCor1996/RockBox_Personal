# Rockboy Performance Boost Spec

## Goal

Improve Rockboy performance enough to reduce frameskip on slower Rockbox
targets, especially iPod Video/5G, while preserving the current behavior as
the compatibility baseline.

The most promising path is not a full emulator rewrite. Rockboy already has
several important Gnuboy-era optimizations: direct memory read/write maps,
dirty decoded tile patterns, line-based rendering, iPod Video fast scaling,
CPU boost while running, and recent audio/pacing profiling. The next useful
boost should be a measured set of optional fast paths around the two remaining
hot areas:

- CPU opcode dispatch and hot register/memory access.
- LCD line rendering when a scanline does not need the full CGB/sprite/priority
  path.

## Online Research Summary

- Pan Docs documents the hardware cost model: a Game Boy frame is 154
  scanlines and 70224 dots, with visible drawing on 144 scanlines at about
  59.7 fps. It also documents variable Mode 3 timing from scroll, window, and
  OBJ penalties. That means a fast renderer must be explicitly a compatibility
  tradeoff unless it preserves scanline state timing.
  Source: https://gbdev.io/pandocs/Rendering.html
- Pan Docs also documents STAT behavior, LY/LYC comparison, LCD modes, and the
  DMG STAT write quirk. These are regression risks for any event coalescing or
  LCDC simplification.
  Source: https://gbdev.io/pandocs/STAT.html
- SameBoy's current SM83 core is heavily timing-aware, but its source comments
  still call out opcode decoding overhead. It explicitly says `LD r,r` is
  extremely common and that runtime decoding is a significant performance hit,
  so it generates dedicated opcode functions for those cases.
  Source: https://github.com/LIJI32/SameBoy
- Gambatte's CPU core keeps hot CPU state local and uses macros partly because
  older GCC had trouble keeping hot variables in registers when inline
  functions took addresses/references. This maps directly to Rockboy's large
  C switch interpreter.
  Source: https://github.com/libretro/gambatte-libretro
- mGBA positions itself as a fast, accurate emulator that runs full speed on
  low-end hardware and still exposes frameskip. This supports the strategy of
  keeping accuracy and speed as separate measured dimensions, rather than
  making a single risky fast path mandatory.
  Source: https://github.com/mgba-emu/mgba
- PyBoy's performance notes show how expensive rendering can be for Game Boy
  emulation: rendering fewer frames can be far faster than full rendering, and
  no-render mode is faster again. Rockboy already skips rendering for skipped
  frames, so the useful lesson is to make every actually rendered line cheaper.
  Source: https://github.com/Baekalfen/PyBoy

## Local Findings

- `apps/plugins/rockboy/fastmem.c` uses nibble-indexed `mbc.rmap` and
  `mbc.wmap` tables to bypass function calls for normal memory ranges.
- `apps/plugins/rockboy/HACKING` reports that this memory-map optimization
  reduced `mem_read` from about 13.17% of profile time to about 0.59% in the
  old Gnuboy benchmark. Reworking memory maps is therefore unlikely to produce
  another large gain unless a profile proves a specific miss path is hot.
- `apps/plugins/rockboy/lcd.c` already has dirty tile decode via `patdirty` and
  `anydirty`, separate DMG/CGB render paths, sprite enumeration caps, and an
  iPod Video 320x240 fast scaling path.
- `apps/plugins/rockboy/emu.c` already avoids LCD refresh work when `fb.enabled`
  is false and has deadline-based frame pacing.
- `docs/rockboy-performance-audio.md` records recent work on profiling,
  PCM buffering, sound mixer cleanup, and frame pacing. This spec builds on
  that instead of revisiting audio first.

## Recommendation

Yes, there is a credible route to a good performance boost, but it should be
implemented in tiers:

1. Keep `Accurate` as the current default.
2. Add `Balanced` fast paths that are automatically disabled on risky lines.
3. Add an opt-in `Fast` mode only after the measured safe fast paths are stable.

The first target should be a 15-30% reduction in average frame time on the
iPod Video/5G profile corpus. Anything less than 10% should not justify broad
renderer or CPU-core churn.

## Phase 0: Baseline and Acceptance Harness

Add repeatable profile runs before changing behavior.

Tasks:

- Use `Profile: Overlay + Log` and capture per-ROM summaries from
  `/.rockbox/rockboy/profile.log`.
- Benchmark at least:
  - Tetris, DMG, low sprite load.
  - Pokemon Red, DMG + menus + scrolling.
  - Pokemon Crystal, CGB + MBC3/RTC path.
  - Zelda Oracle, CGB + scrolling/sprites.
- Run each ROM with:
  - sound on/off,
  - scaling native/fullscreen,
  - frameskip max 0/2/4.
- Record:
  - average frame ticks,
  - average CPU ticks,
  - average render/scale/blit ticks,
  - skipped frames,
  - PCM underruns,
  - visible corruption notes.

Acceptance:

- A candidate fast path must have before/after logs for the same ROM, build,
  and settings.
- No default behavior change until the profile corpus is clean.
- Simulator runs must be used as the main tuning loop. Keep testing candidate
  changes in the simulator across the profile corpus until the best measured
  configuration is found, not merely until the first improvement appears.

Status - 2026-06-20:

- Added `tools/rockboy_profile_gate.py` for the repeatable simulator side of
  the harness.
- The gate verifies the simulator build, Rockboy plugin, and ROM corpus, then
  prepares an isolated simdisk with `Profile: Overlay + Log` enabled in
  `.rockbox/rockboy/options`.
- The gate validates collected `profile.log` entries after a simulator run and
  requires the Phase 1 counters before accepting the log.
- The current simulator CLI cannot directly launch Rockboy with a ROM
  parameter, so the gate prints the exact simulator command and ROM manifest for
  the manual plugin-launch portion.
- Prepare/validate coverage uses four staged ROMs: Tetris, Pokemon Red,
  Zelda Oracle, and Mario Tennis. Launch stability remains covered by the
  simulator-first gate.
- The gate can now write a direct-start Rockbox `plugin.dat` plus matching
  `config.cfg` start-screen entries, stage the freshly built `rockboy.rock`,
  clear stale copied `profile.log` data, and print an autowrite simulator
  command. This makes repeatable single-ROM before/after profile runs possible
  without manual menu navigation.
- `--rom-only` can target one ROM pattern without appending the default corpus,
  which was used for Pokemon Red and Oracle of Ages direct-start runs.
- Direct-start validation passed with Tetris in the iPod Video simulator:
  `tools/rockboy_profile_gate.py --validate /tmp/rockboy-profile-gate-4p0ms7cp/simdisk`.

## Phase 1: Profiling Detail

Current profiling separates CPU, render, scale, blit, audio, PCM wait, and
save. Add enough detail to decide whether CPU dispatch or LCD work is actually
the bottleneck on the target build.

Add optional counters for:

- CPU op count and average cycles per `cpu_emulate` call.
- Slow `mem_read` / `mem_write` counts when `mbc.rmap` or `mbc.wmap` misses.
- VRAM writes that dirty pattern data.
- Lines with no sprites.
- Lines with no window.
- DMG lines vs CGB lines.
- Lines that use CGB priority.
- Lines eligible for each renderer fast path.

Acceptance:

- The new counters compile out or stay cheap when profiling is off.
- Profile logs remain single-line summaries.

Status - 2026-06-20:

- Implemented Phase 1 profiling detail in the current working tree.
- `profile.log` remains a single-line summary and now includes:
  - `cpu_ops`
  - `slow_mem_reads`
  - `slow_mem_writes`
  - `vram_dirty_writes`
  - `lcd_lines`
  - `dmg_lines`
  - `cgb_lines`
  - `no_sprite_lines`
  - `no_window_lines`
  - `dmg_bg_only_eligible`
  - `dmg_bg_window_no_spr_eligible`
  - `cgb_no_sprite_lines`
- Instrumentation points:
  - `apps/plugins/rockboy/cpu.c` counts executed CPU opcodes.
  - `apps/plugins/rockboy/fastmem.c` counts slow read/write map misses.
  - `apps/plugins/rockboy/lcd.c` counts dirty VRAM writes and scanline classes.
  - `apps/plugins/rockboy/profiler.c` writes the extended profile fields.
- Validation passed:
  - `pytest tests/test_online_artwork.py tests/test_rockbox_wps_art.py tests/test_album_art_first_load_source.py tests/test_rockboy_profile_instrumentation.py tests/test_rockbox_games.py -q`
  - `pytest tests/test_rockboy_profile_gate.py tests/test_rockboy_profile_instrumentation.py -q`
  - `tools/rockboy_profile_gate.py`
  - `make -C build-sim-video-5g -j4`
  - `tools/simulator_first_gate.sh --target ipodvideo --skip-build --rockpod-tests --theme-tests --smoke --timeout 3 --manual-checklist --allow-mounted-ipod --evidence-file /tmp/rockbox-album-gameboy-final4-gate.txt`
  - Full combined gate result: 629 RockPod tests passed, 53 WPS/SBS/FMS tests
    passed, and the simulator stayed alive for the 3 second smoke run.
- Follow-up performance guard:
  - Profile event/timing calls are now inline-guarded in `profiler.h`, so
    normal gameplay with profiling off avoids the per-op/per-line profiler
    function calls introduced by instrumentation.
  - The profile log now records fast-path use/reject counters:
    `dmg_bg_only_used`, `dmg_bg_only_rejected`,
    `dmg_bg_window_no_spr_used`, `dmg_bg_window_no_spr_rejected`,
    `cgb_bg_only_eligible`, `cgb_bg_only_used`, `cgb_bg_only_rejected`,
    `cgb_bg_window_no_spr_eligible`, `cgb_bg_window_no_spr_used`, and
    `cgb_bg_window_no_spr_rejected`.

## Phase 2: CPU Interpreter Hot Path

Add a compile-time optional CPU dispatch backend while keeping the current
switch interpreter as the fallback.

Preferred order:

1. Move hot CPU state into locals for the duration of `cpu_emulate`, then write
   back on exit, interrupt, or return. This mirrors Gambatte's approach and is
   safer than starting with dynamic recompilation.
2. Add a GCC/Clang computed-goto backend under a feature macro such as
   `ROCKBOY_THREADED_CPU`, with the existing switch backend retained for
   portability.
3. If the compiler backend is too fragile, generate specialized opcode handlers
   for the high-frequency register/register and ALU cases only.

Do not revive the existing `DYNAREC` path as the first step. It appears to be
old 68k/ColdFire-oriented code, is not in `SOURCES`, and would be higher risk
than a portable threaded interpreter on current Rockbox ARM targets.

Acceptance:

- CPU profile time improves by at least 10% on at least two profile ROMs, or
  the phase is abandoned.
- Save/load state remains byte-for-byte compatible.
- Interrupt, HALT, STOP, timer, and LCDC event timing tests still pass at the
  existing Rockboy accuracy level.

Status - 2026-06-20:

- Started with the lowest-risk interpreter locality change.
- `cpu_emulate()` now keeps opcode scratch state (`op`, `cbop`, `acc`, `b`,
  and `w`) as per-call locals instead of `static IBSS` objects, giving the
  compiler a chance to keep those values in registers in the hot switch loop.
- The existing switch interpreter remains the compatibility fallback; no
  threaded dispatch or generated opcode backend has been introduced.
- Direct-start simulator profile gates still pass after the CPU locality
  change.

## Phase 3: LCD Fast-Line Renderer

Add small, explicit fast paths inside `lcd_refreshline()` after line state is
computed and before the full renderer path.

Fast path candidates:

- `DMG_BG_ONLY`: no window, no visible sprites, DMG mode, no CGB priority.
  Render background directly from cached `patpix` to `BUF` and scale.
- `DMG_BG_WINDOW_NO_SPR`: DMG mode with window but no sprites. Avoid sprite
  copy/priority setup entirely.
- `CGB_NO_SPR_NO_PRI`: CGB mode, no visible sprites, no priority attributes in
  the line's tile map. Avoid priority buffer generation and sprite blending.
- `UNCHANGED_BLIT_REGION`: when scale mode and rotation imply a contiguous
  destination, use target-specific line copy helpers instead of generic
  per-pixel fixed-point scaling.

Guardrails:

- Never skip `updatepatpix()` when `anydirty` is set.
- Never use a CGB fast path if the relevant attribute map line has priority
  bits or nontrivial palette state that the path does not handle.
- Keep full renderer fallback per line, not per ROM.
- Add counters for eligible, used, and rejected lines.

Acceptance:

- Render+scale profile time improves by at least 20% on the ROMs where the
  fast path is commonly eligible.
- No visible corruption in the profile corpus.
- The fast path can be disabled at runtime or compile time for bisection.

Status - 2026-06-20:

- Started with the lowest-risk no-sprite-line fast path.
- `lcd_refreshline()` now skips the `spr_scan()` function call entirely when
  `spr_enum()` found no visible sprites for the line. Sprite enumeration and
  all full renderer behavior remain unchanged.
- Added background-only direct scan paths:
  - `DMG_BG_ONLY`: no CGB, no visible sprites, and no window.
  - `CGB_BG_ONLY`: CGB, no visible sprites, and no window; preserves tile bank,
    x/y flip, and palette attribute handling.
- Added window/no-sprite direct scan paths:
  - `DMG_BG_WINDOW_NO_SPR`: no CGB, no visible sprites, and an on-screen window.
  - `CGB_BG_WINDOW_NO_SPR`: CGB, no visible sprites, and an on-screen window;
    preserves tile bank, x/y flip, and palette attribute handling.
- Fast-line rendering is disabled under the existing `Quality` performance
  preset, giving a runtime fallback for bisection while keeping the current
  Balanced/Performance presets on the fast path.
- Direct-start Tetris evidence after the DMG fast path:
  - `lcd_lines=17280`
  - `no_sprite_lines=13688`
  - `dmg_bg_only_eligible=13688`
  - `dmg_bg_only_used=13688`
  - `dmg_bg_only_rejected=0`
  - profile validation passed in the iPod Video simulator.
- Direct-start Oracle of Ages CGB evidence after the CGB fast path:
  - `cgb_lines=17280`
  - `cgb_bg_only_eligible=17280`
  - `cgb_bg_only_used=17280`
  - `cgb_bg_only_rejected=0`
  - profile validation passed in the iPod Video simulator.
- Direct-start Pokemon Red evidence after the DMG window/no-sprite fast path:
  - `dmg_bg_window_no_spr_eligible=17280`
  - `dmg_bg_window_no_spr_used=17280`
  - `dmg_bg_window_no_spr_rejected=0`
  - profile validation passed in the iPod Video simulator.
- Direct-start Mario Tennis CGB evidence after adding the CGB window/no-sprite
  fast path:
  - `cgb_bg_only_eligible=15476`
  - `cgb_bg_only_used=15476`
  - `cgb_bg_window_no_spr_eligible=0` in the sampled boot window
  - profile validation passed in the iPod Video simulator.
- Build validation:
  - `make -C build-sim-video-5g -j8`
  - `make -C build-hw-ipodvideo-5g -j8`
  - hardware rebuild required clearing stale generated Rockboy objects that
    still referenced the previous profiler function symbols.

## Phase 4: Compatibility Tiers

Expose performance policy as a Rockboy option once fast paths are proven.

Options:

- `Accurate`: current path only.
- `Balanced`: safe fast paths that preserve line-level behavior.
- `Fast`: opt-in paths that may break rare raster/palette effects but reduce
  frame drops.

Status - 2026-06-20:

- Runtime bisection is available through the existing Rockboy performance
  preset:
  - `Quality` uses the full renderer path for background-only lines.
  - `Balanced` and `Performance` use the measured background-only fast paths.
- A larger UI rename to `Accurate/Balanced/Fast` has not been done because the
  current menu already exposes `Balanced/Performance/Quality` and changing
  persisted option semantics would be higher risk than the renderer work.

Default:

- Keep `Accurate` until profile logs and manual ROM checks show the tuned
  simulator-winning configuration is clean across the corpus.
- Promote a tier only after simulator profiling shows it is the best available
  performance/compatibility point for the current implementation.
- Once that best-performing clean configuration is found, make it Rockboy's
  default settings so normal users get the performance benefit without manual
  tuning.

## Phase 5: Optional Target-Specific Work

Only after portable fast paths are measured:

- Add ARM-specialized line copy/scale helpers for RGB565 targets.
- Add iPod Video/5G-specific fast fullscreen variants that extend the existing
  320x240 path.
- Consider ICODE placement changes only if map/profile output shows hot code is
  missing instruction cache.

## Risks

- Game Boy raster effects depend on scanline and sometimes dot-level behavior.
  Pan Docs documents variable Mode 3 timing, window penalties, sprite penalties,
  and STAT quirks; fast rendering must not silently claim more accuracy than it
  has.
- The current memory-map system is already effective. Optimizing it further may
  add complexity for little return.
- Full dynamic recompilation is likely too much complexity for Rockbox plugin
  constraints and target diversity.
- Renderer shortcuts can improve average speed but still miss the worst CGB
  lines if a game uses many sprites, window, priority, or frequent VRAM writes.

## Stop Criteria

Stop or defer the effort if:

- Phase 1 shows audio/PCM wait, not CPU/render, is still the dominant cost.
- CPU dispatch changes improve total frame time by less than 5%.
- Renderer fast paths are eligible on fewer than 25% of rendered lines in the
  target ROM corpus.
- The fast paths introduce hard-to-debug visual issues in common games.

## Expected Outcome

The realistic win is a measured `Balanced` mode that reduces total frame time
by roughly 15-30% on favorable ROMs, with larger gains in scenes that have no
sprites or simple DMG backgrounds. The highest-risk/highest-effort option,
dynamic recompilation, should stay out of scope unless the portable CPU and LCD
fast paths fail and profiles still show CPU dispatch dominates.

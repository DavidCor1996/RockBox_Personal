# Composite graphics and TV transitions

## Intent

Keep the verified original Apple graphics and fonts. Improve their rendering
for a small interlaced composite picture, with the same restrained feedback
across TV lists, app grids, shelves and dialogs. This is a personal-tree change.

The centered artwork revision is deployed first: SHA-256
`8575bd67f87db4bee2f83ba00361e4dc935de1da5cc79eb2e6d395b8f7051b84`.
Both boot copies match; all 11 database files, configuration, codecs and the
isolated firmware slots were verified unchanged. Leave the device mounted.

## Graphics

- Keep the mirrored handheld-WPS projection and the original silver Apple
  placeholder on idle Music.
- Interpolate artwork from resident source pixels. Resolve fractional slanted
  boundaries with coverage rather than a hard staircase.
- Sample reflections through the same filter, preserving the source aspect,
  orientation and fade. Do not sharpen edges or add dithering that can crawl
  on an interlaced display.
- Apply a vertical 1:2:1 filter to the projected cover/reflection only. Keep
  original neighbours while filtering in place so softness does not accumulate
  down the image. Text, buttons and focus graphics are excluded.
- Keep app icons at one stable size and position when focus moves. Use the
  original Apple focus graphics to communicate selection.
- Keep existing two-pixel borders. Do not change NTSC timing, driver filters,
  decoder output, picture geometry, or the handheld UI.

## Motion

Use a common 160 ms smoothstep reveal for the destination focus strip. For
screens without a narrow focus strip, reveal the title strip instead. This
pass does not implement full-screen slides or crossfades: those would require
another large framebuffer or a change to the video scanout interface.

The completed destination is composed once. Cache at most 32 rows of it,
present the strip from 70% brightness to its exact original pixels, and yield
between frames at no more than 25 fps. Derive progress from elapsed ticks;
missed frames do not prolong the effect. Unchanged selection/content does not
restart a fade. Do not dim the picture or artwork outside the strip.

Queued input, Hold, inactive output or a busy/unavailable Music database must
skip or finish the effect immediately. Never consume input, sleep in interrupt
context, load artwork, allocate memory, or query metadata in the animation
loop. Guide rendering, diagnostic screens, video frames and regular WPS
updates retain their immediate presentation path.

One fixed strip costs 426 * 32 * 2 = 27,264 bytes, plus small identity state.
No core allocation, playback shrink, PCM changes, new artwork slot, or shared
audio buffer is permitted. A strip rather than a second framebuffer bounds
the cost; an unsuitable strip falls back to direct presentation.

## Validation and rollout

Run the production renderer with address/undefined-behavior checking across
both aspect ratios, all text sizes and overscan settings. Check projection
edges, readable artwork orientation, reflection bounds, monotonic transition
samples, unchanged-frame suppression and exact final pixels. Simulate input,
Hold, database-busy and output-loss cancellation. Build simulator and native
firmware; inspect native BSS and stack cost and record render timing.

Run the existing cache, ownership and remote/navigation gates and attempt the
full simulator navigation regression. Its existing recursive fixture links
must be reported if still blocking. Do not claim hardware picture quality,
playback stress or CRT flicker qualification from host images. Keep the new
graphics/transition candidate local until the required validation is met.

## Implementation results

Implemented in `apps/gui/tv_ui.c`, with production-renderer coverage in
`tools/tests/test_tv_renderer.py`. The projection computes source increments
once per column, avoiding per-pixel coordinate division. The fade modifies only its
bounded rectangle, leaving neighbouring artwork untouched at every sample.

- Native iPod 6G firmware and simulator core builds pass.
- Renderer passes 24 aspect/text/overscan combinations and existing guide,
  artwork-cache and app cases with ASan/UBSan. Leak detection is disabled
  because this host's ptrace setup prevents LeakSanitizer from running.
- Projection checks cover fractional edges, mixed interpolated colors,
  original source orientation, extreme aspect ratios and reflection bounds.
  A one-pixel black/white stripe input is attenuated to nearly equal midtones.
- Transition checks produce five samples over 160 simulated milliseconds,
  assert monotonic brightness and exact final pixels, suppress repeated
  identities, and finish on queued input, Hold, database-busy or output loss.
  A neighbouring region remains unchanged during each narrow-strip sample.
- Existing TV ownership, album-cache, Home-navigation and remote-navigation
  gates pass. Scoped whitespace validation passes.
- Native ELF: text 3,224,636, data 17,724, BSS 9,522,756 bytes. Relative to the
  deployed centered-artwork build: text +1,552 bytes, data unchanged, BSS
  +27,264 bytes. Small state fits existing alignment padding.
- ARM `present_transition` has a 36-byte local frame plus 36 bytes of saved
  registers; the strip is static, never on the stack. The list renderer uses
  332 local bytes plus 36 bytes of saved registers, including its existing
  256-byte label buffer.
- Full navigation regression was attempted again and remains blocked during
  fixture copying by the pre-existing recursive `simdisk` preview tree.
  Physical composite quality and playback/navigation stress are still untested.

The candidate firmware SHA-256 is
`17fb7999ba264bca023de6cc9c92ba1365a0e55c171d94f9aefa200e8ab51e58`.
It is built locally, not deployed over the centered-artwork revision. The iPod
was subsequently disconnected. Still and focus-fade previews are stored in
`docs/tv-composite-graphics/`; the GIF holds the final sample for readability.
Host samples establish timing logic, not measured ARM/CRT frame delivery.

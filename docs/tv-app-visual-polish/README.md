# TV visual pass — 2026-09-27

Visual-only changes to the existing TV renderer and TV Netflix artwork choice:

- DIRECTV header artwork from the receiver screenshot that matches the supplied
  reference, with reference proportions, foreground colors and picture border.
  Existing channel logos remain. Rating field accommodates `TV-PG-V` in the
  320 x 240 preview. Dynamic text still uses the resident Apple font: an exact
  receiver font was not verified and is not claimed.
- Netflix artwork keeps its aspect ratio, bottom alignment and fitted selection
  border. TV category illustrations are replaced by the real Netflix logo;
  synced movie/show posters remain. Handheld category behavior is unchanged.
- The Home section tab uses Apple's real BackRow inactive artwork when focus
  is in the contents, and its original blue active artwork when tabs have focus.
- Rounded RGB565 blending and alpha-aware bilinear scaling retain image edges.

Previews use the production renderer with cached real artwork. Native and
simulator builds plus bounded ASan/UBSan visual captures are the validation;
no stress/navigation loop testing was run for this follow-up, per request.
Screenshots are fixture previews, not physical composite-output captures.

No playback, PCM, playlist, plugin-buffer or navigation behavior was changed.
The original header adds 69,252 read-only pixel bytes; the inactive tab adds
7,332. Renderer BSS remains 357,688 bytes, with no additional framebuffer.
Source provenance is in `assets/ipodjs/sources/directv/guide/SOURCES.md`,
`assets/ipodjs/sources/apple-tv/SOURCES.md`, and `docs/tv-apple-assets.json`.

Deployment verification is recorded after copying the completed firmware to
both personal boot locations and checking preservation of database/config/
codec files and isolated firmware. The upstream and TV-test slots are untouched.

## Verified deployment

Installed to `/rockbox.ipod` and `/.rockbox/rockbox.ipod` on the 477 GB personal
6G iPod; both SHA256 values match the local build:
`b4d206ef76126cb77c082a78f6d51eb07f3fdca927f5e542a1053a40ee072c43`.
Preservation checks passed for 57 database/configuration/codec/isolated-firmware
files. Synced and left mounted. Native and simulator builds completed
successfully; unrelated plugin compiler warnings remain in the native log.
Final native firmware text/data/BSS: 3,390,064 / 17,716 / 9,571,588 bytes.
The `tv_guide_render` ARM prologue uses 240 bytes including saved registers.
The bounded rating preview also displays `TV-14-DLSV` in full.

## Guide readability follow-up

After hardware feedback that the schedule was too small, schedule text was
increased from reference height 16 to 21 (12 to 16 pixels in the 320x240
preview) and uses natural-width glyphs. Six rows now use reference height 23
instead of 20, consuming the spare bottom margin; channel logos grow with the
rows. Time labels are enlarged. Header, metadata, picture and description are
pixel-identical to the prior preview. Longer cell titles truncate earlier;
the selected full title remains in the existing top field.

Native and simulator firmware builds passed, as did bounded sanitizer preview
rendering and a pixel comparison of the unchanged top section. No stress tests.
No data/BSS growth; firmware text increases by 24 bytes. Deployed and verified
both boot paths with SHA256:
`6af71ca33660215aa164fd36dc0b7b818497e7643e500de3a15baa1ec24e229f`.
All 57 preserved-file hashes match; synced and left mounted.
See `directv-readable.png` for the updated preview.

## Main-menu responsiveness follow-up

Home now presents its completed frame immediately instead of running the
160 ms selection fade. This removes decorative sleeps and repeated full-frame
submissions before the next action can be handled. Apps/page/tab navigation,
icon caching, input mappings and other application transitions are unchanged.
No cache or framebuffer is added; native text grows by 8 bytes, data/BSS unchanged.

Both firmware builds passed. Focused ASan/UBSan renderer check:
`ASAN_OPTIONS=detect_leaks=0 python3 tools/tests/test_tv_renderer.py build-sim-ipod6g /tmp/tv-scroll-check /tmp/tv-app-fixtures --home-scroll-only`
checks selection/page/tab changes for zero tick advancement and bitmap decoding,
and verifies the non-Home fade remains. No stress test or hardware latency
benchmark was performed. Both iPod boot copies verified and synced with SHA256
`a81c4be497054d3c0bb9be7a447184bbc9f653edbe53145bf14278b47a1b12c7`;
57 preserved file checksums match. Left mounted.

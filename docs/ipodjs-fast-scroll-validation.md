# iPodJS alphabetical jumping

Fast wheel input in the existing eligible Music lists activates the letter
plate. Subsequent wheel detents select the first item of the next populated
A–Z/number bucket, in either direction, without wrapping. Ordinary slow
scrolling remains row-based. The existing Latin accent folding is retained;
lists containing unsupported initials fall back to ordinary scrolling.

The renderer uses the existing extracted RetailOS 2.0.4 resources 004/005
(`system-quick-scroll` and `system-quick-scroll-123`, 74x70), and the prepared
Apple embedded bitmap font `23-Helvetica-Apple.fnt`. Both plates and the font
must be available before fast scrolling activates. No new artwork is generated.

Apple's iPod classic user guide describes fast wheel movement to activate the
alphabet display and lifting the thumb to resume normal scrolling:
https://cdsassets.apple.com/live/6GJYWVAV/user/ma630_ipod_classic_120gb_en.pdf

This change completes the existing implementation's exit and index lifecycle:

- Poll native wheel contact through `HAVE_WHEEL_POSITION` at 50 ms intervals
  while the overlay is active, after draining queued wheel events.
- Retain the one-second inactivity fallback, including in the simulator,
  which has no wheel-position API.
- Process expiration before new input, without swallowing that input.
- Invalidate the index when a list is initialized or its contents are reset,
  even when the address, title and count are reused.
- Cache failed index attempts until reload; reject failed name lookups and
  invalid selected buckets before accessing the first-item array.
- Clear fast mode when returning from Hold handling.

No new core allocation, image buffer, file operation in the overlay draw path,
or audio/PCM/playlist operation is introduced. The native index remains 192
bytes; the new boolean uses existing structure padding. ARM stack inspection
shows a 128-byte name buffer plus an 8-byte saved-register frame in the bucket
helper, and 40 bytes in the step function (excluding called functions).

Validation commands:

```
python3 tools/tests/test_ipodjs_fast_scroll.py
make -C build-sim-ipod6g -j4
make -C build-hw-ipod6g -j4 bin
bash tools/ipodjs_fast_scroll_sim_regression.sh
bash tools/ipodjs_navigation_sim_regression.sh
```

The host test executes extracted production C under ASan/UBSan for sparse
buckets, direction reversal, endpoint clamping, same-size content replacement,
failed lookup caching, finger lift with queued input, and inactivity expiry.
The simulator gate stages the current private Apple assets, sets its required
Hold effect explicitly, and accepts `IPODJS_FAST_SCROLL_SOURCE_ROOT` for an
isolated fixture.

Native wheel feel and playback/navigation memory stress still require the
physical-device gate in `ipodjs-ui-memory-animation-steering.md`. Simulator
keyboard input cannot validate physical finger-lift timing. No device deploy
is part of this change.

## Session results

- Simulator and native iPod 6G builds passed.
- Host ASan/UBSan tests passed.
- A–Z simulator regression passed for short Album Artist, Artist, Albums,
  and title-sorted Songs, including visible glyph and overlay expiry checks.
- Navigation regression passed: 357 trace records, playback track
  `/Music/Fast Scroll/Iris/09.mp3`, playlist identity `('1', '1')`.
- Focused `git diff --check` and shell syntax validation passed.
- Native ELF text/data/BSS before: 2,945,452 / 11,032 / 9,024,516 bytes;
  after: 2,945,784 / 11,032 / 9,026,564 bytes. These are whole-tree incremental
  build measurements in an already modified workspace, not an isolated
  attribution of every section delta. The letter-index symbol is 192 bytes.

The supplied simulator disk had recursively nested preview directories and no
Music folder. Tests used an isolated fixture with the existing fonts/codecs,
current Apple assets, and a generated 125-second sine-wave MP3 plus the
26-entry A–Z tagcache fixture. Passing these scripted journeys does not replace
the full repeated hardware memory/playback stress gate.

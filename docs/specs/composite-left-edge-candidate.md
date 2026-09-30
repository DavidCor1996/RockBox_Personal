# Composite left-edge displacement candidate

The user reports a very narrow far-left sliver displaced slightly downward on
both static menus and moving output. Their photographs show the discontinuity
at the status-bar boundary and selector. The earlier color-fringe interpretation
was not an adequate explanation of that reported positional error.

## Candidate and uncertainty

The candidate gives the VP horizontal filter valid same-row neighbors outside
both visible edges. It uses a 704×480 YUV420 image with 32 replicated luma pixels
(and 16 chroma samples) on each side. The visible window is still 640×480,
starting at source X=32. Destination remains `(36,24) 648×432`, and H/V ratios
remain 505/284. There is no visible crop, stretch, whole-picture shift, or
hard-coded upward compensation of the first columns.

This targets an edge-fetch/filter-boundary hypothesis. The precise hardware
cause is not established, and the unit tests cannot prove that it fixes the
analog output. User confirmation on the dock is required. Field polling and
three-plane descriptor publication are unchanged, avoiding a simultaneous
unverified timing modification.

`VIDEOOUT_EDGE_GUARD_TEST` requires `VIDEOOUT_ENHANCED_TEST`. Both are enabled in
the existing normal-runtime `build-composite-personal/Makefile`. RGB partial
updates, TV artwork and direct YUV video writes now address the visible window
inside the padded stride. Guards are extended in the inactive frame before
cache publication, and on initial frame setup. The diagnostic grid retains its
original visible coordinates.

## Evidence

- The exact SHA-verified 35.2.0.4 stock image/descriptor setters accept 704×480
  with luma/chroma spans 704/352. The stock auto-crop block at
  `0x0815e460..0x0815e478` computes `(704-640)/2 << 4 = 512` for source X.
  This proves the encoding used, not the analog filter's edge behavior.
- `tools/tests/composite_stock_edge_gate.py` executes those stock operations.
- `tools/tests/composite_edge_guard_gate.py` compiles actual target RGB,
  artwork, border-extension and YUV mirror functions under ASan/UBSan. It
  checks full-frame versus 1..32-pixel left-strip partitions, each plane's
  same-row guards, artwork positioning, and bilinear YUV values against an
  independent reference. The earlier unpadded gate also passes.
- Native candidate and normal unpadded baseline builds pass. The simulator
  build remains current; this change touches only target scanout code, not
  the previously qualified app idle service or navigation lifecycle.
- The existing scanout allocation stays 1228800 bytes. Two padded frames use
  1013760 bytes; artwork plus comparison pixels use 202368, ending at 1216128.
  No playback/core allocation or memory resizing is introduced.
- Explicit 1024-byte array alignment avoids depending on incidental ELF
  placement. Candidate array base is `0x0890f400`. ELF text/data/BSS are
  2947308/11036/9032964, versus 2946980/11036/9031236 before this change:
  +328 code bytes and +1728 static alignment padding. Border-extension own ARM
  stack is 40 bytes, excluding its memset calls.
- Package generation and RetailOS asset/font verification pass. This is a
  firmware-only device update: plugin APIs and existing runtime files remain
  unchanged, so no full package overlay is needed for this correction.

Normal firmware SHA256:
`c2917ec5915f6ac37c80c7a0fac0b7f34a204999da8640bb45a32fbdc888b842`.

The deploy backs up the current firmware and info file, writes both normal
firmware locations, checks runtime/settings/database/artwork preservation, and
syncs. Its final record is `build-composite-personal/edge-deployment.json`.

After normal reboot, check the status-bar boundary and selector at the extreme
left on a still menu, then while scrolling. Check video and audio continuity.
If the step remains, this candidate has not resolved the cause; the next
investigation must distinguish VP crop/filter behavior from descriptor latch
phase using a controlled static pattern and hardware readback.

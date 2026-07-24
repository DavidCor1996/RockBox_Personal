# Pocket Sky Rockbox Port Specification

## Decision

Build **Pocket Sky** as a native, offline Rockbox application for the 320x240
iPod Video (5G/5.5G) and iPod Classic (6G) targets.

This should be a port-and-integration project, not a new planetarium written
from first principles. Use this upstream stack:

- **Astroterm v1.2.0** for the star catalogue parser, star proper-motion and
  coordinate code, stereographic projection, magnitude ordering, star names,
  constellation figures, and filtered city data.
- **Astronomy Engine C**, initially pinned to commit
  `865d3da7d8112bbc7911238052c6af4aaf877181`, for accurate Sun, Moon, and
  planet positions, Moon phase, magnitude, rise/set, twilight, and event
  calculations.
- **Rockbox native LCD, font, button, settings, and buffer APIs** for the thin
  device adaptation layer.

Do not port Astroterm's terminal UI. Do not copy a desktop or Android renderer.
Do not draw or commission celestial artwork. The sky view is generated from
catalogue records, constellation line records, upstream projection math, and
Rockbox drawing primitives.

The unavoidable original work is limited to:

- a Rockbox plugin entry point and build integration;
- an LCD renderer that consumes upstream projected coordinates;
- click-wheel input and small-screen screen flow;
- a compact, versioned on-device resource format generated from upstream data;
- cache, memory, and power management needed by iPod hardware;
- host-side import/build scripts and validation tests.

This boundary is a requirement. New astronomy algorithms, manually transcribed
catalogues, hand-authored constellation figures, custom planet art, and copied
Sky Map/Stellarium visual assets are out of scope.

## Why This Stack

### Candidate assessment

| Candidate | Useful parts | Port cost | Asset/data status | Decision |
| --- | --- | --- | --- | --- |
| Astroterm | C astronomy core, BSC5 support, projection, names, constellation figures, cities | Low to moderate after removing curses | MIT code; third-party data needs a separate provenance audit | **Primary port base** |
| Astronomy Engine C | Small self-contained C ephemeris, horizontal coordinates, phase, rise/set, twilight and events | Moderate because Rockbox needs math/libc adapters | MIT, no runtime data files | **Planet/Moon/event engine** |
| Sky Map/Stardroid | Mature feature ideas and Android interaction model | Very high: Java, Android framework, sensors and mobile graphics | Current combined app is GPLv3; current branded visual assets are explicitly all-rights-reserved | Reject code and assets |
| Stellarium | Excellent desktop reference and broad sky-culture data | Prohibitive: C++, Qt and OpenGL | GPL-2.0; individual sky-culture resources have their own provenance | Reference only; do not port runtime or copy assets |
| Celestia | Rich 3D solar-system visualization | Prohibitive for this display and CPU | Large renderer and asset ecosystem | Reject |

Astroterm is the closest match to Rockbox because the useful code is mostly C,
its renderer is already abstract in spirit, and its default magnitude-5 view is
appropriate for a 320x240 display. Its current planetary and lunar model should
not ship as the authoritative model: Astroterm's own source labels Moon age as
crude and its Moon rendering has a correctness FIXME. Astronomy Engine is a
better reusable component for the small number of moving Solar System bodies.
It targets one-arcminute accuracy, has no external runtime dependency, and is
tested against NOVAS and JPL data.

### Exact reuse boundary

Retain from Astroterm, with a documented vendor patch set:

- `src/astro.c`: star proper motion, time and sidereal helpers needed by stars;
- `src/coord.c`: equatorial/horizontal and stereographic projection helpers;
- `src/core_position.c`: star positioning logic, adapted to compact records;
- `src/parse_BSC5.c` and the upstream conversion scripts as host-side import
  references;
- `data/bsc5_names.txt` and `data/bsc5_constellations.txt`, only after the data
  provenance gate passes;
- `data/cities.csv`, with GeoNames attribution;
- relevant Astroterm unit-test vectors.

Do not retain:

- `main.c`, `term.c`, `drawing.c`, or `core_render.c` as runtime code;
- curses, argtable2/3, terminal resizing, Unicode celestial glyphs, emoji, or
  terminal screenshots;
- Astroterm's approximate planet and Moon orbital-element path;
- a 24 or 64 frames-per-second animation loop.

Retain from Astronomy Engine:

- the upstream C public interface and only the implementation required by the
  selected calls;
- Sun, Moon, Mercury, Venus, Mars, Jupiter, Saturn, Uranus, Neptune, and Pluto
  apparent positions;
- illumination/magnitude, Moon phase, rise/set, and twilight routines;
- upstream validation vectors for every function used by Pocket Sky.

If the linker cannot eliminate unused Astronomy Engine functionality, create a
reproducible host-side extraction patch that excludes unused sections. Do not
rewrite the retained equations. Keep the full upstream file in the vendor
source/provenance package so updating and diffing remain mechanical.

## Target Envelope

The first supported devices are:

| Target | Display | Plugin buffer | Practical CPU constraint |
| --- | --- | --- | --- |
| iPod Video 5G/5.5G | 320x240, RGB565 | 3 MiB | Treat 80 MHz as the performance floor |
| iPod Classic 6G | 320x240, RGB565 | 3 MiB | Up to 216 MHz; not permission for a busy idle loop |

Both configurations expose 64 MiB system RAM, but the plugin's real design
limit is its 3 MiB plugin buffer. The `.rock` image, static data, stack, heap,
catalogue caches, and frame-related scratch space must fit within the memory
actually available to the loaded plugin.

Hard budgets:

- complete loaded plugin plus working data: **less than 2.5 MiB**;
- leave at least **384 KiB measured headroom** on both targets;
- on-device resource package: **less than 768 KiB** for the full default data;
- no second full-screen framebuffer unless measurement proves it necessary;
- normal interactive redraw: **10 fps minimum** on 5G while panning;
- idle live view: no more than one celestial recomputation per minute and one
  lightweight status redraw per second;
- enter-to-first-sky target: **under 2 seconds on 5G** with a warm filesystem;
- never require the shared audio buffer.

These are gates, not estimates to relax silently. If Astronomy Engine plus the
full BSC5 catalogue misses the loaded-size gate, reduce features through the
fallbacks defined below before increasing the budget.

## Product Scope

### V1 screens

1. **Sky**
   - offline sky for the configured place and UTC instant;
   - horizon, zenith, cardinal directions, stars, constellation figures and
     labels, Sun, Moon, and planets;
   - manual sight direction with a fixed center reticle;
   - live and time-machine modes;
   - normal and red night palettes.
2. **Identify**
   - nearest visible object to the reticle;
   - object name/type, altitude, azimuth, apparent magnitude, constellation,
     and rise/set summary where applicable;
   - star details come from imported catalogue fields, never invented copy.
3. **Search**
   - named stars, planets, Sun, Moon, and all 88 IAU constellations;
   - case-insensitive prefix search first, then substring fallback;
   - selecting a result returns to Sky and centers the view on it.
4. **Tonight**
   - sunset, civil/nautical/astronomical twilight, Moon phase and illumination;
   - rise/set times for Moon and visible planets;
   - calculations come directly from Astronomy Engine.
5. **Location**
   - manual latitude/longitude/elevation;
   - searchable upstream GeoNames city subset;
   - explicit UTC offset because the iPod has no location or timezone service.
6. **Time Machine**
   - live now, paused instant, and adjustable minute/hour/day/month/year steps;
   - one action to return to live time;
   - all displays clearly mark simulated time.
7. **Display Settings**
   - star magnitude cutoff;
   - star labels, constellation figures/names, azimuth grid and horizon;
   - night mode;
   - screen timeout override while actively exploring.
8. **About / Data Credits**
   - exact upstream versions, catalogue hashes, licences and attributions;
   - no network link is required to use the application.

### Explicitly out of scope for V1

- GPS, compass, accelerometer, camera or augmented-reality pointing;
- online weather, cloud cover, catalogue updates or image downloads;
- telescope control;
- satellites, comets, asteroids and user orbital elements;
- deep-sky photographs, Milky Way textures, landscapes or constellation art;
- a simulated 3D Solar System;
- sound effects, music or spoken descriptions;
- local civil-time/DST rule databases;
- arbitrary user plug-ins or scripting.

The iPods have no useful orientation sensors. Pocket Sky is a manually steered
planisphere, not a point-the-iPod-at-the-sky AR application.

## 320x240 Sky Design

### Sky viewport

Use a perspective-style stereographic sky view adapted from Astroterm's
projection, with a fixed center reticle. The user controls center azimuth and
altitude. Do not create a raster sky background.

Layout:

- 18-pixel top strip: place, LIVE/SIM state, UTC time and night-mode status;
- 204-pixel sky viewport;
- 18-pixel bottom strip: center altitude/azimuth and current control hint;
- overlays may temporarily use the bottom 44 pixels for object identity or
  time-step controls.

The renderer draws, in order:

1. solid sky background;
2. below-horizon mask and horizon line;
3. optional azimuth/altitude grid;
4. constellation segments clipped to the viewport;
5. stars from faint to bright;
6. Sun, Moon and planets;
7. non-overlapping labels;
8. center reticle and UI strips.

Render stars as filled Rockbox circles/points sized from the upstream apparent
magnitude. Render planets as small filled discs with labels. Render the Moon
phase as a lit disc from Astronomy Engine's phase angle using clipped LCD
primitives. These are data-driven renderer outputs, not authored artwork.

V1 uses white/off-white stars. Spectral coloring may be added only if it is
driven by a retained catalogue field and an upstream, cited color mapping. Do
not invent a decorative color table.

### Palettes

- **Normal:** near-black/navy background, subdued blue-gray grid, white stars,
  muted constellation lines, distinct but restrained Solar System bodies.
- **Night:** black background with red-only foreground intensities; avoid blue,
  green and white pixels throughout the viewport and overlays.

Use constants derived from existing Rockbox theme/display conventions. No
bitmap skin is required.

### Label policy

At 320x240, labels are a scarce resource:

- always consider the selected object first;
- then planets, Sun and Moon;
- then named stars above the configured label magnitude;
- then constellation names;
- reject a label whose bounding box overlaps an accepted label or UI strip;
- cap ordinary sky labels at 12;
- never recompute label placement during an unchanged idle frame.

Use the Rockbox system font. Do not bundle a custom font unless a later
language requirement proves the system font insufficient and a clearly
licensed upstream font is selected.

## Click-Wheel Interaction

### Sky mode

- wheel clockwise/counterclockwise: change center azimuth; repeat speed
  accelerates from 2 degrees to 15 degrees per detent;
- `RIGHT` / `LEFT`: raise/lower center altitude in 5-degree steps;
- `SELECT`: identify the nearest object within the reticle radius and open its
  object card; if nothing qualifies, show center coordinates briefly;
- `PLAY`: toggle live versus paused time;
- long `PLAY`: open Time Machine controls;
- `MENU`: open the Pocket Sky menu;
- long `MENU`: exit cleanly to Rockbox;
- `SELECT` + wheel: change field of view through 120, 90, 60, 40 and 25 degrees.

The default field of view is 90 degrees. Center altitude is clamped from -10 to
90 degrees so objects just below the horizon can still be inspected.

### Time Machine mode

- wheel: move simulated time backward/forward;
- `LEFT` / `RIGHT`: select minute, hour, day, month or year step;
- `SELECT`: accept the simulated instant and return to Sky;
- `PLAY`: return immediately to live time;
- `MENU`: cancel and restore the prior instant.

### Lists and cards

- wheel: move selection;
- `SELECT`: open/activate;
- `LEFT` or `MENU`: back;
- `RIGHT`: secondary/open action where shown;
- long `MENU`: exit only from the top-level Pocket Sky menu.

All bindings must go through a small target keymap layer. Do not scatter raw
button constants through astronomy, rendering, or screen code. USB events must
use the normal Rockbox exit/USB handling path.

## Data Package

### Source data

The intended default package contains:

- all 9,110 Yale Bright Star Catalog 5 records;
- 333 Astroterm/IAU named-star records;
- Astroterm's 88 converted Western constellation figures;
- all city records in Astroterm's filtered GeoNames file plus the pinned
  GeoNames Moncton record, currently 2,961 entries;
- no images.

Astroterm expects a 291,548-byte BSC5 binary. Its build script documents and
checks the canonical binary and ASCII source forms. The Pocket Sky importer
must accept the same canonical inputs, verify an allow-listed SHA-256, and
generate the Rockbox resource. It must never scrape catalogue values from web
pages or require hand correction.

### Resource layout

Install immutable resources under:

```text
/.rockbox/apps/pocketsky/catalog.psc
/.rockbox/apps/pocketsky/constellations.psc
/.rockbox/apps/pocketsky/names.psc
/.rockbox/apps/pocketsky/cities.psc
/.rockbox/apps/pocketsky/PROVENANCE.txt
```

Store user state beside the plugin, following Rockbox's native plugin config
convention:

```text
/.rockbox/rocks/apps/pocketsky.cfg
```

Every `.psc` starts with:

```text
magic[8] = "PSKYRSC1"
kind
format_version
record_count
payload_bytes
source_sha256
payload_crc32
```

All multi-byte fields have a specified byte order. The importer writes them;
the plugin only validates and reads them. A resource version mismatch or bad
CRC produces a useful error and exits without reading past the buffer.

### Compact star records

Do not keep Astroterm's pointer-heavy desktop `struct Star` layout on-device.
The host importer emits a packed record containing only:

- BSC/HR catalogue number;
- J2000 right ascension and declination;
- RA and declination proper motion;
- apparent magnitude;
- optional name-table offset;
- constellation/search metadata needed by the UI.

The quantization scale is chosen by an automated comparison against the source
record. Maximum introduced position error must remain below 0.5 arcminute over
the supported date range, and magnitude error below 0.01. This is serialization
engineering, not a replacement astronomy model.

Sort the runtime projection index by brightness during import. At magnitude 5,
the device processes only the prefix that can be visible. The complete
catalogue remains available for search and higher thresholds without a runtime
sort or duplicate table.

### Provenance and licence gate

The Astroterm MIT licence covers Astroterm code, not automatically every input
dataset named by its README. Before committing or distributing generated data,
create `PROVENANCE.txt` with, for each input:

- upstream title and canonical URL;
- exact upstream revision or download date;
- SHA-256 of the unmodified source;
- transformation script and version;
- applicable licence/terms and required attribution;
- SHA-256 of the generated resource.

Known items requiring explicit treatment:

- Astroterm code: MIT;
- Astronomy Engine code: MIT;
- city subset: GeoNames attribution licence (the current GeoNames site states
  Creative Commons Attribution 3.0);
- constellation lines: derived from a pinned Stellarium modern-sky-culture file
  and mapped through HYG; verify both source licences and preserve attribution;
- IAU star names and BSC5: verify redistribution terms instead of assuming that
  public access means public domain.

If any default dataset cannot be redistributed confidently, do not replace it
with manually copied data. Ship a host-side personal-use importer for that
input, or select a clearly licensed upstream catalogue in a separate decision.

## Proposed Source Layout

```text
apps/plugins/pocketsky.c
apps/plugins/pocketsky/
    app.c
    app.h
    cache.c
    cache.h
    catalog.c
    catalog.h
    keymap.c
    keymap.h
    render.c
    render.h
    screens.c
    screens.h
    settings.c
    settings.h
    rb_compat.c
    rb_compat.h
    upstream/
        astroterm/
        astronomy_engine/
        UPSTREAM.md
tools/pocketsky_import_data.py
tools/pocketsky_reference_gate.py
```

Keep upstream files recognizable and changes reviewable. `UPSTREAM.md` records
the original repository URL, release/commit, included files, omitted files and
every local patch. Prefer compatibility headers and narrow wrapper functions to
editing scientific code.

Add `pocketsky.rock` to the Apps category for color 320x240 targets. Initial
build guards may restrict it to `IPOD_4G_PAD` and the two target configurations
until a generic keymap and small-display layout exist.

The customized root menu in this tree also uses a curated Applications list,
so Pocket Sky has a dedicated launcher under **Extras -> Applications** in
addition to its standard `pocketsky,apps` category registration.

## Implemented Port

The V1 implementation lives in `apps/plugins/pocketsky/`. It includes the live
sky, click-wheel steering/FOV, reticle identification and object cards,
star/body/constellation search, automatic Moncton setup, location override from
2,961 cities or manual
coordinates, saved UTC offset, Time Machine, Tonight rise/set and three
twilight levels, label collision, horizon/grid/constellation layers, normal and
red-night palettes, bounded CRC-checked resources, USB exit handling, and
on-device attribution.

`tools/pocketsky_import_data.py` performs the deterministic conversion and
`tools/pocketsky_reference_gate.py` validates counts, ordering, CRCs, string
bounds, coordinates, and every HR reference. The convenience installer
`tools/pocketsky_install_resources.sh` fetches only the pinned Astroterm source;
the user must supply the canonical unmodified BSC5 binary. No generated
catalogue or celestial artwork is committed.

`tools/pocketsky_science_gate.py` builds the same pinned scientific sources
twice, once against host `libm` and once against Pocket Sky's selected musl
path, then compares 1,296 outputs across 24 fixed UTC/location cases including
both hemispheres, polar latitudes, the date line, leap days, DST boundaries and
the supported date-range endpoints.

## Runtime Architecture

```text
Rockbox clock/settings/buttons
             |
             v
      Pocket Sky controller
       /        |         \
      v         v          v
Astroterm   Astronomy    resource/search
star core    Engine C       indexes
       \        |          /
        v       v         v
       cached horizontal/projected scene
                     |
                     v
             Rockbox LCD renderer
```

### Compatibility layer

`rb_compat` owns all substitutions for missing hosted-C facilities:

- Rockbox time to Astronomy Engine UTC;
- `malloc`/`calloc`/`realloc`/`free` backed by one TLSF arena obtained from
  `plugin_get_buffer()`;
- `sin`, `cos`, `atan`, `atan2`, `acos`, `sqrt`, `floor`, `ceil`, `pow`, and
  formatting wrappers;
- diagnostic output to Rockbox logging or bounded on-screen errors;
- removal or replacement of hosted `FILE`, `stderr`, and current-time calls.

Start from the proven compatibility approach in
`apps/plugins/puzzles/rbcompat.h` and `rbwrappers.c`, including Rockbox's
fixed-point sine/cosine implementation. Every wrapper used by astronomy code
gets a host test for domain, sign, quadrant, NaN/error behavior and the accuracy
needed by the reference gate.

The first prototype may use software floating point. Do not convert upstream
astronomy to fixed point speculatively. Measure the 5G gate first. If it fails,
optimize in this order:

1. dirty-state caching and magnitude culling;
2. precomputed time/location terms shared by all stars;
3. replace only compatibility-layer math functions with measured equivalents;
4. quantize cached/render coordinates;
5. as a final fallback, generate minute-bucket lookup aids on the host.

Do not rewrite Astronomy Engine equations into bespoke fixed point.

### Scene cache

Maintain separate dirty flags for:

- UTC instant;
- observer location;
- centre/FOV projection;
- visibility toggles and magnitude threshold;
- labels/UI only.

On a time or location change:

1. compute sidereal/shared terms once;
2. update stars up to the active magnitude prefix using Astroterm logic;
3. update the small Solar System set with Astronomy Engine;
4. cache horizontal coordinates;
5. project only records potentially inside the field of view;
6. run label placement once.

On a view-only change, reuse horizontal coordinates and only reproject. On a
UI-only change, redraw cached scene data. In live mode, star and planet
positions refresh once per minute; the displayed clock may refresh once per
second without recalculating the sky.

Boost the CPU only around a measured catalogue or event recomputation. Always
release boost before waiting for input and on every exit/error path.

### Memory strategy

- call `plugin_get_buffer()` once and initialize one bounded TLSF arena;
- load/validate resources sequentially so raw input and parsed output do not
  coexist unnecessarily;
- use compact arrays and offsets, not per-record heap strings;
- keep one shared label/text scratch buffer;
- avoid per-frame allocation entirely;
- expose peak arena use in a debug build and assert the headroom gate.

Pocket Sky must not call `plugin_get_audio_buffer()`, stop playback, alter the
playlist, change mixer state, or take over PCM. Existing music should continue
while the user explores the sky. If audio is ever proposed later, it is a new
scope that requires `docs/plugin-audio-lifecycle-steering.md` before code work.

## Supported Dates and Time Semantics

- Internal calculations use UTC only.
- Initial supported UI range: 1900-01-01 through 2099-12-31.
- The configured UTC offset is display metadata and local-time conversion; it
  does not alter the observer longitude.
- DST is a manual offset change in V1.
- Invalid/unset RTC opens the location/time setup screen instead of silently
  showing a plausible but wrong sky.
- Simulated time never changes the Rockbox RTC.

Astronomy Engine supports much wider epochs, but the smaller range avoids
pretending the UI, RTC and imported proper-motion data are validated for
millennia. A later expansion requires reference tests at the new boundaries.

## Delivery Milestones

### M0: Licensing and import proof

- pin Astroterm v1.2.0 and the Astronomy Engine commit;
- produce the complete input/output hash manifest;
- resolve BSC5, IAU-name, HYG/Stellarium and GeoNames redistribution terms;
- generate all `.psc` resources without manual edits;
- verify record counts, references and CRCs.

**Stop gate:** no device data package is committed or distributed until its
provenance record is complete.

### M1: Hosted scientific reference harness

- build unmodified upstream Astroterm and Astronomy Engine paths on the host;
- define fixed cases covering both hemispheres, equator, polar regions,
  horizon crossing, leap day and date-range boundaries;
- save expected numerical outputs, not screenshots alone;
- prove compact resource quantization stays within its error budget.

### M2: Rockbox compile and static sky

- add build integration and compatibility layer;
- validate resource loading and bounded allocation;
- render a fixed UTC/location sky in the iPod 6G simulator;
- compare selected object coordinates and a deterministic screenshot against
  the host harness.

### M3: 5G performance gate

- build and run the same scene for iPod Video;
- measure cold load, recomputation, view-only reprojection, redraw and peak
  memory;
- implement cache/cull optimizations until the target envelope passes;
- verify no CPU boost remains active while idle.

**Stop gate:** do not add Tonight/search polish to an engine that misses the 5G
memory or interaction gate.

### M4: Complete sky interaction

- implement manual steering, FOV, identify, label collision and display
  settings;
- add normal/night palettes;
- test button repeat, long presses, USB handling and every exit path;
- verify current audio playback continues unchanged.

### M5: Search, object cards and location

- add offset-based star/name/constellation indexes;
- add city search and manual coordinate entry;
- add saved settings with safe defaults and corruption recovery;
- add full on-device credits/provenance display.

### M6: Time Machine and Tonight

- add simulation controls without changing the RTC;
- add Astronomy Engine phase, illumination, rise/set and twilight output;
- cache event calculations separately from sky frames;
- handle never-rises, circumpolar, polar day and polar night results explicitly.

### M7: Hardware acceptance

- test real 5G/5.5G and 6G devices;
- verify display legibility outdoors and red night mode in darkness;
- measure idle and active battery/CPU behavior;
- run 30-minute navigation/time-machine soak tests;
- verify resume/exit, USB insertion and malformed-resource behavior;
- package resources without touching the Rockbox database files.

No physical deployment is part of this specification task. Any later full 6G
package deployment must use `tools/deploy_ipod6g_preserve_database.sh` as
required by repository policy.

## Validation Matrix

### Scientific correctness

For at least 24 fixed UTC/location cases:

- compare star altitude/azimuth with unmodified Astroterm output;
- compare Sun/Moon/planet altitude/azimuth, magnitude, phase and rise/set with
  the pinned Astronomy Engine host build;
- target Solar System agreement: 2 arcminutes or better versus upstream output;
- target star agreement: 1 arcminute or better versus the imported/upstream
  record path;
- verify azimuth wrap, below-horizon clipping and north/south projection;
- include latitude 0, +/-45, +/-80 and both sides of the date line.

Do not use visual resemblance to a screenshot as the scientific accuracy test.
Screenshots test projection and UI; numerical vectors test astronomy.

### Resource correctness

- exactly 9,110 BSC5 records accepted;
- every name and constellation segment references a valid catalogue number;
- every string offset and record extent is bounds checked;
- bad magic/version/CRC/truncation fails cleanly;
- deterministic imports produce byte-identical resources;
- displayed About hashes match installed resource hashes.

### Device behavior

- peak loaded footprint and arena headroom pass on both targets;
- no allocation occurs in the steady-state frame path;
- 5G interactive steering remains at or above 10 fps;
- idle live view does not busy-loop or hold boost;
- backlight policy is restored on exit;
- playback, volume and playlist state are unchanged;
- every menu and error path restores LCD/font/viewport state;
- USB connection exits through the normal Rockbox path.

## Fallbacks if a Gate Fails

Apply these in order and record the decision:

1. Keep the full source catalogue on disk but default to a magnitude-5 runtime
   prefix.
2. Lazy-load the city list only while the Location screen is open.
3. Compile out Astronomy Engine event searches on 5G while retaining accurate
   current body positions; Tonight becomes a 6G-only optional feature.
4. Split the application into `pocketsky.rock` and an optional
   `pocketsky_tonight.rock` sharing the resource package.
5. If redistribution blocks a dataset, require the deterministic host importer
   for that dataset.

Do not respond to a failed gate by dropping attribution, copying protected
assets, reducing numerical tests, taking the audio buffer, or replacing the
upstream scientific core with unreviewed custom approximations.

## Definition of Done

Pocket Sky V1 is complete when:

- it builds reproducibly for the iPod Video and iPod 6G targets and simulator;
- all shipped astronomy/data comes from pinned, attributed upstream sources;
- no celestial bitmap or constellation figure was manually authored;
- a user can configure location, explore and identify the live sky, search an
  object, time-travel, use night mode, and see Tonight information offline;
- numerical results pass the pinned-upstream reference suite;
- memory, performance, idle-power and audio-coexistence gates pass on real 5G
  and 6G hardware;
- malformed data and invalid RTC/location state fail safely;
- licences, source hashes and generated-resource hashes are visible in the
  installed app and preserved in the repository.

## Upstream References

- Astroterm: <https://github.com/da-luce/astroterm>
- Astronomy Engine: <https://github.com/cosinekitty/astronomy>
- Sky Map/Stardroid candidate audit: <https://github.com/sky-map-team/stardroid>
- Stellarium candidate audit: <https://github.com/Stellarium/stellarium>
- GeoNames attribution/source information: <https://www.geonames.org/about.html>

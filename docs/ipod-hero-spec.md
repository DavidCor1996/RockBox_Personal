# iPod Hero Rockbox Game Specification

## Document Status

- Status: implementation specification
- Date: 2026-07-22
- Working title: `iPod Hero`
- Initial targets: iPod Classic 6G/7G (`ipod6g`) and iPod Video
  5G/5.5G (`ipodvideo`)
- Display: 320x240, 16-bit color
- Runtime: native Rockbox game plugin written in C99
- Music source: charted tracks indexed by the installed iPod Hero package
- Art source: an external, user-installed Guitar Hero skin pack

Normative terms such as **must**, **must not**, **should**, and **may** describe
implementation requirements.

## Product Decision

Build a five-lane rhythm game which plays against music already stored on and
decoded by Rockbox. The game must use authored or host-generated note charts;
it must not invent evenly spaced notes at runtime or treat raw volume peaks as
a complete chart.

The game loop is:

1. Launch iPod Hero from Games, a shortcut, or Game Cover Flow.
2. Browse the installed songs from `index.tsv`; entries whose audio or charts
   are missing are not shown.
3. Choose a song and difficulty.
4. Press Play. If the song is not already current, iPod Hero creates a
   one-track Rockbox playback session for the explicitly selected file, then
   seeks it to the start.
5. Play using the five click-wheel buttons while Rockbox remains the sole
   owner and decoder of the song audio.
6. View results, retry, or return to the in-game song list.

## Non-Negotiable Asset Boundary

The open-source tree must not contain Guitar Hero logos, note gems, highway
art, fonts, HUD art, menu panels, hit effects, characters, backgrounds, sound
effects, charts, or other copyrighted game media.

The implementation must instead provide a local import pipeline:

```text
user-supplied files -> profile-specific extractor -> validated iPod Hero skin
```

The importer may accept assets the user has extracted from a game they own or
otherwise has permission to use. It may also accept local files the user found
online, but it must not download them, recommend an unauthorized archive, or
assert that personal use makes an unauthorized source lawful.

The runtime must have no hand-drawn or procedurally approximated gameplay-art
fallback. If a required skin item is absent or invalid, the plugin must show a
plain Rockbox-font diagnostic and refuse to start gameplay. Drawing primitives
may be used for clipping, clearing, debug overlays, and selection bounds, but
not to replace missing Guitar Hero artwork.

The repository may contain:

- importer and conversion code;
- asset profile manifests containing names, dimensions, crop coordinates, and
  hashes, but no pixels from the source games;
- synthetic test fixtures which are visually unrelated to Guitar Hero;
- a text-only missing-assets/error screen;
- documentation telling the user where to place their own files.

The user-installed asset pack must remain ignored by Git. Every imported pack
must contain `provenance.txt` recording the source selected by the user and a
generated manifest of output hashes.

`iPod Hero` is a working name for a personal build. A distributed build should
use a non-infringing name and must not imply endorsement by Apple, Harmonix,
RedOctane, Activision, or any other rights holder.

## Goals

1. Play real songs already present in the Rockbox library.
2. Keep note timing locked to Rockbox playback through pauses, seeks, and
   decoder timing updates.
3. Preserve authored chords, sustains, tempo changes, star phrases, sections,
   and difficulty levels where the source chart provides them.
4. Provide a useful desktop chart generator for songs without an authored
   chart, while labeling generated charts honestly.
5. Present authentic user-supplied Guitar Hero graphics rather than recreated
   programmer art.
6. Hold 30 frames per second on iPod 6G and remain playable on iPod Video 5G.
7. Leave the selected Rockbox playback session and audio codec in a clean
   state after exit.
8. Require no network access on the iPod.

## Non-Goals

V1 will not:

- download music, charts, or copyrighted art;
- separate stems, detect beats, or transcribe pitch on the iPod;
- decode a second private copy of the selected song;
- mutate a playlist merely for menu/background music; the only playlist
  change is the user's explicit Play action for a selected charted song;
- use `plugin_get_audio_buffer()`;
- provide isolated guitar/vocal stems or a missed-note audio mute effect;
- promise that an automatically generated chart equals a human-authored one;
- emulate a console Guitar Hero executable;
- support a guitar controller, USB host peripherals, or dock accessories;
- reproduce 3D venues, video backgrounds, or character animation;
- require changes to iPodJS playback memory or its framebuffer lifecycle.

## Target Envelope

| Target | Screen | Plugin buffer | Performance target |
| --- | --- | ---: | --- |
| iPod Classic 6G/7G | 320x240 RGB565 | 3 MiB | stable 30 fps |
| iPod Video 5G/5.5G | 320x240 RGB565 | 3 MiB | stable 20 fps, 30 fps stretch |

The loaded `.rock`, static data, decoded skin, chart, game state, and scratch
space all share the fixed plugin buffer. The plugin must use
`rb->plugin_get_buffer()` only. It must never request the shared audio buffer.

Hard runtime budgets:

- loaded plugin plus all working data: less than 2.5 MiB;
- measured free headroom after gameplay load: at least 384 KiB;
- decoded active skin data: at most 1,024 KiB;
- active difficulty chart: at most 256 KiB;
- gameplay stack: no large sprite, chart, or framebuffer arrays;
- no private second full-screen framebuffer;
- no file I/O or bitmap decode in the gameplay draw path;
- initial gameplay load: under 2 seconds on iPod 5G with warm storage;
- input-to-judgement processing: within one Rockbox tick;
- no busy-waiting; every frame loop must sleep or block to its deadline.

If a selected skin exceeds the budget, the host importer must reduce it by
packing/cropping and pre-scaling. The device must not steal playback memory to
make the skin fit.

## User Experience

### First launch

If no valid skin is installed, display:

```text
iPod Hero assets are not installed.
Run tools/ipodhero_prepare_assets.py on a computer,
then copy the generated ipodhero directory to the iPod.
```

No imitation notes, highway, logo, or placeholder art may appear.

### Song library and ready screen

On startup, read `index.tsv` once outside all draw callbacks and show every
entry whose audio file and at least one compiled chart are readable. The wheel
moves through the list, Select opens the selected song, and Menu exits. The
song's ready screen shows these items using its installed skin:

- title and artist from the validated index entry;
- chart origin: `AUTHORED`, `IMPORTED`, or `GENERATED`;
- available difficulties;
- high score and best percentage for the selected difficulty;
- calibration offset;
- `Play`, `Practice`, `Calibration`, and `Songs`.

Menus use the stock iPod visual language: white list rows, a blue selected row,
right-edge disclosure chevrons, a centered title bar, and persistent `MENU`
and `CENTER` action labels. Raw menu input is passed to Rockbox's standard
key-click handler so software click, piezo click, and haptic feedback continue
to follow the device's global settings.

If no playable entries remain after validation, explain that the installed
library has no readable audio/chart pairs. Returning from the ready screen
must return to the song list without rescanning it.

### Song start

Before gameplay the plugin must:

1. load and validate the complete chart and gameplay skin;
2. take a stable copy of the selected indexed path and identity;
3. if that path is not current, create a one-track Rockbox playlist and start
   the selected file through `playlist_start()`;
4. validate the decoder-reported path, length, and byte size against the index;
5. pause the selected song if needed;
6. call `audio_pre_ff_rewind()` followed by `audio_ff_rewind(0)`;
7. wait for the audio clock to acknowledge the seek;
8. reset score and timing state, resume through the normal playback API, and
   begin the chart from its signed per-song audio offset.

No loading or decode work may begin after the song resumes.

### Gameplay layout

The 320x240 gameplay screen consists of:

- a perspective highway centered in the screen;
- five colored lanes and five receptors supplied by the skin;
- descending note heads, sustain bodies/tails, HOPO/tap variants, and star
  phrase variants supplied by the skin;
- score, multiplier, streak, rock meter, and star meter supplied by the skin;
- hit flames and miss feedback from supplied animation frames;
- optional song/section text using either the skin's bitmap font or an existing
  Rockbox UI font.

V1 loads two attributed live-performance photographs per skin. Gameplay makes
timed camera cuts between those cached images using a segmented wipe derived
from elapsed song time. The draw path only blits decoded pixels: it performs no
file access, decode, allocation, scaling, or full-screen framebuffer copy.
This is not a video background or a 3D venue.

### Pause and exit

Moving the physical Hold switch to Hold pauses gameplay and calls
`audio_pause()`. Releasing Hold opens the skinned pause menu instead of
resuming immediately. This prevents the five gameplay buttons from losing a
lane to pause.

The simulator and targets without a readable Hold transition may use
`Select+Menu` as the pause chord. That chord is consumed and never judged as
notes.

The pause menu provides `Resume`, `Restart`, `Calibration`, `No-Fail`, and
`Exit`. `Restart` seeks the existing current track; `Exit` leaves the selected
one-track playback session intact.

If Rockbox advances to another track, the game must stop judging immediately,
pause playback, and report `Track changed`. It must never continue a chart
against the wrong audio.

## Controls

### Tap-5 mode (required and default)

Tap-5 adapts the five physical click-wheel buttons into five fret lanes and
does not require a separate strum:

| Lane | Color | iPod input |
| ---: | --- | --- |
| 0 | Green | Previous / Left |
| 1 | Red | Menu / Up |
| 2 | Yellow | Select / Center |
| 3 | Blue | Play/Pause / Down |
| 4 | Orange | Next / Right |

Gameplay permanently labels all five receptors `LEFT`, `MENU`, `SELECT`,
`PLAY`, and `RIGHT` in bordered, color-matched cells. The labels use the
built-in fixed font and do not require a runtime-loaded or hand-drawn font.

Each press is a pick event for its lane. A chord is collected as a lane mask:
all required buttons must arrive within the chord judgement window, but they
do not have to be mechanically simultaneous. This rolled-chord rule preserves
multi-note events on hardware where the click wheel cannot reliably report
multiple down states at once.

During a sustain, the player keeps its lane button held. Wheel motion applies
whammy to a held sustain. A fast clockwise wheel sweep activates star power
when the meter is ready. Star activation must require enough accumulated wheel
delta that ordinary navigation noise cannot trigger it.

Button release and repeat events must be normalized before judgement. A repeat
must never create a second note hit.

### Wheel-5 accessibility mode (optional after V1)

Wheel movement selects one of five lanes and Select hits the selected lane.
This mode may flatten chords during chart load and must mark the score as
`ASSIST`. It is not the correctness baseline.

### Input qualification gate

Before locking the control scheme, a hardware diagnostic must record raw press,
release, repeat, wheel direction, and Hold events for:

- every individual button;
- every two-button combination;
- a five-note roll in both directions;
- a held lane plus wheel movement;
- Hold engaged and released during active playback.

The event trace becomes a regression fixture for the input normalizer.

## Chart Sources

### Accuracy order

1. A human-authored chart matched to the exact audio master.
2. Imported `.chart` or rhythm-game MIDI data, manually offset-verified.
3. A desktop-generated chart reviewed in the companion editor.
4. An unreviewed desktop-generated chart, visibly labeled `AUTO`.

An audio analyzer cannot guarantee authored Guitar Hero quality for an
arbitrary mastered song. The UI and scores must never hide this distinction.

### Authored chart import

The host tool must import at least:

- text `.chart` files with tempo and time-signature changes;
- standard MIDI rhythm-game guitar tracks;
- difficulty parts for Easy, Medium, Hard, and Expert;
- five-lane notes and simultaneous chord masks;
- sustains;
- forced HOPO/tap flags where represented;
- star-power phrases;
- section markers and end events;
- signed audio offset.

Tick-to-millisecond conversion must integrate every tempo segment with 64-bit
integer arithmetic. Notes at the same source tick become one chord event.

The importer must reject a chart if its declared audio does not match the
selected iPod track by duration and user-confirmed identity. Filename equality
alone is insufficient when multiple masters of a song exist.

### Automatic chart generation

Automatic generation runs only on a host computer. The planned pipeline is:

1. Decode the exact iPod audio file to normalized PCM.
2. Detect tempo, beat grid, downbeats, and structural sections.
3. Optionally separate a guitar stem before transcription.
4. Transcribe pitched onsets and durations to MIDI-like events.
5. Quantize only events with adequate timing confidence.
6. Map pitch contour and phrase motion into five playable lanes.
7. Merge near-simultaneous notes into chords where the difficulty permits.
8. Generate Expert first, then derive lower difficulties using density,
   movement, chord, and sustain constraints.
9. Present an editable preview and confidence warnings.
10. Compile the reviewed result to `.ihc`.

An initial implementation may integrate Spotify's Basic Pitch for audio-to-MIDI
transcription. An optional stem adapter may use a pinned Demucs-compatible
model, but the analyzer interface must not make that unmaintained upstream a
permanent runtime dependency. Both are host tools, never Rockbox libraries.

Generated difficulties must obey these defaults:

| Difficulty | Maximum average density | Chords | Fastest repeated subdivision |
| --- | ---: | --- | --- |
| Easy | 2.0 notes/s | none | eighth note |
| Medium | 3.5 notes/s | two-note, rare | eighth note |
| Hard | 5.5 notes/s | two-note | sixteenth note |
| Expert | source-limited | up to three-note | source-limited |

The generator must favor musically meaningful onsets over filling every grid
position. Low-confidence regions may be sparse; fabricated rhythm is worse
than silence.

### Definition of a proper chart

A chart is considered timing-correct when:

- events are tied to measured or authored musical onsets, not random spacing;
- tempo changes and the selected audio master's lead-in are honored;
- the chart has an explicit signed audio offset;
- imported conversion differs from the host reference by no more than 1 ms;
- the device clock does not accumulate timing drift over the song;
- a reviewed hardware calibration run keeps authored transient hits within
  the selected judgement window;
- chords, sustains, HOPO/tap state, and star phrases retain source semantics.

Automatic musical quality is a separate, human-reviewed gate.

## Compiled Chart Format

The device format is little-endian `.ihc` (`IPOD HERO CHART`). It is designed
to load one difficulty completely before playback.

### Header

```c
struct ih_chart_header {
    char magic[4];              /* "IHC1" */
    uint16_t header_size;
    uint16_t flags;
    uint32_t event_count;
    uint32_t section_count;
    uint32_t song_length_ms;
    int32_t audio_offset_ms;
    uint32_t source_crc32;
    uint32_t source_size_low;
    uint8_t lane_count;         /* must be 5 */
    uint8_t difficulty;
    uint8_t origin;
    uint8_t reserved;
    uint32_t payload_crc32;
};
```

The final implementation must serialize fields explicitly; it must not write
the compiler's native struct layout.

### Note event

```c
struct ih_note_event {
    uint32_t time_ms;
    uint32_t duration_ms;
    uint8_t lane_mask;
    uint8_t flags;
    uint16_t phrase_id;
};
```

`lane_mask` uses bits 0 through 4. Flags cover HOPO, forced strum, tap, star
phrase, phrase end, and generated-low-confidence status. Events must be sorted
by `time_ms`; duplicate times are merged before serialization.

Section records use a millisecond timestamp plus an offset into a bounded UTF-8
string table. The parser must validate all counts, offsets, lane masks,
durations, CRCs, and integer additions before allocating or drawing anything.

### Song matching

`index.tsv` maps the normalized Rockbox path to chart files and stores:

- exact device path;
- audio byte size;
- audio length in milliseconds;
- CRC32 generated by the host from fixed beginning/end samples;
- title and artist for diagnostics only;
- chart paths and difficulties;
- asset skin ID used when authored as part of a set.

Runtime matching is path-first and performs no audio-file scan during
gameplay. Library construction checks file presence before rendering, and the
host tool handles moved-file relinking. A duration or identity mismatch blocks
playback until the package is corrected.

## Skin Package

### Device layout

```text
/.rockbox/rocks/games/ipodhero.rock
/.rockbox/rocks.data/ipodhero/
    config.cfg
    index.tsv
    scores.dat
    charts/
        <song-id>-easy.ihc
        <song-id>-medium.ihc
        <song-id>-hard.ihc
        <song-id>-expert.ihc
    skins/
        <skin-id>/
            skin.ihs
            provenance.txt
            background.bmp
            background-alt.bmp
            highway.bmp
            receptors.bmp
            gems.bmp
            sustains.bmp
            flames.bmp
            hud.bmp
            digits.bmp
            menus.bmp
```

The host workspace uses an ignored staging directory under
`assets/ipodhero/local/`. No imported output is copied into the source tree's
compiled bitmap library.

### Required visual items

The skin manifest must resolve all of these semantic roles:

- title/logo and menu panel states;
- two gameplay performance-photo backgrounds;
- perspective highway and lane/receptor states;
- normal, HOPO/tap, pressed, and star note heads for all five colors;
- sustain body, cap, and active states;
- hit flame animation and miss state;
- score digits, multiplier states, and streak digits;
- rock meter and pointer states;
- star meter, phrase, ready, and active states;
- results grades/stars and difficulty labels;
- pause, calibration, and error panels.

Transparent sprites use Rockbox's transparent bitmap blitter and the skin's
declared key color. The importer must crop transparent margins and convert all
runtime images to device-native BMP/RGB565. It must pre-scale note gems into a
small set of perspective size bands; the runtime must not resample art per
frame.

The runtime loads two screen backgrounds and only the atlases required for the
current state. Both backgrounds count against the fixed 1,024 KiB decoded-skin
budget. Menu and gameplay storage should overlap through a simple arena reset
when their lifetimes do not overlap.

### Host asset tool

The planned command is:

```text
tools/ipodhero_prepare_assets.py \
    --profile <supported-source-layout> \
    --source <user-owned-local-directory> \
    --output <mounted-ipod>/.rockbox/rocks.data/ipodhero/skins/<skin-id>
```

The tool must:

- never use network access;
- fail if a required source is missing;
- validate source dimensions before cropping;
- preserve aspect ratio unless a profile explicitly specifies a crop;
- generate all perspective sizes offline;
- enforce the 1,024 KiB decoded-skin budget;
- write the source/output hash manifest and provenance template;
- support `--verify` without modifying files;
- use an atomic staging-and-rename install.

## Runtime Architecture

The plugin should be split into these units:

```text
apps/plugins/ipodhero/
    ipodhero.c          plugin entry, state machine, cleanup
    ih_audio_clock.c    playback observation, interpolation, seek handling
    ih_chart.c          index lookup and bounded .ihc parser
    ih_input.c          button normalization, chords, Hold, wheel gestures
    ih_judge.c          hit windows, sustains, score, star power, rock meter
    ih_render.c         cached-pixel gameplay renderer
    ih_skin.c           manifest and BMP loading outside draw paths
    ih_scores.c         versioned settings and atomic score persistence
    ipodhero.h
    SOURCES
```

Host tooling should be separate:

```text
tools/ipodhero_chart.py
tools/ipodhero_prepare_assets.py
tools/ipodhero_sim_gate.py
tools/ipodhero/
    chart_import.py
    chart_generate.py
    chart_compile.py
    skin_profiles.py
```

No plugin API addition is expected for V1.

## Audio and Timing Lifecycle

### Playback ownership

Rockbox remains the sole decoder and playback owner. The plugin may call:

- `playlist_create()`, `playlist_insert_track()`, and `playlist_start()` only
  after the user presses Play for a selected indexed song;
- `audio_status()`;
- `audio_current_track()`;
- `audio_pause()` and `audio_resume()`;
- `audio_pre_ff_rewind()` and `audio_ff_rewind()`;
- ordinary volume adjustment APIs.

It must not call:

- `plugin_get_audio_buffer()` or `plugin_release_audio_buffer()`;
- `audio_stop()` merely to start or exit the game;
- `playlist_remove_all_tracks()` or background/implicit playlist mutation;
- direct PCM or mixer playback in V1;
- codec, sample-rate, source, or audio-hardware power controls.

This deliberately avoids the highest-risk audio teardown path. Guitar Hero
sound effects are deferred; V1 uses the song plus visual hit feedback.

### Smooth song clock

`audio_current_track()->elapsed` is authoritative but may update less often
than the display. The game clock must follow the proven lyrics-player pattern:

1. When `id3->elapsed` advances or jumps backward, save it with
   `*rb->current_tick` as a new synchronization point.
2. Between updates while playing, interpolate milliseconds from elapsed ticks.
3. Clamp interpolation to at most 200 ms ahead of the last reported audio
   position.
4. Freeze immediately when playback is paused.
5. Rebase after every seek, resume, track change, or discontinuity.
6. Apply `chart.audio_offset_ms + settings.calibration_ms` only after deriving
   the playback clock.

All gameplay and animation positions derive from this clock. Frame count must
never be used as song time.

### Calibration

Global calibration is adjustable from -300 ms to +300 ms in 5 ms increments.
Each chart may add a signed source offset. Practice mode may apply a temporary
offset without changing ranked scores.

The calibration screen shows a repeating visual target against a known chart
section and records the median of at least eight user taps. It must show the
proposed change before saving it. Automatic compensation is capped at 150 ms;
larger values require explicit confirmation because they usually indicate a
wrong audio master.

## Judgement and Scoring

Default windows are symmetric around the calibrated event time:

| Judgement | Absolute error |
| --- | ---: |
| Perfect | <= 45 ms |
| Great | <= 90 ms |
| Good | <= 135 ms |
| Miss | > 160 ms or wrong input |

The 136-160 ms region is a late/early grace hit worth no accuracy bonus. The
windows must be configurable for accessibility, but widened windows mark the
score as `ASSIST`.

Required rules:

- a chord is one event whose required lane mask is collected within the same
  160 ms outer window;
- extra lanes make the event a miss in ranked mode;
- a sustain starts only from a successfully hit head;
- releasing more than 80 ms early breaks the remaining sustain ticks;
- HOPO/tap notes may be hit without resetting other held lanes according to
  chart flags;
- combo multipliers are 1x, 2x at 10, 3x at 20, and 4x at 30 notes;
- active star power doubles the current multiplier;
- star power is earned only by completing an entire marked phrase;
- No-Fail continues at an empty rock meter and marks the result;
- pause time, dropped render frames, and storage stalls cannot advance the
  judgement clock independently of audio.

The score record contains chart payload CRC, audio identity, difficulty,
settings flags, score, hit counts, maximum streak, accuracy in basis points,
timestamp, and calibration offset. Records use a version and CRC and are
written atomically only from menus/results, never from the draw loop.

## Renderer

Gameplay rendering is fixed-point and atlas-based:

1. restore the cached background/highway pixels for dirty regions;
2. draw sustain strips back-to-front;
3. choose a pre-scaled note size band from time-to-hit;
4. interpolate the lane center between vanishing-point and receptor anchors;
5. blit note sprites with transparent clipping;
6. draw receptors, flames, and HUD sprites;
7. update the LCD once at the frame deadline.

The visible note window defaults to 2,000 ms and is difficulty-adjustable. The
chart cursor advances monotonically, so each frame examines only events in the
visible window, not the full chart.

The renderer must not open files, decode BMPs, allocate memory, query tagcache,
or inspect audio metadata. State loading and draw functions are separate APIs.

Animation uses elapsed milliseconds. On a missed frame, sprites jump directly
to the correct current position; the game must not play catch-up frames.

## State Machine

```text
BOOT
  -> ASSET_ERROR
  -> NO_TRACK
  -> NO_CHART
  -> READY
READY -> LOAD_GAME -> SEEKING -> COUNT_IN -> PLAYING
PLAYING -> PAUSED -> PLAYING
PLAYING -> RESULTS
PLAYING -> TRACK_CHANGED
RESULTS -> SEEKING (retry)
RESULTS -> READY
any safe state -> EXIT
USB event -> CLEANUP -> PLUGIN_USB_CONNECTED
```

Every transition has one owner for playback changes. Cleanup is idempotent and
must process USB, shutdown, and default Rockbox events.

## Failure Behavior

The plugin fails closed with a specific message for:

- missing or oversized skin;
- bad skin version, dimensions, atlas bounds, or CRC;
- missing chart for the current path;
- chart/audio duration mismatch;
- corrupt or oversized chart;
- unsupported lane count or flags;
- seek which fails to settle;
- track change during play;
- insufficient plugin-buffer headroom;
- score file corruption;
- USB connection or shutdown.

Corrupt scores may be renamed and rebuilt only with user confirmation. A bad
skin or chart must never trigger audio stop, playlist replacement, database
rebuild, or firmware recovery.

## Implementation Milestones

### M0: input and clock probe

- Build an iPod-only diagnostic plugin.
- Record button/Hold/wheel behavior on 5G and 6G.
- Display interpolated audio time against `id3->elapsed`.
- Verify pause, seek-to-zero, resume, track change, and exit.

Exit criterion: a five-minute song shows no accumulating clock drift and music
continues correctly after plugin exit.

### M1: chart compiler and headless judge

- Define `.ihc` serializer/parser.
- Import `.chart` and MIDI fixtures.
- Add deterministic timing, chord, sustain, HOPO, score, and star tests.
- Add exact audio identity/index generation.

Exit criterion: all reference events convert within 1 ms and malformed files
fail without out-of-bounds access.

### M2: external skin pipeline

- Define `skin.ihs` and one local-source profile.
- Crop, convert, pack, pre-scale, hash, and verify user assets.
- Enforce the decoded-size budget on the host.
- Render a static device preview without committing any copyrighted output.

Exit criterion: a complete locally installed pack validates and an incomplete
pack cannot enter gameplay.

### M3: simulator vertical slice

- Match an already playing fixture track.
- Select difficulty, seek, resume, render, judge, pause, finish, and retry.
- Drive deterministic synthetic input against a known chart.
- Produce timing, frame, score, and resource logs.

Exit criterion: the expected score and framebuffer checkpoints match across
ten consecutive runs with no file-descriptor or memory drift.

### M4: physical iPod 6G

- Qualify Tap-5 and rolled chords.
- Tune calibration and wheel gesture thresholds.
- Meet 30 fps and input-latency budgets.
- Complete the full audio transition matrix.

Exit criterion: three complete songs across MP3, FLAC, and AAC pass without a
freeze, track mismatch, playback loss, or score discrepancy.

### M5: iPod Video 5G and auto-chart beta

- Meet the 20 fps floor without changing judgement timing.
- Run desktop generation against a reviewed multi-genre corpus.
- Add editor output and confidence labels.

Exit criterion: every generated chart is playable within density constraints;
musical-quality release still requires human review.

## Verification Gates

### Host tests

- tempo-map integration including mid-song tempo changes;
- negative and positive chart offsets;
- exact simultaneous-note chord merge;
- sustain overlap and early release;
- HOPO/tap/forced-strum transitions;
- malformed sizes, counts, paths, CRCs, and integer overflows;
- deterministic score and accuracy totals;
- skin crop/atlas bounds and decoded-memory estimate;
- generator difficulty-density limits;
- no network calls in chart or asset preparation.

### Simulator tests

- no track, no chart, missing skin, and corrupt package screens;
- seek acknowledgement and countdown;
- scripted full combo with exact final score;
- deliberate early, late, wrong-lane, chord, and sustain misses;
- pause/resume and retry at least twenty times;
- injected dropped frames without judgement drift;
- track change and USB event cleanup;
- file-descriptor count returns to baseline after every run;
- plugin-buffer headroom remains stable;
- render path trace proves zero file opens, bitmap decodes, and allocations.

### Required physical audio matrix

- fresh boot -> Files song -> iPod Hero;
- fresh boot -> Database song -> iPod Hero;
- Files song -> game -> Files playback still works;
- Database song -> game -> Database playback still works;
- playing -> pause -> resume -> retry -> exit;
- rapid game/menu/music switching twenty times;
- volume changes during gameplay;
- MP3, FLAC, AAC/ALAC, and one long VBR track;
- automatic next-track transition is caught before further judgement;
- Hold pause never emits a phantom lane hit;
- no playlist identity or ordering change.

### Performance evidence

Record on both hardware targets:

- average, 95th percentile, and maximum frame ticks;
- missed frame deadlines;
- input event to judgement ticks;
- audio-clock corrections and maximum interpolation lead;
- active event count and maximum sprites drawn;
- plugin image size, active asset bytes, chart bytes, and free headroom;
- file-descriptor count before load, during play, and after exit.

Simulator screenshots are visual evidence only. Hardware timing and playback
behavior are authoritative.

## Build and Deployment

V1 should add `ipodhero` to `apps/plugins/SUBDIRS`, its local `SOURCES`, the
Games category, and target guards requiring a 320x240 color iPod keypad.

Plugin-only changes may be copied as `ipodhero.rock`. Any firmware or plugin API
change requires a full firmware rebuild. If a physical firmware deploy is
needed, follow the repository rule: deploy `rockbox.ipod` to both the volume
root and `.rockbox/rockbox.ipod`, verify both hashes against the local build,
then sync/eject. A full iPod 6G package deploy must use
`tools/deploy_ipod6g_preserve_database.sh` and preserve all tagcache/database
files.

## V1 Definition of Done

V1 is complete only when:

1. a user can prepare a complete external Guitar Hero skin from local files;
2. no copyrighted media is required in the repository or build artifacts;
3. the in-game library lists readable indexed songs and can start a selected
   song without requiring playback first;
4. all five lanes, rolled chords, sustains, HOPO/taps, star phrases, scoring,
   pause, retry, and results work on physical iPod 6G;
5. note judgement stays synchronized for a complete long track;
6. the game meets its memory and frame-rate gates;
7. exiting leaves playback APIs and the selected session usable, and normal
   Files/Database playback works immediately;
8. simulator, host, iPod 6G, and iPod Video 5G gates pass;
9. the source audit finds no embedded Guitar Hero pixels, audio, charts, or
   downloader URLs;
10. documentation clearly distinguishes authored and generated chart quality.

## Recommended First Slice

Implement M0 and M1 before touching copyrighted visual files. The first
playable slice should use a synthetic test skin kept outside the final runtime
package, one user-owned song, and a 60-second hand-authored test chart. Once
clocking, controls, and playback exit are proven on hardware, connect the
external asset importer and the user's real skin. This ordering prevents art
work from hiding the two hardest risks: click-wheel chord input and song-clock
accuracy.

## Host Analysis References

- [Spotify Basic Pitch](https://github.com/spotify/basic-pitch) is the initial
  candidate for optional host-side audio-to-MIDI transcription.
- [Demucs](https://github.com/facebookresearch/demucs) documents the optional
  guitar-capable separation model, as well as the maintenance limitations that
  require iPod Hero to keep stem separation behind a replaceable adapter.

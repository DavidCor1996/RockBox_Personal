# iPod 5G docked/undocked video-output research

## Scope and result

This pass investigated why Rockpod's Apple VideoCore H.264 path was smooth
while docked but choppy on the iPod LCD. The implemented conclusion is:

- choose exactly one MPlayer output when playback starts;
- use display `0` for the iPod LCD and display `2` for TV output;
- enable the TV DAC only when display `2` is selected; and
- turn the TV DAC off during teardown if it was enabled.

This reproduces the stock player/service topology. Rockpod uses the iPod 5G
dock-mode GPIO to make the destination choice automatically: docked selects
TV and undocked selects the LCD. It intentionally does not change destination
in the middle of a movie.

## Source provenance

The static analysis used Apple's unmodified iPod 5G 1.3 updater image from
Apple's CDN:

- source: <https://secure-appldnld.apple.com/iPod/SBML/osx/bundles/061-2965.20080313.R45jT/iPod_13.1.3.ipsw>
- IPSW SHA-256: `66aad071f960061dcfbdfe69773a698a59b9635c18ba9cb4478f57fd69306cb7`
- extracted OSOS SHA-256: `f7b7670bafd1d6ac45effa00b4d314550c030218a953468cfc6057d6d80e6ea7`
- OSOS runtime base used for disassembly: `0x10000000`

No Apple binary is committed to this repository. Users provision the needed
VideoCore files from their own copy of the updater with
`tools/install_ipod5g_videocore.py`.

Apple's user-facing model is documented in the official
[iPod classic User Guide](https://cdsassets.apple.com/live/6GJYWVAV/user/ma630_ipod_classic_120gb_en.pdf):
`TV Out = Ask` offers TV or iPod as a choice each time a video starts, while
fit-to-screen and PAL/NTSC are separate options. This manual is for the next
click-wheel generation, so it is supporting evidence for the UI contract;
the exact 5G service behavior below comes from the 5G 1.3 RetailOS image.

## RetailOS evidence

### External-output controller

The function at `0x10163b98` formats one command from one display identifier
stored at object offset `+0x04`:

```
mp_region display=%d dest=fullscreen mode=%s
```

It then formats:

```
display_control %d dac=1 encoding=%d
```

The controller constructor at `0x10163c68` initializes that display identifier
to `2`. The mode is selected between `letterbox` and `fill`; the encoding is
selected between values `1` and `5`. The teardown function at `0x10163b64`
checks its DAC-enabled byte and sends the exact command:

```
display_control 2 dac=0
```

The important property is that this controller creates one region and owns
the DAC for one TV output. It does not create simultaneous display `0` and
display `2` regions.

### MPlayer destination selection

The function at `0x101f92a4` returns either `0` or `2` after parsing the
current display/output state. Its result is consumed by the MPlayer controller,
which stores a single display ID. The nearby region formatting paths at
`0x101f9328` and `0x101f93d4` likewise accept one display ID.

This establishes the stock architecture:

1. RetailOS resolves the playback destination.
2. MPlayer receives one region for that destination.
3. The composite DAC is enabled only for the TV-output controller.

### Dock signal available to Rockbox

Rockbox's iPod 5G diagnostic screen labels `GPIOA_INPUT_VAL & 0x10` as
`Dock mode`, separately from USB power and external power. This is a better
automatic selector than `charger_inserted()`, which would misclassify an
ordinary USB charging cable as TV output.

## Root cause in the initial Rockpod implementation

The initial player always sent all of the following, whether docked or not:

```
mp_region display=0 dest=fullscreen mode=letterbox
mp_region display=2 dest=fullscreen mode=letterbox
display_control 2 dac=1 encoding=5
```

That diverged from RetailOS by keeping two output/scaler paths active and by
powering the TV DAC during LCD playback. The reported symptom—smooth composite
output but choppy undocked LCD playback—is consistent with unnecessary display
pipeline work. This is a causal inference from the observed symptom plus the
confirmed command-topology mismatch; RetailOS has no source-level diagnostic
statement naming the symptom.

## Implementation mapping

`apps/video_playback_5g.c` now:

- samples dock mode once at movie launch;
- sends one `mp_region` command for display `0` or `2`;
- applies fill/letterbox changes only to that selected display;
- sends `dac=1 encoding=5` only for docked TV playback; and
- sends `dac=0` during cleanup only if this playback enabled the TV DAC.

No CPU boost was added. Although RetailOS contains playback-starvation
diagnostics and dynamic clock-management code, this pass did not prove a
movie-start clock transition. Adding a permanent Rockbox CPU boost would
therefore be a workaround, not an Apple-authentic result.

## Evidence-gap matrix

| Question | Evidence | Confidence | Remaining gap |
|---|---|---:|---|
| Does stock drive LCD and TV simultaneously? | 5G RetailOS controller builds one region from one display ID. | High | None for service topology. |
| Which display IDs are used? | 5G RetailOS selection returns `0` or `2`; TV controller initializes `2`. | High | Symbol names are absent from the stripped image. |
| Is the TV DAC always on? | DAC-on is in the TV controller; teardown conditionally sends `dac=0`. | High | Exact PAL/NTSC preference mapping to encoding `1`/`5` is not completed. |
| Does Apple auto-detect the dock? | Apple guide exposes a TV/iPod choice; Rockbox identifies a dock GPIO. | Medium | The exact 5G preference-to-hardware decision path is not fully named. |
| Does Apple boost the PortalPlayer CPU for movies? | Clock manager and player diagnostics exist, but no proven call edge from movie start. | Low | More call-graph recovery would be required. |
| Does stock hot-switch outputs mid-play? | Apple guide frames selection at play start; MPlayer stores one output. | Medium | A physical RetailOS hot-plug trace was not performed. |

## Verification

- iPod Video hardware build completed successfully after the change.
- The linked hardware ELF contains one parameterized `mp_region` format,
  `display_control 2 dac=1 encoding=5`, and the matching DAC-off command.
- The undocked performance result still requires a physical 5G/5.5G run,
  because the simulator does not emulate the BCM2722 VideoCore/display path.

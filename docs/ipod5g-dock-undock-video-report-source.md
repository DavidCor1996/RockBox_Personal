# iPod 5G docked/undocked video-output research

## Scope and result

This pass investigated why Rockpod's Apple VideoCore H.264 path was smooth
while docked but choppy on the iPod LCD. The implemented conclusion is:

- choose exactly one MPlayer output when playback starts;
- use display `0` for the iPod LCD and display `2` for TV output;
- enable the TV DAC only when display `2` is selected; and
- turn the TV DAC off during teardown if it was enabled;
- install RetailOS's VideoCore playback power policy for the VMCS session; and
- hold Rockbox's PP5022 performance clock while its host transport is active.

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

### Playback performance policy

The first output-routing fix was not sufficient on physical hardware: the
reported frame rate remained low only when undocked. A second pass recovered
the MPlayer power lifecycle that had been missing from Rockpod.

The 5G RetailOS MPlayer calls the power-management singleton at `0x101afd74`
and its acquire method at `0x101b013c` before both select/play paths:

- `mp_play`: calls at `0x101f8e98` and `0x101f8ea0`;
- `mp_selectplay`: calls at `0x101fa3e8` and `0x101fa3f0`.

The acquire method submits the exact VideoCore policy command:

```
pm_set_policy min
```

RetailOS also contains a conditional policy-release method at `0x101b0094`
and a default command installed by the power-manager constructor:

```
power_management set_policy manual 27
```

The default string is at OSOS offset `0x4f8870`; its pointer is stored by the
constructor at `0x101b0548`. The release path is conditional on internal
power-manager client state and is not a simple inverse of MPlayer's one-time
`pm_set_policy min` initialization, so Rockpod does not issue it speculatively.
Stopping the BCM2722 terminates the whole VMCS session. Apple's VMCS image
independently contains the `power_management` and `pm_set_policy` command
handlers. This establishes a direct MPlayer-to-VideoCore policy call edge,
rather than merely a nearby clock-management routine.

Rockpod also has a host-side obligation that RetailOS's internal scheduler
handled: it must service passthrough requests, read MP4 samples, and transfer
them across the PP5022/BCM2722 interface quickly enough to avoid late packets.
Rockbox normally runs the PP5022 at 30 MHz and raises it to 80 MHz through its
balanced CPU-boost API. Rockbox's MPEG player holds that boost throughout
active playback and releases it on pause/stop. The Apple-backed VideoCore
policy plus the equivalent Rockbox host-performance hold are therefore both
applied; neither changes the encoded media or decoder quality.

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
powering the TV DAC during LCD playback. Correcting it reproduced Apple's
output topology but did not restore undocked frame rate on physical hardware.
The remaining confirmed mismatch was the omitted MPlayer power-policy acquire;
Rockpod's host transport also lacked Rockbox's normal active-video CPU hold.

## Implementation mapping

`apps/video_playback_5g.c` now:

- samples dock mode once at movie launch;
- sends one `mp_region` command for display `0` or `2`;
- applies fill/letterbox changes only to that selected display;
- sends `dac=1 encoding=5` only for docked TV playback; and
- sends `dac=0` during cleanup only if this playback enabled the TV DAC;
- sends Apple's `pm_set_policy min` before selecting/playing media;
- balances `cpu_boost(true/false)` across play, pause, resume, every error path,
  and normal teardown.

The CPU hold is not permanent: pause releases it and resume reacquires it. This
matches Rockbox's established long-form MPEG playback lifecycle and avoids
changing output resolution, frame rate, profile, bitrate, or decode quality.

## Evidence-gap matrix

| Question | Evidence | Confidence | Remaining gap |
|---|---|---:|---|
| Does stock drive LCD and TV simultaneously? | 5G RetailOS controller builds one region from one display ID. | High | None for service topology. |
| Which display IDs are used? | 5G RetailOS selection returns `0` or `2`; TV controller initializes `2`. | High | Symbol names are absent from the stripped image. |
| Is the TV DAC always on? | DAC-on is in the TV controller; teardown conditionally sends `dac=0`. | High | Exact PAL/NTSC preference mapping to encoding `1`/`5` is not completed. |
| Does Apple auto-detect the dock? | Apple guide exposes a TV/iPod choice; Rockbox identifies a dock GPIO. | Medium | The exact 5G preference-to-hardware decision path is not fully named. |
| Does Apple request a movie performance state? | `mp_play` and `mp_selectplay` directly install `pm_set_policy min`. | High for VideoCore | The separate conditional release client's exact semantics remain unnamed in the stripped image. |
| Does Rockpod's host transport need a CPU hold? | PP5022 normal/max are 30/80 MHz; Rockbox MPEG playback holds the standard boost while active. | High | Physical undocked validation of this correction remains required. |
| Does stock hot-switch outputs mid-play? | Apple guide frames selection at play start; MPlayer stores one output. | Medium | A physical RetailOS hot-plug trace was not performed. |

## Verification

- iPod Video hardware build completed successfully after the change.
- The linked hardware ELF contains one parameterized `mp_region` format,
  the stock `pm_set_policy min` command,
  `display_control 2 dac=1 encoding=5`, and the matching DAC-off command.
- The undocked performance result still requires a physical 5G/5.5G run,
  because the simulator does not emulate the BCM2722 VideoCore/display path.

# iPod 5G docked/undocked video-output research

## Scope and result

This pass investigated the complete 5G RetailOS playback/output path, including
the later report that H.264 blanked the LCD and left the iPod in a permanent
wait. The implemented conclusions are:

- always retain MPlayer display `0` for the iPod LCD;
- add a matching display `2` region when TV output is requested;
- enable the TV DAC only when display `2` is selected; and
- turn the TV DAC off during teardown if it was enabled;
- install RetailOS's VideoCore playback power policy for the VMCS session;
- hold Rockbox's PP5022 performance clock while its host transport is active;
- recreate Apple's reset/power/strap precondition before the retail VMCS loader;
- upload the updater's exact 201,376-byte retail runtime before discovering its
  playback service rings; and
- bound every Rockbox player-side BCM2722 wait, then cold-restore the
  device-matched NOR LCD image and backlight circuit on failure.

The loader, transport, codecs, display identifiers and DAC lifecycle reproduce
the stock player/service topology. Rockpod intentionally gives the 5G the same
mirror contract as its 6G implementation: `Off` keeps only the iPod LCD, while
`On` retains display `0`, adds Apple display `2`, and enables its TV DAC.
Rockpod's `Auto` policy samples the iPod 5G dock-mode GPIO at launch. The GPIO
policy and simultaneous mirror are Rockbox behavior, not claims about Apple's
stock preference resolver. Output policy does not change mid-movie.

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
selected between values `1` and `5`. A second output controller at `0x101c76f0`
maps output mode `2` to 720x480 and mode `3` to 720x576; its enable path selects
encoding `1` only for mode `3`. Therefore encoding `1` is PAL/576 and encoding
`5` is NTSC/480. The teardown function at `0x10163b64` checks its DAC-enabled
byte and sends the exact command:

```
display_control 2 dac=0
```

Apple's controller creates one region and owns the DAC for one TV output. It
does not create simultaneous display `0` and display `2` regions. Rockpod adds
the display-0 region as its explicit 6G-parity mirror policy while preserving
Apple's display-2 and DAC commands unchanged.

### MPlayer destination selection

The earlier interpretation of `0x101f92a4` as an output selector was wrong.
Its callee at `0x101f9404` parses `mp_get_status` fields including `duration`,
`elapsed`, `paused`, `mode`, `state`, and `substate`. `0x101f92a4` returns `2`
for MPlayer's `playstill` or `play` states and returns `0` for idle/end states;
those values are playback-state codes, not LCD/TV display identifiers.

The external-output controller above independently establishes display `2`,
single-region ownership, and conditional DAC control. Internal playback uses
display `0`. The exact stripped RetailOS preference resolver that chooses
between those controllers remains unnamed, so Rockpod does not cite the
playback-status parser as evidence for its `Auto` policy.

This establishes the stock architecture:

1. RetailOS constructs either its internal or external playback controller.
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
ordinary USB charging cable as TV output. It is evidence for implementing a
useful Rockbox `Auto` mode, not proof that RetailOS used this bit in its
preference resolver.

### NOR and RetailOS VMCS ownership

RetailOS function `0x10287698` is the generic VMCS/VLL image uploader used
while establishing the VideoCore runtime. It first
calls helper `0x10287998`, which writes the primary bootstrap bytes

```
A1 81 91 02 12 22 72 62
```

and the alternate bytes beginning at `02`, then waits until both
`BCM_RD_ADDR` and `BCM_ALT_RD_ADDR` are ready and consumes both write-address
ports. Only after that helper returns does `0x10287698` wait for alternate
control bit `0x80` to clear and bit `0x40` to set, upload the image, and run
the startup command/poll sequence.

Decompiling both images resolves the ambiguity around the word "VMCS." The
device-matched image in NOR is 101,728 bytes and implements the LCD/diagnostic
command set used by Rockbox. It does not contain the `dispman`, `gencmd`, or
`hostreq` runtime and cannot expose MPlayer's GENCMD, DISPLAY, HOSTFS, and
PASSTHRU services. The updater resource is a different, 201,376-byte flat
VideoCore image containing that full runtime and the exports required by the
codec VLLs.

RetailOS uploads the full image once while establishing its display runtime,
then its movie application uses the already-running services. Rockbox starts
from the smaller NOR image because its LCD driver depends on that command set.
It must therefore perform RetailOS's NOR-to-retail transition when entering
the Apple movie player, and restore the NOR image on return to the Rockbox UI.
The frequency is a consequence of the two host display stacks; the hardware
transition itself now matches the recovered Apple uploader.

The first Rockpod implementation cold-powered the BCM down before the retail
upload and reached service discovery, but its host-service implementation was
still incomplete. A later warm-upload experiment verified SRAM successfully
but timed out starting the core because Rockbox's NOR VMCS had never been
reset into Apple's loader precondition. Another experiment incorrectly treated
the resident NOR image as the retail service runtime. Several of those builds
also failed to repaint the LCD or re-enable the backlight circuit on errors,
which obscured the actual startup stage.

The corrected player waits for an idle LCD while its owner is still intact,
stops LCD traffic, shuts down the NOR image through its supported command set,
holds the BCM rail low, restores Apple's power/strap precondition, writes the
primary and alternate bootstrap sequences, uploads the retail image to
internal address zero, clears `0x1f8`, writes
`0xC0000000` to `0x10000c00`, waits for bit zero, clears that register, writes
`0xA5A50002` to `0x10000400`, and waits for `0x1f8` to become nonzero. Only
then does the application discover the retail service table at `0x1f0`.

The `0xA5A50001`/`0xA5A50002` exchange in the separate RetailOS state machine
is a resume transition for an already-loaded retail runtime, not the initial
NOR-to-retail loader. Adding that exchange before the image upload would not
match either executable.

## Root cause in the initial Rockpod implementation

The initial player always sent all of the following, whether docked or not:

```
mp_region display=0 dest=fullscreen mode=letterbox
mp_region display=2 dest=fullscreen mode=letterbox
display_control 2 dac=1 encoding=5
```

That diverged from RetailOS by keeping two output/scaler paths active and by
powering the TV DAC during LCD playback. A later report that this correction
produced working 5G docked playback was from a different device/chat and is not
5G validation. The first 5G hardware run remained a black screen and lockup.
The warm-upload mismatch was leaving the NOR runtime executing while replacing
its SRAM image; a complete readback could succeed while the start handshake
still timed out. The resident-only attempt also failed because the NOR runtime
is LCD-only. The cold loader now preserves the later transport, MPlayer power
policy, host CPU hold, and full LCD/backlight recovery fixes.

## Implementation mapping

`apps/video_playback_5g.c` now:

- applies `Off`, `Auto`, or `On` once at movie launch;
- always sends an `mp_region` command for display `0` and adds display `2`
  when TV output is requested;
- applies the same fill/letterbox change to both mirrored displays;
- sends `dac=1 encoding=5` only for docked TV playback; and
- sends `dac=0` during cleanup only if this playback enabled the TV DAC;
- sends Apple's `pm_set_policy min` before selecting/playing media;
- balances `cpu_boost(true/false)` across play, pause, resume, every error path,
  and normal teardown; and
- uses only deadline-bounded host reads/writes while the retail image owns the
  BCM2722.

`firmware/target/arm/ipod/video/lcd-video.c` keeps the proven normal Rockbox
LCD bootstrap unchanged. Movie startup now recreates the cold-reset condition
under which Apple enters its two-channel bootstrap, then uploads retail
`vmcs.bin`. Normal
movie teardown powers down the retail runtime and cold-loads the
device-matched NOR LCD image. Every player-side bus wait has a deadline. A
failed handoff runs the same NOR recovery immediately and explicitly restores
both the backlight circuit and LED before returning an error.

The CPU hold is not permanent: pause releases it and resume reacquires it. This
matches Rockbox's established long-form MPEG playback lifecycle and avoids
changing output resolution, frame rate, profile, bitrate, or decode quality.

## Evidence-gap matrix

| Question | Evidence | Confidence | Remaining gap |
|---|---|---:|---|
| Does stock drive LCD and TV simultaneously? | 5G RetailOS controller builds one region from one display ID. | High | None for service topology. |
| Which display IDs are used? | Internal playback uses `0`; the TV controller initializes its single display ID to `2`. | High | The stripped preference-controller constructor remains unnamed. |
| Is the TV DAC always on? | DAC-on is in the TV controller; teardown conditionally sends `dac=0`; 480/576 paths resolve encoding `5`/`1`. | High | The saved PAL/NTSC preference object's name remains stripped. |
| Does Apple auto-detect the dock? | Rockbox identifies a dock GPIO; the prior claimed selector is actually an MPlayer status parser. | Low/unknown for Apple | Rockpod's `Auto` GPIO mapping is explicitly its own launch policy. |
| Does Apple request a movie performance state? | `mp_play` and `mp_selectplay` directly install `pm_set_policy min`. | High for VideoCore | The separate conditional release client's exact semantics remain unnamed in the stripped image. |
| Does Rockpod's host transport need a CPU hold? | PP5022 normal/max are 30/80 MHz; Rockbox MPEG playback holds the standard boost while active. | High | Physical undocked validation of this correction remains required. |
| Does stock hot-switch outputs mid-play? | Apple guide frames selection at play start; MPlayer stores one output. | Medium | A physical RetailOS hot-plug trace was not performed. |
| What caused the black/no-backlight failure? | Error recovery disabled LCD ownership without fully reloading the NOR image and restoring both backlight controls. A separate warm upload left the old NOR core executing and timed out at start despite a verified SRAM image. | High; the stages are directly visible in source and hardware diagnostics. | Physical confirmation of the cold-reset loader with repaired recovery remains required. |

## Verification

- iPod Video hardware build completed successfully after the change.
- The linked hardware ELF contains one parameterized `mp_region` format,
  the stock `pm_set_policy min` command,
  `display_control 2 dac=1 encoding=5`, and the matching DAC-off command.
- Static source gates require Apple's cold-reset dual-channel bootstrap and exact
  retail start-register sequence, finite player bus deadlines, LCD-idle
  ownership transfer, and explicit LCD/backlight recovery.
- The undocked performance result still requires a physical 5G/5.5G run,
  because the simulator does not emulate the BCM2722 VideoCore/display path.

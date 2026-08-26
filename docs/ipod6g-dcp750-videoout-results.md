# iPod 6G/7G to Philips DCP750/37 video-out results

This is the hardware experiment ledger for the physical DCP750/37.  Update it
before changing another video format, plane address, geometry value, or field
mode.  A successful build or simulator run is not a hardware result.

## Fixed hardware facts

- Device under test: iPod Classic 6G/7G target (`ipod6g`) seated directly in
  the Philips DCP750/37 30-pin dock.
- The 6G mechanically fits the dock.
- Charging and analog sound work.
- Accessory detection works: representative readback is `dock:1`,
  `rid:535403` (also observed as `539375`), `port:1`, `link:1`, with `rx`
  normally zero and occasionally nonzero.
- The DCP750 backlight and video lock react to the iPod's SDO output.  Moving
  the clickwheel used to restore a timed-out image until the LCD/output clock
  lifetime was fixed.  Therefore charging, iAP authentication, and basic CVBS
  routing are not the remaining image problem.

## Proven properties that must not regress

| Property | Physical observation | Conclusion |
| --- | --- | --- |
| NTSC sync and SDO routing | DCP750 locks; its screen/backlight responds | Clock, GPIO E4, SDO, DAC mux, and dock CVBS pin are functional |
| Mixer background | Black, gray, and white fields are repeatable | Mixer-to-SDO path works independently of framebuffer DMA |
| Format-8 bars/UI | Bars and legible Rockbox text have appeared; one stage-6 build held fully readable text | VP DMA reaches the DCP750 and format 8 is the correct producer family |
| Clock lifetime | Image formerly vanished until the wheel moved; retaining the shared LCD/output clock made stages persistent | Do not reintroduce an idle/backlight timeout as a video shutdown |
| Horizontal geometry | Centered `x=36`, width `648` was reported correct | Preserve the qualified 90% NTSC horizontal viewport |
| Plane color order | The three-plane Y/Cb/Cr lineage produced correct red Netflix artwork and blue selection | Preserve the working private format-8 plane order |
| User control | Composite output can be toggled in Quick Settings | Keep immediate on/off and clean shutdown |

## Failed configurations

| Configuration | Result on DCP750 | What it rules out |
| --- | --- | --- |
| Mixer RGB565/XRGB candidates | White screens, flashes, clipped text, rainbow fragments, or unstable images | The normal UI should not use the unqualified mixer graphics path |
| VP image/source `640x240`, destination about `640x240` | All text could be readable, but the picture occupied only the upper half | A 240-line VP frame cannot fill NTSC vertically at 1:1 |
| Three-plane Y/Cb/Cr with a `640x480` physical Y plane made by duplicating each LCD row, image/source geometry `480`, and `V=284` | Correct first four 60-line grid rows followed by physical row 239 extending downward | In logical LCD coordinates the failure still begins at row 120; doubling stored rows and the ratio does not expose the lower logical half |
| Three-plane Y/Cb/Cr with a `640x240` physical Y plane, image/source geometry `240`, and `V=142` | Four of eight 30-line grid rows are correct, then source row 119 extends downward; live UI fails at the same halfway line | The native-height plane still fails at logical row 120; a 640-wide software expansion is not yet proven equivalent to RetailOS |
| Same `640x240` physical planes and `V=142`, changing only image height from `240` to `480` while source height stays `240` | Exact same result: correct colors and upper 120 rows, then the lower half stretches source row 119 | `VP_IMG_HEIGHT` does not control the observed boundary |
| Same `640x240` physical planes, image height `480`, and `V=142`, changing only source height from `240` to `480` | Exact same Stage-4 and Stage-5 result | `VP_SRC_HEIGHT` also does not control the observed boundary; the former `SRC_HEIGHT / 2` conclusion was false |
| Destination Y/height divided as in later S5P Linux drivers | Upper-half-only picture | This S5L private path uses RetailOS full-frame destination coordinates |
| Firmware `ab731d43335ce8e24c87b93bfcd30489d9b062f489cc81f25575e402a59629b7`: NV12 plus `VP_MODE=0x24`, `SRC_HEIGHT=120`, top/bottom Y pointers | Yellow/green/blue corruption and the same lower-half stretching; photo `codex-clipboard-3c004aa2-1e82-4455-9911-c19920bef96d.png` | Standard Samsung NV12 field-pointer semantics do not describe S5L8702 private plane mode 1; do not repeat this path |

Earlier diagnostic runs also showed green-only stages, white instead of
yellow, brief distorted text, and unstable color bars.  Those runs predate
the corrected mixer register map and shared-clock lifetime and are not useful
geometry candidates.

## RetailOS evidence and the missing invariant

RetailOS format 8 does all of the following:

- the higher-level pixel-format mapper at decrypted-body offset `0x120ad4` maps software format
  `0` to hardware format `8`, identifying this as the stock movie/VP path;
- writes three caller descriptors in the order `0`, `2`, `1` to VP offsets
  `0x28`, `0x2c`, and `0x30`;
- clears the fourth descriptor at `0x34`;
- writes luma span `width` and chroma span `width / 2`;
- selects private plane mode `1`;
- leaves `VP_MODE` at its reset value `0`;
- writes image dimensions and source-crop dimensions through independent
  fields: the config copier at body offset `0x120978` clamps image height to
  at least destination height, the image setter at `0x169b04` writes that
  value to `VP_IMG_HEIGHT`, and the draw path at `0x168ae8` separately writes
  the unmodified movie height to `VP_SRC_HEIGHT`;
- scales movie-source geometry into a full NTSC-height output surface.

The hardware results disambiguate those private descriptors as the proven
three-plane 4:2:0 color path, not the later public Samsung top/bottom-field
NV12 interpretation. The stock geometry probe for a nominal 320x240 movie
configuration programs source height `240`, destination height `480`, and
vertical ratio `128`; this records the high-level stock configuration but does
not explain the private format-8 field-count interpretation by itself.

Four controlled bordered-grid builds reject that interpretation. Changing
`VP_IMG_HEIGHT` alone from 240 to 480 did not move the boundary. Changing
`VP_SRC_HEIGHT` alone from 240 to 480 also did not move it. Doubling every
stored luma row and the vertical ratio moved the physical repeated row from
119 to 239 but left the failure at logical LCD row 120. Neither height
register has therefore been shown to impose the boundary.

The remaining concrete difference from the stock path is source width and
packing. RetailOS's nominal 4:3 configuration passes a native 320x240 movie
surface, source crop 320x240, destination 720x480, H ratio 227, and V ratio
128. The failing Rockbox experiments instead supplied a 640-wide luma plane
created by duplicating every LCD pixel horizontally and assumed that the VP
would treat it as mathematically equivalent. Private format-8 DMA behavior
makes that assumption unproven. The exact RetailOS-native allocation is:

| Plane | Dimensions | Bytes |
| --- | ---: | ---: |
| Y | 320x240 | 76,800 |
| Cb | 160x120 | 19,200 |
| Cr | 160x120 | 19,200 |
| Total | planar 4:2:0 | 115,200 |

Each chroma sample covers a native 2x2 luma block. In the qualified centered
`(36,24) 648x432` viewport, the same RetailOS formulas give H ratio `252` and
V ratio `142`.

RockPod's simulator cannot arbitrate this issue. Its host preview transports
and aspect-fits an ARGB copy of the Rockbox framebuffer; it does not model the
S5L8702 VP, private planar DMA, interlaced fields, spans, or composite timing.
It confirms that all 320x240 source rows exist, but not how format 8 reads
them.

## Current qualification target

The next build changes only the source allocation/packing and the registers
directly implied by exact RetailOS 320x240 geometry:

- restore private three-plane pointers and `VP_MODE=0`;
- allocate Y 320x240, Cb 160x120, Cr 160x120 with true 2x2 chroma averaging;
- program image and source geometry 320x240 with spans 320/160;
- retain destination `(36,24) 648x432`, clock lifetime, mixer/SDO setup,
  three-plane color order, and Quick Settings behavior;
- use the stock formulas, producing H ratio 252 and V ratio 142.

Success requires the entire 320x240 UI—including Applications, Extras,
Settings, and Now Playing—to appear with correct colors and without a stretched
lower section.  Only after that static result passes should moving-video
tearing or refresh-rate work begin.

## Failed qualification build: full-height source

Firmware SHA-256:
`562baeb444adab918250caf914d6c50ecd6cd251c7ad0020a3d7d83c549e751a`.

Deployed firmware-only on 2026-08-25 to both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`; both on-device hashes matched the local build before
the volume was flushed and safely unmounted.

The static gate passed before deployment.  Compiled ARM disassembly confirms:

- descriptor `0x28` is Y; descriptor `0x2c` is exactly Y + 307,200; descriptor
  `0x30` is exactly Y + 384,000; descriptor `0x34` is zero;
- `VP_MODE=0`, image width/height `640x480`, plane mode `1`, and spans
  `640/320`;
- the normal UI call supplies source height `480` and destination
  `(36,24) 648x432`;
- the stock formulas produce H ratio `505` and V ratio `284`.

Physical result: the first four of the grid's eight logical rows render with
correct colors and geometry. At the exact 240-line source boundary, the fourth
row repeats vertically to the bottom. Photo:
`codex-clipboard-f4023e74-f8d6-4004-810f-6d406038d9ab.png`.

This proves all of the following in one frame:

- private Y/Cb/Cr order and BT.601 color conversion are correct;
- horizontal packing, `x=36`, width `648`, and the bordered viewport are
  correct;
- the VP consumes 240 physical luma lines, not the software-declared 480;
- `V=284` consumes those 240 lines in half the destination, then clamps line
  239. This correctly requires source height 240 and V ratio 142, but does not
  imply that the independent image-surface height should also become 240.

## Qualification build: native 240-line source

Firmware SHA-256:
`aa00bb883e0c22ce6b959e23ca2d9377daf0a95f8f19f4f34fb8aaf173984286`.

Built on 2026-08-26. The static gate passes, and compiled ARM disassembly
confirms:

- descriptor `0x28` is Y; descriptor `0x2c` is exactly Y + 153,600;
  descriptor `0x30` is exactly Y + 192,000; descriptor `0x34` is zero;
- the complete allocation is exactly 230,400 bytes: Y `640x240`, Cb
  `320x120`, and Cr `320x120`;
- `VP_MODE=0`, image width/height `640x240`, private plane mode `1`, and
  spans `640/320` are preserved;
- each LCD pixel is duplicated only horizontally in luma; chroma is averaged
  from each adjacent pair of 320-pixel LCD rows;
- the normal UI call supplies source height `240` and destination
  `(36,24) 648x432`; the unchanged stock formulas produce H ratio `505` and
  V ratio `142`.

This build changes only the vertical source packing and geometry proven wrong
by the `562bae...` grid. Plane order, color conversion, horizontal geometry,
destination viewport, clock lifetime, and Quick Settings control are
unchanged.

Deployed firmware-only on 2026-08-26 to both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`. Both on-device hashes matched the local build before
the volume was flushed and safely unmounted.

### Physical result

Stage 4 still renders exactly four of the eight grid rows correctly, then
extends the fourth row as vertical columns to the bottom. Stage 5 renders its
upper live lines correctly and produces the same lower vertical extension.
Photos:

- `codex-clipboard-fdfae2ae-919e-47a2-b1f4-81c668f1f0e1.png` (Stage 4)
- `codex-clipboard-4b8999f3-2890-44a7-abd9-f0cc0e456fcd.png` (Stage 5)

This rejects the build. Because this version stores each of the 240 LCD rows
once and divides the grid into eight 30-line rows, the transition after four
rows is source line 120. The preceding `562bae...` build stored each LCD row
twice and transitioned after four 60-line rows, source line 240. The invariant
is therefore half of the shared image/source-height value, not a fixed
240-line buffer boundary. In these two builds image height and source height
were always changed together, so this physical result alone could not identify
which register imposed the boundary. Color planes, horizontal packing, and the
destination viewport remain proven.

At this point image and source height had always changed together, so the
boundary-owning register was still ambiguous. The following one-register
isolation resolves it physically.

## Next qualification build: independent image and source heights

Firmware SHA-256:
`bfcd8ef9660794ab899d2e134191720d07d38b509634ac9ae4a349836a640f12`.

This build changes one register value from `aa00bb...`:

- physical planes stay Y `640x240`, Cb `320x120`, Cr `320x120`;
- private format-8 pointers, spans, mode, colors, and cache handling stay
  unchanged;
- `VP_IMG_HEIGHT` becomes the full NTSC surface height `480`;
- `VP_SRC_HEIGHT` remains the physical movie-source height `240`;
- destination remains `(36,24) 648x432`, H ratio `505`, V ratio `142`.

This is the first build that reproduces the stock image/source-height
separation. It is not a ratio sweep and does not alter any property already
qualified on the DCP750.

Built on 2026-08-26. The static gate passes. Compiled ARM disassembly confirms
literal image height 480 at `VP+0x40`, source height 240 at `VP+0x50`, and the
stage-4/5 vertical ratio 142 at `VP+0x68`. The complete 230,400-byte planar
allocation, descriptor offsets, spans, private plane mode, and color path are
unchanged.

Deployed firmware-only on 2026-08-26 to both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`. Both on-device hashes matched the local build before
the volume was flushed and safely unmounted.

### Physical result

Rejected. The result is exactly the same as `aa00bb...`: colors are correct,
the upper half is correct, and the bottom half is a stretched final source
row. Changing only `VP_IMG_HEIGHT` from 240 to 480 did not move the transition
from physical source row 119.

This is the decisive register isolation: the clamp follows
`VP_SRC_HEIGHT / 2`, not `VP_IMG_HEIGHT / 2`. It requires a source-register
height of 480 to expose physical rows 0-239. The earlier `562bae...` build used
that register value but also used the doubled `V=284`, which consumed those
240 exposed rows in half of the destination. The next build therefore combines
`SRC_HEIGHT=480` with the independently qualified physical-row ratio `V=142`.

## Next qualification build: interlaced source count, physical-row ratio

Firmware SHA-256:
`b420d98e350108329879b89124269ef7d8342a801842a19cc977b6b8b76e67e7`.

This build changes only two coupled source-geometry values from `bfcd8e...`:

- physical planes remain Y `640x240`, Cb `320x120`, Cr `320x120`;
- `VP_IMG_HEIGHT=480` and `VP_SRC_HEIGHT=480` describe the interlaced raster;
- `V_RATIO=142` continues to sample the 240 physical rows across the qualified
  432-line destination;
- destination `(36,24) 648x432`, H ratio `505`, private mode/pointers/spans,
  colors, cache handling, clock lifetime, and Quick Settings remain locked.

Predicted falsifiable result: all eight 30-line grid rows should be visible.
If the lower half still repeats, this combined model is rejected; no unrelated
format, color, field-mode, pointer, or viewport setting is being changed.

Built on 2026-08-26. The static gate passes. Compiled ARM disassembly confirms:

- the normal composite UI caller supplies source-register height `480`;
- the default vertical-ratio calculation divides the physical-row numerator
  `240 << 12` by destination height and then shifts by four, yielding `142` for
  the 432-line viewport;
- the worker writes literal `480` to `VP_IMG_HEIGHT`, its independent source
  argument to `VP_SRC_HEIGHT`, and the physical-row ratio to `VP_V_RATIO`;
- physical allocation and descriptor offsets remain exactly 230,400 bytes:
  Y at base, Cb at base + 153,600, and Cr at base + 192,000.

Deployed firmware-only on 2026-08-26 to both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`. The local file and both on-device copies all matched
SHA-256 `b420d98e350108329879b89124269ef7d8342a801842a19cc977b6b8b76e67e7`
before the volume was flushed and safely unmounted.

### Physical result

Rejected. Stage 4 has exactly the same lower-four-row stretch as the two
preceding builds, and Stage 5 is also unchanged. Colors remain correct.
Changing `VP_SRC_HEIGHT` from 240 to 480 while holding the physical planes,
image height, ratio, destination, and color path fixed had no visible effect.

This disproves the predicted `VP_SRC_HEIGHT / 2` clamp. Together, the last two
single-variable isolates prove that neither the value written at `VP+0x40`
nor the value written at `VP+0x50` controls the observed halfway boundary in
the way previously assumed. Do not perform another image/source-height or
vertical-ratio sweep. The repeated boundary must now be treated as a field or
plane-layout problem—or as evidence that one or both offsets have been
misidentified—until the stock descriptor semantics and live register
readbacks are resolved.

## Qualification build: exact RetailOS-native source packing

Firmware SHA-256:
`ca935f0cdfe70f90bc8009f64f1d9b17fea6fa485d05bd2d72eac5b5b9c265c4`.

This build removes the remaining source-layout mismatch with the nominal stock
4:3 movie path:

- Y is native `320x240` (76,800 bytes), Cb is `160x120` (19,200 bytes), and
  Cr is `160x120` (19,200 bytes), for a complete 115,200-byte frame;
- luma is copied one-to-one from the Rockbox LCD and each chroma sample is the
  average of its native 2x2 luma block;
- image and source dimensions are both `320x240`, spans are `320/160`, and the
  private format-8 Y/Cb/Cr descriptor order remains unchanged;
- full-output Stage 3 uses the exact stock H/V ratios `227/128`;
- centered Stages 4 and 5 retain destination `(36,24) 648x432` and use the
  stock formulas, producing H/V ratios `252/142`;
- the iPod diagnostics now show image and source readbacks directly as
  `I:320x240 S:320x240` and include those values in live-change detection.

Built on 2026-08-26. The compiled ARM image and source pass the video-output
static gate. Disassembly confirms the 76,800-byte Y offset, 19,200-byte chroma
offset, 115,200-byte total cache range, 320/160 spans, and native conversion.

Deployed firmware-only on 2026-08-26 to both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`. The local file and both on-device copies all matched
the SHA-256 above before the volume was flushed and safely unmounted. No full
`.rockbox` package write was performed.

### Physical result

Rejected. Stage 4 again shows exactly the first four of eight logical grid
rows, followed by the fourth row stretched to the bottom. This is the same
logical row-120 boundary as every qualified 640-wide build.

The exact RetailOS-native source width, 320/160 spans, 115,200-byte plane
packing, and stock-derived H/V ratios therefore do not change the failure.
Together with the preceding height isolates, this rules out all of the
following as the boundary owner:

- 320-wide versus 640-wide source packing;
- 240 versus 480 image height;
- 240 versus 480 source height;
- 142 versus 284 vertical ratio when paired with the corresponding physical
  row duplication;
- plane allocation size at the tested YUV420 boundaries.

The surviving invariant is one 120-line field of a 240-line source. Treat the
lower-half repeat as missing second-field presentation or field-address
selection until the RetailOS decoded-frame descriptor constructor and field
handoff are reconstructed. Do not run another width, height, or ratio sweep.

## Next qualification build: restore the stock mixer scan state

Tracing the format-8 frame descriptors back to RetailOS's decoded-frame
object closes the descriptor-layout question.  Software format 0 asks that
object for indices 0, 2, and 3 and passes the returned allocations as the
three descriptors consumed by hardware format 8.  A separate RetailOS test
path allocates those buffers as one full-size Y plane and two quarter-size
Cb/Cr planes.  There is no fourth field buffer or hidden lower-half pointer.

The first register-level difference that survives every physical result is
in the mixer scan configuration, downstream of those descriptors:

- RetailOS clears mixer-config bit 2 for NTSC at runtime `0x0816aad0` through
  `0x0816aad8`;
- its progressive/interlaced selector at `0x0816ab90` through `0x0816abc4`
  then clears and sets mixer-config bit 1 before loading the interlaced SDO
  table;
- enabling the VP layer adds bit 4, so the stock format-8 frame runs with
  mixer config `0x12`;
- every failing Rockbox format-8 diagnostic has instead run with config
  `0x10`, as both the source and the on-iPod `C:` readback show.

The next build restores that exact stock bit transition and changes nothing
else.  It retains native Y `320x240`, Cb/Cr `160x120`, mode 0, plane mode 1,
spans `320/160`, H/V ratios `252/142`, destination `(36,24) 648x432`, the
qualified color order, clock lifetime, and Quick Settings behavior.  This is
the first field-state correction supported by a direct stock disassembly and
an observed register mismatch; it is not another geometry sweep.

Firmware SHA-256:
`94e864fa467ae4c09848178c0e72e87f016fb604c8e5ee512d9236c9271aeb08`.

Built on 2026-08-26.  The complete video-output static gate passes.  Compiled
ARM disassembly of `ipod6g_videoout_enable_sync` confirms the mixer operation
as `bic ..., #6`, `orr ..., #2`, and a store to mixer offset `0x04`; enabling
the existing VP layer later preserves bit 1 and adds bit 4.

Deployed firmware-only to both `/rockbox.ipod` and
`/.rockbox/rockbox.ipod`.  The local image and both on-device copies matched
the SHA-256 above before `sync`; the iPod volume was then safely unmounted.
No full `.rockbox` package write was performed.

Physical pass criteria: Stage 4 must report mixer config `C:00000012` and
show all eight bordered grid rows.  The fourth row must not extend through the
bottom half.  Stage 5 should then show the complete UI with the already
qualified colors and viewport.

### Physical result

Passed on the Philips DCP750/37.  The user confirmed that both color and size
are now correct: the full frame is visible, the lower-half stretch is gone,
and the output matches the iPod viewport.  This physically validates mixer
config `0x12` as the missing second-field/interlaced presentation state.

Lock the following output contract for subsequent work: native planar YUV420
with Y `320x240`, Cb/Cr `160x120`, spans `320/160`, mode 0, plane mode 1,
destination `(36,24) 648x432`, H/V ratios `252/142`, and mixer config `0x12`.
Do not alter this qualified color, geometry, packing, or field state while
optimizing throughput.  UI and video motion are currently choppy; that is a
separate performance issue in the live framebuffer mirror path.

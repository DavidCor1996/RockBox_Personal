# Native decoded video on the Classic TV output

This records the initial H.264 implementation. The restored TV-mode code now
uses the shared native presenter for both H.264 and MPEG. See
[MPEG integration and Live TV sizing](livetv-native-mpeg.md) for the current
follow-up and measured conversion estimates. Historical build hashes below
identify the earlier H.264-only artifact.

The iPod 6G hardware H.264 player now supplies its decoded YUV420 planes to
the TV compositor before their detail is lost in the 320x240 LCD image.
The enhanced TV build presents up to 640x480 source detail inside its existing
648x432 analog viewport. This is still interlaced composite output, not a
change to output timing or a promise of 480p.

## Presentation contract

`video_draw_frame()` continues to construct its normal LCD image, including
letterboxes, captions, controls, chat and other application decoration. It
also passes a borrowed `videoout_frame` describing the decoded image and its
rectangle on the LCD canvas. `videoout_blit_yuv()` consumes both synchronously
under the LCD mutex. No decoded pointer survives the call and no decoder
surface becomes a hardware scanout pointer.

The TV compositor produces the canvas in one pass. It compares each 2x2
LCD movie tile (four luma and two
chroma samples) against the original decoder image sampled with the player's
exact fixed-point LCD scaler. Unchanged tiles are replaced from the original
source at TV resolution. Changed tiles retain the composed LCD image on all
three planes. Thus captions, translucent controls and chat remain visible;
their affected tiles retain LCD resolution. This is not a separate native
resolution subtitle or text renderer. An overlay that produces exactly the
same six samples as the underlying LCD tile is indistinguishable from an
unchanged tile and takes the native-detail path.

At 640x480 mapped to the full 320x240 LCD rectangle, TV source samples are
copied exactly, including detail between the samples retained by the LCD.
Other source/rectangle sizes use bounded bilinear resampling with replicated
edges. Existing fit/fill and application layout decisions are preserved.
No extra detail is invented for files encoded at 320x240: when the source is
no larger than its LCD rectangle, the original mirror path remains in use.

The entire inactive scanout frame is replaced without copying the current
front frame first. Existing padding, cache publication and field handoff are
preserved. A field timeout retains the last complete TV frame without retrying
the same presentation at LCD resolution. Later RGB presentation overlays still
take precedence through the existing LCD composition path.

## Scope and resource ownership

- Enabled by `HAVE_VIDEOOUT_NATIVE_YUV` in enhanced Classic builds.
- No plugin API change; no new heap, core, audio or frame allocation.
- Reuses the existing two target-owned TV surfaces, including edge guards.
- No changes to PCM, mixer callbacks, decoder buffer ownership, playlists,
  codec power, or exit/seek/pause lifecycle.
- Plain builds, the simulator, low-resolution input and unsupported geometry
  use the established presentation behavior.
- Applies to the core hardware H.264 player and its application wrappers.
  MPEG player's LCD-sized output is not changed by this work.

## Validation

`python3 tools/tests/composite_native_video_gate.py` compiles the actual
driver entry, compositor and application's LCD scaler with ASan/UBSan.
It checks both padded and unpadded scanout layouts: every native output pixel,
640x480 identity, letterboxing, non-integer resampling, embedded rectangles,
minimal plane dimensions, chroma-only overlay changes, source immutability,
front-frame/row guards, invalid inputs, inactive output and field timeout.
The existing enhanced compositor and edge-guard gates also pass.

Clean enhanced and ordinary iPod 6G firmware builds and the existing iPod 6G
simulator build pass. The isolated enhanced artifact is
`build-tv-native-video/rockbox.ipod`, configured for `/.rbtv`; SHA-256:
`2b6b952bd6725b7b7fc47d4de23896c375b5d6496cffc04fde8d0e9d5b45054f`.
No device installation was performed.

The pre-change existing enhanced ELF measured text/data/BSS
2953972/17420/9032964 bytes; the final isolated ELF measures
2959040/17424/9033988. These artifacts use different runtime paths, so this
is not an exact same-configuration code-size comparison. No new static
frame arrays were introduced. The native presentation's compiled ARM stack
frame is 200 bytes including saved registers, excluding callees; the LCD
presentation frame is 96 bytes. The borrowed frame description is 40 bytes
on ARM, local to the player's draw call.

Physical acceptance is still required. Use a compatible 640x480 H.264 clip
with fine text/detail, plus a widescreen clip. Compare the TV to the former
LCD-sized path; verify controls, captions, pause/seek, audio sync, and return
to Database/Files playback. Check dock/undock and the far-left edge. The
simulator cannot establish analog image quality or real-time decoder load.

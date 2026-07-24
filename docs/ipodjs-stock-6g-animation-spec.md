# iPodJS Stock iPod Classic 6G Animation Spec

## Scope

This pass covers stock-style motion owned by the iPodJS shell:

- forward and backward menu navigation;
- root-menu preview settling and replacement;
- cached Music/video/photo/game preview motion;
- Cover Flow handoff behavior where the shell owns both frames.

It does not change codecs, PCM, mixer state, the audio buffer, playlists, or
plugin-owned animation.

## Reference Evidence

- Apple's archived September 2007 event page identifies the launch UI and
  original iPod classic presentation:
  `https://www.apple.com/ca/quicktime/qtv/specialevent0907/`
- Period device footage, *iPod classic menus*, shows full-screen horizontal
  push navigation on a real classic:
  `https://www.youtube.com/watch?v=DBrGyMPsZ5s`
- 24 fps close-up device footage, *Getting to Know the iPod Classic and iPod
  Nano Controls For Dummies*, provides measurable transition frames:
  `https://www.youtube.com/watch?v=L2-5fTttt_c`
- The September 2007 Gadgeteer review describes the contextual pane as a
  random slideshow using a Ken Burns-style effect:
  `https://the-gadgeteer.com/2007/09/19/apple_ipod_classic/`
- The September 2007 Ars Technica review describes panning, zooming, fading,
  the menu shadow, and Cover Flow's two halves sliding from the edges to meet
  at center:
  `https://arstechnica.com/gadgets/2007/09/the-ipod-gets-a-makeover-a-review-of-the-ipod-nano-and-ipod-classic/`

### Measured navigation timing

The 24 fps close-up was inspected frame by frame around the Home -> Podcasts
transition. The first destination strip appears at the right edge, the old
screen moves left, and the destination reaches full width over approximately
six captured frame intervals: about 250 ms. Positions are consistent with a
smooth ease-in/ease-out curve rather than linear motion.

Because camera cadence, LCD response, and video encoding limit precision, the
implementation target is a range, not a false exact constant:

- duration: 240-280 ms;
- presentation cadence: 24-30 fps;
- curve: cubic smoothstep;
- forward: old screen exits left, destination enters from right;
- back/Menu: old screen exits right, destination enters from left;
- no alpha fade during the menu push.

### Measured preview timing

In the same close-up, contextual art remains stable while the highlight is
moving, then changes after the selection settles. The visible replacement is
about a quarter second and includes intermediate blended frames. The target is:

- selection settle: about 500 ms after the last wheel step;
- replacement crossfade: 240-280 ms at 24-30 fps;
- active pan: cached pixels only, approximately 10-12 fps;
- image change: crossfade between already cached images;
- no decode, path scan, or storage wake inside the draw/fade loop.

The stock pane is described as Ken Burns-style motion. iPodJS preserves its
bounded center-crop pan. True continuous zoom is deferred because scaling every
frame would add cost and interpolation unlike the stock precomposed assets;
the pass must first deliver the observed pan and fade timing safely.

## Memory And Playback Contract

- Follow `docs/ipodjs-ui-memory-animation-steering.md`.
- Navigation must animate while a track is playing without core allocation,
  audio-buffer shrink, stop/restart, or playlist mutation.
- Transition workspace is fixed and iPod-6G-scoped. Its exact BSS cost must be
  reported from the native ELF.
- All effects degrade to the final destination frame if they cannot run.
- Queued rapid navigation must never trigger artwork service for abandoned
  screens.

## Rendering Contract

### Menu push/pop

- Capture source before dispatch and destination after its complete draw.
- Present eight logical samples including endpoints (`0..7`).
- Use seven timed intervals with cubic smoothstep and scheduling derived
  from `HZ`, producing approximately 250 ms total motion.
- Compose exact framebuffer pixels; do not redraw or approximate Apple UI
  elements inside the animator.
- Leave the final framebuffer equal to the destination frame.

### Preview crossfade

- Capture only the right pane before replacement.
- Draw the destination normally from verified assets or cached artwork.
- Blend RGB565 channels over the same measured duration.
- Preserve the left menu exactly. The right pane is blended from captured
  pixels, including any compositor-owned header, battery, and play indicator;
  the animator must not redraw or approximate those elements.
- Ordinary pan frames update directly and do not recursively crossfade.

### Cover Flow

- Generic shell entry/return uses the same directional push when both shell
  frames exist.
- A plugin-owned Cover Flow scene remains responsible for cover movement.
- Do not fake the stock edge-to-center Cover Flow exit unless both independent
  half panes are available; a faithful full-screen push is preferred to a
  hand-drawn approximation.

## Acceptance

- Navigation trace contains complete monotonic `0..7` forward/back groups.
- Measured trace duration is 240-280 ms on `HZ == 100`.
- Transitions occur with `AUDIO_STATUS_PLAY` set and playback identity remains
  unchanged.
- No transition calls `core_alloc`, `core_free`, or an audio control API.
- Preview replacement has intermediate blended frames and leaves exact final
  destination pixels.
- Ten full-depth and twenty rapid Albums/Artists cycles pass with stable core
  memory and file descriptors.
- Native build and ARM stack/BSS audit pass before hardware deployment.

## Implemented 6G Resource Cost

The implementation uses two target-scoped 320x240 RGB565 workspaces:

- old/source frame: 153,600 bytes;
- new/destination frame: 153,600 bytes;
- exact framebuffer workspace total: 307,200 bytes of BSS;
- runtime core/buflib allocation: zero.

The native ELF grew from 3,826,504 to 4,133,736 bytes of BSS. The additional
32 bytes are animation state and alignment. ARM prologue inspection shows an
88-byte maximum frame for the menu compositor and a 112-byte maximum frame for
the pane fade.

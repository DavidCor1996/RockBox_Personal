# Cart Surfer 2006 Native Reference

This table records the parity-critical values extracted from the preserved
2006 Cart Surfer ActionScript. It is the behavioral source for the Rockbox
native minigame; the SWF is not shipped or executed on the iPod.

## Provenance

```text
repository: nhaar/Waddle-Forever
commit: bcf7e9d4d4f7619710492448d532f4e7eb1e5caa
path: media/default/fix/CartSurfer2006.swf
sha256: fd30e04c8de51fbfd970841fac9e761d0f9c41481f77b3bee74524a42fe188f9
format: Flash 6
extractor: JPEXS FFDec 26.2.1
script: frame 1 DoAction
```

## Gameplay Constants

| Constant | 2006 value |
| --- | ---: |
| Starting lives | 4 |
| Ollie | 20 points |
| Backflip | 100 points |
| Spin | 80 points |
| Handstand/flap | 50 points |
| Maximum grind | 80 points |
| Maximum slide | 40 points |
| Successful lean | 10 points |
| Repeated identical trick | floor(base / 2) |
| Grind scoring threshold | more than 10 ticks |
| Maximum grind duration | 28 ticks |
| Maximum slide duration | 32 ticks |
| Maximum lean duration | 38 ticks |
| Coin reward | floor(score / 10) |

The original track segment sequence is:

```text
1,1,4,2,1,5,3,1,4,2,5,3,1,1,1,4,2,4,2,1,1,5,3,1,1,6
```

Segment `1` is straight and establishes a safe restart point. Segment `2`
requires a right lean during its corner. Other corner segments require a left
lean. The final `6` segment ends the run.

## Original Keyboard Semantics

- neutral + left/right: lean
- up: surfing stance; left/right while surfing: slide
- down: wheelie stance; left/right while wheelie: grind
- Space: jump/ollie
- left/right while jumping: spin
- up while jumping: handstand/flap
- Space while wheelie: backflip

The click-wheel port maps these semantics to fewer physical controls, but the
score values, repeat penalty, lives, ordered segments, and coin conversion
remain sourced from this table.

## Extracted Art Used By The Native Package

- title: main timeline frame 1
- tunnel: `DefineSprite 201`, frame 1
- animated track patches: `DefineSprite 201`, frames 1, 2, 3, and 4
- cart animation: `DefineSprite 173`, frames 1, 9, 5, 10, 15, and 22

The device atlas preserves these extracted pixels in a 320x64 sheet: six
40x40 cart cells followed by four transparent 80x24 track patches. The track
patches are composited over the preserved frame-1 tunnel at fixed-step speed;
no replacement track or character art is drawn by the native renderer.

Generated BMP checksums are recorded in the package manifests. Any change to
the source hash or extracted character/frame IDs requires a new visual review.

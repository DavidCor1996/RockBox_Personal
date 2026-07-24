# iPone Designer video right pane handoff

## Goal

Support MP4/video wallpaper for iPone Designer themes without breaking the stock iPone SBS layout.

Expected behavior:

- Video appears only on the SBS right pane.
- Video is tied to the mini player/right-pane wallpaper behavior.
- Video must not appear on lockscreen, hold screen, charging screen, WPS, or settings submenus where the right pane should use normal iPone behavior.
- Full Art mode must remain the original iPone-style album-art slideshow behavior.
- Default/no-video designer themes must render exactly like current iPone Designer output.

## Current implementation state

RockPod currently preprocesses MP4 into a generated framepack asset:

- `RightPaneVideo.rbvp`
- 12 fps
- the complete source clip (up to the RBVP 65,535-frame format limit)
- 156x240 right-pane crop
- RGB565 raw frames with a small `RBVP` header

RockPod still generates a normal static fallback right-pane bitmap:

- `RightPaneWallpaper.bmp`

The SBS text injection approach was abandoned because adding a new SBS skin tag or extra preloaded image content can push the already-large iPone SBS over Rockbox skin limits and cause fallback to the native Rockbox menu.

Current preferred approach:

- Keep generated `.sbs` structurally identical to normal iPone Designer SBS.
- Do not inject `%xv(...)` or other video tags into the SBS.
- Have Rockbox draw the generated framepack from `statusbar-skinned.c` after the SBS renders, similar to the existing hardcoded iPone full-art slideshow hook.

## Confirmed findings

- Stock iPone still renders correctly in simulator.
- A no-video iPone Designer theme renders correctly in simulator.
- Long generated SBS/asset names can break/fallback the generated iPone SBS.
- Short generated SBS names fix that layout fallback.
- The same MP4 theme rendered correctly once the generated SBS name was shortened from:
  - `iPoneDesigner-video-smoke-framepack.sbs`
  - to `iPD-video-smok-0914a8.sbs`
- The video framepack contains distinct frames.
- The simulator capture showed correct iPone layout with a right-pane video frame after the short-name fix.

## Runtime animation

The menu-list timeout now treats an eligible right-pane video as fast-update
content, so an idle menu continues calling the 12-fps renderer. The renderer
uses the frame count in the RBVP header instead of assuming the original
24-frame/two-second prototype.

The designer's baked simulator result remains a captured BMP by design. Its
preview widget now reads the deployed RBVP, composites each frame over that
capture, and loops it at the framepack rate. This keeps WPS and lockscreen
previews static and avoids keeping a hidden simulator process alive.

Physical iPod cadence, storage bandwidth, and battery impact still require the
hardware gate before release.

## Current test assets and screenshots

MP4 used:

- `/home/david/Downloads/snaptik_7240088802050641198_v3.mp4`

Useful simulator screenshot outputs:

- `/tmp/ipone-video-fixed-check/00-sbs-slideshow-a.png`
- `/tmp/ipone-video-fixed-check/01-sbs-slideshow-b.png`
- `/tmp/ipone-video-motion2-check/00-sbs-slideshow-a.png`
- `/tmp/ipone-video-motion2-check/01-sbs-slideshow-b.png`

Known good visual result:

- Correct iPone layout appears with shortened SBS name.
- Right pane shows the MP4-derived image.
- It no longer falls back to native Rockbox menu in that case.

## Important implementation notes

- Keep video out of the SBS skin text.
- Keep video out of lockscreen/WPS.
- Keep Full Art mode using original iPone album-art slideshow logic.
- Keep generated SBS names short for designer themes.
- If using hardcoded Rockbox draw logic, detect both old and new designer SBS prefixes:
  - `iPoneDesigner-`
  - `iPD-`
- The generated framepack should live in the SBS asset folder as well as the theme asset folder because the SBS path is now shortened.

## Next video tasks

1. Record a short simulator clip and confirm right-pane pixel motion.
2. Confirm hold/lockscreen does not show video.
3. Confirm Full Art still shows original iPone album-art slideshow.
4. Confirm Settings/submenus keep normal iPone background behavior.
5. Test source clips longer than two seconds and verify the final frame loops to the first.
6. Only after simulator proof, test on device.

## Next requested designer fixes

### SBS battery position

For themes customized through iPone Designer, move the SBS battery one pixel left.

Current target area:

- SBS battery viewport currently appears around `x=134`.
- Designer-customized themes should use `x=133`.
- Stock/original iPone should not change unless explicitly desired.

Implementation direction:

- Patch generated SBS output for iPone Designer themes only.
- Search/replace the SBS battery viewport line from:
  - `%V(134,5,25,11,-)`
- to:
  - `%V(133,5,25,11,-)`

### Hold text and battery percent color

Hold text and lockscreen battery percent text should match the user-selected lockscreen clock color.

Affected lockscreen/AOD-style text:

- `HOLD`
- battery percent text near the top-right of lockscreen

Implementation direction:

- Use `variant["lockscreen_clock"]["color"]` or the normalized lockscreen clock color source.
- Apply that color to generated SBS viewport foreground colors for:
  - normal lockscreen hold text
  - normal lockscreen battery percent
- Be careful not to force AOD/light hold text to white if the current AOD contrast depends on black.

Recommended behavior:

- Normal lockscreen: use selected lockscreen clock color.
- AOD/light lockscreen: either keep current high-contrast black or add a separate AOD-safe override after testing.

# iPodJS Menu Return and Frame Handoff

## Required behavior

- Menu from While Playing returns to the exact browser that launched it.
- Playback started from Cover Flow returns to Cover Flow, at its saved album,
  instead of opening Music or the Home screen.
- Repeated Menu presses walk up one artist, album, track, or folder level.
- Menu at the iPodJS Home screen does nothing; Settings remains a Home item.
- A direct iPodJS screen handoff keeps the source frame visible until the
  destination has drawn its first complete frame.
- No grey placeholder, Rockbox theme, status bar, or intermediate Home frame
  may be presented during the handoff.
- While Playing must render the current album cover as an opaque native
  bitmap. It must not apply the transparent-colour UI-asset path that causes
  missing, stippled, or false-colour cover pixels.

## Implementation contract

The iPod 6G WPS returns `GO_TO_PREVIOUS` for Menu so the root screen history
restores the database or file browser and its saved selection. In an iPodJS
tree, Menu is treated as the normal cancel/back action rather than the
Rockbox jump-to-root action. Activity teardown does not refresh the old theme,
and an existing native-screen handoff does not present the gradient staging
frame. PictureFlow additionally records an explicit WPS origin when it starts
playback. Normal WPS Menu/browse exits consume that origin and relaunch
PictureFlow directly; plugin dispatch is therefore not allowed to erase the
Cover Flow return destination.

## Simulator acceptance

1. Browse Music through Artist, Album, and Tracks, then start a track.
2. Press Menu once and confirm the selected Tracks list is restored.
3. Continue pressing Menu and confirm each parent level appears in order,
   followed by Home.
4. Capture every presented frame for the WPS-to-browser transition; only the
   source WPS and complete destination browser frames are allowed.
5. From a Home-launched While Playing screen, press Menu rapidly five times;
   the result must remain Home without opening Settings or showing a gradient.
6. Start a track from Cover Flow, press Menu in While Playing, and confirm the
   saved Cover Flow album is restored without an intermediate Music or Home
   frame.
7. Compare that album's cover in Cover Flow and While Playing; the artwork
   must have the same intact image content and colours (apart from intentional
   scaling, border, and reflection).

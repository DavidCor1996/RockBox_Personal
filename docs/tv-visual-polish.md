# TV artwork and typography polish

The TV album pane uses the handheld stock WPS projection from
`root_menu_video_draw_stock_wps_art`: a 10-pixel slant and 50-pixel reflection
per 136-pixel cover, scaled to the available TV pane. The slant is reversed
for the right-hand pane; source artwork and its lettering are not flipped.
The reflection fades from the handheld effect's 112/256 source strength to
black. Non-square covers retain their proportions. The entire projected
cover and reflection fit inside the current overscan area.

Album-pane labels use the existing smaller Apple Helvetica face. List rows
have centered baselines and space for the original BackRow selection caps.
Now Playing centers its metadata beside filtered cover art, separates title
and secondary text, adds a short reflection, and shows elapsed and remaining
time at opposite ends of the original Apple progress strip. Unknown duration
shows `--:--` for remaining time.

Graphics and fonts are the existing Apple resources documented in
`tv-apple-assets.json` and `ipodjs-wps-retail-parity-spec.md`; no replacement
imagery was generated. All drawing uses resident pixels and the existing TV
canvas. There are no added buffers, file reads, artwork slots, or playback
ownership changes.

## Validation

- Native iPod 6G firmware and simulator core builds pass.
- Production-renderer host harness passes ASan/UBSan across 24 combinations
  of TV shape, text size and overscan, plus its existing app/guide cases.
- Projection assertions verify reversed slope, readable source orientation,
  reflection fade and extreme portrait/landscape input safety.
- Existing TV ownership, album cache, Home navigation and remote navigation
  tests pass.
- Scoped whitespace check passes.
- No static storage added. Final native ELF: text 3,223,084 bytes, data
  17,724 bytes, BSS 9,495,492 bytes. The initial ELF predates other pending
  source changes, so its total-size difference is not attributable to this
  visual pass.
- Full navigation simulator regression was attempted but fails during fixture
  preparation: the existing `simdisk/.rockbox/ipodjs/previews` tree contains
  recursive links. Live playback/navigation and physical composite appearance
  are not qualified by the host renderer tests.

Previews in `tv-visual-polish/` come from the production renderer with the
original Apple placeholder artwork. They are not photographs of TV output.
The initial revision was deployed and verified at both boot locations. The
centering/idle-cover follow-up is built but not deployed: the iPod is no
longer mounted.


## Centering and idle Music follow-up

Use equal list/art columns with a 16-pixel gutter. Center the projected cover
horizontally within its column and the cover/reflection group vertically in
the available height. Idle Music uses the cached original RetailOS silver
music-note cover and the same projection as album artwork; the dark flat
BackRow placeholder is no longer used in this pane. No drawing-time loads
are introduced if that cached asset is unavailable.

Native and simulator core builds pass after regenerating stale simulator
language outputs. The renderer checks stopped playback explicitly and verifies
that implicit idle artwork produces the same pixels as explicitly supplying
the original Apple cover. Album-cache and Home-navigation gates pass.
Address/undefined-behavior checks pass; leak detection was disabled for the
follow-up runs because LeakSanitizer cannot run under this host's ptrace setup.
The existing full-navigation fixture recursion remains unresolved.

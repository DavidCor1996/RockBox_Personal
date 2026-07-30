iPodJS Extras/Clock pane background
===================================

previews/clock-pane-stock.174x240x24.bmp is composed of exact pixels from
the iPod Classic-look main-menu backdrop (pic21997.bmp, 320x240) in the
"Classic Firmware 5.5G resources and images" community pack, archived
locally at:

  ipod5g-stock-patch/Classic.zip:
    Classic/5.5/5.5G/resource and images/
      Classic Firmware 5.5G resources and images only/images/pic21997.bmp
  sha256 (source BMP):
    d3b238a19f8a102f576b437ed18082f7719de445552e87461147c50b20b41bb2

Construction (tools/generate_ipodjs_clock_pane.py) is crop and column
replication only -- no artwork is traced, redrawn, interpolated, or
resampled:

  - output columns 0-9 copy source columns 190-199 (the pane's left
    shadow ramp);
  - output columns 10-173 replicate source column 240 per row (the pane
    gradient is horizontally uniform from x=200 to the right edge).

  sha256 (generated BMP):
    e7f8cb39f3d8628020b789801afd3b6c248eb08783433712a96c432b7918b277

A pixel-exact Apple capture placed at
.rockbox/ipodjs/apple/previews/clock-pane-stock.174x240x24.bmp overrides
this file automatically through root_menu_video_asset_path().

Note: no large analog clock face bitmap exists in any extractable Apple
firmware image (the 5G firmware draws its clocks procedurally and ships
only 45x45 clock icons plus hand-needle frames; the Classic 6G UI
resources are inside the encrypted OSOS image).  The pane clock is
therefore rendered at runtime over this background, as the stock
firmware itself renders clocks.

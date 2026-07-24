# Personal Apple Asset Overrides

This tree does not commit Apple-owned artwork. For personal-use device builds,
place your own converted Apple assets in the override directories below. The
firmware will prefer these files when present and fall back to the checked-in
RockPod assets when they are absent.

The status bar and accelerated-scroll assets have a stricter path: prepare
them from verified official Apple downloads with
`tools/prepare_ipodjs_apple_assets.py`. The generated directory is gitignored
and is included automatically in personal simulator and device packages.

## iPodJS UI

Runtime path:

```text
.rockbox/ipodjs/apple/
```

This directory mirrors `.rockbox/ipodjs/`. Any bitmap loaded through the iPodJS
asset helper can be overridden by preserving the same relative path. Dark-mode
variants use the existing `-dark` suffix before the file extension.

Examples:

```text
.rockbox/ipodjs/apple/apple-logo-white.48x58x24.bmp
.rockbox/ipodjs/apple/battery-frame.27x12x24.bmp
.rockbox/ipodjs/apple/play.12x12x24.bmp
.rockbox/ipodjs/apple/pause.12x12x24.bmp
.rockbox/ipodjs/apple/default_album_artwork.128x128x24.bmp
.rockbox/ipodjs/apple/volume_full.24x24x24.bmp
.rockbox/ipodjs/apple/volume_mute.24x24x24.bmp
.rockbox/ipodjs/apple/volume_left_stock.13x19x24.bmp
.rockbox/ipodjs/apple/volume_right_stock.21x21x24.bmp
.rockbox/ipodjs/apple/gloss-blue.64x9x24.bmp
.rockbox/ipodjs/apple/icons/<name>.72x72x24.bmp
.rockbox/ipodjs/apple/previews/<name>.174x220x24.bmp
.rockbox/ipodjs/apple/previews/<name>.174x240x24.bmp
.rockbox/ipodjs/apple/qs/<name>.18x18x24.bmp
.rockbox/ipodjs/apple/qs/<name>-dark.18x18x24.bmp
.rockbox/ipodjs/apple/status-battery.apple.26x65x24.bmp
.rockbox/ipodjs/apple/status-header.apple.320x24x24.bmp
.rockbox/ipodjs/apple/status-playback.apple.20x32x24.bmp
.rockbox/ipodjs/apple/status-hold.apple.12x15x24.bmp
.rockbox/ipodjs/apple/status-repeat.apple.21x38x24.bmp
.rockbox/ipodjs/apple/status-shuffle.apple.21x19x24.bmp
.rockbox/ipodjs/apple/fast-scroll-blank.apple.95x82x32.bmp
.rockbox/ipodjs/apple/fast-scroll-123.apple.95x82x32.bmp
.rockbox/ipodjs/apple/volume-low.apple.11x17x24.bmp
.rockbox/ipodjs/apple/volume-high.apple.19x18x24.bmp
.rockbox/ipodjs/apple/brightness-low.apple.20x21x32.bmp
.rockbox/ipodjs/apple/brightness-high.apple.30x31x32.bmp
.rockbox/ipodjs/apple/slider-light.apple.248x20x32.bmp
.rockbox/ipodjs/apple/slider-dark.apple.248x20x32.bmp
.rockbox/ipodjs/apple/slider-fill.apple.316x20x24.bmp
.rockbox/ipodjs/apple/progress-frame.apple.200x22x32.bmp
.rockbox/ipodjs/apple/23-Helvetica-Apple.fnt
```

The A-Z font is a direct conversion of the verified Apple TTF's embedded
23-pixel bitmap strike. `tools/convttf -B` opts into that strike; it does not
rasterize or substitute outline glyphs.

Lookup order in dark mode:

1. `.rockbox/ipodjs/apple/<relative-path>-dark.bmp`
2. `.rockbox/ipodjs/apple/<relative-path>.bmp`
3. `.rockbox/ipodjs/<relative-path>-dark.bmp`
4. `.rockbox/ipodjs/<relative-path>.bmp`

Lookup order in light mode:

1. `.rockbox/ipodjs/apple/<relative-path>.bmp`
2. `.rockbox/ipodjs/<relative-path>.bmp`

## Weather Icons

Runtime path:

```text
.rockbox/rockpod/weather/apple-icons/
```

The home-screen weather pane uses 40px icons:

```text
.rockbox/rockpod/weather/apple-icons/clear.40x40x24.bmp
.rockbox/rockpod/weather/apple-icons/clear-night.40x40x24.bmp
.rockbox/rockpod/weather/apple-icons/cloud.40x40x24.bmp
.rockbox/rockpod/weather/apple-icons/fog.40x40x24.bmp
.rockbox/rockpod/weather/apple-icons/rain.40x40x24.bmp
.rockbox/rockpod/weather/apple-icons/snow.40x40x24.bmp
.rockbox/rockpod/weather/apple-icons/thunderstorm.40x40x24.bmp
.rockbox/rockpod/weather/apple-icons/unknown.40x40x24.bmp
```

The Weather plugin uses matching 64px icons:

```text
.rockbox/rockpod/weather/apple-icons/clear.64x64x24.bmp
.rockbox/rockpod/weather/apple-icons/clear-night.64x64x24.bmp
.rockbox/rockpod/weather/apple-icons/cloud.64x64x24.bmp
.rockbox/rockpod/weather/apple-icons/fog.64x64x24.bmp
.rockbox/rockpod/weather/apple-icons/rain.64x64x24.bmp
.rockbox/rockpod/weather/apple-icons/snow.64x64x24.bmp
.rockbox/rockpod/weather/apple-icons/thunderstorm.64x64x24.bmp
.rockbox/rockpod/weather/apple-icons/unknown.64x64x24.bmp
```

## Bitmap Requirements

Use uncompressed 24-bit BMP files at the exact dimensions in each filename.
Transparent icons should use the same magenta transparency convention as the
existing Rockbox BMP pipeline.

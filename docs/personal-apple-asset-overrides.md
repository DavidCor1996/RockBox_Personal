# Personal Apple Asset Overrides

This tree does not commit Apple-owned artwork. For personal-use device builds,
place your own converted Apple assets in the override directories below. The
firmware will prefer these files when present and fall back to the checked-in
RockPod assets when they are absent.

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
```

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

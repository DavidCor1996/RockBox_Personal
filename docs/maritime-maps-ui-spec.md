# Maritime Maps visual specification

## Scope

`nb_maps.rock` is the offline map browser launched from iPodJS **Maps**. It
opens on a true global satellite map and retains a full-world satellite atlas
through zoom level 8, with a vector reference layer for New Brunswick, Nova
Scotia, and Prince Edward Island. It is an offline reference map, not a
turn-by-turn or live-traffic product.

## Visual and interaction contract

- The initial view is a full-world satellite globe. Left and Right select the
  next real orthographic Blue Marble heading while zoomed out. A globe frame
  is never pixel-wrapped: the Earth stays whole and rotates without a doubled
  or split hemisphere. Every zoom level uses geographically corresponding
  atlas imagery, so no regional frame is ever stretched or substituted for
  another place.
- Satellite is the only base map. Map chrome, sheets, controls, and the status
  bar are always light. Satellite frames are transferred to the iPod before
  use and are never downloaded by the iPod.
- A 22-pixel iPodJS-style top status bar shows the real clock, centred
  **Maps**, and a battery indicator.  It is part of the app chrome and is not
  a Rockbox skinned status bar.
- The globe uses a full-width, light Apple-style information tray rather than
  black caption bars; regional views use a compact white information pill.
- Native-style +/− and scale controls communicate the current zoom state
  without pretending the iPod click wheel is a touch screen.  Highways, major
  regional roads, waterways, province labels, and 20 destinations provide
  useful detail beyond the regional overview.
- **Explore Maritimes** is an offline destination sheet. Play opens it;
  wheel movement selects a city or town; Select loads the matching atlas view
  and leaves a map pin/callout; Right opens available city Satellite Detail;
  Left opens the real Dashcam/Look Around list; Menu closes the sheet. No
  location, traffic, or route is fabricated when offline.
- Moncton, Fredericton, and Saint John provide **Satellite Detail**: Right
  from a city in Explore, or hold Select only while that city is focused, loads the
  matching real 320x184 GeoNB imagery frame once. Select returns to the map
  and Menu returns to Explore. This is a truthful satellite-detail preview,
  not a fabricated or generated street panorama.
- **World Look Around** is opened with Left from Explore. It currently ships
  a real four-direction 360° Times Square scene and a real Westminster,
  London and Berlin dashcam scenes from KartaView plus Eiffel Tower, a
  three-frame downtown Toronto drive sequence, a four-direction Toronto 360°
  Look Around scene, a two-frame Fredericton drive sequence, and a Tokyo Tower
  street-camera scene from Panoramax. All four Times Square 320x184 RGB565
  directions are preloaded before the scene appears, so rotation is
  display-only and does not read storage. Dashcam scenes are a single real
  direction. The scene pack carries its CC BY-SA attribution; additional 360°
  or dashcam scenes follow the same attributed offline-frame contract.
- Map pan is six display pixels per click and is composed from a fixed 2×2
  satellite-tile cache. Tile I/O occurs only when the view crosses a real tile
  boundary; missing zoom tiles are reported rather than substituted with an
  unrelated regional image.
- Outside Explore, wheel forward/back zooms; Left/Right pan west/east; Menu
  pans north. Select toggles labels; hold Select opens the closest installed
  real satellite-detail frame; hold Menu + Select exits. The satellite layer
  deliberately has no schematic road overlay: road data is not drawn unless
  it is part of the cited raster source.

## RockPod offline sync

RockPod's **Device** menu provides **Sync Maps Current Location…** and
**Sync Maps Route (GPX)…**. The location dialog is seeded from Weather but is
explicitly confirmed by the user; the iPod never claims live GPS. The sync
writes `.rockbox/maps/location.v1.tsv` with a current-location marker,
offline nearby-category entry points, up to 12 EXIF-geotagged library photos,
and up to 24 sampled GPX points. In Maps, blue is the current location, amber
markers are synced photos, and the blue line is the GPS route.

## World satellite atlas

RockPod's **Device → Sync Maps World Satellite Atlas…** imports a standard
`z/x/y` raster satellite tile tree. It converts every chosen tile to an exact
320x160 RGB565 frame at `.rockbox/maps/world/z/x_y.r16`. A complete sync
contains the global base image plus every tile at zoom levels 1 through 8.
Because FAT32 cannot place all 65,536 z8 files in one directory, z8 is stored
transparently as `8/` (x 0–126), `8_hi/` (x 127–253), and `8_hi2/` (x 254–255).
Maps selects that storage path from the x coordinate; it remains one
continuous world atlas to the user.
Maps derives the tile coordinate from the current global view, then opens a
frame only when zooming or crossing a tile boundary. It never resizes an
overview image during draw or substitutes a regional image for missing global
coverage.

## Resource contract

The vector data is compiled into the plugin. Satellite and Satellite Detail share
one fixed 320x184 RGB565 (117,760-byte) plugin-owned buffer. Satellite loads
only on a zoom change; road scenes load only on an explicit open action. The
render loop does not open files, decode images, allocate, access tagcache,
touch shared audio, or modify playlists. It never uses `core_alloc`, so it
cannot shrink playback memory.

## Acceptance checks

- At the overview, Left/Right continuously rotate the full wrapped world.
- Moncton (46.0878, -64.7782) opens at its geographic world coordinate rather
  than an eastern-Canada artwork coordinate.
- Pan and zoom retain a continuous map with matching satellite imagery; no
  blue fallback, `tile missing`, or repeated-tile state may appear.
- Explore lists offline destinations and Select focuses the chosen place with
  a pin and label without opening a file or changing playback.
- Street View opens only for the featured cities and rotates its pre-rendered
  vector geometry without allocating, decoding, or accessing storage.
- Enter and exit Maps while music plays: playback and playlist identity remain
  unchanged.
- The iPod 6G simulator and hardware plugin builds complete without warnings.

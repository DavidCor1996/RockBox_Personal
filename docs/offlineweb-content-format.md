# OfflineWeb Content Format

OfflineWeb is a local archive browser. It does not fetch network pages. Keep
each saved site below `.rockbox/offlineweb/` and register its entry page in
`.rockbox/offlineweb/cache/pages.tsv`.

Each `pages.tsv` row is tab-separated:

```
title    original-url    local-page-path    source    neighborhood    author    archived-date    keywords
```

Supported HTML includes headings, paragraphs, lists, links, rules, images, and
local media. Relative links resolve from the current archived page. BMP and
JPEG images render directly. Imported PNG and GIF images render through their
generated `.bmp` sidecars when present. Images without a renderable sidecar,
plus `audio`, `video`, `source`, `embed`, `bgsound`, and `object` elements,
appear as selectable media cards and open their registered Rockbox viewer or
player.

Pages use a 384-row, pixel-scrolling layout. Wheel movement is intentionally
small for readable browsing; a held wheel scrolls faster. One fixed image cache
is filled outside the draw routine, so rendering itself performs no file I/O or
image decoding.

Controls:

```
Wheel             browse sites or scroll the document
Centre            open the focused link or media card
Left / Right      focus the previous or next link
Menu              go back one page; exit from the site library
Play              cycle text zoom
Menu + Centre     exit immediately
```

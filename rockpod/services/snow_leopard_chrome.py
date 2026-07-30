"""Snow Leopard chrome composition from 1:1 Mac OS X 10.6 pixels.

Desktop Mode used to import whole window screenshots and downscale them to the
iPod window size.  A 1162x735 iTunes window reduced to 304x174 is unreadable
mush, and the plugin then painted its own list on top of the scaled screenshot's
own list, so nothing lined up.

This module instead cuts every piece of chrome out of the real screenshots at
its native 1:1 scale and reassembles it at the iPod's geometry: real title bar
caps with real traffic lights, the real toolbar controls, the real source-list
gradient, the real menu panel and highlight, the real Dock shelf.  Nothing is
resampled, so the result is exactly as crisp on the iPod as it was on the Mac.

Only three operations are ever applied to Apple pixels here:

* crop at 1:1;
* repeat a 1-pixel strip along the axis in which the real artwork is constant;
* fill a nine-slice centre with a colour sampled from that same artwork.

The one exception is documented per-recipe: the translucent menu bar is
reconstructed by removing the menu titles and status items with a horizontal
interpolation across the columns they occupy, then reducing the strip
horizontally only.  Its vertical structure and its real Aurora-driven tint are
untouched.  Nothing is redrawn, sharpened or invented.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from PIL import Image

MAGENTA = (255, 0, 255)


# --------------------------------------------------------------------------
# measured 1:1 geometry of the real Snow Leopard artwork
# --------------------------------------------------------------------------

# 10-6-Snow-Leopard-Finder-Home.png (977x751, 1:1, active Finder window)
FINDER_WINDOW_LEFT = 40
FINDER_WINDOW_RIGHT = 937
FINDER_TITLE_TOP = 25
FINDER_TITLE_BOTTOM = 49
FINDER_TOOLBAR_TOP = 49
FINDER_TOOLBAR_BOTTOM = 78
FINDER_BODY_TOP = 103
FINDER_STATUS_TOP = 672
FINDER_STATUS_BOTTOM = 696
FINDER_SIDEBAR_FILL = (222, 228, 234)
FINDER_SIDEBAR_SEPARATOR = 175
FINDER_CONTENT_FILL = (255, 255, 255)
# the "shackett" row is the selected source-list item in this capture
FINDER_SELECTION_TOP = 331
FINDER_SELECTION_BOTTOM = 350

# 10-6-Snow-Leopard-Desktop.png / -Finder-Apple-Menu.png (1920x1080, 1:1)
SCREEN_W = 1920
SCREEN_H = 1080
MENUBAR_HEIGHT = 21
# 4:3 window inside the 16:9 capture; the iPod wallpaper is this same crop, so
# the translucent menu bar tint lines up with the wallpaper underneath it.
CROP_LEFT = (SCREEN_W - SCREEN_H * 4 // 3) // 2
CROP_RIGHT = CROP_LEFT + SCREEN_H * 4 // 3

# 10-6-Snow-Leopard-Finder-Apple-Menu.png: the open Apple menu panel, measured
# to its 1-pixel border.  The first item is highlighted in this capture, so the
# panel body and the highlight gradient are sampled from different rows.
APPLE_MENU_LEFT = 9
APPLE_MENU_RIGHT = 235
APPLE_MENU_TOP = 21
APPLE_MENU_BOTTOM = 300
APPLE_MENU_ROW_TOP = 26
APPLE_MENU_ROW_BOTTOM = 45
# x=215 is right of every item title and left of the shortcut column
APPLE_MENU_CLEAN_COLUMN = 215
APPLE_MENU_BODY_ROW = 60
APPLE_MENU_CAP = 5

LCD_W = 320
LCD_H = 240


@dataclass(frozen=True)
class WindowGeometry:
    """Layout of the composed 304x174 Aqua window, in window-local pixels."""

    width: int = 304
    height: int = 174
    title_h: int = FINDER_TITLE_BOTTOM - FINDER_TITLE_TOP
    toolbar_h: int = FINDER_TOOLBAR_BOTTOM - FINDER_TOOLBAR_TOP
    status_h: int = FINDER_STATUS_BOTTOM - FINDER_STATUS_TOP
    sidebar_w: int = 86

    @property
    def body_y(self) -> int:
        return self.title_h + self.toolbar_h

    @property
    def body_h(self) -> int:
        return self.height - self.title_h - self.toolbar_h - self.status_h

    @property
    def status_y(self) -> int:
        return self.body_y + self.body_h


WINDOW = WindowGeometry()


# --------------------------------------------------------------------------
# primitives
# --------------------------------------------------------------------------


def _tile_h(strip: Image.Image, width: int) -> Image.Image:
    """Repeat a 1-pixel-wide column across `width`."""
    out = Image.new("RGB", (width, strip.height))
    for x in range(width):
        out.paste(strip, (x, 0))
    return out


def _tile_v(strip: Image.Image, height: int) -> Image.Image:
    """Repeat a 1-pixel-tall row down `height`."""
    out = Image.new("RGB", (strip.width, height))
    for y in range(height):
        out.paste(strip, (0, y))
    return out


def _column(image: Image.Image, x: int, top: int, height: int) -> Image.Image:
    return image.crop((x, top, x + 1, top + height))


def _row(image: Image.Image, y: int, left: int, width: int) -> Image.Image:
    return image.crop((left, y, left + width, y + 1))


# --------------------------------------------------------------------------
# window frame
# --------------------------------------------------------------------------


def compose_window(finder: Image.Image, sidebar: bool, width: int = 0,
                   height: int = 0, sidebar_w: int = 0) -> Image.Image:
    """Reassemble one 304x174 active Aqua window from 1:1 Finder pixels."""
    geometry = WindowGeometry(
        width=width or WINDOW.width,
        height=height or WINDOW.height,
        sidebar_w=sidebar_w or WINDOW.sidebar_w,
    )
    window = Image.new("RGB", (geometry.width, geometry.height), MAGENTA)
    right = FINDER_WINDOW_RIGHT

    title_h = geometry.title_h
    window.paste(
        _tile_h(_column(finder, 700, FINDER_TITLE_TOP, title_h), geometry.width),
        (0, 0),
    )
    window.paste(
        finder.crop(
            (FINDER_WINDOW_LEFT, FINDER_TITLE_TOP,
             FINDER_WINDOW_LEFT + 64, FINDER_TITLE_BOTTOM)
        ),
        (0, 0),
    )
    window.paste(
        finder.crop((right - 12, FINDER_TITLE_TOP, right, FINDER_TITLE_BOTTOM)),
        (geometry.width - 12, 0),
    )

    toolbar_h = geometry.toolbar_h
    window.paste(
        _tile_h(_column(finder, 660, FINDER_TOOLBAR_TOP, toolbar_h), geometry.width),
        (0, title_h),
    )
    window.paste(
        finder.crop(
            (FINDER_WINDOW_LEFT, FINDER_TOOLBAR_TOP,
             FINDER_WINDOW_LEFT + 230, FINDER_TOOLBAR_BOTTOM)
        ),
        (0, title_h),
    )
    window.paste(
        finder.crop((right - 10, FINDER_TOOLBAR_TOP, right, FINDER_TOOLBAR_BOTTOM)),
        (geometry.width - 10, title_h),
    )

    body_y, body_h = geometry.body_y, geometry.body_h
    window.paste(
        Image.new("RGB", (geometry.width, body_h), FINDER_CONTENT_FILL), (0, body_y)
    )
    if sidebar:
        window.paste(
            Image.new("RGB", (geometry.sidebar_w, body_h), FINDER_SIDEBAR_FILL),
            (0, body_y),
        )
        window.paste(
            _tile_v(
                _row(finder, 400, FINDER_SIDEBAR_SEPARATOR, 1), body_h
            ),
            (geometry.sidebar_w, body_y),
        )

    status_y, status_h = geometry.status_y, geometry.status_h
    window.paste(
        _tile_h(_column(finder, 500, FINDER_STATUS_TOP, status_h), geometry.width),
        (0, status_y),
    )
    window.paste(
        finder.crop(
            (FINDER_WINDOW_LEFT, FINDER_STATUS_TOP,
             FINDER_WINDOW_LEFT + 6, FINDER_STATUS_BOTTOM)
        ),
        (0, status_y),
    )
    window.paste(
        finder.crop((right - 14, FINDER_STATUS_TOP, right, FINDER_STATUS_BOTTOM)),
        (geometry.width - 14, status_y),
    )
    return window


def compose_title_bar(finder: Image.Image, width: int) -> Image.Image:
    """Build the real active Aqua title bar without allocating a whole window."""
    title_h = FINDER_TITLE_BOTTOM - FINDER_TITLE_TOP
    right = FINDER_WINDOW_RIGHT
    title = _tile_h(
        _column(finder, 700, FINDER_TITLE_TOP, title_h), width
    )
    title.paste(
        finder.crop(
            (
                FINDER_WINDOW_LEFT,
                FINDER_TITLE_TOP,
                FINDER_WINDOW_LEFT + 64,
                FINDER_TITLE_BOTTOM,
            )
        ),
        (0, 0),
    )
    title.paste(
        finder.crop(
            (right - 12, FINDER_TITLE_TOP, right, FINDER_TITLE_BOTTOM)
        ),
        (width - 12, 0),
    )
    return title


def compose_selection(finder: Image.Image, width: int) -> Image.Image:
    """Real source-list selection gradient, repeated to `width`."""
    height = FINDER_SELECTION_BOTTOM - FINDER_SELECTION_TOP
    return _tile_h(_column(finder, 160, FINDER_SELECTION_TOP, height), width)


# --------------------------------------------------------------------------
# desktop: wallpaper, translucent menu bar, Dock shelf
# --------------------------------------------------------------------------


def compose_wallpaper(aurora: Image.Image, width: int = LCD_W,
                      height: int = LCD_H) -> Image.Image:
    """The real Aurora desktop picture, centre-cropped to 4:3 and reduced."""
    source_w, source_h = aurora.size
    if source_w * height > source_h * width:
        crop_w = source_h * width // height
        box = ((source_w - crop_w) // 2, 0,
               (source_w - crop_w) // 2 + crop_w, source_h)
    else:
        crop_h = source_w * height // width
        box = (0, (source_h - crop_h) // 2, source_w,
               (source_h - crop_h) // 2 + crop_h)
    return aurora.crop(box).resize((width, height), Image.Resampling.LANCZOS)


# x=400 is the first text-free column right of the "Help" menu title, over the
# dark top of the Aurora picture: the ordinary neutral Snow Leopard menu bar.
MENUBAR_CLEAN_COLUMN = 400
# The Apple glyph, cut with the menu bar it already sits on.  Taking it as an
# opaque tile rather than keying it out keeps Apple's own antialiasing against
# Apple's own backdrop, and the backdrop is the same bar we repeat beside it.
APPLE_LEFT = 15
APPLE_RIGHT = 37
APPLE_X = 4


def compose_menubar(desktop: Image.Image, width: int = LCD_W,
                    height: int = MENUBAR_HEIGHT) -> Image.Image:
    """The real translucent menu bar, repeated from a text-free column."""
    bar = _tile_h(_column(desktop, MENUBAR_CLEAN_COLUMN, 0, height), width)
    bar.paste(desktop.crop((APPLE_LEFT, 0, APPLE_RIGHT, height)), (APPLE_X, 0))
    return bar


def compose_apple_highlight(capture: Image.Image,
                            height: int = MENUBAR_HEIGHT) -> Image.Image:
    """The same Apple glyph as the open Apple menu draws it: white on blue."""
    return capture.crop((APPLE_LEFT, 0, APPLE_RIGHT, height))


# The 3D Dock shelf is a perspective plane: its back and front edges are
# horizontal across the middle and slope only at the two ends, so the middle
# repeats exactly and only the end caps have to be cut out whole.
DOCK_SHELF_TOP = 1028
DOCK_SHELF_BOTTOM = 1080
DOCK_SHELF_LEFT = 346
DOCK_SHELF_RIGHT = 1570
DOCK_CAP_WIDTH = 34
# x=1471 falls in the gap between two Dock icons: shelf with nothing on it.
DOCK_CLEAN_COLUMN = 1471
# The whole Dock is the real Dock at exactly half scale: Apple's own 32-pixel
# icon variants stand on a shelf reduced 2:1 from the real 64-pixel Dock, so
# the icon-to-shelf proportions stay the ones Apple drew.
DOCK_SCALE = 2
DOCK_SHELF_HEIGHT = (DOCK_SHELF_BOTTOM - DOCK_SHELF_TOP) // DOCK_SCALE
DOCK_ICON = 32
DOCK_SLOTS = 7
DOCK_STEP = 38
DOCK_SHELF_W = DOCK_SLOTS * DOCK_STEP + 22
DOCK_SHELF_X = (LCD_W - DOCK_SHELF_W) // 2
DOCK_SHELF_Y = LCD_H - DOCK_SHELF_HEIGHT
DOCK_ICON_Y = DOCK_SHELF_Y - 16
DOCK_HEIGHT = LCD_H - DOCK_ICON_Y


def compose_dock(desktop: Image.Image, width: int = DOCK_SHELF_W,
                 scale: int = DOCK_SCALE) -> Image.Image:
    """The real Dock shelf: real end caps, real repeating middle, halved."""
    height = DOCK_SHELF_BOTTOM - DOCK_SHELF_TOP
    source_width = width * scale
    shelf = _tile_h(
        _column(desktop, DOCK_CLEAN_COLUMN, DOCK_SHELF_TOP, height),
        source_width,
    )
    shelf.paste(
        desktop.crop(
            (DOCK_SHELF_LEFT, DOCK_SHELF_TOP,
             DOCK_SHELF_LEFT + DOCK_CAP_WIDTH, DOCK_SHELF_BOTTOM)
        ),
        (0, 0),
    )
    shelf.paste(
        desktop.crop(
            (DOCK_SHELF_RIGHT - DOCK_CAP_WIDTH, DOCK_SHELF_TOP,
             DOCK_SHELF_RIGHT, DOCK_SHELF_BOTTOM)
        ),
        (source_width - DOCK_CAP_WIDTH, 0),
    )
    if scale == 1:
        return shelf
    return shelf.resize((width, height // scale), Image.Resampling.LANCZOS)


# --------------------------------------------------------------------------
# menus
# --------------------------------------------------------------------------


def compose_menu_panel(
    capture: Image.Image, width: int, height: int, cap: int = APPLE_MENU_CAP
) -> Image.Image:
    """Reassemble a real menu panel at an arbitrary size from its own edges."""
    cap = max(1, min(cap, width // 2, height // 2))
    left, right = APPLE_MENU_LEFT, APPLE_MENU_RIGHT
    top, bottom = APPLE_MENU_TOP, APPLE_MENU_BOTTOM
    clean = APPLE_MENU_CLEAN_COLUMN
    panel = Image.new("RGB", (width, height), MAGENTA)

    middle_h = height - cap * 2
    panel.paste(
        _tile_h(_column(capture, clean, APPLE_MENU_BODY_ROW, 1), width),
        (0, cap),
    )
    panel.paste(
        Image.new(
            "RGB",
            (width, middle_h),
            capture.getpixel((clean, APPLE_MENU_BODY_ROW)),
        ),
        (0, cap),
    )
    panel.paste(
        _tile_v(_row(capture, APPLE_MENU_BODY_ROW, left, cap), middle_h),
        (0, cap),
    )
    panel.paste(
        _tile_v(_row(capture, APPLE_MENU_BODY_ROW, right - cap, cap), middle_h),
        (width - cap, cap),
    )

    top_strip = capture.crop((left, top, right, top + cap))
    bottom_strip = capture.crop((left, bottom - cap, right, bottom))
    for source, destination_y in ((top_strip, 0), (bottom_strip, height - cap)):
        panel.paste(
            _tile_h(source.crop((clean - left, 0, clean - left + 1, cap)), width),
            (0, destination_y),
        )
        panel.paste(source.crop((0, 0, cap, cap)), (0, destination_y))
        panel.paste(
            source.crop((source.width - cap, 0, source.width, cap)),
            (width - cap, destination_y),
        )
    return panel


def compose_menu_selection(capture: Image.Image, width: int) -> Image.Image:
    """The real blue menu highlight gradient, repeated to `width`."""
    height = APPLE_MENU_ROW_BOTTOM - APPLE_MENU_ROW_TOP
    return _tile_h(
        _column(capture, APPLE_MENU_CLEAN_COLUMN, APPLE_MENU_ROW_TOP, height),
        width,
    )


def crop_apple_glyph(capture: Image.Image) -> Image.Image:
    """The real Apple menu glyph, already composited over the real menu bar."""
    return capture.crop((15, 2, 29, 19))


# --------------------------------------------------------------------------
# scroller
# --------------------------------------------------------------------------

# 10-6-Snow-Leopard-iTunes-v9.png (1162x735, 1:1): the list scroller
SCROLLER_LEFT = 1105
SCROLLER_RIGHT = 1121
SCROLLER_TRACK_ROW = 130
SCROLLER_THUMB_TOP = 141
SCROLLER_THUMB_BOTTOM = 640
SCROLLER_CAP = 10
SCROLLER_WIDTH = SCROLLER_RIGHT - SCROLLER_LEFT


def compose_scroller_track(itunes: Image.Image, height: int) -> Image.Image:
    """The real scroller track, repeated down `height`."""
    return _tile_v(
        _row(itunes, SCROLLER_TRACK_ROW, SCROLLER_LEFT, SCROLLER_WIDTH), height
    )


def compose_scroller_thumb(itunes: Image.Image, height: int) -> Image.Image:
    """The real scroller knob: real rounded caps, real repeating middle."""
    cap = min(SCROLLER_CAP, height // 2)
    thumb = _tile_v(
        _row(itunes, 300, SCROLLER_LEFT, SCROLLER_WIDTH), height
    )
    thumb.paste(
        itunes.crop(
            (SCROLLER_LEFT, SCROLLER_THUMB_TOP,
             SCROLLER_RIGHT, SCROLLER_THUMB_TOP + cap)
        ),
        (0, 0),
    )
    thumb.paste(
        itunes.crop(
            (SCROLLER_LEFT, SCROLLER_THUMB_BOTTOM - cap,
             SCROLLER_RIGHT, SCROLLER_THUMB_BOTTOM)
        ),
        (0, height - cap),
    )
    return thumb


# --------------------------------------------------------------------------
# iTunes 9
# --------------------------------------------------------------------------

# 10-6-Snow-Leopard-iTunes-v9.png (1162x735, 1:1), measured to its dividers.
ITUNES_LEFT = 40
ITUNES_RIGHT = 1121
ITUNES_TITLE_TOP = 25
ITUNES_TITLE_BOTTOM = 47
ITUNES_TRANSPORT_TOP = 47
ITUNES_TRANSPORT_BOTTOM = 88
ITUNES_HEADER_TOP = 123
ITUNES_HEADER_BOTTOM = 140
ITUNES_ROW_TOP = 399
ITUNES_BOTTOM_COLUMN = 450
ITUNES_ROW_HEIGHT = 17
ITUNES_SELECTION_TOP = 141
ITUNES_BOTTOM_TOP = 656
ITUNES_BOTTOM_BOTTOM = 680
ITUNES_SOURCE_FILL = (217, 223, 231)
# the status display in the middle of the transport bar
ITUNES_LCD_LEFT = 349
ITUNES_LCD_RIGHT = 813
ITUNES_LCD_CAP = 20
ITUNES_TRANSPORT_CAP = 150
ITUNES_EDGE_CAP = 31

ITUNES_WINDOW_W = 304
ITUNES_WINDOW_H = 174
ITUNES_SOURCE_W = 86
ITUNES_TITLE_H = ITUNES_TITLE_BOTTOM - ITUNES_TITLE_TOP
ITUNES_TRANSPORT_H = ITUNES_TRANSPORT_BOTTOM - ITUNES_TRANSPORT_TOP
ITUNES_HEADER_H = ITUNES_HEADER_BOTTOM - ITUNES_HEADER_TOP
ITUNES_BOTTOM_H = ITUNES_BOTTOM_BOTTOM - ITUNES_BOTTOM_TOP
ITUNES_BODY_Y = ITUNES_TITLE_H + ITUNES_TRANSPORT_H + ITUNES_HEADER_H
ITUNES_BODY_H = ITUNES_WINDOW_H - ITUNES_BODY_Y - ITUNES_BOTTOM_H


def compose_itunes_window(itunes: Image.Image, width: int = ITUNES_WINDOW_W,
                          height: int = ITUNES_WINDOW_H,
                          source_w: int = ITUNES_SOURCE_W) -> Image.Image:
    """Reassemble the iTunes 9 window from 1:1 iTunes 9 pixels.

    A generic Aqua frame is a Finder window with a different title.  iTunes is
    recognisable because of its own furniture - the round transport buttons,
    the status display between them and the search field, the blue source list
    and the striped track view - so all of it is cut from the real thing.
    """
    body_h = height - ITUNES_BODY_Y - ITUNES_BOTTOM_H
    window = Image.new("RGB", (width, height), MAGENTA)
    left, right = ITUNES_LEFT, ITUNES_RIGHT

    # title bar
    window.paste(
        _tile_h(_column(itunes, 600, ITUNES_TITLE_TOP, ITUNES_TITLE_H),
                width),
        (0, 0),
    )
    window.paste(
        itunes.crop((left, ITUNES_TITLE_TOP, left + 64, ITUNES_TITLE_BOTTOM)),
        (0, 0),
    )
    window.paste(
        itunes.crop((right - 12, ITUNES_TITLE_TOP, right, ITUNES_TITLE_BOTTOM)),
        (width - 12, 0),
    )

    # transport bar: real controls, real status display, real right edge
    y = ITUNES_TITLE_H
    window.paste(
        _tile_h(_column(itunes, 330, ITUNES_TRANSPORT_TOP, ITUNES_TRANSPORT_H),
                width),
        (0, y),
    )
    window.paste(
        itunes.crop((left, ITUNES_TRANSPORT_TOP,
                     left + ITUNES_TRANSPORT_CAP, ITUNES_TRANSPORT_BOTTOM)),
        (0, y),
    )
    window.paste(
        itunes.crop((right - ITUNES_EDGE_CAP, ITUNES_TRANSPORT_TOP,
                     right, ITUNES_TRANSPORT_BOTTOM)),
        (width - ITUNES_EDGE_CAP, y),
    )
    lcd_x = ITUNES_TRANSPORT_CAP + 4
    lcd_w = width - ITUNES_EDGE_CAP - 4 - lcd_x
    if lcd_w > ITUNES_LCD_CAP * 2:
        window.paste(
            _tile_h(_column(itunes, 600, ITUNES_TRANSPORT_TOP,
                            ITUNES_TRANSPORT_H), lcd_w),
            (lcd_x, y),
        )
        window.paste(
            itunes.crop((ITUNES_LCD_LEFT, ITUNES_TRANSPORT_TOP,
                         ITUNES_LCD_LEFT + ITUNES_LCD_CAP,
                         ITUNES_TRANSPORT_BOTTOM)),
            (lcd_x, y),
        )
        window.paste(
            itunes.crop((ITUNES_LCD_RIGHT - ITUNES_LCD_CAP,
                         ITUNES_TRANSPORT_TOP, ITUNES_LCD_RIGHT,
                         ITUNES_TRANSPORT_BOTTOM)),
            (lcd_x + lcd_w - ITUNES_LCD_CAP, y),
        )

    # column header
    y += ITUNES_TRANSPORT_H
    window.paste(
        _tile_h(_column(itunes, 600, ITUNES_HEADER_TOP, ITUNES_HEADER_H),
                width),
        (0, y),
    )

    # source list and striped track view
    y += ITUNES_HEADER_H
    stripes = _tile_h(
        _column(itunes, 600, ITUNES_ROW_TOP, ITUNES_ROW_HEIGHT * 2),
        width,
    )
    for row in range(0, body_h, ITUNES_ROW_HEIGHT * 2):
        window.paste(stripes, (0, y + row))
    window.paste(
        Image.new("RGB", (source_w, body_h), ITUNES_SOURCE_FILL),
        (0, y),
    )
    window.paste(
        _tile_v(_row(itunes, 300, 246, 1), body_h), (source_w, y)
    )

    # bottom bar
    y += body_h
    window.paste(
        _tile_h(_column(itunes, ITUNES_BOTTOM_COLUMN, ITUNES_BOTTOM_TOP,
                        ITUNES_BOTTOM_H), width),
        (0, y),
    )
    window.paste(
        itunes.crop((left, ITUNES_BOTTOM_TOP, left + 8,
                     ITUNES_BOTTOM_BOTTOM)),
        (0, y),
    )
    window.paste(
        itunes.crop((right - 14, ITUNES_BOTTOM_TOP, right,
                     ITUNES_BOTTOM_BOTTOM)),
        (width - 14, y),
    )
    return window


def compose_itunes_selection(itunes: Image.Image, width: int) -> Image.Image:
    """The real iTunes track-selection gradient."""
    return _tile_h(
        _column(itunes, 600, ITUNES_SELECTION_TOP, ITUNES_ROW_HEIGHT), width
    )


__all__ = [
    "compose_itunes_selection",
    "compose_itunes_window",
    "ITUNES_BODY_Y",
    "ITUNES_BODY_H",
    "ITUNES_ROW_HEIGHT",
    "ITUNES_SOURCE_W",
    "WINDOW",
    "WindowGeometry",
    "compose_dock",
    "compose_apple_highlight",
    "compose_scroller_thumb",
    "compose_scroller_track",
    "SCROLLER_WIDTH",
    "compose_menu_panel",
    "compose_menu_selection",
    "compose_menubar",
    "compose_selection",
    "compose_wallpaper",
    "compose_title_bar",
    "compose_window",
    "crop_apple_glyph",
    "DOCK_HEIGHT",
    "LCD_H",
    "LCD_W",
    "MENUBAR_HEIGHT",
]

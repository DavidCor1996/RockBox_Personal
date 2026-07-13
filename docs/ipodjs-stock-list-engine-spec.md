# iPodJS Stock-Style List Engine Specification

## Goals

The iPodJS UI must behave like one continuous hierarchy rather than a set of
pages. Click-wheel movement advances a stable selection through an absolute
item space; cache refills and database queries must never change the selected
item or expose a page boundary.

## Navigation contract

- Lists scroll one row at a time and keep the selection visible.
- The viewport follows the selection by one row once it reaches its lower or
  upper focus margin.
- Lists do not wrap at either end.
- iPodJS ignores Rockbox's paginated-list preference.
- Repeated wheel actions may be coalesced for drawing, but every action updates
  the absolute selection.
- Returning from a child restores the parent's selection and viewport.
- Menu moves back one hierarchy level; Select opens the current item.

## Database window

Database lists use an absolute selected index and a sliding row window. The
window is refilled before the selection reaches either edge. A refill keeps the
same absolute selection and maps it to a new local row; it never substitutes a
new page while retaining the old local index.

The implementation keeps enough rows before and after the selection for rapid
wheel input. It supports reverse refills and derives the scrollbar position
from the absolute index and the known or progressively discovered item count.

## Rendering

All native iPodJS menus share list geometry, focus-window calculation, endpoint
clamping, and selection-delta drawing. A normal wheel step redraws only the old
row, new row, and any independently changing preview region. Full redraws are
reserved for viewport shifts, theme changes, hold overlays, and screen entry.

## Memory and caches

- Database row storage is sized to the sliding window, not the maximum library.
- Mutually exclusive slideshow and static-menu preview buffers share storage.
- Album-art cache entries retain only decoded pixels. Resize/decode scratch is
  shared by all entries.
- Album grouping uses a bounded hash index instead of a quadratic linear scan.
- Cache misses remain negatively cached to avoid repeated filesystem probes.

## Text

Text fitting operates on UTF-8 codepoint boundaries, uses logarithmic fitting,
and appends an ellipsis when truncation is necessary. Cache identity includes
the complete source string rather than only its displayed prefix.

## Acceptance criteria

- Scrolling past database row 72 is indistinguishable from any other row.
- Rapid forward and reverse scrolling does not skip or repeat entries.
- No iPodJS list changes selection by a full page.
- Long non-ASCII titles never produce malformed UTF-8.
- The ipod6g and ipodvideo builds succeed.
- Static iPodJS cache memory decreases measurably in the ipod6g map.
- Database/Files playback and playlist start behavior remain unchanged.

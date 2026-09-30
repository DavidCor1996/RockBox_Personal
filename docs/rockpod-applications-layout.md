# Applications preview and icon order

RockPod's Applications > Applications page offers Arrange icons (wiggle mode),
drag-and-drop ordering, Ctrl+arrow moves, Default order, and Save to iPod.
Drafts do not write to the device. Saving writes only the application order;
it does not change firmware slots, music, databases, or settings.

The iPodJS Applications grid and list, including the dock view, read
`/.rockbox/ipodjs/applications-order.txt` when entering Applications. The file
contains one stable icon basename per line (for example `maps`, `weather`,
`clock`), without an extension. The parser reads at most 64 records. It ignores
unknown and duplicate IDs, then appends omitted apps in their original order.
Absent files retain the default order. Visibility rules still apply. Desktop
Mode remains dock-only on Classic hardware. The Rockbox entry retains its
upstream-only launch contract.

RockPod retains hidden entries in their saved positions. The editor supports
Classic and Video targets, four icons per row, twelve per page, and preserves
an unsaved draft when refreshing the same connected device. A different device
loads its own layout. The save is atomic and the written file is read back.

The animated right pane uses six native compiled 80x80 bitmaps, copied from
the existing glossy application artwork. All icons exist on its first frame;
selecting Applications bypasses the delayed media-preview path. There is no
image decode, file access, cache acquisition or playback allocation on reveal.
The native RGB565 images occupy 76,800 read-only bytes; order indices add 32
bytes of static state. There is no added framebuffer. Borderless clipping and
the existing tick-based, bounded animation cadence are unchanged.

Validation includes RockPod editor/save tests and an executable harness using
the actual firmware order parser. A simulator run with the runtime 80px icons
removed showed the full pane in 13 ms and opened the grid in the order saved
by RockPod (Maps, Weather, Clock). Host descriptors were stable across the
Applications visit. These checks do not replace the playback/navigation and
physical-device gates in the UI memory steering document.

Classic and Video native builds and the Classic simulator build pass. The
RockPod focused/editor and related UI suites pass (53 tests). The standard
playback navigation trace also passes using an AIFF track; the trace validator
now accepts AIFF alongside MP3 and FLAC without relaxing lifecycle checks.

Ten full-depth navigation cycles returned to the initial descriptor count.
Twenty rapid Albums/Artists cycles completed through simulator button gates,
with stable playback identity, monotonic elapsed time, no database failures,
and matching initial/final Home resource counts. The aggregate stress gate
still rejected one 46-tick transition against its 35-tick limit. The new
firmware was initially withheld; the timing gate remains outstanding.

Following the user's explicit deployment request after that disclosure, the
tested Classic build was deployed to both firmware locations and synced.
Both copies matched SHA256
`5eea12d71ee994fdf7a7d781ff75af49b248d928b4c3a3b2f4c249c8b0f3afc0`.
All 11 database files, 2,929 indexed tracks, and settings were preserved and
verified; database autoupdate remains enabled. The device remains mounted
for arranging icons in RockPod before safe eject and restart.

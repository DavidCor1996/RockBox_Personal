# DIRECTV TV guide and interactive Featured

DIRECTV retains its guide model, six rows, programme spans, tuning, parental
filter, reminders, options, handheld rendering and audio lifecycle. In TV-safe
mode only, it also sends synchronous drawing commands to the existing core TV
canvas. Text uses the resident Apple font with bounded clipping/wrapping; the
original DIRECTV palette and cached logos remain. Receiver chrome fills the
TV edge to edge; overscan insets apply to text and controls without an outer
matte/border. The live picture is sampled
from the current decoded YUV frame, preserves display aspect and has a complete
border inset from the safe right edge. There are no new image caches, media
reads or decoder/audio changes. Subscreens return to the existing LCD mirror with a complete first refresh,
so a partial modal update cannot leave TV guide pixels in the old coordinates.

A guide paint owns a mutex until presentation. Preview frames skip a guide
paint in progress, use no retained decoder pointers, and stop owning TV when
normal playback, another screen or a release takes over. Output disappearing
between begin and present still unlocks the canvas. Plugin API 291 adds this
presentation call; all plugins must be rebuilt and deployed with the firmware.

Videos Home has category, Featured and application focus rows. Up/Down changes
rows; Left/Right browses Featured or applications; Select on Featured opens
Netflix with that show/movie selected. Normal Netflix hierarchy construction
and lock filtering remain authoritative. A removed/locked title falls back
to its normal category. No promotion starts playback automatically. Dots below
the banner indicate the current item. The label “Open Netflix” is not shown.

Featured keeps at most 32 unique banner IDs and titles (2,880 bytes), and reuses
the existing banner pixels. It scans at most 32 manifest rows and attempts at
most one bitmap in an idle service, with no queued input and a settle delay.
Unfocused promotions rotate every eight seconds; focused promotions stay put.
Focus changes do not decode, allocate or access storage. The connected library
fits within the bound. No new synthetic brand assets are introduced.

Validation includes actual production Home/Featured logic, filtered Netflix
entry, ASan/UBSan TV rendering across both aspects/all overscan levels, PIG
border/ownership cleanup, existing dock and app remote tests, native Classic
and Video builds, simulator guide behavior and active-music navigation stress.
The guide gate's long-press PIN segment also fails with the previous guide
source compiled against the same simulator/API; this pre-existing test failure
is recorded separately and no PIN behavior is changed by this visual pass.
Physical TV appearance and dock controls still require the user's hardware test.

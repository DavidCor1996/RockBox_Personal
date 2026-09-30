# RockPod Notification Center Specification

## Product Decision And Feasibility

Implement a target-scoped notification system for the 320x240 iPodJS builds.
It has two related surfaces:

1. a non-modal banner that may be composited above a core menu, WPS, or
   plugin without giving the notification system ownership of that screen;
2. a persistent Notification Center opened by tapping Left on the iPodJS Home
   dashboard.

This is feasible, but a banner that is genuinely visible above *every* plugin
cannot be implemented as another `splash()` call or as a root-menu drawing
helper. Plugins own the framebuffer and may use `lcd_update_rect()` or direct
YUV output independently of the application UI. The global form therefore
requires an iPod display-output compositor below both core and plugin drawing.
The history, event API, Home gesture, Achievements producer, and Sitekick
scheduler should be completed before enabling the global compositor.

The reference design is specifically iOS 5/6. iOS 3/4 push notifications were
center-screen modal alerts; iOS 5 introduced the noninterrupting top banner
and pull-down notification history. RockPod should borrow the visual language,
not pretend that a click-wheel has a touch pull-down gesture.

## Scope

The first release supports:

- local achievement unlocks;
- optional Sitekick Chip Dump ready and Chip Shop refreshed events;
- Sitekick chip rescued, bought, or inbox-granted events;
- a bounded recent-notification history;
- per-source settings and a global banner switch;
- source-specific routing when a history item is opened;
- iPod Classic 6G/7G, iPod Video 5G/5.5G, and their simulators.

It does not add networking, push delivery, badges on arbitrary menu entries,
notification actions inside a transient banner, arbitrary plugin-defined
artwork, vibration, or a background scan of Achievements/Sitekick files.

## User Experience

### Incoming banner

An incoming notification appears at the top of the current screen for three
seconds. It does not pause, stop, dim, or redirect the current activity. New
events queue behind the visible event and duplicates coalesce.

For a 320x240 display, use a 320x42 pixel banner:

- one-pixel black top/bottom keylines;
- a dark graphite vertical gradient with a subtle highlight at the top;
- a 28x28 source icon at x=7, y=7;
- a bold 12-14 pixel title and one compact body line;
- white title text, light-grey body text, and a small right-aligned age only
  when it fits;
- square geometry with restrained 3-pixel corner rounding, matching the
  pre-iOS-7 stock aesthetic rather than modern floating notification pills.

The entrance is a 120 ms top-down reveal and the exit is a 160 ms upward
reveal. Position is derived from elapsed ticks. Frames are capped at 20 fps,
may be dropped, and never busy-wait. Animation is disabled when reduced motion
is enabled; the same three-second dwell remains.

The banner consumes no input. Center, Menu, wheel, Left, Right, Play, Hold,
USB, and shutdown continue to belong to the foreground activity. Opening an
event is done from Notification Center, not by overloading a foreground
plugin's controls.

### Notification Center

Tap Left on the iPodJS Home dashboard to open the center.

The 320x240 sheet uses the original iOS 5 capture-derived linen and section
surfaces in `assets/ipodjs/rockbox/notifications`. Section strips retain their
24-pixel native height; two 84-pixel notification groups fit beneath the
24-pixel title. No gradient cards, empty-state panel, drawn unread blobs, or
imitation pull handle cover the linen. A narrow focus rule supports the wheel.

Text uses Apple's original Helvetica and HelveticaBold outlines from the
verified private RetailOS extraction: 13-point regular and 15-point bold,
rasterized at 60 dpi for this display. These are Apple iPod font sources,
not claimed to be an extracted iOS system font. See the private font
`provenance.json` for source hashes. Title/body clipping preserves the age
column and screen bounds. Fonts share existing caches where possible and
are prepared before playback, never allocated on a late visit during audio.
The change adds no framebuffer or static bitmap storage.

| Input | Action |
| --- | --- |
| Wheel | Move through notifications |
| Select | Open the selected source/route and mark it read |
| Left or Menu | Return to Home |
| Hold Select | Show `Clear all notifications?` and `Select = yes` |
| Fresh Select in confirmation | Clear the complete notification history |
| Left or Menu in confirmation | Cancel and return to the unchanged history |

Confirmation remains on the linen sheet, with one confirmation action and a
`Menu to cancel` footer. The initial hold release and wheel cannot accept it.
The empty sheet says `No Notifications`. Hold, USB, charging, shutdown, and
playback events continue through the existing system-event handler.

## Settings

Add these iPodJS settings, all independent of themes:

| Setting | Default | Meaning |
| --- | --- | --- |
| Notifications | On | Store events and expose Notification Center |
| Notification Banners | On | Show transient overlays |
| Achievement Notifications | On | Accept achievement events |
| Sitekick Notifications | Off | Accept scheduled and acquired-chip events |
| Notification Sound | Off | Play one bounded UI cue for a newly visible banner |

Turning banners off keeps history. Turning a source off prevents new history
records from that source and cancels its pending schedules; it does not erase
old records. The sound uses an existing short UI side channel and must not take
the shared audio buffer, mutate PCM playback ownership, or load audio from a
banner draw/flush path.

## Core Architecture

### Notification manager

Add:

```text
apps/notification_manager.c
apps/notification_manager.h
apps/gui/notification_center.c
apps/gui/notification_center.h
```

Compile the feature only for `HAVE_IPODJS_UI && HAVE_LCD_COLOR &&
LCD_WIDTH == 320 && LCD_HEIGHT == 240` initially.

The manager owns:

- a fixed 24-record history ring;
- an eight-event transient queue;
- up to eight keyed schedules;
- one bounded worker queue and target-audited worker stack;
- unread count and monotonic sequence;
- source settings and deduplication;
- one fixed banner pixel strip;
- persistence dirty state.

No manager allocation may use `core_alloc()`, `plugin_get_audio_buffer()`, the
playback arena, or a full-screen framebuffer. Use fixed target-scoped BSS.
A 320x42 RGB565 banner costs 26,880 bytes. Records, queues, schedules, strings,
and synchronization must stay below 12 KiB, making the initial total budget no
more than 39 KiB BSS. Record actual before/after ELF text/data/BSS sizes.

### Posting API

Expose a copy-in API to core and, at the end of `struct plugin_api`, to
plugins. Adding the plugin entry requires a plugin API version bump and a full
firmware/plugin rebuild.

```c
enum notification_source {
    NOTIFICATION_SOURCE_ACHIEVEMENTS,
    NOTIFICATION_SOURCE_SITEKICK,
    NOTIFICATION_SOURCE_SYSTEM,
};

enum notification_kind {
    NOTIFICATION_ACHIEVEMENT_UNLOCKED,
    NOTIFICATION_SITEKICK_DUMP_READY,
    NOTIFICATION_SITEKICK_SHOP_READY,
    NOTIFICATION_SITEKICK_CHIP_ACQUIRED,
};

struct notification_request {
    unsigned source;
    unsigned kind;
    unsigned priority;
    uint32_t stable_id;
    long timestamp;
    char title[40];
    char body[88];
    char route[80];
};

bool notification_post(const struct notification_request *request);
bool notification_schedule(const struct notification_request *request,
                           long rtc_deadline);
void notification_cancel(unsigned source, unsigned kind,
                         uint32_t stable_id);
```

The functions copy all fields before returning; manager state never retains a
pointer into plugin memory. Text is UTF-8 validated, control characters are
removed, and overlong fields are truncated at codepoint boundaries. A stable
ID identifies the logical event and prevents replay across launch/reboot.

Posting only mutates bounded RAM and marks persistence dirty. It does no
catalog lookup, bitmap decode, font load, or filesystem I/O. Persistence is
flushed at explicit safe points: plugin exit, Home idle, clean shutdown, and
after Notification Center changes. A simulator-only forced-flush hook supports
crash/reboot tests.

### Scheduling

Schedules use RTC epoch seconds, not `current_tick`, so they survive sleep and
reboot. A small notification worker sleeps on a queue until the nearest
deadline, a new post, shutdown, or a settings change. The worker may update
manager state, rasterize the fixed banner strip, and publish or retire that
completed strip through the display-overlay contract. It must not scan source
files, touch tagcache, use the foreground viewport/font state, or write the
persistent snapshot while an arbitrary plugin is active. A tick callback, if
used to wake the worker, may only mark work due and post to its queue; it may
not open files, compose text, draw, call LCD APIs, or play sound.

Banner rasterization uses a tiny built-in fixed font and cached source glyphs,
not Rockbox's mutable foreground font/glyph cache. This keeps scheduled events
safe while a plugin owns the UI and prevents a notification from loading a
font or shrinking playback memory. Include the worker stack and glyph data in
the 12 KiB non-pixel budget; reduce history strings before exceeding it.

When a deadline becomes due, the manager converts the already-cached request
to a history event and banner. Missed deadlines coalesce on boot: one Dump
notification and one Shop notification are enough, regardless of elapsed
rotations.

Sitekick remains the authority for its random stock. Scheduling `Shop ready`
does not roll inventory, and scheduling `Dump ready` does not choose or award a
chip. Sitekick performs those operations on its next entry, as today. Whenever
Sitekick writes `sk_dump_ready_at` or `sk_shop_ready_at`, it updates the keyed
manager schedule. This avoids teaching core to parse `save.v1.dat` and avoids
racing Sitekick's atomic save.

### Persistence

Store an atomic, versioned snapshot at:

```text
/.rockbox/notifications/state.v1.dat
/.rockbox/notifications/state.v1.new
```

The header contains magic, version, record count, schedule count, next
sequence, and CRC32. Records contain only fixed-width scalar/string data—no
pointers, ticks, framebuffer state, or allocator handles. Write the `.new`
file, fsync/close it, and rename it over the live snapshot. A missing, old, or
bad-CRC file yields an empty center; it never blocks boot or requests a
database rebuild.

Do not append indefinitely. The snapshot always contains at most 24 history
records and eight schedules. Unread records evict oldest-read records first,
then the oldest unread record if the ring is completely unread.

## Display-Wide Overlay Compositor

The compositor is the feature's highest-risk portion and is required before
claiming banners work over every plugin.

Add a small display-overlay contract shared by:

- `firmware/target/arm/s5l8702/lcd-s5l8702.c` for iPod 6G;
- `firmware/target/arm/ipod/video/lcd-video.c` for iPod Video;
- the simulator LCD backend;
- RGB framebuffer updates, rectangle updates, and direct YUV blits.

The LCD driver must treat the foreground framebuffer/video as the base layer
and the manager's immutable 320x42 strip as the top layer. It must never draw
the banner into the foreground framebuffer. For every physical transfer that
intersects the banner:

1. capture/update the affected base pixels in a fixed base-strip cache;
2. blend or replace only the active banner rows in the driver's staging path;
3. transfer the composed rows under the existing LCD serialization;
4. leave the caller's framebuffer, viewport, font, colors, draw mode, and
   backdrop unchanged.

On 6G, compose into the existing DMA staging path before queueing DMA. On 5G,
use the fixed strip as the source for the banner rows instead of modifying the
BCM source framebuffer. Direct YUV transfers must update the base-strip cache
and then send the banner rows last. Expiry restores the cached base strip under
the LCD driver's normal lock, so a static plugin does not leave a stale banner
on the panel.

The manager publishes a completed strip plus generation number atomically.
The LCD path only reads cached pixels and timing state. It performs no text
layout, source lookup, event dispatch, file I/O, allocation, or sound. If the
overlay is unavailable or synchronization cannot be obtained safely, flush
the base frame and omit the decorative banner.

Do not implement this by globally wrapping `lcd_update()` alone. That misses
rectangle updates, YUV video, target-specific panel paths, and plugins that
retain the existing API function pointers.

## Event Producers

### Achievements

The current achievements browser discovers some local unlocks while loading a
game's achievement list, while shared launcher telemetry records session totals
only after a plugin returns. Refactor the unlock edge—not the Achievements
screen renderer—into a bounded core helper.

After `rockachievements_record_session()` atomically records the completed
session, evaluate only the just-finished known target against its catalog and
known-unlock state. For each new edge:

1. preserve the existing `unlocks.v1.tsv` and `events.v1.tsv` contracts;
2. post one notification using a stable hash of game key + achievement ID;
3. never replay baseline/imported unlocks as new banners;
4. let the existing Achievements plugin show the same unlocked state.

Version one therefore shows time/session-derived achievements immediately
after returning from the relevant plugin. A game that has a genuine runtime
achievement hook may call `notification_post()` directly later; polling a game
or catalog during play is out of scope.

### Sitekick

Post acquired-chip events only after ownership and the atomic Sitekick save
succeed. Use the chip ID as part of the stable ID and include its authored name
and rarity in the body.

- `sk_dump_claim()`: post `Chip rescued` and reschedule Dump readiness.
- `sk_shop_buy()`: post `Chip purchased`; keep the existing shop deadline.
- `sk_apply_inbox()`: after a successful save, post one summary event rather
  than one banner per grant; include the applied count.
- `sk_dump_service()`: when it creates immediately available stock, cancel
  any matching ready schedule. Do not post a second availability event if the
  schedule already fired.
- `sk_shop_rotate()`: cancel the fulfilled schedule and register the next
  shop deadline.

If Sitekick notifications are disabled, these calls are cheap no-ops and its
save/gameplay behavior remains identical.

## Input Integration

Do not infer a long Left from `button_queue_count()`. Add a dedicated iPodJS
Home action context or `get_action_custom()` mapping:

- Left press: `ACTION_NONE` while the hold decision is pending;
- Left release before 700 ms: existing Home back action;
- Left repeat after 700 ms: one-shot open Notification Center;
- release after the one-shot: consume;
- Hold switch or system event during the decision: cancel it.

This isolates the gesture from `ACTION_TREE_ROOT_INIT`, list page-left, WPS
seek, and plugin-specific Left mappings. It applies only inside
`root_menu_video_dashboard()`; holding Left elsewhere never opens the center.

## Rendering And Resource Rules

- Banner and center draw functions paint cached pixels/metadata only.
- Source icons are compiled monochrome/vector-style primitives or fixed tiny
  built-in bitmaps. Do not open BMP files for an incoming banner.
- Use the already-loaded UI font. Do not call ordinary `font_load()` from a
  notification or playback-adjacent entry path.
- No framebuffer snapshot, `core_alloc()`, tagcache query, album-art claim,
  playlist operation, audio stop/restart, PCM/mixer change, or plugin audio
  buffer ownership is permitted.
- A banner never delays Hold, USB, shutdown, or foreground input.
- Notification Center file reads happen once at manager initialization or its
  explicit screen-entry service point, never per row or per frame.

## Delivery Plan

### Phase 1: event and history foundation

- manager, fixed records, dedupe, settings, persistence, and simulator tests;
- Achievements post-return unlock producer;
- Sitekick acquisition producers and persisted readiness schedules;
- Notification Center screen and Home Left-hold gesture;
- banners limited to cooperative iPodJS screens while validating semantics.

### Phase 2: global compositor

- simulator output compositor and deterministic frame tests;
- 6G RGB/rectangle compositor;
- 5G RGB/rectangle compositor;
- direct YUV integration and static-screen expiry;
- enable banners over all activities only after every output path passes.

### Phase 3: polish

- source deep links;
- optional existing UI cue;
- additional carefully reviewed system producers such as charging completion;
- accessibility/reduced-motion refinements.

Do not ship Phase 1 cooperative banners under wording that promises global
plugin coverage. History and scheduling may ship independently while the
global banner setting remains hidden or labelled experimental.

## Acceptance Tests

### Functional simulator tests

- Hold Left on Home for 700 ms opens Notification Center exactly once; short
  Left preserves existing behavior.
- Achievement threshold crossing produces one post-return record and never
  replays it after reboot or reopening Achievements.
- Sitekick Dump and Shop deadlines survive reboot, coalesce missed periods,
  and never award or roll a chip outside Sitekick.
- Dump rescue, Shop purchase, and multi-chip inbox grant post only after a
  successful save.
- Disabled source settings produce neither history nor banner.
- Twenty-five posts preserve the documented 24-record eviction order.
- CRC corruption yields an empty center without boot failure.
- Clear one and Clear All persist across restart.
- Route fallback is safe when the source plugin or catalog entry is missing.

### Overlay tests

- Banner entrance/dwell/exit pixels are deterministic at elapsed-tick
  checkpoints and never depend on delivered frame count.
- Full, partial, and banner-intersecting rectangle updates preserve both the
  foreground framebuffer checksum and LCD base-strip checksum.
- Static plugins clear the banner at expiry without a stale strip.
- MPEG/YUV video continues beneath the banner and is restored correctly.
- Rapid 20 fps game/video redraws cannot tear the manager strip or retain a
  plugin-memory pointer.
- Foreground viewport, font, draw mode, colors, backdrop, and framebuffer are
  byte-identical before and after a banner flush.

### Playback and resource gates

- simulator and native iPod 6G and iPod Video builds pass;
- active Database and Files playback continue across banner posting, center
  entry/exit, plugin entry/exit, and Sitekick/Achievements routes;
- playlist identity, elapsed time, mixer frequency, codec state, and audio
  buffer size do not change because of a notification;
- BSS delta stays within the stated 39 KiB cap and no core allocatable-memory
  trend appears across 100 posts;
- file descriptor count returns to baseline after persistence, routing, and
  repeated center entry;
- `tools/ipodjs_navigation_sim_regression.sh` passes with notifications queued;
- hardware tests cover 6G and 5G RGB menus, WPS, Sitekick, Achievements, a
  rapidly redrawing game, MPEG video, Hold, USB, and shutdown.

## Risks And Stop Conditions

- If a 5G or 6G display path cannot restore the base strip without mutating the
  foreground framebuffer, keep global banners disabled on that path.
- If LCD expiry work can race a foreground draw without using the existing
  target serialization, do not solve it with a second UI thread drawing into
  the framebuffer.
- If the fixed BSS budget causes native memory pressure, reduce banner height,
  history depth, and string widths before considering playback/core memory.
- If achievement evaluation needs broad catalog scans at plugin return,
  pre-index the bounded per-target rules at an explicit service point; never
  move the scan into drawing or LCD code.
- Notification failure is always cosmetic. It must not roll back an
  achievement, Sitekick purchase/rescue, plugin exit, playback action, or
  system event.

## Historical Design References

- Apple, “New Version of iOS Includes Notification Center, iMessage,
  Newsstand, Twitter Integration Among 200 New Features,” 2011:
  <https://www.apple.com/newsroom/2011/06/06New-Version-of-iOS-Includes-Notification-Center-iMessage-Newsstand-Twitter-Integration-Among-200-New-Features/>
- Macworld, “Up close with iOS 5: Notification improvements,” 2011:
  <https://www.macworld.com/article/214742/ios_5_notification_improvements.html>
- Ars Technica, “iOS 5 reviewed: Notifications, iMessages, and iCloud, oh
  my!,” 2011:
  <https://arstechnica.com/gadgets/2011/10/ios-5-reviewed-notifications-imessages-and-icloud-oh-my/4/>

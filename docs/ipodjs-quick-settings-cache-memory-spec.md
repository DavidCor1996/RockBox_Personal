# iPodJS Quick Settings Cache and Memory Safety Specification

## Decision

Add a final **Cache & Memory** action to the iPodJS Quick Settings screen. It
opens a small maintenance screen with these choices:

1. **Refresh UI Cache** -- invalidate disposable iPodJS display caches without
   stopping playback, changing settings, deleting files, or resetting Rockbox.
2. **Restart to Clear RAM** -- use Rockbox's normal graceful reboot path after
   an explicit confirmation. This is the only option described as clearing all
   volatile memory.
3. **Back** -- return without making a change.

Do not add a generic "free memory" operation. Rockbox has no safe global
garbage collector, and unrelated buffers have different owners and lifecycle
rules. In particular, the action must never seize, shrink, clear, or release
the shared audio buffer.

## User Contract

### Refresh UI Cache

The action means:

- forget decoded iPodJS images and cached lookup results;
- force changed or previously missing iPodJS assets to be checked again;
- rebuild affected UI data lazily on the next draw;
- keep the current track, playback position, pause state, playlist, volume,
  repeat/shuffle state, and all settings unchanged;
- keep the on-disk music database, directory cache files, thumbnails, photo
  previews, weather data, themes, and game assets unchanged.

The completion message must say **UI cache refreshed**. It must not report
memory as freed when the invalidated storage is a fixed static array.

### Restart to Clear RAM

The action means:

- ask for a second confirmation;
- use the same charger guard and `sys_reboot()` shutdown path as the existing
  root power menu;
- save any settings/status edits already made in the parent Quick Settings
  session before requesting the reboot;
- allow the normal shutdown event handling to save state, finish database
  work, flush storage, stop audio, and reboot;
- never call `system_reboot()` directly from the Quick Settings handler.

The confirmation copy should be:

```text
Restart Rockbox?
Clears RAM; music will stop.
Select: Restart   Menu: Cancel
```

This is a volatile reset only. It does not erase persistent cache files or
user data.

## Why The Split Is Required

The current iPodJS caches in `apps/root_menu.c` and
`apps/gui/ipodjs_ui.c` are mostly fixed-size static storage. Invalidating them
refreshes their content but does not return their `.bss` memory to the core
allocator.

The current iPod 6G object provides a useful scale reference:

| Storage | Current size | Reclaimable by invalidation |
| --- | ---: | --- |
| Preview bitmap union | about 401 KiB | No; static storage |
| Preview source lists | about 98 KiB | No; static storage |
| Current preview paths | about 16 KiB | No; static storage |
| iPodJS label cache | about 9 KiB | No; static storage |
| Quick Settings icons | about 7 KiB | No; static storage |
| All `root_menu.o` BSS | about 669 KiB | No; not as one owned cache |

These figures are implementation diagnostics, not a guaranteed ABI or UI
value. They explain why **Refresh UI Cache** and **Restart to Clear RAM** must
not be presented as equivalent operations.

## Quick Settings Layout

### Entry row

Append this enum item after `IPODJS_QS_HOLD`:

```c
IPODJS_QS_CACHE_MEMORY,
```

Row presentation:

```text
Cache & Memory                                      Open >
```

Use a dedicated 18x18 maintenance/cache bitmap under
`/.rockbox/ipodjs/qs/`. If the asset is missing or cannot be decoded, draw the
row without an icon; the action must remain usable.

This is an action row, not a setting. Left/right must not activate it. Select
opens the maintenance screen. It must never set `changed_settings` or
`changed_status`. Pass the parent's pending-change state into the maintenance
screen so a confirmed restart can persist earlier Quick Settings edits.

### Scrolling

Adding a row must not shrink every row below the existing icon and font
requirements. Replace `list_h / IPODJS_QS_COUNT` with a fixed safe row height:

```text
max(18, selected font height + 2)
```

Calculate a visible-row count from the area between the header and footer, and
keep the selected row in a scrolling window. On builds where all rows fit, the
screen remains visually unchanged. When they do not fit, show a narrow right
edge scroll indicator.

The Quick Settings selection should be restored after returning from the
maintenance screen.

### Maintenance screen

Use the existing iPodJS header, row, selection-gradient, font, dark-mode, hold
overlay, and system-event helpers. Do not invoke the stock themed menu between
native-screen enter/finish calls merely for these three rows.

Controls:

- wheel: move selection;
- Select: run/open the selected action;
- Menu: return to Quick Settings;
- Play/Pause: preserve the existing iPodJS Quick Settings playback behavior;
- hold switch: block actions and show the normal hold overlay;
- USB, charger, power, and other system events: pass through the existing
  iPodJS system-event path.

## Refresh Scope

Implement one centralized, idempotent invalidation function. It should clear
metadata and validity state, not waste time zeroing large pixel arrays that
are already inaccessible after invalidation.

### Invalidate

- iPodJS fitted-label entries and their LRU stamp through
  `ipodjs_ui_label_cache_reset()`;
- native iPodJS asset `tried` and `valid` flags, including light/dark mode
  synchronization state;
- Quick Settings icon `tried`, `valid`, and mode metadata;
- slideshow/menu preview slot validity, mode, victim index, stamp, failures,
  decode budget, and retry timestamps;
- preview source `loaded`, count, reload, and current-source state;
- video thumbnail lookup and bitmap validity plus replacement counters;
- iPodJS database row-art validity and replacement index on targets that use
  the native iPodJS database browser;
- native album-group cache validity on targets that use it;
- weather parse/icon validity and retry state so the next visit re-reads the
  existing local weather data;
- cached disk-free text/timestamp after that function's local static state is
  moved into an explicitly resettable state structure.

### Do not invalidate or release

- `root_menu_video_aa_slot` or any handle returned by
  `playback_current_aa_hid()`;
- the active WPS, codec, PCM, mixer, pcmbuf, or playback buffers;
- the current playlist or playlist directory-cache references;
- tagcache RAM state or `database*.tcd`/`tagcache*.tcd` files;
- Rockbox dircache state;
- plugin memory or the shared plugin/audio buffer;
- skin-engine allocations, backdrops, language data, talk/voice buffers, or
  unrelated Rockbox icon caches;
- loaded iPodJS fonts in the first version;
- any file or directory on storage.

Fonts are excluded because the current font IDs are held in function-local
static variables and the selected font is live while this screen is drawn.
Safe font reclamation requires a separate ownership refactor; it must not be
bolted onto this action.

## Required Execution Order

`Refresh UI Cache` runs synchronously on the main UI thread:

1. Wait for Select/Menu release and clear queued button repeats.
2. Refuse activation while hold is engaged.
3. Switch the current LCD font to `FONT_SYSFIXED` before resetting fitted-label
   metadata.
4. Invalidate cache metadata through the centralized helper.
5. Do not call `settings_save()`, `status_save()`, `audio_stop()`,
   `audio_flush_and_reload_tracks()`, `dircache_disable()`,
   `tagcache_shutdown()`, `core_alloc_maximum()`, or any plugin-buffer API.
6. Draw **UI cache refreshed** for approximately one second.
7. Redraw the maintenance screen. Reload only the assets required for that
   redraw; all other caches remain cold until used.

If an asset reload later fails, use the existing fallback rendering. A failed
reload must not turn the refresh action into an error or a reboot request.

## API Shape

Keep iPodJS-owned reset operations explicit rather than exposing a global
purge API. Suggested internal functions:

```c
static void root_menu_video_clear_disposable_caches(void);
static void root_menu_video_clear_preview_caches(void);
static void root_menu_video_clear_database_view_caches(void);
static void root_menu_video_clear_asset_caches(void);
static int root_menu_video_cache_memory_menu(void);
```

The public iPodJS UI surface only needs its existing label-cache reset entry
point unless font ownership is redesigned later.

The centralized helper is allowed to reset caches located earlier in
`root_menu.c`; add narrow helper functions near those owners rather than
exporting their storage or duplicating `memset()` calls in the Quick Settings
handler.

## Concurrency and Playback Invariants

Before and after `Refresh UI Cache`, all of these must remain identical:

- `audio_status()` play/pause bits;
- current track identity;
- elapsed position, allowing only normal wall-clock progress;
- playlist amount, current index, and order;
- mixer frequency and playback channel ownership;
- volume, repeat, and shuffle values.

Do not pre-stop playback, request the shared audio buffer, or release any
buffer whose callbacks may still be live. The refresh operation owns UI cache
metadata only.

If refresh is requested while a preview bitmap is being decoded, finish the
current decode/draw iteration first. In the current cooperative main-thread
path this is naturally serialized. If decoding moves to another thread later,
the cache owner must add a lock/generation protocol before this action may
clear it.

## Error and Power Handling

- Refresh is idempotent and succeeds even when every cache is already cold.
- Low memory during a later lazy reload uses existing no-image/text fallback.
- USB connection cancels the maintenance screen through the normal system
  event result; it does not continue into refresh or reboot.
- A reboot request while charging must follow the current power-menu policy.
- A reboot rejected because the tagcache is busy or charging policy blocks it
  returns through the existing splash/event behavior; it must not fall back to
  a direct hardware reset.
- Critical-battery behavior remains owned by the existing graceful shutdown
  path; the maintenance screen must not add a competing battery policy.
- No completion message may claim a number of KiB freed unless it is measured
  from an actually released core allocation.

## Real Runtime Reclamation: Deferred Design

Do not migrate the large static preview caches to `core_alloc()` merely to make
the button report freed RAM. Such a migration changes playback-buffer pressure,
pointer movement, decoding lifetime, and concurrent shrink behavior.

If runtime reclamation without reboot is pursued later, it is a separate
project with these gates:

- each allocation has one documented owner and a valid buflib handle;
- bitmap pointers are never retained across an unpin/yield;
- clear can free the handle only when no decoder or LCD draw references it;
- allocation failure leaves previews disabled rather than affecting audio;
- no `core_alloc_maximum()` call is used as a synthetic compaction command;
- memory pressure must not stop and restart active music;
- the UI reports actual before/after `core_available()` only for handles it
  released itself;
- both iPod Video 5G and iPod Classic 6G pass the playback transition matrix.

Until those gates are met, graceful restart is the safe full-memory-reset
operation.

## Test Plan

### Source and simulator checks

- Quick Settings still renders every row at a legible height with and without
  `HAVE_HARDWARE_CLICK`.
- The final row can be reached, selected, and returned from without changing a
  setting.
- Missing maintenance icon falls back to a text-only row.
- Refresh twice in a row succeeds.
- Light and dark assets reload in the correct mode.
- Replacing a preview/asset file while the simulator is running becomes
  visible after refresh.
- Missing-asset negative cache is cleared and retried once, without a tight
  disk-read loop.
- USB and charger events remain handled while the maintenance screen is open.
- AddressSanitizer/UBSan simulator runs show no stale bitmap/font pointer use.

### Playback matrix

Run Refresh UI Cache in each state:

- no playback;
- Database playback;
- Files playback;
- paused playback;
- Quick Settings opened from the dashboard;
- return to native iPodJS WPS after refresh;
- hold engaged/released around the confirmation;
- repeated rapid refresh/back/navigation cycles.

After each refresh, verify uninterrupted sound, correct current track and
playlist, working volume/play-pause controls, and successful transitions back
to Database and Files playback.

### Reboot checks

- cancel leaves playback and UI untouched;
- confirm uses graceful shutdown and restarts;
- resume state and saved settings survive reboot;
- database files remain present and readable;
- charging behavior matches the existing root power menu;
- simulator treats reboot using its established simulator behavior.

## Acceptance Criteria

The feature is complete only when:

- users can distinguish UI refresh from a full volatile-memory reset;
- Refresh UI Cache performs no storage deletion and no audio/playlist
  mutation;
- the UI never claims that static `.bss` was freed;
- Restart to Clear RAM uses the normal graceful power path;
- the extra Quick Settings row is legible and reachable on 320x240 targets;
- cache refresh is idempotent and safe during active or paused playback;
- all simulator and physical-device playback transitions pass without a
  freeze, playback restart, lost playlist, or silent codec state.

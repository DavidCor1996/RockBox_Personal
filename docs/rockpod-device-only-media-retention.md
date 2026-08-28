# RockPod device-only media retention

RockPod treats a successfully synced iPod copy as authoritative when its
laptop source is later removed. This makes it safe to reclaim laptop storage
without making the next sync erase media that remains on the iPod.

## Retention rules

| Content | After laptop source deletion | Required retained state |
| --- | --- | --- |
| Video library / Netflix | The library row, TV metadata, playback markers, lock flag, artwork relationship, and existing iPod MPEG remain | `~/.rockpod/library.db` and the linked iPod file |
| Locked video | Remains in `Videos/.Locked` and remains `locked=1` in the video manifest | Library database and existing iPod file |
| Photos | The mounted-device copy is rediscovered as `missing_source` and is not included in automatic removals | Existing iPod photo and RockPod profile |
| OnlyFans | Profile/post metadata and converted device photo/video outputs remain | `~/.rockpod` app metadata and existing iPod outputs |
| Instagram | Profile/post metadata, thumbnails, displays, and converted device video remain | `~/.rockpod` app metadata and existing iPod outputs |
| Live TV | A scheduled device MPEG is adopted when both the source and disposable laptop encode cache are gone | Saved lineup plus mounted iPod copy |
| Music | Unchanged: a missing laptop music source is removed from the local library | Keep laptop music sources |

## Safe cleanup sequence

1. Connect the iPod and complete one successful sync of the media to keep.
2. Verify the item plays on the iPod. For private video, verify it appears only
   after unlocking and its path is under `Videos/.Locked`.
3. Close RockPod before deleting laptop originals.
4. Delete only the chosen local video/photo sources. Do not delete
   `~/.rockpod`, `~/.rockpod/library.db`, app metadata, or the saved Live TV
   lineup. Do not use a **Remove from iPod** action.
5. Reopen RockPod and refresh the library. The status bar reports the number
   of retained device-only videos.
6. Mount the same iPod before syncing Photos, OnlyFans, Instagram, or Live TV,
   because those services verify or adopt their existing device outputs.
7. Inspect the sync summary before applying it. Device-only media must not
   appear in removals or source-missing errors.

Deleting or resetting the RockPod database removes the relationship that
proves a generic video is already on the iPod. Back up `~/.rockpod` before any
unrelated system cleanup.

# Test update: complete guide gestures and five Featured shows

Guide Select/Play now completes on remote button release, like the clickwheel center button. The guide consumes both halves before returning WATCH/TUNE, so playback cannot interpret a leftover release as Open Guide. Held Left still exits the guide; a fresh fullscreen Select/Play click still opens it.

Home scans its existing bounded 32-entry candidate pool on idle ticks, filtering to unlocked TV shows and deduplicating banner IDs. Once scanning completes it publishes up to five unique shows selected by a private Fisher-Yates shuffle. Movies are excluded. Each Home reset chooses afresh without reseeding or consuming music's RNG; the selection stays stable while browsing. The carousel never exposes the full candidate count during scanning.

Focused production tests cover paired Select/Play gestures, held Left, 20 randomized five-show selections, duplicate episodes, locked/invalid/missing art, focus/rotation and bounded I/O. Native build and installation only; broader testing remains deferred per the user's low-credit/testing preference. Leave the Classic mounted.

# Small fork fixes — September 2026

Implements the four first-batch recommendations in
[the fork audit](fork-low-risk-audit-2026-09.md).

| Change | Origin | Local integration |
| --- | --- | --- |
| Bound iAP title, artist, and album reply lengths | [Rockpod 77fe8391bf](https://github.com/nuxcodes/rockpod/commit/77fe8391bf) | Import only the three 63-byte clamps in `apps/iap/iap-lingo4.c`; include the terminator in the transmitted length. |
| Preserve the keymap picker's explicit file filter | [bahusoid 88bfc61f4d](https://github.com/bahusoid/rockbox/commit/88bfc61f4d) | Add `BROWSE_DIRFILTER`; omit unrelated debug text. |
| Trim trailing spaces in imported keymap fields | [bahusoid a73ca71d30](https://github.com/bahusoid/rockbox/commit/a73ca71d30) | Add an explicit `pbuf > pact` lower bound to the original trim loop. |
| Skip AAC elapsed-time division without a positive bitrate | [Rockboxd e6521d32b6](https://github.com/tsirysndr/rockboxd/commit/e6521d32b6) | Import the guard with a target-neutral comment; omit hosted package changes. |

The changes add no allocations, settings, or plugin API changes. They do not
change shared audio-buffer ownership, playlist state, PCM, or mixer behavior.
MP4 chapter validation remains deferred for separate error-path review.

## Validation

`python3 tools/tests/test_fork_lowrisk.py` compiles extracted production code
with undefined-behavior sanitization and checks:

- All three iAP reply types with null, empty, boundary-length, and 1024-byte
  tags, including transmitted lengths and NUL termination.
- Normal, padded, empty, all-space, and unterminated keymap fields.
- Zero and negative AAC bitrate preserving elapsed time, plus normal bitrate
  conversion and a zero offset relative to the first frame.

All three host tests pass. Running the same tests against the saved pre-change
sources makes all three fail, confirming the tests detect the original bugs.
These tests exercise the changed code paths, not an entire accessory transport,
keymap import session, or AAC decoder. Browser filter behavior was checked
against `apps/tree.c`: without `BROWSE_DIRFILTER`, it substitutes the user's
ordinary file filter for the picker's filter.

Incremental builds in the existing `build-upstream-small-20260912/` directories
passed for iPod 6G and iPod Video (`make -j8 bin codecs rocks`), including the
changed firmware, AAC codec, and keyremap plugin. The iPod 6G simulator default
build also passed. Builds emitted warnings in other code; these were not
warning-free builds. The three modified source files pass `git diff --check`.

Evidence is in `build-fork-lowrisk-20260912/`, including pre-change sources,
the source patch, regression output, build logs, and artifact SHA-256 hashes
in `artifacts.json`.

## Deployment

Deployed on 13 September 2026 UTC. A concurrent full composite-output deployment
held the shared device lock, so the original targeted update wrote nothing.
Its build already contained an identical iAP object, AAC codec, and keyremap
plugin. The iAP object predates that build's linked firmware. The prior small
string-search and option-formatting fixes also have identical compiled objects.

Preserved that composite firmware rather than replacing it. After the other
deployment released the lock, backed up and updated the remaining legacy
`.rockbox/rocks/keyremap.rock` copy. Both firmware locations match
`build-composite-personal/rockbox.ipod`, SHA-256
`beb8129bb9aec3c1fc8bf379106a417fa52b1c1c4003b3280130b0c22bce5830`.
The installed AAC codec and both keyremap plugin copies match the tested
binaries. All 11 database files and configuration were byte-identical across
the targeted update; all 2,929 indexed tracks were readable with existing
device paths, and database autoupdate remained enabled. Sync and post-sync
checksum checks passed. The iPod remains mounted.

See `build-fork-lowrisk-20260912/deployment-result.json` for the backup path
and verified checksums. No reboot or interactive hardware test was performed.
Remaining device checks are dock metadata replies with long tags, importing
a padded keymap with a restrictive browser filter, and ordinary AAC
playback/elapsed-time behavior.

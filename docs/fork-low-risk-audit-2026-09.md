# Rockbox fork audit — 12 September 2026

Scope: beneficial, small changes for this personal iPod 6G/Video tree.
This is a source review, not an implementation or hardware validation.
Public repository metadata and recent commit histories were inspected, and
shortlisted patches were compared with the current local source. Popularity
is a discovery signal, not evidence that a patch is safe.

## Forks surveyed

| Repository | Public stars at review | Assessment |
| --- | ---: | --- |
| [Rockboxd](https://github.com/tsirysndr/rockboxd) | 293 | Active; mostly hosted/network playback work, with one useful codec guard. |
| [Rockpod](https://github.com/nuxcodes/rockpod) | 190 | Closest fit; much of its headline functionality is already in this tree. |
| [Rockbox Y1](https://github.com/rockbox-y1/rockbox) | 84 | Recent history mostly upstream merges and target-specific work. |
| [IncognitoMan](https://github.com/IncognitoMan/rockbox) | 51 | No additional compelling small iPod-specific fix identified in the inspected history. |
| [bahusoid](https://github.com/bahusoid/rockbox) | 10 | Smaller development fork; useful isolated keymap fixes among larger/WIP changes. |
| [PodBox](https://github.com/anthonyfletcher/podbox) | 7 | Emerging iPod fork; useful parser hardening mixed with larger changes. |

Olsro and older forks were also checked. The UI/UX Overhaul repository could
not be retrieved at review time, so its implementation is not assessed here.
This survey is not an exhaustive review of every branch or historical patch.

## Recommended first batch

All four fixes below are missing in the current local source. Their low-risk
rating assumes importing only the identified code, with focused validation.

1. **Clamp long iAP title, artist, and album replies.**
   [Rockpod 77fe8391bf](https://github.com/nuxcodes/rockpod/commit/77fe8391bf)
   adds three length clamps in `apps/iap/iap-lingo4.c`. The existing code uses
   the full source length returned by `strlcpy` as the outgoing packet length,
   even though only 63 text bytes fit. Long tags can therefore cause a reply
   to read beyond its stack buffer. Import those three clamps independently;
   the same commit's database-request changes depend on other accessory work.
   Verify empty, 63-byte, 64-byte, and much longer tags, and dock metadata replies.

2. **Honor the keymap import picker's explicit file filter.**
   [bahusoid 88bfc61f4d](https://github.com/bahusoid/rockbox/commit/88bfc61f4d)
   adds `BROWSE_DIRFILTER` to the picker in `apps/plugins/keyremap.c`, making
   its `SHOW_ALL` selection effective. This prevents the ordinary browser
   filter from hiding an import file. Import only the flag change, omitting
   the unrelated commented debug line. Verify with a restrictive browser filter.

3. **Accept spaces before a keymap prebutton field's closing brace.**
   [bahusoid a73ca71d30](https://github.com/bahusoid/rockbox/commit/a73ca71d30)
   trims trailing spaces in the text keymap parser. Useful for hand-edited
   imports; confined to the keyremap plugin. Verify normal, padded, empty,
   and malformed fields, including bounds behavior.

4. **Guard AAC elapsed-time calculation when bitrate is unknown.**
   [Rockboxd e6521d32b6](https://github.com/tsirysndr/rockboxd/commit/e6521d32b6)
   avoids division by zero in `lib/rbcodec/codecs/aac_bsf.c` by skipping the
   calculation for nonpositive bitrate. The reported failure concerns its
   hosted ADTS streaming path; normal iPod file metadata usually supplies a
   bitrate, so an ordinary local-file crash has not been demonstrated here.
   Port the guard and an appropriate generic comment, not the hosted-package
   changes or the assumption that a host tracks elapsed time. Verify zero and
   valid bitrate paths and ordinary AAC playback.

## Useful second pass

**Validate MP4 chapter atom sizes and reads** from
[PodBox 92c37179d0](https://github.com/anthonyfletcher/podbox/commit/92c37179d0).
The local `lib/rbcodec/metadata/mp4.c` subtracts from an unsigned atom size
without first checking the chapter header fits, and ignores read results.
The fix addresses malformed/truncated M4A metadata. However, the proposed
`break` error paths interact with the enclosing loop's remaining-size seek;
adapt them to fail cleanly and test truncated atoms rather than assuming
the patch is correct as written. Exclude its unrelated USB and tagcache edits.

## Defer or exclude

- Rockpod's wider iAP receive/reset/index/authentication changes need a
  coherent transport audit: local prerequisites differ and subsequent commits
  correct earlier behavior. The large accessory overhaul is not a small import.
- PodBox playlist locking, database recursion limits, and USB transfer checks
  affect custom lifecycle behavior or hardware. A hard recursion cap can skip
  valid deep folders. These need separate design and regression coverage.
- Rockboxd's larger MP4/streaming changes and PodBox's artwork/soundscan features
  are outside a low-risk patch batch for this tree.
- Rockpod's blank-list wake fix and bahusoid's plugin status-bar title cleanup
  are already present locally. Existing SSD, MFi, CoverFlow, and dynamic-color
  functionality should not be counted as new wins.

Evidence snapshots are in `build-fork-audit-20260912/`. No firmware source was
changed, built, deployed, or hardware-tested as part of this audit.

# Small upstream fixes, September 2026

## Scope and implementation

Implement the two small, low-risk recommendations first, preserving the
previously integrated batch and local customizations:

- [511d4dd90b](https://github.com/Rockbox/rockbox/commit/511d4dd90b): use
  unsigned byte pointers in the size-optimized `strcasestr` implementation.
  This prevents sign extension from breaking UTF-8 substring matching on
  signed-char builds. ASCII case-insensitive matching remains supported;
  Unicode case folding is not added.
- [f2985dc8a2](https://github.com/Rockbox/rockbox/commit/f2985dc8a2): omit the
  separator space when formatting integer/table settings with no unit.
  Retain nonempty units, custom formatters, and time-setting formatting.

Both upstream patches applied without adaptation. The production diff is two
files, ten added lines and three removed lines. There are no new allocations,
playback calls, settings fields, plugin API changes, or artwork operations.
Playlist Single Mode, cuesheet accessibility, reproducible packaging, and the
larger hardware/DSP features remain separate work.

## Validation

- Fresh native iPod 6G and iPod Video firmware builds succeeded.
- A fresh iPod 6G simulator build succeeded.
- Neither changed source file produced compiler warnings in those builds.
- Two focused behavioral tests passed under undefined-behavior sanitization:
  the actual Rockbox search implementation and ctype tables, built with both
  signed and unsigned char; and the extracted integer/table formatting path.
- Search cases cover ASCII case matching, UTF-8 leading/continuation bytes,
  empty and missing needles, and unchanged non-ASCII case behavior.
- Formatting covers null/empty/nonempty units, negative/zero values, bounded
  output including zero capacity, custom formatters, and time settings.
- Both tests detect the pre-port bugs using saved source files: formatting
  fails for the stray space; search fails under signed char. Unsigned-char
  search already passed before the fix.
- Task-only reverse patch and whitespace checks passed.
- Relative to the previously deployed 6G build, ELF text grows by 96 bytes;
  data and BSS are unchanged. No additional static RAM is required.

Run `python3 tools/tests/test_upstream_small_fixes.py -v` to repeat the tests.
Builds, pre-port copies, the task-only patch, and test logs are retained under
`build-upstream-small-20260912/`. This batch has not been deployed or committed.

# Quick Settings icons and TV backlight policy

Every Quick Settings row now uses a built-in icon from one Classic-style
silver-and-blue glass family. The symbols are original artwork, not extracted
Apple resources. Volume, brightness, dark mode, shuffle, repeat, accent,
surface, density, font, extras, haptics, hold, USB, Bluetooth, composite output,
TV backlight, cache/memory and time/battery each have a distinct symbol.

The fixed glyphs use 504 bytes of read-only data. The shared renderer draws
bounded 18x18 icons without allocation, file access, decoding, or theme files.
The previous per-row bitmap cache and idle icon loader are removed. Selected,
light and dark rows retain a contrasting rim and white symbols on blue glass.
Generate the header with `python3 tools/generate_ipodjs_qs_icons.py`.

On the Classic target, TV Backlight Off defaults to On. It is available in
Display / LCD Settings and Quick Settings, with `videoout backlight off: on`
in configuration files. Actual composite activity is queued to the backlight
thread independently of the persistent preference. While both are active,
normal button, charger and timeout wake requests cannot relight the backlight.
Stopping composite output or disabling the preference restores the ordinary
backlight timeout, including respecting an always-off preference. Auto mode
without active output does not suppress the backlight. The LCD/SVID controller
remains available through the existing target LCD sleep guard.

Validation includes native Classic and simulator firmware builds, execution
of the real backlight setter/event/state code against a fake hardware backend,
and simulator visual/persistence checks. The full navigation gate was attempted
but the existing simdisk contains recursively nested `.rockbox` directories in
its previews tree, preventing fixture setup. Physical dock, video playback,
and backlight qualification remain outstanding. No firmware was deployed.

The host lacks pytest and the PDF manual toolchain; the focused Python test
functions were invoked directly. Display/configuration manual sources were
updated, but a rendered manual was not built.

The icon coverage/render test compiles the production C renderer with every
conditional Quick Settings item enabled. It checks all 18 mappings, unique
nonempty glyphs, drawing bounds, and light/dark selected/unselected cases.
Seven focused tests passed, including the existing cache/memory checks.
Simulator captures are in `docs/ipodjs-quick-settings-proof/`.

# ZeroSlackr UI Audit

This audit covers the currently available ZeroSlackr/ZeroLauncher material in this repo and the extracted runtime now staged for the iPod.

## Summary

ZeroSlackr's visual shell is `ZeroLauncher`, a custom `podzilla2` build.
The repo does **not** contain the full upstream `podzilla2` core source checkout.
Instead it contains:

- local build glue and add-ons in `tmp/ProjectZeroSlackr-SVN/base/ZeroLauncher/`
- patch files against upstream `podzilla2` core under `tmp/ProjectZeroSlackr-SVN/base/ZeroLauncher/src/patches/`
- a compiled runtime in the extracted release payload under `/tmp/zeroslackr-release/ZeroSlackr/opt/Base/ZeroLauncher/`

That means:

- visual assets, schemes, fonts, configs, and launch scripts are directly inspectable now
- menu geometry and low-level shell drawing logic are only visible through patch files until upstream `podzilla2` is fetched again

## Where Menus Are Drawn

Primary menu bootstrap and group structure:

- `tmp/ProjectZeroSlackr-SVN/base/ZeroLauncher/src/patches/core-menu.c.patch`

This patch shows that ZeroLauncher replaces stock `podzilla` top-level groups with:

- `/Music`
- `/Now Playing`
- `/Emulators`
- `/Media`
- `/Tools`
- `/Zillae`
- `/Settings`
- `/~Power`

The same patch also confirms that menu windows are normal `ttk` windows and still use the upstream menu widget:

- `ttk_add_widget(ret, menu_wid);`
- `ttk_window_title(ret, "ZeroLauncher");`

Implication:

- the first real layout rewrite for an Apple Classic shell will eventually need upstream `core/menu.c`, not just runtime asset swaps

## Where Fonts Are Loaded

Font loading is patched in:

- `tmp/ProjectZeroSlackr-SVN/base/ZeroLauncher/src/patches/core-pz.c.patch`

Relevant lines:

- text font loaded via `pz_load_font(&ttk_textfont, "Espy Sans", TEXT_FONT, ...)`
- menu font loaded via `pz_load_font(&ttk_menufont, "Snap", MENU_FONT, ...)`

Runtime font assets live in:

- `/tmp/zeroslackr-release/ZeroSlackr/usr/share/fonts/`

ZeroSlackr already has a broad font pool there, so a later runtime profile switch can likely reuse existing font-selection machinery once `zerolauncher.conf` handling is understood or rebuilt.

## Where Colors / Backgrounds / Decorations Are Defined

Default appearance settings are patched in:

- `tmp/ProjectZeroSlackr-SVN/base/ZeroLauncher/src/patches/core-pz.c.patch`
- `tmp/ProjectZeroSlackr-SVN/base/ZeroLauncher/src/patches/core-header.c.patch`

Important defaults:

- `COLORSCHEME` default changed to `moonlight-lite.cs`
- header decorations default changed from `Plain` to `CS Gradient`
- `GROUPED_MENUS` default enabled

Appearance property names are declared in:

- `tmp/ProjectZeroSlackr-SVN/libs/launch/pz.h`

Relevant appearance keys:

- `menu.selbg`
- `menu.selborder`
- `menu.selfg`
- `header.bg`
- `header.fg`
- `header.line`

Runtime schemes and scheme art live in:

- `/tmp/zeroslackr-release/ZeroSlackr/usr/share/schemes/`

This is the most practical near-term hook for a `classic_6g` visual profile without touching boot or mount logic.

## Where Clickwheel / Button Navigation Is Handled

Runtime shell power/reboot behavior is patched in:

- `tmp/ProjectZeroSlackr-SVN/base/ZeroLauncher/src/patches/core-ipod.c.patch`

That patch only shows reboot/poweroff handling, not the full wheel event loop.

The available evidence for input and widget plumbing is:

- `tmp/ProjectZeroSlackr-SVN/libs/launch/pz.h`
- `tmp/ProjectZeroSlackr-SVN/libs/pz0/libs/microwindows/ipod/src/include/nano-X.h`

These expose screen/event/widget primitives used by `podzilla2`.

Implication:

- precise clickwheel interaction tuning for a real Apple-style shell will need the upstream `podzilla2` core source fetched by `build.sh`
- for this milestone, menu-navigation behavior should be treated as a visual shell contract, not reimplemented on-device

## Where Screen Dimensions Are Assumed

Direct screen-size assumptions appear in several places:

- Rockbox `iClassic` reference assets are explicitly `320x240`
- `modules-browser-browser.c.patch` references `ttk_screen->w`
- `nano-X.h` exposes `xres` and `yres`

The extracted theme reference confirms:

- `menu-iClassic_v1.0_Menu.png` is `320x240`
- `wps-iClassic_v1.0_WPS.png` is `320x240`

For this milestone, `320x240` should be treated as the single fixed target.

## Runtime Files Worth Preserving

Compiled launcher and shell entry:

- `/tmp/zeroslackr-release/ZeroSlackr/opt/Base/ZeroLauncher/ZeroLauncher`
- `/tmp/zeroslackr-release/ZeroSlackr/opt/Base/ZeroLauncher/Launch/Launch.sh`

Config paths used by the patched runtime:

- `/opt/Base/ZeroLauncher/Conf/zerolauncher.conf`
- `/opt/Base/ZeroLauncher/Conf/menu.conf`

Current constraint:

- those config files are runtime-generated or absent in the extracted payload, so the safest first milestone is to stage a profile and renderer in-repo before attempting runtime config mutation on the iPod

## Audit Conclusion

What is easy now:

- stage assets
- document source/layout
- define a `classic_6g` profile
- generate reference screenshots and mockups

What is not yet safe to promise from the current repo alone:

- a true rebuilt ZeroLauncher with new split-pane menu geometry
- a real runtime toggle inside the compiled iPod shell without rebuilding upstream `podzilla2`

That is why this milestone uses the live ZeroSlackr runtime as the base, but keeps the actual visual redesign work in a safe dev renderer and asset/profile layer first.

# Sitekick Presets

Sitekick supports up to eight named presets. A preset captures the complete
visible Sitekick loadout: all eight equipped chip positions, body colour and
background. Chips are stored by catalogue ID, not their current catalogue
index, so catalogue updates do not change a saved loadout.

## Controls

On the main Sitekick workshop screen, Left and Right cycle through saved
presets and apply the selected loadout immediately. The active preset name is
shown in the status bar. With no saved presets, cycling reports that none are
available.

The iPodJS Sitekick right pane is a cached view of the last explicitly saved
or applied preset. Changing individual workshop items does not replace that
pane; save the changed loadout as a preset to publish it. This avoids root-menu
draw-time file work and makes the pane a reliable representation of the last
chosen preset. The plugin's live workshop stage uses a 24-frame elapsed-tick
float, sway and contact-shadow animation over its existing fixed pixel caches.

Select remains the only control for opening or activating a menu item. The
workshop adds `Presets` and `Save Preset` items. `Save Preset`, and `Save new
preset` inside the Presets list, open the same iPodJS scrolling alphabet
carousel used by Music Search, extended with `0`–`9`. The wheel scrolls
characters, Select inserts the selected character, Left deletes, and Right
inserts a space. Play saves a non-empty name and Menu cancels. The main
workshop has a permanent `+ CLEAR` button; scroll to it and press Select to
clear the live Sitekick to no chips with the default body colour and
background. Selecting an existing preset applies it.

Saving a name that already exists updates that preset. New names use an empty
slot; when all eight are used, Sitekick reports that the preset list is full.

## Persistence and migration

Presets are stored in `/.rockbox/sitekick/state/save.v1.dat` in the `SKS3`
format. The save continues to use write-to-temp then rename. Existing `SKS1`
and `SKS2` saves load unchanged and are rewritten as `SKS3` when Sitekick next
saves state. Existing ownership, coins, dump, shop and active loadout data are
preserved during migration.

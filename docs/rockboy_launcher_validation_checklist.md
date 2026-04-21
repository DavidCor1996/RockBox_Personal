# Rockboy Launcher Validation Checklist

Keep this pass limited to launcher validation. Do not add new Games features until every checklist item below is verified.

## Simulator (iPod Video 5G)

Preconditions:
- Build the sim with `make -C build-sim-video-5g -j2`.
- Launch `./build-sim-video-5g/rockboxui`.
- Seed the simdisk with at least one `.gb` or `.gbc` ROM under `/GameBoy`.
- If testing indexed mode, add `/.rockbox/rocks/games/rockboy_launcher/games.tsv` plus matching cover files.

Checks:
- From the root menu, confirm `Games` is visible without entering the plugin browser manually.
- Enter `Games` and confirm the dedicated launcher opens instead of a plain file tree.
- Scroll through at least 10 entries and verify list movement remains responsive with no obvious input lag or redraw stalls.
- Confirm the selected game title updates on each move and the cover pane refreshes to the newly selected item.
- Confirm missing covers show the placeholder card instead of blank or corrupted graphics.
- Launch a ROM with the center/select button and verify Rockboy opens directly without an intermediate file picker.
- Exit Rockboy and confirm control returns to the launcher with the prior selection still highlighted.
- Back out of the launcher and confirm the firmware returns to the normal menu stack cleanly.

## Real Device (iPod Video 5G)

Preconditions:
- Install the updated build, `rockboy_launcher.rock`, and `rockboy.rock`.
- Copy ROMs to `/GameBoy`.
- Copy optional covers as pre-sized `.bmp`, `.jpg`, or `.jpeg` sidecars, or provide an indexed `games.tsv`.
- If validating saves, place test save data under `/.rockbox/rockboy/`.

Checks:
- Open `Games` from the root menu and confirm startup time feels comparable to other built-in menu actions.
- Scroll rapidly through the library and verify the wheel remains responsive while cover updates occur.
- Confirm cover changes track the current selection and do not leave stale art on screen.
- Launch at least one ROM with an existing save and one without, then verify both return cleanly to the launcher after exit.
- Confirm save-aware rows report expected status after returning from Rockboy.
- Confirm Back/Menu exits the launcher cleanly without leaving a TSR loop or blank screen.

## Notes To Capture

Record:
- Firmware build/date tested.
- Whether the launcher used indexed mode or `/GameBoy` scan fallback.
- Number of ROMs in the library during the run.
- Any slow cover loads, stale cover frames, failed returns from Rockboy, or menu-state regressions.

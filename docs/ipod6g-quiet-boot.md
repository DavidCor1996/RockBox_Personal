# Classic image-only startup

Normal iPod Classic (6G/7G) startup displays the existing boot image without
bootloader version/loading text. The bootloader stores that image as 4,852
bytes of RGB565 runs rather than adding a 153,600-byte bitmap to its limited
instruction RAM. Regenerate the runs with `tools/generate_quiet_boot_logo.py`
when changing the existing boot bitmap.

After publishing the firmware boot image, LCD rectangle transfers are held
while initialization and the iPodJS Home menu draw into the existing
framebuffer. The completed Home or Hold lockscreen frame releases the hold.
There is no timed delay, additional framebuffer, core allocation, or playback
memory ownership change. Alternate configured startup screens and the TV
interface retain their existing presentation lifecycle. This change targets
Classic; other iPod models retain their current behavior.

Storage/mount failures, explicit notices, panic output, USB recovery, and
startup plugins can release the hold so an actionable screen is never hidden.
Development bootloader diagnostics remain available.

## Validation (2026-09-26)

- Classic firmware, Classic simulator, and Classic bootloader builds pass.
- A 12-second cold simulator capture observed exactly two distinct frames:
  the existing Apple boot image and the fully drawn Home menu at 1.318 seconds.
  There was no intermediate blank or text frame.
- The packed bootloader image matches all 76,800 source RGB565 pixels.
- Firmware text grows by 224 bytes; reported data and BSS sizes are unchanged
  (17,724 and 9,495,492 bytes). The hold uses one boolean, within alignment
  padding. Bootloader text is 110,176 bytes and links within its RAM limits.
- Changed tracked files pass `git diff --check`.
- The broader headless navigation regression reached Home, Music, and Artists
  (including Hold/unlock captures), then failed with `simulator did not enter
  a new list after Artist`. It does not establish active-playback or full-depth
  navigation qualification. The normal fixture also has a pre-existing recursive
  preview directory, so the run used an isolated startup fixture.

No firmware or bootloader has been installed. A firmware-only update fixes the
menu handoff; removing the earlier bootloader text also requires installing
the matching bootloader. Physical cold boot, Hold, USB recovery, and error
screens still require device validation before claiming hardware qualification.

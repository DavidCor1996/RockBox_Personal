# USB Connected animation and safe eject

The native USB HID action reader returns `ACTION_NONE` on timeout. The USB
screen previously waited for `SYS_TIMEOUT`, leaving the source animation on
its first few frames. The HID reader now accepts the screen's timeout, and
the iPodJS screen returns to its draw loop after each event. The existing
18-frame animation remains elapsed-tick driven at 12 frames per second.

A successful SCSI START STOP UNIT eject flushes cached ATA/NAND writes where
supported. The storage driver publishes safe-to-disconnect only after all
exposed drives are ejected and the successful command status wrapper has
completed. A failed flush or USB transfer does not publish success. Connect,
disconnect and media replacement clear the published state. The UI keeps
waiting for unplug after showing the safe screen.

The safe screen uses private RetailOS resource 563,
`DiskModeImage_DisconnectIcon`, on the existing resource 392 gold badge and
resource 11 background, with `OK to disconnect.` in the cached retail font.
The icon is loaded before USB acknowledgement beside the badge in the
existing transition workspace. Neither draw path reads storage or allocates
pixel memory. The native build initially measured 64 additional BSS bytes
for descriptors/state, with no additional framebuffer. The USB draw function
uses 56 bytes of local/register stack in the inspected ARM prologue.

Focused checks:

```sh
rockpod/.venv/bin/python -m pytest -q \
    rockpod/tests/test_usb_screen_events.py \
    rockpod/tests/test_ipodjs_usb_retail_source.py
bash tools/ipodjs_usb_retail_sim_regression.sh build-sim-ipod6g /tmp/usb-loop
bash tools/ipodjs_usb_eject_sim_regression.sh build-sim-ipod6g /tmp/usb-eject
```

The eject simulator gate uses `ROCKPOD_SIM_USB_EJECT_SECONDS=4` to inject the
UI state without requiring a simulated mass-storage host. The native C
harness separately executes the production event loop and SCSI eject/CSW
paths, including HID/non-HID idle, queued input, failed flush, partial eject,
hidden first drive, failed status transfer, and disconnect handling.

Hardware validation remains required: leave Connected spinning with HID on
and off, transfer a file, eject using the host OS, verify the safe screen,
unplug, and reconnect. No physical firmware deployment is part of these
checks.

Validation in this session: native and simulator builds passed, all six
focused tests passed, all 18 USB frames were captured over two connections
in both appearance modes, and the safe-eject visual gate passed including
return to Home after unplug. The broader navigation gate was also attempted
but stopped with `simulator did not enter a new list after Artist`; it did
not reach its playback/stress checks. Its shutdown trace came from cleanup.

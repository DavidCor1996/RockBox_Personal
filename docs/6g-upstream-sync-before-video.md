# 6G upstream sync before video pass

## Scope and baseline

Local-only changes against `master` at `04984f554e`, on branch
`codex/6g-upstream-sync-before-video`. The checkout already contained 1,326
modified tracked files and many untracked files. Existing work was retained;
only this series' changes are staged in its commits. Builds and host probes
use the actual combined working tree, not a pristine upstream checkout.
No pushes, remote mutations, deployment, or video/remote changes are included.

## USB audio

Adapt Rockbox `3bd18f5a443bf9560c188a189c760c64ae6de1a5` to this tree's
`usb_audio.c` plus separate `usb_iap_hid.c`, rather than importing upstream's
replacement iAP/audio architecture.

- Advertise the hardware-supported subset of 32, 44.1 and 48 kHz, ascending.
- Restrict internal nearest-rate selection to that same list.
- Reject unsupported SET_CUR frequencies without changing the current rate,
  fractional cadence, or host-selection flag. Retain the existing three-byte
  request validation.
- Already present: fractional accumulator repeats every ten frames at 44.1 kHz
  (nine 176-byte packets and one 180-byte packet); 32 kHz yields 128 bytes and
  48 kHz 192 bytes. No uint8_t packet counter to fix.
- Already present: AC total length sums serialized bLength fields, stream alt
  is reset/tracked, and AC/HID interfaces accept only alt 0.

## USB storage

Adapt the following upstream changes as a coherent local storage commit:

- `93b49594d525e25544b8d098d81f179d459d0364`: account completed data bytes in
  CSW residue; stall the requested data pipe for unknown commands; reject
  unsupported WRITE BUFFER forms; send ATA IDENTIFY from the initialized buffer.
- `2b664d6025e34970f3c58c05f90b0c086dd25e44`: repeated configuration must not
  restart ownership handover; retain ownership through bus reset; clear pending
  acknowledgement count on release; handle a broadcast with zero recipients.
- `e2ee665ccea91cb84d44339617aed6759833adca`: retain the first command until
  exclusive access is granted, notify the class when ready, cancel retained
  commands on BOT reset. Also reset the deferred-command state on disconnect.

This tree registers classes centrally in usb_core.c. Wire the storage callback
there and expose its declaration in usb_storage.h. Preserve existing read-only
LUN handling, successful-eject reporting, flush-on-eject and host-role code.
A reset without reconfiguration intentionally leaves local volumes unavailable
until unplug, rather than remounting under a possible host owner.

## SET_ADDRESS: skipped

`841007dfa17c8979189bfe224e1fbb698c4d0353` is not a clean transplant. Upstream
moves SETUP/ACK ownership into controller drivers using usb_core_setup_received
and completion suppression. This branch instead has DesignWare-owned EP0
REQ/CTRLWRITE/CANCELLED/DATA/STATUS state, pending-request replay, and
usb_core_control_request/control_response. It also deliberately avoids an EP0
FIFO flush during active ISO audio. Its SET_ADDRESS register write already
occurs in the controller before response; the core still coordinates the ACK.
Applying the upstream patch would require a separate EP0 lifecycle migration
and matching ARC changes for 5G. Leave the controller/core address path intact.

## iFlash issue 37 audit

Source: https://github.com/nuxcodes/rockpod/issues/37

The immediate non-PM `else if (canflush) ata_power_down()` path is still present.
However, this working tree already has HAVE_STORAGE_FLUSH, propagated flush
errors, and an SSD logical-idle path that retains controller and adapter rails.
Its SSD behavior is different from the issue's older clock-gating path.

Add only a narrow IDENTIFY model-prefix match for `iFlash-Platform`, decoding
ATA word byte order without allocating a model buffer. Exclude CE-ATA. Apply
at initial detection and when Auto mode is selected. Show the existing heuristic
and effective SSD/HDD mode together in View disk info. This routes matching
adapters in Auto through the existing SSD protection without a new delay or
power-state mechanism. Keep this in its own commit; hardware validation pending.

Do not add the proposed 500 ms delay: it is an unproven timing guess, and issue
comments do not establish that it cures corruption. Do not globally disable
non-PM powerdown while leaving ata_powered true: shutdown_hw waits for storage
inactivity, so that change would force the timeout path. Forced HDD mode and
unrecognized adapter models retain existing behavior; select Storage Mode SSD
as the current workaround. This is not proof of safe physical shutdown or a
complete corruption fix. Long-duration testing and filesystem checks remain
necessary. No new generic ATA or hibernate policy is introduced.

## Artwork

Track-directory folder.jpg already exists. Add only parent-directory folder.jpg
after existing parent album/cover lookups, under the existing JPEG guard and
only when size_string is empty. Sized lookups, extension preferences, embedded
artwork and existing search priority are unchanged.

## Validation

- Five host probes compile extracted firmware C functions with strict host
  warnings and run assertions: packet totals over 10,000 frames per rate;
  invalid rate rejection; BOT residue/deferred-command state transitions;
  repeated ownership requests/ack epochs/zero recipients; model identification;
  artwork precedence, parent depth and sized-lookup exclusion.
- These use stubs and do not exercise USB hardware, DMA, actual BOT resets,
  complete descriptors on the wire, or physical filesystem/power behavior.
- ARM GCC 9.5.0 normal hardware builds attempted for ipod6g and ipodvideo.
  Both stop in pre-existing modified apps/root_menu.c: MENUITEM_FUNCTION at
  lines 5930-5932 has seven arguments, but the macro accepts six; msn_item is
  consequently undeclared at line 6240. This task does not modify that file.
- All changed applicable C components compile successfully for both hardware
  targets (the 6G-only ATA file is built for 6G). No installable full build was
  produced. Existing unrelated compiler warnings remain in full-build logs.
- Task changes pass git diff --check. The overall checkout remains dirty.

Run probes: `python3 tools/tests/test_6g_upstream_sync.py`.

## Hardware tests after full builds become available

1. 6G and 5G enumeration/reconnect, repeated configuration and bus resets.
   Confirm local filesystems stay unmounted until storage is released/unplugged.
2. Fast first TEST UNIT READY/READ/WRITE during filesystem handover; BOT reset
   while waiting; unplug/reconnect during handover. No cancelled command replay.
3. ATA IDENTIFY as the first command, short data transfers, unsupported commands
   with IN/OUT data stages, unsupported WRITE BUFFER; verify CSW residue/status.
4. Real MFi DAC at 32/44.1/48 kHz; long 44.1 kHz playback; alt 0/1 toggles;
   invalid frequency followed by GET_CUR; connect/disconnect and normal playback.
5. iFlash Auto reports mode SSD, including cold boot and USB return. Verify idle,
   repeated normal shutdown/boot, existing hibernate/resume and write persistence
   on backed-up media. Check filesystems and firmware/database checksums over
   prolonged use. Validate HDD behavior separately. Detection is unvalidated.
6. Track and parent folder.jpg, existing cover/album precedence, sized art,
   PictureFlow and WPS on both targets while music plays.

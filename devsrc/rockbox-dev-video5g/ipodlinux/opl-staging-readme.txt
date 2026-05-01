ProjectZeroSlackr-SVN staging for iPodLoader2 / OPL experiments

This directory is a staged reference copy for an iPod Video 5G WinPod.
It is not active by itself and does not change the current boot chain.

Current preserved boot targets:
- Rockbox
- Apple firmware

Staged here for later integration:
- boot/vmlinux
- boot/boot.pzm
- bin/
- dev/
- etc/
- ZeroSlackr/

Not done yet:
- no live loader.cfg changes
- no firmware writes
- no bootloader patch step
- no pack merge beyond the minimal base

Expected future loader entry example:
ZeroSlackr @ (hd0,1)/boot/vmlinux root=/dev/hda2 rootfstype=vfat rw quiet

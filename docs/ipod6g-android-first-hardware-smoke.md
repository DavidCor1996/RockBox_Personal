# iPod Classic N25 first volatile hardware smoke test

Status: **COMPRESSED U-BOOT STAGE A HOST-QUALIFIED FOR ONE VOLATILE TEST**
Persistent-storage action: none. Leave every historical diagnostic directory
untouched; Stage A does not mount or read the iPod volume.

Do not execute an old DFU loader. `diagnostic-trace1` changed from Rockbox text
to a full white screen. That proves a later display-side transition occurred,
but TRACE1 incorrectly modeled raw LCD register writes without the panel GRAM
cursor and streamed complete colors too quickly to observe individually. It is
disqualified and its white result cannot identify the final execution stage.

TRACE2 passed an exact linked Rockbox panel-prepare/quiesce/cache/jump host
model, but its physical boot retained the Rockbox legend ending at
`3 BANDS = PID 1` and never produced its first red band. It is disqualified;
no Rockbox-to-Linux packet is approved for another device test. The independent
storage-free U-Boot enumeration gate below is now the only approved next step.

The original 195,136-byte direct U-Boot DFU packet is also disqualified for
Boot ROM transport. wInd3x rejected block 129 with status 3 before U-Boot
executed, leaving the iPod in `05ac:1223` DFU. Do not retry that packet. The
replacement uses a 99,913-byte stage zero and a 101,968-byte IMG1 packet, so
the entire transfer completes below the observed transport boundary.

The failed TRACE2 host gate executed the packaged Image in an ARM926 model.
Rockbox reads the physical panel strap,
issues the correct 8- or 16-bit full-window/memory-write sequence, and starts
one GRAM transaction. Loader, raw kernel entry, live Timer E, first Timer B IRQ,
and Linux LCD probe then append red, green, yellow, blue, and amber 320x48
bands. The completed pattern is persistent and exactly 76,800 pixels, so it
does not depend on cursor wraparound or human-visible timing. All four N25
panel straps, Linux initcalls, PL192 handshakes, and final ARM entry state pass.
This clears the corrected host gate; it is not physical qualification.

Superseded timer-only packet identities (do not boot):

| Artifact | SHA-256 |
|---|---|
| `n25-visible-kernel.ipod` | `b93f230b71b5f04f079af273c9b45194d118b6f5380fd169540b19dd435a7a5b` |
| `n25-visible-initramfs.ipod` | `69298f5c47264f375f0cbafd50b2cbb62a088d64b0d63ad36652afd7f31d21df` |
| `n25-visible-dtb.ipod` | `a3a14339c46872fb48de2e65a9f0121b5a82804afcdb3c2ccbbeaeca8f659d68` |
| `n25-visible-rockbox-loader.dfu` | `edbe9b13ef598fb285340cb4e5bed53254e7544c1500016a7821b9fa6b839537` |

The failed `diagnostic-lcd1` packet was staged with the create-only stager and
read back successfully. After that failed boot, both installed Rockbox firmware
copies and protected database/tagcache/config files still matched their
recorded values. Do not restage, replace, or delete historical packets. The new
`diagnostic-trace1` was then create-only staged, read-back verified, and
executed once, producing the full-white result. Both
Rockbox firmware copies and protected database/tagcache/config hashes were
unchanged, and the FAT volume was synced and cleanly unmounted.

Failed `diagnostic-lcd1` packet identities (do not boot):

| Artifact | SHA-256 |
|---|---|
| `n25-visible-kernel.ipod` | `fb72e46c784fac2140160504c751be9677e08c053dd9dd72455c84e68fce7fda` |
| `n25-visible-initramfs.ipod` | `69298f5c47264f375f0cbafd50b2cbb62a088d64b0d63ad36652afd7f31d21df` |
| `n25-visible-dtb.ipod` | `f479ba061cdf5f3e357ffb2022036c356072d76ba12e40e4d46cdf8344b380a5` |
| `n25-visible-rockbox-loader.dfu` | `71f141ea901925a4ad61e8ed82130c5e2839b7bc507dac751b5834c0c20e9225` |
| `n25-lcd-strap0-emulation.json` | `4e45517a2f57a3789b9ad266d24b6008c0f6607cefd22767381da430cb38bc4d` |
| `n25-lcd-strap1-emulation.json` | `615932428981184d9f3d615f4e1407d4242579f60c45a011a7b13f34da159cd3` |
| `n25-lcd-strap2-emulation.json` | `af3515f6a227c10897ee26d55994dc9aa9f10ca188941214c0ce3e33ca6b10a7` |
| `n25-lcd-strap3-emulation.json` | `4cef64ac08dba28409f8d8cad75f3188fee4b2eb992d301bb354c41934726031` |

Failed `diagnostic-trace1` packet identities (do not boot):

| Artifact | SHA-256 |
|---|---|
| `n25-visible-kernel.ipod` | `6aaeb7082df613d3bb00506a235023835fcedf6d999f63b39790c34c60a471a1` |
| `n25-visible-initramfs.ipod` | `69298f5c47264f375f0cbafd50b2cbb62a088d64b0d63ad36652afd7f31d21df` |
| `n25-visible-dtb.ipod` | `f479ba061cdf5f3e357ffb2022036c356072d76ba12e40e4d46cdf8344b380a5` |
| `n25-visible-rockbox-loader.ipod` | `4c77b277dd7f24e90c4968a275b5debb79681264f7d346772848b06f6bb4f3ea` |
| `n25-visible-rockbox-loader.dfu` | `4494c752e2e3294d0aa9104b7075ad3a707d64795ddf7e74042b4769c80d7447` |
| `n25-lcd-strap0-emulation.json` | `a11b8370341740c07a42aa82e3b4d2f00e98f5e5852b3c8abfd2e7ff10e6137d` |
| `n25-lcd-strap1-emulation.json` | `b7f38b479dadc1c3680a2047003fff1a18196ab882cc3ce4932ddbe537181b33` |
| `n25-lcd-strap2-emulation.json` | `c167dbb472064280bf998f5472384922f3830cf4313d24f05894f2cd3f0edab2` |
| `n25-lcd-strap3-emulation.json` | `bf75737a09b90027e502717607a90132cc39d53995638905849d2a5c71d31715` |
| `qualification.json` | `cb720a41577c707e7ab1492b5bdf8fd7cb18c40cc6007323847f7bcbbc57b6ef` |

Failed `diagnostic-trace2` packet identities (do not boot):

| Artifact | SHA-256 |
|---|---|
| `n25-visible-kernel.ipod` | `da5dba681e563c690bb5aaf05f066279e94b3edee3557e387576df8bda9696de` |
| `n25-visible-initramfs.ipod` | `69298f5c47264f375f0cbafd50b2cbb62a088d64b0d63ad36652afd7f31d21df` |
| `n25-visible-dtb.ipod` | `f479ba061cdf5f3e357ffb2022036c356072d76ba12e40e4d46cdf8344b380a5` |
| `n25-visible-rockbox-loader.ipod` | `e2b43e1a801b4ca55e4a08e3ca56fa6df5048b76e344badfa519c6ffdc53330a` |
| `n25-visible-rockbox-loader.dfu` | `3c52526e02d01b889e0cc2dcdc5ed4f8f2f892403701ee730211045c76899df0` |
| `n25-lcd-strap0-emulation.json` | `cd7cedb5ad2cbedbbc4087f1435ab26f84d6f065147746d24e7e2bb046a8b119` |
| `n25-lcd-strap1-emulation.json` | `fc59dd81a5e46af0d003c7a92d8987d2aae326c1bce4d092a2e003bc2b2bacbf` |
| `n25-lcd-strap2-emulation.json` | `b6478e46398a8da704e38fc43d360b39ac8ade51794859ac1941034a63a46fc0` |
| `n25-lcd-strap3-emulation.json` | `02bfc914b9eff969b89d3000b9e8c5d4be83af7eacbf838aad8c826c7ebae160` |
| `qualification.json` | `562094b8eb8413e8d539364c8df86a8ccd6cf9ad574776d3f235d5daa1b30a73` |

## Purpose and boundary

The stages answer, in order, whether N25 executes the qualified U-Boot payload,
whether U-Boot exposes its RAM-only DFU endpoint, whether the diagnostic Linux
FIT reaches its USB-serial heartbeat and resets safely, and finally whether the
qualified Eclair FIT presents pixels and receives Classic input.

The transient Rockbox bootloader opens the existing FAT volume only to read
three fixed, checksum-validated files into fixed RAM ranges. It performs no
filesystem write operation, then unmounts and sleeps storage before Linux
handoff. Linux has no block layer or storage node. The path does not install
firmware, update either Rockbox copy, alter the MBR, resize FAT32, create a
partition, or write NOR. Reset or power loss discards all running code and
Android state.

## Superseded direct Menu+Play packet — do not use

The qualified Eclair bundle contains these direct-boot artifacts:

| Artifact | Size | SHA-256 |
|---|---:|---|
| `n25-eclair-menu-play-bootloader.dfu` | 100,480 | `08646a1f20c18cfa95c2faebc54ea2be0d1985f5bcce368cd0a78b8e2a2464d4` |
| `n25-eclair-kernel.ipod` | 1,683,488 | `aba83932dcb54cc342d008c28e684212a7ba04627bc871c5f00bb0aedc06dc9b` |
| `n25-eclair-initramfs.ipod` | 13,842,312 | `959c8ff59013c98ab6bdd80129e0fe70888079920902e6a8c8ec3e2b03fba372` |
| `n25-eclair-dtb.ipod` | 3,032 | `64112d9058839b8e0dd5856d6eb2a9762166df4795148d6331d3b9069f73d81c` |

The bootloader requires the exact **Menu+Play** chord at its 400 ms selection
point. Existing Menu, Select+Left, Select+Play, Select+Right, Hold, and
Menu+Select behavior is unchanged. A wrong chord boots ordinary Rockbox.
Missing, oversized, wrong-model, or checksum-invalid components stop on a
visible error screen and never enter Linux.

### Historical first-direct-run procedure — do not run

1. Sync and unmount the iPod data volume.
2. Hold Select+Menu for about 12 seconds until Apple Boot ROM DFU enumerates.
3. Release Select+Menu and hold Menu+Play.
4. Run only `wInd3x run n25-eclair-menu-play-bootloader.dfu`.
5. Keep Menu+Play held until the transient Rockbox screen says it is loading
   Android.
6. Observe LCD/input. Menu+Select for eight seconds requests reset; otherwise
   the kernel forces reset at 180 seconds.
7. After normal Rockbox returns, verify both `rockbox.ipod` copies, the staged
   manifest, and all database/tagcache hashes again.

Never use `mks5lboot --bl-inst`: that command builds and executes a NOR
installer and is outside this test.

## Fixed compressed Stage-A packet

Use only the files under `rockpod/bin/ipod6g-android/ramdiag-stage0` after
`sha256sum -c SHA256SUMS` passes and `qualification.json` reports
`device_test_ready: true`. At this checkpoint the binary identities are:

| Artifact | SHA-256 |
|---|---|
| `n25-ramdiag-stage0-uboot.dfu` | `ebff6e3f0c7cd3c7293d3c09ea081801fde81246493d9c4b5ff289ce71b521b1` |
| `n25-ramdiag-stage0.bin` | `c5f8479dbbd950d133a0c034aee2276b1716b024792d2d6bb07c9a836f42bb69` |
| reconstructed `u-boot.bin` | `a2ba496dd0b7fc3fc2f02e3a5b707da81e7d406f932cf4975a2b2d842afca978` |

The exact stage zero passed 32,880,326 ARM926 instructions in the host gate.
That gate verifies Rockbox's Classic cold-DRAM sequence, the DRAM payload copy,
IRAM relocator copy, decompression and CRC path, byte-for-byte reconstruction
of the qualified 193,080-byte U-Boot, and entry at `0x22000000`. U-Boot's
configuration has a RAM-only DFU alternate, environment-nowhere, no storage
commands or drivers, and USB identity `05ac:8007`. This remains host evidence,
not physical qualification.

## Preconditions

Before connecting the device:

1. Obtain explicit approval for this volatile test only.
2. Stop if the three hashes differ or qualification reports false.
3. Use a native Linux host, a direct USB cable, and no hub or VM.
4. Disconnect every other iPod/Apple DFU device so selection cannot be
   ambiguous.
5. Ensure the iPod data volume is unmounted before DFU entry. No tool in this
   procedure accepts a block-device path.
6. Record the current Rockbox database/tagcache hashes and confirm a separate,
   readable backup exists. The test should not reach storage, but the backup is
   a recovery prerequisite for any experimental hardware work.
7. Have a known reset procedure available: hold Menu+Select until reset, then
   allow the installed boot flow to resume.

## Stage A: U-Boot enumeration only

The commands below are the reviewed operator packet; they are intentionally
not wired into Rockpod. First verify the installed packet:

```sh
cd rockpod/bin/ipod6g-android/ramdiag-stage0
sha256sum -c SHA256SUMS
```

Put only the Classic into Boot ROM DFU. It should enumerate as Apple
`05ac:1223`; wInd3x currently treats the Classic through its S5L8702/Nano3
device kind. Then the only payload command is conceptually:

```sh
wInd3x run n25-ramdiag-stage0-uboot.dfu
```

Success is re-enumeration as U-Boot's `05ac:8007` “USB download gadget” with
one DFU alternate named `diagnostic`. Record timestamps, complete wInd3x
output, `lsusb`, and `dfu-util -l`. Do not upload the FIT during the first run.
Reset the iPod and confirm normal Rockbox boot plus unchanged database/tagcache
hashes.

Stop immediately if the USB identity is unexpected, the device exposes a mass
storage interface, wInd3x identifies a different model, U-Boot does not appear,
or normal reset does not recover the installed boot flow.

## Stage B: RAM-only Linux heartbeat

Stage B requires a separate approval after Stage A evidence is reviewed. The
pinned U-Boot implementation exits its DFU loop on a DFU detach request and
then executes `bootm` from its default FIT staging address, `0x08800000`. This
address replaces the rejected `0x09000000` layout, which could overlap a
larger Android FIT with its loaded kernel. The future reviewed host sequence is:

```sh
dfu-util -a diagnostic -D n25-ramdiag.itb
dfu-util -a diagnostic -e
```

Do not use `dfu-util -R`: this pinned U-Boot interprets the following USB reset
as a request to reboot instead of continuing to `bootm`. A successful kernel
boot should replace `05ac:8007` with the built-in CDC-ACM gadget
`0525:a4a7`, create a host `/dev/ttyACM*`, and emit:

```text
ROCKPOD N25 RAMDIAG: PID1 started; no storage drivers
ROCKPOD N25 RAMDIAG HEARTBEAT; send r to reboot
```

Sending only lower-case `r` requests the exact S5L8702 reset sequence used by
Rockbox. PID 1 ignores every other byte. If no command is sent, the kernel
forces that reset after 60 seconds. After reset, verify normal Rockbox boot and
the preservation hashes again.

## Stage C: RAM-only Eclair display and input

Stage C requires separate approval after Stage B evidence and recovery are
reviewed. Verify `rockpod/bin/ipod6g-android/eclair-native/SHA256SUMS`, enter
Boot ROM DFU again, run only `n25-eclair-native-uboot.dfu`, and upload only
`n25-eclair-native.itb` to the same `diagnostic` alternate. The fixed
identities are:

| Artifact | SHA-256 |
|---|---|
| `n25-eclair-native-uboot.dfu` | `c2d88f752900734da7ee946bab00ef43cd60502e4f68d81fb1217e2b59ecb00e` |
| `n25-eclair-native.itb` | `678ef70c56f503e432e7b5f5b9083fe3af9869f6a7005d9bd872decbe9c065ec` |
| `n25-eclair-native-initramfs.cpio.gz` | `2e8134f8b496ea1d8c93523f364664abdf26c8defdccfa67ce72756e543cd10d` |

Expected observations are panel/backlight activation, the 320x240 Rockpod
Launcher, clockwise/counter-clockwise DPAD movement, Select, Previous/Next,
Play/Pause, Menu/Back, and immediate release/suppression when Hold is enabled.
Test Menu+Select for eight seconds only after ordinary input works. Regardless
of UI state, the kernel forces a reset after 180 seconds. Confirm normal
Rockbox boot and repeat all preservation hashes afterward.

## Hard stop conditions

Never improvise with wInd3x `nand`, `nor`, `restore`, or CFW commands. Never
pass `/dev/sd*`, `/dev/sg*`, `/dev/nvme*`, or a mounted iPod path to any tool.
Do not continue after a partial result. Do not add storage, partitioning,
Froyo, audio, charger, shutdown, or installer experiments to the same session;
each is a separate promotion gate.

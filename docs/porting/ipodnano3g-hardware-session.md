# iPod Nano 3G Hardware Session Log

Date: 2026-04-23
Host timezone: America/Halifax
Repo: `/home/david/Documents/RockBox_Personal-master`

## Session Goals

1. Validate host tooling and DFU detection.
2. Confirm the safest available Nano 3G payload path.
3. Preserve recovery and document the last known good boundary.

## Safety Baseline

- `NAN03G_SAFE_BRINGUP` must stay enabled.
- No guessed MMIO writes.
- No NAND writes.
- Recovery via force-reset plus DFU remains mandatory before advancing.

## Running Log

### 2026-04-23

#### Step 1: Baseline repo and documentation audit

- Command: `git status --short`
- Observation: worktree is already dirty in many unrelated areas, including Nano 3G target files and generated build outputs. Existing user changes will be preserved.
- Recovery status: unchanged, host-side only.
- Next action: inspect Nano 3G bring-up docs and host-side DFU/install helpers.

- Command: `rg -n "nano3g|Nano 3G|NANO3G|DFU|decrypt|bringup|safe bringup|SAFE_BRINGUP" docs firmware tools utils bootloader apps -g '!**/build*/**'`
- Observation: existing bring-up documentation, safe-mode guard, boot-trace support, and `mks5lboot` helper sources are present in-tree.
- Recovery status: unchanged, host-side only.
- Next action: inspect the safe bring-up policy and boot/install runbook before any hardware attempt.

- Command: `sed -n '1,220p' docs/porting/ipodnano3g-hardware-bringup.md`
- Observation: safe bring-up policy confirms `NAN03G_SAFE_BRINGUP 1`, blocks risky Nano 3G writes, and documents DFU recovery as the mandatory fallback.
- Recovery status: unchanged, documented path exists.
- Next action: inspect the boot/install flow and DFU tooling details.

- Command: `sed -n '1,260p' docs/porting/ipodnano3g-boot-flow.md`
- Observation: repo documents the intended exploit-assisted path as `wInd3x`/NorBoot plus `utils/mks5lboot`; DFU and recovery procedure is described, but the exact host-side exploit binary still needs to be located on this machine.
- Recovery status: unchanged, documented path exists.
- Next action: verify local tool availability and current USB/DFU state.

#### Step 2: Host tool availability and USB visibility

- Command: `which lsusb`
- Observation: `lsusb` is present at `/usr/bin/lsusb`.
- Recovery status: unchanged, host-side only.
- Next action: enumerate USB devices outside the sandbox because in-sandbox libusb initialization failed.

- Command: `python3 -c 'import usb; print(usb.__version__)'`
- Observation: `pyusb` is not installed on this host (`ModuleNotFoundError: No module named 'usb'`).
- Recovery status: unchanged, host-side only.
- Next action: prefer the repo-native `mks5lboot` path instead of assuming `wInd3x` is locally runnable.

- Command: `pkg-config --modversion libusb-1.0`
- Observation: host has `libusb-1.0` version `1.0.29`.
- Recovery status: unchanged, host-side only.
- Next action: build `mks5lboot` and use it for real DFU scans.

- Command: `which wInd3x`
- Observation: no `wInd3x` binary is in the current PATH.
- Recovery status: unchanged, host-side only.
- Next action: continue with repo-native tooling unless a validated local `wInd3x` checkout is located later.

- Command: `find . -maxdepth 1 -type d -name 'build*ipodnano3g*' | sort`
- Observation: only `./build-sim-ipodnano3g` currently exists; there is no ready native Nano 3G build directory yet.
- Recovery status: unchanged, host-side only.
- Next action: keep Phase 1 focused on DFU/tooling first, then create a native build directory before Phase 2 if hardware access proceeds.

- Command: `lsusb`
- Observation: outside the sandbox, host USB enumeration succeeds and shows `05ac:1262 Apple, Inc. iPod Nano 3.Gen` on `Bus 003 Device 013`. Device is connected and visible, but this is not DFU PID `0x1223`.
- Recovery status: good; host still sees the device in its current non-DFU state.
- Next action: build and run `mks5lboot` DFU scan against the real USB stack.

#### Step 3: Repo-native DFU helper validation

- Command: `make -C utils/mks5lboot`
- Observation: `mks5lboot` builds successfully. Warnings are limited to existing string-initializer and `calloc` diagnostics; no build failure.
- Recovery status: unchanged, host-side only.
- Next action: use the built binary for DFU scanning.

- Command: `./mks5lboot --dfuscan`
- Observation: in-sandbox run fails with `LIBUSB_ERROR_OTHER`, consistent with sandbox USB restrictions rather than a device-side issue.
- Recovery status: unchanged, host-side only.
- Next action: re-run DFU scan with direct USB access outside the sandbox.

- Command: `./mks5lboot --dfuscan`
- Observation: outside the sandbox, `mks5lboot` initializes libusb correctly and reports `no DFU devices found` / `DFU device not found`. This confirms the toolchain is functional and the current blocker is simply that the device has not yet been placed into DFU mode.
- Recovery status: good; device remains visible to the host and no write/install action has occurred.
- Next action: run a live DFU scan loop and have the device manually entered into DFU mode using the documented button sequence.

#### Step 4: Native Nano 3G artifact preparation

- Command: `../tools/configure --target=80 --type=N`
- Observation: native Nano 3G firmware build config succeeds in `build-native-ipodnano3g/` using `arm-elf-eabi-gcc 9.5.0`.
- Recovery status: unchanged, host-side only.
- Next action: build the normal firmware artifact set while waiting for the physical DFU transition.

- Command: `../tools/configure --target=80 --type=B`
- Observation: bootloader build config succeeds in `build-bootloader-ipodnano3g/` with the same cross-toolchain.
- Recovery status: unchanged, host-side only.
- Next action: build the bootloader artifact set.

- Command: `make -j4` in `build-native-ipodnano3g/`
- Observation: native firmware build completes successfully and produces `rockbox.elf` plus scrambled `rockbox.ipod`.
- Recovery status: unchanged, host-side only.
- Next action: record artifact sizes and keep the device in the current safe state until DFU is confirmed.

- Command: `make -j4` in `build-bootloader-ipodnano3g/`
- Observation: initial bootloader build failed in `firmware/powermgmt.c` because `sys_get_reboot_type()` referenced `requested_reboot_type` even when `BOOTLOADER` compiled that state out.
- Recovery status: unchanged, build-time only.
- Next action: apply a minimal compile-guard fix that does not change device hardware behavior.

- Code change: `firmware/powermgmt.c`
- Observation: `sys_get_reboot_type()` now returns `SHUTDOWN_REBOOT` in bootloader builds, preserving existing non-bootloader behavior and unblocking compilation.
- Recovery status: unchanged, host-side/build-only change.
- Next action: rebuild the bootloader.

- Command: `make -j4` in `build-bootloader-ipodnano3g/` after the fix
- Observation: bootloader build completes successfully and produces `bootloader-ipodnano3g.ipod`, `bootloader.bin`, and `bootloader.elf`.
- Recovery status: unchanged, host-side only.
- Next action: package or otherwise prepare the safest DFU-executable payload path.

- Command: `ls -lh build-native-ipodnano3g/rockbox.ipod build-native-ipodnano3g/rockbox.elf build-bootloader-ipodnano3g/bootloader-ipodnano3g.ipod build-bootloader-ipodnano3g/bootloader.elf`
- Observation:
  - `build-native-ipodnano3g/rockbox.elf` = `1.2M`
  - `build-native-ipodnano3g/rockbox.ipod` = `817K`
  - `build-bootloader-ipodnano3g/bootloader.elf` = `101K`
  - `build-bootloader-ipodnano3g/bootloader-ipodnano3g.ipod` = `61K`
- Recovery status: unchanged, host-side only.
- Next action: attempt host-side DFU image preparation.

#### Step 5: DFU image preparation and exploit-tool validation

- Command: `./mks5lboot --mkdfu-inst ../../build-bootloader-ipodnano3g/bootloader-ipodnano3g.ipod ../../build-bootloader-ipodnano3g/dualboot-installer-nano3g.dfu`
- Observation: packaging fails with `Model name "nn3g" unknown`. `utils/mks5lboot/mkdfu.c` only carries an `ipod6g` identity table, so the repo-native installer path is not ready for Nano 3G and must not be forced.
- Recovery status: preserved; failure occurred before any device communication.
- Next action: switch to the official `wInd3x` haxed-DFU workflow instead of pretending the Classic 6G installer path is safe for Nano 3G.

- Command: `git clone https://github.com/freemyipod/wInd3x /tmp/wInd3x`
- Observation: official `wInd3x` source cloned successfully to `/tmp/wInd3x`.
- Recovery status: unchanged, host-side only.
- Next action: build the official exploit tool locally.

- Command: `GOCACHE=/tmp/go-build-cache GOMODCACHE=/tmp/go-mod-cache go build ./cmd/wInd3x`
- Observation: build succeeds after allowing dependency fetches; resulting binary is `/tmp/wInd3x/wInd3x` (`15M`).
- Recovery status: unchanged, host-side only.
- Next action: validate the exact haxed-DFU command surface for Nano 3G.

- Command: `/tmp/wInd3x/wInd3x --help`
- Observation: official tool exposes `haxdfu`, `run`, `decrypt`, `dump`, `restore`, `nor`, and `nand` subcommands.
- Recovery status: unchanged, host-side only.
- Next action: inspect `haxdfu`, `run`, and `makedfu` syntax before attempting any DFU-time action.

- Command: `/tmp/wInd3x/wInd3x haxdfu --help`
- Observation: `haxdfu` is available and is the documented temporary exploit step that disables DFU signature enforcement without persisting changes across reboots.
- Recovery status: strong; this aligns with the “preserve recovery” requirement.
- Next action: use `haxdfu` after the device is physically entered into DFU mode.

- Command: `/tmp/wInd3x/wInd3x run --help`
- Observation: `run` will start haxed DFU if needed and then upload a DFU image.
- Recovery status: unchanged, host-side only.
- Next action: identify a Nano 3G-safe DFU payload format before invoking `run`.

- Command: `/tmp/wInd3x/wInd3x makedfu --help`
- Observation: current upstream `makedfu` only supports `-k n4g` and `-k n5g`; it does not directly generate Nano 3G unsigned images.
- Recovery status: unchanged, host-side only.
- Next action: do not guess a Nano 3G payload format. Enter DFU first, validate `haxdfu`, then decide between an evidence-backed raw DFU path and a documented device-assisted/decrypted payload path.

## Current Stop Point

- DFU has not yet been entered during this session, so no exploit has been run and no device-side state has changed.
- Host-side bring-up prerequisites are now much stronger than at session start:
  - host USB visibility confirmed (`05ac:1262`)
  - `mks5lboot` built and DFU scan validated
  - native Nano 3G firmware and bootloader artifacts built
  - official `wInd3x` built locally from upstream source
- Recovery remains intact because all work so far has stayed on the host.

#### Step 6: Post-haxdfu payload format research

- Command: source audit of `/tmp/wInd3x/cmd/wInd3x/cmd_run.go`, `/tmp/wInd3x/cmd/wInd3x/cmd_makedfu.go`, `/tmp/wInd3x/pkg/image/image.go`, `/tmp/wInd3x/pkg/dfu/dfu.go`, `/tmp/wInd3x/pkg/devices/devices.go`, and `/tmp/wInd3x/pkg/exploit/wind3x_n3g.go`
- Observation:
  - `run` always invokes `haxeddfu.Trigger(...)` first, then uploads with standard DFU download/status sequencing.
  - If the provided file is not IMG1, `run` auto-wraps it with `image.MakeUnsigned(app.Desc.Kind, 0, data)`.
  - `cmd_makedfu.go` supports `-k n3g` in code, even though the built `--help` text is stale.
  - For `devices.Nano3`, `image.MakeUnsigned(...)` emits:
    - SoC magic `8702`
    - version `1.0`
    - format `SIGNED (2)`
    - header/body padding to `0x800`
    - no body signature or certificate bundle
  - Nano 3G uses `DFUProtoVersion1`, so `dfu.SendImage(...)` appends the inverted CRC32 trailer required by the older DFU transport.
  - Nano 3G `haxeddfu` patches the bootrom’s `OnImage` path and explicitly forces IMG1 version `1.0` before execution.
- Recovery status: unchanged, host-side only.
- Next action: treat upstream `wInd3x` as the authoritative Nano 3G payload wrapper/sender and avoid the older `mks5lboot` installer path for minimal test payload execution.

- Command: `printf '\\x00\\x00\\x00\\x00' > /tmp/wind3x-min.bin && /tmp/wInd3x/wInd3x makedfu /tmp/wind3x-min.bin /tmp/wind3x-min-n3g.dfu -k n3g`
- Observation: host-only verification succeeded. This confirms that current upstream `wInd3x` really does accept `-k n3g` despite stale help output.
- Recovery status: unchanged, host-side only.
- Next action: inspect the emitted header bytes to validate the exact Nano 3G wrapper format.

- Command: `xxd -g 1 -l 96 /tmp/wind3x-min-n3g.dfu`
- Observation: emitted header begins with:
  - `38 37 30 32` (`"8702"`)
  - `31 2e 30` (`"1.0"`)
  - format byte `02`
  This matches the current upstream `image.MakeUnsigned(Nano3, ...)` implementation.
- Recovery status: unchanged, host-side only.
- Next action: recommend the `wInd3x run` / `makedfu -k n3g` path as the safe next send mechanism after successful `haxdfu`.

#### Step 7: Minimal Nano 3G execution-proof payload build

- Code change: added:
  - `tools/ipodnano3g/minimal_payload/minimal-n3g.S`
  - `tools/ipodnano3g/minimal_payload/minimal-n3g.lds`
  - `tools/ipodnano3g/minimal_payload/Makefile`
- Observation: payload is pure ARM assembly with no Rockbox runtime, no stack setup, no MMIO access, and no peripheral writes. Its only state change is incrementing a `heartbeat` word stored in its own loaded `.data` region.
- Recovery status: unchanged, host-side only.
- Next action: build the artifact set and verify the flat binary before any DFU send.

- Command: `make` in `tools/ipodnano3g/minimal_payload/`
- Observation: build succeeds and produces:
  - `tools/ipodnano3g/minimal_payload/minimal-n3g.bin`
  - `tools/ipodnano3g/minimal_payload/minimal-n3g.elf`
  - `tools/ipodnano3g/minimal_payload/minimal-n3g.map`
- Recovery status: unchanged, host-side only.
- Next action: verify entry address, disassembly, symbol table, and binary contents.

- Command: `arm-elf-eabi-readelf -h minimal-n3g.elf`
- Observation: ELF header reports:
  - machine `ARM`
  - `EXEC`
  - entry point `0x22000000`
  This matches the Nano 3G haxed-DFU load-address evidence already established from the S5L87xx linker setup.
- Recovery status: unchanged, host-side only.
- Next action: inspect the actual instructions and data addresses.

- Command: `arm-elf-eabi-objdump -d minimal-n3g.elf`
- Observation: payload disassembles to:
  - `ldr r0, =0x22000018`
  - `mov r1, #0`
  - loop: `add r1, r1, #1`
  - `str r1, [r0]`
  - `b loop`
  There are no loads or stores to MMIO ranges, no branches to external code, and no startup/runtime calls.
- Recovery status: unchanged, host-side only.
- Next action: confirm the heartbeat storage lives inside the image and that no unresolved symbols remain.

- Command: `arm-elf-eabi-readelf -S minimal-n3g.elf`
- Observation:
  - `.text` at `0x22000000`, size `0x18`
  - `.data` at `0x22000018`, size `0x4`
  This places the heartbeat word immediately after the code inside the loaded image body.
- Recovery status: unchanged, host-side only.
- Next action: confirm symbol resolution and flat-binary bytes.

- Command: `arm-elf-eabi-nm -u minimal-n3g.elf`
- Observation: no undefined symbols.
- Recovery status: unchanged, host-side only.
- Next action: inspect the send artifact itself.

- Command: `ls -lh minimal-n3g.bin minimal-n3g.elf minimal-n3g.map`
- Observation:
  - `minimal-n3g.bin` = `28` bytes
  - `minimal-n3g.elf` = `65K`
  - `minimal-n3g.map` = `1.8K`
- Recovery status: unchanged, host-side only.
- Next action: confirm the flat binary begins with valid ARM instructions at file offset 0.

- Command: `xxd -g 4 -l 32 minimal-n3g.bin`
- Observation: first 28 bytes are:
  - `0c009fe5 0010a0e3 011081e2 001080e5`
  - `fcffffea 18000022 00000000`
  These correspond exactly to the audited instruction stream plus the in-image heartbeat word.
- Recovery status: unchanged, host-side only.
- Next action: check the current USB state, then send the payload via `wInd3x run` once DFU is physically entered.

#### Step 8: Real-device DFU entry and minimal payload send

- Command: `./mks5lboot --dfuscan --loop`
- Observation: after repeated polling, the device entered DFU cleanly and was repeatedly reported as:
  - VID:PID `05ac:1223`
  - `iPod Nano 3G / Classic, mode: DFU`
  - `DFU device state: 2`
- Recovery status: still good; device reached the expected ROM DFU state.
- Next action: stop the watcher and use the upstream `wInd3x run` path with the minimal payload.

- Command: `lsusb`
- Observation: host USB enumeration also confirmed `Bus 003 Device 025: ID 05ac:1223 Apple, Inc. iPod Classic/Nano 3.Gen (DFU mode)` immediately before the send.
- Recovery status: still good; DFU was stable and visible.
- Next action: send the minimal payload.

- Command: `/tmp/wInd3x/wInd3x run /home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/minimal-n3g.bin`
- Observation:
  - first sandboxed attempt failed with `failed to initialize USB: libusb: unknown error [code -99]`
  - rerun with direct USB access succeeded and produced:
    - `INFO Generating payload...`
    - `INFO Running rce....`
    - `INFO Haxed DFU running!`
    - `INFO Uploading... path=/home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/minimal-n3g.bin`
    - `INFO Given firmware file is not IMG1, packing into one...`
    - `INFO Got dfuMANIFEST, image uploaded.`
    - `INFO Image sent.`
- Recovery status: unknown but not obviously lost; the transfer completed without tool-side error.
- Next action: classify post-send USB behavior before attempting any further hardware step.

- Command: `lsusb` immediately after send
- Observation: device still enumerated as `Bus 003 Device 025: ID 05ac:1223 Apple, Inc. iPod Classic/Nano 3.Gen (DFU mode)`.
- Recovery status: not lost; the device remains reachable on USB.
- Next action: wait briefly and re-check whether it leaves DFU or resets.

- Command: `sleep 5` then `lsusb`
- Observation: after an additional delay, the device still enumerated as `05ac:1223`.
- Recovery status: still reachable.
- Next action: attempt one more DFU-state probe and then stop at the safe boundary if the result stays ambiguous.

- Command: `./mks5lboot --dfuscan`
- Observation: the one-shot DFU scan hit `Could not set USB configuration: LIBUSB_ERROR_OTHER`, so it did not give a trustworthy post-send state classification.
- Recovery status: still appears intact from `lsusb`, but this scan result is inconclusive.
- Next action: perform a manual reset and confirm the device can re-enter DFU before drawing stronger conclusions.

## Current Classification

- Minimal payload build: confirmed.
- Nano 3G `wInd3x run` transfer: confirmed.
- Unsafe hardware access introduced: none.
- On-device execution proof: inconclusive from host USB evidence alone.
- Strongest current interpretation:
  - the upload/haxdfu path works end-to-end
  - the post-send USB state did not show a clean departure from DFU
  - recovery still appears available, but must be confirmed by manual reset and DFU re-entry

#### Step 9: Deliberate-fault observable-signature payload

- Code change: replaced the prior heartbeat loop in `tools/ipodnano3g/minimal_payload/minimal-n3g.S` with a deliberate-fault payload:
  - `mvn r0, #0`  -> `r0 = 0xffffffff`
  - `mov r1, #0`
  - `strb r1, [r0]`
  - `b` back to the `strb` as dead-code fallback
- Observation: this removes all loaded data storage and leaves a single intentional invalid memory write as the only possible side effect.
- Recovery status: unchanged before send; device was already in DFU.
- Next action: rebuild and audit the artifact before sending.

- Command: `make clean && make` in `tools/ipodnano3g/minimal_payload/`
- Observation: rebuild succeeds and regenerates:
  - `minimal-n3g.bin`
  - `minimal-n3g.elf`
  - `minimal-n3g.map`
- Recovery status: unchanged, host-side only.
- Next action: verify binary size, entry point, and disassembly.

- Command: `arm-elf-eabi-readelf -h minimal-n3g.elf`
- Observation: entry point remains `0x22000000`.
- Recovery status: unchanged, host-side only.
- Next action: inspect the exact instruction stream.

- Command: `arm-elf-eabi-objdump -d minimal-n3g.elf`
- Observation: payload disassembles to exactly:
  - `e3e00000  mvn  r0, #0`
  - `e3a01000  mov  r1, #0`
  - `e5c01000  strb r1, [r0]`
  - `eafffffd  b    22000008`
  There are no other writes, no external calls, and no peripheral accesses.
- Recovery status: unchanged, host-side only.
- Next action: inspect section layout and flat bytes.

- Command: `arm-elf-eabi-readelf -S minimal-n3g.elf`
- Observation: only `.text` is loadable; there is no `.data` or `.bss`.
- Recovery status: unchanged, host-side only.
- Next action: inspect flat binary size and contents.

- Command: `ls -lh minimal-n3g.bin minimal-n3g.elf minimal-n3g.map`
- Observation:
  - `minimal-n3g.bin` = `16` bytes
  - `minimal-n3g.elf` = `65K`
  - `minimal-n3g.map` = `1.5K`
- Recovery status: unchanged, host-side only.
- Next action: confirm the raw bytes match the audited instructions.

- Command: `xxd -g 4 -l 32 minimal-n3g.bin`
- Observation: flat binary bytes are:
  - `0000e0e3 0010a0e3 0010c0e5 fdffffea`
- Recovery status: unchanged, host-side only.
- Next action: send the payload while the device remains in DFU.

- Command: `lsusb`
- Observation: immediately before send, the device still enumerated as `Bus 003 Device 029: ID 05ac:1223 Apple, Inc. iPod Classic/Nano 3.Gen (DFU mode)`.
- Recovery status: good; DFU still available.
- Next action: run `wInd3x` with the new payload.

- Command: `/tmp/wInd3x/wInd3x run /home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/minimal-n3g.bin`
- Observation: transfer completed successfully:
  - `INFO Generating payload...`
  - `INFO Running rce....`
  - `INFO Haxed DFU running!`
  - `INFO Uploading...`
  - `INFO Given firmware file is not IMG1, packing into one...`
  - `INFO Got dfuMANIFEST, image uploaded.`
  - `INFO Image sent.`
- Recovery status: not obviously lost; host-side transfer succeeded.
- Next action: inspect immediate and delayed USB behavior.

- Command: `lsusb` immediately after send
- Observation: device still enumerated as `05ac:1223`.
- Recovery status: still reachable on USB.
- Next action: wait briefly and sample again.

- Command: `sleep 2`, `sleep 5`, then `lsusb`
- Observation: the device still enumerated as `05ac:1223` after the delay.
- Recovery status: still reachable on USB.
- Next action: attempt one more DFU-state probe, then stop at the safe boundary and classify the result.

- Command: `./mks5lboot --dfuscan`
- Observation: once again the one-shot scan failed to classify the device and reported `Could not set USB configuration: LIBUSB_ERROR_OTHER`.
- Recovery status: USB presence remains visible via `lsusb`, but this scan is not trustworthy enough for stronger claims.
- Next action: require manual reset and DFU re-entry to confirm recovery after this deliberate-fault test.

## Updated Classification After Deliberate-Fault Test

- Payload build: confirmed.
- `wInd3x` send path: confirmed.
- Observable difference versus the previous silent-loop payload:
  - no new USB-visible behavior was observed; both runs left the host seeing steady `05ac:1223`
- Execution proof from host USB evidence: still inconclusive.
- Failure mode: not a transfer failure; the transfer path worked.
- Recovery status: not yet re-verified after this exact run and still requires manual reset plus DFU re-entry.

#### Step 10: Post-haxdfu execution-path investigation

- Command: source audit of:
  - `/tmp/wInd3x/cmd/wInd3x/cmd_run.go`
  - `/tmp/wInd3x/pkg/exploit/wind3x_n3g.go`
  - `/tmp/wInd3x/pkg/image/image.go`
  - `/tmp/wInd3x/pkg/dfu/dfu.go`
  - `/tmp/wInd3x/cmd/wInd3x/main.go`
  - `/tmp/wInd3x/README.md`
- Observation:
  - `run` does not use any second-stage USB command after upload.
  - `run` calls `haxeddfu.Trigger(...)`, wraps non-IMG1 input with `image.MakeUnsigned(kind, 0, data)`, then sends it with standard DFU `DNLOAD` plus `GETSTATUS`.
  - Nano 3G haxed DFU patches the bootrom `OnImage` callback and does **not** install a second-stage loader trigger.
  - The Nano 3G hook explicitly:
    - calls `DFUBoot::CopyHeaderBody`
    - sets `g_State->dfu_done = 1`
    - sets `g_State->img1_version = "1.0"`
    - sets `g_State->entrypoint = 0`
  - This means Nano 3G execution is expected to happen automatically as part of the normal post-manifest bootrom path; there is no extra host command to “jump”.
- Recovery status: unchanged by this source-only step.
- Next action: verify whether entrypoint/load-address assumptions are correct or whether the observation problem is elsewhere.

- Command: host-side wrapper verification with `wInd3x makedfu /tmp/n3g-test.bin /tmp/n3g-test.dfu -k n3g -e 0x20` and `xxd -g 4 -l 32 /tmp/n3g-test.dfu`
- Observation:
  - IMG1 header fields are serialized exactly as expected:
    - magic `8702`
    - version `1.0`
    - format `0x02`
    - entrypoint `0x00000020`
  - Therefore the image wrapper itself does support a non-zero entrypoint offset.
- Recovery status: unchanged, host-side only.
- Next action: compare this to the Nano 3G runtime hook semantics.

- Observation from code comparison:
  - `cmd/wInd3x/main.go` documents the IMG1 entrypoint as an **offset added to load address `0x22000000`**.
  - `README.md` says flat binaries for haxed DFU are expected to run from `0x22000000`.
  - Rockbox S5L87xx target headers define `IRAM_ORIG 0x22000000`.
  - However, the Nano 3G haxed DFU hook in `wind3x_n3g.go` hardcodes `g_State->entrypoint = 0`, which means the runtime path ignores any non-zero IMG1 entrypoint field.
- Recovery status: unchanged, source-only conclusion.
- Next action: classify what this means for our existing payloads and why the observed USB behavior stayed at `05ac:1223`.

- Conclusion:
  - For our minimal payloads, linked to start at body offset `0`, the hardcoded `entrypoint = 0` is **not** the reason they failed to show a different signature.
  - The load-address assumption `0x22000000` is well supported and is not the likely problem.
  - The post-transfer expectation was too strict: seeing `05ac:1223` in `lsusb` does **not** prove the bootrom is still servicing DFU. A tiny raw payload can take over CPU execution while leaving the USB device electrically enumerated with the same PID until a bus reset.
  - The strongest host-side clue in that direction is that after sending our payloads, `mks5lboot --dfuscan` no longer behaved like a normal DFU probe and instead failed with `Could not set USB configuration: LIBUSB_ERROR_OTHER`.
  - That behavior is consistent with “USB still enumerates, but DFU firmware is no longer responsive”, which is compatible with payload execution.

## Current Best Interpretation

- Nano 3G does **not** require a second host-side trigger after DFU manifest.
- `wInd3x run` is already using the intended execution path for a flat payload at `0x22000000`, entry offset `0`.
- The most likely issue is not entrypoint, load address, or missing transfer steps.
- The more likely issue is the **observation method**:
  - `lsusb` alone is insufficient to decide whether code ran
  - DFU control-transfer responsiveness is the correct signal to watch

## Next Safe Test Direction

- Keep the same payload format (`wInd3x run` on a flat binary at `0x22000000`).
- Immediately after upload, use an active DFU liveness probe instead of passive enumeration:
  - `./mks5lboot --dfuscan`
- If `lsusb` still shows `05ac:1223` but `mks5lboot --dfuscan` cannot talk to the device, treat that as likely payload takeover rather than “still plain DFU”.
- If we want to isolate the Nano 3G entrypoint behavior specifically, the next safe payload-format test is:
  - place the deliberate fault at a non-zero body offset
  - wrap it with `wInd3x makedfu -k n3g -e <offset>`
  - send that explicit IMG1
  This will test whether the current Nano 3G hook’s hardcoded `entrypoint = 0` is suppressing non-zero entry offsets.

#### Step 11: DFU liveness confirmation after payload upload

- Command: `lsusb`
- Observation: after manual reset and DFU re-entry, host USB enumeration showed `Bus 003 Device 037: ID 05ac:1223 Apple, Inc. iPod Classic/Nano 3.Gen (DFU mode)`.
- Recovery status: confirmed before this test; device re-entered DFU normally.
- Next action: confirm active DFU responsiveness before upload.

- Command: `./mks5lboot --dfuscan`
- Observation:
  - `[INFO] libusb: found [05ac:1223] at bus 3, device 37, USB ver. 0200`
  - `[INFO] iPod Nano 3G / Classic, mode: DFU`
  - `[INFO] DFU device state: 2`
- Recovery status: confirmed; normal DFU was alive before upload.
- Next action: send the current minimal payload unchanged.

- Command: `/tmp/wInd3x/wInd3x run /home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/minimal-n3g.bin`
- Observation:
  - `INFO Generating payload...`
  - `INFO Running rce....`
  - `INFO Haxed DFU running!`
  - `INFO Uploading...`
  - `INFO Given firmware file is not IMG1, packing into one...`
  - `INFO Got dfuMANIFEST, image uploaded.`
  - `INFO Image sent.`
- Recovery status: not yet re-checked after this upload.
- Next action: compare passive USB visibility and active DFU liveness immediately afterward.

- Command: `lsusb` immediately after upload
- Observation: device still enumerated as `Bus 003 Device 037: ID 05ac:1223 Apple, Inc. iPod Classic/Nano 3.Gen (DFU mode)`.
- Recovery status: device still visible on USB.
- Next action: actively probe DFU liveness instead of trusting enumeration.

- Command: `./mks5lboot --dfuscan` immediately after upload
- Observation:
  - `[INFO] DFU scan:`
  - `[ERR] Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Recovery status: not lost, but active DFU access no longer behaves like pre-upload ROM DFU.
- Next action: re-check once after a short delay to rule out a transient.

- Command: `sleep 2`, `lsusb`, then `./mks5lboot --dfuscan`
- Observation:
  - `lsusb` still showed `05ac:1223`
  - `mks5lboot --dfuscan` again failed with `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Recovery status: post-upload recovery not yet exercised, but the behavior is stable rather than transient.
- Next action: record the liveness classification and stop before any LCD or hardware-driver work.

## Liveness Classification

- Before upload:
  - `lsusb`: `05ac:1223`
  - `mks5lboot --dfuscan`: normal DFU state `2`
- After upload:
  - `lsusb`: still `05ac:1223`
  - `mks5lboot --dfuscan`: `LIBUSB_ERROR_OTHER`
- Classification: `LIKELY EXECUTION / DFU TAKEOVER`
- Rationale:
  - passive USB enumeration persists
  - active DFU liveness disappears only after payload upload
  - this is the exact signature we were looking for to distinguish “still enumerated” from “still actually in plain DFU”

#### Step 12: Timed-reset visible execution attempt

- Code change: added a separate timed payload in:
  - `tools/ipodnano3g/timed_reset_payload/timed-reset-n3g.S`
  - `tools/ipodnano3g/timed_reset_payload/timed-reset-n3g.lds`
  - `tools/ipodnano3g/timed_reset_payload/Makefile`
- Observation: payload is still MMIO-free and performs only:
  - register-only busy wait
  - final `strb` to `0xffffffff`
- Recovery status: unchanged before send; host-side only.
- Next action: build and verify the timed payload.

- Command: `make` in `tools/ipodnano3g/timed_reset_payload/`
- Observation: build succeeded and produced:
  - `timed-reset-n3g.bin`
  - `timed-reset-n3g.elf`
  - `timed-reset-n3g.map`
- Recovery status: unchanged, host-side only.
- Next action: verify entrypoint, bytes, and instruction stream.

- Command: `arm-elf-eabi-readelf -h timed-reset-n3g.elf`
- Observation: entry point `0x22000000`.
- Recovery status: unchanged, host-side only.
- Next action: inspect exact instructions.

- Command: `arm-elf-eabi-objdump -d timed-reset-n3g.elf`
- Observation: initial timed payload loop:
  - outer count `0x200`
  - inner count `0x80000`
  - then `mvn r0, #0`, `mov r1, #0`, `strb r1, [r0]`
- Recovery status: unchanged, host-side only.
- Next action: increase the visible watch window after the first run window was missed.

- Code change: increased the outer delay count from `0x200` to `0x500` in `timed-reset-n3g.S`
- Observation: verified disassembly now begins with `mov r2, #1280 ; 0x500`.
- Recovery status: unchanged, host-side only.
- Next action: rerun with an explicit user countdown and visual observation window.

- Command: `lsusb` and `./mks5lboot --dfuscan` before timed send
- Observation:
  - `lsusb` showed `05ac:1223`
  - `mks5lboot --dfuscan` reported normal DFU state `2`
- Recovery status: confirmed before timed test.
- Next action: send the timed-reset payload while the user watches continuously.

- Command: `/tmp/wInd3x/wInd3x run /home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/timed_reset_payload/timed-reset-n3g.bin`
- Observation:
  - `INFO Generating payload...`
  - `INFO Running rce....`
  - `INFO Haxed DFU running!`
  - `INFO Uploading...`
  - `INFO Given firmware file is not IMG1, packing into one...`
  - `INFO Got dfuMANIFEST, image uploaded.`
  - `INFO Image sent.`
- Recovery status: not yet re-checked after this send.
- Next action: correlate user-visible behavior with post-send DFU liveness.

- Observation from user: screen stayed black during the extended timed-reset watch window; no Apple logo or visible reset was observed.
- Recovery status: not yet re-entered/rechecked after this exact run.
- Next action: compare host-side DFU liveness after the run.

- Command: `lsusb` and `./mks5lboot --dfuscan` after timed send
- Observation:
  - `lsusb` still showed `05ac:1223`
  - `mks5lboot --dfuscan` failed with `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Recovery status: likely intact based on prior runs, but not yet re-validated after this exact attempt.
- Next action: update classification without overstating visible confirmation.

## Timed-Reset Classification

- Visible delayed reset: **not observed**
- DFU liveness after upload: **lost**
- Best classification for the timed-reset payload: `LIKELY EXECUTION / DFU TAKEOVER`, still **not** `EXECUTION CONFIRMED`
- Rationale:
  - the device again stopped behaving like live DFU after upload
  - but the deliberate delayed fault did not produce a human-visible reset/logo event
  - therefore execution remains strongly indicated by host-side evidence, but not yet visually confirmed

#### Step 13: MMIO-free observability payload comparison

- Code change: added a dedicated comparison set under `tools/ipodnano3g/observability_payloads/`:
  - `immediate-loop-n3g.S`
  - `delayed-loop-n3g.S`
  - `ram-mutate-loop-n3g.S`
  - shared linker script and Makefile
- Observation:
  - Payload A: one immediate self-branch, no writes
  - Payload B: register-only delay loop, then self-branch
  - Payload C: increments a word at `0x22000018` inside its own loaded image
- Recovery status: unchanged before on-device tests.
- Next action: build and verify all three artifacts.

- Command: `make` in `tools/ipodnano3g/observability_payloads/`
- Observation: build succeeded and produced flat binaries, ELFs, and maps for all three payloads.
- Recovery status: unchanged, host-side only.
- Next action: verify instruction streams and side effects.

- Command: `arm-elf-eabi-objdump -d immediate-loop-n3g.elf delayed-loop-n3g.elf ram-mutate-loop-n3g.elf`
- Observation:
  - Payload A:
    - `b 22000000`
  - Payload B:
    - outer delay count `0x300`
    - inner count `0x80000`
    - final branch to self
  - Payload C:
    - `ldr r0, =0x22000018`
    - `mov r1, #0`
    - `add r1, r1, #1`
    - `str r1, [r0]`
    - loop
- Recovery status: unchanged, host-side only.
- Next action: run each payload with before/after and delayed DFU liveness checks.

##### Payload A: immediate loop

- Command sequence:
  - `./mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x run .../immediate-loop-n3g.bin`
  - `./mks5lboot --dfuscan` immediately
  - `sleep 3`
  - `./mks5lboot --dfuscan` delayed
- Observation:
  - before upload: normal DFU state `2`
  - immediately after upload: `LIBUSB_ERROR_OTHER`
  - delayed re-check: `LIBUSB_ERROR_OTHER`
  - `lsusb` remained `05ac:1223`
- Recovery status: not exercised immediately after this run; subsequent manual reset restored DFU.

##### Payload B: delayed loop

- Command sequence:
  - `./mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x run .../delayed-loop-n3g.bin`
  - `./mks5lboot --dfuscan` immediately
  - `sleep 4`
  - `./mks5lboot --dfuscan` delayed
- Observation:
  - before upload: normal DFU state `2`
  - immediately after upload: `LIBUSB_ERROR_OTHER`
  - delayed re-check: `LIBUSB_ERROR_OTHER`
  - `lsusb` remained `05ac:1223`
- Recovery status: not exercised immediately after this run; subsequent manual reset restored DFU.

##### Payload C: RAM-mutate loop

- Command sequence:
  - `./mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x run .../ram-mutate-loop-n3g.bin`
  - `./mks5lboot --dfuscan` immediately
  - `sleep 3`
  - `./mks5lboot --dfuscan` delayed
- Observation:
  - before upload: normal DFU state `2`
  - immediately after upload: `LIBUSB_ERROR_OTHER`
  - delayed re-check: `LIBUSB_ERROR_OTHER`
  - `lsusb` remained `05ac:1223`
- Recovery status: not exercised immediately after this run; subsequent manual reset restored DFU.

## Payload Comparison Result

- All three MMIO-free payloads produced the same host-side signature:
  - normal DFU before upload
  - persistent `05ac:1223` after upload
  - active DFU liveness lost immediately after upload and still lost after delay
- Classification for the A/B/C differentiation test: **still inconclusive**
- Reason:
  - there is no timing difference between “loop now”, “delay then loop”, and “RAM mutate” visible from the DFU liveness probe
  - therefore this comparison did **not** produce a differential signature strong enough to upgrade to `EXECUTION CONFIRMED`
- However, it still reinforces the existing best interpretation:
  - payload upload causes consistent DFU takeover-like behavior on the real device

#### Step 14: Nano 3G entrypoint-offset control test

- Code change: added an explicit control payload under `tools/ipodnano3g/entry_offset_control/`:
  - offset `0x0`: immediate infinite loop (`b .`)
  - deterministic zero padding through offset `0x1f`
  - offset `0x20`: delayed-fault routine
- Observation:
  - this payload is designed so behavior should differ if the IMG1 entry offset `0x20` is honored
  - if runtime always starts at offset `0x0`, it should behave like the immediate-loop baseline
- Recovery status: unchanged before the test.
- Next action: verify the binary layout and explicit wrapped entry field.

- Command: `arm-elf-eabi-objdump -d entry-offset-control-n3g.elf` and `xxd -g 4 -l 80 entry-offset-control-n3g.bin`
- Observation:
  - body offset `0x0` contains only `b 22000000`
  - bytes `0x4..0x1f` are deterministic zeros
  - body offset `0x20` contains the alternate delayed-fault routine
- Recovery status: unchanged, host-side only.
- Next action: wrap with explicit entrypoint offset `0x20`.

- Command: `/tmp/wInd3x/wInd3x makedfu /home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/entry_offset_control/entry-offset-control-n3g.bin /tmp/entry-offset-test-n3g.dfu -k n3g -e 0x20`
- Observation: wrapper generation succeeded.
- Recovery status: unchanged, host-side only.
- Next action: verify the IMG1 header field was written correctly.

- Command: `xxd -g 4 -l 32 /tmp/entry-offset-test-n3g.dfu`
- Observation: IMG1 header begins with:
  - magic `8702`
  - version `1.0`
  - format `0x02`
  - entrypoint field `0x00000020`
- Recovery status: unchanged, host-side only.
- Next action: run the explicitly wrapped image from clean DFU state.

- Command: `./mks5lboot --dfuscan` before send
- Observation:
  - `[INFO] libusb: found [05ac:1223] at bus 3, device 60`
  - `[INFO] iPod Nano 3G / Classic, mode: DFU`
  - `[INFO] DFU device state: 2`
- Recovery status: confirmed before the control test.
- Next action: send the wrapped image.

- Command: `/tmp/wInd3x/wInd3x run /tmp/entry-offset-test-n3g.dfu`
- Observation:
  - `INFO Parsed image. kind="Nano 3G"`
  - `INFO Got dfuMANIFEST, image uploaded.`
  - `INFO Image sent.`
- Recovery status: not yet re-checked after this send.
- Next action: probe immediate and delayed DFU liveness.

- Command: `./mks5lboot --dfuscan` and `lsusb` immediately after send
- Observation:
  - `lsusb` remained `05ac:1223`
  - `mks5lboot --dfuscan` failed with `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Recovery status: device still visible on USB, but no longer behaving as live DFU.
- Next action: repeat after a short delay for comparison against the offset-0 baseline.

- Command: `sleep 4`, then `./mks5lboot --dfuscan` and `lsusb`
- Observation:
  - `lsusb` still remained `05ac:1223`
  - `mks5lboot --dfuscan` again failed with `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Recovery status: post-send recovery not yet re-validated after this exact run.
- Next action: classify the entrypoint-offset behavior.

## Entrypoint-Offset Interpretation

- The explicit `-e 0x20` control image behaved like the offset-0 immediate-loop baseline, not like the delayed-fault routine placed at body offset `0x20`.
- Strongest interpretation: **Nano 3G haxed DFU is forcing runtime entry offset `0` and ignoring the IMG1 entry field**.
- This matches the source audit:
  - `wind3x_n3g.go` explicitly writes `g_State->entrypoint = 0`
- Classification:
  - likely execution / DFU takeover still holds
  - entrypoint-offset behavior is now strongly constrained in favor of forced offset `0`

#### Step 15: Firmware-assisted visibility research

- Command: `find /home/david/.local/share/wInd3x -maxdepth 5 -type f`
- Observation: no decrypted Nano 3G firmware or WTF artifact was found in the local `wInd3x` cache. Additional workspace and `/tmp` searches likewise did not turn up a usable decrypted Nano 3G firmware image.
- Recovery status: unchanged; host-side research only.
- Next action: audit in-tree Nano 3G/S5L8702 code and existing bring-up notes for the first evidence-backed visibility mechanism.

- Command: source audit of:
  - `firmware/target/arm/s5l8702/ipodnano3g/backlight-nano3g.c`
  - `firmware/target/arm/s5l8702/ipodnano3g/pmu-target.h`
  - `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c`
  - `firmware/target/arm/s5l8702/ipodnano3g/lcd-nano3g.c`
  - `firmware/target/arm/s5l8702/lcd-s5l8702.c`
  - `firmware/target/arm/s5l8702/ipodnano3g/piezo-nano3g.c`
  - `bootloader/ipod-s5l87xx.c`
  - `docs/porting/ipodnano3g-ghidra-triage.md`
  - `tools/ghidra/Nano3GHwInitTriage.py`
  - `/tmp/wInd3x/pkg/exploit/wind3x_n3g.go`
  - `/tmp/wInd3x/pkg/cache/cache.go`
  - `/tmp/wInd3x/cmd/wInd3x/cmd_download.go`
  - `/tmp/wInd3x/cmd/wInd3x/cmd_decrypt.go`
  - `/tmp/wInd3x/pkg/exploit/decrypt/decrypt.go`
- Observation:
  - no local decrypted Nano 3G firmware is available yet to validate Apple’s early backlight or LCD bring-up writes
  - the strongest current visible candidate is Nano 3G PMU backlight control via:
    - `D1671_REG_LEDCTL = 0x20`
    - `D1671_LEDCTL_ENABLE = 0x80`
    - used by `backlight-nano3g.c`
    - explicitly manipulated in `pmu-nano3g.c::pmu_preinit()`
  - however, a tiny DFU payload would still need to bring up I2C using `i2c-s5l8702.c`, which includes multiple raw clock/pinmux/controller writes and several `TBC` notes
  - LCD init remains less safe than backlight because `lcd-s5l8702.c` still has multiple `TBC` comments around Nano 3G controller/interface mode
  - piezo tone is source-backed and externally observable, but it is not a visible signal and still requires timer/GPIO setup beyond a single clearly-validated early-boot write
- Recovery status: unchanged; no new payload or hardware action attempted.
- Next action: stop before MMIO testing and prepare the firmware-assisted decryption path.

- Code/documentation change: created `docs/porting/ipodnano3g-visibility-research.md`
- Observation:
  - the research note ranks candidates by safety and records the current blocker
  - safe visible candidate found right now: **no**
  - best visible candidate once Apple firmware evidence is added: **PMU backlight enable**
  - best non-visible external confirmation candidate: **piezo tone**
- Recovery status: unchanged; documentation only.
- Next action: prepare the exact decryption workflow rather than widening hardware access.

- Command plan prepared:
  - `/tmp/wInd3x/wInd3x download wtf /tmp/n3g-wtf-upstream.dfu`
  - `/tmp/wInd3x/wInd3x decrypt /tmp/n3g-wtf-upstream.dfu /tmp/n3g-wtf-decrypted.dfu`
  - import decrypted output into Ghidra and run `tools/ghidra/Nano3GHwInitTriage.py`
- Observation:
  - this is the next firmware-assisted path to obtain Apple-backed evidence for backlight or LCD wake writes
  - current blockers are:
    - no decrypted firmware artifact present locally
    - `download` needs network access
    - `decrypt` needs a connected Nano 3G in DFU
- Recovery status: unchanged; no device-side action taken in this step.
- Next action: stop at the current safe boundary until the decryption workflow is executed.

#### Step 16: Decryption acquisition retry and artifact verification

- Command: `lsusb`
- Observation: the Nano 3G is currently present in DFU as `05ac:1223`, so the device-side precondition for `wInd3x download` / `decrypt` is satisfied.
- Recovery status: unchanged; device remained in DFU.
- Next action: retry the upstream WTF acquisition.

- Command: `/tmp/wInd3x/wInd3x download wtf /tmp/n3g-wtf-upstream.dfu`
- Observation:
  - `wInd3x` started the expected Jingle XML fetch path
  - acquisition failed before any file was written:
    - `could not download iTunes XML`
    - `lookup itunes.apple.com: Temporary failure in name resolution`
- Recovery status: unchanged; host/network-side failure only, no device-side modification.
- Next action: verify whether a decrypted WTF artifact already exists under any alternate path before attempting firmware triage.

- Command: targeted file search across:
  - `/tmp`
  - `/home/david/.local/share/wInd3x`
  - `/home/david/Documents/RockBox_Personal-master`
  - `/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis`
- Observation:
  - no `/tmp/n3g-wtf-decrypted.dfu`
  - no cached `n3g-wtf-decrypted.bin` / `n3g-wtf-upstream.bin` under `wInd3x`
  - the older analysis workspace contains encrypted `OSOS.fw`, `aupd.fw`, `rsrc.fw`, extracted IPSW artifacts, and notes, but no decrypted WTF image
- Recovery status: unchanged; host-side verification only.
- Next action: stop firmware triage until a real decrypted WTF artifact exists locally.

#### Step 17: Decrypted WTF triage for first visibility candidate

- Command: `lsusb`
- Observation: the Nano 3G still enumerated in DFU as `05ac:1223`, so decryption through the device remained safe to attempt.
- Recovery status: unchanged; device still in DFU.
- Next action: retry WTF acquisition with working network access and decrypt it immediately.

- Command: `/tmp/wInd3x/wInd3x download wtf /tmp/n3g-wtf-upstream.dfu`
- Observation:
  - upstream WTF download succeeded
  - `wInd3x` fetched Apple Jingle metadata and downloaded:
    - `http://appldnld.apple.com/iPod/SBML/osx/bundles/041-8552.20121203.Bile3/x12230000_Recovery.ipsw`
  - output artifact created:
    - `/tmp/n3g-wtf-upstream.dfu`
- Recovery status: unchanged; host-side download only.
- Next action: decrypt the downloaded WTF through the connected Nano 3G.

- Command: `/tmp/wInd3x/wInd3x decrypt /tmp/n3g-wtf-upstream.dfu /tmp/n3g-wtf-decrypted.dfu`
- Observation:
  - decryption completed successfully with final `INFO Done!`
  - resulting artifacts:
    - `/tmp/n3g-wtf-upstream.dfu` = `35955` bytes
    - `/tmp/n3g-wtf-decrypted.dfu` = `32816` bytes
- Recovery status: preserved; decryption succeeded and the device remained recoverable.
- Next action: inspect the decrypted IMG1 header and extract the body for code triage.

- Command: `xxd -g 1 -l 128 /tmp/n3g-wtf-decrypted.dfu`
- Observation:
  - decrypted IMG1 header confirms:
    - magic `8702`
    - version `1.0`
    - format `0x02`
    - entrypoint `0`
    - body length `0x7830`
- Recovery status: unchanged; host-side inspection only.
- Next action: extract the IMG1 body and begin firmware triage.

- Command: body extraction to `/tmp/n3g-wtf-decrypted.body.bin`
- Observation:
  - extracted body size is `30768` bytes
  - plaintext strings and disassembly confirm the decrypted WTF is usable for reverse-engineering
- Recovery status: unchanged; host-side extraction only.
- Next action: attempt the planned Ghidra path, then fall back to objdump/xxd triage if Ghidra is unavailable locally.

- Command: local tool check for `analyzeHeadless` / `ghidraRun`
- Observation:
  - Ghidra is not installed on this host
  - firmware triage therefore continued with:
    - `arm-elf-eabi-objdump`
    - `xxd`
    - targeted literal-pool / callsite inspection
- Recovery status: unchanged; host-side tooling limitation only.
- Next action: trace Apple-backed PMU and LCD init paths directly from the decrypted WTF.

- Command: disassembly / source cross-check of:
  - `/tmp/n3g-wtf-decrypted.body.bin`
  - `firmware/target/arm/s5l8702/i2c-s5l8702.c`
  - `firmware/target/arm/s5l8702/ipodnano3g/pmu-target.h`
  - `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c`
  - `firmware/target/arm/s5l8702/ipodnano3g/lcd-nano3g.c`
  - `firmware/target/arm/s5l8702/lcd-s5l8702.c`
- Observation:
  - Apple WTF contains a confirmed I2C0/PMU path:
    - I2C controller base `0x3c600000` is used in the low-level transfer engine at `0x4fc0..0x5334`
    - PMU slave address is `0x73`, matching Rockbox Nano 3G PMU `0xe6 >> 1`
    - read/write wrappers are at:
      - `0x5420` read multiple
      - `0x5474` write multiple
  - Apple WTF does touch PMU registers, but only the following were explicitly observed during this pass:
    - `0x1d`
    - `0x1b`
    - `0x43`
  - No Apple-backed `D1671_REG_LEDCTL (0x20)` use was found in the decrypted WTF during this triage pass.
- Recovery status: unchanged; research only, no payload emitted.
- Next action: inspect the LCD controller path as the remaining candidate for first visible output.

- Command: LCD-side disassembly around `0x42c8..0x4ddc` plus embedded sequence-table dumps near `0x7010` / `0x70d1`
- Observation:
  - Apple WTF contains a real LCD command path using `0x38300000`
  - confirmed controller preamble at `0x45bc` writes:
    - `0x38300000 = 0x80000000`
    - `0x38300000 = 0x80100db1`
    - `0x38300088 = 0x01000000`
    - `0x38300020 = 0x00000033`
    - `0x3830007c = 0x00000804`
  - command-mode helper `0x42c8` waits for `LCD_STATUS` readiness and then rewrites the low configuration bits based on runtime panel grouping
  - sequence interpreter `0x4624` decodes Apple table bytes into:
    - 8-bit command writes
    - timed delay tokens
    - 16-bit command/data writes
  - Apple embedded awake sequence at body offset `0x70d1`, length `11`, decodes as:
    - `CMD8 0x11`
    - delay token `0x3c`
    - `CMD8 0x13`
    - `CMD8 0x29`
  - this matches the shape of the in-tree Nano 3G awake sequence and adds explicit Apple evidence for `0x13` before `0x29`
- Recovery status: unchanged; research only.
- Next action: decide whether the LCD path is reduced enough for a safe test payload.

- Command: command-mode dependency trace through `0x42c8`, `0x4438`, `0x4500`, `0x455c`, `0x47e4`, and `0x4838`
- Observation:
  - the Apple LCD awake path is not yet universal enough for a first payload
  - unresolved dependency:
    - `0x42c8` chooses between at least two command-mode low-bit patterns depending on runtime panel grouping:
      - `0x0c20`
      - `0x0da8`
  - the corresponding panel-type / mode-selection path is only partially traced in this session
  - because of that unresolved selection, a one-size-first payload would still require a speculative choice between two Apple-backed controller modes
- Recovery status: unchanged; no device-side action taken.
- Next action: stop before creating or running a visibility payload and document the exact blocker.

- Documentation change:
  - updated `docs/porting/ipodnano3g-bringup-notes.md`
  - updated `docs/porting/ipodnano3g-visibility-research.md`
  - created `docs/porting/ipodnano3g-first-visibility-sequence.md`
- Observation:
  - the new note records the exact Apple-backed LCD and PMU findings
  - no first-visibility payload was created, because no single non-speculative sequence satisfied the current safety bar
- Recovery status: unchanged; documentation only.
- Next action: continue firmware-assisted tracing of panel-type selection or locate an explicit Apple LEDCTL write before preparing any hardware-writing payload.

#### Step 18: Reduce Apple LCD command-mode selection to a non-speculative rule

- Command: disassembly / table inspection around:
  - `0x42c8`
  - `0x4438`
  - `0x4500`
  - `0x455c`
  - `0x47e4`
  - `0x4838`
  - `0x4f58`
  - embedded table regions near `0x728b`, `0x72bc`, `0x72e4`, `0x730c`
- Observation:
  - the unresolved command-mode choice is now reduced enough to avoid a guessed mode
  - `0x4438` reads a cached selector at `0x2200700c`
  - if the cached value is `4`, it reads GPIO52 and GPIO53 via `0x4f58`
  - `0x4f58` is a GPIO bit-read helper on base `0x3cf00000`, not an LCD ID path
  - selector computation is:
    - `selector = gpio52 | (gpio53 << 1)`
  - command-mode grouping from `0x42c8`:
    - selector `0` / `1` -> low bits `0x0c20`
    - selector `2` / `3` -> low bits `0x0da8`
  - after the Apple preamble value `0x80100db1`, the exact runtime LCD config writes become:
    - `0x80000c21`
    - `0x80000da9`
  - `0x4500` then maps selectors to table families:
    - `0` -> `0x2200730c`
    - `1` -> `0x220072bc`
    - `2` / `3` -> `0x220072e4`
  - selectors `0` / `1` share one command/data helper family and selectors `2` / `3` share another
- Recovery status: unchanged; reverse-engineering only.
- Next action: verify timing semantics and decide whether a prepared-only LCD payload can mirror Apple’s runtime selection without adding guessed support code.

- Command: delay-helper trace around `0x834`, `0x884`, and `0x8c4`
- Observation:
  - Apple delay helpers use a free-running counter at `0x3c7000b4`
  - `0x8c4` waits raw counter ticks and is used with argument `1` around command-mode switching
  - `0x884` multiplies its argument by `1000`, so the awake-table token `0x3c` becomes `60,000` counter ticks
  - this gives an Apple-backed timing basis for the first LCD-awake payload without inventing a software delay constant
- Recovery status: unchanged; reverse-engineering only.
- Next action: prepare, but do not run, a minimal LCD-awake payload that mirrors the Apple preamble, runtime selector, and awake sequence.

#### Step 19: Prepare a minimal Apple-backed LCD-awake payload

- Code change:
  - updated `tools/ipodnano3g/minimal_payload/Makefile`
  - added `tools/ipodnano3g/minimal_payload/lcd-awake-n3g.S`
- Observation:
  - the payload starts at body offset `0`
  - it remains linked for `0x22000000`
  - it performs only:
    - Apple LCD preamble writes from `0x45bc`
    - Apple runtime strap reads from GPIO52 / GPIO53
    - Apple-backed command-mode selection
    - Apple awake commands `0x11`, delay, `0x13`, `0x29`
    - infinite loop
  - it does not access:
    - NAND
    - PMU / backlight
    - USB PHY
    - audio
    - storage
- Recovery status: unchanged; payload prepared only, not executed.
- Next action: build the prepared payload host-side and record its artifact paths.

- Command: `make TARGET=lcd-awake-n3g SRC=lcd-awake-n3g.S` in `tools/ipodnano3g/minimal_payload/`
- Observation:
  - host-side build succeeds
  - prepared artifacts:
    - `tools/ipodnano3g/minimal_payload/lcd-awake-n3g.bin`
    - `tools/ipodnano3g/minimal_payload/lcd-awake-n3g.elf`
    - `tools/ipodnano3g/minimal_payload/lcd-awake-n3g.map`
- Recovery status: unchanged; host-side build only.
- Next action: verify the built artifact before documenting it as the prepared LCD-awake test image.

- Command: artifact verification with:
  - `arm-elf-eabi-readelf -h lcd-awake-n3g.elf`
  - `arm-elf-eabi-size lcd-awake-n3g.elf`
  - `arm-elf-eabi-objdump -d lcd-awake-n3g.elf`
- Observation:
  - entry point is `0x22000000`
  - flat binary size is `276` bytes
  - the disassembly matches the intended Apple-backed action set:
    - LCD preamble writes only
    - GPIO52 / GPIO53 strap read from `0x3cf000c4`
    - command-mode write selecting `0x0c20` or `0x0da8`
    - LCD command writes `0x11`, `0x13`, `0x29`
    - timer-counter read from `0x3c7000b4`
    - infinite loop
  - no PMU, NAND, USB PHY, audio, or storage access is present in the payload
- Recovery status: unchanged; verification only.
- Next action: update bring-up notes and the visibility documents with the final selector rule and prepared-only payload status.

- Documentation change:
  - updated `docs/porting/ipodnano3g-first-visibility-sequence.md`
  - updated `docs/porting/ipodnano3g-visibility-research.md`
  - updated `docs/porting/ipodnano3g-bringup-notes.md`
- Observation:
  - the mode-selection blocker is now closed by Apple-backed GPIO strap evidence
  - a first LCD-awake payload exists for review, but it has not been run
- Recovery status: unchanged; documentation and host-only build only.
- Next action: stop at the current safe boundary until an explicit hardware-test decision is made.

#### Step 20: Extend LCD analysis from awake-only to fuller panel init

- Command: disassembly / table trace around:
  - `0x220031b4`
  - `0x22003278..0x22003294`
  - `0x220048bc`
  - `0x22004968`
  - `0x22004a50`
  - `0x220049a8`
  - body tables `0x7018`, `0x70dc`, `0x728b`, `0x729f`, `0x70d1`
- Observation:
  - the short awake tail is not the whole Apple LCD path
  - earliest confirmed LCD entry goes through:
    - `0x22003ce0(1)`
    - `0x22003ce0(4)`
    - `0x22003d14(0x22007338)`
    - `0x22003cc0()`
    - `0x2200455c()`
  - `0x2200455c()` itself only covers:
    - a gate/resource wrapper via `0x2200428c` -> `0x22000664`
    - direct LCD controller preamble at `0x220045bc`
    - panel-group table caching
  - the larger panel-specific init is dispatched through `0x220048bc`, which
    calls the cached table’s init helper at offset `+0x8`
  - decoded Apple init tables now include:
    - `0x7018` len `0xb9`: large 8-bit init table ending in the awake tail
    - `0x70dc` len `0x179`: large 16-bit init table with many register writes
    - `0x728b` len `0x14`: shorter 8-bit init/awake sequence
    - `0x729f` len `0x0a`: short sleep table
    - `0x70d1` len `0x0b`: short awake tail only
- Recovery status: unchanged; reverse-engineering and documentation only.
- Next action: record that the remaining blocker is no longer “missing LCD commands” but the unresolved runtime callback object behind `0x22007398`.

- Documentation change:
  - updated `docs/porting/ipodnano3g-first-visibility-sequence.md`
  - updated `docs/porting/ipodnano3g-bringup-notes.md`
- Observation:
  - the docs now distinguish:
    - pre-LCD runtime callbacks
    - direct controller preamble
    - panel-group init helpers
    - short awake tail
  - no new payload was built or run in this step
- Recovery status: unchanged; docs only.
- Next action: continue reducing the `0x22007398` callback object before any
  “full LCD init” payload is considered.

#### Step 21: Reconstruct the runtime callback boundary at `0x22007398`

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin`
  - targeted ranges:
    - `0x22003c98..0x22003d38`
    - `0x220031b4..0x220032dc`
    - `0x22004540..0x22004ab0`
  - data inspection:
    - `xxd -g 4 -s 0x7330 -l 0xc0 /tmp/n3g-wtf-decrypted.body.bin`
- Observation:
  - `0x22007398` is still `0` in the static WTF body and no in-image store to
    that pointer was found
  - the only static references are the four wrappers:
    - `0x22003ce0` -> object slot `+0x04`
    - `0x22003d14` -> object slot `+0x08`
    - `0x22003c98` -> object slot `+0x28`
    - `0x22003cc0` -> object slot `+0x2c`
  - observed argument patterns:
    - `+0x04` receives mode values `1`, `3`, `4`
    - `+0x08` receives pointer `0x22007338`
    - `+0x28` receives pointer `0x22007338`
    - `+0x2c` is called with no explicit arguments
  - `0x22007338` is a zeroed static block, which makes it look like service
    context storage rather than a raw MMIO descriptor
  - a different static service-object family exists at `0x220073bc`, but there
    is still no evidence that `0x22007398` points to it
  - the direct LCD path after the callbacks is now clearly reduced:
    - `0x2200455c` handles the gate wrapper and controller preamble
    - `0x220048bc` dispatches a cached panel helper from `0x2200739c + 0x4`
- Recovery status: unchanged; reverse-engineering and documentation only.
- Next action: continue tracing where the runtime environment populates
  `0x22007398` before attempting any wider LCD cold-init payload.

#### Step 22: Trace WTF startup and the external runtime-service boundary

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22001420 --stop-address=0x220014d0`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220014f0 --stop-address=0x22001570`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220003f0 --stop-address=0x22000680`
- Observation:
  - the primary WTF reset entry is `0x22001420`
  - startup immediately calls local helpers:
    - `0x22001124`
    - `0x220010f8`
    - `0x220010e4`
    - `0x22001394`
    - `0x220013fc`
  - startup then:
    - switches CPU modes
    - installs IRQ/SVC stacks
    - copies image/data state
    - zeros state through `0x22007810`
    - calls `0x22002f20`
    - issues `svc 0x00123456` with `r0 = 24`, `r1 = 0x00020026`
  - crucially, this path does **not** preserve incoming `r0`/`r1`/`r2`/`r3`
    into globals before clobbering them, so there is no evidence here that
    `0x22007398` is populated by a simple bootrom register handoff
  - the WTF contains multiple other `svc 0x00123456` callsites, including:
    - `0x22000484`
    - `0x22000640`
    - `0x220013ac`
    - `0x220014c4`
  - this strengthens the theory that the WTF relies on a wider runtime service
    ABI, and that `0x22007398` is more likely an external runtime/loader-owned
    service object than an in-image static object
- Recovery status: unchanged; reverse-engineering and documentation only.
- Next action: keep tracing whether the external service object is installed by
  bootrom, DFU runtime, or a loader-side environment before claiming the LCD
  path can bypass it safely.

#### Step 23: Reduce the minimum stub ABI for `0x22007398`

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003c80 --stop-address=0x22003d40`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg "bl\\s+0x22003c98|bl\\s+0x22003cc0|bl\\s+0x22003ce0|bl\\s+0x22003d14"`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg "22007338|22007398|2200739c"`
- Observation:
  - wrapper ABI is now concrete:
    - `0x22003c98` tail-calls slot `+0x28` with caller `r0` and otherwise
      returns `0x11`
    - `0x22003cc0` tail-calls slot `+0x2c` with no explicit argument setup and
      otherwise returns `0x11`
    - `0x22003ce0` calls slot `+0x04` with caller `r0`, but then forces the
      wrapper return to `0`
    - `0x22003d14` tail-calls slot `+0x08` with caller `r0` and otherwise
      returns `0x11`
  - these wrappers are only called from the LCD state path around `0x220031b4`
  - none of their return values are used for later branching in that path
  - `0x22007338` is not referenced elsewhere by the currently reduced direct
    LCD path; it remains just a context pointer passed into the wrappers
  - this is enough to justify a minimal control-flow stub design:
    - install a table pointer at `0x22007398`
    - populate valid no-op callbacks at offsets `+0x04`, `+0x08`, `+0x28`,
      `+0x2c`
    - return `0` from each callback
  - exact blocker remains:
    - this only reconstructs the wrapper ABI
    - a standalone DFU payload still does not automatically contain the Apple
      WTF routines at `0x2200455c`, `0x220048bc`, and related addresses
- Recovery status: unchanged; reverse-engineering and documentation only.
- Next action: prepare a host-only stub-bootstrap payload that installs the
  service table without touching hardware, and keep the LCD integration itself
  blocked until the Apple code is explicitly translated or embedded.

#### Step 24: Prepare a no-MMIO service-table bootstrap payload

- Code change:
  - added `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.S`
- Command:
  - `make TARGET=lcd-service-stub-n3g SRC=lcd-service-stub-n3g.S` in `tools/ipodnano3g/minimal_payload/`
  - verification:
    - `arm-elf-eabi-readelf -h lcd-service-stub-n3g.elf`
    - `arm-elf-eabi-size lcd-service-stub-n3g.elf`
    - `arm-elf-eabi-objdump -d lcd-service-stub-n3g.elf`
- Observation:
  - host-side build succeeds and produces:
    - `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.bin`
    - `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.elf`
    - `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.map`
  - verified properties:
    - entry point `0x22000000`
    - text size `88` bytes
    - the only write is:
      - `[0x22007398] = 0x22000020`
    - table slots are populated as:
      - `+0x04 = 0x22000010`
      - `+0x08 = 0x22000010`
      - `+0x28 = 0x22000010`
      - `+0x2c = 0x22000010`
    - callback body is a minimal `mov r0, #0 ; bx lr`
    - no LCD, PMU, USB PHY, NAND, storage, or other MMIO access is present
  - the payload intentionally stops after installing the table pointer; it does
    not attempt to call the Apple LCD routines yet
- Recovery status: unchanged; host-only build and verification.
- Next action: keep this as a safe bootstrap artifact and only widen toward an
  LCD payload after the Apple LCD code is explicitly translated or embedded.

#### Step 25: Prepare a self-contained Apple-backed LCD full-init payload

- Code change:
  - added `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.S`
  - added `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.lds`
- Command:
  - `make TARGET=lcd-fullinit-n3g SRC=lcd-fullinit-n3g.S LDSCRIPT=lcd-fullinit-n3g.lds` in `tools/ipodnano3g/minimal_payload/`
  - verification:
    - `arm-elf-eabi-readelf -h lcd-fullinit-n3g.elf`
    - `arm-elf-eabi-size lcd-fullinit-n3g.elf`
    - `arm-elf-eabi-objdump -d lcd-fullinit-n3g.elf --start-address=0x22000000 --stop-address=0x22000120`
    - `arm-elf-eabi-readelf -S lcd-fullinit-n3g.elf`
    - `ls -lh lcd-fullinit-n3g.bin`
- Observation:
  - build succeeds and produces:
    - `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.bin`
    - `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.elf`
    - `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.map`
  - verified properties:
    - entry point `0x22000000`
    - flat binary size `0x8000`
    - bootstrap at offset `0`
    - Apple code/data slices preserved at original VMAs:
      - `0x22000664`
      - `0x22000828`
      - `0x22003b18`
      - `0x22007000`
    - explicit zero pad covers `0x22007830..0x22008000` for Apple work buffers
    - the payload installs a valid runtime service table at `0x22007398`
  - payload execution plan:
    - wrapper sequence via the reduced service ABI
    - `0x2200455c()`
    - `0x220048bc(1, 0)`
    - `0x220048bc(4, 0)`
    - infinite loop
  - no hardware run was performed in this step
- Recovery status: unchanged; host-only build and documentation.
- Next action: stop at the prepared-only boundary and review the larger MMIO
  fan-out before deciding on a single controlled hardware test.

#### Step 26: Single controlled hardware run of `lcd-fullinit-n3g.bin`

- Command before send:
  - `lsusb`
  - `./mks5lboot --dfuscan`
- Observation before send:
  - host saw `05ac:1223`
  - `mks5lboot --dfuscan` reported normal DFU state `2`
- Recovery status: good; clean DFU baseline established.
- Next action: perform exactly one `wInd3x run` with the prepared embedded LCD payload.

- Command:
  - `/tmp/wInd3x/wInd3x run /home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.bin`
- Observation:
  - transfer succeeded cleanly:
    - `Generating payload...`
    - `Running rce....`
    - `Haxed DFU running!`
    - `Given firmware file is not IMG1, packing into one...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Image sent.`
- Recovery status: temporarily unknown until post-send probe and manual reset.
- Next action: classify visible behavior and post-send DFU liveness.

- Observation during the 15-second screen watch window:
  - screen stayed black
  - no flicker
  - no backlight change
  - no Apple logo
  - no visible reset
- Recovery status: still pending post-send and reset checks.
- Next action: capture host-side post-send USB/DFU behavior.

- Command after send:
  - `lsusb`
  - `./mks5lboot --dfuscan`
- Observation after send:
  - `lsusb` still showed `05ac:1223`
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Classification at this point:
  - host-side: DFU takeover / likely execution
  - visible result: no LCD response
- Recovery status: not yet re-confirmed until manual reset + DFU re-entry.
- Next action: force reset, re-enter DFU, and verify state `2`.

- Command after manual reset and DFU re-entry:
  - `lsusb`
  - `./mks5lboot --dfuscan`
- Observation after recovery:
  - host again sees `05ac:1223`
  - `mks5lboot --dfuscan` again reports DFU state `2`
- Recovery status: confirmed.
- Final classification for this single run:
  - `EXECUTION ONLY`
- Reason:
  - payload transfer and DFU takeover were observed
  - recovery remained intact
  - no visible LCD or backlight response occurred
- Next action: stop. Do not run another payload until the result is reviewed.

#### Step 27: Pre-LCD hardware bring-up reduction after the black-screen `lcd-fullinit` run

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22001600 --stop-address=0x22002120`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003000 --stop-address=0x22003200`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220053f0 --stop-address=0x22005540`
  - `rg -n "pmu_preinit|D1671_REG_LEDCTL|backlight_hw_on|lcd_awake|LCD_MODE_P8|LCD_MODE_P18" firmware/target/arm/s5l8702 -S`
- Observation:
  - the missing dependency now appears to be *earlier platform state*, not
    missing LCD code linkage
  - confirmed PMU transport and writes before the LCD path:
    - `0x22005420`: PMU read helper on slave `0x73`
    - `0x22005474`: PMU write helper on slave `0x73`
    - `0x220054b0`: writes PMU reg `0x1d = 0x0a`, then reg `0x1b = 0x01/0x00`
    - `0x220054f8`: read-modify-write PMU reg `0x43` bit `0`
    - callers observed at `0x22003018`, `0x2200304c`, and `0x22003078`
  - confirmed earlier-than-LCD gate/resource writes through `0x22000664`:
    - `0x22001630`: `(r0=0x10000, r1=0, r2=1)`
    - `0x22001650`: `(r0=0x10000, r1=0, r2=0)`
    - `0x220018d8`: `(r0=0x400, r1=0, r2=1)`
    - `0x220018e8`: `(r0=0x1, r1=0, r2=1)`
    - `0x220018fc`: `(r0=0, r1=0x2000, r2=1)`
    - `0x2200205c`: `(r0=0x2007df65, r1=0x1ef49, r2=0)`
    - `0x2200206c`: `(r0=0x06002082, r1=0x1036, r2=1)`
  - additional earlier MMIO evidence:
    - `0x22001634..0x22001640`: clear bits `0..2` at `0x3930003c`
    - `0x220017e8..0x22001804`: toggle bit `1` at `0x38400804`
  - confirmed GPIO / pin-configuration staging before the LCD path:
    - `0x22001f4c` calls `0x22000958`, `0x22003890`, `0x220039fc`, `0x22003350`
    - `0x220030f0` copies GPIO descriptors into the `0x3cf00000`-backed area
    - `0x22003130` rewrites them with per-field adjustments
  - Rockbox cross-check still matches Apple LCD command-mode families:
    - `LCD_MODE_P8 = 0x80000c20`
    - `LCD_MODE_P18 = 0x80000da8`
  - but Rockbox source does **not** by itself justify promoting the whole PMU
    preinit path into a “safe minimal prerequisite sequence”
- Interpretation:
  - the strongest current Apple-backed dependency chain is:
    1. PMU-side setup
    2. earlier resource/clock gates
    3. GPIO / pin configuration
    4. runtime service stage at `0x22007398`
    5. LCD-local gate wrapper
    6. LCD controller preamble
    7. command-mode selection
    8. panel helper / panel tables
    9. awake tail
  - steps 1-3 are now credible blockers for visible LCD output
  - they are still too broad to compress into a new payload without further
    reduction
- Recovery status: unchanged; reverse-engineering and documentation only.
- Next action: keep reducing the earlier PMU / gate / GPIO sequence before any
  new hardware-writing payload is prepared.

#### Step 28: Reduce the pre-LCD chain into explicit PMU / gate / GPIO-command candidates

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22000640 --stop-address=0x220006c0`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22000958 --stop-address=0x22000d30`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003350 --stop-address=0x220033c0`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003890 --stop-address=0x22003b18`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220031b4 --stop-address=0x220032f0`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220061f4 --stop-address=0x220062b0`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22004f0c --stop-address=0x22004f5c`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003bd4 --stop-address=0x22003c18`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg "bl\\s+0x2200160c|bl\\s+0x220031b4|bl\\s+0x22001f4c|bl\\s+0x22003018|bl\\s+0x2200304c|bl\\s+0x220054b0|bl\\s+0x220054f8|bl\\s+0x220061f4|bl\\s+0x22004f0c|bl\\s+0x220030f0|bl\\s+0x22003130|bl\\s+0x22003078"`
- Observation:
  - `0x22000664` is now fully explicit:
    - reads/writes `0x3c500048`
    - reads/writes `0x3c50004c`
    - `r2 == 0` means OR/set, `r2 != 0` means BIC/clear
  - the state-machine ordering is clearer:
    - `0x2200160c` calls `0x220031b4(6)` and then `0x22001f4c()`
    - the shared `0x220031b4` branch for states `5/6` does:
      - `0x220061f4(3)`
      - `0x220030f0(0x2200791c)`
      - `0x22003078(4)`
      - if state `5`: runtime callbacks then `0x2200455c()`
    - a different branch later calls `0x22003078(1)`, which is the path that
      reaches the observed PMU writes
  - `0x22003078` only performs the PMU helper sequence when input is `1`:
    - `0x22003018(0)` -> `0x220054b0`
    - `0x2200304c(0)` -> `0x220054f8`
    - when input is `4`, `0x22003078` returns without doing the PMU writes
  - `0x22001f4c` now looks less like pure pinmux and more like early
    clock/reset setup:
    - `0x22000958` mutates `0x3c500000` register slots
    - `0x22003890` mutates `0x3c500020`, `0x3c500024`, or `0x3c500028`
      together with `0x3c500044/0x40` state
    - `0x220039fc` toggles specific bits in `0x3c500000` register slots
    - `0x22003350` sets/clears bits at `0x39a000c0 + group*4`
  - `0x220030f0` / `0x22003130` are now reduced as GPIO state transfer helpers:
    - `0x22003130(0x2200791c)` snapshots live `0x3cf00000` descriptor state to
      RAM
    - `0x220030f0(0x2200791c)` restores that RAM-backed descriptor set to the
      live GPIO bank
    - `0x2200791c` lies beyond the decrypted body and therefore sits in the
      zero-padded work area in the embedded payload
  - `0x220061f4` is the clearest direct GPIO-command helper on the LCD path:
    - `0x220061f4(2)` issues commands through `0x22004f0c` to pin `74`, then
      pin `72`
    - `0x220061f4(4)` issues a command through `0x22004f0c` to pin `74`
    - `0x22004f0c` writes the encoded command to `0x3cf00200`
  - exact unresolved point remains:
    - which of these early clock/reset and GPIO-command steps are genuinely
      required for first visible LCD output, versus broader platform state
- Interpretation:
  - the earlier chain is now better separated:
    1. PMU writes are real but sit on a different `0x22003078(1)` state path
       than the immediate `0x220031b4(6)` startup path, which instead calls
       `0x22003078(4)`
    2. `0x22001f4c` is mainly early clock/reset work, not just pinmux
    3. `0x220061f4` is the stronger direct GPIO-precondition candidate near the
       LCD path
    4. `0x220030f0` / `0x22003130` are state-buffer transfer helpers and do not
       by themselves explain the missing visible output
- Recovery status: unchanged; reverse-engineering and documentation only.
- Next action: continue reducing whether `0x220061f4`, the narrow-mask
  `0x22000664` calls, and some subset of the PMU path are sufficient
  preconditions before the LCD-local code.

#### Step 29: Re-rank the remaining blockers around exact LCD-local gate masks vs sideband GPIO work

- Command:
  - `sed -n '1,120p' tools/ghidra/Nano3GHwInitTriage.py`
  - `sed -n '1,120p' firmware/target/arm/s5l8702/ipodnano3g/nand-nano3g.c`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x2200428c --stop-address=0x220042d0`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003b18 --stop-address=0x22003bd4`
  - `sed -n '1,220p' firmware/target/arm/s5l8702/gpio-s5l8702.c`
  - `sed -n '1,120p' firmware/target/arm/s5l8702/spi-s5l8702.c`
  - `sed -n '468,500p' firmware/target/arm/s5l8702/ipod6g/storage_ata-6g.c`
- Observation:
  - local triage notes classify `0x3c200000..0x3c200100` as:
    - `WHEEL_OR_NAND_COLLISION`
  - in-tree Nano 3G NAND notes also state:
    - `0x3c200000` is clickwheel on S5L8702, not NAND FMC
  - this weakens `0x220061f4` as an LCD-specific clue because the LCD-adjacent
    call is `0x220061f4(3)`, which drops into:
    - `0x22005ef4`
    - `0x220060e0`
    - both operate on `0x3c200000`
    - and configure pins `72..75` through `0x22004f0c`
  - `0x22004f0c` now decodes cleanly using in-tree GPIOCMD examples:
    - command format is `(group << 16) | (index << 8) | op`
    - special case for mode `1` becomes:
      - `0xE` = output low
      - `0xF` = output high
    - thus pin `72` is group `9`, index `0`
    - pin `74` is group `9`, index `2`
  - exact LCD-local gate call is stronger evidence than the sideband GPIO path:
    - `0x2200428c` does `0x22000664(r0=2, r1=0, r2=1)`
    - because `r2 != 0`, effect is:
      - clear bit `1` in `0x3c500048`
    - `0x220042b8` later restores the saved prior state
  - `0x22003b18` confirms the saved state is derived from whether the bit was
    already present in `0x3c500048/0x4c`
- Interpretation:
  - exact remaining blockers are now better ranked:
    1. exact `0x22000664` masks near startup and the LCD-local gate wrapper
    2. descriptor-state dependency at `0x2200791c`
    3. PMU inclusion/exclusion decision
    4. `0x220061f4` is currently **demoted** because its strongest observed path
       points at the `0x3c200000` sideband block, not clean LCD evidence
- Recovery status: unchanged; reverse-engineering and documentation only.
- Next action: keep narrowing the startup gate masks and descriptor dependency
  before considering any new pre-LCD payload.

#### Step 30: Separate the immediate LCD state path from adjacent PMU and later gate paths

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220015c0 --stop-address=0x22002120`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003000 --stop-address=0x22003220`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22002f20 --stop-address=0x22003010`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220053f0 --stop-address=0x22005540`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003b00 --stop-address=0x22003c90`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22004260 --stop-address=0x220045e0`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg -n "220030f0|22003130|2200791c|220054b0|220054f8|22000664|2200160c|22001698|22001f4c|220031b4|2200455c|22003078"`
- Observation:
  - startup entry continues through:
    - `0x22001420 -> 0x22002f20 -> 0x2200160c`
  - `0x2200160c` definitely performs:
    - `0x220031b4(6)`
    - `0x22001f4c()`
    - `0x22000664(0x10000, 0, 1)`
    - `0x3930003c &= ~0x7`
    - `0x22000664(0x10000, 0, 0)`
  - the later gate calls:
    - `0x220018d8`
    - `0x220018e8`
    - `0x220018fc`
    are not on that direct startup function; they sit inside the later
    `0x22001698` path reached via the tail branch at `0x22002fe4`
  - `0x220031b4(5)` / `0x220031b4(6)` share the pre-LCD path at `0x22003258`:
    - `0x220061f4(3)`
    - `0x220030f0(0x2200791c)`
    - `0x22003078(4)`
  - only state `5` then performs:
    - `0x22003ce0(1)`
    - `0x22003ce0(4)`
    - `0x22003d14(0x22007338)`
    - `0x22003cc0()`
    - `0x2200455c()`
  - the PMU writes remain real, but they are now clearly off this path:
    - `0x22003078(1)` calls `0x22003018(0)` and `0x2200304c(0)`
    - those reach:
      - `0x220054b0` -> PMU regs `0x1d`, `0x1b`
      - `0x220054f8` -> PMU reg `0x43` bit `0`
    - `0x22003078(4)` does **not** execute those PMU helpers
  - `0x2200791c` is now a firmer blocker:
    - state `5/6` restores it through `0x220030f0(0x2200791c)`
    - the only in-body producer is state `2/3` via
      `0x22003130(0x2200791c)`
    - there are no direct `0x2200791c` reads inside the LCD-local functions
      themselves; the dependency is in the earlier state-machine path
- Interpretation:
  - PMU writes are currently **exclude for next LCD candidate** because they are
    not on the immediate state `5/6` path that reaches `0x2200455c`
  - startup gate work now splits into:
    1. direct startup path:
       - `0x22001630`
       - `0x22001650`
       - inline clear at `0x3930003c`
       - `0x22001f4c` helper block including `0x2200205c` / `0x2200206c`
    2. later-path gates in `0x22001698`:
       - `0x220018d8`
       - `0x220018e8`
       - `0x220018fc`
       - these are currently excluded from a minimal pre-LCD candidate
  - the remaining unresolved safe blocker is now narrower:
    - whether a standalone payload can synthesize or safely bypass the
      `0x2200791c` descriptor restore that Apple state `5/6` expects
- Recovery status: unchanged; analysis and documentation only.
- Next action: keep reducing `0x22001f4c` and the `0x2200791c` descriptor
  dependency; do not prepare or run a new pre-LCD payload yet.

#### Step 31: Decode the `0x2200791c` GPIO descriptor buffer and classify whether it can be bypassed

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220030f0 --stop-address=0x220031b4`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22004f0c --stop-address=0x22004fc0`
  - `sed -n '1360,1445p' firmware/export/s5l87xx.h`
  - `sed -n '1,180p' firmware/target/arm/s5l8702/gpio-s5l8702.c`
- Observation:
  - prior docs saying “15 entries” were off by one; both helpers iterate
    `0..15`, so the buffer is **16 entries**
  - `0x22003130` is the live-hardware-to-buffer direction:
    - base `0x3cf00000 + group*0x20`
    - reads:
      - `+0x00` = `PCON(group)`
      - `+0x04` = `PDAT(group)`
      - `+0x0c` = `PUNB(group)`
      - `+0x10` = `PUNC(group)`
    - writes one 8-byte descriptor per group at `0x2200791c + group*8`
    - descriptor fields are:
      - `+0x00..+0x03`: packed `PCON(group)` nibble config
      - `+0x04`: `PUNB(group)` low byte
      - `+0x05`: `PUNC(group)` low byte
      - `+0x06..+0x07`: untouched
    - before storing the packed config word, it walks all 8 nibbles and rewrites
      any nibble equal to `1` (output mode) into:
      - `0xE` if the corresponding `PDAT` bit is `0`
      - `0xF` if the corresponding `PDAT` bit is `1`
    - this exactly matches the in-tree S5L8702 GPIO representation used in
      `gpio-s5l8702.c`
  - `0x220030f0` is the buffer-to-live-hardware direction:
    - writes descriptor `+0x00..+0x03` back to live `PCON(group)` at `+0x00`
    - writes descriptor `+0x04` back to `PUNB(group)` at `+0x0c`
    - writes descriptor `+0x05` back to `PUNC(group)` at `+0x10`
    - it does **not** write `PDAT(group)` directly
  - state-machine role is now clearer:
    - earlier state `1/2` snapshots the live GPIO group state with
      `0x22003130(0x2200791c)`
    - later state `4/5` restores that saved state with
      `0x220030f0(0x2200791c)`
    - the same later path also performs `0x220061f4(3)` before the restore
  - this makes the descriptor buffer look like generic GPIO-state preservation
    around the sideband `0x220061f4` path, not like a private LCD config table
- Interpretation:
  - current decision on `0x2200791c` is now:
    - **BYPASS SAFE** for a standalone LCD visibility payload that explicitly
      omits `0x220061f4` and does not try to replay Apple state `4/5`
    - **not** synthesis-safe yet for a state-machine-faithful replay, because
      no Apple-backed descriptor contents beyond the live snapshot mechanism
      exist in the static body
  - this removes `0x2200791c` as the primary blocker for a standalone LCD test;
    the remaining blocker shifts back to the earlier `0x22001f4c` helper block
    and which subset of that startup gate/reset work is actually LCD-critical
- Recovery status: unchanged; analysis and documentation only.
- Next action: keep reducing `0x22001f4c`; do not run any new payload yet.

#### Step 32: Correct and reduce the direct `0x22001f4c` helper block enough to prepare a pre-gate LCD payload

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22001f4c --stop-address=0x22002090`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22000958 --stop-address=0x22000d40`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220008f0 --stop-address=0x22000958`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22000d6c --stop-address=0x22000df8`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003890 --stop-address=0x22003a60`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220039fc --stop-address=0x22003b20`
  - `arm-elf-eabi-readelf -h tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.elf`
  - `arm-elf-eabi-size tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.elf`
  - `arm-elf-eabi-objdump -D tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.elf | sed -n '1,220p'`
  - `make TARGET=lcd-pregate-fullinit-n3g SRC=lcd-pregate-fullinit-n3g.S LDSCRIPT=lcd-pregate-fullinit-n3g.lds`
- Observation:
  - corrected call-graph finding:
    - `0x22001f4c` does **not** call `0x22003350`
    - earlier notes that grouped `0x22003350` under `0x22001f4c` were wrong;
      that helper belongs to the later excluded path
  - corrected `0x22001f4c` sequence is:
    - `0x22003c18(0)`
    - `0x22000958(0,0,1)`
    - `0x22000958(4,0,1)`
    - `0x22000958(2,0,1)`
    - `0x22003890(1,0xd8)`
    - `0x22003890(2,0xd8)`
    - `0x22003890(3,0xd8)`
    - `0x22000958(4,3,4)`
    - `0x22000958(2,3,2)`
    - `0x22000958(0,3,1)`
    - `0x22000958(5,3,0x12)`
    - `0x22000958(8,0,4)`
    - `0x220039fc(0,1)` (no-op)
    - `0x220039fc(5,1)`
    - `0x220039fc(15,0)`
    - `0x220039fc(14,0)`
    - `0x220039fc(6,0)`
    - `0x22003890(1,0)`
    - `0x22003890(2,0)`
    - `0x22000664(0x2007df65, 0x1ef49, 0)`
    - `0x22000664(0x06002082, 0x1036, 1)`
    - `0x22003c18(1)`
  - the helper block is broad but fully Apple-backed and explicit:
    - `0x3c500000`
    - `0x3c500004`
    - `0x3c500008`
    - `0x3c50000c`
    - `0x3c500010`
    - `0x3c500020`
    - `0x3c500024`
    - `0x3c500028`
    - `0x3c500040`
    - `0x3c500044`
    - `0x3c500048`
    - `0x3c50004c`
    - `0x39900000`
    - `0x39300000`
    - `0x38100000`
    - `0x38501000`
  - direct overlap with later LCD-local path is now explicit:
    - `0x2200206c -> 0x22000664(0x06002082, 0x1036, 1)`
    - this clears bit `1` in `0x3c500048`
    - `0x2200428c` later clears the same bit for the LCD wrapper
  - candidate decision therefore changed from “still blocked” to:
    - **INCLUDE MINIMAL SUBSET**
    - meaning:
      - keep the full corrected `0x22001f4c` block
      - keep direct startup gates from `0x22001630`, inline
        `0x3930003c &= ~0x7`, and `0x22001650`
      - still omit:
        - PMU writes
        - `0x220061f4` / `0x3c200000`
        - `0x2200791c` restore path
  - prepared new host-side artifact:
    - `tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.S`
    - `tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.lds`
    - `tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.elf`
    - `tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.bin`
    - `tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.map`
  - host-side validation:
    - entry point `0x22000000`
    - ELF text size `0x4090`
    - `_start` does:
      - service table setup
      - panel selector/state seed
      - `apple_01f4c()`
      - `apple_00664(0x10000, 0, 1)`
      - inline `0x3930003c &= ~0x7`
      - `apple_00664(0x10000, 0, 0)`
      - reduced service-wrapper and LCD-local full init
- Interpretation:
  - the remaining uncertainty is no longer “what to include before LCD-local
    init”; it is now only whether this prepared pre-gate subset is sufficient on
    hardware
  - because every included write is Apple-backed and the excluded paths remain
    intentionally omitted, the new payload is prepared but still unrun
- Recovery status: unchanged; no hardware run performed.
- Next action: if the user chooses to continue, perform one controlled DFU test
  of `lcd-pregate-fullinit-n3g.bin` and stop immediately for observation and
  recovery check.

#### Step 33: Abort the planned `lcd-pregate-fullinit-n3g` run because the host-side DFU baseline was not clean

- Command:
  - `lsusb`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `sleep 2`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - the Nano enumerated as:
    - `05ac:1223 Apple, Inc. iPod Classic/Nano 3.Gen (DFU mode)`
  - but the required clean DFU pre-check failed twice:
    - `[INFO] DFU scan:`
    - `[ERR] Could not init USB library: LIBUSB_ERROR_OTHER`
  - because the baseline `dfuscan` was not normal, the payload was **not**
    uploaded
- Interpretation:
  - this was not a hardware-payload result
  - it was an aborted test due to a host-side USB/DFU baseline issue
  - do not mix this attempt with previous Nano-side execution observations
- Recovery status:
  - device still enumerated as `05ac:1223`
  - no payload was sent
- Next action:
  - re-establish a clean DFU baseline before any future run attempt

#### Step 34: Confirm the `LIBUSB_ERROR_OTHER` baseline issue was sandbox-related, not a Nano DFU failure

- Command:
  - `lsusb`
  - `ls -l /dev/bus/usb/003 /dev/bus/usb/003/084`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
    - run outside the sandbox
- Observation:
  - inside the sandbox:
    - `lsusb` could still see `05ac:1223`
    - but `/dev/bus/usb/003/084` was not visible
    - `mks5lboot --dfuscan` failed with:
      - `[ERR] Could not init USB library: LIBUSB_ERROR_OTHER`
  - outside the sandbox:
    - `/dev/bus/usb/003/084` existed as:
      - `crw-rw-r--+ 1 root root 189,339`
    - `./utils/mks5lboot/mks5lboot --dfuscan` succeeded:
      - `[INFO] libusb: found [05ac:1223] at bus 3, device 84, USB ver. 0200`
      - `[INFO] iPod Nano 3G / Classic, mode: DFU`
      - `[INFO] DFU device state: 2`
- Interpretation:
  - the earlier aborted `lcd-pregate-fullinit` run attempt was a host sandbox
    visibility problem, not a Nano DFU problem
  - future DFU probes in this environment should be treated as requiring
    outside-sandbox USB access
- Recovery status:
  - unchanged; no payload was sent
- Next action:
  - the single controlled `lcd-pregate-fullinit-n3g.bin` run can be retried with
    an outside-sandbox DFU baseline check

#### Step 35: Reduce the minimal power / backlight candidate set to one Apple-backed PMU write and prepare `lcd-powerstep-n3g`

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22002ff0 --stop-address=0x22003100`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220053f0 --stop-address=0x22005540`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220061d0 --stop-address=0x22006260`
  - `sed -n '70,110p' firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c`
  - `sed -n '330,390p' firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c`
  - `sed -n '1,220p' firmware/target/arm/s5l8702/ipodnano3g/backlight-nano3g.c`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22005140 --stop-address=0x22005540`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22004ff8 --stop-address=0x22005178`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22000d10 --stop-address=0x22000d24`
  - `make TARGET=lcd-powerstep-n3g SRC=lcd-powerstep-n3g.S LDSCRIPT=lcd-powerstep-n3g.lds`
  - `arm-elf-eabi-size tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.elf`
  - `arm-elf-eabi-readelf -h tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.elf`
  - `arm-elf-eabi-objdump -d tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.elf`
- Observation:
  - reg `0x1b` remains a poor first visibility step:
    - Apple helper `0x220054b0` writes it
    - in-tree Nano 3G code already uses `0x1b` for `pmu_hdd_power(bool on)`
  - reg `0x43` bit `0` also remains excluded:
    - Apple helper `0x220054f8` exists
    - but the only observed call site is `0x2200304c(0)`, which clears bit `0`
    - using the opposite polarity as an “enable” would be a guess
  - no single safe reset-style GPIO pulse was found:
    - the only clear candidate remains under `0x220061f4`
    - that still falls into the excluded `0x3c200000` sideband block
  - the narrowest remaining exact Apple-backed candidate is:
    - PMU slave `0x73`
    - register `0x1d`
    - value `0x0a`
  - prepared new host-side artifact:
    - `tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.S`
    - `tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.lds`
    - `tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.elf`
    - `tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.bin`
    - `tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.map`
  - host-side validation:
    - entry point `0x22000000`
    - ELF size `0x45f4`
    - flat binary size `0x8000`
    - `_start` adds exactly one new hardware action before the existing
      `lcd-pregate-fullinit` sequence:
      - `apple_05474(0x1d, 1, &0x0a)`
  - embedded PMU transport is still Apple-backed:
    - write helper at `0x22005474`
    - read/write transport at `0x22005420`, `0x22005178`, `0x22005258`
    - I2C controller block `0x3c600000`
    - timer basis `0x3c7000b4`
- Interpretation:
  - this is the smallest Apple-backed “powerstep” payload that is still honest
    about the evidence
  - it is **not** a proven backlight-enable payload
  - it is a single exact PMU write added ahead of the pregate + LCD path
  - risk remains medium because PMU/I2C hardware is now exercised, but this is
    still narrower than replaying the broader PMU branch
- Recovery status:
  - unchanged; host-side build and documentation only
- Next action:
  - if the user chooses to continue later, do one controlled DFU run of
    `lcd-powerstep-n3g.bin` and stop immediately for observation and recovery

#### Step 36: Add one more Apple-backed PMU action and prepare `lcd-powerstep2-n3g`

- Command:
  - `make TARGET=lcd-powerstep2-n3g SRC=lcd-powerstep2-n3g.S LDSCRIPT=lcd-powerstep2-n3g.lds`
  - `arm-elf-eabi-size tools/ipodnano3g/minimal_payload/lcd-powerstep2-n3g.elf`
  - `arm-elf-eabi-readelf -h tools/ipodnano3g/minimal_payload/lcd-powerstep2-n3g.elf`
  - `arm-elf-eabi-objdump -d tools/ipodnano3g/minimal_payload/lcd-powerstep2-n3g.elf`
- Observation:
  - prepared new payload sources:
    - `tools/ipodnano3g/minimal_payload/lcd-powerstep2-n3g.S`
    - `tools/ipodnano3g/minimal_payload/lcd-powerstep2-n3g.lds`
  - exact added PMU action relative to `lcd-powerstep-n3g`:
    - `apple_054f8(0)`
    - effect: read PMU reg `0x43`, clear bit `0`, write it back
  - exact PMU sequence in `_start`:
    - `apple_05474(0x1d, 1, &0x0a)`
    - `apple_054f8(0)`
  - host-side validation:
    - entry point `0x22000000`
    - ELF size `0x45f4`
    - flat binary size `0x8000`
    - no new subsystem beyond the already embedded Apple PMU/I2C transport and
      the existing pregate + LCD-local path
- Interpretation:
  - this is the smallest honest follow-on PMU refinement after the first
    power-step candidate
  - it keeps the exact observed Apple `0x43` clear polarity and avoids guessing
    the opposite direction
  - risk stays medium and bounded
- Recovery status:
  - unchanged; host-side build and documentation only
- Next action:
  - if the user chooses to continue later, do one controlled DFU run of
    `lcd-powerstep2-n3g.bin` and stop immediately for observation and recovery

#### Step 37: Isolate the remaining LCD visibility candidates and classify pins 72..75 as `NOT_LCD`

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg -n "220054b0|220054f8|22003018|2200304c|22003078|220031b4|22003258|220061f4|22005ef4|220060e0|22004f0c|22004f58|22004f88|220030f0|22003130"`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003000 --stop-address=0x22003320`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22005ed8 --stop-address=0x22006290`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220060e0 --stop-address=0x22006180`
  - `sed -n '75,95p' firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c`
  - `sed -n '420,450p' firmware/export/s5l87xx.h`
  - `sed -n '20,40p' firmware/target/arm/s5l8702/ipodnano3g/nand-nano3g.c`
- Observation:
  - PMU reg `0x1b` still appears only through the early Apple PMU helper
    `0x220054b0`
  - that helper is reached on the separate branch:
    - `0x22003078(1)` -> `0x22003018(0)` -> `0x220054b0`
  - local Nano 3G source still maps:
    - `pmu_hdd_power(bool on) { pmu_write(0x1b, on ? 1 : 0); }`
  - so `0x1b` remains more strongly correlated with storage / HDD power than
    LCD visibility
  - the LCD-adjacent state path remains:
    - states `5/6`: `0x220061f4(3)` -> `0x220030f0(0x2200791c)` ->
      `0x22003078(4)`
    - state `5` then continues into the LCD routines
  - mode `3` of `0x220061f4` is now explicit:
    - `0x22006260` -> delay `25`
    - call `0x22005ef4`
    - tail-call `0x220060e0`
  - `0x22005ef4`:
    - configures pins `72`, `73`, `74`, `75` via `0x22004f0c` with op `2`
    - then performs multiple direct writes to `0x3c200000`
  - `0x220060e0`:
    - continues polling and programming `0x3c200000`
    - uses sideband values `0x8000063a` / `0x8000062a`
    - stores sideband state at `0x22007788`
  - local Rockbox definitions and Nano 3G NAND notes still identify
    `0x3c200000` on S5L8702 as clickwheel-side hardware, not proven LCD MMIO
- Additional observation:
  - `0x22005ef4` is not a reset-pulse pattern:
    - `GPIO72 <- op2`
    - delay `1`
    - `GPIO73 <- op2`
    - `GPIO74 <- op2`
    - `GPIO75 <- op2`
  - mode `3` of `0x220061f4` is:
    - delay `25`
    - `0x22005ef4`
    - `0x220060e0`
  - `0x2200616c` inside that path:
    - clears and re-sets bit `0x200000` in `0x3c200000`
    - writes command payload to `0x3c20001c`
    - sets bit `0` in `0x3c200004`
    - polls `0x3c20000c`
  - Nano 3G GPIO defaults show group `9` is `0x22222222`
    - pins `72..79` are therefore default `op2`
    - so the sideband path restoring `72..75` to `op2` matches wheel-side
      alternate-function setup
- Interpretation:
  - `PMU 0x1b` is now best classified as unrelated / storage-oriented
  - `0x220061f4(3)` is on the immediate LCD-adjacent branch, but its concrete
    behavior now points more strongly at clickwheel-side peripheral setup than
    LCD reset/power
  - final decision for this pass:
    - **NOT_LCD**
- Recovery status:
  - unchanged; reverse-engineering and documentation only
- Next action:
  - do not prepare or run `lcd-powerstep3-n3g.bin`
  - move the search away from PMU `0x1b` and pins `72..75`; they no longer look
    like the next safe LCD visibility lead

#### Step 38: Search outside rejected PMU / wheel-side paths for the real display visibility dependency

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg -n "mov\\s+r0, #0x20|mov\\s+r0, #32|#0x20\\b|#32\\b|22005474|22005420|220054b0|220054f8|22003018|2200304c|220031b4|22004540|2200455c|220048bc|22001698|220017e8|22001804|220018d8|220018e8|220018fc|38400804|3930003c|3c500048|3c50004c"`
  - `rg -n "LEDCTL|backlight|brightness|screen on|screen off|sleep|wake|display on|display off|0x20|D1671_REG_LEDCTL|D1671_LEDCTL_ENABLE|0x38400804|0x3930003c|3c500048|3c50004c" firmware/target/arm/s5l8702 docs/porting -g '!build*'`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22005420 --stop-address=0x22005540`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg -n "bl\\s+0x22005420|bl\\s+0x22005474|bl\\s+0x220054b0|bl\\s+0x220054f8|bl\\s+0x2200455c|bl\\s+0x220048bc|bl\\s+0x22004540|bl\\s+0x2200428c|bl\\s+0x22001f4c|bl\\s+0x22001698|bl\\s+0x2200160c"`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22002fc0 --stop-address=0x22003220`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003220 --stop-address=0x220032e8`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x2200160c --stop-address=0x22001920`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22004540 --stop-address=0x220045b8`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg -n "bl\\s+0x22004f0c|bl\\s+0x22004f58|bl\\s+0x22004500|bl\\s+0x22004598|bl\\s+0x220045bc|bl\\s+0x220061f4|bl\\s+0x22003c18|bl\\s+0x2200205c|bl\\s+0x2200206c"`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22004ec0 --stop-address=0x22004fe8`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22001f4c --stop-address=0x220020b0`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22004500 --stop-address=0x22004540`
  - `rg -n "38400804|0x38400804|0x3930003c|39300000|3c500048|3c50004c|CLOCKGATE_LCD_2|CLOCKGATE|WHEEL_BASE|LCD_BASE|GPIO72|GPIO74|LEDCTL|0x43|0x1d" firmware docs/porting -g '!build*'`
- Observation:
  - No new Apple-backed PMU display/backlight helper was found in the decrypted
    WTF.
  - The only concrete PMU helpers still exposed are:
    - `0x220054b0`
      - reg `0x1d = 0x0a`
      - reg `0x1b = on/off`
    - `0x220054f8`
      - PMU reg `0x43` bit `0` read-modify-write
  - Cross-reference search found no additional callers using:
    - `LEDCTL (0x20)`
    - brightness-like PMU registers such as `0x28` / `0x29`
  - The LCD state machine still reaches the LCD-local path through:
    - `0x22003278..0x22003294`
    - `0x22003ce0(1)`
    - `0x22003ce0(4)`
    - `0x22003d14(0x22007338)`
    - `0x22003cc0()`
    - `0x2200455c()`
  - The only remaining reset-like pulse outside rejected wheel-side code is in
    the later `0x22001698` startup path:
    - `0x220017e8..0x22001804`
    - set bit `1` in `0x38400804`
    - delay `500`
    - clear bit `1`
  - That same later path also performs three exact narrow gate clears:
    - `0x220018d8`: `0x3c500048 &= ~0x400`
    - `0x220018e8`: `0x3c500048 &= ~0x1`
    - `0x220018fc`: `0x3c50004c &= ~0x2000`
  - Those later-path operations appear beside service/object-mediated calls using
    IDs `19` and `33`, not as isolated one-off LCD-local writes.
- Interpretation:
  - The Apple WTF still does **not** provide a clean explicit backlight enable
    candidate outside already tested PMU regs `0x1d` and `0x43`.
  - The strongest remaining non-PMU, non-wheel-side visibility lead is now the
    later `0x22001698` gate/reset cluster:
    - `0x38400804` bit-1 pulse
    - `0x3c500048 &= ~0x400`
    - `0x3c500048 &= ~0x1`
    - `0x3c50004c &= ~0x2000`
  - But that cluster is still coupled to the broader later startup path and
    cannot yet be justified as a single isolated next hardware action.
  - Decision for this pass:
    - **STILL_BLOCKED**
- Recovery status:
  - unchanged; reverse-engineering and documentation only
- Next action:
  - keep the search focused on the later `0x22001698` gate/reset cluster
  - do **not** prepare or run `lcd-visibility-next-n3g.bin` yet

#### Step 39: Reduce the later `0x22001698` gate/reset cluster and decide if it can stand as one bounded pre-LCD step

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22001698 --stop-address=0x22001b40`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg -n "38400804|220032fc|220024d8|22001c70|22006380|22006430|22006558|22003b5c|22003c18|22000664|22000884|220008c4"`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220032fc --stop-address=0x22003380`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22003838 --stop-address=0x220038a8`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22000648 --stop-address=0x220006c0`
  - `python3 - <<'PY'`
    - `base=0x3c500000`
    - `print(hex(base-0x3700000))`
    - `print(hex(base-0x4400000))`
    - `print(hex(base-0x2b00000))`
    - `PY`
  - `rg -n "0x38100000|0x38e00000|0x39300000|0x39900000|CLOCKGATE_LCD|CLOCKGATE_GPIO|CLOCKGATE_TIMER|CLOCKGATE_CWHEEL|CLOCKGATE_SPI0|CLOCKGATE_SPI1" firmware/export firmware/target/arm/s5l8702 docs/porting -g '!build*'`
- Observation:
  - `0x22001698` does **not** reduce to only:
    - pulse bit `1` at `0x38400804`
    - clear `0x400` in `0x3c500048`
    - clear `0x1` in `0x3c500048`
    - clear `0x2000` in `0x3c50004c`
  - The function also directly:
    - clears bits `0..2` at `0x38100000`
    - writes `0x38e00014`
    - writes `0x38e01014`
    - later sets bits in `0x38e00010` / `0x38e01010`
    - restores saved values to:
      - `0x3c50004c`
      - `0x3c500048`
      - `0x38100000`
  - The path also depends on multiple service/runtime calls:
    - `0x220024d8`
    - `0x22001c70`
    - `0x22006380`
    - `0x22006430`
    - `0x22006558`
    - `0x22003b5c`
    - `0x22003350`
    - callbacks through the runtime object at `0x22007784`
  - `0x220032fc` decodes IDs into bank/bit pairs.
  - `0x22003838` then sets the matching bit in:
    - `0x38e00010` for bank `0`
    - `0x38e01010` for bank `1`
  - Important cross-check:
    - ID `33` -> bank `1`, bit `1`
    - in-tree `CLOCKGATE_CWHEEL = 33`
  - This means the later cluster contains at least one non-display-specific gate
    operation.
  - `0x38400804` still has no second confirming cross-reference in decrypted
    WTF, so bit `1` remains an unknown peripheral reset/enable pulse rather than
    a proven LCD reset.
- Interpretation:
  - the later `0x22001698` cluster is **not** a bounded display-only preamble
  - splitting out one member would be speculative
  - replaying the whole path would pull in unresolved service/runtime setup and
    at least one gate ID that aligns with clickwheel (`33`)
  - decision for this pass:
    - **STILL_BLOCKED**
- Recovery status:
  - unchanged; reverse-engineering and documentation only
- Next action:
  - do **not** prepare `lcd-clustergate-fullinit-n3g.bin`
  - if this line is pursued further, the next safe analysis step is to prove
    whether the remaining gate IDs in this path are actually display-owned or
    whether `0x22001698` is a broader peripheral wake path that should be
    excluded entirely

#### Step 40: Classify the remaining IDs in `0x22001698` and decide whether to exclude the whole path

- Command:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | rg -n "22003838|220032fc|0x38e00010|0x38e00014|0x38e01010|0x38e01014|0x38100000|22007784|2200778c|22007790|22007e88"`
  - `xxd -g 4 -s 0x7780 -l 0x80 /tmp/n3g-wtf-decrypted.body.bin`
  - `sed -n '1678,1905p' firmware/export/s5l87xx.h`
  - `sed -n '200,225p' firmware/target/arm/s5l8702/system-s5l8702.c`
  - `sed -n '140,210p' firmware/target/arm/s5l8702/system-s5l8702.c`
  - `sed -n '120,180p' firmware/export/s5l87xx.h`
  - `sed -n '560,640p' firmware/export/s5l87xx.h`
  - `sed -n '110,140p' firmware/target/arm/s5l8702/crt0.S`
- Observation:
  - `0x38e00000` is defined in-tree as `VIC_BASE`, not as a display/clock
    register block.
  - In-tree mappings:
    - `0x38e00010` -> `VIC0INTENABLE`
    - `0x38e00014` -> `VIC0INTENCLEAR`
    - `0x38e01010` -> `VIC1INTENABLE`
    - `0x38e01014` -> `VIC1INTENCLEAR`
  - `crt0.S` resets the VIC using the same `0x38e00014` / `0x38e01014`
    addresses.
  - Therefore the IDs set by `0x22003838` are IRQ numbers, not display clock
    gates.
  - Decoded IDs in `0x22001698`:
    - `19` -> bank `0`, bit `19` -> `IRQ_USB_FUNC`
    - `33` -> bank `1`, bit `1` -> `IRQ_EXT6`
    - `39` -> bank `1`, bit `7` -> `IRQ_AES`
    - `40` -> bank `1`, bit `8` -> unknown IRQ, still not LCD
  - Negative check:
    - in-tree `IRQ_LCD = 14`
    - `0x22001698` never enables IRQ `14`
  - `0x38100000` is also in-tree `MIU_BASE`, and the base register is
    `MIUCON`, so `0x38100000 &= ~0x7` is memory-interface state, not
    display-specific state.
  - The runtime/service dependency near `0x22007784` is also non-display:
    - `0x2200778c = 3`
    - entries at `0x22007790...` are tagged `"Uart$"`
    - those entries point at:
      - `0x3cc00000`
      - `0x3cc04000`
      - `0x3cc08000`
    - `0x22006380`, `0x22006430`, and `0x22006558` walk/configure that table
- Interpretation:
  - `0x22001698` is not a display-visibility candidate anymore
  - it now looks like a broader peripheral/service/interrupt path involving:
    - USB function IRQ
    - external IRQ
    - AES IRQ
    - a UART-tagged runtime service table
    - MIU state changes
  - final decision for this pass:
    - **EXCLUDE_0x22001698**
- Recovery status:
  - unchanged; reverse-engineering and documentation only
- Next action:
  - do **not** build or run any payload from this path
  - move the search away from `0x22001698` and toward a different Apple-backed
    display-power/backlight/reset dependency

#### Step 41: Fresh display-dependency search outside all rejected paths

- Command:
  - `rg -n "IRQ_LCD|0x38300000|lcd|backlight|brightness|LEDCTL|sleep|wake" /tmp/n3g-wtf-decrypted.body.bin docs/porting firmware/target/arm/s5l8702 -g '!build*'`
  - `find '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries' -maxdepth 2 -type f | sort`
  - `strings -a '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw.payload_0x800.bin' | rg -n "lcd|LCD|display|backlight|bright|sleep|wake|led|panel|screen|dim|light"`
  - `strings -a '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/aupd.fw.payload_0x800.bin' | rg -n "lcd|LCD|display|backlight|bright|sleep|wake|led|panel|screen|dim|light"`
  - `sed -n '1,220p' '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/firmware_layout.json'`
  - `sed -n '1,220p' '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/notes.md'`
  - `sed -n '1,220p' firmware/target/arm/s5l8702/ipodnano3g/pmu-target.h`
  - `sed -n '1,220p' firmware/target/arm/s5l8702/ipodnano3g/backlight-nano3g.c`
  - `sed -n '1,220p' firmware/target/arm/s5l8702/debug-s5l8702.c`
  - `sed -n '1,140p' firmware/target/arm/s5l8702/ipod6g/backlight-6g.c`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22005420 --stop-address=0x22005540`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22001ff0 --stop-address=0x22002060`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220039fc --stop-address=0x22003b18`
  - `rg -n "IRQ_LCD|LCD_DRV_RST|backlight_hw_on\\(|backlight_hw_brightness\\(|lcd_awake\\(" firmware/target/arm/s5l8702 firmware/export -g '!build*'`
- Observation:
  - The local `OSOS` / `aupd` payload slices are still high-entropy and behave
    like encrypted blobs in the current workspace.
  - No Apple-backed backlight or `LEDCTL` call site was recovered from those
    artifacts.
  - In decrypted WTF, the only PMU helpers/callers still visible are:
    - `0x22005420`: read helper on PMU slave `0x73`
    - `0x22005474`: write helper on PMU slave `0x73`
    - `0x220054b0`: write reg `0x1d = 0x0a`, then reg `0x1b = on/off`
    - `0x220054f8`: read-modify-write reg `0x43` bit `0`
  - No Apple-backed PMU use of:
    - `0x20`
    - `0x28`
    - `0x29`
    was found in the decrypted WTF body.
  - In-tree Nano 3G code still points at PMU backlight ownership through:
    - `D1671_REG_LEDCTL = 0x20`
    - `D1671_LEDCTL_ENABLE = 0x80`
  - In-tree debug code reports:
    - `pmu_read(0x29)` as backlight on/off
    - `pmu_read(0x28)` as brightness value
  - In-tree iPod 6G backlight code uses:
    - `pmu_write(0x28, brightness)`
    - `pmu_write(0x29, on/off)`
  - The only fresh `#14` use in decrypted WTF is:
    - `0x22002024: mov r0, #14`
    - then call `0x220039fc`
  - `0x220039fc` is not a dedicated VIC/LCD helper; it is a generic selector
    that toggles bits in the `0x3c500000` clock/power bank according to an
    index table.
- Interpretation:
  - The strongest remaining missing display dependency is still likely in the
    PMU/backlight domain.
  - But the current workspace still lacks an Apple-backed backlight sequence:
    - `LEDCTL (0x20)` is only anchored by in-tree Nano 3G target code
    - `0x28 / 0x29` are only anchored by in-tree debug/Nano3G/6G code
  - The fresh `#14` helper is too generic to promote as an LCD-only gate/IRQ
    candidate.
  - Decision for this pass:
    - **STILL_BLOCKED**
- Recovery status:
  - unchanged; reverse-engineering and documentation only
- Next action:
  - do **not** prepare `lcd-nextvisible-n3g.bin`
  - next safe search should target a decrypted higher-level firmware phase
    (`OSOS` / display UI path) or another Apple-backed backlight trace outside
    WTF

#### Step 42: Review OSOS decryption support and attempt supported device-assisted decrypt

- Command:
  - `rg -n "decrypt|retailos|PayloadKindRetailOSDecrypted|getRetailOSDecrypted" /tmp/wInd3x -g '!**/vendor/**'`
  - `sed -n '1,220p' /tmp/wInd3x/cmd/wInd3x/cmd_decrypt.go`
  - `sed -n '1,220p' /tmp/wInd3x/cmd/wInd3x/cmd_download.go`
  - `sed -n '297,368p' /tmp/wInd3x/pkg/cache/cache.go`
  - `sed -n '1,220p' /tmp/wInd3x/pkg/exploit/decrypt/decrypt.go`
  - `xxd -g 1 -l 64 '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw'`
  - `xxd -g 1 -l 64 '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/aupd.fw'`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-decrypted.dfu`
  - `kill -TERM 56733`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `lsusb`
- Observation:
  - `wInd3x` supports RetailOS extraction/decryption internally:
    - `download retailos`
    - `PayloadKindRetailOSUpstream`
    - `PayloadKindRetailOSDecrypted`
    - `getRetailOSDecrypted()`
  - `cmd_decrypt.go` is generic over parsed IMG1 files even though its help
    string says “DFU image”.
  - Local `OSOS.fw` is a valid Nano 3G IMG1:
    - magic `8702`
    - version `1.0`
    - format `3`
  - Clean pre-check succeeded before the first attempt:
    - `mks5lboot --dfuscan` found `05ac:1223`
    - DFU state `2`
  - Direct supported decrypt against `OSOS.fw` started successfully and made
    early progress.
  - Because full OSOS size is ~10.8 MiB and the decrypt engine works in tiny
    chunks, the first attempt was stopped so it could be restarted with a
    recovery file.
  - The resumable retry failed immediately with:
    - `clean failed: ClrStatus: control: libusb: i/o error [code -1]`
  - Follow-up host checks showed:
    - `lsusb` still lists `05ac:1223`
    - `mks5lboot --dfuscan` now fails with:
      - `Could not set USB configuration: LIBUSB_ERROR_OTHER`
  - Recovery file state:
    - `/tmp/n3g-osos-work/n3g-osos.recovery`
    - size `0` bytes
- Interpretation:
  - The supported OSOS decryption path is real and works in principle on Nano
    3G.
  - The immediate blocker is not unsupported format or missing tooling.
  - The immediate blocker is a stale/non-clean DFU USB session after the first
    long decrypt attempt.
  - No decrypted OSOS plaintext is available yet, so no higher-phase display or
    backlight candidate can be extracted from OSOS in this pass.
- Recovery status:
  - hardware not modified permanently
  - Nano still enumerates as DFU (`05ac:1223`) but must be reset back into a
    clean host-usable DFU session before decryption can continue
- Next action:
  - re-enter clean DFU
  - rerun the supported resumable decrypt:
    - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`

#### Step 43: Resume supported OSOS decrypt after clean DFU re-entry

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
- Observation:
  - clean DFU baseline restored successfully:
    - device `05ac:1223`
    - DFU state `2`
  - resumable OSOS decrypt started normally and progressed without the earlier
    immediate `ClrStatus` failure
  - after stopping the long-running transfer, the saved recovery file contained:
    - `72960` bytes of plaintext
    - real completion `0.676%` of the `OSOS.fw` body
  - upstream `wInd3x` progress logging for this path is easy to misread:
    - the printed `percent` field is percentage points
    - actual progress must be measured from the recovery file size
- Interpretation:
  - supported OSOS decryption is now confirmed in practice, not just in source
  - the current blocker is runtime length: full OSOS decryption will take many
    repeated exploit/decrypt cycles and is not a short single-turn operation
  - no decrypted OSOS plaintext image is available yet, so higher-phase display
    or backlight triage cannot start yet
- Recovery status:
  - no firmware or NAND modifications were made
  - decryption was interrupted intentionally after checkpointing
  - the saved recovery buffer can be resumed later
- Next action:
  - resume the same command in a later session/turn until a complete
    `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` exists

#### Step 44: Continue resumable OSOS decrypt from the saved checkpoint

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `lsusb`
- Observation:
  - clean DFU baseline restored again before resuming:
    - device `05ac:1223`
    - DFU state `2`
  - the recovery-backed decrypt resumed successfully from:
    - `72960` bytes
  - progress advanced through:
    - `73728`
    - `74496`
    - `75264`
    - ...
    - `125952`
  - after checkpointing, the recovery file contained:
    - `126096` bytes plaintext
    - real completion `1.167%`
  - raw USB enumeration after stopping still shows:
    - `05ac:1223`
- Interpretation:
  - the saved recovery checkpoint is valid and reusable
  - the decrypt remains extremely slow but is progressing correctly
  - no completed OSOS plaintext exists yet, so higher-phase display/backlight
    triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - continue resuming the same command in later turns until the decrypted OSOS
    image is complete

#### Step 45: Continue resumable OSOS decrypt and checkpoint a third recovery state

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
- Observation:
  - clean DFU baseline was restored again before resuming:
    - device `05ac:1223`
    - DFU state `2`
  - the same recovery-backed decrypt resumed successfully from:
    - `126096` bytes
  - after a bounded run and intentional checkpoint, the recovery file contained:
    - `165696` bytes plaintext
    - real completion `1.536%`
  - no OSOS plaintext artifact was complete yet, so no higher-level display or
    backlight triage started
- Interpretation:
  - the recovery file continues to be valid across repeated DFU re-entry
  - the blocking issue remains throughput/runtime, not unsupported format or
    failed recovery state
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - resume the same command again from the saved recovery file

#### Step 46: Continue resumable OSOS decrypt to the current checkpoint

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - the Nano resumed from a non-clean but still usable DFU state:
    - initial `dfuscan` showed DFU state `3`
  - the resumable decrypt still recovered cleanly from that state and advanced
    through:
    - `165888`
    - `166656`
    - ...
    - `284928`
  - repeated `libusb: interrupted [code -10]` messages appeared during the run,
    but the decrypt continued advancing and did not invalidate the recovery file
  - after intentionally stopping the run, the saved recovery file contained:
    - `285648` bytes plaintext
    - real completion `2.647%`
  - post-checkpoint host state returned to the usual stale-DFU pattern:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption is progressing correctly and the recovery file remains
    durable across repeated bounded sessions
  - the practical blocker is still runtime length plus the need to re-enter
    clean DFU between some decrypt sessions
  - no completed OSOS plaintext exists yet, so higher-phase display/backlight
    triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command until a complete
    `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` exists

#### Step 47: Continue resumable OSOS decrypt to the next stable checkpoint

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `285648` bytes
  - progress advanced through:
    - `285696`
    - `286464`
    - ...
    - `349440`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `350064` bytes plaintext
    - real completion `3.244%`
  - post-checkpoint host state again returned to the stale-DFU pattern:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - the recovery-backed OSOS decrypt remains healthy and is advancing
    monotonically across repeated short sessions
  - the only practical blockers remain total runtime and intermittent need for
    clean DFU re-entry before another resume
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from
    `/tmp/n3g-osos-work/n3g-osos.recovery`

#### Step 48: Continue resumable OSOS decrypt to the next checkpoint

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `350064` bytes
  - progress advanced through:
    - `350208`
    - `350976`
    - ...
    - `420096`
  - repeated `libusb: interrupted [code -10]` messages continued to appear
    during the long run but did not stop forward progress or invalidate the
    recovery state
  - after intentional checkpointing, the recovery file contained:
    - `420240` bytes plaintext
    - real completion `3.895%`
  - post-checkpoint host state again returned to the stale-DFU pattern:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and the recovery file is
    holding up across repeated bounded sessions
  - the only practical blockers remain runtime length and the recurring need
    for clean DFU re-entry before some resume attempts
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

## 2026-04-24 OSOS decrypt live checkpoint 42

- The long-running `wInd3x decrypt` session is still active and holding the
  DFU device.
- Latest observed progress from the live session:
  - `7150080` bytes
  - `66.264%`
- Current host state:
  - `mks5lboot --dfuscan` is expected to fail with `LIBUSB_ERROR_BUSY` while
    the active decrypt process owns the DFU session
  - no DFU re-entry is needed at this checkpoint
- Interpretation:
  - device-assisted OSOS decryption remains healthy despite repeated
    non-fatal `libusb: interrupted [code -10]` messages in the running log
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage is still deferred

## 2026-04-23 OSOS decrypt live checkpoint (active session continues)

- Context:
  - active foreground decrypt PTY session is still running
  - no user action required
- Command still running:
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
- Latest observed progress from live session:
  - `ix=3989760`
  - `36.976%`
- Observed behavior:
  - decrypt continues normally
  - intermittent `libusb: interrupted [code -10]` messages appear, but progress
    keeps advancing
  - DFU re-entry is not needed while the session remains active
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session
  - only prompt for DFU re-entry if the session exits and DFU is no longer
    usable

## 2026-04-23 OSOS decrypt live checkpoint (session still active)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=4165632`
  - `38.605%`
- Observed behavior:
  - decrypt continues normally
  - repeated `libusb: interrupted [code -10]` messages still occur, but they do
    not stop forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - continue monitoring without interrupting the decrypt

## 2026-04-23 OSOS decrypt live checkpoint (session still active, later)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=4364544`
  - `40.449%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session

## 2026-04-23 OSOS decrypt live checkpoint (session still active, 41 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=4504320`
  - `41.744%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-23 OSOS decrypt live checkpoint (session still active, 43 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=4675584`
  - `43.332%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-23 OSOS decrypt live checkpoint (session still active, 46 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=5009664`
  - `46.428%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-23 OSOS decrypt live checkpoint (session still active, 48 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=5207040`
  - `48.257%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-23 OSOS decrypt live checkpoint (session still active, near 50 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=5379072`
  - `49.851%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-23 OSOS decrypt live checkpoint (session still active, 51 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=5532672`
  - `51.275%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-23 OSOS decrypt live checkpoint (session still active, 52 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=5675520`
  - `52.599%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-24 OSOS decrypt live checkpoint (session still active, 55 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=6023424`
  - `55.823%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-24 OSOS decrypt live checkpoint (session still active, 57 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=6187776`
  - `57.346%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-24 OSOS decrypt live checkpoint (session still active, 59 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=6398976`
  - `59.303%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-24 OSOS decrypt live checkpoint (session still active, 61 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=6646272`
  - `61.595%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

## 2026-04-24 OSOS decrypt live checkpoint (session still active, 63 percent)

- Context:
  - active foreground decrypt PTY session remains running
  - no DFU re-entry required
- Latest observed progress from live session:
  - `ix=6800640`
  - `63.026%`
- Observed behavior:
  - decrypt continues normally
  - recurring `libusb: interrupted [code -10]` messages still do not stop
    forward progress
- Recovery status:
  - recovery file remains `/tmp/n3g-osos-work/n3g-osos.recovery`
  - no payloads run
  - no hardware-writing step performed
- Next action:
  - keep monitoring the active decrypt session until it exits or completes

#### Step 78: Record the exited background decrypt checkpoint

- Commands run:
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `awk 'BEGIN { printf "%.3f\n", (3461328 / 10790256) * 100 }'`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `ps -o pid,ppid,stat,cmd -p 102203`
- Observation:
  - the long-running background decrypt process eventually exited
  - final checkpoint observed from the recovery file:
    - `3461328` bytes plaintext
    - real completion `32.078%`
  - after the process ended, DFU was no longer cleanly usable:
    - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
  - the decrypt process itself is now gone
- Interpretation:
  - this turn met the user’s condition for surfacing again: the background
    decrypt stopped and the Nano now needs another clean DFU re-entry
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**

#### Step 77: Update the fifth live in-progress background decrypt checkpoint

- Commands run:
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `ps -eo pid,ppid,cmd | rg "wInd3x decrypt|wInd3x"`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (3347856 / 10790256) * 100 }'`
- Observation:
  - the background `wInd3x decrypt` process is still running and the recovery
    file continues to advance
  - live checkpoint observed:
    - `3347856` bytes plaintext
    - real completion `31.027%`
  - while the active decrypt still owns the USB session:
    - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
- Interpretation:
  - this remains the active-decrypt state, not stale DFU
  - no user action is needed while the background decrypt continues

#### Step 76: Update the fourth live in-progress background decrypt checkpoint

- Commands run:
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `ps -eo pid,ppid,cmd | rg "wInd3x decrypt|wInd3x"`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (3243168 / 10790256) * 100 }'`
- Observation:
  - the background `wInd3x decrypt` process is still running and the recovery
    file continues to advance
  - live checkpoint observed:
    - `3243168` bytes plaintext
    - real completion `30.056%`
  - while the active decrypt still owns the USB session:
    - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
- Interpretation:
  - this remains the active-decrypt state, not stale DFU
  - no user action is needed while the background decrypt continues

#### Step 75: Update the third live in-progress background decrypt checkpoint

- Commands run:
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `ps -eo pid,ppid,cmd | rg "wInd3x decrypt|wInd3x"`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (3138624 / 10790256) * 100 }'`
- Observation:
  - the background `wInd3x decrypt` process is still running and the recovery
    file continues to advance
  - live checkpoint observed:
    - `3138624` bytes plaintext
    - real completion `29.088%`
  - while the active decrypt still owns the USB session:
    - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
- Interpretation:
  - this remains the active-decrypt state, not stale DFU
  - no user action is needed while the background decrypt continues

#### Step 74: Update the second live in-progress background decrypt checkpoint

- Commands run:
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `ps -eo pid,ppid,cmd | rg "wInd3x decrypt|wInd3x"`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2992032 / 10790256) * 100 }'`
- Observation:
  - the background `wInd3x decrypt` process is still running and the recovery
    file continues to advance
  - live checkpoint observed:
    - `2992032` bytes plaintext
    - real completion `27.729%`
  - while the active decrypt still owns the USB session:
    - `mks5lboot --dfuscan` continues to return `LIBUSB_ERROR_BUSY`
- Interpretation:
  - this remains the active-decrypt state, not stale DFU
  - no user action is needed while the background decrypt continues

#### Step 73: Update the live in-progress background decrypt checkpoint

- Commands run:
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `ps -eo pid,ppid,cmd | rg "wInd3x decrypt|wInd3x"`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2923536 / 10790256) * 100 }'`
- Observation:
  - the background `wInd3x decrypt` process is still running
  - live checkpoint observed:
    - `2923536` bytes plaintext
    - real completion `27.094%`
  - the recovery file advanced from the earlier `2794128`-byte live note to
    this newer checkpoint without any user intervention
  - while the active decrypt still owns the USB session:
    - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
- Interpretation:
  - this is still the active-decrypt state, not stale DFU
  - no user action is needed while the background decrypt continues

#### Step 71: Auto-resume OSOS decryption to the `24.719%` checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2666640 / 10790256) * 100 }'`
- Observation:
  - the automatic loop resumed from clean DFU state `2`
  - one intermediate checkpoint remained usable in DFU state `9`, so the loop
    continued without user intervention
  - after the newest checkpoint, the recovery file contained:
    - `2666640` bytes plaintext
    - real completion `24.719%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is nearing the 25%
    mark
  - the automatic loop should pause here and wait for another clean DFU
    re-entry before continuing

#### Step 72: Record the live in-progress checkpoint while background decrypt continues

- Commands run:
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `ps -eo pid,ppid,cmd | rg "wInd3x decrypt|wInd3x"`
  - `awk 'BEGIN { printf "%.3f\n", (2794128 / 10790256) * 100 }'`
- Observation:
  - after the PTY session detached, the `wInd3x decrypt` process was still
    running in the background and continued advancing the recovery file
  - live checkpoint observed:
    - `2794128` bytes plaintext
    - real completion `25.895%`
  - while the decrypt process is still holding the USB session:
    - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
- Interpretation:
  - this is not the stale-DFU failure mode; it is an active in-progress decrypt
    holding the device open
  - no user action is needed yet while the background decrypt continues

#### Step 67: Auto-resume OSOS decryption from the `2421456`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2476224 / 10790256) * 100 }'`
- Observation:
  - this auto-resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `2421456` bytes
  - the decrypt advanced monotonically through:
    - `2421504`
    - `2422272`
    - ...
    - `2476032`
  - after intentional checkpointing, the recovery file contained:
    - `2476224` bytes plaintext
    - real completion `22.949%`
  - post-checkpoint host state remained usable:
    - `mks5lboot --dfuscan` reported DFU state `9`

#### Step 68: Auto-resume OSOS decryption from the `2476224`-byte checkpoint

- Commands run:
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2591376 / 10790256) * 100 }'`
- Observation:
  - this second auto-resume started from usable DFU state `9`
  - the decrypt advanced monotonically through:
    - `2476800`
    - `2477568`
    - ...
    - `2591232`
  - after intentional checkpointing, the recovery file contained:
    - `2591376` bytes plaintext
    - real completion `24.019%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`

#### Step 69: Auto-resume OSOS decryption from the `2591376`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2591376 / 10790256) * 100 }'`
- Observation:
  - after the user restored clean DFU, the recovery-backed decrypt resumed from
    `2591376` bytes
  - the recovery file now contains:
    - `2591376` bytes plaintext
    - real completion `24.019%`
  - post-checkpoint host state again returned to the stale-DFU pattern
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    24% mark
  - the automatic loop pauses only when DFU falls back to the stale
    host-visible state

#### Step 41: Resume OSOS decryption from the corrected `1720752`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (1774224 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1720752` bytes
  - the decrypt advanced monotonically through:
    - `1721088`
    - `1721856`
    - ...
    - `1774080`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `1774224` bytes plaintext
    - real completion `16.443%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    16% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from `1774224` bytes

#### Step 42: Resume OSOS decryption from the `1774224`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (1828560 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1774224` bytes
  - the decrypt advanced monotonically through:
    - `1774848`
    - `1775616`
    - ...
    - `1827840`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `1828560` bytes plaintext
    - real completion `16.941%`
  - post-checkpoint host state remained usable:
    - `mks5lboot --dfuscan` reported clean DFU state `2`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is approaching the
    17% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - resume the same recovery-backed decrypt command from `1828560` bytes
  - no DFU reset is required before the very next slice if state `2` persists

#### Step 43: Resume OSOS decryption from the `1828560`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (1884288 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1828560` bytes
  - the decrypt advanced monotonically through:
    - `1828608`
    - `1829376`
    - ...
    - `1883904`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `1884288` bytes plaintext
    - real completion `17.459%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    17% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from `1884288` bytes

#### Step 44: Resume OSOS decryption from the `1884288`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (1935696 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1884288` bytes
  - the decrypt advanced monotonically through:
    - `1884672`
    - `1885440`
    - ...
    - `1935360`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `1935696` bytes plaintext
    - real completion `17.938%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is approaching the
    18% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from `1935696` bytes

#### Step 45: Resume OSOS decryption from the `1935696`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (1994448 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1935696` bytes
  - the decrypt advanced monotonically through:
    - `1936128`
    - `1936896`
    - ...
    - `1993728`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `1994448` bytes plaintext
    - real completion `18.482%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    18% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from `1994448` bytes

#### Step 46: Resume OSOS decryption from the `1994448`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2044896 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1994448` bytes
  - the decrypt advanced monotonically through:
    - `1994496`
    - `1995264`
    - ...
    - `2044416`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `2044896` bytes plaintext
    - real completion `18.952%`
  - post-checkpoint host state remained usable:
    - `mks5lboot --dfuscan` reported clean DFU state `2`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is closing in on
    the 19% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - resume the same recovery-backed decrypt command from `2044896` bytes
  - no DFU reset is required before the very next slice if state `2` persists

#### Step 47: Resume OSOS decryption from the `2044896`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2096064 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `2044896` bytes
  - the decrypt advanced monotonically through:
    - `2045184`
    - `2045952`
    - ...
    - `2095872`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `2096064` bytes plaintext
    - real completion `19.428%`
  - post-checkpoint host state remained usable:
    - `mks5lboot --dfuscan` reported clean DFU state `2`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is approaching the
    20% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - resume the same recovery-backed decrypt command from `2096064` bytes
  - no DFU reset is required before the very next slice if state `2` persists

#### Step 48: Resume OSOS decryption from the `2096064`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2147184 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `2096064` bytes
  - the decrypt advanced monotonically through:
    - `2096640`
    - `2097408`
    - ...
    - `2146560`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `2147184` bytes plaintext
    - real completion `19.894%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is about to cross
    the 20% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from `2147184` bytes

#### Step 49: Resume OSOS decryption from the `2147184`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2197776 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `2147184` bytes
  - the decrypt advanced monotonically through:
    - `2147328`
    - `2148096`
    - ...
    - `2197248`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `2197776` bytes plaintext
    - real completion `20.367%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    20% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from `2197776` bytes

#### Step 50: Resume OSOS decryption from the `2197776`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2254272 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `2197776` bytes
  - the decrypt advanced monotonically through:
    - `2198016`
    - `2198784`
    - ...
    - `2254080`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `2254272` bytes plaintext
    - real completion `20.891%`
  - post-checkpoint host state remained usable:
    - `mks5lboot --dfuscan` reported clean DFU state `2`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    20.8% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - resume the same recovery-backed decrypt command from `2254272` bytes
  - no DFU reset is required before the very next slice if state `2` persists

#### Step 51: Resume OSOS decryption from the `2254272`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2308800 / 10790256) * 100 }'`
- Observation:
  - this bounded resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `2254272` bytes
  - the decrypt advanced monotonically through:
    - `2254848`
    - `2255616`
    - ...
    - `2308608`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the run but did not stop progress
  - after intentional checkpointing, the recovery file contained:
    - `2308800` bytes plaintext
    - real completion `21.402%`
  - post-checkpoint host state remained usable:
    - `mks5lboot --dfuscan` reported clean DFU state `2`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    21% mark
  - the recovery file remains durable across bounded resume/interrupt cycles
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - resume the same recovery-backed decrypt command from `2308800` bytes
  - no DFU reset is required before the very next slice if state `2` persists

#### Step 52: Auto-resume OSOS decryption from the `2308800`-byte checkpoint

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2365104 / 10790256) * 100 }'`
- Observation:
  - this auto-resume started cleanly from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `2308800` bytes
  - the decrypt advanced monotonically through:
    - `2309376`
    - `2310144`
    - ...
    - `2364672`
  - after intentional checkpointing, the recovery file contained:
    - `2365104` bytes plaintext
    - real completion `21.918%`
  - post-checkpoint host state remained usable:
    - `mks5lboot --dfuscan` reported clean DFU state `2`
- Interpretation:
  - the automatic resume loop worked as intended through one full additional
    bounded slice without user intervention

#### Step 53: Auto-resume OSOS decryption from the `2365104`-byte checkpoint

- Commands run:
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `awk 'BEGIN { printf "%.3f\n", (2421456 / 10790256) * 100 }'`
- Observation:
  - the second auto-resume started immediately from clean DFU state `2`
  - the decrypt advanced monotonically through:
    - `2365440`
    - `2366208`
    - ...
    - `2420736`
  - after intentional checkpointing, the recovery file contained:
    - `2421456` bytes plaintext
    - real completion `22.435%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    22% mark
  - the automatic loop should pause here and wait for another clean DFU
    re-entry before continuing
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from `2421456` bytes

#### Step 40: Record the current OSOS recovery checkpoint before the next resume

- Commands run:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `awk 'BEGIN { printf "%.3f\n", (1720752 / 10790256) * 100 }'`
- Observation:
  - the Nano was still host-visible as `05ac:1223`, but `mks5lboot --dfuscan`
    did not reach a clean DFU state and failed immediately with:
    - `Could not set USB configuration: LIBUSB_ERROR_OTHER`
  - the recovery file had already advanced beyond the last logged checkpoint
    and now contains:
    - `1720752` bytes plaintext
    - real completion `15.947%`
- Interpretation:
  - OSOS decryption is still progressing overall, but this turn stopped at the
    clean-DFU precondition rather than starting a new bounded decrypt segment
  - the recovery file remains valid and should be resumed from this newer
    checkpoint, not from the earlier `1643520`-byte note
- Recovery status:
  - no payloads run
  - no permanent device modification
  - no decrypt restart from scratch
- Next action:
  - re-enter clean DFU
  - resume the same recovery-backed decrypt command from `1720752` bytes

#### Step 49: Continue resumable OSOS decrypt to the next checkpoint

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `420240` bytes
  - progress advanced through:
    - `420864`
    - `421632`
    - ...
    - `483072`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `483696` bytes plaintext
    - real completion `4.481%`
  - post-checkpoint host state again returned to the stale-DFU pattern:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and the recovery file
    remains durable across repeated bounded sessions
  - the only practical blockers remain total runtime and the recurring need for
    clean DFU re-entry before another resume
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 50: Continue resumable OSOS decrypt past the 5% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `483696` bytes
  - progress advanced through:
    - `483840`
    - `484608`
    - ...
    - `545280`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `545472` bytes plaintext
    - real completion `5.055%`
  - unlike the previous several checkpoints, post-stop host state remained
    clean:
    - `mks5lboot --dfuscan` still reported DFU state `2`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    5% mark
  - the recovery file remains durable across repeated bounded sessions
  - the decrypt path can occasionally return to a clean DFU baseline even after
    an intentional checkpoint stop
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 51: Continue resumable OSOS decrypt to the next checkpoint

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - because the previous stop had left the Nano in clean DFU, this resume
    started immediately from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `545472` bytes
  - the resumable decrypt advanced through:
    - `546048`
    - `546816`
    - ...
    - `605184`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `605760` bytes plaintext
    - real completion `5.610%`
  - unlike the prior checkpoint, post-stop host state returned to the stale-DFU
    pattern:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and the recovery file
    remains durable across repeated bounded sessions
  - post-stop DFU cleanliness is not deterministic: some checkpoints return to
    state `2`, while others fall back to the familiar host-visible stale state
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 52: Continue resumable OSOS decrypt past the 6% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `605760` bytes
  - progress advanced through:
    - `605952`
    - `606720`
    - ...
    - `668160`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `668688` bytes plaintext
    - real completion `6.196%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    6% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 53: Continue resumable OSOS decrypt to the next checkpoint

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `668688` bytes
  - progress advanced through:
    - `668928`
    - `669696`
    - ...
    - `729600`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `730320` bytes plaintext
    - real completion `6.763%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and the recovery file
    remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 54: Continue resumable OSOS decrypt past the 7% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `730320` bytes
  - progress advanced through:
    - `730368`
    - `731136`
    - ...
    - `790272`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `790512` bytes plaintext
    - real completion `7.325%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    7% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 55: Continue resumable OSOS decrypt to the next checkpoint

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `790512` bytes
  - progress advanced through:
    - `791040`
    - `791808`
    - ...
    - `850176`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `850560` bytes plaintext
    - real completion `7.882%`
  - post-checkpoint host state stayed USB-visible and DFU-usable, but no longer
    returned to clean state `2`:
    - `mks5lboot --dfuscan` reported DFU state `9`
- Interpretation:
  - OSOS decryption continues to advance monotonically and the recovery file
    remains durable across repeated bounded sessions
  - post-stop DFU state can land in multiple host-visible conditions:
    - clean state `2`
    - state `9`
    - or the familiar stale `LIBUSB_ERROR_OTHER` case
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - if needed, re-enter clean DFU before the next resume
  - continue the same recovery-backed decrypt command from the current recovery
    file

#### Step 56: Resume OSOS decrypt directly from DFU state `9`

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - pre-run host state was:
    - device `05ac:1223`
    - DFU state `9`
    - recovery size `850560` bytes
  - the resumable decrypt still recovered cleanly from DFU state `9` and
    advanced through:
    - `850944`
    - `851712`
    - ...
    - `921600`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `921840` bytes plaintext
    - real completion `8.547%`
  - post-checkpoint host state returned to clean DFU:
    - `mks5lboot --dfuscan` reported DFU state `2`
- Interpretation:
  - the recovery-backed OSOS decrypt does not always require a fresh clean DFU
    re-entry; DFU state `9` is usable for resume
  - the post-stop DFU state is also non-deterministic in both directions:
    - it may degrade to `LIBUSB_ERROR_OTHER`
    - remain at state `9`
    - or return to clean state `2`
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - continue the same recovery-backed decrypt command from the current
    recovery file

#### Step 57: Continue resumable OSOS decrypt past the 9% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - because the previous step had returned the Nano to clean DFU, this resume
    started immediately from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `921840` bytes
  - the resumable decrypt advanced through:
    - `922368`
    - `923136`
    - ...
    - `989952`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `990048` bytes plaintext
    - real completion `9.176%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    9% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 58: Continue resumable OSOS decrypt toward the 10% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - because the previous step had returned the Nano to clean DFU, this resume
    started immediately from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `990048` bytes
  - the resumable decrypt advanced through:
    - `990720`
    - `991488`
    - ...
    - `1054464`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `1054848` bytes plaintext
    - real completion `9.775%`
  - post-checkpoint host state returned to clean DFU:
    - `mks5lboot --dfuscan` reported DFU state `2`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is approaching the
    10% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - continue the same recovery-backed decrypt command from the current
    recovery file

#### Step 59: Continue resumable OSOS decrypt past the 10% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - because the previous step had returned the Nano to clean DFU, this resume
    started immediately from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1054848` bytes
  - the resumable decrypt advanced through:
    - `1055232`
    - `1056000`
    - ...
    - `1136640`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `1137024` bytes plaintext
    - real completion `10.538%`
  - post-checkpoint host state returned to clean DFU:
    - `mks5lboot --dfuscan` reported DFU state `2`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    10% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - continue the same recovery-backed decrypt command from the current
    recovery file

#### Step 60: Continue resumable OSOS decrypt past the 11% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - because the previous step had returned the Nano to clean DFU, this resume
    started immediately from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1137024` bytes
  - the resumable decrypt advanced through:
    - `1137408`
    - `1138176`
    - ...
    - `1202688`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `1202784` bytes plaintext
    - real completion `11.148%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    11% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 61: Continue resumable OSOS decrypt toward the 12% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - because the previous step had returned the Nano to clean DFU, this resume
    started immediately from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1202784` bytes
  - the resumable decrypt advanced through:
    - `1203456`
    - `1204224`
    - ...
    - `1273344`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `1273968` bytes plaintext
    - real completion `11.803%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is approaching the
    12% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 62: Continue resumable OSOS decrypt past the 12% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `1273968` bytes
  - progress advanced through:
    - `1274112`
    - `1274880`
    - ...
    - `1345536`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `1346016` bytes plaintext
    - real completion `12.473%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    12% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 63: Continue resumable OSOS decrypt past the 13% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `1346016` bytes
  - progress advanced through:
    - `1346304`
    - `1347072`
    - ...
    - `1413888`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `1414464` bytes plaintext
    - real completion `13.107%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    13% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 64: Continue resumable OSOS decrypt toward the 14% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `1414464` bytes
  - progress advanced through:
    - `1414656`
    - `1415424`
    - ...
    - `1482240`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `1482672` bytes plaintext
    - real completion `13.740%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is approaching the
    14% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file

#### Step 65: Continue resumable OSOS decrypt deeper past the 14% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean DFU baseline was restored again:
    - device `05ac:1223`
    - DFU state `2`
  - the resumable decrypt resumed immediately from:
    - `1482672` bytes
  - progress advanced through:
    - `1483008`
    - `1483776`
    - ...
    - `1574400`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `1574688` bytes plaintext
    - real completion `14.595%`
  - post-checkpoint host state returned to clean DFU:
    - `mks5lboot --dfuscan` reported DFU state `2`
- Interpretation:
  - OSOS decryption continues to advance monotonically and is now approaching
    the 15% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - continue the same recovery-backed decrypt command from the current
    recovery file

#### Step 66: Continue resumable OSOS decrypt past the 15% mark

- Command:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
  - `stat -c '%n %s bytes' /tmp/n3g-osos-work/n3g-osos.recovery`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - because the previous step had returned the Nano to clean DFU, this resume
    started immediately from:
    - device `05ac:1223`
    - DFU state `2`
    - recovery size `1574688` bytes
  - the resumable decrypt advanced through:
    - `1575168`
    - `1575936`
    - ...
    - `1643520`
  - repeated `libusb: interrupted [code -10]` messages still appeared during
    the long run but did not stop progress or invalidate the recovery state
  - after intentional checkpointing, the recovery file contained:
    - `1643520` bytes plaintext
    - real completion `15.235%`
  - post-checkpoint host state returned to the stale-DFU pattern again:
    - `mks5lboot --dfuscan` failed with
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
- Interpretation:
  - OSOS decryption continues to advance monotonically and has now crossed the
    15% mark
  - the recovery file remains durable across repeated bounded sessions
  - no completed decrypted OSOS plaintext exists yet, so higher-phase
    display/backlight triage still cannot start
  - classification remains:
    - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Recovery status:
  - no payloads run
  - no permanent device modification
  - decryption interrupted intentionally after checkpointing
- Next action:
  - re-enter clean DFU when needed
  - resume the same recovery-backed decrypt command from the current recovery
    file
#### Step 90: Complete OSOS decrypt and extract the first OSOS-backed visibility candidate

- Commands:
  - `strings -t x -n 6 /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | rg 'HandleCycleBacklightSetting|HandleBacklightSelected|Backlight|SetBacklight_Always|TCSlideshowLCD'`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | sed -n '/22004c40/,/22004ca0/p'`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | sed -n '/22005620/,/22005820/p'`
  - `make TARGET=lcd-osos-visible-n3g SRC=lcd-osos-visible-n3g.S LDSCRIPT=lcd-osos-visible-n3g.lds`
  - `arm-elf-eabi-readelf -h tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.elf`
  - `arm-elf-eabi-size tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.elf`
- Observation:
  - OSOS decryption completed successfully and yielded:
    - `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
    - `/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin`
  - the decrypted body contains explicit backlight/display strings:
    - `HandleCycleBacklightSetting`
    - `HandleBacklightSelected`
    - `Backlight`
    - `Backlight On`
    - `Backlight Off`
    - `SetBacklight_AlwaysOff`
    - `SetBacklight_AlwaysOn`
    - `TCSlideshowLCD`
  - the clearest body-visible display-on sequence is:
    - `0x220057d4`: `bl 0x2200374c`
    - `0x220057d8`: `bl 0x22004c5c`
    - `0x220057dc`: `bl 0x2200374c`
    - `0x220057e0`: `bl 0x22003774`
  - supporting wrappers reinforce that `0x22003774` is the “on” side of the
    pair:
    - `0x22005620` uses `0x2200376c`
    - `0x22005640` uses `0x22003774`
  - `0x22004c5c` is small and visible in-body:
    - service pointer `+ 0x44`
    - call `0x2200367c`
    - clear byte `[service + 0x05]`
    - service pointer `+ 0x44`
    - tail-call `0x22003684`
  - the exact hardware-facing MMIO remains hidden inside ROM/service imports:
    - `0x080646b4`
    - `0x08064790`
    - `0x080db704`
    - `0x080dbe58`
  - prepared new payload:
    - `tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.bin`
    - `tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.elf`
    - `tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.map`
  - host-side payload validation:
    - entrypoint `0x22000000`
    - ELF text size `0x40f0`
    - payload prepared only, not executed
- Interpretation:
  - OSOS finally provides a higher-level Apple-backed visibility candidate that
    was missing from WTF
  - the best current sequence is not a raw PMU/GPIO recipe but a compact
    Apple service-call chain layered on top of the existing LCD-local init path
  - this is strong enough to prepare a test payload, but not yet to claim exact
    register ownership for the visibility step
- Classification:
  - **OSOS_DECRYPT_COMPLETE_CANDIDATE_FOUND**
- Recovery status:
  - no hardware payload executed in this step
  - no NAND, USB PHY, storage, or audio work introduced
- Next action:
  - keep the prepared `lcd-osos-visible-n3g.bin` as the next visibility test
    candidate
  - do not run it until explicitly requested

#### Step 91: Single controlled run of `lcd-osos-visible-n3g.bin`

- Commands:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x run /home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.bin`
  - `lsusb`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - force reset + re-enter DFU
  - `lsusb`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean pre-send DFU baseline:
    - device `05ac:1223`
    - DFU state `2`
  - upload completed successfully:
    - `Haxed DFU running!`
    - `Given firmware file is not IMG1, packing into one...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Image sent.`
  - during the intended 20-second observation window, no trustworthy screen
    observation was captured from the user
  - post-send host state matched the established takeover pattern:
    - `lsusb` still showed `05ac:1223`
    - `mks5lboot --dfuscan` failed with:
      `Could not set USB configuration: LIBUSB_ERROR_OTHER`
  - recovery was then confirmed:
    - after reset/re-entry, `lsusb` again showed `05ac:1223`
    - `mks5lboot --dfuscan` again reported DFU state `2`
- Interpretation:
  - the OSOS-derived visibility payload executed strongly enough to take over
    DFU, consistent with prior successful payload runs
  - no visual screen result can be claimed from this run because the
    observation was missed
  - this run therefore does not upgrade the visibility milestone
- Classification:
  - **EXECUTION ONLY**
  - note: **no visual observation captured**
- Recovery status:
  - confirmed
  - no persistent failure introduced
- Next action:
  - do not infer visible success from this run
  - any follow-up visibility attempt must be treated as a fresh controlled test

#### Step 92: Reduce the OSOS visibility path to raw hardware operations

- Commands:
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | sed -n '/22003660/,/220037a0/p'`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | sed -n '/22004c40/,/22004d20/p'`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | sed -n '/22005610/,/22005840/p'`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | sed -n '/22007390/,/22007660/p'`
- Observation:
  - `0x2200374c` is only:
    - `ldr pc, [pc, #-4]`
    - target `0x080db704`
  - `0x22003774` is only:
    - `ldr pc, [pc, #-4]`
    - target `0x080dbe58`
  - `0x22004c5c` is the only minimal-candidate helper visible in-body:
    - `add r0, r0, #0x44`
    - `bl 0x2200367c -> 0x080646b4`
    - `strb r0, [r4, #5]` after zeroing `r0`
    - `add r0, r4, #0x44`
    - tail-call `0x22003684 -> 0x08064790`
  - the “on” wrapper `0x22005640` adds further service calls:
    - `0x220073b4`
    - `0x22007610`
  - `0x220073b4` itself still depends on imported ROM/service helpers:
    - `0x2200368c`
    - `0x22003694`
    - `0x2200369c`
  - `0x22007610` is also only an object-method dispatch:
    - load `[r0 + 0x20]`
    - branch through `0x220038dc` into ROM/service code
  - no direct PMU writes, GPIO writes, LCD MMIO writes, or display-specific
    delay loops are exposed by reducing these OSOS calls in the decrypted body
- Interpretation:
  - the OSOS visibility path is real and Apple-backed at the call level, but it
    remains encapsulated in Apple ROM/service helpers not present in the
    decrypted OSOS body
  - the only direct body-visible state change in the minimal candidate is a
    service-object byte clear, not a display hardware write
  - converting this path into a direct-write standalone payload would require
    inventing hardware operations that are not visible in the Apple-backed body
    code
- Classification:
  - **STILL_BLOCKED**
- Recovery status:
  - analysis only
  - no hardware payload run
- Next action:
  - do not prepare `lcd-osos-raw-visible-n3g.bin`
  - preserve the reduced note as the current hard boundary for OSOS-backed
    display visibility work

#### Step 93: Investigate ROM/service targets behind the blocked OSOS display path

- Commands:
  - `rg -n "080db704|080dbe58|080646b4|08064790|0x08000000|BootROM|bootrom|ROM dump|rom dump|dump memory|memdump|read memory|peek|0x080" /tmp/wInd3x . -g '!**/build*/**'`
  - `rg -n "func.*dump|dump .*memory|read .*memory|peek|memdump|upload|download|decrypt|bootrom|rom" /tmp/wInd3x -g '!**/build*/**'`
  - `strings -t x -n 6 /tmp/n3g-wtf-decrypted.body.bin /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | rg -n "BootROM|bootrom|ROM|service|db704|dbe58|646b4|64790|0x080|S5L8702|norboot|wtf"`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | sed -n '/22003660/,/220037a0/p'`
  - `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin | sed -n '/22004c40/,/22004d20/p'`
  - `sed -n '90,140p' /tmp/wInd3x/README.md`
  - `sed -n '1,220p' /tmp/wInd3x/cmd/wInd3x/cmd_dump.go`
  - `sed -n '1,80p' firmware/export/s5l87xx.h`
  - `sed -n '1,80p' docs/porting/ipodnano3g-ghidra-triage.md`
- Observation:
  - the blocked OSOS display-service targets remain:
    - `0x080646b4`
    - `0x08064790`
    - `0x080db704`
    - `0x080dbe58`
  - local S5L8702/Nano 3G references agree that:
    - `DRAM_ORIG = 0x08000000`
    - `IRAM_ORIG = 0x22000000`
  - local `wInd3x` documentation explicitly states bootrom is mapped at:
    - `0x00000000`
    - `0x20000000`
  - this makes the `0x080...` display-service targets best fit **DRAM-backed
    runtime code**, not direct BootROM bodies
  - searches across decrypted WTF/OSOS bodies, extracted firmware artifacts,
    repo notes, and local `wInd3x` sources did **not** recover concrete bodies
    for those four service targets
  - local `wInd3x` does implement a documented read-only dump primitive:
    - `wInd3x dump [offset] [size] [file]`
    - README example dumps bootrom from `0x20000000`
  - that is enough to justify a safe **prepared-only** dump plan for narrow
    `0x080...` service windows, for example:
    - `/tmp/wInd3x/wInd3x dump 0x08064000 0x2000 /tmp/n3g-romsvc-08064000.bin`
    - `/tmp/wInd3x/wInd3x dump 0x080db000 0x2000 /tmp/n3g-romsvc-080db000.bin`
  - no memory dump was executed in this step
- Interpretation:
  - OSOS body reduction is no longer the only blocker; the missing hardware
    sequence now lives behind service code that appears to reside in DRAM
  - the next evidence-backed move is to recover those narrow service windows
    with the documented read-only dump path rather than guess PMU/GPIO/MMIO
    writes
- Classification:
  - **ROM_SERVICE_DUMP_POSSIBLE**
- Recovery status:
  - analysis only
  - no hardware payload built or run
  - no dump executed yet
- Next action:
  - if later approved, use `wInd3x dump` to recover the narrow `0x080...`
    service ranges and then disassemble them to extract the real
    display/backlight hardware sequence

#### Step 94: Attempt first read-only dump of the runtime/service window

- Commands:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x dump 0x08064000 0x2000 /tmp/n3g-romsvc-08064000.bin`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `ls -l /tmp/n3g-romsvc-08064000.bin`
- Observation:
  - pre-dump baseline was clean:
    - device `05ac:1223`
    - DFU state `2`
  - sandboxed `wInd3x dump` could not access the device, so the dump was
    retried with direct USB access
  - the unrestricted dump reached the device and logged:
    - `INFO Dumping... offset=134627328`
  - the first `dumpmem` trigger then failed immediately with:
    - `failed to run wInd3x exploit: bug trigger: USB timeout error`
  - the output file was created but remained empty:
    - `/tmp/n3g-romsvc-08064000.bin`
    - size `0`
  - after the failed trigger, DFU fell into the usual stale host-visible state:
    - `mks5lboot --dfuscan` -> `LIBUSB_ERROR_OTHER`
- Interpretation:
  - the read-only dump primitive is present and callable
  - but, in the tested Nano 3G DFU context, it did not return even the first
    `0x40` bytes from the target `0x08064000` service window
  - local `wInd3x` documentation only explicitly demonstrates bootrom dumping
    from `0x20000000`, so the current blocker may be specific to DRAM-backed
    runtime/service dumping rather than absence of the dump feature itself
- Classification:
  - **ROM_SERVICE_DUMP_FAILED_WITH_REASON**
- Recovery status:
  - no NAND or persistent write path used
  - DFU recovery still requires manual re-entry after the failed trigger
- Next action:
  - do not claim the service bodies are dumped yet
  - if this path is revisited, first determine whether Nano 3G `dumpmem` works
    only against bootrom-space reads or requires a different context before
    retrying the second `0x080db000` window

#### Step 95: Classify Nano 3G `wInd3x dump` with tiny read-only probes

- Commands:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x dump 0x20000000 0x40 /tmp/n3g-dump-test-20000000.bin`
  - `ls -l /tmp/n3g-dump-test-20000000.bin`
  - `xxd -g 1 -l 64 /tmp/n3g-dump-test-20000000.bin`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x dump 0x00000000 0x40 /tmp/n3g-dump-test-00000000.bin`
  - `ls -l /tmp/n3g-dump-test-00000000.bin`
  - `xxd -g 1 -l 64 /tmp/n3g-dump-test-00000000.bin`
  - `cmp -l /tmp/n3g-dump-test-20000000.bin /tmp/n3g-dump-test-00000000.bin`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x dump 0x08064000 0x40 /tmp/n3g-dump-test-08064000.bin`
  - `ls -l /tmp/n3g-dump-test-08064000.bin`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean starting baseline:
    - device `05ac:1223`
    - DFU state `2`
  - `0x20000000` probe succeeded:
    - output size `64`
    - bytes are ARM-like and nontrivial:
      - `2e 00 00 ea 64 f0 9f e5 ...`
  - post-first-probe DFU remained usable in state `9`
  - `0x00000000` probe also succeeded:
    - output size `64`
    - contents matched the `0x20000000` dump exactly
  - post-second-probe DFU again remained usable in state `9`
  - `0x08064000` probe failed immediately:
    - `bug trigger: USB timeout error`
    - output file size `0`
  - after the failed `0x080...` probe, DFU degraded to the usual stale
    host-visible state:
    - `mks5lboot --dfuscan` -> `LIBUSB_ERROR_OTHER`
- Interpretation:
  - Nano 3G `wInd3x dump` is working for bootrom-accessible aliases
    (`0x00000000` and `0x20000000`)
  - the same primitive does **not** currently work for the DRAM-backed
    runtime/service window at `0x08064000`
  - this narrows the blocker from “dump broken” to “runtime DRAM/service dump
    inaccessible in the tested DFU context”
- Classification:
  - **DUMP_080_SMALL_FAILS**
- Recovery status:
  - no NAND or persistent write path used
  - DFU requires manual re-entry after the failed `0x080...` probe
- Next action:
  - do not attempt larger `0x080...` dumps from this context
  - if service recovery is pursued later, the next work should focus on finding
    a context or primitive that can reach DRAM runtime/service space instead of
    retrying larger reads blindly

#### Step 96: Investigate alternate DRAM/runtime-service read primitives

- Commands:
  - `sed -n '1,260p' /tmp/wInd3x/pkg/exploit/exploit.go`
  - `sed -n '1,220p' /tmp/wInd3x/pkg/exploit/dumpmem/dumpmem.go`
  - `sed -n '1,220p' /tmp/wInd3x/pkg/exploit/decrypt/decrypt.go`
  - `sed -n '1,240p' /tmp/wInd3x/pkg/exploit/haxeddfu/haxeddfu.go`
  - `sed -n '1,220p' /tmp/wInd3x/cmd/wInd3x/cmd_spew.go`
  - `sed -n '1,220p' /tmp/wInd3x/cmd/wInd3x/cmd_nor_read.go`
  - `sed -n '1,260p' /tmp/wInd3x/cmd/wInd3x/cmd_nand_read.go`
  - repo searches for:
    - `dumpmem`
    - `HandlerFooter`
    - `0x22000100`
    - `0x080...` target addresses
    - relocation/copy-table hints
- Observation:
  - `dumpmem.Trigger()` is a thin wrapper around the normal DFU exploit path:
    - clean DFU
    - place payload in DFU buffer / IRAM
    - execute via bootrom bug trigger
    - return `0x40` bytes through `HandlerFooter(addr)`
  - on Nano 3G, `HandlerFooter(addr)` explicitly:
    - loads `addr` into `r0`
    - sets `r1 = 0x40`
    - calls bootrom helper `0x2000aa40`
    - returns via bootrom address `0x200048d4`
  - `cmd_spew`, CP14/CP15 reads, NAND reads, and NOR reads all use the same
    exploit/return model rather than a distinct runtime reader
  - `cmd_spew` and storage helpers do demonstrate one structural escape hatch:
    - payload writes data into `0x22000100`
    - `HandlerFooter(0x22000100)` returns that IRAM scratch buffer
  - however, no current evidence proves that the bootrom DFU execution context
    can dereference the target `0x080...` runtime/service window safely
  - additional static searching did not recover a hidden relocation/copy-table
    path that would reconstruct the missing service bodies from already dumped
    WTF/OSOS artifacts
- Interpretation:
  - no alternate local primitive was found that clearly reads the `0x080...`
    region from a later runtime context
  - a payload-assisted copy into `0x22000100` is conceptually plausible, but it
    is not yet evidence-backed enough to promote because the underlying ability
    to read `0x080...` from the present context is still unproven
- Classification:
  - **CURRENTLY_BLOCKED_WITH_REASON**
- Recovery status:
  - static analysis only
  - no new payload built or run
- Next action:
  - if work continues, the next non-speculative path is to find a later-stage
    execution context or documented primitive that can actually see DRAM
    runtime/service space, rather than reusing the current bootrom-oriented
    reader family

#### Step 97: Prepare a minimal standalone `0x080...` read probe without running it

- Commands:
  - add `tools/ipodnano3g/minimal_payload/probe-080-read-n3g.S`
  - add `tools/ipodnano3g/minimal_payload/probe-080-read-n3g.lds`
  - `make TARGET=probe-080-read-n3g LDSCRIPT=probe-080-read-n3g.lds -C tools/ipodnano3g/minimal_payload`
  - `arm-elf-eabi-readelf -h tools/ipodnano3g/minimal_payload/probe-080-read-n3g.elf`
  - `arm-elf-eabi-objdump -d tools/ipodnano3g/minimal_payload/probe-080-read-n3g.elf`
  - `ls -lh tools/ipodnano3g/minimal_payload/probe-080-read-n3g.bin tools/ipodnano3g/minimal_payload/probe-080-read-n3g.elf tools/ipodnano3g/minimal_payload/probe-080-read-n3g.map`
- Observation:
  - prepared new smallest-possible read-only probe payload
  - verified properties:
    - entrypoint `0x22000000`
    - flat binary size `52` bytes
    - direct behavior:
      - `0x22000100 = 0x11111111`
      - `0x22000104 = *(uint32_t *)0x08064000`
      - `0x22000108 = 0x22222222`
      - infinite loop afterward
  - there are no MMIO writes and no peripheral interactions in the payload
  - result-readback review:
    - existing working readback paths depend on live bootrom DFU handling
    - earlier payload runs consistently showed the takeover pattern where
      post-send `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`
    - therefore no existing command can be honestly documented as a working
      way to read `0x22000100` after this probe has taken over
- Interpretation:
  - the probe itself is safe enough and minimal enough to keep as a prepared
    artifact
  - but running it now would not produce an evidence-backed retrievable result
    with the current tool path
- Classification:
  - **PROBE_PREPARED_BUT_NO_RESULT_READBACK**
- Recovery status:
  - host-side preparation only
  - no hardware payload run
- Next action:
  - do not run the probe until a credible post-takeover readback path exists

#### Step 98: Search for a minimal payload result-output channel

- Commands:
  - source review of:
    - `/tmp/wInd3x/pkg/exploit/haxeddfu/haxeddfu.go`
    - `/tmp/wInd3x/pkg/exploit/wind3x_n3g.go`
    - `/tmp/wInd3x/pkg/dfu/dfu.go`
    - `/tmp/wInd3x/cmd/wInd3x/cmd_spew.go`
    - `/tmp/wInd3x/cmd/wInd3x/cmd_nor_read.go`
    - `/tmp/wInd3x/cmd/wInd3x/cmd_nand_read.go`
  - repo searches for:
    - return-to-DFU possibilities
    - USB/status output helpers
    - piezo/beep/reset physical signals
- Observation:
  - all existing host-readable memory/result paths are tied to live bootrom DFU
    request handling:
    - `dump`
    - `spew`
    - CP14/CP15 reads
    - NAND/NOR helpers
  - these all depend on `HandlerFooter(...)` and the bootrom DFU USB handlers
    remaining active
  - standalone image execution does not preserve that condition; the established
    post-send pattern is still:
    - USB may remain enumerated as `05ac:1223`
    - `mks5lboot --dfuscan` falls to `LIBUSB_ERROR_OTHER`
  - haxed-DFU’s USB descriptor change is exploit-time only and does not provide
    a reusable standalone output primitive
  - no already-reversed tiny standalone USB/status routine was found for Nano 3G
  - the only source-backed simple physical output still standing is piezo tone,
    but that path is audio-based and therefore outside the current task
    constraints
- Interpretation:
  - there is no evidence-backed way today for a standalone Nano 3G payload to
    report a small value back to the host after takeover
  - the output problem is now the binding blocker for the prepared `0x080...`
    probe
- Classification:
  - **NO_SAFE_OUTPUT_CHANNEL_FOUND**
- Recovery status:
  - analysis only
  - no payload built or run in this step
- Next action:
  - do not run the `0x080...` probe until either:
    - a host-readable post-takeover path exists, or
    - the task explicitly allows the piezo/audio route as a physical signal

#### Step 99: Prepare a minimal piezo proof-of-execution payload

- Commands:
  - source review of:
    - `firmware/target/arm/s5l8702/ipodnano3g/piezo-nano3g.c`
    - `bootloader/ipod-s5l87xx.c`
    - `firmware/export/s5l87xx.h`
  - source cross-check of sibling targets:
    - `firmware/target/arm/s5l8702/ipod6g/piezo-6g.c`
    - `firmware/target/arm/s5l8702/ipodnano4g/piezo-nano4g.c`
- Observation:
  - the narrowest Nano 3G piezo path is the bootloader-style `piezo_tone()`
    loop, not the wider timer/PWM interrupt path
  - exact Nano 3G register basis:
    - `PCON0 = 0x3cf00000`
    - `GPIOCMD = 0x3cf00200`
    - `USEC_TIMER = 0x3c7000b4`
  - bootloader `alive[]` sequence is:
    - period `500` us
    - duration `100` ms
    - no post-gap
  - prepared payload behavior:
    - write `PCON0 = (PCON0 & ~0xff000000) | 0x53000000`
    - alternate `GPIOCMD` between `0x0000060e` and `0x0000060f`
      every `250` us for about `100000` us total
    - write `PCON0 = (PCON0 & ~0xff000000) | 0xee000000`
    - loop forever
  - this avoids:
    - timer-A PWM setup
    - PMU
    - LCD
    - backlight
    - NAND/storage
    - USB PHY
    - full codec/audio path
  - Apple firmware corroboration was searched again in decrypted WTF/OSOS but no
    direct Apple piezo helper was isolated confidently enough to claim a fully
    Apple-reduced raw sequence
- Artifacts:
  - `tools/ipodnano3g/minimal_payload/piezo-beep-n3g.S`
  - `tools/ipodnano3g/minimal_payload/piezo-beep-n3g.lds`
  - `tools/ipodnano3g/minimal_payload/piezo-beep-n3g.bin`
  - `tools/ipodnano3g/minimal_payload/piezo-beep-n3g.elf`
  - `tools/ipodnano3g/minimal_payload/piezo-beep-n3g.map`
- Validation:
  - build command:
    - `make -C tools/ipodnano3g/minimal_payload TARGET=piezo-beep-n3g LDSCRIPT=piezo-beep-n3g.lds`
  - entrypoint:
    - `0x22000000`
  - artifact sizes:
    - `piezo-beep-n3g.bin`: `152` bytes
    - `piezo-beep-n3g.elf`: about `65 KiB`
    - `piezo-beep-n3g.map`: about `1.5 KiB`
  - disassembly confirms only:
    - `PCON0` reads/writes at `0x3cf00000`
    - `GPIOCMD` writes at `0x3cf00200`
    - `USEC_TIMER` reads at `0x3c7000b4`
- Interpretation:
  - a safe-enough minimal physical proof payload can now be prepared from the
    Nano 3G source-backed piezo path
  - confidence is lower than the Apple LCD work because the Apple piezo call
    path is still not reduced directly from WTF/OSOS
- Classification:
  - **PIEZO_PROOF_PREPARED**
- Recovery status:
  - host-side preparation only
  - payload not run
- Next action:
  - if explicitly approved later, a single controlled run of the fixed
    `piezo-beep-n3g.bin` can test audible proof-of-execution

#### Step 100: Reduce OSOS early system init and prepare a bounded probe

- Commands:
  - OSOS startup disassembly around:
    - `0x22008808`
    - `0x220039c4`
    - `0x220044c4`
    - `0x22002770`
    - `0x22002420`
    - `0x22002d78`
  - source cross-check against:
    - `firmware/export/s5l87xx.h`
    - existing Nano 3G porting notes
- Observation:
  - OSOS reset entry is:
    - `0x22000000 -> 0x22008808 -> 0x220039c4`
  - the full reset path remains too service-heavy to reduce honestly as a small
    standalone write list
  - the cleanest bounded common-init wrapper below reset is:
    - `0x22002770`
  - `0x22002770` performs:
    - imported front-edge service call via `0x22003414`
    - CP15 control bit clear through `0x22003150` and `0x22003138`
    - MIU/global memory programming through `0x22002420`
    - clock/reset-style programming through `0x22002d78`
    - CP15 control bit restore through `0x220030fc` and `0x22003110`
  - visible direct state touched by this wrapper includes:
    - `0x38100000` (`MIU_BASE`)
    - `0x3c500000` (`CLK_BASE`)
    - `0x39900000` (`DMA1_BASE`)
  - unresolved imports remain:
    - `0x22003414 -> 0x08016234`
    - `0x220034bc -> 0x0801542c`
- Artifacts:
  - `docs/porting/ipodnano3g-early-init.md`
  - `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.S`
  - `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.lds`
  - `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.bin`
  - `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.elf`
  - `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.map`
- Interpretation:
  - the likely blocker for both LCD and piezo is now earlier global init, not
    another subsystem-local write
  - a fully raw write-only reduction is still blocked by unresolved OSOS
    service imports
  - a bounded Apple call-level early-init probe is nevertheless preparable
    without widening into LCD/audio/NAND/USB logic
- Recovery status:
  - host-side analysis and build only
  - no payload run
  - build validated:
    - entrypoint `0x22000000`
    - binary size about `14 KiB`
    - `_start` only sets stack, calls `0x22002770`, then loops

#### Step 101: Run bounded early-init probe once and classify host-side result

- Commands:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x run tools/ipodnano3g/minimal_payload/system-init-probe-n3g.bin`
  - `lsusb`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean pre-run DFU was confirmed:
    - `05ac:1223`
    - DFU state `2`
  - upload succeeded end-to-end
  - post-send USB still enumerated as:
    - `05ac:1223`
  - post-send DFU probe failed with:
    - `LIBUSB_ERROR_OTHER`
  - user later clarified there was no visible or audible behavior observed
- Interpretation:
  - the run is execution-confirmed at the usual DFU-takeover level
  - the bounded early-init wrapper did not produce an observable user-facing
    effect in this test
- Classification:
  - **SAFE_EXECUTION_ONLY**

#### Step 102: Split `0x22002770` by ROM dependency and prepare observable probe

- Commands:
  - OSOS disassembly cross-check around:
    - `0x22002770`
    - `0x22002420`
    - `0x22002d78`
    - `0x220033f0`
    - selected call sites of `0x220034bc`
  - build:
    - `make -C tools/ipodnano3g/minimal_payload TARGET=system-init-observable-n3g LDSCRIPT=system-init-observable-n3g.lds`
- Observation:
  - `0x22003414` is called only by:
    - `0x22000510`
    - `0x22002770`
  - it takes no visible arguments and its return value is ignored
  - so it behaves like a front-edge init barrier/hook and is required before
    any meaningful local early-init work in `0x22002770`
  - `0x220034bc` consistently behaves like a pure arithmetic/timing helper:
    - arguments in `r0` / `r1`
    - result in `r0`
    - used by the first `0x22002420(1)` MIU phase
    - not needed by the later `0x22002420(3)` phase
  - prepared a new observable variant:
    - `tools/ipodnano3g/minimal_payload/system-init-observable-n3g.bin`
    - same bounded early-init wrapper
    - two software-only delay phases
    - final branch to `0xdead0000` for delayed observable reset/crash
- Validation:
  - entrypoint:
    - `0x22000000`
  - no new MMIO beyond the original early-init wrapper
  - no payload run
  - observable payload validated:
    - binary size about `14 KiB`
    - `_start` calls `0x22002770`, delays twice in software, then branches to
      `0xdead0000`
- Interpretation:
  - the early-init path is now split cleanly enough to say the ROM imports are
    still required for meaningful init progress
- Decision:
  - **ROM_IMPORT_REQUIRED_FOR_INIT**

#### Step 103: Redesign observability around watchdog reboot instead of fault

- Commands:
  - source review:
    - `firmware/target/arm/s5l8702/system-s5l8702.c`
    - `firmware/export/s5l87xx.h`
    - prior timed-reset session notes
  - build:
    - `make -C tools/ipodnano3g/minimal_payload TARGET=system-init-timingprobe-n3g LDSCRIPT=system-init-timingprobe-n3g.lds`
- Observation:
  - invalid-branch fault probes are no longer considered useful for Nano 3G
    observability because they have not produced a visible reset/logo path
  - a narrower real reboot path already exists in-tree:
    - `system_reboot()` writes `0x00100000` to `WDT_BASE`
    - `WDT_BASE = 0x3c800000`
  - prepared:
    - `tools/ipodnano3g/minimal_payload/system-init-timingprobe-n3g.bin`
  - payload behavior:
    - call `0x22002770`
    - short software delay
    - long software delay
    - watchdog write to `0x3c800000`
    - infinite wait for reset
- Validation:
  - entrypoint:
    - `0x22000000`
  - binary size:
    - about `14 KiB`
  - new hardware write added beyond the existing early-init wrapper:
    - `str 0x00100000 -> [0x3c800000]`
- Interpretation:
  - this is the safest remaining observable channel that does not rely on LCD,
    audio, or host readback
  - true repeating timing patterns are not possible without persistence across
    reboot, so the current design is a single known-delay watchdog reset marker
- Recovery status:
  - host-side preparation only
  - payload not run

#### Step 104: Run watchdog-based timing probe once

- Commands:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x run tools/ipodnano3g/minimal_payload/system-init-timingprobe-n3g.bin`
  - `lsusb`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean pre-run DFU was confirmed:
    - `05ac:1223`
    - DFU state `2`
  - upload succeeded end-to-end
  - post-send USB still enumerated as:
    - `05ac:1223`
  - post-send DFU probe failed with:
    - `LIBUSB_ERROR_OTHER`
  - user-visible result over a full 30-second watch window:
    - black / no change
    - no delayed Apple logo
    - no immediate reboot
    - no flicker
    - no click
    - no beep
    - no visible USB disconnect/reconnect
- Interpretation:
  - the watchdog-timed reboot path did not yield a visible reboot/logo signal
  - either the watchdog write is not taking effect in this payload context, or
    the resulting reset path is still not externally observable in the tested
    window
- Classification:
  - **WATCHDOG_NO_VISIBLE_RESET**
- Recovery status:
  - confirmed after manual reset + DFU re-entry:
    - `05ac:1223`
    - DFU state `2`

#### Step 105: RetailOS chainload hook-target discovery

- Commands:
  - local `wInd3x` source inspection:
    - `cmd/wInd3x/cmd_cfw.go`
    - `pkg/cfw/cfw.go`
    - `pkg/cfw/defang_wtf.go`
    - `pkg/cache/cache.go`
  - raw OSOS inspection:
    - `strings -td /tmp/n3g-osos-work/n3g-osos-decrypted.body.bin`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 ...`
  - prepared helper:
    - `/tmp/wInd3x/cmd/patch_n3g_connected_ui.go`
  - built patched RetailOS proof:
    - `/tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu`
- Observation:
  - Nano 3G `cfw run` remains the correct later-stage execution context
  - Nano 3G RetailOS does **not** currently look like an EFI/PE32 firmware
    volume from body offset `0`
  - Nano 3G WTF defanging already uses raw offset patching in local `wInd3x`
  - the safest first initialized-runtime proof target found is the USB/DiskMode
    connected-screen resource cluster
  - prepared raw body substitutions:
    - `0x817980`: `Connected` -> `CFW mode!`
    - `0x8179b8`: `Do not disconnect.` -> `CFW runtime ready!`
    - `0x8179f0`: `OK to Disconnect` -> `CFW booted      `
- Interpretation:
  - for Nano 3G, the first `cfw run` proof should use a raw RetailOS resource
    patch, not a PE32/DXE hook
  - higher-risk raw code-hook candidates still exist in the `TCRemoteUI`
    cluster (`0x22005620`, `0x22005640`, `0x22005660`) but are not the first
    thing to test
- Recovery status:
  - host-side preparation only
  - no `cfw run` executed

#### Step 106: First Nano 3G RetailOS chainload test

- Commands:
  - `./utils/mks5lboot/mks5lboot --dfuscan`
  - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu`
  - `lsusb`
  - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - clean pre-run DFU was confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `cfw run` completed the expected early chainload stages:
    - haxed DFU exploit
    - defanged WTF upload
    - switch into WTF mode
    - firmware send
  - user-visible result over the full 60-second window:
    - black screen / no visible change
    - no patched text
    - no original Apple connected text
    - no normal Apple boot
    - no reset loop
  - host-side post-observation state:
    - USB remained `05ac:1242` (WTF mode)
    - `mks5lboot --dfuscan` returned `LIBUSB_ERROR_BUSY`
    - the `wInd3x cfw run` process had to be terminated manually
- Interpretation:
  - the device did not transition from defanged WTF into a visible RetailOS UI
  - the patched RetailOS artifact therefore did not reach the intended
    connected-screen resource path
  - this is a chainload/context failure after WTF handoff, not a visible UI
    proof success
- Classification:
  - **CHAINLOAD_BLACKSCREEN**
- Recovery status:
  - confirmed after manual reset + DFU re-entry:
    - `05ac:1223`
    - DFU state `2`

#### Step 107: Diagnose WTF -> RetailOS handoff failure

- Commands:
  - source inspection:
    - `/tmp/wInd3x/cmd/wInd3x/cmd_cfw.go`
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
    - `/tmp/wInd3x/pkg/image/image.go`
    - `/tmp/wInd3x/README.md`
    - `/tmp/wInd3x/pkg/exploit/wind3x_n3g.go`
  - disassembly:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220018c0 --stop-address=0x22001a40`
  - header comparison:
    - stock `OSOS.fw`
    - `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
    - `/tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu`
- Observation:
  - Nano 3G `cfw run` does not modify the provided RetailOS image beyond
    sending it after defanged WTF
  - stock OSOS wrapper:
    - format `3`
    - entry `0x0`
    - body length `0xa4a570`
  - decrypted / patched OSOS wrapper:
    - format `2`
    - entry `0x0`
    - body length `0xa4a570`
  - Nano 3G defanged WTF only applies one functional patch:
    - body offset `0x1990`
    - comment: skip signature check
  - an additional intact handoff block remains at runtime
    `0x22001998..0x220019dc`:
    - callback from `[service + 0x74]`
    - followed by checks on:
      - `0x38c00040`
      - `0x38c0000c`
  - later-device defangers in local `wInd3x` patch broader validation/decrypt
    paths, while Nano 3G does not
- Interpretation:
  - the strongest current blocker is not RetailOS UI patching
  - the handoff is most likely blocked inside remaining Nano 3G WTF
    validator/decrypt logic after the first signature-bypass patch
- Classification:
  - **WTF_HANDOFF_BLOCKED_BY_CHECK**
- Recovery status:
  - analysis only
  - no additional device run performed

#### Step 108: Prepare Nano 3G WTF second-stage handoff patch

- Commands:
  - source edit:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild:
    - `GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
- Observation:
  - existing Nano 3G defang patch:
    - body offset `0x1990`
    - original bytes:
      - `00 70 a0 03 22 00 00 0a`
    - patched bytes:
      - `00 70 a0 e3 22 00 00 ea`
  - new second-stage patch added:
    - body offset `0x19b8`
    - original bytes:
      - `08 00 00 0a`
    - replacement bytes:
      - `19 00 00 ea`
  - effect:
    - replace the remaining failure branch after callback result handling with a
      branch to the existing success path at `0x22001a24`
  - rebuild of `/tmp/wInd3x/wInd3x` succeeded offline against the local module
    cache
  - stale cache note:
    - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
    - this file predates the second-stage patch and must be invalidated before
      the next `cfw run`
- Interpretation:
  - minimal Nano 3G-specific WTF extension is now prepared
  - no RetailOS changes were required for this step
- Classification:
  - **WTF_SECOND_PATCH_PREPARED**
- Recovery status:
  - host-side source/build only
  - no new device run performed

#### Step 109: Trace full Nano 3G WTF runtime handoff and cross-check freemyipod docs

- Commands:
  - disassembly:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22001640 --stop-address=0x22001b40`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22002138 --stop-address=0x220021a8`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220024b8 --stop-address=0x22002530`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220062b8 --stop-address=0x22006620`
  - source/doc review:
    - `/tmp/wInd3x/README.md`
    - `/tmp/wInd3x/cmd/wInd3x/cmd_cfw.go`
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
    - `/tmp/wInd3x/pkg/image/image.go`
    - freemyipod wiki pages:
      - `https://freemyipod.org/wiki/Boot_Process`
      - `https://freemyipod.org/wiki/Modes`
      - `https://freemyipod.org/wiki/IMG1`
- Observation:
  - `0x22001a24` is not the final launch. It only sets `r4 = 0`.
  - After `0x22001a24`, WTF still:
    - calls `[service + 0x8c]` twice for IDs `19` and `33`
    - runs `0x22006558`
    - only calls the execute wrapper at `0x220024b8` if:
      - `r4 == 0`
      - `r5 & 0x10` is set
  - `0x22006558` contains a polling loop waiting for
    `[selected UART base + 0x18] & 0x200` to clear.
  - Earlier callback gates also remain:
    - `[service + 0x6c]` must return nonzero or the path exits with `r4 = 23`
    - `[service + 0x74]` is called with:
      - `r0 = 0x08000000`
      - `r1 = 0x08000800`
      - `r2 = 2`
  - freemyipod docs say WTF is a real second-stage bootloader that still
    performs IMG1 verification/decryption before booting the next stage.
  - upstream `wInd3x` README still lists Nano 3G `CFW` as `soon`, while Nano
    5G and Nano 7G are the supported `cfw run` devices.
- Interpretation:
  - the remaining blocker is not a single top-level branch
  - branch-skipping alone is insufficient because WTF still depends on
    validator/decrypt callback side effects and can still stall in the later
    service/UART phase
- Classification:
  - **CALLBACK_DEPENDENCY_REQUIRED**
- Recovery status:
  - analysis only
  - no additional device run performed

#### Step 110: Map Nano 3G WTF service callback table origin

- Commands:
  - disassembly:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220033e0 --stop-address=0x220037a0`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220053a8 --stop-address=0x22005520`
  - data inspection:
    - `xxd -g4 -s 0x7760 -l 0x80 /tmp/n3g-wtf-decrypted.body.bin`
    - `xxd -g4 /tmp/n3g-dump-test-20000000.bin`
  - source review:
    - `/tmp/wInd3x/pkg/exploit/wind3x_n3g.go`
    - `/tmp/wInd3x/README.md`
- Observation:
  - the handoff path does not source its callbacks from the local WTF runtime
    registry at `0x220073ec`
  - instead:
    - `0x22007784` contains `0x20000020`
    - the handoff code loads its service object from that slot
  - therefore the critical callback slots resolve to BootROM words:
    - `[service + 0x6c]` -> `0x2000008c`
    - `[service + 0x74]` -> `0x20000094`
    - `[service + 0x8c]` -> `0x200000ac`
    - related:
      - `[service + 0x80]` -> `0x200000a0`
      - `[service + 0x90]` -> `0x200000b0`
  - the local WTF registry at `0x220073ec` is managed by:
    - `0x220053a8`
    - `0x220053e0`
    - `0x22003508`
    and is used for local services like `Uart$` at `0x22007790+`
  - `wInd3x` README confirms Nano 3G haxed DFU works by overriding the
    BootROM `OnImage` callback inside the BootROM state structure
- Interpretation:
  - the handoff-critical callbacks are BootROM-side, not WTF-local
  - the table base and slot layout are now known
  - but the actual function-pointer contents at `0x2000008c`,
    `0x20000094`, and `0x200000ac` were not covered by the earlier tiny
    BootROM dump
- Classification:
  - **CALLBACK_TABLE_BOOTROM_KNOWN**
- Recovery status:
  - analysis only
  - no additional device run performed

#### Step 111: Recover BootROM callback bodies and resolve handoff blocker

- Commands:
  - pre-check:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - dump BootROM service table:
    - `/tmp/wInd3x/wInd3x dump 0x20000080 0x40 /tmp/n3g-bootrom-service-table-20000080.bin`
  - decode table:
    - `xxd -g4 /tmp/n3g-bootrom-service-table-20000080.bin`
  - dump callback windows:
    - `/tmp/wInd3x/wInd3x dump 0x20003600 0x200 /tmp/n3g-romcb-20003600.bin`
    - `/tmp/wInd3x/wInd3x dump 0x20000600 0x200 /tmp/n3g-romcb-20000600.bin`
    - `/tmp/wInd3x/wInd3x dump 0x20001000 0x200 /tmp/n3g-romcb-20001000.bin`
    - `/tmp/wInd3x/wInd3x dump 0x20001200 0x200 /tmp/n3g-romcb-20001200.bin`
    - `/tmp/wInd3x/wInd3x dump 0x20000800 0x400 /tmp/n3g-romcb-20000800.bin`
  - disassembly:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20003600 /tmp/n3g-romcb-20003600.bin`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20000600 /tmp/n3g-romcb-20000600.bin`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20000800 /tmp/n3g-romcb-20000800.bin`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20001000 /tmp/n3g-romcb-20001000.bin`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20001200 /tmp/n3g-romcb-20001200.bin`
  - local image-format check:
    - `/tmp/wInd3x/pkg/image/image.go`
    - Python header decode of:
      - stock `OSOS.fw`
      - `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
      - `/tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu`
- Observation:
  - recovered service-table pointers:
    - `0x2000008c` -> `0x200036c8`
    - `0x20000094` -> `0x200006dc`
    - `0x200000ac` -> `0x2000106c`
    - `0x200000b0` -> `0x2000132c`
  - `0x2000106c` / `0x2000132c` are a setter/getter pair for per-ID BootROM
    tables
  - `0x200036c8` is a readiness/state callback using haxed-DFU scratch globals
    at `0x2203fff8` / `0x2203fffc`
  - `0x200006dc` is the real image loader/validator callback
  - in the exact handoff call mode used by WTF:
    - `r0 = 0x08000000`
    - `r1 = 0x08000800`
    - `r2 = 2`
    it takes the mode-2 path and explicitly accepts only image format:
    - `3`
    - `4`
    any other format returns failure
  - local Nano 3G repack logic in `pkg/image/image.go` currently forces
    `FormatSigned` (`2`) for Nano 3G
  - header decode confirms:
    - stock `OSOS.fw`: format `3`
    - local decrypted/patched Nano 3G RetailOS: format `2`
- Interpretation:
  - the immediate handoff blocker is not another outer WTF branch
  - it is an image-format mismatch at the BootROM callback layer
  - the BootROM loader callback rejects the current format-2 Nano 3G RetailOS
    wrapper before handoff can complete
- Classification:
  - **CALLBACK_BODIES_RECOVERED_PATCH_PLAN_READY**
- Recovery status:
  - dump-only workflow
  - no `cfw run` performed

#### Step 112: Prepare Nano 3G format-4 RetailOS chainload artifact

- Commands:
  - source edit:
    - `/tmp/wInd3x/pkg/image/image.go`
  - rebuild:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/patch_n3g_connected_ui ./cmd/patch_n3g_connected_ui.go`
  - regenerate patched RetailOS:
    - `/tmp/patch_n3g_connected_ui /tmp/n3g-osos-work/n3g-osos-decrypted.dfu /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - header verification:
    - Python decode of stock `OSOS.fw`, old decrypted Nano 3G OSOS, and new
      format-4 artifact
- Observation:
  - Nano 3G `MakeUnsigned(...)` previously forced:
    - format `2`
    - no signature area
    - no certificate area
  - that special case was removed
  - Nano 3G now emits:
    - magic `8702`
    - version `1.0`
    - format `4`
    - signature area `0x80`
    - certificate area `0x300`
  - regenerated artifact:
    - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - verified header:
    - format `4`
    - entry `0x0`
    - body `0xa4a570`
    - data `0xa4a8f0`
    - cert offset `0xa4a5f0`
    - cert length `0x300`
  - stock Nano 3G `OSOS.fw` remains:
    - format `3`
    - entry `0x0`
    - body `0xa4a570`
  - prior local decrypted/patched Nano 3G images were:
    - format `2`
- Interpretation:
  - prepared the first Nano 3G chainload artifact whose wrapper type matches
    the recovered BootROM loader callback's accepted set (`3` or `4`)
  - no additional WTF patching was needed for this step
- Classification:
  - **FORMAT4_CFW_IMAGE_PREPARED**
- Recovery status:
  - host-side build/regen only
  - no `cfw run` performed

#### Step 113: Test Nano 3G format-4 RetailOS chainload artifact

- Commands:
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
  - pre-run DFU verification:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - chainload test:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - post-run host checks:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - recovery verification:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
    - `lsusb`
- Observation:
  - pre-run DFU was clean:
    - `05ac:1223`
    - DFU state `2`
  - during/after `cfw run`, the device immediately switched into WTF mode:
    - `05ac:1242`
  - host-side post-run scan reported:
    - `LIBUSB_ERROR_BUSY`
  - user-visible result:
    - immediate WTF-mode behavior
    - no RetailOS boot
    - no Apple logo
    - no patched connected text
    - no original connected text
  - recovery succeeded after reset:
    - `05ac:1223`
    - DFU state `2`
- Interpretation:
  - changing the Nano 3G RetailOS wrapper from format `2` to format `4`
    removed the earlier proven image-format blocker, but it did not complete the
    WTF -> RetailOS handoff
  - the device still stalls in WTF runtime before RetailOS executes
- Classification:
  - **CHAINLOAD_WTF_STUCK**
- Recovery status:
  - confirmed

#### Step 114: Trace Nano 3G WTF path after format-4 chainload failure

- Commands:
  - verify patched cached WTF and format-4 image headers:
    - Python decode of:
      - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
      - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect `wInd3x` chainload path:
    - `sed -n '1,320p' /tmp/wInd3x/cmd/wInd3x/cmd_cfw.go`
    - `sed -n '340,430p' /tmp/wInd3x/pkg/cache/cache.go`
  - disassemble recovered BootROM callback bodies:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20000600 /tmp/n3g-romcb-20000600.bin`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20000800 /tmp/n3g-romcb-20000800.bin`
- Observation:
  - cached defanged WTF still contains the intended Nano 3G patches:
    - `0x1990`:
      - `00 70 a0 e3 22 00 00 ea`
    - `0x19b8`:
      - `19 00 00 ea`
  - the actual chainload image used by `cfw run` is really:
    - magic `8702`
    - version `1.0`
    - format `4`
    - entry `0`
    - body `0xa4a570`
    - cert offset `0xa4a5f0`
    - cert length `0x300`
  - `cmd_cfw.go` reads the given firmware file as IMG1 and sends it directly if
    parsing succeeds, so the format-4 image was not rewrapped again
  - recovered BootROM callback `0x200006dc` takes the type-`4` path at
    `0x20000830` and, after header/layout checks, reaches:
    - `0x200008e4: bl 0x200055f0`
    - `0x200008e8: cmp r0, #1`
    - `0x200008ec: bne 0x2000095c`
  - local `MakeUnsigned(...)` still fills the type-`4` footer with placeholder
    data:
    - signature area:
      - `0x80` bytes of `'S'`
    - certificate area:
      - `0x300` bytes of `'C'`
- Interpretation:
  - the format-`4` wrapper fix removed the earlier proven type-`2` rejection
  - the next concrete blocker is later inside the BootROM loader callback,
    where the type-`4` verification call still fails
  - therefore the device remains in WTF mode before later execute-gate logic
    becomes relevant
- Classification:
  - **FORMAT4_STILL_REJECTED**
- Recovery status:
  - analysis only
  - no new `cfw run` performed

#### Step 115: Prepare Nano 3G WTF-local loader stub patch

- Commands:
  - assemble local stub prototype:
    - `arm-none-eabi-as -o /tmp/n3g_loader_stub.o /tmp/n3g_loader_stub.S`
    - `arm-none-eabi-ld -Ttext=0x22006ccc -o /tmp/n3g_loader_stub.elf /tmp/n3g_loader_stub.o`
    - `arm-none-eabi-objcopy -O binary /tmp/n3g_loader_stub.elf /tmp/n3g_loader_stub.bin`
    - `arm-none-eabi-objdump -d /tmp/n3g_loader_stub.elf`
  - verify patch bytes and free space:
    - Python check of:
      - branch patch at `0x19ac`
      - stub size
      - zero run at `0x6cca`
  - source edit:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
- Observation:
  - assembled stub size:
    - `300` bytes (`0x12c`)
  - confirmed free zeroed run in WTF body:
    - start `0x6cca`
    - length `0x30a`
  - chosen stub placement:
    - body offset `0x6ccc`
    - runtime `0x22006ccc`
  - exact new callsite patch:
    - body offset `0x19ac`
    - original:
      - `33 ff 2f e1`
    - replacement:
      - `c6 14 00 eb`
  - Nano 3G defang source now includes:
    - existing branch patches:
      - `0x1990`
      - `0x19b8`
    - new callsite patch:
      - `0x19ac`
    - new local loader stub:
      - `0x6ccc`
  - stub behavior:
    - preserve the type-`4` pre-verification BootROM loader work
    - skip only the later `0x200055f0` cert/signature check
    - replay success-side version/entrypoint metadata writes
    - return success
- Interpretation:
  - no full service-table redirect is needed yet
  - the minimal justified next experiment is a single WTF-local replacement for
    the `[service + 0x74]` loader callback path
- Classification:
  - **LOADER_CALLBACK_STUB_PLAN_READY**
- Recovery status:
  - host-side build/patch only
  - no `cfw run` performed

#### Step 116: Verify why the first local loader stub still failed

- Commands:
  - extract and inspect the cached defanged WTF body:
    - `dd if=/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin of=/tmp/n3g-wtf-defanged-body.bin bs=1 skip=2048`
    - `xxd -g 1 -s 0x19a0 -l 48 /tmp/n3g-wtf-defanged-body.bin`
    - `xxd -g 1 -s 0x6ccc -l 96 /tmp/n3g-wtf-defanged-body.bin`
  - disassemble patched WTF callsite and stub:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-defanged-body.bin | sed -n '/22001998/,/220019dc/p'`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-defanged-body.bin --start-address=0x22006ccc --stop-address=0x22006e00`
  - compare with recovered BootROM callback paths:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20000600 /tmp/n3g-romcb-20000600.bin | sed -n '/200006dc/,/20000780/p'`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20000800 /tmp/n3g-romcb-20000800.bin | sed -n '/20000830/,/20000970/p'`
- Observation:
  - cached first-stub artifact really contained:
    - body `0x19ac`:
      - `c6 14 00 eb`
    - body `0x19b8`:
      - `19 00 00 ea`
    - stub bytes at `0x6ccc`
  - BL math for `0x19ac` is exact:
    - source:
      - `0x220019ac`
    - ARM PC:
      - `0x220019b4`
    - destination:
      - `0x22006ccc`
    - delta:
      - `0x5318`
    - imm24:
      - `0x14c6`
    - instruction:
      - `0xeb0014c6`
  - the first local stub did not preserve enough BootROM behavior:
    - did not preserve `r9`
    - did not call:
      - `0x20001ef0`
      - `0x20001fe0`
    - did not keep the original `0x19b8` status branch
    - replayed only part of the BootROM success path
- Interpretation:
  - the branch patch itself is not the problem
  - the first local stub is best classified as missing required BootROM-side
    effects
- Classification:
  - **MISSING_SIDE_EFFECT**
- Recovery status:
  - analysis only
  - no new `cfw run` performed

#### Step 117: Rebuild Nano 3G local loader stub with fuller BootROM replay

- Commands:
  - update and rebuild local stub:
    - `arm-none-eabi-as -o /tmp/n3g_loader_stub.o /tmp/n3g_loader_stub.S`
    - `arm-none-eabi-ld -Ttext=0x22006ccc -o /tmp/n3g_loader_stub.elf /tmp/n3g_loader_stub.o`
    - `arm-none-eabi-objcopy -O binary /tmp/n3g_loader_stub.elf /tmp/n3g_loader_stub.bin`
  - update local `wInd3x` Nano 3G defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - generate a fresh offline check artifact:
    - `go run ./tmp_defang_check.go`
  - inspect fresh body:
    - `xxd -g 1 -s 0x19a0 -l 48 /tmp/n3g-wtf-defanged-check.body.bin`
    - `xxd -g 1 -s 0x6ccc -l 96 /tmp/n3g-wtf-defanged-check.body.bin`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-defanged-check.body.bin | sed -n '/22001998/,/220019dc/p;/22006ccc/,/22006e90/p'`
- Observation:
  - revised assembled stub size:
    - `452` bytes
  - callsite patch remains:
    - body `0x19ac`:
      - `c6 14 00 eb`
  - body `0x19b8` is now restored to the original conditional branch:
    - `08 00 00 0a`
  - fresh stub at `0x6ccc` begins with the revised prologue:
    - `f0 43 2d e9 2c d0 4d e2 ...`
  - revised stub now:
    - saves `{r4-r9, lr}`
    - uses a BootROM-like `0x2c` stack frame
    - runs:
      - `0x20001ef0`
      - `0x20001fe0`
    - tail-calls original `0x200006dc` for `r2 != 2`
    - replays the type-`4` success-side path more faithfully for `r2 == 2`
    - skips only the later verification call to `0x200055f0`
- Interpretation:
  - the local Nano 3G defanger now matches the current best hypothesis for the
    missing handoff prerequisites
  - this is the first revised build that preserves the original `0x19b8`
    status handling instead of forcing a forward branch
- Classification:
  - **LOADER_CALLBACK_STUB_REVISED_OFFLINE**
- Recovery status:
  - host-side build and offline verification only
  - no new `cfw run` performed

#### Step 118: Hardware test revised Nano 3G local loader stub

- Commands:
  - confirm DFU:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - force cached defanged WTF to the revised offline-verified artifact:
    - `cp -f /tmp/n3g-wtf-defanged-check.bin /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
  - run revised chainload test:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - post-run USB checks:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - pre-run DFU was clean:
    - `05ac:1223`
    - state `2`
  - `wInd3x` reported:
    - cached Nano 3G `wtf-defanged` in use
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - post-handoff USB enumeration became:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - post-run `mks5lboot --dfuscan` reported:
    - `LIBUSB_ERROR_BUSY`
  - user-visible result:
    - no Apple logo
    - no RetailOS boot
    - no connected-screen proof text
- Interpretation:
  - the revised local loader stub still did not complete the WTF -> RetailOS
    handoff
  - restoring the original `0x19b8` branch and replaying more BootROM helper
    work was not sufficient
  - the exact remaining blocker is still unresolved
- Classification:
  - **REVISED_STUB_TESTED_STILL_WTF**
- Recovery status:
  - device left DFU and is currently in WTF mode
  - manual reset / DFU re-entry is still required for the next iteration

#### Step 119: Reconstruct BootROM callback-owned state and redesign stub

- Commands:
  - disassemble recovered BootROM callback windows:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20000600 /tmp/n3g-romcb-20000600.bin | sed -n '/200006dc/,/200007c0/p'`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20000800 /tmp/n3g-romcb-20000800.bin | sed -n '/20000830/,/200009b0/p'`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20003600 /tmp/n3g-romcb-20003600.bin | sed -n '/200036c8/,/20003800/p'`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20001000 /tmp/n3g-romcb-20001000.bin | sed -n '/2000106c/,/20001180/p'`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20001200 /tmp/n3g-romcb-20001200.bin | sed -n '/2000132c/,/20001400/p'`
  - disassemble WTF post-callback path:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin | sed -n '/22001998/,/22001b0c/p'`
  - inspect current local stub:
    - `/tmp/n3g_loader_stub.S`
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
- Observation:
  - explicit success-side writes in `0x200006dc` are only:
    - `[state + 0x34] = img_hdr[4]`
    - `[state + 0x35] = img_hdr[5]`
    - `[state + 0x36] = img_hdr[6]`
    - `[state + 0x30] = *(img_hdr + 0x08)`
  - immediate post-callback WTF consumers are not those bytes; they are:
    - `0x38c00040`
    - `0x38c0000c`
    - `[service + 0x8c]`
    - `[service + 0x44]`
    - `0x22006558`
    - `0x220024b8`
  - `[service + 0x6c]` also clarified related BootROM state:
    - `[state + 0x2c]`
    - `[state + 0x04]`
    - `[state + 0x738 + 0x36]`
    - `[state + 0x738 + 0x37]`
    - scratch globals at:
      - `0x2203fff8`
      - `0x2203fffc`
  - the old local stub's decisive gap is that it replayed helper calls up to
    `0x200020f8` but skipped:
    - `0x200055f0`
- Interpretation:
  - the remaining missing state is more likely helper/MMIO side effects
    preserved by the original BootROM validator path than one more manual state
    write
  - the minimal better patch is to let original `0x200006dc` run first, then
    only replace the final failure-to-success decision for the expected type-3/4
    path by replaying the explicit metadata writes
- Classification:
  - **CALLBACK_STATE_REQUIRED_PATCH_READY**
- Recovery status:
  - host-side analysis and local patch design only

#### Step 120: Implement narrower local Nano 3G callback-state patch

- Commands:
  - update local stub:
    - `/tmp/n3g_loader_stub.S`
  - rebuild stub:
    - `arm-none-eabi-as -o /tmp/n3g_loader_stub.o /tmp/n3g_loader_stub.S`
    - `arm-none-eabi-ld -Ttext=0x22006ccc -o /tmp/n3g_loader_stub.elf /tmp/n3g_loader_stub.o`
    - `arm-none-eabi-objcopy -O binary /tmp/n3g_loader_stub.elf /tmp/n3g_loader_stub.bin`
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline check artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify new bytes:
    - `xxd -g 1 -s 0x6ccc -l 96 /tmp/n3g-wtf-defanged-check.body.bin`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-defanged-check.body.bin | sed -n '/22006ccc/,/22006e04/p'`
- Observation:
  - new local stub size:
    - `308` bytes
  - new stub now:
    - calls original `0x200006dc` first for mode `2`
    - returns directly if BootROM returns `1`
    - only if BootROM returns `0`, and the image still passes the recovered
      type/layout checks, replays:
      - `[state + 0x34..0x36]`
      - `[state + 0x30]`
      - type-`3` tail via `0x20001f04` / `0x20001d48`
      - then returns `1`
  - callsite patch remains:
    - body `0x19ac`
      - `c6 14 00 eb`
  - conditional branch remains:
    - body `0x19b8`
      - `08 00 00 0a`
- Interpretation:
  - this is the first local stub revision that preserves the original
    `0x200055f0` execution path instead of replacing it outright
  - it matches the current best theory for the missing callback side effects
- Classification:
  - **CALLBACK_STATE_REQUIRED_PATCH_READY**
- Recovery status:
  - host-side build and offline verification only
  - no hardware run performed for this new revision

#### Step 121: Test narrowed local callback-state patch on Nano 3G hardware

- Preconditions:
  - device manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
- Commands:
  - refresh cached defanged WTF from the verified offline artifact:
    - `cp -f /tmp/n3g-wtf-defanged-check.bin /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
  - run the local tool:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log reached:
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - device then re-enumerated as:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - `mks5lboot --dfuscan` afterward reported:
    - `LIBUSB_ERROR_BUSY`
  - no visible RetailOS progress occurred:
    - no Apple logo
    - no connected-screen proof text
- Interpretation:
  - preserving original `0x200006dc` execution and `0x200055f0` side effects
    still did not complete the handoff
  - the remaining blocker is after format-4 acceptance and after the explicit
    success-side metadata writes already reconstructed
  - the exact failure source remains unresolved
- Classification:
  - **STILL_BLOCKED**
- Recovery status:
  - device left DFU and is currently in WTF mode
  - another manual DFU re-entry is required before the next iteration

#### Step 122: Trace the post-loader WTF handoff without new hardware runs

- Commands:
  - map the post-loader function and caller:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22001500 --stop-address=0x22001b40`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22002ef0 --stop-address=0x22003010`
  - map execute wrapper and service/UART helpers:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220024b8 --stop-address=0x22002540`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22006274 --stop-address=0x22006640`
  - map BootROM setter/getter:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20001000 /tmp/n3g-romcb-20001000.bin --start-address=0x2000106c --stop-address=0x20001180`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20001200 /tmp/n3g-romcb-20001200.bin --start-address=0x2000132c --stop-address=0x20001400`
  - inspect local `Uart$` table:
    - `xxd -g 1 -s 0x7790 -l 108 /tmp/n3g-wtf-decrypted.body.bin`
- Observation:
  - caller `0x22002f20` seeds flags:
    - `r4 = 0x1d`
  - `0x22002fe4` enters `0x22001698` with:
    - `r0 = 0x1d`
    - `r1 = 0x08000000`
    - `r2 = 0x00f80000`
  - therefore the active path already has:
    - `r5 & 0x8`
    - `r5 & 0x10`
  - `[service + 0x8c]` resolves to:
    - `0x2000106c`
    - setter-only BootROM helper
  - `[service + 0x44]` is used as:
    - pointer-to-word slot
    - save old word
    - write `0x22007e88`
    - restore old word
  - after `0x22001a24`, `r4` is set to `0` and never written again before:
    - `0x22001b04`
  - final execute test is:
    - `cmp r4, #0`
    - `tst r5, #0x10`
    - `blne 0x220024b8`
  - `0x220024b8` executes:
    - `0x08000800`
  - `0x22006558` return value is ignored
  - the only non-returning path left before execute is:
    - `0x220065d0..0x220065fc`
    - wait for `[selected_uart_base + 0x18] & 0x200` to clear
  - resolved local UART bases:
    - ID `0` -> `0x3cc00000`
    - ID `1` -> `0x3cc04000`
    - ID `2` -> `0x3cc08000`
- Interpretation:
  - once the post-loader path reaches `0x22001a24`, execution is already armed
  - `[service + 0x8c]` and `[service + 0x44]` are not the next direct execute
    blockers
  - the next exact pre-execute blocker in the post-loader region is the
    non-returning UART/service wait inside `0x22006558`
- Classification:
  - **UART_SERVICE_STALL**
- Recovery status:
  - analysis only
  - no new `cfw run` performed

#### Step 123: Reduce the UART service stall and prepare a Nano 3G-only patch

- Commands:
  - trace the UART helpers:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22006430 --stop-address=0x22006648`
  - inspect nearby literal data:
    - `arm-none-eabi-objdump -s -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220019e8 --stop-address=0x22001a20`
  - cross-check UART semantics from local sources:
    - `sed -n '1,220p' firmware/target/arm/imx233/uartdbg-imx233.c`
    - `sed -n '1,220p' firmware/target/arm/imx233/regs/stmp3700/uartdbg.h`
    - `sed -n '1,220p' '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/freemyipod_src/apps/uarttest/main.c'`
  - patch local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild and verify:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
    - parsed-body verification at:
      - `0x1990`
      - `0x19ac`
      - `0x19b8`
      - `0x6558`
- Observation:
  - `0x22006558` takes:
    - `r0 = service record`
    - `r1 = byte buffer`
    - `r2 = pointer to byte count`
  - after validation, its raw transmit path:
    - waits on `[uart_base + 0x18] & 0x200`
    - writes bytes to `[uart_base + 0x20]`
  - active path selects service ID `0`, therefore UART base:
    - `0x3cc00000`
  - local freemyipod `uarttest` confirms:
    - `UFSTAT = 0x3cc00018`
    - `while (UFSTAT & BIT(9))` before transmit
    - `UTXH = 0x3cc00020`
  - this identifies the stalled bit as:
    - TX FIFO full
  - earlier setup in `0x22006430` is preserved
  - caller still ignores the return value from `0x22006558`
  - local patch prepared at:
    - runtime `0x22006558`
    - body offset `0x6558`
  - original bytes:
    - `f8 40 2d e9 02 70 a0 e1`
  - replacement bytes:
    - `00 00 a0 e3 1e ff 2f e1`
  - verified in the parsed Nano 3G defanged image body after rebuild
- Interpretation:
  - the UART stall is a debug / serial transmit drain, not a BootROM execute
    prerequisite
  - returning success immediately from `0x22006558` is the narrowest justified
    Nano 3G-only reduction
- Classification:
  - **UART_STALL_PATCH_PREPARED**
- Recovery status:
  - local patch implemented and rebuilt
  - no new `cfw run` performed

#### Step 124: Test the UART-bypass Nano 3G defanged WTF on hardware

- Preconditions:
  - device manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
- Commands:
  - refresh cached defanged WTF:
    - `cp -f /tmp/n3g-wtf-defanged-check.bin /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
  - run the local tool:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log reached:
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - device then re-enumerated as:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - `mks5lboot --dfuscan` afterward reported:
    - `LIBUSB_ERROR_BUSY`
  - patched `0x22006558` did not produce a host-visible change in the end state
- Interpretation:
  - bypassing the UART helper was not sufficient to complete the handoff
  - the remaining blocker is not explained by the `0x22006558` TX FIFO poll
    alone
  - the exact failure source remains unresolved
- Classification:
  - **STILL_BLOCKED_WITH_REASON**
- Recovery status:
  - device left DFU and is currently in WTF mode
  - another manual DFU re-entry is required before the next iteration

#### Step 125: Verify the execute gate and prepare a proof-only pre-execute marker

- Commands:
  - verify exact patch bytes in offline and cached artifacts:
    - parsed-body dump for:
      - `0x1990`
      - `0x19ac`
      - `0x19b8`
      - `0x1af4`
      - `0x1b04`
      - `0x24b8`
      - `0x6558`
  - inspect execute call site:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x22001ae8 --stop-address=0x22001b20`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-decrypted.body.bin --start-address=0x220024b8 --stop-address=0x22002500`
  - inspect payload wrapper and body:
    - parse `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
    - parse `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
    - disassemble body with runtime VMA:
      - `0x08000800`
  - prepare proof stub:
    - `/tmp/n3g_exec_proof_stub.S`
    - assembled at runtime `0x22006d40`
  - verify branch encoding:
    - `0x22001b14 -> 0x22006d40`
- Observation:
  - cached and offline Nano 3G WTF artifacts both contain:
    - `0x6558: 00 00 a0 e3 1e ff 2f e1`
  - so the UART-bypass patch was definitely applied to the tested image
  - execute wrapper `0x220024b8`:
    - takes target directly from `r0`
    - calls `0x22002138(0)`
    - `blx r4`
    - calls `0x22002138(2)` after return
  - execute call site still passes:
    - `r0 = 0x08000800`
  - OSOS payload IMG1 header still has:
    - `Entrypoint = 0`
  - Nano 3G load model still implies:
    - header at `0x08000000`
    - body at `0x08000800`
  - runtime disassembly at `0x08000800` shows valid code at body start
  - proof stub prepared in free zero-filled space:
    - runtime `0x22006d40`
    - body offset `0x6d40`
    - bytes:
      - `03 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
  - proof call-site patch prepared:
    - runtime `0x22001b14`
    - body offset `0x1b14`
    - original:
      - `67 02 00 1b`
    - replacement:
      - `89 14 00 1b`
- Interpretation:
  - entrypoint/load address is no longer the strongest failure explanation
  - the unresolved ambiguity is now runtime only:
    - execute gate not reached
    - or execute reached and OSOS body immediately returns/crashes
  - a proof-only patch is ready to distinguish these on the next hardware run
- Classification:
  - **STILL_BLOCKED_WITH_MAP**
- Recovery status:
  - proof patch prepared only
  - not integrated into `wInd3x`
  - no new `cfw run` performed

#### Step 126: Integrate the pre-execute marker, rebuild, and clear cache

- Commands:
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify parsed body bytes at:
    - `0x1990`
    - `0x19ac`
    - `0x19b8`
    - `0x1b14`
    - `0x6558`
    - `0x6d40`
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- Observation:
  - local defanger now includes:
    - `0x1b14 -> 89 14 00 1b`
    - `0x6d40 -> 03 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
  - parsed Nano 3G defanged image body verifies:
    - `0x1b14` patched
    - `0x6d40` stub present
  - cached Nano 3G defanged WTF was removed successfully
  - RetailOS image under test remains:
    - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
    - unchanged
- Interpretation:
  - the next hardware run will answer whether control reaches the execute-gate
    branch at `0x22001b14`
  - no other variable changed in the RetailOS payload
- Classification:
  - **PRE_EXECUTE_MARKER_PREPARED**
- Recovery status:
  - local tool rebuilt
  - cache cleared
  - run command prepared but not executed:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

#### Step 127: Run the pre-execute marker proof test on Nano 3G hardware

- Preconditions:
  - device manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - defanged WTF cache was absent
- Commands:
  - run the local tool:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - `wInd3x` reported:
    - `No cached data, performing slow action...`
    - so the marker-patched defanged WTF was rebuilt fresh
  - host log reached:
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - device then re-enumerated as:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - `mks5lboot --dfuscan` afterward reported:
    - `LIBUSB_ERROR_BUSY`
  - there was no host-visible behavioral change attributable to the marker
- Interpretation:
  - the strongest current conclusion is that the patched execute-gate branch at
    `0x22001b14` was not reached
  - the failure remains earlier than the execute wrapper
- Classification:
  - **EXECUTE_GATE_NOT_REACHED**
- Recovery status:
  - device left DFU and is currently in WTF mode
  - another manual DFU re-entry is required before the next iteration

#### Step 128: Prepare upstream checkpoint marker test at `0x22001a24`

- Goal:
  - move the next proof split earlier than the failed `0x22001b14` marker
  - determine whether the post-callback success landing at `0x22001a24` is
    reached at all
- Commands:
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify parsed image body at:
    - `0x1a24`
    - `0x1b14`
    - `0x6558`
    - `0x6d40`
    - `0x6d60`
    - `0x6d80`
  - clear cached defanged WTF again:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- Observation:
  - local defanger now restores the old execute-gate site:
    - `0x1b14 -> 67 02 00 1b`
  - local defanger now selects the upstream branch site:
    - `0x1a24 -> cd 14 00 ea`
    - branch target:
      - `0x22006d60`
  - the shared staged marker block is present at:
    - `0x22006d40`
  - staged markers now available:
    - marker A:
      - `0x22006d40`
      - `0x22002138(2)` then loop
    - marker B:
      - `0x22006d60`
      - `0x22002138(3)` then loop
    - marker C:
      - `0x22006d80`
      - `0x22002138(4)` then loop
  - `0x22006558` UART bypass remains present
  - cached Nano 3G defanged WTF was removed successfully
- Interpretation:
  - this is the cleanest next split because if control reaches `0x22001a24`,
    the currently prepared path to `0x22001b14` should be nearly linear
  - if the next hardware run still does not show the marker effect, the blocker
    is earlier, most likely around:
    - `0x220019bc..0x220019dc`
- Classification:
  - **MARKER_PATCH_PREPARED**
- Recovery status:
  - no hardware run performed yet
  - next prepared command remains:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

#### Step 129: Run the single upstream marker test at `0x22001a24`

- Preconditions:
  - Nano manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `lsusb` also showed:
    - Nano 3G in DFU mode
- Commands:
  - run the prepared local tool once:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log showed:
    - `No cached data, performing slow action...`
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - after that the host remained in repeated:
    - `handle_events: error: libusb: interrupted [code -10]`
  - post-run `lsusb` showed:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - post-run `mks5lboot --dfuscan` reported:
    - `LIBUSB_ERROR_BUSY`
- Interpretation:
  - there was no host-visible change attributable to the prepared marker at:
    - `0x22001a24`
  - strongest current conclusion:
    - the active failing path does not reach the post-callback success landing
      at `0x22001a24`
- Classification:
  - **MARKER_1A24_NOT_REACHED**
- Recovery status:
  - device initially remained in WTF mode after the single run
  - a later follow-up scan confirmed clean DFU recovery:
    - `05ac:1223`
    - DFU state `2`

#### Step 130: Prepare the next earlier checkpoint marker in `0x22001998..0x220019dc`

- Goal:
  - move the marker earlier than `0x22001a24`
  - isolate whether the failing path reaches the post-loader compare at
    `0x220019b0`
- Commands:
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify disassembly around:
    - `0x22001998..0x22001a24`
  - verify staged stub bytes at:
    - `0x22006d40`
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- Observation:
  - selected earlier checkpoints:
    - `0x220019b0`
    - `0x220019cc`
    - `0x220019dc`
  - prepared branch replacements:
    - active:
      - `0x220019b0 -> e2 14 00 ea`
      - branch to `0x22006d40`
    - inactive but recorded:
      - `0x220019cc -> e3 14 00 ea`
      - `0x220019dc -> e7 14 00 ea`
  - `0x22001a24` restored to original:
    - `00 40 a0 e3`
  - `0x22001b14` left at original:
    - `67 02 00 1b`
  - staged marker block remains at:
    - `0x22006d40`
  - cached Nano 3G defanged WTF removed successfully
- Interpretation:
  - the next single hardware test should use only the earliest checkpoint at
    `0x220019b0`
  - if that marker also does not trigger, the divergence is even earlier than
    the post-loader compare
- Classification:
  - **MARKER_EARLY_PREPARED**
- Recovery status:
  - no hardware run performed yet with this earlier marker

#### Step 131: Run the single earlier marker test at `0x220019b0`

- Preconditions:
  - Nano manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `lsusb` also showed:
    - Nano 3G in DFU mode
- Commands:
  - run the prepared local tool once:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - after manual reboot and DFU re-entry, confirm recovery:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log showed:
    - `No cached data, performing slow action...`
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - after that the host remained in repeated:
    - `handle_events: error: libusb: interrupted [code -10]`
  - post-run `lsusb` showed:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - post-run `mks5lboot --dfuscan` reported:
    - `LIBUSB_ERROR_BUSY`
  - after manual reboot and DFU re-entry, `mks5lboot --dfuscan` again showed:
    - `05ac:1223`
    - DFU state `2`
- Interpretation:
  - there was no host-visible change attributable to the prepared marker at:
    - `0x220019b0`
  - strongest current conclusion:
    - the active failing path does not reach the compare site at
      `0x220019b0`
- Classification:
  - **MARKER_19B0_NOT_REACHED**
- Recovery status:
  - manual DFU recovery succeeded
  - device is back in clean DFU

#### Step 132: Prepare the loader-callback precall marker at `0x220019ac`

- Goal:
  - move the next proof split to the callback callsite itself
  - determine whether execution reaches the `bl 0x22006ccc` site at all
- Commands:
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify body/disassembly at:
    - `0x220019ac`
    - `0x220019b0`
    - `0x22006ccc`
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- Observation:
  - active next test target:
    - `0x220019ac`
  - original callsite bytes:
    - `c6 14 00 eb`
  - active replacement bytes:
    - `e3 14 00 ea`
    - branch to:
      - `0x22006d40`
  - marker A at `0x22006d40` remains:
    - `0x22002138(2)` then loop forever
  - later checkpoints restored:
    - `0x220019b0 -> 00 00 50 e3`
    - `0x22001a24 -> 00 40 a0 e3`
    - `0x22001b14 -> 67 02 00 1b`
  - prepared but inactive follow-up:
    - `0x22006ccc`
    - original:
      - `f0 43 2d e9`
    - prepared alternate replacement:
      - `23 00 00 ea`
      - branch to `0x22006d60`
  - cache cleared successfully
- Interpretation:
  - next single hardware run should answer whether the path even reaches the
    loader-callback callsite
  - if not, the divergence is earlier than `0x220019ac`
- Classification:
  - **MARKER_PRECALL_PREPARED**
- Recovery status:
  - no hardware run performed yet with this precall marker

#### Step 133: Run the single pre-call marker test at `0x220019ac`

- Preconditions:
  - Nano manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `lsusb` also showed:
    - Nano 3G in DFU mode
- Commands:
  - run the prepared local tool once:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - after manual reboot and DFU re-entry, confirm recovery:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log showed:
    - `No cached data, performing slow action...`
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - after that the host remained in repeated:
    - `handle_events: error: libusb: interrupted [code -10]`
  - post-run `lsusb` showed:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - post-run `mks5lboot --dfuscan` reported:
    - `LIBUSB_ERROR_BUSY`
  - after manual reboot and DFU re-entry, `mks5lboot --dfuscan` again showed:
    - `05ac:1223`
    - DFU state `2`
- Interpretation:
  - there was no host-visible change attributable to the prepared marker at:
    - `0x220019ac`
  - strongest current conclusion:
    - the active failing path does not reach the loader-callback callsite
- Classification:
  - **MARKER_PRECALL_NOT_REACHED**
- Recovery status:
  - manual DFU recovery succeeded
  - device is back in clean DFU

#### Step 134: Prepare the pre-loader setup marker at `0x22001998`

- Goal:
  - move the next proof split into the setup block before the callback callsite
  - determine whether execution reaches the first instruction at `0x22001998`
- Commands:
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify body/disassembly at:
    - `0x22001998`
    - `0x220019ac`
    - `0x220019b0`
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- Observation:
  - active next test target:
    - `0x22001998`
  - original bytes:
    - `00 00 99 e5`
  - active replacement bytes:
    - `e8 14 00 ea`
    - branch to:
      - `0x22006d40`
  - marker A at `0x22006d40` remains:
    - `0x22002138(2)` then loop
  - prepared but inactive follow-ups:
    - `0x220019a0 -> ee 14 00 ea`
    - `0x220019a8 -> f4 14 00 ea`
  - later checkpoints restored:
    - `0x220019ac -> c6 14 00 eb`
    - `0x220019b0 -> 00 00 50 e3`
    - `0x22001a24 -> 00 40 a0 e3`
    - `0x22001b14 -> 67 02 00 1b`
  - cache cleared successfully
- Interpretation:
  - the next single hardware run should answer whether the path reaches the
    start of the setup block at all
  - if not, the divergence is earlier than `0x22001998`
- Classification:
  - **MARKER_1998_PREPARED**
- Recovery status:
  - no hardware run performed yet with this marker

#### Step 135: Run the single pre-loader marker test at `0x22001998`

- Preconditions:
  - Nano manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `lsusb` also showed:
    - Nano 3G in DFU mode
- Commands:
  - run the prepared local tool once:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - after manual reboot and DFU re-entry, confirm recovery:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log showed:
    - `No cached data, performing slow action...`
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - after that the host remained in repeated:
    - `handle_events: error: libusb: interrupted [code -10]`
  - post-run `lsusb` showed:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - post-run `mks5lboot --dfuscan` reported:
    - `LIBUSB_ERROR_BUSY`
  - after manual reboot and DFU re-entry, `mks5lboot --dfuscan` again showed:
    - `05ac:1223`
    - DFU state `2`
- Interpretation:
  - there was no host-visible change attributable to the prepared marker at:
    - `0x22001998`
  - strongest current conclusion:
    - the active failing path does not reach the pre-loader setup block
- Classification:
  - **MARKER_1998_NOT_REACHED**
- Recovery status:
  - manual DFU recovery succeeded
  - device is back in clean DFU

#### Step 136: Prepare the predecessor-path marker at `0x22001994`

- Goal:
  - move the next proof split to the real immediate predecessor of
    `0x22001998`
  - correct the earlier issue where the old `0x1990/0x1994` patch made
    `0x22001998` unreachable in the defanged build
- Commands:
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify body/disassembly at:
    - `0x22001990`
    - `0x22001994`
    - `0x22001998`
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- Observation:
  - active next test target:
    - `0x22001994`
  - original bytes:
    - `22 00 00 0a`
  - active replacement bytes:
    - `e9 14 00 ea`
    - branch to:
      - `0x22006d40`
  - marker A at `0x22006d40` remains:
    - `0x22002138(2)` then loop
  - prepared but inactive follow-ups:
    - `0x2200198c -> f3 14 00 ea`
    - `0x22001988 -> fc 14 00 ea`
  - restored predecessor path bytes:
    - `0x22001990 -> 00 70 a0 03`
    - `0x22001998 -> 00 00 99 e5`
  - later checkpoints restored:
    - `0x220019ac -> c6 14 00 eb`
    - `0x220019b0 -> 00 00 50 e3`
    - `0x22001a24 -> 00 40 a0 e3`
    - `0x22001b14 -> 67 02 00 1b`
  - cache cleared successfully
- Interpretation:
  - the next single hardware run should answer whether the path reaches the real
    immediate predecessor edge into the loader setup block
  - if not, the divergence is earlier than `0x22001994`
- Classification:
  - **PREDECESSOR_MARKER_PREPARED**
- Recovery status:
  - no hardware run performed yet with this marker

#### Step 137: Run the single predecessor marker test at `0x22001994`

- Preconditions:
  - Nano manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `lsusb` also showed:
    - Nano 3G in DFU mode
- Commands:
  - run the prepared local tool once:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - after manual reboot and DFU re-entry, confirm recovery:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log showed:
    - `No cached data, performing slow action...`
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - after that the host remained in repeated:
    - `handle_events: error: libusb: interrupted [code -10]`
  - post-run `lsusb` showed:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - post-run `mks5lboot --dfuscan` reported:
    - `LIBUSB_ERROR_BUSY`
  - after manual reboot and DFU re-entry, `mks5lboot --dfuscan` again showed:
    - `05ac:1223`
    - DFU state `2`
- Interpretation:
  - there was no host-visible change attributable to the prepared marker at:
    - `0x22001994`
  - strongest current conclusion:
    - the active failing path does not reach the real predecessor edge into the
      loader setup block
- Classification:
  - **MARKER_1994_NOT_REACHED**
- Recovery status:
  - manual DFU recovery succeeded
  - device is back in clean DFU

#### Step 138: Prepare the callback-result marker at `0x22001988`

- Goal:
  - move the next proof split directly onto the callback-result branch
  - determine whether execution reaches the failure decision after the
    `[service + 0x6c]` callback return
- Commands:
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify body/disassembly at:
    - `0x22001988`
    - `0x22001994`
    - `0x220019ac`
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- Observation:
  - active next test target:
    - `0x22001988`
  - original bytes:
    - `26 00 00 0a`
  - active replacement bytes:
    - `fc 14 00 ea`
    - branch to:
      - `0x22006d80`
  - marker C at `0x22006d80` remains:
    - `0x22002138(4)` then loop
  - prepared but inactive follow-up:
    - `0x2200198c -> f3 14 00 ea`
  - restored sites:
    - `0x22001994 -> 22 00 00 0a`
    - `0x22001998 -> 00 00 99 e5`
    - `0x220019ac -> c6 14 00 eb`
    - `0x220019b0 -> 00 00 50 e3`
    - `0x22001a24 -> 00 40 a0 e3`
    - `0x22001b14 -> 67 02 00 1b`
  - cache cleared successfully
- Interpretation:
  - the next single hardware run should answer whether the path reaches the
    callback-result decision block at all
- Classification:
  - **MARKER_1988_PREPARED**
- Recovery status:
  - no hardware run performed yet with this marker

#### Step 139: Run the single callback-result marker test at `0x22001988`

- Preconditions:
  - Nano manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `lsusb` also showed:
    - Nano 3G in DFU mode
- Commands:
  - run the prepared local tool once:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - after manual reboot and DFU re-entry, confirm recovery:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log showed:
    - `No cached data, performing slow action...`
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - after that the host remained in repeated:
    - `handle_events: error: libusb: interrupted [code -10]`
  - post-run `lsusb` showed:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - post-run `mks5lboot --dfuscan` reported:
    - `LIBUSB_ERROR_BUSY`
  - after manual reboot and DFU re-entry, `mks5lboot --dfuscan` again showed:
    - `05ac:1223`
    - DFU state `2`
- Interpretation:
  - there was no host-visible change attributable to the prepared marker at:
    - `0x22001988`
  - strongest current conclusion:
    - the active failing path does not reach the callback-result branch
- Classification:
  - **MARKER_1988_NOT_REACHED**
- Recovery status:
  - manual DFU recovery succeeded
  - device is back in clean DFU

#### Step 140: Prepare the callback pre-call marker at `0x2200197c`

- Goal:
  - isolate whether execution reaches the `[service + 0x6c]` readiness callback
    at all
  - prepare the post-call split at `0x22001980` but keep it inactive
- Commands:
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify body/disassembly at:
    - `0x2200197c`
    - `0x22001980`
    - `0x22001988`
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- Observation:
  - active next test target:
    - `0x2200197c`
  - original bytes:
    - `30 ff 2f e1`
  - active replacement bytes:
    - `ef 14 00 ea`
    - branch to:
      - `0x22006d40`
  - marker A at `0x22006d40` remains:
    - `0x22002138(2)` then loop
  - prepared but inactive post-call split:
    - `0x22001980`
    - original:
      - `00 00 50 e3`
    - prepared replacement:
      - `f6 14 00 ea`
      - branch to `0x22006d60`
  - later checkpoints restored:
    - `0x22001988 -> 26 00 00 0a`
    - `0x22001994 -> 22 00 00 0a`
    - `0x22001998 -> 00 00 99 e5`
    - `0x220019ac -> c6 14 00 eb`
    - `0x220019b0 -> 00 00 50 e3`
    - `0x22001a24 -> 00 40 a0 e3`
    - `0x22001b14 -> 67 02 00 1b`
  - cache cleared successfully
- Interpretation:
  - the next single hardware run should answer whether the path reaches the
    readiness callback callsite
  - if it does, the following run should activate the already-prepared post-call
    split at `0x22001980`
- Classification:
  - **CALLBACK_PRECALL_MARKER_PREPARED**
- Recovery status:
  - no hardware run performed yet with this marker

#### Step 141: Run the single callback pre-call marker test at `0x2200197c`

- Preconditions:
  - Nano manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `lsusb` also showed:
    - Nano 3G in DFU mode
- Commands:
  - run the prepared local tool once:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - after manual reboot and DFU re-entry, confirm recovery:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log showed:
    - `No cached data, performing slow action...`
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - unlike earlier “not reached” runs, the host log then went quiet instead of
    continuing to emit repeated:
    - `handle_events: error: libusb: interrupted [code -10]`
  - post-run `lsusb` showed:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - post-run `mks5lboot --dfuscan` reported:
    - `LIBUSB_ERROR_BUSY`
  - after manual reboot and DFU re-entry, `mks5lboot --dfuscan` again showed:
    - `05ac:1223`
    - DFU state `2`
- Interpretation:
  - this run differed materially from the earlier no-marker runs
  - strongest current conclusion:
    - the callback pre-call marker likely executed
  - the next clean split should be the already-prepared post-call marker at:
    - `0x22001980`
- Classification:
  - **PRECALL_REACHED_POSTCALL_PENDING**
- Recovery status:
  - manual DFU recovery succeeded
  - device is back in clean DFU

#### Step 142: Prepare the callback post-call marker at `0x22001980`

- Goal:
  - isolate whether the `[service + 0x6c]` readiness callback returns
  - keep the callback pre-call site restored so only the post-call split is
    active
- Commands:
  - update local defanger:
    - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - rebuild local tool:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline artifact:
    - `cd /tmp/wInd3x && GOCACHE=/tmp/gocache-n3g GOPATH=/home/david/go GOMODCACHE=/home/david/go/pkg/mod GOPROXY=off GOSUMDB=off GOFLAGS=-mod=mod go run ./tmp_defang_check.go`
  - verify body/disassembly at:
    - `0x2200197c`
    - `0x22001980`
    - `0x22001988`
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- Observation:
  - active next test target:
    - `0x22001980`
  - original bytes:
    - `00 00 50 e3`
  - active replacement bytes:
    - `f6 14 00 ea`
    - branch to:
      - `0x22006d60`
  - marker B at `0x22006d60` remains:
    - `0x22002138(3)` then loop
  - callback pre-call site restored:
    - `0x2200197c -> 30 ff 2f e1`
  - later checkpoints restored:
    - `0x22001988 -> 26 00 00 0a`
    - `0x22001994 -> 22 00 00 0a`
    - `0x22001998 -> 00 00 99 e5`
    - `0x220019ac -> c6 14 00 eb`
    - `0x220019b0 -> 00 00 50 e3`
    - `0x22001a24 -> 00 40 a0 e3`
    - `0x22001b14 -> 67 02 00 1b`
  - cache cleared successfully
- Interpretation:
  - the next single hardware run should answer whether the readiness callback
    returns to WTF at all
- Classification:
  - **POSTCALL_MARKER_PREPARED**
- Recovery status:
  - no hardware run performed yet with this marker

#### Step 143: Run the single callback post-call marker test at `0x22001980`

- Preconditions:
  - Nano manually returned to DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `lsusb` also showed:
    - Nano 3G in DFU mode
- Commands:
  - run the prepared local tool once:
    - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
  - inspect post-run state:
    - `lsusb`
    - `./utils/mks5lboot/mks5lboot --dfuscan`
  - after manual reboot and DFU re-entry, confirm recovery:
    - `./utils/mks5lboot/mks5lboot --dfuscan`
- Observation:
  - host log showed:
    - `No cached data, performing slow action...`
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Sending firmware...`
  - after that the host returned to the older repeated:
    - `handle_events: error: libusb: interrupted [code -10]`
  - post-run `lsusb` showed:
    - `05ac:1242`
    - iPod Nano 3G WTF mode
  - post-run `mks5lboot --dfuscan` reported:
    - `LIBUSB_ERROR_BUSY`
  - after manual reboot and DFU re-entry, `mks5lboot --dfuscan` again showed:
    - `05ac:1223`
    - DFU state `2`
- Interpretation:
  - this differs from the pre-call test, which had gone quiet after upload
  - strongest combined reading:
    - the readiness callback is entered
    - the readiness callback does not return
- Classification:
  - **POSTCALL_NOT_REACHED**
  - **CALLBACK_ENTERED_NO_RETURN**
- Recovery status:
  - manual DFU recovery succeeded
  - device is back in clean DFU

#### Step 144: Prepare a local readiness-callback bypass after confirming `0x200036c8` never returns

- Preconditions:
  - callback split already resolved:
    - pre-call marker at `0x2200197c` likely reached
    - post-call marker at `0x22001980` not reached
  - no new hardware run requested yet
- Commands:
  - trace BootROM callback:
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x20003600 /tmp/n3g-romcb-20003600.bin | sed -n '/200036c8/,/20003770/p'`
  - rebuild local tool:
    - `env GOCACHE=/tmp/wind3x-gocache GOMODCACHE=/tmp/wind3x-gomodcache go build -o /tmp/wInd3x/wInd3x ./cmd/wInd3x`
  - regenerate offline defanged artifact:
    - `env GOCACHE=/tmp/wind3x-gocache GOMODCACHE=/tmp/wind3x-gomodcache go run /tmp/wInd3x/tmp_defang_check.go`
  - clear cached defanged WTF:
    - `rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
  - verify decoded body:
    - `env GOCACHE=/tmp/wind3x-gocache GOMODCACHE=/tmp/wind3x-gomodcache go run /tmp/wInd3x/tmp_verify_patch.go`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-defanged-check.body.fromimg.bin | sed -n '/22001970/,/220019c0/p'`
    - `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 /tmp/n3g-wtf-defanged-check.body.fromimg.bin | sed -n '/22006df0/,/22006e40/p'`
- Observation:
  - `0x200036c8` contains a readiness wait loop, not a simple boolean test
  - it can branch back to `0x200036d4` while polling:
    - `[state + 0x2c]`
    - `[state + 0x738 + 0x37]`
    - `[state + 0x04]`
  - explicit persistent writes on return were identified as:
    - `*0x2203fff8 = (*0x2203fff8)->next`
    - `*0x2203fffc = (*0x2203fffc + 0x2000)->0x720`
  - prepared Nano 3G-only callsite patch:
    - `0x2200197c`
    - `30 ff 2f e1` -> `1f 15 00 eb`
    - `blx r0` -> `bl 0x22006e00`
  - post-call site remains original:
    - `0x22001980`
    - `00 00 50 e3`
  - prepared local stub:
    - `0x22006e00`
    - replay the two observed scratch-global updates
    - `mov r0, #1`
    - return to `0x22001980`
  - offline verification succeeded:
    - `0x2200197c: eb00151f`
    - `0x22001980: e3500000`
    - `0x22006e00: e92d4010`
    - `0x22006e28: e3a00001`
    - `0x22006e2c: e8bd8010`
  - cached Nano 3G defanged WTF removed again
- Interpretation:
  - the non-returning BootROM readiness wait is now bypassed as narrowly as the
    current trace justifies
  - the prepared stub preserves the only directly observed callback-owned
    scratch-global side effects and keeps the downstream WTF logic intact
- Classification:
  - **READINESS_STUB_PREPARED**
- Recovery status:
  - no hardware run performed yet with this build

#### Step 145: Run the readiness-callback stub once on hardware

- Preconditions:
  - Nano manually entered DFU
  - pre-run scan confirmed:
    - `05ac:1223`
    - DFU state `2`
  - `lsusb` showed:
    - Nano 3G present in DFU mode
- Command:
  - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- Observation:
  - host log showed:
    - `No cached data, performing slow action...`
    - `Parsed image. kind="Nano 3G"`
    - `Running rce....`
    - `Haxed DFU running!`
    - `Sending defanged WTF...`
    - `Got dfuMANIFEST, image uploaded.`
    - `Waiting 10s for device to switch to WTF mode...`
    - `Error: device did not switch to WTF mode: context deadline exceeded`
  - immediate post-run USB state:
    - `lsusb` no longer showed the Nano at all
    - `mks5lboot --dfuscan` reported:
      - `DFU device not found`
  - repeated checks after the observation window still showed:
    - no `05ac:1242`
    - no DFU device
  - after manual reboot and DFU re-entry, recovery was confirmed:
    - `05ac:1223`
    - DFU state `2`
- Interpretation:
  - this is a material change from the earlier stable WTF-stuck runs
  - the device no longer re-enumerates as:
    - `05ac:1242`
  - host-only evidence is consistent with boot progress past the previous WTF
    blocker, but does not distinguish successful UI boot from a later crash
    without direct screen observation
- Classification:
  - **CHAINLOAD_BOOT_PROGRESS**
- Recovery status:
  - manual DFU recovery succeeded
  - device is back in clean DFU

## 2026-04-24 Prepared next hardware image: early OSOS backlight hook

- Motivation:
  - readiness stub changed Nano 3G behavior from:
    - stable WTF `05ac:1242`
  - to:
    - disappearing from USB entirely after upload
  - but the runtime visibility patch still showed nothing on screen
- Working assumption:
  - RetailOS progresses, but display/backlight still is not visibly enabled

Prepared OSOS patch:

- keep the existing connected-runtime proof strings
- keep the `0x57d0 -> nop` visibility-path patch
- add a new earlier startup hook

Exact startup hook:

- runtime `0x22003a9c`
- body offset `0x3a9c`
- original:
  - `bd 00 00 eb`
  - `bl 0x22003d98`
- replacement:
  - `43 14 00 eb`
  - `bl 0x22008bb0`

Local stub:

- runtime `0x22008bb0`
- body offset `0x8bb0`
- original:
  - 16 bytes of `00`
- replacement bytes:
  - `04 e0 2d e5`
  - `77 ec ff eb`
  - `a0 f2 ff eb`
  - `04 f0 9d e4`
- verified behavior:
  - `push {lr}`
  - `bl 0x22003d98`
  - `bl 0x22005640`
  - `pop {pc}`

Reason this path was selected:

- `0x22005640` is still the best compact Apple-owned "visibility on" wrapper
  recovered from the decrypted OSOS body
- this patch preserves the original startup helper before invoking the Apple
  visibility wrapper
- no new raw PMU/MMIO guesses were introduced

Prepared files:

- `/tmp/wInd3x/cmd/patch_n3g_backlight_signal.go`
- `/tmp/patch_n3g_backlight_signal`
- `/tmp/n3g-osos-work/n3g-osos-cfw-backlight-n3g.dfu`
- `/tmp/n3g-osos-work/lcd-osos-backlight-n3g.bin`

Run status:

- prepared only
- not yet tested on hardware

## 2026-04-24 Single hardware run with early OSOS backlight hook

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-backlight-n3g.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result from continuous 90-second observation:
  - black screen / no visible change for the full window
  - no Apple logo
  - no backlight
  - no flicker
  - no patched text
  - no stock text
  - no visible reset loop

Classification:

- **RUNTIME_BLACKSCREEN**

Recovery status:

- manual DFU recovery succeeded
- device is back in clean DFU:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-24 Prepared later OSOS runtime proof hook after black-screen tests

Reason for changing strategy:

- readiness-stub handoff now gets the Nano past the old WTF-stuck behavior
- but both the connected-runtime visibility patch and the early
  `0x22003a9c -> 0x22005640` hook still produced:
  - black screen
  - no backlight
  - no visible text

So the next proof needs to be:

- later than startup/display wrappers
- Apple-owned
- non-display if possible

Recovered later runtime map:

- entry:
  - `0x22008814 -> 0x220039c4`
- early startup helpers:
  - `0x22003a9c`
  - `0x22003d98`
- early display cluster:
  - `0x22005620`
  - `0x22005640`
  - `0x22005660`
  - `0x220057d0`
- later controller/runtime region:
  - `0x220ffb00..0x22100324`
- Apple-owned sound path:
  - `0x2219d048`
  - `0x2219d1e8`
  - `0x2219d410`
  - `0x22276c98`

Selected later proof patch:

- input:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- output:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-later-beep-n3g.dfu`
- body dump:
  - `/tmp/n3g-osos-work/lcd-osos-later-beep-n3g.bin`
- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_later_beep_signal.go`

Exact hook:

- runtime:
  - `0x22100318`
- body offset:
  - `0x100318`
- original:
  - `91 fb ff eb`
  - `bl 0x220ff164`
- replacement:
  - `2c 22 fc eb`
  - `bl 0x22008bd0`

Local stub:

- runtime:
  - `0x22008bd0`
- body offset:
  - `0x8bd0`
- verified disassembly:
  - `push {r0, r1, lr}`
  - `bl 0x220ff164`
  - `ldr r0, [sp]`
  - `ldr r0, [r0, #0xc8]`
  - `add r1, sp, #12`
  - `bl 0x2219d410`
  - `pop {r0, r1, pc}`

Meaning:

- preserve the original later-runtime Apple call
- then invoke the Apple-owned `Beep` manager from the same caller frame
- do not introduce new storage, USB PHY, shutdown, or raw MMIO changes

Verification commands/results:

- `xxd -g 4 -l 32 -s 0x100318 /tmp/n3g-osos-work/lcd-osos-later-beep-n3g.bin`
  - `00100318: 2c22fceb ...`
- `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 --start-address=0x22100310 --stop-address=0x22100328 /tmp/n3g-osos-work/lcd-osos-later-beep-n3g.bin`
  - `0x22100318: ebfc222c  bl 0x22008bd0`
- `arm-none-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000 --start-address=0x22008bd0 --stop-address=0x22008bec /tmp/n3g-osos-work/lcd-osos-later-beep-n3g.bin`
  - stub disassembles as expected

Run status:

- prepared only
- no hardware run performed yet

Classification:

- **LATER_HOOK_PATCH_READY**

## 2026-04-24 Single hardware run with later Apple-owned beep proof hook

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-later-beep-n3g.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen/audio result:
  - black screen for the full window
  - no Apple logo
  - no backlight
  - no flicker
  - no patched text
  - no stock text
  - no audible beep/tone/click
  - no visible reset loop

Classification:

- **RUNTIME_SIGNAL_BLOCKED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

#### Step 146: Analyze why the late hardware-init patch stayed black

- reviewed the patched helper:
  - `/tmp/wInd3x/cmd/patch_n3g_display_hw_init.go`
- confirmed two concrete patch bugs:
  - it called `0x2200455c` directly, but `0x2200455c` is inside the larger
    routine at `0x220044dc..0x22004598`, not a real entry
  - it called `0x220048bc` directly, but the real panel-helper entry is
    `0x2200488c`; `0x220048bc` is inside that function after the prologue
- mapped the consequence:
  - the previous hardware patch did not prove the intended Apple LCD path was
    invoked correctly
- prepared hook reachability probes:
  - `/tmp/wInd3x/cmd/patch_n3g_display_hook_reach.go`
  - `entry` image:
    - `/tmp/n3g-osos-work/n3g-osos-cfw-display-hook-entry-n3g.dfu`
  - `after0742c` image:
    - `/tmp/n3g-osos-work/n3g-osos-cfw-display-hook-after0742c-n3g.dfu`
- prepared corrected lower-level patch:
  - `/tmp/wInd3x/cmd/patch_n3g_display_gate_callbacks.go`
  - force late gate open at `0x22002f94`
  - hook `0x22002f9c` to a scratch stub at `0x2200d6e8`
  - install minimal runtime callback table at `0x22007398`
  - reseed `0x2200700c`, `0x2200739c`, `0x220073a0`
  - call only real Apple entrypoints:
    - `0x22003c98`
    - `0x22003ce0(1)`
    - `0x22003ce0(4)`
    - `0x22003d14`
    - `0x22003cc0`
    - `0x22005640`
- produced artifacts:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-display-gate-callbacks-n3g.dfu`
  - `/tmp/n3g-osos-work/lcd-osos-display-gate-callbacks-n3g.bin`
- no hardware run performed in this step

## 2026-04-25 Single hardware run with hardware-level LCD/backlight init patch

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-displayhw-n3g.dfu`
- patch summary:
  - hook site:
    - `0x22002f9c`
    - `22 11 00 eb` -> `03 17 00 eb`
  - stub:
    - `0x22008bb0`
    - call original `0x2200742c`
    - preserve return value
    - call `0x2200455c`
    - call `0x220048bc(1, 0)`
    - call `0x220048bc(4, 0)`
    - call `0x22005640`
    - return original `r0`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **HARDWARE_PATCH_POST_ESCAPE_SIGNATURE**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with late OSOS visibility wrapper patch

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visibility-late-n3g.dfu`
- patch summary:
  - hook site:
    - `0x22002f9c`
    - `22 11 00 eb` -> `03 17 00 eb`
  - stub:
    - `0x22008bb0`
    - call original `0x2200742c`
    - preserve return value
    - call Apple visibility-on wrapper `0x22005640`
    - return original `r0`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **VISIBILITY_PATCH_POST_ESCAPE_SIGNATURE**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with late display-hook entry reachability probe

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-display-hook-entry-n3g.dfu`
- patch summary:
  - hook site:
    - `0x22002f9c`
    - `22 11 00 eb` -> `03 17 00 eb`
  - stub:
    - `0x22008bb0`
    - `mov r0, #48`
    - `bl 0x22002138`
    - `b .`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **DISPLAY_HOOK_ENTRY_REACHED**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-25 Single hardware run with dispatch-table seed patch for slot (1,0)

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-seed-dispatch-1-0-n3g.dfu`
- patch intent:
  - seed runtime dispatch-table slot `(1,0)` at `0x2200f880`
  - via the official runtime writer `0x22004c08`
  - from the known-reached late display site `0x22002f9c`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - black screen / no visible change
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Classification:

- **TABLE_SEED_POST_ESCAPE_SIGNATURE**

Recovery status:

- confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with framebuffer white test-pattern patch

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-fb-testpattern-n3g.dfu`
- patch intent:
  - fill candidate framebuffer `0x08a3a20c` with solid white over `0x25800` bytes
  - then call the existing display pipeline-start wrapper `0x220085c8`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - black screen / no visible change
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Classification:

- **FRAMEBUFFER_TEST_PATTERN_POST_ESCAPE_SIGNATURE**

Recovery status:

- confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with forced display-pipeline-start wrapper

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-force-pipeline-start-n3g.dfu`
- patch intent:
  - force the first recovered display pipeline activation wrapper
  - from the known-reached late display site `0x22002f9c`
  - via `0x220085c8`, which drives `0x220049b0` with worker mode `r3 = 1`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - black screen / no visible change
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Classification:

- **FINAL_DISPLAY_PATCH_POST_ESCAPE_SIGNATURE**

Recovery status:

- confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with direct panel-worker candidate `0x220049b0`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-force-panel-worker-direct-n3g.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - black screen / no visible change
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Classification:

- **BACKLIGHT_CANDIDATE_3_POST_ESCAPE_SIGNATURE**

Recovery status:

- confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with direct PMU bit-0 candidate `0x220054f8`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-force-pmu-reg43-direct-n3g.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - black screen / no visible change
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Classification:

- **BACKLIGHT_CANDIDATE_2_POST_ESCAPE_SIGNATURE**

Recovery status:

- confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with direct PMU-register candidate `0x220054b0`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-force-pmu-reg1b-direct-n3g.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - black screen / no visible change
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Classification:

- **BACKLIGHT_CANDIDATE_1_POST_ESCAPE_SIGNATURE**

Recovery status:

- confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with forced panel-preamble candidate `0x220045b0`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-force-panel-preamble-n3g.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - pending user observation

Classification:

- **PANEL_PREAMBLE_PATCH_POST_ESCAPE_SIGNATURE**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-25 Single hardware run with PMU enable force patch

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-force-pmu-enable-n3g.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - black screen / no visible change
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Classification:

- **FORCE_BACKLIGHT_PATCH_POST_ESCAPE_SIGNATURE**

Recovery status:

- confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with direct low-level LCD init force patch

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-force-lowlevel-display-n3g.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - black screen / no visible change
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Classification:

- **FORCE_DISPLAY_PATCH_POST_ESCAPE_SIGNATURE**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-25 Single hardware run with caller-side second-stage dispatcher exit marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-caller-stage-next7-marker-2f60.dfu`
- marker:
  - `0x22002f60`
  - `f8 8f bd e8` -> `26 19 00 ea`
  - stub ID:
    - `29`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **CALLER_STAGE_NEXT7_MARKER_REACHED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with post-`0x22002f60` startup-stage entry marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-startup-stage-after-2f60-marker-2f70.dfu`
- marker:
  - `0x22002f70`
  - `10 40 2d e9` -> `22 19 00 ea`
  - stub ID:
    - `30`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **STARTUP_STAGE_AFTER_2F60_MARKER_REACHED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with `0x22002f70` startup-stage first-call marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-startup-stage-2f74-marker.dfu`
- marker:
  - `0x22002f74`
  - `26 03 00 eb` -> `21 19 00 ea`
  - stub ID:
    - `31`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **STARTUP_STAGE_2F74_MARKER_REACHED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with `0x22002f70` startup-stage second-call marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-startup-stage-2f78-marker.dfu`
- marker:
  - `0x22002f78`
  - `74 02 00 eb` -> `20 19 00 ea`
  - stub ID:
    - `32`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **STARTUP_STAGE_2F78_MARKER_REACHED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with `0x22002f70` startup-stage third-call marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-startup-stage-2f7c-marker.dfu`
- marker:
  - `0x22002f7c`
  - `6d 02 00 eb` -> `1f 19 00 ea`
  - stub ID:
    - `33`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **STARTUP_STAGE_2F7C_MARKER_REACHED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with first post-call state-step marker in `0x22002f70` stage

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-startup-stage-2f84-marker.dfu`
- marker:
  - `0x22002f84`
  - `25 ff ff eb` -> `1d 19 00 ea`
  - stub ID:
    - `34`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **STARTUP_STAGE_2F84_MARKER_REACHED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with next-call marker at `0x22002f88`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-startup-stage-2f88-marker.dfu`
- marker:
  - `0x22002f88`
  - `7a 01 00 eb` -> `1c 19 00 ea`
  - stub ID:
    - `35`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **STARTUP_STAGE_2F88_MARKER_REACHED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with second state-transition marker at `0x22002f90`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-startup-stage-2f90-marker.dfu`
- marker:
  - `0x22002f90`
  - `22 ff ff eb` -> `1a 19 00 ea`
  - stub ID:
    - `36`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **STARTUP_STAGE_2F90_MARKER_REACHED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Single hardware run with next-call marker at `0x22002f94`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-startup-stage-2f94-marker.dfu`
- marker:
  - `0x22002f94`
  - `58 02 00 eb` -> `19 19 00 ea`
  - stub ID:
    - `37`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **STARTUP_STAGE_2F94_MARKER_REACHED**

Recovery status:

- recovery confirmed clean:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-24 Caller-side startup-stage wrapper run

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-caller-stage-marker-42f4.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- classification:
  - **CALLER_STAGE_MARKER_REACHED**
- recovery:
  - confirmed later by manual DFU re-entry
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Second-stage second-callback-return run

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-caller-stage-next6-marker-2f48.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- classification:
  - **CALLER_STAGE_NEXT6_MARKER_REACHED**
- recovery:
  - pending manual DFU re-entry confirmation

## 2026-04-25 Second-stage first-callback-return run

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-caller-stage-next5-marker-2ee0.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- classification:
  - **CALLER_STAGE_NEXT5_MARKER_REACHED**
- recovery:
  - confirmed later by manual DFU re-entry
  - `05ac:1223`
  - DFU state `2`

## 2026-04-25 Caller-side second-stage dispatcher run

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-caller-stage-next4-marker-2ea4.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- classification:
  - **CALLER_STAGE_NEXT4_MARKER_REACHED**
- recovery:
  - confirmed later by manual DFU re-entry
  - `05ac:1223`
  - DFU state `2`

## 2026-04-24 Third caller-side startup-stage run

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-caller-stage-next3-marker-2e70.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- classification:
  - **CALLER_STAGE_NEXT3_MARKER_REACHED**
- recovery:
  - confirmed later by manual DFU re-entry
  - `05ac:1223`
  - DFU state `2`

## 2026-04-24 Second caller-side startup-stage run

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-caller-stage-next2-marker-2e4c.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- classification:
  - **CALLER_STAGE_NEXT2_MARKER_REACHED**
- recovery:
  - confirmed later by manual DFU re-entry
  - `05ac:1223`
  - DFU state `2`

## 2026-04-24 Next caller-side startup-stage run

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-caller-stage-next-marker-2e28.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- classification:
  - **CALLER_STAGE_NEXT_MARKER_REACHED**
- recovery:
  - confirmed later by manual DFU re-entry
  - `05ac:1223`
  - DFU state `2`

## 2026-04-24 Hardware run with early OSOS marker at `0x220024e4`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-early-marker-024e4.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano present
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **EARLY_OSOS_MARKER_REACHED**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 Hardware run with OSOS entry-loop probe on preserved-service68 baseline

Pre-run state:

- clean DFU:
  - `05ac:1223`
  - state `2`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-entry-loop-probe.dfu`

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano present
- post-run `mks5lboot --dfuscan` found no DFU device

Classification:

- **POST_ESCAPE_OSOS_ENTRY_REACHED**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 Exact `[service + 0x68]` callback target resolved

Recovered from the BootROM service table:

- `0x20000088 -> 0x200035a8`

Current status:

- original `[service + 0x68]` must remain active
- preserving it changes behavior to “USB disappears entirely”
- bypassing it collapses back to the DFU-resident state

Current blocker:

- the existing local BootROM dump set does not include the code body at
  `0x200035a8`
- exact side-effect reconstruction is therefore blocked on a new dump covering
  that address range

## 2026-04-24 Preserved-`[service + 0x68]` baseline prepared

Prepared but not yet run:

- restored:
  - `0x1938 = 31 ff 2f e1`
  - `0x193c = 27 00 a0 e3`
  - `0x197c = 30 ff 2f e1`
  - `0x1b14 = 67 02 00 1b`
  - `0x24b8 = 10 40 2d e9`
  - `0x24c8 = 34 ff 2f e1`
- active:
  - `0x19ac = c6 14 00 eb`
  - `0x19d0 = 01 10 b0 e3`
  - `0x6558 = 00 00 a0 e3 1e ff 2f e1`

Verified with raw-body dumper only.

Expected next classifications:

- `PRESERVED_SERVICE68_EXECUTE_CHANGED`
- `PRESERVED_SERVICE68_BLACKSCREEN`
- `PRESERVED_SERVICE68_DFU_RESIDENT`

## 2026-04-24 Hardware run with preserved-`[service + 0x68]` execute baseline

Pre-run state:

- clean DFU:
  - `05ac:1223`
  - state `2`

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano present
- post-run `mks5lboot --dfuscan` found no DFU device

Classification:

- **PRESERVED_SERVICE68_EXECUTE_CHANGED**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 Repeat hardware run with original `[service + 0x68]`

Repeated from clean DFU with the same split:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 69 17 00 ea`

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano present
- post-run `mks5lboot --dfuscan` showed no DFU device found

Classification:

- **SERVICE_68_RETURNS**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 Hardware run with cache-aware DRAM execution probe

Pre-run state:

- clean DFU:
  - `05ac:1223`
  - state `2`

Tested path:

- preserve wrapper path and current WTF-side handoff fixes
- replace final handoff with a local copy stub that:
  - writes:
    - `ldr pc, [pc, #-4]`
    - `.word 0x220076d4`
    into `0x08000800`
  - cleans dcache
  - invalidates icache
  - jumps to `0x08000800`
- marker target:
  - `0x220076d4`
  - `0x22002138(12)`
  - loop forever

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Observed screen result:

- black screen / no visible change
- no Apple logo
- no backlight
- no flicker
- no text
- no visible reset loop

Interpretation:

- the stronger DRAM probe did not change the late-handoff signature
- the negative result is no longer explainable by a missing cache clean /
  icache invalidate alone
- execution from `0x08000800` is still not externally proven

Classification:

- **STILL_BLOCKED_WITH_REASON**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 IRAM execution control probe prepared

Prepared but not yet run:

- final wrapper handoff:
  - `0x220024c8`
  - `34 ff 2f e1` -> `81 14 00 ea`
  - branch to:
    - `0x220076d4`

IRAM control stub:

- `0x220076d4`
- behavior:
  - call `0x22002138(12)`
  - loop forever

Verified with raw-body dumper:

- `0x24c8 = 81 14 00 ea`
- `0x76d4 = 0c 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- active prerequisite fixes still present:
  - `0x1938 = 64 17 00 eb`
  - `0x197c = 1f 15 00 eb`
  - `0x19ac = c6 14 00 eb`
  - `0x19d0 = 01 10 b0 e3`
  - `0x6558 = 00 00 a0 e3 1e ff 2f e1`

Expected next classification:

- `IRAM_EXECUTION_CONTROL_READY`

## 2026-04-24 Prepared DFU-context diagnostic for `[service + 0x68]`

Prepared but not yet run:

- restore original callback:
  - `0x22001938 = 31 ff 2f e1`
- place immediate post-call marker:
  - `0x2200193c = 69 17 00 ea`
  - branch to:
    - `0x220076e8`

Marker stub:

- `0x220076e8`
- call `0x22002138(13)`
- loop forever

Verified with raw-body dumper:

- `0x1938 = 31 ff 2f e1`
- `0x193c = 69 17 00 ea`
- `0x76e8 = 0d 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`

Still active prerequisite fixes:

- readiness stub
- loader callback stub
- UART immediate-return patch
- status emulation

Expected next classification:

- `SERVICE_68_RETURNS`
- `SERVICE_68_BLOCKED`

## 2026-04-24 Hardware run with original `[service + 0x68]` and post-call marker

Pre-run state:

- clean DFU:
  - `05ac:1223`
  - state `2`

Tested state:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 69 17 00 ea`
- marker target:
  - `0x220076e8`
  - `0x22002138(13)`
  - loop forever

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - no Nano present
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device found

Interpretation:

- strongest reading is that the original `[service + 0x68]` callback returned
  and control advanced into the immediate post-call marker
- this is stronger evidence for missing callback side effects than for another
  non-return path

Classification:

- **SERVICE_68_RETURNS**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 DRAM probe negative-result analysis and stronger prep

The first DRAM probe stayed in the same DFU-resident state, but the probe was
not decisive because it wrote code into DRAM and jumped there without cache
maintenance.

Prepared stronger probe:

- `0x220024c8 = a2 14 00 ea`
- `0x220076d4`
  - marker stub:
    - `0x22002138(12)`
    - loop forever
- `0x22007758..0x2200777c`
  - copy known redirect stub into destination
  - clean dcache
  - invalidate icache
  - jump to destination

Prepared destination literals:

- `0x08000800`
- `0x08000000`
- `0x08001000`
- `0x22008000`

Status:

- verified with raw-body dumper only
- no hardware run performed yet for the stronger probe

## 2026-04-24 OSOS entry dependency and DRAM probe prepared

Prepared but did not run a raw DRAM execution probe.

Reason:

- OSOS body entry at `0x08000800` is now understood as a relocation trampoline
- the first startup path quickly aims toward the linked IRAM image at
  `0x22000000`
- this means a failure at or after `0x08000800` may still be caused by OSOS
  entry assumptions rather than inability to execute DRAM at all

Probe state:

- `0x220024c8 = a2 14 00 ea`
- `0x220076d4`
  - marker stub:
    - `0x22002138(12)`
    - loop forever
- `0x22007758..0x22007778`
  - copy stub that writes:
    - `ldr pc, [pc, #-4]`
    - `.word 0x220076d4`
    into `0x08000800`
  - then jumps to `0x08000800`

Purpose:

- isolate whether execution from `0x08000800` works at all when OSOS is removed
  from the equation

Status:

- verified with raw-body dumper only
- no hardware run performed yet

## 2026-04-24 - direct PC entry patch prepared

Prepared but did not run a direct-PC entry experiment.

Patch state:

- `0x22001b14` left original:
  - `67 02 00 1b`
- `0x220024c8` patched:
  - `34 ff 2f e1 -> a7 14 00 ea`
- stub at `0x2200776c`:
  - `04 f0 1f e5 00 08 00 08`
  - direct `pc` load to `0x08000800`

Purpose:

- preserve wrapper entry and `0x22002138(0)`
- remove final `blx r4` semantics
- test whether a direct non-returning branch into `0x08000800` changes the
  DFU-resident late-handoff behavior

Status:

- verified with raw-body dumper only
- no hardware run performed yet

## 2026-04-24 execution-context fix run result

Hardware run performed with the context stub at the final handoff:

- `0x220024c8 = a2 14 00 ea`
- `0x22007758 = 17 e4 ff eb 60 e6 ff eb 6a e6 ff eb 63 e6 ff eb`
- `0x22007768 = 72 e6 ff eb 6c e6 ff eb`
- `0x22007770 = 04 f0 1f e5 00 08 00 08`

Observed:

- exploit and defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run USB stayed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`
- screen showed nothing
- no Apple logo
- no backlight
- no flicker
- no text
- no visible reset loop

Classification:

- **CONTEXT_BLOCKED_WITH_REASON**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 - direct PC entry run result

Hardware run performed with:

- `0x22001b14 = 67 02 00 1b`
- `0x220024c8 = a7 14 00 ea`
- `0x2200776c = 04 f0 1f e5 00 08 00 08`

Observed:

- exploit and defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run USB stayed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`
- screen stayed black for the full window
- no Apple logo
- no backlight
- no flicker
- no text
- no visible reset loop

Classification:

- **DIRECT_PC_SAME_DFU_RESIDENT_STATE**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 execution-context fix prepared

Prepared but did not run a context-oriented handoff patch.

Reason:

- direct `pc = 0x08000800` did not change behavior
- solved S5L87xx handoff explicitly disables IRQ and tears down caches before
  entry
- Nano 3G WTF disables IRQ/FIQ earlier, then restores CPSR interrupt bits via
  `0x220007d0` just before execute

Patch state:

- `0x220024c8 = a2 14 00 ea`
- `0x22007758 = 17 e4 ff eb 60 e6 ff eb 6a e6 ff eb 63 e6 ff eb`
- `0x22007768 = 72 e6 ff eb 6c e6 ff eb`
- `0x22007770 = 04 f0 1f e5 00 08 00 08`

Stub behavior:

- disable IRQ/FIQ
- disable MMU
- disable dcache
- disable icache
- invalidate/drain caches
- load `pc = 0x08000800`

Status:

- verified with raw-body dumper only
- no hardware run performed yet

## 2026-04-24 Preparation for forced-r4 handoff run

Prepared wrapper-preserving forced-target variant:

- `0x22001b14 = 67 02 00 1b`
- `0x220024c8 = a7 14 00 ea`
- `0x2200776c` stub:
  - load `r4 = 0x08000800`
  - `blx r4`
  - return to `0x220024cc`

Run status:

- not run yet

## 2026-04-24 Single hardware run with forced-r4 handoff patch

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active forced-r4 change:
  - `0x220024c8 = a7 14 00 ea`
  - stub at `0x2200776c` loads `r4 = 0x08000800`, executes `blx r4`, then
    branches to `0x220024cc`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **TARGET_STATE_STILL_BLOCKED**

## 2026-04-24 Entry-first-loop probe prepared

Prepared OSOS entry probe image:

- `/tmp/n3g-osos-work/n3g-osos-entry-first-loop-probe.dfu`
- `/tmp/n3g-osos-work/lcd-osos-entry-first-loop-probe.bin`

Verified entry-state assumptions:

- IMG1 header entrypoint:
  - `0x0`
- expected runtime load base:
  - `0x08000000`
- expected runtime body entry:
  - `0x08000800`
- wrapper handoff target:
  - `r4 = 0x08000800`

Verified stock OSOS entry bytes:

- `0x08000800 = 00 22 00 ea`
- `0x08000804 = f2 0c 00 ea`

Verified probe bytes:

- `0x08000800 = fe ff ff ea`
- disassembly:
  - `b 0x08000800`

Run status:

- not run yet

## 2026-04-24 Single hardware run with OSOS first-instruction loop probe

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active OSOS entry probe:
  - `0x08000800`
  - `00 22 00 ea` -> `fe ff ff ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification status:

- host-side only:
  - `ENTRY_EXECUTION_REACHED` not confirmed

## 2026-04-24 Preparation for direct-jump wrapper-bypass run

Prepared direct-jump execute variant:

- `0x22001b14`
  - `67 02 00 1b` -> `6b 02 00 ea`
  - branch directly to `0x220024c8`

Raw-body verification confirmed:

- `0x22001b14 = 6b 02 00 ea`
- `0x220024b8 = 10 40 2d e9`
- `0x220024bc = 00 40 a0 e1`
- `0x220024c0 = 00 00 a0 e3`
- `0x220024c4 = 1b ff ff eb`
- `0x220024c8 = 34 ff 2f e1`

Run status:

- not run yet

## 2026-04-24 Single hardware run with direct-jump wrapper-bypass test

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active direct-jump change:
  - `0x22001b14`
  - `67 02 00 1b` -> `6b 02 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **DIRECT_JUMP_SAME_DFU_RESIDENT_STATE**

## 2026-04-24 Single hardware run with the 0x22001b14 execute-gate marker

- active execute-gate split:
  - `0x22001b14`
  - `67 02 00 1b` -> `14 17 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1B14_REACHED**

## 2026-04-24 Preparation for single 0x22001a78 tail-marker run

Prepared the next Nano 3G WTF tail marker after the confirmed `0x22001a4c`
reach:

- `0x22001a78`
  - original bytes:
    - `32 ff 2f e1`
  - replacement bytes:
    - `22 17 00 ea`
  - marker stub:
    - `0x22007708`

Raw-body verification confirmed:

- `0x22001a24 = 00 40 a0 e3`
- `0x22001a28 = 63 fb ff eb`
- `0x22001a4c = 2a 06 00 eb`
- `0x22001a78 = 22 17 00 ea`

Supporting active Nano 3G patches still present:

- readiness stub at `0x2200197c`
- loader callback stub at `0x220019ac`
- UART immediate-return patch at `0x22006558`
- `[service + 0x68]` bypass at `0x22001938`
- status emulation at `0x220019d0`

One layout fix was also applied before verification:

- moved the USB product string to `0x2200771c`
- kept the active marker stub at `0x22007708`

Run status:

- not run yet
- next expected classification:
  - `MARKER_1A78_REACHED`
  - `MARKER_1A78_NOT_REACHED`

## 2026-04-24 Single hardware run with the 0x22001a78 tail marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active tail split:
  - `0x22001a78`
  - `32 ff 2f e1` -> `22 17 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1A78_REACHED**

Recovery status:

- pending clean manual DFU reconfirmation

## 2026-04-24 Preparation for single 0x220024b8 wrapper-entry run

Prepared the execute-wrapper entry marker:

- `0x220024b8`
  - original bytes:
    - `10 40 2d e9`
  - replacement bytes:
    - `ab 14 00 ea`
  - marker stub:
    - `0x2200776c`

Raw-body verification confirmed:

- `0x220024b8 = ab 14 00 ea`
- `0x220024bc = 00 40 a0 e1`
- `0x220024c0 = 00 00 a0 e3`
- `0x220024c4 = 1b ff ff eb`
- `0x220024c8 = 34 ff 2f e1`
- `0x22001b14 = 67 02 00 1b`

Optional direct-jump patch prepared but not applied:

- `0x220024b8`
  - `10 40 2d e9` -> `02 00 00 ea`
  - branch directly to `0x220024c8`

Run status:

- not run yet
- next expected classification:
  - `WRAPPER_ENTRY_REACHED`
  - `WRAPPER_BLOCKED`

## 2026-04-24 Preparation for single 0x220024c8 wrapper-pre-jump run

Prepared the execute-wrapper pre-jump marker:

- `0x220024c8`
  - original bytes:
    - `34 ff 2f e1`
  - replacement bytes:
    - `a7 14 00 ea`
  - marker stub:
    - `0x2200776c`

Raw-body verification confirmed:

- `0x220024b8 = 10 40 2d e9`
- `0x220024bc = 00 40 a0 e1`
- `0x220024c0 = 00 00 a0 e3`
- `0x220024c4 = 1b ff ff eb`
- `0x220024c8 = a7 14 00 ea`
- `0x22001b14 = 67 02 00 1b`

Run status:

- not run yet
- next expected classification:
  - `WRAPPER_PREJUMP_REACHED`
  - `WRAPPER_BLOCKED`

## 2026-04-24 Single hardware run with the 0x220024c8 wrapper-pre-jump marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active wrapper pre-jump split:
  - `0x220024c8`
  - `34 ff 2f e1` -> `a7 14 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **WRAPPER_PREJUMP_REACHED**

## 2026-04-24 Single hardware run with the 0x220024b8 wrapper-entry marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active wrapper-entry split:
  - `0x220024b8`
  - `10 40 2d e9` -> `ab 14 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - host also printed:
    - `handle_events: error: libusb: interrupted [code -10]`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **WRAPPER_ENTRY_REACHED**

## 2026-04-24 Single hardware run with the 0x22001b0c execute-gate marker

- active execute-gate split:
  - `0x22001b0c`
  - `10 00 15 e3` -> `16 17 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1B0C_REACHED**

## 2026-04-24 Preparation for single 0x22001b04 execute-gate run

Prepared the first execute-gate-region marker:

- `0x22001b04`
  - original bytes:
    - `00 00 54 e3`
  - replacement bytes:
    - `18 17 00 ea`
  - marker stub:
    - `0x2200776c`

Raw-body verification confirmed:

- `0x22001ae8 = 0d 03 00 eb`
- `0x22001b04 = 18 17 00 ea`
- `0x22001b08 = 02 00 00 1a`
- `0x22001b0c = 10 00 15 e3`
- `0x22001b10 = 07 00 86 10`
- `0x22001b14 = 67 02 00 1b`

Supporting active Nano 3G patches still present:

- readiness stub at `0x2200197c`
- loader callback stub at `0x220019ac`
- UART immediate-return patch at `0x22006558`
- `[service + 0x68]` bypass at `0x22001938`
- status emulation at `0x220019d0`

Run status:

- not run yet
- next expected classification:
  - `MARKER_1B04_REACHED`
  - `MARKER_1B04_NOT_REACHED`

## 2026-04-24 Single hardware run with unconditional execute-force patch

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active execute-force change:
  - `0x22001b14`
  - `67 02 00 1b` -> `67 02 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification status:

- pending device-side screen observation

## 2026-04-24 Single hardware run with the 0x22001b04 execute-gate marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active execute-gate split:
  - `0x22001b04`
  - `00 00 54 e3` -> `18 17 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1B04_REACHED**

Recovery status:

- pending clean manual DFU reconfirmation

## 2026-04-24 Preparation for single 0x22001ae8 tail-marker run

Prepared the final unresolved Nano 3G WTF tail marker:

- `0x22001ae8`
  - original bytes:
    - `0d 03 00 eb`
  - replacement bytes:
    - `1f 17 00 ea`
  - marker stub:
    - `0x2200776c`

Raw-body verification confirmed:

- `0x22001ae0 = 3a fb ff eb`
- `0x22001ae8 = 1f 17 00 ea`

Supporting active Nano 3G patches still present:

- readiness stub at `0x2200197c`
- loader callback stub at `0x220019ac`
- UART immediate-return patch at `0x22006558`
- `[service + 0x68]` bypass at `0x22001938`
- status emulation at `0x220019d0`

Run status:

- not run yet
- next expected classification:
  - `MARKER_1AE8_REACHED`
  - `MARKER_1AE8_NOT_REACHED`

## 2026-04-24 Single hardware run with the 0x22001ae8 tail marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active tail split:
  - `0x22001ae8`
  - `0d 03 00 eb` -> `1f 17 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1AE8_REACHED**

Recovery status:

- pending clean manual DFU reconfirmation

## 2026-04-24 Preparation for single 0x22001ae0 tail-marker run

Prepared the last meaningful Nano 3G WTF tail marker before the still-unreached
`0x22001ae8` call:

- `0x22001ae0`
  - original bytes:
    - `3a fb ff eb`
  - replacement bytes:
    - `21 17 00 ea`
  - marker stub:
    - `0x2200776c`

Raw-body verification confirmed:

- `0x22001a24 = 00 40 a0 e3`
- `0x22001a28 = 63 fb ff eb`
- `0x22001a4c = 2a 06 00 eb`
- `0x22001a78 = 32 ff 2f e1`
- `0x22001ab0 = 32 ff 2f e1`
- `0x22001ae0 = 21 17 00 ea`

Supporting active Nano 3G patches still present:

- readiness stub at `0x2200197c`
- loader callback stub at `0x220019ac`
- UART immediate-return patch at `0x22006558`
- `[service + 0x68]` bypass at `0x22001938`
- status emulation at `0x220019d0`

Run status:

- not run yet
- next expected classification:
  - `MARKER_1AE0_REACHED`
  - `MARKER_1AE0_NOT_REACHED`

## 2026-04-24 Single hardware run with the 0x22001ae0 tail marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active tail split:
  - `0x22001ae0`
  - `3a fb ff eb` -> `21 17 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1AE0_REACHED**

Recovery status:

- pending clean manual DFU reconfirmation

## 2026-04-24 Preparation for single 0x22001ab0 tail-marker run

Prepared the next Nano 3G WTF tail marker after the confirmed `0x22001a78`
reach:

- `0x22001ab0`
  - original bytes:
    - `32 ff 2f e1`
  - replacement bytes:
    - `28 17 00 ea`
  - marker stub:
    - `0x22007758`

Raw-body verification confirmed:

- `0x22001a24 = 00 40 a0 e3`
- `0x22001a28 = 63 fb ff eb`
- `0x22001a4c = 2a 06 00 eb`
- `0x22001a78 = 32 ff 2f e1`
- `0x22001ab0 = 28 17 00 ea`

Supporting active Nano 3G patches still present:

- readiness stub at `0x2200197c`
- loader callback stub at `0x220019ac`
- UART immediate-return patch at `0x22006558`
- `[service + 0x68]` bypass at `0x22001938`
- status emulation at `0x220019d0`

Run status:

- not run yet
- next expected classification:
  - `MARKER_1AB0_REACHED`
  - `MARKER_1AB0_NOT_REACHED`

## 2026-04-24 Single hardware run with the 0x22001ab0 tail marker

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active tail split:
  - `0x22001ab0`
  - `32 ff 2f e1` -> `28 17 00 ea`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
  - post-run `mks5lboot --dfuscan` showed:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1AB0_REACHED**

Recovery status:

- pending clean manual DFU reconfirmation

## 2026-04-24 Prepared late-gap branch/poll marker after `0x22001938` bypass

Prepared local Nano 3G-only WTF state for the next single hardware run:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
- active marker:
  - `0x220019c8`
  - `15 00 00 0a` -> `41 17 00 ea`
  - branch target:
    - `0x220076d4`
- marker stub:
  - `0x220076d4`
  - `0x22002138(11)`
  - loop forever

Late-gap logic under test:

- `0x220019c8`
  - direct branch when `0x38c00040 & 3 == 0`
- `0x220019cc..0x220019d4`
  - poll loop on `0x38c0000c & 1`

Verification from fresh local defanger output:

- `0x1938 = 64 17 00 eb`
- `0x19c8 = 41 17 00 ea`
- `0x76d0 = 1e ff 2f e1`
- `0x76d4 = 0b 00 a0 e3 ... 38 21 00 22`

Status:

- image prepared
- not run yet

## 2026-04-24 Single hardware run with post-poll marker at `0x220019d8`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active local state:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
  - post-poll marker at `0x220019d8`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_19D8_REACHED**

Recovery status:

- manual clean DFU re-entry not yet re-confirmed

## 2026-04-24 Verified deeper post-landing marker at `0x22001a4c`

Prepared local Nano 3G-only state for the next single hardware run:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- restore:
  - `0x22001a28 = 63 fb ff eb`
- active marker:
  - `0x22001a4c = 2b 17 00 ea`
- marker stub:
  - `0x22007700`

Raw-body verification:

- `0x1a24 = 00 40 a0 e3`
- `0x1a28 = 63 fb ff eb`
- `0x1a4c = 2b 17 00 ea`
- `0x1938 = 64 17 00 eb`
- `0x197c = 1f 15 00 eb`
- `0x19ac = c6 14 00 eb`
- `0x19d0 = 01 10 b0 e3`
- `0x6558 = 00 00 a0 e3 1e ff 2f e1`

Status:

- image prepared
- verified ready
- not run yet

## 2026-04-24 Single hardware run with deeper post-landing marker at `0x22001a4c`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active local state:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
  - deeper tail marker at `0x22001a4c`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1A4C_REACHED**

Recovery status:

- manual clean DFU re-entry not yet re-confirmed

## 2026-04-24 Verified post-landing marker at `0x22001a28`

Prepared local Nano 3G-only state for the next single hardware run:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- restore:
  - `0x22001a24 = 00 40 a0 e3`
- active marker:
  - `0x22001a28 = 33 17 00 ea`
- marker stub:
  - `0x220076fc`

Raw-body verification:

- `0x1a24 = 00 40 a0 e3`
- `0x1a28 = 33 17 00 ea`
- `0x1938 = 64 17 00 eb`
- `0x197c = 1f 15 00 eb`
- `0x19ac = c6 14 00 eb`
- `0x19d0 = 01 10 b0 e3`
- `0x6558 = 00 00 a0 e3 1e ff 2f e1`
- USB string now starts at `0x7710`

Status:

- image prepared
- verified ready
- not run yet

## 2026-04-24 Single hardware run with post-landing marker at `0x22001a28`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active local state:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
  - post-landing marker at `0x22001a28`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1A28_REACHED**

Recovery status:

- manual clean DFU re-entry not yet re-confirmed

## 2026-04-24 Verified landing-block marker at `0x22001a24`

Resolved the local helper verification mismatch by switching to the raw-body
dumper:

- `go run -a ./cmd/dump_n3g_defanged.go <in> <out-img1> <out-body>`
- inspect `<out-body>` directly with `xxd`

Verified local Nano 3G-only state:

- `0x220019d8`
  - `00 00 50 e3`
- `0x220019dc`
  - `10 00 00 1a`
- `0x22001a24`
  - `34 17 00 ea`
- still active:
  - `0x22001938 = 64 17 00 eb`
  - `0x2200197c = 1f 15 00 eb`
  - `0x220019ac = c6 14 00 eb`
  - `0x220019d0 = 01 10 b0 e3`
  - `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Status:

- image prepared
- verified ready
- not run yet

## 2026-04-24 Single hardware run with landing-block marker at `0x22001a24`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active local state:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
  - landing-block marker at `0x22001a24`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1A24_REACHED**

Recovery status:

- manual clean DFU re-entry not yet re-confirmed

## 2026-04-24 Single hardware run with final-tail marker at `0x220019dc`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active local state:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
  - final-tail marker at `0x220019dc`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_19DC_REACHED**

Recovery status:

- manual clean DFU re-entry not yet re-confirmed

## 2026-04-24 Single hardware run with AES/status completion emulation at `0x220019d0`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active local state:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`
- screen result:
  - black screen for the full 90-second window
  - no Apple logo
  - no backlight
  - no flicker
  - no patched text
  - no stock UI
  - no visible reset loop

Classification:

- **CHAINLOAD_BLACKSCREEN**

Recovery status:

- manual clean DFU re-entry not yet re-confirmed

## 2026-04-24 Prepared post-poll tail marker at `0x220019d8`

Prepared local Nano 3G-only WTF state for the next single hardware run:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- keep original:
  - `0x220019d4`
    - `fc ff ff 0a`
- active marker:
  - `0x220019d8`
  - `00 00 50 e3` -> `3d 17 00 ea`
  - branch target:
    - `0x220076d4`

Prepared but inactive:

- `0x220019dc` -> `0x220076e8`
- `0x22001a24` -> `0x220076fc`

Verification from fresh local-module defanger output:

- `0x19c8 = 15 00 00 0a`
- `0x19d0 = 01 10 b0 e3`
- `0x19d4 = fc ff ff 0a`
- `0x19d8 = 3d 17 00 ea`

Status:

- image prepared
- not run yet

## 2026-04-24 Single hardware run with late-gap poll-loop bypass at `0x220019d4`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active local state:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - late-gap poll-loop bypass at `0x220019d4`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`
- screen result:
  - black screen for the full 90-second window
  - no Apple logo
  - no backlight
  - no flicker
  - no patched text
  - no stock UI
  - no visible reset loop

Classification:

- **CHAINLOAD_BLACKSCREEN**

Recovery status:

- manual clean DFU re-entry not yet re-confirmed

## 2026-04-24 Prepared AES/status completion emulation at `0x220019d0`

Prepared local Nano 3G-only WTF state for the next single hardware run:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
- restore:
  - `0x220019c8`
    - `15 00 00 0a`
  - `0x220019d4`
    - `fc ff ff 0a`
- active completion emulation:
  - `0x220019d0`
  - `01 00 11 e3` -> `01 10 b0 e3`

Verification from fresh local-module defanger output:

- `0x19c8 = 15 00 00 0a`
- `0x19d0 = 01 10 b0 e3`
- `0x19d4 = fc ff ff 0a`
- `0x1938 = 64 17 00 eb`
- `0x197c = 1f 15 00 eb`
- `0x19ac = c6 14 00 eb`

Status:

- image prepared
- not run yet

## 2026-04-24 Single hardware run with late-gap marker at `0x220019c8`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active local state:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - late-gap marker at `0x220019c8`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_19C8_REACHED**

Recovery status:

- manual clean DFU re-entry not yet re-confirmed

## 2026-04-24 Prepared late-gap poll-loop bypass at `0x220019d4`

Prepared local Nano 3G-only WTF state for the next single hardware run:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
- restore:
  - `0x220019c8`
    - `15 00 00 0a`
- active bypass:
  - `0x220019d4`
  - `fc ff ff 0a` -> `00 00 a0 e1`

Verification from fresh local-module defanger output:

- `0x19c8 = 15 00 00 0a`
- `0x19d4 = 00 00 a0 e1`
- `0x1938 = 64 17 00 eb`
- `0x197c = 1f 15 00 eb`
- `0x19ac = c6 14 00 eb`

Status:

- image prepared
- not run yet

## 2026-04-24 Single hardware run with forward marker at 0x22001938

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active marker:
  - `0x22001938`
  - `31 ff 2f e1` -> `64 17 00 ea`
  - branches to:
    - `0x220076d0`
- marker stub:
  - calls `0x22002138(10)`
  - loops forever
- kept active:
  - readiness bypass at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1938_REACHED**

Recovery status:

- the device remained present as DFU on USB

## 2026-04-24 Static final-gap reduction after `0x22001938` marker

No hardware run in this step. This is a static reduction plus local patch
preparation only.

Conclusion:

- first unresolved call in the remaining gap:
  - `0x22001938: blx [service + 0x68]`
- later branches are not the first live unknown in the current build because:
  - `0x2200197c` is already replaced by the readiness stub
  - `0x22001994` is statically bypassed by `r5 = 0x1d`
  - `0x220019ac` is already replaced by the loader stub
- later unresolved candidates remain:
  - `0x220019c8`
  - `0x220019cc..0x220019d4`
  - but only after the `0x22001938` callback is bypassed or proven to return

Prepared bypass:

- `0x22001938`
  - `31 ff 2f e1` -> `64 17 00 eb`
- `0x220076d0`
  - `1e ff 2f e1`

Local verification:

- fresh clean-cache defanger output shows:
  - `0x1938 = 64 17 00 eb`
  - `0x76d0 = 1e ff 2f e1`

## 2026-04-24 Single hardware run with `[service + 0x68]` bypass at `0x22001938`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- active bypass:
  - `0x22001938`
  - `31 ff 2f e1` -> `64 17 00 eb`
  - local stub:
    - `0x220076d0`
    - `bx lr`
- kept active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Observed screen result:

- black screen for the full 90-second window
- no Apple logo
- no backlight
- no flicker
- no patched text
- no stock UI
- no visible reset loop

Classification:

- **CHAINLOAD_BLACKSCREEN**

Recovery status:

- separate recovery step not required; device remained on USB as DFU

## 2026-04-24 Single hardware run with middle-path marker at `0x22001768`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active marker:
  - `0x22001768`
  - `6f ff ff eb` -> `ec 15 00 ea`
  - branch to:
    - `0x22006f20`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
    - DFU mode
  - post-run `mks5lboot --dfuscan` failed with:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1768_REACHED**

## 2026-04-24 Single hardware run with middle-path marker at `0x2200177c`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active marker:
  - `0x2200177c`
  - `ff 12 00 eb` -> `ef 15 00 ea`
  - branch to:
    - `0x22006f40`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
    - DFU mode
  - post-run `mks5lboot --dfuscan` failed with:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_177C_REACHED**

## 2026-04-24 Single hardware run with middle-path marker at `0x22001788`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active marker:
  - `0x22001788`
  - `28 13 00 eb` -> `f4 15 00 ea`
  - branch to:
    - `0x22006f60`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
    - DFU mode
  - post-run `mks5lboot --dfuscan` failed with:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1788_REACHED**

## 2026-04-24 Single hardware run with post-`0x1788` marker at `0x22001798`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active marker:
  - `0x22001798`
  - `6e 13 00 eb` -> `f8 15 00 ea`
  - branch to:
    - `0x22006f80`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
    - DFU mode
  - post-run `mks5lboot --dfuscan` failed with:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1798_REACHED**

## 2026-04-24 Attempted hardware run with marker at `0x220017a0`

This attempt did not start from a clean usable DFU state, so it is not a valid
marker result.

Observed host behavior before/at launch:

- `lsusb` showed:
  - `05ac:1223`
  - DFU mode
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`
- `/tmp/wInd3x/wInd3x cfw run ...` failed before the exploit with:
  - `retrieving string descriptor`
  - `failed to get string descriptor 2`
  - `libusb: i/o error [code -1]`

Classification:

- **RUN_ABORTED_DIRTY_DFU**

## 2026-04-24 Single hardware run with marker at `0x220017a0`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active marker:
  - `0x220017a0`
  - `ed 08 00 eb` -> `fe 15 00 ea`
  - branch to:
    - `0x22006fa0`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
    - DFU mode
  - post-run `mks5lboot --dfuscan` failed with:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_17A0_REACHED**

## 2026-04-24 Single hardware run with marker at `0x2200181c`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active marker:
  - `0x2200181c`
  - `b6 06 00 eb` -> `e7 15 00 ea`
  - branch to:
    - `0x22006fc0`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
    - DFU mode
  - post-run `mks5lboot --dfuscan` failed with:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_181C_REACHED**

## 2026-04-24 Single hardware run with marker at `0x2200182c`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active marker:
  - `0x2200182c`
  - `31 ff 2f e1` -> `eb 15 00 ea`
  - branch to:
    - `0x22006fe0`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` still showed:
    - `05ac:1223`
    - DFU mode
  - post-run `mks5lboot --dfuscan` failed with:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_182C_REACHED**

## 2026-04-24 Middle-path marker prepared, not yet run

Reason for moving earlier:

- final-tail marker at `0x22001ae8` was not reached
- blocker remains earlier inside `0x22001698`

Active prepared marker:

- `0x22001758`
  - `30 ff 2f e1` -> `e8 15 00 ea`
  - branch to:
    - `0x22006f00`

Marker stub:

- `0x22006f00`
  - `mov r0, #2`
  - `blx 0x22002138`
  - `b .`

Inactive prepared follow-ups:

- `0x22001768 -> 0x22006f20`
- `0x2200177c -> 0x22006f40`
- `0x22001788 -> 0x22006f60`

Restored/kept:

- `0x22001ae8` restored to original
- readiness stub active
- loader stub active
- UART return patch active

Status:

- local `/tmp/wInd3x/wInd3x` rebuilt
- no hardware run performed yet

## 2026-04-24 Single hardware run with middle-path marker at `0x22001758`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active local WTF marker:
  - `0x22001758`
  - `30 ff 2f e1` -> `e8 15 00 ea`
  - branch to:
    - `0x22006f00`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed:
    - `05ac:1223`
    - DFU mode
  - repeated `mks5lboot --dfuscan` attempts failed to claim/configure the
    device:
    - `LIBUSB_ERROR_OTHER`

Classification:

- **MARKER_1758_REACHED**

Recovery status:

- device remained/re-entered DFU on USB without a separate manual reset

## 2026-04-24 Static correction of the WTF -> OSOS transfer path

No hardware run in this step.

Correction:

- the execute path is in the WTF body, not the OSOS body

Recovered static path:

- `0x22002fe4`
  - enters `0x22001698` with:
    - `r0 = 0x1d`
    - `r1 = 0x08000000`
    - `r2 = 0x00f80000`
- final gate:
  - `0x22001b04: cmp r4, #0`
  - `0x22001b0c: tst r5, #16`
  - `0x22001b10: addne r0, r6, r7`
  - `0x22001b14: blne 0x220024b8`
- execute wrapper:
  - `0x220024bc: mov r4, r0`
  - `0x220024c4: bl 0x22002138`
  - `0x220024c8: blx r4`

Intended OSOS target:

- `0x08000800`

Current reading:

- register state, stack, and ARM/Thumb mode at the intended transfer are not
  the primary suspicion
- the strongest failure class remains:
  - `EXECUTE_NEVER_CALLED`

Status:

- execute path mapped
- no force-jump patch prepared

## 2026-04-24 Final-gate marker prepared in current defanged WTF

No hardware run in this step.

Key reduction:

- in the current local defanger, `0x22006558` already returns immediately
- therefore the last unresolved call before the final gate is now:
  - `0x22001ae8 -> 0x22002724`

Prepared marker:

- callsite:
  - `0x22001ae8`
  - `0d 03 00 eb` -> `94 14 00 ea`
- stub:
  - `0x22006d40`
  - `mov r0, #4`
  - `blx 0x22002138`
  - `b .`

Reason:

- `r4` and `r5` already look favorable for the final execute gate
- the unresolved question is whether control reaches the tail immediately before
  `0x22001b04`

Status:

- local `/tmp/wInd3x/wInd3x` rebuilt
- no `cfw` run performed yet

Classification:

- **FINAL_GATE_MARKER_PREPARED**

## 2026-04-24 Single hardware run with final-tail marker at `0x22001ae8`

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- active local WTF marker:
  - `0x22001ae8`
  - `0d 03 00 eb` -> `94 14 00 ea`
  - branch to:
    - `0x22006d40`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **MARKER_1AE8_NOT_REACHED**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 OSOS entry-loop probe prepared, not yet run

Reason for probe change:

- scheduler/main-loop watchdog probe did not trigger any reset behavior
- next proof should avoid display, audio, and watchdog dependence

Corrected entry mapping:

- `0x22000800` is not the reset entry
- startup shell is:
  - `0x22000000`
  - `0x22008808`
  - `0x22008814`
  - `0x220039c4`

Prepared image:

- `/tmp/n3g-osos-work/n3g-osos-entry-loop-probe.dfu`

Prepared patch:

- `0x22008814`
  - `6a ec ff ea` -> `fe ff ff ea`
  - original effect:
    - branch to `0x220039c4`
  - probe effect:
    - self-loop before the full startup body

Other recovered runtime candidates for later proof work:

- recurring callback setup:
  - `0x22003ac4 -> 0x22003af4 -> 0x22001d48`
- RTXC-style dispatch wrapper:
  - `0x22003b28 -> 0x2200360c`

Expected meaning:

- changed behavior versus the current post-handoff baseline would strongly
  support entry execution
- unchanged behavior would remain ambiguous, because USB disappearance may
  happen before this branch point

Status:

- image prepared
- not run yet

## 2026-04-24 Single hardware run with OSOS entry-loop probe

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-entry-loop-probe.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present

Classification:

- **ENTRY_LOOP_NOT_REACHED**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 Scheduler / main-loop probe prepared, not yet run

Reason for changing strategy:

- readiness-stub chainload now escapes the old WTF-stuck failure
- USB disappearance suggests post-WTF progress
- display and later Apple-owned beep hooks still produced no visible or audible
  proof
- next step should prove post-handoff execution without relying on display or
  audio

Recovered hook point:

- `0x22003af8`
- this is the first stable post-init loop reached from the recovered startup
  path:
  - `0x220039c4`
  - `0x22003aa4: b 0x22003af8`

Why this point was chosen:

- it is always reached if OSOS makes it through the recovered startup path
- it is not conditional on UI visibility, connected screen, backlight, or later
  audio services
- it is lower risk than patching later scheduler tasks before reachability is
  proven

Prepared probe image:

- `/tmp/n3g-osos-work/n3g-osos-scheduler-probe.dfu`

Prepared patch:

- hook site:
  - `0x22003af8`
  - `00 00 a0 e1` -> `3c 14 00 ea`
  - branch to:
    - `0x22008bf0`
- local stub:
  - `mov r1, #0x3c800000`
  - `mov r0, #0x00100000`
  - `str r0, [r1]`
  - `b .`

Expected meaning on hardware:

- if the Nano reaches the first stable OSOS loop, it should now take the known
  watchdog-reset path instead of silently idling black
- if no reset behavior change appears, the failure is earlier than the recovered
  idle shell

Status:

- image prepared
- not run yet

## 2026-04-24 Single hardware run with scheduler / main-loop probe

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-scheduler-probe.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - black screen for the full window
  - no immediate reboot
  - no delayed reboot
  - no repeated reboot loop
  - no Apple logo
  - no flicker
  - no backlight
  - no audible sound

Classification:

- **SCHEDULER_PROBE_NOT_TRIGGERED**

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-25 Single hardware run with corrected late display-gate callbacks patch

- pre-run DFU was clean:
  - `05ac:1223`
  - DFU state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-display-gate-callbacks-n3g.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano at all
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- screen result:
  - pending user observation

Classification:

- **TARGETED_HW_PATCH_POST_ESCAPE_SIGNATURE**

Recovery status:

- pending manual DFU recovery confirmation

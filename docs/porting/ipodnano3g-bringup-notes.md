# iPod Nano 3G Bring-up Notes

## Current Safe Boundary

- Simulator support and safe-mode hardware scaffolding are already in-tree.
- `firmware/export/config/ipodnano3g.h` defines `NAN03G_SAFE_BRINGUP 1`.
- `firmware/target/arm/s5l8702/ipodnano3g/bringup-nano3g.c` provides an early boot trace ring plus a non-returning failsafe halt path.
- `docs/porting/ipodnano3g-hardware-bringup.md` classifies LCD write paths, PMU writes, USB PHY writes, audio hardware writes, and NAND writes as blocked or stubbed during safe bring-up.

## Hardware Bring-up Priorities

1. Reliable DFU detection on the host.
2. Reliable exploit / payload handoff without losing recovery.
3. Observable early execution traces.
4. LCD response only after execution is proven and evidence supports the next step.

## Known Constraints

- Do not write NAND.
- Do not disable safe bring-up casually.
- Do not add guessed controller register writes.
- Prefer improving observability over widening hardware access.

## Open Questions For This Session

- Is the Nano 3G currently visible to the host in normal USB mode, recovery mode, or DFU mode?
- Is a validated exploit tool already installed locally, or does this session need to use `mks5lboot`/repo-native tooling only?
- Is there an existing native `build-native-ipodnano3g/` artifact set ready to package, or must it be rebuilt from source before any payload attempt?

## Answers Established So Far

- The host sees a real Nano 3G as `05ac:1262 Apple, Inc. iPod Nano 3.Gen`, which indicates the device is connected but not currently in DFU mode.
- `wInd3x` is not currently in PATH and `pyusb` is not installed, so the most concrete local tool path is the repo-native `utils/mks5lboot`.
- `mks5lboot` builds successfully on this host and can talk to libusb when run outside the sandbox.
- A direct `mks5lboot --dfuscan` outside the sandbox reports no DFU device, so the next gating action is physical entry into DFU mode.
- Only `build-sim-ipodnano3g/` exists at the moment; a native Nano 3G build directory will still need to be created before Phase 2 payload testing.

## Additional Facts Established In This Session

- Native Nano 3G artifacts now exist again:
  - `build-native-ipodnano3g/rockbox.ipod`
  - `build-bootloader-ipodnano3g/bootloader-ipodnano3g.ipod`
- `mks5lboot --mkdfu-inst` is not Nano 3G-ready in this tree because `utils/mks5lboot/mkdfu.c` only recognizes the Classic 6G model table and rejects the Nano 3G `nn3g` model tag.
- The bootloader build exposed and is now fixed by a host-safe compile guard in `firmware/powermgmt.c`; this was a build issue, not a hardware-facing change.
- Official upstream `wInd3x` is now built locally at `/tmp/wInd3x/wInd3x`.
- Upstream `wInd3x` confirms a Nano 3G-safe exploit primitive:
  - `haxdfu` is temporary and does not persist across reboot.
  - `run` can upload a DFU image after `haxdfu`.
  - The built `makedfu --help` text is stale, but the actual source and host-side verification confirm that `makedfu -k n3g` works.

## Nano 3G Payload Format Decision

- Nano 3G after successful `haxdfu` does not need a different USB transfer command. It still uses ordinary DFU `DNLOAD` / `GETSTATUS` / manifest sequencing.
- The payload should not be sent as a naked raw blob.
- The safe wrapper path is upstream `wInd3x` IMG1 generation for `n3g`, not the older `mks5lboot` installer logic.
- Current upstream `wInd3x` wraps Nano 3G payloads as:
  - magic `8702`
  - version `1.0`
  - format byte `0x02` (`SIGNED`)
  - `0x800` body offset
  - no X509 body signature or certificate bundle
- Nano 3G then uses DFU protocol v1, so the transfer layer appends the CRC32 trailer during send.

## Practical Send Path

- If the payload is already a valid Nano 3G IMG1/DFU image, use `wInd3x run <image>`.
- If the payload is a flat ARM binary linked for load address `0x22000000` and entrypoint offset `0`, `wInd3x run <binary>` is sufficient because it auto-wraps the file into the correct Nano 3G IMG1.
- If the payload entrypoint is not at file offset `0`, explicitly wrap it first with:
  - `/tmp/wInd3x/wInd3x makedfu <input.bin> <output.dfu> -k n3g -e <offset>`
  - then `/tmp/wInd3x/wInd3x run <output.dfu>`

## Current Bring-up Boundary

- Safe to continue:
  - enter DFU physically
  - validate `wInd3x haxdfu`
  - verify that recovery still works after temporary haxed DFU
- Not yet justified:
  - forcing a Classic 6G `mks5lboot` installer path on Nano 3G
  - inventing a Nano 3G raw DFU payload wrapper
  - any NOR/NAND write path

## Next Safest Step

1. Put the Nano 3G into DFU mode with the documented button sequence.
2. Run `/tmp/wInd3x/wInd3x haxdfu`.
3. Confirm the device stays recoverable and classify the resulting USB state.
4. Only then choose a Nano 3G-safe payload execution path backed by real format/tool evidence.

## Minimal Execution-Proof Payload

- A standalone proof payload now exists under `tools/ipodnano3g/minimal_payload/`.
- It is intentionally smaller and safer than the Rockbox bootloader:
  - pure ARM assembly
  - no C runtime
  - no stack setup
  - no MMIO writes
  - no NAND, PMU, USB PHY, or LCD access
- Behavior:
  - load the address of an in-image `heartbeat` word
  - increment a register forever
  - store the counter back to that RAM word
  - loop forever

## Built Artifacts

- `tools/ipodnano3g/minimal_payload/minimal-n3g.bin`
- `tools/ipodnano3g/minimal_payload/minimal-n3g.elf`
- `tools/ipodnano3g/minimal_payload/minimal-n3g.map`

## Verification Results

- Link/load address: `0x22000000`
- Entry point: `0x22000000`
- Payload size: `28` bytes
- In-image heartbeat address: `0x22000018`
- Undefined symbols: none
- Instruction audit:
  - one `ldr` of the heartbeat pointer literal
  - one `mov`
  - one `add`
  - one `str` back to `0x22000018`
  - one backward branch
- The only write performed by the payload is to its own loaded RAM image, which keeps this step aligned with the “execution proof first, no hardware side effects” goal.

## Safe Send Command

- Once the Nano 3G is in DFU, the intended command is:
  - `/tmp/wInd3x/wInd3x run /home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/minimal-n3g.bin`
- This should:
  - enter haxed DFU if needed
  - wrap the flat binary into the current upstream Nano 3G IMG1 format
  - transfer it with the normal Nano 3G DFU v1 path

## Next Safest Observation Goal

- Observe whether the device leaves DFU and appears to hang in the infinite loop without immediately resetting.
- Immediately after the send, compare `lsusb` state:
  - before send
  - after send
  - after forced reset / DFU re-entry
- Do not move to LCD or bootloader testing until this result and recovery are both classified.

## Minimal Payload Execution Attempt Result

- DFU entry was confirmed in practice:
  - `05ac:1223 Apple, Inc. iPod Classic/Nano 3.Gen (DFU mode)`
  - `mks5lboot --dfuscan --loop` repeatedly reported `DFU device state: 2`
- The actual send path succeeded through upstream `wInd3x`:
  - `Generating payload...`
  - `Running rce....`
  - `Haxed DFU running!`
  - `Given firmware file is not IMG1, packing into one...`
  - `Got dfuMANIFEST, image uploaded.`
  - `Image sent.`

## Current Interpretation

- What is confirmed:
  - DFU detection is reliable.
  - `haxdfu` plus Nano 3G IMG1 wrapping plus DFU upload works on the real device.
  - The minimal payload itself is RAM-only and does not contain risky hardware writes.
- What is not yet confirmed:
  - Whether the Nano actually left DFU and executed the infinite loop.
- Host-side ambiguity after send:
  - `lsusb` still showed `05ac:1223` several seconds later instead of a clear disconnect or different mode.
  - A one-shot post-send `mks5lboot --dfuscan` did not classify the state reliably because it hit `LIBUSB_ERROR_OTHER`.

## Safe Classification

- Payload transfer: confirmed.
- Controlled on-device execution: inconclusive.
- Recovery status: likely intact, but manual reset and DFU re-entry still need to be confirmed before advancing.

## Next Safest Step

1. Force-reset the Nano manually.
2. Confirm whether it boots or changes state visibly.
3. Re-enter DFU and verify `05ac:1223` again.
4. Only after recovery is confirmed decide whether the next iteration should add a different non-MMIO execution signature.

## Deliberate-Fault Payload Attempt

- The minimal payload was changed from a silent RAM loop to a deliberate invalid access:
  - `mvn r0, #0`
  - `mov r1, #0`
  - `strb r1, [r0]`
  - `b` back to the store as fallback
- This preserves the same safety boundary:
  - no NAND
  - no LCD
  - no PMU
  - no USB stack
  - no guessed MMIO

## Deliberate-Fault Artifact Summary

- `tools/ipodnano3g/minimal_payload/minimal-n3g.bin`
- Entry point: `0x22000000`
- Flat binary size: `16` bytes
- Undefined symbols: none
- Only intended write: byte store to `0xffffffff`

## Deliberate-Fault Run Result

- `wInd3x run` again completed successfully and packed the flat binary into Nano 3G IMG1.
- Before send: `lsusb` showed `05ac:1223`.
- Immediately after send: `lsusb` still showed `05ac:1223`.
- Several seconds later: `lsusb` still showed `05ac:1223`.
- One-shot `mks5lboot --dfuscan` remained unreliable for post-send classification because it failed with `LIBUSB_ERROR_OTHER`.

## Current Classification

- Transfer path: confirmed working.
- Observable execution signature from the deliberate fault: not confirmed.
- Best classification for this run: `STILL INCONCLUSIVE`.
- Reason:
- the host did not observe a disconnect, reset, or re-enumeration difference relative to DFU
- the test therefore does not yet prove that the payload executed, even though the upload path is solid

## Piezo Proof Candidate

- A minimal piezo proof payload is now prepared:
  - `tools/ipodnano3g/minimal_payload/piezo-beep-n3g.bin`
- It intentionally uses the simpler bootloader `piezo_tone()` style from
  `piezo-nano3g.c`, not the wider timer/PWM interrupt path.

### Exact Hardware Touches

- `PCON0 @ 0x3cf00000`
  - enable phase:
    - `(PCON0 & ~0xff000000) | 0x53000000`
  - disable phase:
    - `(PCON0 & ~0xff000000) | 0xee000000`
- `GPIOCMD @ 0x3cf00200`
  - repeated alternating writes:
    - `0x0000060e`
    - `0x0000060f`
- `USEC_TIMER @ 0x3c7000b4`
  - read-only delay basis

### Expected Behavior

- one short beep
- about `100 ms`
- around `2 kHz`

### Safety Notes

- no LCD
- no backlight
- no PMU
- no NAND or storage
- no USB PHY
- no full audio/codec path

### Evidence Level

- strong source backing from Nano 3G target code and bootloader usage
- weaker Apple firmware corroboration than the LCD work
- acceptable as a proof-of-execution channel, but not as strong as a fully
  Apple-reduced raw MMIO sequence

## Early System-Init Candidate

- A new early-system probe is prepared:
  - `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.bin`

Chosen entry:

- OSOS `0x22002770`

Why this wrapper:

- it is the cleanest shared pre-peripheral init block found so far
- it is narrower than the full reset entry chain
- it avoids LCD and audio-local logic directly
- it is more plausible as a prerequisite for both piezo and LCD than any
  remaining subsystem-only path

Visible hardware state it touches indirectly/directly:

- CP15 control register bits
- `0x38100000` (`MIU_BASE`)
- `0x3c500000` (`CLK_BASE`)
- `0x39900000` (`DMA1_BASE` in local headers)

Current limitation:

- the wrapper still contains unresolved OSOS imports at:
  - `0x22003414 -> 0x08016234`
  - `0x220034bc -> 0x0801542c`

So this is a bounded Apple call-level probe, not a fully raw write-only
reduction.

## Runtime Service Table Status

- The full LCD cold-init blocker is now narrowed to the external callback object
  rooted at `0x22007398`.
- Static WTF body state:
  - `0x22007398 = 0`
  - `0x2200739c = 0`
  - `0x220073a0 = 0`
- The only confirmed wrapper uses are:
  - `0x22003ce0` -> slot `+0x04`
  - `0x22003d14` -> slot `+0x08`
  - `0x22003c98` -> slot `+0x28`
  - `0x22003cc0` -> slot `+0x2c`

## WTF Runtime Entry Findings

- Actual WTF entry is `0x22001420`.
- Early startup immediately:
  - calls local setup helpers
  - switches CPU modes and installs IRQ/SVC stacks
  - copies/zeros in-image state
  - calls `0x22002f20`
  - then issues `svc 0x00123456`
- Important negative evidence:
  - the startup path does **not** copy incoming `r0`/`r1`/`r2`/`r3` into
    `0x22007398`
  - there is no current evidence that the service table is passed as a plain
    bootrom register argument at entry

## Current Theory

- `0x22007398` is most likely supplied by an external runtime/loader service
  layer, possibly tied to the WTF `svc 0x00123456` ABI.
- This is stronger than the older “maybe bootrom passed a pointer in a
  register” theory.
- Exact provider is still not proven:
  - bootrom runtime
  - DFU/runtime shim
  - loader-installed service object

## LCD Bypass Decision

- Later direct LCD steps are real and already reduced:
  - gate/resource wrapper
  - controller preamble
  - strap-based command mode
  - panel-group helper dispatch
  - Apple panel tables
- But skipping the callbacks is still not evidence-backed.
- Current decision: **bypass unsafe / still blocked**.

## Runtime Stub Decision

- The wrapper ABI is now reduced enough to prepare a minimal no-op service table
  for `0x22007398`.
- Minimum required slots:
  - `+0x04`
  - `+0x08`
  - `+0x28`
  - `+0x2c`
- Minimum safe callback behavior:
  - return `0`
  - no MMIO
  - no side effects other than allowing the wrappers to return through a valid
    function pointer

## Important Limit

- This only reconstructs the service-table control-flow ABI.
- It does **not** automatically make the Apple LCD path callable from a tiny
  standalone payload, because the WTF routines at `0x2200455c`, `0x220048bc`,
  and related addresses are offsets inside the decrypted WTF body, not fixed ROM
  services.
- So the current prepared work splits into two layers:
  1. non-hardware runtime-service stub bootstrap
  2. future translated/copied LCD code that would consume it

## Current Prepared Artifact Status

- Safe to prepare:
  - a host-built payload that only installs the stub table at `0x22007398`
    and loops
- Not yet justified to run:
  - a wider LCD payload that assumes the no-op stubs fully replace the original
    runtime service semantics

Prepared stub artifact:

- `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.bin`
- `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.elf`
- `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.map`

Verified host-side facts:

- entry point `0x22000000`
- text size `88` bytes
- only intended write: store payload-local table pointer to `0x22007398`
- callback slots `+0x04`, `+0x08`, `+0x28`, `+0x2c` all point to the same
  `return 0` stub

## Prepared Embedded LCD Payload

- A self-contained Apple-backed LCD payload is now built host-side:
  - `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.bin`
  - `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.elf`
  - `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.map`
- It embeds the required Apple code/data slices at their original VMAs instead
  of fully rewriting the LCD path.
- It installs the reduced runtime service table at `0x22007398`.
- It then calls:
  - the Apple wrapper sequence
  - `0x2200455c()`
  - `0x220048bc(1, 0)`
  - `0x220048bc(4, 0)`
- It loops forever afterward.

## Current Safety Classification

- Build-time self-containment: achieved
- Hardware run approval: not yet exercised in this step
- Why still cautious:
  - the payload is Apple-backed and link-closed
  - but the `OP1` LCD side path can touch a larger MMIO fan-out than the
    earlier awake-only payload

## Proposed Run Command

- `/tmp/wInd3x/wInd3x run /home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.bin`

Do not execute it until the current prepared-only state is reviewed.

## Single `lcd-fullinit` Run Result

- The payload was run once on real hardware.
- Before send:
  - `05ac:1223`
  - `mks5lboot --dfuscan` state `2`
- After send:
  - `lsusb` still showed `05ac:1223`
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Visible observation:
  - screen stayed black for the full 15-second watch window
  - no flicker
  - no backlight change
  - no Apple logo
  - no visible reset
- Recovery:
  - force reset succeeded
  - DFU re-entry succeeded
  - `mks5lboot --dfuscan` returned to state `2`

## Current Classification

- `EXECUTION ONLY`

Interpretation:

- the larger Apple-backed LCD payload still appears to take over DFU execution
- but it did not produce the first visible LCD/backlight milestone
- recovery remains intact, so the current safe boundary is preserved

## Next Safe Boundary

- Stop after documentation.
- Do not run another hardware payload until the no-visible-output result is
  reviewed against the Apple LCD path and its wider MMIO fan-out.

## Next Safest Analysis Step

1. Keep tracing where the runtime environment installs or exposes the service
   object behind `0x22007398`.
2. Reduce the role of each callback slot relative to the LCD cold-init path.
3. Do not build or run a wider LCD payload until those callbacks are either
   resolved or explicitly proven unnecessary.

## Firmware-Assisted LCD Reduction

- Decrypted WTF triage now reduces Apple’s early LCD command-mode choice to a
  runtime GPIO strap rule instead of a guessed panel mode.
- The selector path is:
  - cached selector at `0x2200700c`
  - if value is `4`, read GPIO52 / GPIO53 via `0x22004f58`
  - compute `selector = gpio52 | (gpio53 << 1)`
- Apple command-mode grouping is now evidence-backed:
  - selector `0` / `1` -> low bits `0x0c20`
  - selector `2` / `3` -> low bits `0x0da8`
- With the Apple preamble value `0x80100db1`, the exact runtime config writes
  become:
  - `0x80000c21`
  - `0x80000da9`

## First Visible Candidate

- Best visible candidate is now the Apple-backed LCD awake path, not PMU
  backlight.
- Apple-backed minimal sequence:
  - controller preamble at `0x45bc`
  - strap-selected command-mode write from `0x42c8`
  - awake sequence from body `0x70d1`:
    - `0x11`
    - delay token `0x3c`
    - `0x13`
    - `0x29`
- The delay helpers are also Apple-backed:
  - timer counter at `0x3c7000b4`
  - `0x8c4` for the 1-tick command-mode delay
  - `0x884` for the `0x3c -> 60,000 ticks` awake delay

## Prepared-Only LCD Payload

- Prepared, not executed:
  - `tools/ipodnano3g/minimal_payload/lcd-awake-n3g.S`
  - `tools/ipodnano3g/minimal_payload/lcd-awake-n3g.bin`
  - `tools/ipodnano3g/minimal_payload/lcd-awake-n3g.elf`
  - `tools/ipodnano3g/minimal_payload/lcd-awake-n3g.map`
- Behavior:
  - apply Apple LCD preamble
  - read GPIO52 / GPIO53 and choose the Apple-backed mode family at runtime
  - send `0x11`, delay, `0x13`, `0x29`
  - loop forever
- Explicitly still avoided:
  - NAND
  - PMU / backlight
  - USB PHY
  - audio
  - storage

## Current Safe Boundary

- A first LCD-awake payload now exists without a guessed mode.
- It has not been run yet.
- Recovery remains the last confirmed hardware boundary.

## Full LCD Init Analysis

- The awake-only payload is now known to be incomplete.
- Apple’s LCD path includes a larger init chain before the short awake tail.

### Directly confirmed pieces

- earliest LCD entry path reaches:
  - `0x22003ce0(1)`
  - `0x22003ce0(4)`
  - `0x22003d14(0x22007338)`
  - `0x22003cc0()`
  - `0x2200455c()`
- `0x2200455c()` itself only performs:
  - a gate/resource wrapper through `0x2200428c` / `0x22000664`
  - the LCD controller preamble at `0x220045bc`
  - panel-group table caching
- panel-specific init is dispatched later through `0x220048bc`

### Apple panel-init evidence

- selector group `1` has a larger mode-1 init table at body `0x7018`
- selector groups `2/3` have a large mode-4 16-bit init table at body `0x70dc`
- selector group `0` uses a shorter mode-4 table at body `0x728b`
- the short awake tail at `0x70d1` is only one of several panel-group tables

### Remaining blocker

- the runtime object behind `0x22007398` is still unresolved in raw MMIO terms
- because of that, the full cold-init chain is not yet reduced enough to build
  a non-speculative “full LCD init” payload

## Next Safest Step

1. Decide whether to perform the first Apple-backed LCD MMIO test.
2. If yes:
   - confirm DFU with `./mks5lboot --dfuscan`
   - run the prepared LCD-awake payload once
   - observe only the display response
   - immediately force reset and re-confirm DFU recovery
3. If no:
   - continue searching the decrypted WTF for an explicit Apple LEDCTL
     backlight write.

## Immediate Next Requirement

- Manually force-reset the device and re-enter DFU so post-test recovery is confirmed for this exact run.

## Execution Trigger Investigation

- `wInd3x run` on Nano 3G does not send any explicit post-upload "execute" command.
- The intended flow is:
  - enter DFU
  - run `haxdfu`
  - upload IMG1 over normal DFU
  - bootrom reaches manifest and continues automatically into image handling
- Nano 3G haxed DFU works by patching the bootrom `OnImage` callback, not by installing a second USB-visible loader stage.

## What The Nano 3G Hook Actually Changes

- The Nano 3G `OnImage` hook in `wind3x_n3g.go`:
  - calls `DFUBoot::CopyHeaderBody`
  - sets `dfu_done = 1`
  - forces image version `"1.0"`
  - forces runtime `entrypoint = 0`
- That last point matters:
  - the IMG1 header does contain an entrypoint field
  - `makedfu -e <offset>` does serialize it correctly
  - but the current Nano 3G runtime hook ignores non-zero header entrypoints and overwrites the runtime entry offset with `0`

## Correctness Conclusions

- No additional host-side trigger is required after DFU manifest.
- Load address `0x22000000` remains the correct assumption for haxed-DFU flat binaries.
- Our existing minimal payloads already start at offset `0`, so the hardcoded `entrypoint = 0` is not the reason they lacked a visible reset.
- The more likely mistake was in interpretation:
  - `lsusb` showing `05ac:1223` does not prove the bootrom is still actively servicing DFU
  - a tiny payload can take over execution while the USB device still appears enumerated until reset
  - post-send `mks5lboot --dfuscan` failures are a more meaningful signal than passive USB presence

## Classic 6G Comparison

- This checkout does not include a separate Classic 6G exploit implementation file.
- The upstream README groups Nano 3G and Classic together for the same exploit family:
  - both use direct code execution from the USB bug
  - both rely on overriding an `OnImage` function pointer rather than the Nano 4G/5G vtable-hook method
- So the relevant difference is mainly versus Nano 4G/5G:
  - Nano 3G/Classic: `OnImage` hook inside bootrom DFU path
  - Nano 4G/5G: verification/vtable patching, and later CFW flows often use WTF transitions

## Next Safe Test

- Do not change the load address or invent a new transfer step.
- Keep using:
  - `wInd3x run <flat-binary>`
- Improve the proof method:
  - after upload, actively probe DFU responsiveness instead of only checking `lsusb`
- If we specifically want to test Nano 3G entrypoint semantics, use:
  - `wInd3x makedfu <bin> <img> -k n3g -e <nonzero>`
  - where the observable behavior is placed only at that non-zero offset
  - this will show whether the Nano 3G hook’s forced runtime `entrypoint = 0` is suppressing non-zero entry offsets

## DFU Liveness Confirmation Result

- This was tested directly with the current minimal payload, without changing the payload format.
- Before upload:
  - `lsusb` showed `05ac:1223`
  - `./mks5lboot --dfuscan` successfully reported normal DFU state `2`
- After upload:
  - `lsusb` still showed `05ac:1223`
  - `./mks5lboot --dfuscan` immediately failed with `LIBUSB_ERROR_OTHER`
  - the same `LIBUSB_ERROR_OTHER` result persisted on a delayed re-check

## Updated Classification

- `LIKELY EXECUTION / DFU TAKEOVER`

Reason:
- passive USB enumeration remained present
- active DFU liveness disappeared only after payload upload
- therefore the device is very likely no longer in ordinary bootrom DFU even though it still presents the same PID on the bus

## What This Means

- The earlier conclusion "payload is accepted but not executed" was too pessimistic.
- We now have stronger evidence that the Nano 3G is accepting the IMG1 and leaving normal DFU responsiveness after transfer.
- That is the first real execution milestone on hardware, even though we still do not have visible LCD or a human-visible reset signature.

## Next Safe Step

- Do not jump to LCD driver guesses yet.
- The next safe iteration should improve observability of this executing state without using guessed hardware registers.
- Candidate directions:
  - an entrypoint-offset control test to validate Nano 3G entry semantics explicitly
  - a payload that intentionally preserves or breaks DFU liveness in a more distinguishable way
  - firmware-assisted research to ground the next visible-output step

## Timed-Reset Payload Result

- A new payload under `tools/ipodnano3g/timed_reset_payload/` was built to:
  - busy-wait in registers only
  - then trigger the same deliberate invalid write
- The delay window was increased and re-tested with an explicit countdown.

Observed result:
- User-visible screen behavior: black screen throughout, no Apple logo, no visible reset
- Host-visible behavior after upload:
  - `lsusb` still showed `05ac:1223`
  - `./mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`

## Updated Status

- Visible delayed reset: not confirmed
- DFU takeover after payload upload: confirmed again by liveness loss
- Current overall status:
  - `LIKELY EXECUTION / DFU TAKEOVER`
  - not yet `EXECUTION CONFIRMED` by visible device behavior

## Safest Interpretation

- The payload is very likely executing enough to take the device out of ordinary DFU responsiveness.
- The deliberate fault path does not currently produce a user-visible reset indication.
- That means the next step should improve observability, not jump to risky hardware enablement.

## MMIO-Free Observability Comparison

- Three new payloads were tested without touching hardware registers:
  - Payload A: immediate infinite loop
  - Payload B: delay loop, then infinite loop
  - Payload C: RAM-only mutation loop inside the payload image

## Result Across A / B / C

- For all three payloads:
  - before upload: `./mks5lboot --dfuscan` reported normal DFU state `2`
  - after upload: `lsusb` still showed `05ac:1223`
  - after upload: `./mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
  - delayed re-checks still failed the same way

## Updated Classification

- Differential timing proof: **not achieved**
- Best classification for this specific A/B/C comparison: **still inconclusive**

Reason:
- if execution timing were visible through DFU liveness, Payload B should have differed from Payload A
- if RAM-only mutation behaved differently from a pure branch loop, Payload C should also have differed
- none of those differences were observed

## What This Does And Does Not Mean

- It does **not** disprove execution.
- It does show that the current host-visible takeover signal is too coarse to distinguish among these three CPU-only payloads.
- So the standing status remains:
  - `LIKELY EXECUTION / DFU TAKEOVER`
  - but not yet `EXECUTION CONFIRMED` by differential timing or visible output

## Entrypoint-Offset Control Result

- A control image was built with:
  - offset `0x0`: immediate infinite loop
  - offset `0x20`: delayed-fault routine
  - deterministic zero padding in between
- It was explicitly wrapped with:
  - `wInd3x makedfu ... -k n3g -e 0x20`
- Header verification confirmed the IMG1 entry field was really set to `0x20`.

Observed behavior:
- Before send: normal DFU state `2`
- After send:
  - `lsusb` still showed `05ac:1223`
  - `./mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Delayed re-check: same result

## Interpretation

- The wrapped `-e 0x20` image behaved like the offset-0 baseline, not like the alternate routine at body offset `0x20`.
- This strongly supports the source-level conclusion that Nano 3G haxed DFU forces runtime entry offset `0` and ignores the IMG1 header entry field.

## Updated Status

- Runtime entry semantics: **likely confirmed as forced offset 0**
- DFU takeover after upload: still consistent
- Visible execution: still not confirmed

## Safest Next Step

- Do not proceed to LCD guesses.
- With entry-offset behavior now constrained, the next safe step is firmware-assisted research for a visibility mechanism grounded in real firmware evidence.

## Firmware-Assisted Visibility Research Result

- A dedicated research note now exists at:
  - `docs/porting/ipodnano3g-visibility-research.md`
- Sources reviewed for the first safe visibility signal:
  - Nano 3G backlight, PMU, LCD, and piezo target code
  - shared S5L8702 LCD support
  - existing Ghidra triage notes/scripts
  - relevant `wInd3x` download/decrypt source

## What The Evidence Supports

- Best currently identifiable visible candidate:
  - PMU backlight enable through `D1671_REG_LEDCTL`
- Why it is not yet safe enough to test:
  - no decrypted Nano 3G Apple firmware is present locally to confirm the early-boot PMU backlight path
  - a tiny DFU payload would still need `i2c-s5l8702.c` setup, which includes several raw clock/pinmux/controller writes and multiple `TBC` notes
- LCD is less justified than backlight right now:
  - Nano 3G panel tables exist in-tree
  - but the shared controller/interface layer still contains multiple `TBC` notes for Nano 3G mode details

## Current Safe Decision

- Safe visible MMIO action found right now: **no**
- Best visible candidate once Apple firmware evidence is obtained: **PMU backlight enable**
- Best externally observable non-visible candidate: **piezo tone**
- Current stop point:
  - do not test backlight
  - do not test LCD
  - do not widen hardware writes beyond the already-confirmed DFU takeover path

## Next Safe Workflow

1. Continue tracing the Apple panel-type / command-mode selection path in the
   decrypted WTF:
   - `0x4438`
   - `0x4500`
   - `0x455c`
   - `0x47e4`
   - `0x4838`
2. Reduce that path to one exact Apple-backed command-mode choice for the
   connected panel.
3. Only then build a one-action LCD awake payload.

## Decryption Status Update

- Superseded by the successful decryption result below.

## Current Blocker

- Superseded by the decrypted WTF findings below.

## Current Stop Point

- This older stop point is superseded.
- Current stop point is now:
  - decrypted firmware is available
  - Apple PMU and LCD paths are partially reduced
  - no hardware-writing payload is justified until the LCD command-mode choice or
    an explicit Apple LEDCTL backlight write is fully traced

## Decrypted WTF Status

- The upstream Nano 3G WTF was downloaded successfully to:
  - `/tmp/n3g-wtf-upstream.dfu`
- Device-assisted decryption completed successfully to:
  - `/tmp/n3g-wtf-decrypted.dfu`
- The IMG1 body was extracted for host-side analysis at:
  - `/tmp/n3g-wtf-decrypted.body.bin`
- Ghidra is not installed on this host, so the current triage used:
  - `arm-elf-eabi-objdump`
  - `xxd`
  - targeted literal-pool and callsite inspection

## Apple-Backed PMU Findings

- The decrypted WTF contains a real I2C0 transport path at `0x3c600000`.
- Apple PMU traffic uses slave `0x73`, which matches Rockbox Nano 3G PMU slave `0xe6 >> 1`.
- Confirmed Apple wrappers:
  - `0x5420`: PMU read-multiple helper
  - `0x5474`: PMU write-multiple helper
- PMU registers positively observed in the decrypted WTF during this pass:
  - `0x1d`
  - `0x1b`
  - `0x43`

## Backlight Result

- `D1671_REG_LEDCTL = 0x20` is still the best source-backed backlight candidate from Rockbox code.
- However, the decrypted Apple WTF did **not** yield an explicit `LEDCTL (0x20)` write in this triage pass.
- That means the first visible backlight payload is still blocked by missing Apple confirmation for the actual early-boot LEDCTL path.

## Apple-Backed LCD Findings

- The decrypted WTF contains a real LCD controller path using `0x38300000`.
- Confirmed controller preamble at `0x45bc`:
  - `0x38300000 = 0x80000000`
  - `0x38300000 = 0x80100db1`
  - `0x38300088 = 0x01000000`
  - `0x38300020 = 0x00000033`
  - `0x3830007c = 0x00000804`
- Apple sequence interpreter at `0x4624` decodes command/delay tables embedded in the WTF.
- Embedded awake sequence at body offset `0x70d1`, length `11`, decodes as:
  - `CMD8 0x11`
  - delay token `0x3c`
  - `CMD8 0x13`
  - `CMD8 0x29`

## Current Blocker

- The LCD command-mode helper at `0x42c8` still depends on runtime panel grouping and selects between at least two Apple-backed low-bit configurations:
  - `0x0c20`
  - `0x0da8`
- The panel-type / mode-selection path is only partially reduced so far.
- Because of that unresolved dependency, there is still no single universal first-visibility payload that meets the “no speculative MMIO” rule.

## Current Decision

- Decrypted firmware is now available and useful.
- First visible payload ready to test: **no**
- Reason:
  - no explicit Apple LEDCTL write was found yet
  - LCD awake path is real, but its command-mode dependency is not yet reduced to a non-speculative single path

## Next Safest Step

1. Continue tracing the Apple panel-type / command-mode selection path:
   - `0x4438`
   - `0x4500`
   - `0x455c`
   - `0x47e4`
   - `0x4838`
2. Reduce that logic to one exact, panel-backed command-mode choice.
3. Only then prepare a minimal LCD awake payload starting at body offset `0`.

## Payload Status

- No new hardware-writing payload was created in this phase.
- This is intentional:
  - the current evidence is strong enough to narrow the visibility path
  - but not yet strong enough to justify a first real LCD/backlight write test

## Runtime Callback Object Update

- The pre-`0x2200455c` LCD path depends on a runtime callback-object pointer at
  `0x22007398`.
- The static decrypted WTF body contains:
  - `0x22007398 = 0`
  - `0x2200739c = 0`
  - `0x220073a0 = 0`
- No in-image store to `0x22007398` has been found so far.

Confirmed wrapper slots:

- `0x22003ce0` -> object `+0x04`
- `0x22003d14` -> object `+0x08`
- `0x22003c98` -> object `+0x28`
- `0x22003cc0` -> object `+0x2c`

Observed argument patterns:

- slot `+0x04`: mode values `1`, `3`, `4`
- slot `+0x08`: pointer `0x22007338`
- slot `+0x28`: pointer `0x22007338`
- slot `+0x2c`: no explicit arguments

Current interpretation:

- `0x22007398` is probably populated by an earlier runtime phase outside the
  static WTF body
- `0x22007338` is likely service-owned context storage
- the direct LCD chain after that service is mostly reduced, but the service
  itself is still the exact blocker to a fully raw LCD cold-init payload

## Pre-LCD Hardware Bring-up Status

The single `lcd-fullinit-n3g.bin` hardware run changed the blocker.

What it ruled out:

- Apple LCD code and tables being absent from the test payload
- entrypoint misplacement
- a purely LCD-local command-mode mistake as the only issue

What it did **not** prove:

- that all platform prerequisites before `0x2200455c()` were already present

## Earliest Apple-Backed Prerequisite Evidence

### PMU-side work

Confirmed helpers:

- `0x22005420`: PMU read helper on slave `0x73`
- `0x22005474`: PMU write helper on slave `0x73`

Confirmed register activity:

- reg `0x1d = 0x0a` via `0x220054b0`
- reg `0x1b = 0x01` or `0x00` via `0x220054b0`
- reg `0x43` bit `0` read-modify-write via `0x220054f8`

Confirmed callers:

- `0x22003018`
- `0x2200304c`
- `0x22003078`

Current interpretation:

- PMU state is part of the early bring-up chain.
- The minimal LCD-relevant PMU subset is still unresolved.

### Early gate / clock work

Confirmed earlier-than-LCD `0x22000664` calls:

- `0x22001630`: `(0x10000, 0, 1)`
- `0x22001650`: `(0x10000, 0, 0)`
- `0x220018d8`: `(0x400, 0, 1)`
- `0x220018e8`: `(0x1, 0, 1)`
- `0x220018fc`: `(0x0, 0x2000, 1)`
- `0x2200205c`: `(0x2007df65, 0x1ef49, 0)`
- `0x2200206c`: `(0x06002082, 0x1036, 1)`

Additional direct MMIO evidence:

- `0x22001634..0x22001640`: clear bits `0..2` at `0x3930003c`
- `0x220017e8..0x22001804`: toggle bit `1` at `0x38400804`

Current interpretation:

- some platform resource state is being established before the LCD-local
  wrapper at `0x2200428c`
- the startup-local versus later-path split is now clearer:
  - `0x22001630` / `0x22001650` sit on the direct startup function
    `0x2200160c`
  - `0x220018d8` / `0x220018e8` / `0x220018fc` sit in the later
    `0x22001698` path reached through `0x22002fe4`
  - `0x2200205c` / `0x2200206c` are part of `0x22001f4c`, which is on the
    direct startup path
- the exact minimal subset is still not reduced enough to promote into a
  payload

### GPIO / pin configuration

Confirmed staging/config helpers:

- `0x220030f0`
- `0x22003130`
- `0x220061f4`
- `0x22001f4c`

Observed helper calls under `0x22001f4c`:

- `0x22000958`
- `0x22003890`
- `0x220039fc`
- bracketed by `0x22003c18(0)` and `0x22003c18(1)`

Current interpretation:

- `0x22001f4c` now looks more like early clock/reset programming than simple
  GPIO pinmux.
- `0x220030f0` / `0x22003130` move a 16-entry GPIO descriptor set between live
  `0x3cf00000` state and the RAM buffer at `0x2200791c`.
- `0x2200791c` now looks like a real Apple state dependency, not just padding:
  - state `4/5` calls `0x220030f0(0x2200791c)` before the LCD-local work
  - the only in-body producer is state `1/2` calling
    `0x22003130(0x2200791c)`
  - so replaying only the later LCD path skips an earlier Apple state transfer
- descriptor format is now explicit:
  - 8 bytes per entry
  - `+0x00..+0x03`: packed `PCON(group)` nibble config
  - `+0x04`: `PUNB(group)` low byte
  - `+0x05`: `PUNC(group)` low byte
  - `+0x06..+0x07`: unused
- the LCD-adjacent path actually calls `0x220061f4(3)`, which drops into the
  `0x3c200000` peripheral family and manipulates pins `72..75`
- because local triage and in-tree notes already associate `0x3c200000` with
  clickwheel / NAND-collision territory, `0x220061f4` is no longer a clean LCD
  candidate
- because `0x2200791c` appears to preserve whole GPIO-group state around the
  sideband `0x220061f4` path, the current best classification is:
  - **BYPASS SAFE** for a standalone LCD payload that intentionally omits
    `0x220061f4`
  - still unresolved if the Apple state-machine sideband path is replayed
- `0x22001f4c` is no longer treated as opaque:
  - it does **not** call `0x22003350`
  - it is the corrected direct startup helper block
  - it directly overlaps LCD-wrapper gate state because
    `0x2200206c -> 0x22000664` clears bit `1` in `0x3c500048`
  - current decision: include the full corrected `0x22001f4c` block rather
    than guessing a smaller subset

## Current Dependency Chain

Strongest current Apple-backed order:

1. immediate startup gate work via `0x2200160c`
2. state `4/5` pre-LCD transition
3. runtime service stage at `0x22007398`
4. LCD-local gate wrapper
5. LCD controller preamble
6. command-mode selection
7. panel helper / panel tables
8. awake tail

## Reduction Status

What is reduced enough to state explicitly:

- `0x22000664` writes directly to:
  - `0x3c500048`
  - `0x3c50004c`
- `0x22000664` semantics are now explicit:
  - `r2 == 0` sets bits
  - `r2 != 0` clears bits
- the PMU path is better classified:
  - `0x22003078(1)` reaches the observed PMU writes
  - the LCD state path that reaches `0x2200455c` uses `0x22003078(4)` instead
  - so the current PMU writes are adjacent platform state, not immediate LCD
    startup evidence
- `0x2200791c` is now reduced enough to classify:
  - it is a 16-entry GPIO state buffer derived from live `PCON/PDAT/PUNB/PUNC`
  - `0x22003130` produces it
  - `0x220030f0` consumes it
  - current decision: bypass-safe if the sideband `0x220061f4` path is omitted
- `0x22001f4c` is now reduced enough to prepare as a standalone startup block:
  - exact call graph:
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
- current standalone payload decision:
  - **INCLUDE MINIMAL SUBSET**
  - prepared artifact:
    - `tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.bin`
- `0x22004f0c` writes encoded GPIO commands to:
  - `0x3cf00200`
- the LCD-local gate wrapper is now exact:
  - `0x2200428c` calls `0x22000664(2, 0, 1)`
  - effect: clear bit `1` in `0x3c500048`
  - `0x220042b8` restores the prior state afterward

What remains unresolved:

- whether some writes inside the corrected `0x22001f4c` block can be safely
  dropped in a later refinement pass
- whether the prepared pre-gate payload is sufficient to produce the first
  visible LCD response, or whether some omitted Apple state-machine work still
  matters

## Current Safe Boundary

- A new payload was built but not run in this analysis phase:
  - `tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.bin`
- It includes:
  - corrected full `0x22001f4c` startup block
  - direct startup gates from `0x22001630` / inline `0x3930003c` /
    `0x22001650`
  - reduced runtime service table
  - embedded LCD-local full init
- It excludes:
  - PMU writes
  - `0x220061f4` / `0x3c200000`
  - `0x2200791c` restore path
- Next safest step:
  - if you want to proceed later, perform one controlled hardware run of
    `lcd-pregate-fullinit-n3g.bin` with the same DFU/recovery discipline as the
    prior LCD tests
  - if `mks5lboot --dfuscan` cannot establish a clean normal-DFU baseline first,
    abort before upload; `05ac:1223` in `lsusb` alone is not enough
  - in this Codex environment, `mks5lboot --dfuscan` may need outside-sandbox
    USB access; the Nano was later confirmed to be in normal DFU state `2` once
    the same command was run outside the sandbox

## Minimal Power / Backlight Decision

Current narrowed candidate for the next visibility experiment:

- **single added PMU write:** slave `0x73`, reg `0x1d`, value `0x0a`

Why this is the selected one-step addition:

- exact Apple write from the decrypted WTF
- one register write only
- does not depend on the excluded `0x220061f4` / `0x3c200000` path
- does not require guessing the polarity of PMU reg `0x43`

Why the other “one-step” options remain excluded:

- reg `0x1b = 0x01/0x00`
  - in-tree Nano 3G code already uses `0x1b` for `pmu_hdd_power()`
  - too likely to be generic HDD / power gating rather than a display step
- reg `0x43` bit `0`
  - only observed Apple call clears the bit
  - setting it as an “enable” would be speculation
- GPIO reset pulse
  - only clear candidate is still the excluded `0x220061f4` sideband path

Prepared artifact, not run:

- `tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.bin`

Delta relative to `lcd-pregate-fullinit-n3g.bin`:

- add `apple_05474(0x1d, 1, &0x0a)` before the existing pregate + LCD-local
  sequence

Current status:

- payload prepared only
- no hardware run yet
- recovery path unchanged

## Minimal Power / Backlight Decision, Revision 2

Second bounded PMU candidate prepared:

- `tools/ipodnano3g/minimal_payload/lcd-powerstep2-n3g.bin`

Exact PMU sequence:

1. slave `0x73`, reg `0x1d`, write `0x0a`
2. slave `0x73`, reg `0x43`, read-modify-write clear bit `0`

Reason for inclusion:

- both operations are directly Apple-backed in the decrypted WTF
- the second step uses the exact observed `0x43` clear polarity
- no new PMU registers were introduced

Still excluded:

- reg `0x1b`
- reg `0x43` bit-set polarity
- `LEDCTL (0x20)`
- `0x220061f4` / `0x3c200000`

Risk:

- Medium
- wider than `lcd-powerstep-n3g` by one PMU action only
- still much narrower than replaying the whole PMU branch

Run status:

- prepared only
- not executed

## LCD Visibility Root-Cause Isolation

Remaining candidates reviewed:

1. PMU reg `0x1b`
2. sideband `0x220061f4`

Outcome:

- **NOT_LCD** for pins `72..75` / `0x220061f4(3)`

Reasoning:

- `0x1b`
  - still behaves like storage/HDD power, not display
- `0x220061f4(3)`
  - is immediately before the LCD branch by ordering
  - but mode `3` does:
    - delay `25`
    - restore `GPIO72..75` to `op2`
    - then program and poll the confirmed `WHEEL_BASE` block at `0x3c200000`
- Nano 3G GPIO defaults already put group `9` in `op2`
  - pins `72..79` are group `9`
  - so this looks like clickwheel-side peripheral restore, not LCD reset
- no low/high reset pulse was found

Decision:

- do **not** prepare `lcd-powerstep3-n3g.bin`
- the next visibility lead must come from a different evidence path

## Display-Power / Backlight Search Outside Rejected Paths

Result:

- **STILL_BLOCKED**

What changed:

- Re-checked the decrypted WTF for a real Apple backlight path outside the
  rejected candidates.
- No new PMU/display helper was found:
  - still only `0x1d`
  - `0x1b`
  - `0x43`
- No explicit Apple `LEDCTL (0x20)` or brightness-reg write was recovered.

Best remaining lead:

- a later coupled startup cluster in `0x22001698`, not a clean one-step action:
  - `0x38400804` bit-1 pulse with delay `500`
  - `0x3c500048 &= ~0x400`
  - `0x3c500048 &= ~0x1`
  - `0x3c50004c &= ~0x2000`

Why it is still blocked:

- those writes are exact and Apple-backed
- but they sit inside a broader service-mediated path, not the already tested
  immediate startup path
- no single member of that cluster is yet justified as an isolated next test

Prepared artifact:

- none

## OSOS decrypt exited checkpoint

- Background decrypt has now exited.
- Final observed checkpoint from that run:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `3461328` bytes
  - real completion: `32.078%`
- Post-exit host note:
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`
  - so the next resume needs another clean DFU re-entry

## OSOS decrypt live in-progress checkpoint 6

- Background decrypt is still active:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `3347856` bytes
  - real completion: `31.027%`
- Current host note:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - that still matches an active decrypt process holding the USB session

## OSOS decrypt live in-progress checkpoint 5

- Background decrypt is still active:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `3243168` bytes
  - real completion: `30.056%`
- Current host note:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - that still matches an active decrypt process holding the USB session

## OSOS decrypt live in-progress checkpoint 4

- Background decrypt is still active:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `3138624` bytes
  - real completion: `29.088%`
- Current host note:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - that still matches an active decrypt process holding the USB session

## OSOS decrypt live in-progress checkpoint 3

- Background decrypt is still active:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2992032` bytes
  - real completion: `27.729%`
- Current host note:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - that still matches an active decrypt process holding the USB session

## OSOS decrypt live in-progress checkpoint 2

- Background decrypt is still active:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2923536` bytes
  - real completion: `27.094%`
- Current host note:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - that matches an active decrypt process still holding the USB session

## OSOS decrypt checkpoint 40

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2666640` bytes
  - real completion: `24.719%`
- Latest operational note:
  - the automatic loop resumed from clean DFU state `2`
  - one intermediate checkpoint remained usable in DFU state `9`, allowing one
    more bounded slice without user intervention
  - after the newest checkpoint, host state returned to the stale-DFU pattern:
    - `lsusb` still shows `05ac:1223`
    - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS decrypt live in-progress checkpoint

- Background decrypt is still active:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2794128` bytes
  - real completion: `25.895%`
- Current host note:
  - `mks5lboot --dfuscan` now reports `LIBUSB_ERROR_BUSY`
  - that matches an active decrypt process still holding the USB session, not a
    stale DFU failure

## OSOS decrypt checkpoint 38

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2476224` bytes
  - real completion: `22.949%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - after checkpointing, the Nano remained usable in DFU state `9`
  - automatic continue mode remained active

## OSOS decrypt checkpoint 39

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2591376` bytes
  - real completion: `24.019%`
- Latest operational note:
  - this checkpoint resumed directly from usable DFU state `9`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, host state returned to the stale-DFU pattern again:
    - `lsusb` still shows `05ac:1223`
    - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS decrypt checkpoint correction

- Before the next resume attempt, the recovery file was re-checked and found to
  be ahead of the last written note:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1720752` bytes
  - real completion: `15.947%`
- That same pre-check showed the Nano was host-visible but not yet in a clean
  usable DFU session:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- So the current decrypt status is still:
  - **OSOS_DECRYPT_STILL_IN_PROGRESS**
- Operational implication:
  - resume from `1720752` bytes after another clean DFU re-entry
  - do not restart decryption from scratch

## OSOS decrypt checkpoint 25

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1774224` bytes
  - real completion: `16.443%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, host state returned to the stale-DFU pattern again:
    - `lsusb` still shows `05ac:1223`
    - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS decrypt checkpoint 26

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1828560` bytes
  - real completion: `16.941%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint 27

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1884288` bytes
  - real completion: `17.459%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, host state returned to the stale-DFU pattern again:
    - `lsusb` still shows `05ac:1223`
    - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS decrypt checkpoint 28

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1935696` bytes
  - real completion: `17.938%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, host state returned to the stale-DFU pattern again:
    - `lsusb` still shows `05ac:1223`
    - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS decrypt checkpoint 29

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1994448` bytes
  - real completion: `18.482%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, host state returned to the stale-DFU pattern again:
    - `lsusb` still shows `05ac:1223`
    - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS decrypt checkpoint 30

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2044896` bytes
  - real completion: `18.952%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint 31

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2096064` bytes
  - real completion: `19.428%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint 32

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2147184` bytes
  - real completion: `19.894%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, host state returned to the stale-DFU pattern again:
    - `lsusb` still shows `05ac:1223`
    - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS decrypt checkpoint 33

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2197776` bytes
  - real completion: `20.367%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, host state returned to the stale-DFU pattern again:
    - `lsusb` still shows `05ac:1223`
    - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS decrypt checkpoint 34

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2254272` bytes
  - real completion: `20.891%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint 35

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2308800` bytes
  - real completion: `21.402%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint 36

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2365104` bytes
  - real completion: `21.918%`
- Latest operational note:
  - this checkpoint resumed cleanly from DFU state `2`
  - automatic continue mode remained active because DFU stayed usable after
    checkpointing

## OSOS decrypt checkpoint 37

- Newest checkpoint:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `2421456` bytes
  - real completion: `22.435%`
- Latest operational note:
  - this checkpoint resumed immediately from the prior clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages continued during the
    run but did not stop progress
  - after checkpointing, host state returned to the stale-DFU pattern again:
    - `lsusb` still shows `05ac:1223`
    - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS Decrypt Progress Update

Current status:

- **OSOS_DECRYPT_STILL_IN_PROGRESS**

Checkpoint history:

- checkpoint 3:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `165696` bytes
  - real completion: `1.536%`
- checkpoint 4:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `285648` bytes
  - real completion: `2.647%`

Latest run behavior:

- the resumable command recovered successfully from a DFU state `3` session and
  kept advancing
- repeated `libusb: interrupted [code -10]` messages appeared during the long
  run, but they did not invalidate the recovery buffer
- after intentional checkpointing, host USB returned to the common stale DFU
  condition:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Interpretation:

- the decrypt path itself is healthy
- the practical limit is still runtime length plus the need to re-enter clean
  DFU between some sessions
- no completed decrypted `OSOS` plaintext exists yet, so higher-level
  display/backlight triage still cannot begin

Newest checkpoint:

- checkpoint 5:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `350064` bytes
  - real completion: `3.244%`

Most recent session note:

- this checkpoint resumed cleanly from DFU state `2`
- the usual `libusb: interrupted [code -10]` messages appeared during the long
  run but did not stop progress
- after checkpointing, the device again returned to host-stale DFU:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 6:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `420240` bytes
  - real completion: `3.895%`

Latest operational note:

- this checkpoint also resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the device again returned to host-stale DFU:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 8:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `545472` bytes
  - real completion: `5.055%`

Latest operational note:

- this checkpoint also resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- unlike the previous several checkpoints, post-stop host state remained clean:
  - `mks5lboot --dfuscan` still reports DFU state `2`

Newest checkpoint:

- checkpoint 9:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `605760` bytes
  - real completion: `5.610%`

Latest operational note:

- this checkpoint also resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 13:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `850560` bytes
  - real completion: `7.882%`

Latest operational note:

- this checkpoint also resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano remained host-visible and still answered DFU,
  but in state `9` rather than clean state `2`

Newest checkpoint:

- checkpoint 14:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `921840` bytes
  - real completion: `8.547%`

Latest operational note:

- this checkpoint resumed successfully from DFU state `9` without a fresh DFU
  re-entry
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano returned to clean DFU state `2`

Newest checkpoint:

- checkpoint 15:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `990048` bytes
  - real completion: `9.176%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 23:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1574688` bytes
  - real completion: `14.595%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano returned to clean DFU state `2`

Newest checkpoint:

- checkpoint 24:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1643520` bytes
  - real completion: `15.235%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 22:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1482672` bytes
  - real completion: `13.740%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 21:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1414464` bytes
  - real completion: `13.107%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 20:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1346016` bytes
  - real completion: `12.473%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 16:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1054848` bytes
  - real completion: `9.775%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano returned to clean DFU state `2`

Newest checkpoint:

- checkpoint 19:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1273968` bytes
  - real completion: `11.803%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 18:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1202784` bytes
  - real completion: `11.148%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 17:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `1137024` bytes
  - real completion: `10.538%`

Latest operational note:

- this checkpoint resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano returned to clean DFU state `2`

Newest checkpoint:

- checkpoint 12:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `790512` bytes
  - real completion: `7.325%`

Latest operational note:

- this checkpoint also resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 11:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `730320` bytes
  - real completion: `6.763%`

Latest operational note:

- this checkpoint also resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 10:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `668688` bytes
  - real completion: `6.196%`

Latest operational note:

- this checkpoint also resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Newest checkpoint:

- checkpoint 7:
  - recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
  - saved plaintext: `483696` bytes
  - real completion: `4.481%`

Latest operational note:

- this checkpoint also resumed cleanly from DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the device again returned to host-stale DFU:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

## OSOS Decrypt Progress

Current status:

- **OSOS_DECRYPT_STILL_IN_PROGRESS**

Latest checkpoint:

- recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
- saved plaintext: `126096` bytes
- real completion: `1.167%`

What changed:

- clean DFU was restored again
- the resumable OSOS decrypt restarted successfully from `72960` bytes
- progress advanced past `125952` bytes before being stopped intentionally
  after checkpointing

Meaning:

- the recovery file is valid and reusable
- the supported OSOS decrypt path is still functioning
- the blocker is runtime length only
- there is still no completed higher-phase plaintext to triage for
  display/backlight candidates

## OSOS Decryption Follow-Up

Status:

- **OSOS decryption path exists, but the current attempt is blocked by stale
  DFU USB state**

What was confirmed:

- local `OSOS.fw` is a valid Nano 3G IMG1 (`87021.0`, format `3`)
- local `wInd3x` supports RetailOS extraction/decryption internally
- direct command:
  - `/tmp/wInd3x/wInd3x decrypt <OSOS.fw> <output>`
  started successfully and began decrypting

What failed:

- the resumable retry with `-r /tmp/n3g-osos-work/n3g-osos.recovery`
  immediately failed with `ClrStatus` / `libusb i/o error`
- follow-up `dfuscan` showed the Nano was no longer in a clean host-usable DFU
  baseline

Meaning:

- the tool path is real and supported
- the current blocker is just that the Nano needs to be re-entered into clean
  DFU before the long-running OSOS decrypt can resume

No new payload was prepared or run in this step.

Follow-up:

- after clean DFU re-entry, the resumable OSOS decrypt did restart correctly
- saved progress now exists at:
  - `/tmp/n3g-osos-work/n3g-osos.recovery`
  - `72960` bytes plaintext recovered
  - `0.676%` of OSOS body

So the blocker is now duration, not tool support.

## Fresh Display-Dependency Search Status

Current result:

- **STILL_BLOCKED**

What changed:

- Re-searched for a display-owned dependency outside all rejected paths.
- Checked the local `OSOS` / `aupd` artifacts, but their payload slices are
  still high-entropy / effectively encrypted here.
- Re-checked the decrypted WTF body for:
  - Apple backlight PMU writes
  - LCD-owned IRQ / gate setup
  - non-wheel GPIO reset pulses

What was confirmed:

- WTF still only exposes PMU regs:
  - `0x1d`
  - `0x1b`
  - `0x43`
- No Apple-backed PMU use of:
  - `0x20`
  - `0x28`
  - `0x29`
  was recovered in decrypted WTF.
- The only `#14` helper found in WTF is `0x22002024 -> 0x220039fc`, and
  `0x220039fc` is a generic `0x3c500000` bit helper rather than a clean LCD IRQ
  path.
- In-tree Nano 3G code still points at PMU backlight ownership through
  `D1671_REG_LEDCTL = 0x20`, while debug code reports backlight state/value via
  `0x29` / `0x28`.

Interpretation:

- The most likely missing visibility dependency is still in the PMU/backlight
  domain.
- But it is not yet Apple-backed strongly enough to justify a new hardware
  write outside the already tested `0x1d` / `0x43` steps.

Next safest step:

- obtain a decrypted or otherwise traceable higher-level firmware phase
  (`OSOS` or equivalent display/UI path) before trying another visibility
  payload

Prepared artifact:

- none

## Reduction of the `0x22001698` Cluster

Outcome:

- **STILL_BLOCKED**

What changed:

- Fully traced `0x22001698` instead of treating it as just four writes.
- It also touches:
  - `0x38100000`
  - `0x38e00010`
  - `0x38e00014`
  - `0x38e01010`
  - `0x38e01014`
- It depends on multiple service/runtime callbacks through the object at
  `0x22007784`.

Important result:

- gate ID `33` in this path decodes like a normal bank/bit gate ID and matches
  in-tree `CLOCKGATE_CWHEEL = 33`
- so this cluster is not cleanly display-specific

Status of `0x38400804`:

- still only one confirmed use
- still not proven LCD reset or panel-enable

Decision:

- do **not** prepare `lcd-clustergate-fullinit-n3g.bin`
- do **not** split out one cluster member as a standalone next test

## Reclassification of `0x22001698`

Outcome:

- **EXCLUDE_0x22001698**

Reasoning:

- `0x38e00000` is the VIC block, not a display clock block
- so the IDs in this path are IRQ enables, not display-gate enables
- decoded IDs are:
  - `19` -> `IRQ_USB_FUNC`
  - `33` -> `IRQ_EXT6`
  - `39` -> `IRQ_AES`
  - `40` -> unknown IRQ
- LCD would have shown up as `IRQ_LCD = 14`, and it does not
- the path also touches `0x38100000`, which is `MIUCON` on S5L8702
- nearby runtime tables are tagged `"Uart$"` and point at `0x3cc0xxxx`
  peripheral blocks

So this path now looks like broader peripheral/service wake, not display
visibility.

Prepared artifact:

- none

## 2026-04-23 OSOS decrypt live status

- OSOS device-assisted decrypt is actively progressing in a live PTY session.
- Latest observed saved offset from session output:
  - `3989760` bytes
  - `36.976%`
- Current rule:
  - do not interrupt the run
  - only ask for DFU re-entry if the decrypt exits and `dfuscan` stops working
- No new LCD/backlight candidate work is being started until OSOS plaintext is
  complete.

## 2026-04-23 OSOS decrypt live status update

- Live decrypt progress has reached:
  - `4165632` bytes
  - `38.605%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains deferred until plaintext
    completes

## 2026-04-23 OSOS decrypt live status update 2

- Live decrypt progress has reached:
  - `4364544` bytes
  - `40.449%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - higher-level display/backlight triage remains blocked on OSOS completion

## 2026-04-23 OSOS decrypt live status update 3

- Live decrypt progress has reached:
  - `4504320` bytes
  - `41.744%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-23 OSOS decrypt live status update 4

- Live decrypt progress has reached:
  - `4675584` bytes
  - `43.332%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-23 OSOS decrypt live status update 5

- Live decrypt progress has reached:
  - `5009664` bytes
  - `46.428%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-23 OSOS decrypt live status update 6

- Live decrypt progress has reached:
  - `5207040` bytes
  - `48.257%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-23 OSOS decrypt live status update 7

- Live decrypt progress has reached:
  - `5379072` bytes
  - `49.851%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-23 OSOS decrypt live status update 8

- Live decrypt progress has reached:
  - `5532672` bytes
  - `51.275%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-23 OSOS decrypt live status update 9

- Live decrypt progress has reached:
  - `5675520` bytes
  - `52.599%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-24 OSOS decrypt live status update 10

- Live decrypt progress has reached:
  - `6023424` bytes
  - `55.823%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-24 OSOS decrypt live status update 11

- Live decrypt progress has reached:
  - `6187776` bytes
  - `57.346%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-24 OSOS decrypt live status update 12

- Live decrypt progress has reached:
  - `6398976` bytes
  - `59.303%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-24 OSOS decrypt live status update 13

- Live decrypt progress has reached:
  - `6646272` bytes
  - `61.595%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-24 OSOS decrypt live status update 14

- Live decrypt progress has reached:
  - `6800640` bytes
  - `63.026%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion

## 2026-04-24 OSOS decrypt live status update 15

- Live decrypt progress has reached:
  - `7150080` bytes
  - `66.264%`
- Current status:
  - decrypt still owns the active session
  - no DFU intervention needed
  - OSOS-backed display/backlight triage remains blocked on completion
## 2026-04-24 OSOS display candidate

OSOS decryption is now complete, so the display/backlight search has moved past
the earlier WTF-only ceiling.

Current best candidate:

- decrypted OSOS body shows a compact higher-level display/backlight service
  sequence:
  - `0x2200374c`
  - `0x22004c5c`
  - `0x2200374c`
  - `0x22003774`

Why this matters:

- prior WTF-based payloads confirmed execution and LCD-local init, but the
  screen stayed black
- OSOS is the first phase that exposes explicit backlight/display UI strings
  and a plausible “turn visible” service sequence

Current status:

- `lcd-osos-visible-n3g.bin` is prepared
- it appends the OSOS visibility service sequence to the prior best
  startup/pregate/LCD path
- it has not been run yet

Open risk:

- the exact hardware MMIO of the new step is hidden inside Apple ROM/service
  imports, so this is stronger than a guessed PMU/GPIO write but not yet fully
  register-reduced

## 2026-04-24 OSOS visibility payload run result

One controlled hardware run of `lcd-osos-visible-n3g.bin` was completed.

What is confirmed:

- upload succeeded end-to-end through `wInd3x run`
- post-send host state matched the usual execution/takeover signature:
  - `lsusb` remained `05ac:1223`
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- recovery was confirmed afterward:
  - reset + DFU re-entry restored DFU state `2`

What is not confirmed:

- no visual display/backlight result was captured during the observation window

So this run is classified as:

- **EXECUTION ONLY**
- with the explicit note:
  - **no visual observation captured**

## 2026-04-24 OSOS raw reduction status

The follow-up attempt to convert the OSOS visibility path into direct hardware
writes is blocked.

Resolved boundary:

- `0x2200374c` and `0x22003774` are pure ROM/service veneers
- `0x22004c5c` is body-visible but only clears a service-object byte and calls:
  - `0x080646b4`
  - `0x08064790`
- the surrounding wrapper path adds more opaque service helpers rather than
  exposing direct PMU/GPIO/LCD MMIO

Current decision:

- do **not** build or run a raw-operations OSOS visibility payload
- no evidence-backed `lcd-osos-raw-visible-n3g.bin` exists yet

## 2026-04-24 ROM/service target status

The next boundary after OSOS body reduction is the imported service code at:

- `0x080646b4`
- `0x08064790`
- `0x080db704`
- `0x080dbe58`

Current ownership assessment:

- `0x08000000` is DRAM image/runtime space on S5L8702
- BootROM is documented locally at `0x00000000` and `0x20000000`
- therefore these targets are currently best treated as DRAM-backed Apple
  runtime/service code, not direct BootROM bodies

Current search result:

- decrypted WTF and OSOS bodies do not contain the target implementations
- no existing local artifact has yet provided a plaintext dump of those narrow
  service windows

Current supported next step:

- `wInd3x dump` exists locally as a documented read-only memory-read primitive
- narrow dump windows are now prepared conceptually for:
  - `0x08064000..0x08065fff`
  - `0x080db000..0x080dcfff`
- these were **not** executed in this step

Decision:

- **ROM_SERVICE_DUMP_POSSIBLE**

Implication:

- do not guess display/backlight MMIO from OSOS veneers
- next evidence-backed work should recover the missing service bodies first

## 2026-04-24 Runtime/service dump attempt result

The first practical read-only dump attempt against the missing runtime/service
window failed before any data was recovered.

Attempted command:

```bash
/tmp/wInd3x/wInd3x dump 0x08064000 0x2000 /tmp/n3g-romsvc-08064000.bin
```

Observed behavior:

- clean DFU state was confirmed beforehand
- `wInd3x dump` began and logged the first target offset
- the exploit then failed immediately with:
  - `bug trigger: USB timeout error`
- `/tmp/n3g-romsvc-08064000.bin` was left at size `0`
- DFU then fell back into the usual stale host-visible state

Current implication:

- the dump primitive exists locally and is documented
- but dumping the DRAM-backed `0x080...` service window is **not yet working**
  on the real Nano 3G in the tested DFU context

Updated decision:

- **ROM_SERVICE_DUMP_FAILED_WITH_REASON**

## 2026-04-24 Dump primitive classification

Tiny diagnostic reads now show that `wInd3x dump` is **not** universally broken
on Nano 3G.

What works:

- `0x20000000`, size `0x40`
- `0x00000000`, size `0x40`

Both returned identical 64-byte ARM-like data, consistent with documented
bootrom aliasing.

What fails:

- `0x08064000`, size `0x40`

Failure mode:

- immediate `bug trigger: USB timeout error`
- zero-byte output file
- DFU falls into stale host-visible state afterward

Decision:

- **DUMP_080_SMALL_FAILS**

Implication:

- current Nano 3G `dumpmem` use appears to work for bootrom-accessible regions
  but not for the DRAM-backed runtime/service window needed for OSOS display

## 2026-04-24 Alternate runtime read status

Inspection of local `wInd3x` command paths shows that:

- `dump`
- `spew`
- CP14/CP15 readers
- NAND helpers
- NOR helpers

all use the same bootrom-DFU exploit style:

- payload executes from IRAM / DFU buffer
- return data is surfaced through `HandlerFooter(...)`
- Nano 3G `HandlerFooter(...)` itself calls bootrom helper `0x2000aa40`

So there is no separate already-implemented local reader that obviously runs in
the later OSOS/runtime context where the `0x080...` service region appears to
live.

Current best future option:

- a read-only payload-assisted copy into `0x22000100` is structurally possible
  because that IRAM buffer is already used as a return scratch area

Current blocker:

- there is still no evidence that the present bootrom DFU execution context can
  safely dereference the `0x080...` runtime/service window in the first place

Decision:

- **CURRENTLY_BLOCKED_WITH_REASON**

## 2026-04-24 Minimal `0x080...` probe status

A tiny read-only probe payload is now prepared:

- `tools/ipodnano3g/minimal_payload/probe-080-read-n3g.bin`

Behavior:

- writes marker `0x11111111` to `0x22000100`
- reads one word from `0x08064000`
- stores it to `0x22000104`
- writes marker `0x22222222` to `0x22000108`
- loops forever

Safety:

- no MMIO writes
- no NAND
- no USB PHY
- no LCD
- no PMU
- no storage/audio

Current blocker is not the probe itself; it is result retrieval.

Important constraint:

- all currently working local readback paths (`wInd3x dump`, `cmd_spew`
  pattern, `HandlerFooter(...)`) depend on live bootrom DFU request handling
- after payload takeover, the Nano 3G consistently stops behaving like normal
  DFU and `mks5lboot --dfuscan` falls to `LIBUSB_ERROR_OTHER`

So there is no current evidence-backed command to read `0x22000100` **after**
the probe has taken over.

Decision:

- **PROBE_PREPARED_BUT_NO_RESULT_READBACK**

## 2026-04-24 Payload output-channel status

Follow-up search for a minimal result-output channel found no safe option within
the current constraints.

What is ruled out for now:

- clean return to readable DFU after standalone payload takeover
- post-payload `wInd3x dump` / `spew` style readback
- tiny standalone USB status output, because existing USB-capable helpers are
  tied to live bootrom DFU exploit context rather than the standalone image path

What remains theoretically possible but out of scope:

- piezo tone / beep

Why piezo is not promoted here:

- it is the only source-backed simple physical channel still standing
- but it is an audio path, and this task explicitly excludes preparing an
  audio-based proof payload

Decision:

- **NO_SAFE_OUTPUT_CHANNEL_FOUND**
  service reduction

## 2026-04-24 Early shared init probe status

The best bounded early-system init candidate from decrypted OSOS is now:

- `0x22002770`

Why it matters:

- it runs below reset-time global startup and before subsystem-local LCD/audio
  work
- it touches shared system state through:
  - CP15 control helpers
  - `0x38100000` (`MIU_BASE`)
  - `0x3c500000` (`CLK_BASE`)
  - `0x39900000`

Prepared only, not run:

- `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.bin`

Host-side validation:

- entrypoint `0x22000000`
- flat binary size about `14 KiB`
- embedded OSOS slices linked at original VMAs

Current blocker:

- the wrapper still depends on unresolved imported targets:
  - `0x22003414 -> 0x08016234`
  - `0x220034bc -> 0x0801542c`

So this is a bounded Apple call-level probe, not yet a fully raw write-only
init sequence.

## 2026-04-24 Early-init run and ROM import split

One controlled run of `system-init-probe-n3g.bin` is now host-confirmed:

- clean pre-run DFU state `2`
- upload succeeded
- post-send USB still visible as `05ac:1223`
- post-send `dfuscan` failed with `LIBUSB_ERROR_OTHER`

Important limitation:

- user later clarified there was no visible or audible behavior for that run

Classification:

- **SAFE_EXECUTION_ONLY**

ROM import split of `0x22002770`:

- `0x22003414 -> 0x08016234`
  - only called by the two wrapper-style early init entries
  - no visible arguments
  - return value ignored
  - best current classification: front-edge init barrier/hook
- `0x220034bc -> 0x0801542c`
  - used broadly as `r0` / `r1` -> `r0`
  - best current classification: arithmetic/timing helper
  - required by `0x22002420(1)` but not by `0x22002420(3)`

Prepared only, not run:

- `tools/ipodnano3g/minimal_payload/system-init-observable-n3g.bin`

Observable behavior plan:

- run bounded early init
- delay twice in software
- deliberately branch to `0xdead0000`

Purpose:

- distinguish immediate crash inside init from delayed fault after successful
  return through the wrapper

Host-side validation:

- entrypoint `0x22000000`
- flat binary size about `14 KiB`
- `_start` now:
  - sets stack
  - calls `0x22002770`
  - delays twice in software
  - branches to `0xdead0000`

Control probes prepared for ROM-import classification:

- `tools/ipodnano3g/minimal_payload/system-init-skiprom-n3g.bin`
  - no ROM call
  - two software delays
  - branch to `0xdead0000`
- `tools/ipodnano3g/minimal_payload/system-init-1call-n3g.bin`
  - call only `0x22003414`
  - two software delays
  - branch to `0xdead0000`

## 2026-04-24 Observable redesign after fault-path failure

Current conclusion:

- invalid-branch fault probes are not observable enough on Nano 3G
- black-screen behavior remained identical even when control flow differed

Safer replacement chosen:

- watchdog-timed reboot

Source basis:

- `firmware/target/arm/s5l8702/system-s5l8702.c`
  - `system_reboot()`
- `firmware/export/s5l87xx.h`
  - `WDT_BASE = 0x3c800000`

Prepared only, not run:

- `tools/ipodnano3g/minimal_payload/system-init-timingprobe-n3g.bin`

Behavior:

- call `0x22002770`
- short software delay
- long software delay
- write `0x00100000` to `0x3c800000`
- wait for watchdog reset

Why this is preferable:

- replaces undefined crash behavior with the target's own reboot path
- adds only one new MMIO write, to the documented watchdog base
- does not add LCD, PMU, GPIO, USB PHY, NAND, storage, or audio logic

Controlled run result:

- `system-init-timingprobe-n3g.bin` executed at the usual DFU-takeover level
- user observed black / no change for the full 30-second window
- no delayed Apple logo
- no immediate reboot
- no flicker / click / beep
- no visible USB disconnect/reconnect

Current classification:

- **WATCHDOG_NO_VISIBLE_RESET**

Implication:

- even the watchdog-based timing probe did not produce a usable physical output
  channel in the current payload context

## 2026-04-24 Execution-context shift

Current conclusion:

- raw `wInd3x run` payloads are executing too early
- defanged WTF plus a tiny payload still does not provide the initialized Apple
  runtime expected by higher-level OSOS display/service code

Local tooling already points to the better path:

- `wInd3x run`
  - BootROM -> defanged WTF -> standalone payload
- `wInd3x cfw run`
  - BootROM -> defanged WTF -> modified RetailOS

Supporting evidence:

- `/tmp/wInd3x/README.md`
- `/tmp/wInd3x/cmd/wInd3x/cmd_cfw.go`
- `/tmp/wInd3x/web/src/components.ts`

Important additional finding:

- `wInd3x` already has EFI patch infrastructure for RetailOS images:
  - `VisitPE32InFile`
  - `PatchAt`
  - `ReplaceExact`

So the most promising next-stage execution method is:

1. patch a RetailOS PE32/DXE module
2. boot that modified RetailOS through defanged WTF
3. execute inside Apple's already-initialized runtime

Decision:

- **CHAINLOAD_METHOD**

Practical next step:

- identify a post-init RetailOS module/function to hook instead of preparing
  more raw DFU payloads

## 2026-04-24 RetailOS chainload target selection

Nano 3G RetailOS should currently be treated as a raw ARM IMG1 body, not an
EFI/PE32 firmware volume.

Key evidence:

- Nano 3G WTF defanging in local `wInd3x` is raw-offset based
- decrypted Nano 3G OSOS does not parse as an EFI volume from body offset `0`
- `_FVH` firmware-volume signatures were not recovered from the body

So the Nano 3G `cfw run` path is still valid, but the safest first customized
RetailOS proof is a **raw visible-string patch** inside an already-initialized
UI path, not a PE32 hook.

Selected first proof target:

- USB/DiskMode connected-screen resource cluster

Chosen body offsets:

- `0x817980`: `Connected` -> `CFW mode!`
- `0x8179b8`: `Do not disconnect.` -> `CFW runtime ready!`
- `0x8179f0`: `OK to Disconnect` -> `CFW booted      `

Prepared only, not run:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu`

Patch helper:

- `/tmp/wInd3x/cmd/patch_n3g_connected_ui.go`

Why this is the first test instead of a code hook:

- zero new MMIO
- relevant while USB remains attached during `cfw run`
- inside Apple’s initialized UI runtime
- lower risk than patching `TCRemoteUI` dispatch/control flow on the first try

Controlled chainload result:

- `cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu` did not reach visible
  RetailOS UI
- user observed black screen / no change for the full 60-second window
- no patched text appeared
- no original Apple connected text appeared
- no normal Apple boot appeared
- host-side USB remained in `05ac:1242` (`WTF mode`)
- post-run `mks5lboot --dfuscan` was busy until the host `wInd3x` process was
  terminated
- after manual reset, recovery back to clean DFU state `2` succeeded

Current classification:

- **CHAINLOAD_BLACKSCREEN**

Implication:

- the first `cfw run` attempt confirms that a text/resource-only RetailOS patch
  is not enough by itself to force or prove entry into a visible connected UI
  path on Nano 3G
- the next step should focus on why RetailOS never escapes WTF handoff on this
  device, or on a lower-level but still post-init code hook rather than a UI
  resource substitution alone

## 2026-04-24 WTF -> RetailOS handoff notes

Current best diagnosis:

- **WTF_HANDOFF_BLOCKED_BY_CHECK**

Supporting evidence:

- `cfw run` reliably:
  - enters defanged WTF
  - uploads the RetailOS image
- the device then remains in USB `05ac:1242` (`WTF mode`)
- the patched RetailOS screen resources never become visible

Important comparison:

- stock Nano 3G OSOS wrapper:
  - format `3`
- decrypted and patched RetailOS wrapper:
  - format `2`

But that wrapper change is already produced by local `wInd3x` RetailOS
decryption/repack logic, so it is not unique to the custom visible-string patch.

More important is the Nano 3G defanged WTF asymmetry:

- Nano 3G:
  - only one patch at body offset `0x1990`
  - comment: skip signature check
- later devices:
  - patch both signature and later data/AES paths

Unpatched Nano 3G WTF block still present:

- runtime `0x22001998..0x220019dc`
- callback from `[service + 0x74]`
- checks on:
  - `0x38c00040`
  - `0x38c0000c`

Practical next step:

- extend Nano 3G defanged WTF, not RetailOS first
- patch the remaining validator/decrypt gate so the image can actually leave WTF
- then retry the same visible-string RetailOS proof image

## 2026-04-24 Nano 3G defanged WTF second-stage patch

Implemented:

- extend `devices.Nano3` raw defanger in:
  - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`

New patch:

- body offset `0x19b8`
- original:
  - `08 00 00 0a`
- patched:
  - `19 00 00 ea`

Intent:

- keep the callback at `0x220019ac` intact
- bypass the remaining failure branch after the callback result compare
- land on the existing success path at `0x22001a24`

Build result:

- rebuilt `wInd3x` successfully:
  - `/tmp/wInd3x/wInd3x`

Critical note before the next test:

- remove stale cached defanged WTF first:
  - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`

Otherwise the next `cfw run` will still use the pre-patch Nano 3G WTF image.

## 2026-04-24 Revised Nano 3G WTF handoff diagnosis

The second-stage patch at `0x19b8` is not enough by itself. Trace continuation
shows:

- `0x22001a24` only clears `r4`
- WTF still calls two more `[service + 0x8c]` callbacks
- WTF still runs `0x22006558`, which can stall polling
  `[selected UART base + 0x18] & 0x200`
- WTF only reaches the execute wrapper if:
  - `r4 == 0`
  - `r5 & 0x10` is set
  - `0x220024b8` is actually reached

Earlier checks are callback-driven too:

- `[service + 0x6c]` must return nonzero or the path exits with `r4 = 23`
- `[service + 0x74]` is called with:
  - base `0x08000000`
  - base + `0x800`
  - mode `2`

So the current root cause is best classified as:

- **CALLBACK_DEPENDENCY_REQUIRED**

Why this matters:

- freemyipod's docs describe WTF as a real second-stage bootloader that still
  performs IMG1 verification/decryption before booting the next stage
- upstream `wInd3x` still lists Nano 3G `CFW` as `soon`
- therefore the missing condition is likely validator/decrypt callback state,
  not one more outer compare

Current safe next step:

- stop adding blind top-level branch skips
- resolve the callback targets behind `[service + 0x6c]`, `[service + 0x74]`,
  and likely `[service + 0x8c]` before preparing another Nano 3G WTF patch

## 2026-04-24 Service callback table mapping

The critical handoff service object is now traced to a BootROM-side table:

- WTF literal:
  - `0x22007784`
- contents:
  - `0x20000020`

So the callbacks used by the handoff path are not coming from the local WTF
`Uart$` service descriptors. They are being loaded from a BootROM object/table.

Key slots:

- `+0x6c` -> `0x2000008c`
- `+0x74` -> `0x20000094`
- `+0x8c` -> `0x200000ac`
- paired helpers:
  - `+0x80` -> `0x200000a0`
  - `+0x90` -> `0x200000b0`

Observed roles from call conventions:

- `+0x6c`: no-arg readiness/validation gate, must return nonzero
- `+0x74`: called with `(0x08000000, 0x08000800, 2)`, likely image
  validation/decrypt/copy/setup
- `+0x8c`: side-effect callback used around IDs `19` and `33`
- `+0x90`: getter paired with `+0x8c`

Current classification:

- **CALLBACK_TABLE_BOOTROM_KNOWN**

Meaning:

- the service table base and slot layout are now known
- the actual BootROM function pointers stored in those words are still not
  recovered from local artifacts
- therefore no further exact WTF byte patch is justified yet

## 2026-04-24 Recovered callback bodies: root cause is image format

Recovered BootROM callback targets:

- `+0x6c` -> `0x200036c8`
- `+0x74` -> `0x200006dc`
- `+0x8c` -> `0x2000106c`
- `+0x90` -> `0x2000132c`

What they do:

- `0x2000106c` / `0x2000132c`:
  - setter/getter pair for per-ID BootROM tables
  - not the main validator/decrypt blocker
- `0x200036c8`:
  - readiness/state-synchronization callback
  - uses haxed-DFU scratch globals at `0x2203fff8` / `0x2203fffc`
- `0x200006dc`:
  - actual image loader/validator callback
  - called by WTF with:
    - base `0x08000000`
    - base + `0x800`
    - mode `2`

Critical finding from `0x200006dc`:

- in mode `2`, it only accepts image type:
  - `3`
  - `4`
- any other type returns failure

This directly conflicts with current local Nano 3G repack behavior:

- stock `OSOS.fw`:
  - format `3`
- local decrypted/patched Nano 3G RetailOS:
  - format `2`

So the current root cause is no longer best described as "one more missing
callback side effect". The immediate concrete blocker is:

- **the BootROM handoff loader rejects the format-2 Nano 3G RetailOS wrapper**

Correct next step:

- do not add another WTF branch patch
- change Nano 3G `cfw run`/repack to emit a wrapper acceptable to
  `0x200006dc(r2=2)`, most likely format `4`

## 2026-04-24 Format-4 Nano 3G RetailOS artifact prepared

Implemented the wrapper-side fix in:

- `/tmp/wInd3x/pkg/image/image.go`

Effect:

- Nano 3G no longer emits format `2` from `MakeUnsigned(...)`
- Nano 3G now emits format `4` with:
  - signature area `0x80`
  - certificate area `0x300`
  - version still `1.0`
  - magic still `8702`

Rebuilt:

- `/tmp/wInd3x/wInd3x`

Regenerated patched chainload image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Verified:

- format `4`
- entry `0x0`
- body `0xa4a570`
- cert offset `0xa4a5f0`
- cert length `0x300`

Current classification:

- **FORMAT4_CFW_IMAGE_PREPARED**

## 2026-04-24 Format-4 Nano 3G chainload test result

Tested once:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed:

- clean pre-run DFU:
  - `05ac:1223`
  - state `2`
- immediate switch into WTF mode during/after handoff:
  - `05ac:1242`
- post-run host scan:
  - `LIBUSB_ERROR_BUSY`
- no visible RetailOS progress:
  - no Apple logo
  - no patched connected text
  - no original Apple connected text

Recovery:

- reset and re-entered DFU successfully
- confirmed back at:
  - `05ac:1223`
  - DFU state `2`

Current classification:

- **CHAINLOAD_WTF_STUCK**

## 2026-04-24 Post-format-4 handoff diagnosis

The failed format-`4` chainload was traced further.

Verified facts:

- cached defanged WTF contained both Nano 3G branch patches
- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu` was really:
  - magic `8702`
  - version `1.0`
  - format `4`
  - correct stock-matching body length

So the remaining blocker is not stale cache or wrong image selection.

Recovered BootROM callback `0x200006dc` shows the next concrete failure gate
for type `4`:

- `0x200008e4: bl 0x200055f0`
- `0x200008e8: cmp r0, #1`
- `0x200008ec: bne 0x2000095c`

That gate runs after format acceptance and header-layout checks.

Current local Nano 3G unsigned wrapper still uses placeholder footer contents:

- `0x80` bytes of `'S'`
- `0x300` bytes of `'C'`

So the current best diagnosis is:

- the type-`4` wrapper gets farther than the old type-`2` image
- but it is still rejected later by the type-`4` verification path

Current classification:

- **FORMAT4_STILL_REJECTED**

## 2026-04-24 Nano 3G local loader-stub patch prepared

Next fix is prepared in local `wInd3x`.

Chosen approach:

- do not redirect the full service object at `0x22007784`
- patch only the single WTF callsite that invokes `[service + 0x74]`

Exact callsite patch:

- offset `0x19ac`
- original:
  - `33 ff 2f e1`
- replacement:
  - `c6 14 00 eb`
  - branch to local stub at `0x22006ccc`

Local stub:

- offset `0x6ccc`
- size `300` bytes
- placed inside confirmed unused zeroed WTF space

Stub strategy:

- preserve the BootROM loader's type-`4` setup path
- skip only the later cert/signature verification call to `0x200055f0`
- replay success-side version/entrypoint metadata writes
- return success

Rebuilt:

- `/tmp/wInd3x/wInd3x`

Current decision:

- **LOADER_CALLBACK_STUB_PLAN_READY**

## 2026-04-24 Revised Nano 3G chainload blocker

The first Nano 3G WTF-local loader stub did not fail because of a bad branch
encoding.

What is now established offline:

- body `0x19ac` was patched correctly:
  - original:
    - `33 ff 2f e1`
  - replacement:
    - `c6 14 00 eb`
  - target:
    - `0x22006ccc`
- ARM BL math is exact:
  - source `0x220019ac`
  - PC `0x220019b4`
  - destination `0x22006ccc`
  - delta `0x5318`
  - imm24 `0x14c6`
  - instruction `0xeb0014c6`

The first-stub failure class is now:

- **MISSING_SIDE_EFFECT**

Why:

- the first stub omitted BootROM setup work that the WTF handoff still needs
- the first stub did not preserve `r9`
- it skipped BootROM helpers:
  - `0x20001ef0`
  - `0x20001fe0`
- it also replaced body `0x19b8` with an unconditional branch:
  - original:
    - `08 00 00 0a`
  - first-stub replacement:
    - `19 00 00 ea`

That means the most justified current fix is not a new branch target. It is a
more faithful BootROM-callback replay plus restoration of the original status
branch behavior.

## Revised local fix prepared

Local `wInd3x` in `/tmp/wInd3x` is now rebuilt with:

- body `0x19ac`
  - still:
    - `c6 14 00 eb`
- body `0x19b8`
  - restored to:
    - `08 00 00 0a`
- body `0x6ccc`
  - revised local stub:
    - `452` bytes

Revised stub behavior:

- saves `{r4-r9, lr}`
- uses a `0x2c` stack frame like BootROM
- runs BootROM preflight helpers:
  - `0x20001ef0`
  - `0x20001fe0`
- if `r2 != 2`, tail-calls original `0x200006dc`
- if `r2 == 2`, replays the mode-`2` / type-`4` success-side path much more
  closely
- skips only the later verification call:
  - `0x200055f0`
- replays version and entrypoint metadata writes

## Current status

- local `wInd3x` rebuilt successfully
- fresh defanged WTF artifact regenerated offline and verified
- no new device-side `cfw run` performed

Current classification:

- **LOADER_CALLBACK_STUB_REVISED_OFFLINE**

## 2026-04-24 Revised-stub hardware retest

The revised local `wInd3x` build was tested once on hardware after confirming:

- `05ac:1223`
- DFU state `2`

Run used:

- cached revised defanged WTF:
  - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- command:
  - `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed result:

- defanged WTF upload completed
- firmware upload started
- device re-enumerated as:
  - `05ac:1242`
  - WTF mode
- `mks5lboot --dfuscan` after the run reported:
  - `LIBUSB_ERROR_BUSY`
- no visible RetailOS progress occurred

Updated interpretation:

- the revised local stub is still not sufficient
- restoring `0x19b8` and replaying more BootROM setup work did not complete the
  handoff
- the exact blocker remains unresolved

Current classification:

- **REVISED_STUB_TESTED_STILL_WTF**

## 2026-04-24 Callback state patch now narrowed

The strongest new result is that the old local stub was trying to reproduce the
BootROM callback from the outside, while skipping the single function most
likely to own the remaining hardware-side state:

- `0x200055f0`

Recovered explicit state writes from `0x200006dc` itself are only:

- `[*0x2203fff8 + 0x34] = img_hdr[4]`
- `[*0x2203fff8 + 0x35] = img_hdr[5]`
- `[*0x2203fff8 + 0x36] = img_hdr[6]`
- `[*0x2203fff8 + 0x30] = *(img_hdr + 0x08)`

Those writes are real, but they are not what WTF checks immediately after the
callback returns.

Immediate post-callback consumers in WTF are:

- `0x38c00040`
- `0x38c0000c`
- `[service + 0x8c]`
- `[service + 0x44]`
- `0x22006558`
- `0x220024b8`

That means the old stub's main gap was not “missing one more metadata field”.
It was “missing whatever side effects `0x200055f0` and the original callback
path leave behind”.

## Revised minimal patch

Local `wInd3x` is now updated with a smaller stub that:

1. calls original `0x200006dc` first for mode `2`
2. returns directly if BootROM already succeeds
3. only if BootROM returns `0`, and the image still passes the expected
   type/layout checks, replays the explicit success-side metadata writes and
   returns `1`

This preserves BootROM execution of:

- `0x200005dc`
- `0x20002574`
- `0x2000273c`
- `0x200020f8`
- `0x200055f0`

Current local stub size:

- `308` bytes

Current classification:

- **CALLBACK_STATE_REQUIRED_PATCH_READY**

Boundary:

- implemented and rebuilt locally in `/tmp/wInd3x`
- offline artifact regenerated and verified
- no new hardware test yet for this revision

## 2026-04-24 Narrowed callback-state patch tested on hardware

The narrowed local patch was then tested once on a Nano 3G in DFU.

Observed sequence:

- `wInd3x` used the cached local defanged WTF
- exploit completed
- defanged WTF upload completed
- firmware upload started
- the device left DFU and re-enumerated as:
  - `05ac:1242`
  - WTF mode
- `mks5lboot --dfuscan` afterward returned:
  - `LIBUSB_ERROR_BUSY`

Updated interpretation:

- allowing BootROM `0x200006dc` and `0x200055f0` to run still does not produce
  a successful WTF -> RetailOS handoff
- the problem is no longer well explained by skipped explicit metadata writes
  alone
- the remaining gap is likely one of:
  - additional callback-owned state outside the reconstructed metadata writes
  - a later callback/dispatcher expectation after `+0x74` returns
  - a separate execute-path precondition not yet reconstructed

Current classification:

- **STILL_BLOCKED**

## 2026-04-24 Post-loader path traced past callback reconstruction

This pass intentionally stops treating more loader-callback changes as the next
fix.

Concrete findings:

- `0x22002fe4` enters `0x22001698` with:
  - `r0 = 0x1d`
  - `r1 = 0x08000000`
  - `r2 = 0x00f80000`
- therefore the internal flag word `r5` has:
  - bit `0x8` set
  - bit `0x10` set

Post-loader corrections:

- `[service + 0x8c]` is BootROM setter `0x2000106c`
  - save/restore only
  - not a direct execute gate
- `[service + 0x44]` is not a callback
  - it points to a BootROM-owned word
  - WTF saves the old word, writes `0x22007e88`, then restores it later

Critical path narrowing:

- once control reaches `0x22001a24`, WTF sets:
  - `r4 = 0`
- from `0x22001a24` to `0x22001b04`, `r4` is never written again
- final execute test is:
  - `0x22001b04: cmp r4, #0`
  - `0x22001b0c: tst r5, #0x10`
  - `0x22001b14: blne 0x220024b8`

That means:

- if the post-loader path gets to `0x22001a24`
- and if `0x22006558` returns
- then WTF will call the execute wrapper

`0x22006558` therefore becomes the next exact blocker.

Why:

- its return value is ignored
- the only remaining blocking behavior inside it is the infinite poll at:
  - `0x220065d0..0x220065fc`
- wait condition:
  - `[selected_uart_base + 0x18] & 0x200` must clear

Resolved UART bases from the local `Uart$` registry:

- ID `0` -> `0x3cc00000`
- ID `1` -> `0x3cc04000`
- ID `2` -> `0x3cc08000`

Final execute target if reached:

- `0x220024b8` calls:
  - `0x08000800`

Current classification:

- **UART_SERVICE_STALL**

Boundary:

- no new patch prepared
- skipping the UART/service phase without proof would still be speculative

## 2026-04-24 UART stall reduction and patch preparation

The UART wait is now explained tightly enough to justify a Nano 3G-only patch.

Concrete trace result:

- `0x22006558` validates a local `Uart$` service record
- if `service_flags & 1`, it goes through formatting helpers and returns
- otherwise it loops over a caller-provided byte buffer and:
  - waits on `[uart_base + 0x18] & 0x200`
  - writes one byte to `[uart_base + 0x20]`

Selected UART on the active path:

- `0x2200177c` asks for service ID `0`
- first `Uart$` record points to:
  - `0x3cc00000`

Cross-check from local freemyipod source:

- `apps/uarttest/main.c`
  - `UFSTAT = *(volatile uint32_t *)0x3cc00018`
  - TX helper waits:
    - `while (UFSTAT & BIT(9))`
  - `UTXH = *(volatile uint8_t *)0x3cc00020`

Therefore:

- the stalled bit is `UFSTAT.TX FIFO FULL`
- this is a UART transmit-drain stall
- not a storage, USB, or execute-entry prerequisite

Why patching `0x22006558` is safe enough:

- all setup before the wait remains in `0x22006430`
- caller ignores the return value from `0x22006558`
- execute gate remains:
  - `r4 == 0`
  - `r5 & 0x10`
- earlier tracing already showed those conditions are satisfied once the path
  reaches `0x22001a24`

Prepared patch in local `/tmp/wInd3x`:

- runtime address:
  - `0x22006558`
- body offset:
  - `0x6558`
- original bytes:
  - `f8 40 2d e9 02 70 a0 e1`
- replacement bytes:
  - `00 00 a0 e3 1e ff 2f e1`

Effect:

- immediate `return 0` from the UART helper
- preserves prior service/UART setup in `0x22006430`
- avoids the unbounded TX FIFO poll

Current classification:

- **UART_STALL_PATCH_PREPARED**

Boundary:

- local `wInd3x` rebuilt
- offline defanged WTF regenerated and verified
- no new hardware run performed

## 2026-04-24 UART bypass patch tested on hardware

The Nano 3G UART helper bypass was then tested once on hardware.

Patch under test:

- `0x22006558`
  - `mov r0, #0`
  - `bx lr`

Observed sequence:

- clean DFU start:
  - `05ac:1223`
  - DFU state `2`
- `wInd3x` used the cached patched defanged WTF
- exploit completed
- defanged WTF upload completed
- firmware upload started
- device re-enumerated as:
  - `05ac:1242`
  - WTF mode
- `mks5lboot --dfuscan` afterward returned:
  - `LIBUSB_ERROR_BUSY`

Updated interpretation:

- bypassing the UART transmit helper is not enough to complete the handoff
- the old host-visible failure mode did not change
- so the remaining blocker is not explained by the `0x22006558` poll alone

Current classification:

- **STILL_BLOCKED_WITH_REASON**

## 2026-04-24 Execute gate verified, runtime outcome still ambiguous

The execute wrapper itself is no longer the fuzzy part.

Verified from the exact patched Nano 3G WTF artifact:

- `0x6558` UART bypass patch was present in both:
  - offline artifact
  - cached artifact used by `wInd3x`
- execute call site after the UART helper still is:
  - `0x22001b14: blne 0x220024b8`

`0x220024b8` behavior:

- save target from `r0`
- call `0x22002138(0)`
- `blx r4`
- call `0x22002138(2)` after return

No metadata read occurs here.

Entrypoint/load-address result:

- Nano 3G haxed DFU still forces:
  - `g_State->entrypoint = 0`
- `DFUBoot::CopyHeaderBody` still implies:
  - IMG1 header at `0x08000000`
  - body at `0x08000800`
- current OSOS payload also has:
  - `Header.Entrypoint = 0`

So the current runtime execute target:

- `0x08000800`

is consistent with the Nano 3G load model and is not obviously wrong.

What remains unknown:

- whether hardware reaches `0x22001b14` at all
- or reaches it, calls `0x08000800`, and the RetailOS body immediately
  returns/crashes

Prepared proof-only patch, not integrated or run:

- free space chosen:
  - runtime `0x22006d40`
  - body offset `0x6d40`
- proof stub bytes:
  - `03 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
  - meaning:
    - `mov r0, #3`
    - `ldr r3, =0x22002138`
    - `blx r3`
    - loop forever
- proof call-site patch:
  - runtime `0x22001b14`
  - body offset `0x1b14`
  - original:
    - `67 02 00 1b`
  - replacement:
    - `89 14 00 1b`

Purpose:

- if the execute gate is reached, branch to the proof stub instead of
  `0x220024b8`
- if no proof behavior appears, then the execute gate was not reached

Current classification:

- **STILL_BLOCKED_WITH_MAP**

## 2026-04-24 Pre-execute marker integrated, cache cleared

The proof-only execute-gate marker is now integrated into the local Nano 3G
defanged WTF build.

Exact proof patch set:

- `0x1b14`
  - original:
    - `67 02 00 1b`
  - replacement:
    - `89 14 00 1b`
  - effect:
    - `blne 0x22006d40`

- `0x6d40`
  - bytes:
    - `03 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
  - effect:
    - call `0x22002138(3)`
    - loop forever

Other Nano 3G defanged WTF patches remain present:

- `0x1990`
- `0x19ac`
- `0x19b8`
- `0x6558`

Verification:

- rebuilt local `/tmp/wInd3x/wInd3x`
- regenerated `/tmp/n3g-wtf-defanged-check.bin`
- verified parsed IMG1 body contains:
  - `0x1b14: 89 14 00 1b`
  - `0x6d40: 03 00 a0 e3 04 30 9f e5 33 ff 2f e1 ...`

Cache state:

- removed:
  - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`

RetailOS image:

- unchanged

Prepared next command, not run:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Current classification:

- **PRE_EXECUTE_MARKER_PREPARED**

## 2026-04-24 Pre-execute marker tested on hardware

The proof-only execute-gate marker was tested once on a Nano 3G in DFU.

Observed sequence:

- cache was empty, so the marked defanged WTF was generated fresh
- exploit completed
- defanged WTF upload completed
- firmware upload started
- device re-enumerated as:
  - `05ac:1242`
  - WTF mode
- `mks5lboot --dfuscan` afterward returned:
  - `LIBUSB_ERROR_BUSY`

Updated interpretation:

- the marker did not create a new host-visible state
- there is no evidence that control reached the patched branch at:
  - `0x22001b14`
- strongest current conclusion:
  - execute gate not reached

Current classification:

- **EXECUTE_GATE_NOT_REACHED**

## 2026-04-24 Upstream checkpoint map prepared before next hardware test

The failed `0x22001b14` marker means the execute wrapper is still too far
downstream to split the active failure.

Backward reduction from `0x22001b14`:

- `0x22001b14`
  - predecessor:
    - `0x22001b10`
- `0x22001b10`
  - predecessor:
    - `0x22001b0c`
- `0x22001b0c`
  - predecessor:
    - `0x22001b08`
- `0x22001b08`
  - predecessor:
    - `0x22001b04`
- `0x22001b04`
  - predecessor:
    - return from `0x22006558`
- `0x22001a24`
  - predecessors:
    - `0x220019c8`
    - `0x220019dc`
    - `0x22001994` on the alternate `r5 & 0x8 == 0` leg

Current interpretation:

- if `0x22001a24` is reached, the path to `0x22001b14` should be nearly linear
  with the currently prepared patches
- strongest present implication:
  - `0x22001a24` is probably not reached on hardware

Checkpoint map for the next split:

- `0x220016fc`
  - expected:
    - `0x220024d8(...) == 0`
  - failure:
    - `bne 0x22001b24`

- `0x22001710`
  - expected:
    - `0x22001c70(...) == 0`
  - failure:
    - `bne 0x22001ae4`

- `0x2200176c`
  - expected:
    - `0x2200152c(...) == 0`
  - failure:
    - `bne 0x22001ae4`

- `0x22001980`
  - expected:
    - `[service + 0x6c] != 0`
  - failure:
    - `r4 = 23`
    - `0x22001a28`

- `0x220019b0`
  - expected:
    - loader callback returns success
  - failure:
    - `r4 = 87`
    - `0x22001a28`
  - note:
    - already patched/tested indirectly

- `0x220019bc..0x220019dc`
  - expected:
    - `0x38c00040 & 3 == 0`
    - or wait completes and still reaches `0x22001a24`
  - failure:
    - `0x220019e0`
    - `r4 = 87`
    - `0x22001a28`

- `0x22001a24`
  - expected:
    - post-callback success landing
  - note:
    - chosen as the next single test split

- `0x22001b04`
  - expected:
    - reached if `0x22001a24` is reached and the UART bypass returns

Prepared staged markers:

- shared marker block:
  - body offset `0x6d40`
  - runtime `0x22006d40`
- marker A:
  - `0x22006d40`
  - `0x22002138(2)` then loop
- marker B:
  - `0x22006d60`
  - `0x22002138(3)` then loop
- marker C:
  - `0x22006d80`
  - `0x22002138(4)` then loop

Prepared branch replacements:

- `0x220019bc`
  - original:
    - `e3 a0 25 e3`
  - replacement:
    - `df 14 00 ea`

- `0x22001a24`
  - original:
    - `e3 a0 40 00`
  - replacement:
    - `cd 14 00 ea`

- `0x22001b04`
  - original:
    - `e3 54 00 00`
  - replacement:
    - `9d 14 00 ea`

Local preparation status:

- local `/tmp/wInd3x/wInd3x` rebuilt
- offline defanged WTF regenerated and verified
- `0x1a24` marker selected for the next single hardware test
- previous `0x1b14` marker removed and original bytes restored
- cached defanged WTF removed again
- no hardware run performed yet

Current classification:

- **MARKER_PATCH_PREPARED**

## 2026-04-24 Upstream `0x22001a24` marker tested once on hardware

The prepared `0x22001a24` split was tested once with no other changes to the
RetailOS artifact.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`
- `lsusb`
  - Nano 3G present in DFU mode

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x22001a24`
  - `e3 a0 40 00` -> `cd 14 00 ea`
  - branches to Marker B at:
    - `0x22006d60`
- Marker B:
  - calls `0x22002138(3)`
  - loops forever
- `0x22001b14`
  - left restored to original:
    - `67 02 00 1b`

Observed result:

- exploit completed
- defanged WTF upload completed
- firmware upload started
- host remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`
- post-run `lsusb` showed:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_BUSY`

Interpretation:

- no host-visible evidence that control reached the `0x22001a24` marker
- strongest current classification:
  - `MARKER_1A24_NOT_REACHED`
- next narrowing target should move earlier, most likely into:
  - `0x220019bc..0x220019dc`

Recovery status:

- device initially remained in WTF mode after the run
- a later recovery scan confirmed clean DFU again:
  - `05ac:1223`
  - DFU state `2`

## 2026-04-24 Earlier checkpoint marker prepared in `0x22001998..0x220019dc`

The next isolation step moves the marker earlier because `0x22001a24` was not
reached.

Chosen checkpoints:

- `0x220019b0`
  - immediately after local loader callback return
  - original:
    - `cmp r0, #0`
- `0x220019cc`
  - mid-region wait path
  - original:
    - `ldr r1, [r2, #12]`
- `0x220019dc`
  - final branch into the `0x22001a24` success landing
  - original:
    - `bne 0x22001a24`

Prepared marker stubs remain:

- `0x22006d40`
  - ID `2`
- `0x22006d60`
  - ID `3`
- `0x22006d80`
  - ID `4`

Prepared branch bytes:

- active first test target:
  - `0x220019b0`
  - `e2 14 00 ea`
- prepared but inactive:
  - `0x220019cc`
  - `e3 14 00 ea`
- prepared but inactive:
  - `0x220019dc`
  - `e7 14 00 ea`

Current local defanger state:

- `0x19b0` is the only active earlier marker
- `0x1a24` restored to:
  - `00 40 a0 e3`
- `0x1b14` remains restored to original
- local `/tmp/wInd3x/wInd3x` rebuilt
- offline defanged artifact regenerated and verified
- cached Nano 3G defanged WTF removed again
- no hardware run performed yet

Current classification:

- **MARKER_EARLY_PREPARED**

## 2026-04-24 Earlier `0x220019b0` marker tested once on hardware

The earliest prepared marker was tested once with no changes to the RetailOS
artifact and no changes to loader/stub logic.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`
- `lsusb`
  - Nano 3G present in DFU mode

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x220019b0`
  - `e3 50 00 00` -> `e2 14 00 ea`
  - branches to Marker A at:
    - `0x22006d40`
- Marker A:
  - calls `0x22002138(2)`
  - loops forever
- `0x22001a24`
  - restored to original:
    - `00 40 a0 e3`
- `0x22001b14`
  - left original:
    - `67 02 00 1b`

Observed result:

- exploit completed
- defanged WTF upload completed
- firmware upload started
- host remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`
- post-run `lsusb` showed:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- no host-visible evidence that control reached the `0x220019b0` marker
- strongest current classification:
  - `MARKER_19B0_NOT_REACHED`
- next narrowing target should move earlier than the callback result handling

Recovery status:

- manual recovery succeeded
- device is back in clean DFU

## 2026-04-24 Early OSOS marker prepared after `0x220039c4`

- preserved-service68 WTF baseline re-verified with raw-body dumper only:
  - `0x22001938 = 31 ff 2f e1`
  - `0x2200193c = 27 00 a0 e3`
  - `0x2200197c = 30 ff 2f e1`
  - `0x220019ac = c6 14 00 eb`
  - `0x220019d0 = 01 10 b0 e3`
  - `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
  - `0x22001b14 = 67 02 00 1b`
  - `0x220024b8 = 10 40 2d e9`
  - `0x220024c8 = 34 ff 2f e1`
- stock OSOS `0x22008814` restored:
  - `6a ec ff ea`
- early startup trace refinement:
  - `0x22008814` is `bl 0x2200558c`
  - `0x220039c4..0x22003a34` is a pure CP15 region/MMU setup helper
  - the first useful continuation point after that helper chain returns is
    `0x220024e4`
- prepared fresh OSOS probe image:
  - `/tmp/n3g-osos-work/n3g-osos-early-marker-024e4.dfu`
  - body:
    - `/tmp/n3g-osos-work/lcd-osos-early-marker-024e4.bin`
- marker:
  - `0x220024e4`
  - `f4 37 1f e5 -> c1 1b 00 ea`
  - stub at `0x220093f0`:
    - `0e 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- status:
  - prepared only
  - not run yet

## 2026-04-24 Cache-aware DRAM probe hardware retest

Prepared probe:

- keep current WTF handoff fixes active
- patch final handoff to branch to a local copy stub
- copy 8 bytes into `0x08000800`:
  - `ldr pc, [pc, #-4]`
  - `.word 0x220076d4`
- call:
  - `0x22000f8c`
  - `0x22001124`
- then jump to `0x08000800`

Expected effect:

- if CPU executes from DRAM at `0x08000800` after the copy, the tiny DRAM stub
  should immediately jump to the IRAM marker at `0x220076d4`

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
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

- adding explicit cache maintenance after the DRAM write did not change the
  externally visible outcome
- this strengthens the earlier negative result: the problem is not just “we
  wrote code into DRAM but forgot to invalidate caches”
- CPU execution from `0x08000800` is still not proven on hardware

Updated decision:

- **STILL_BLOCKED_WITH_REASON**

## 2026-04-24 IRAM execution control probe prepared

The next control test removes DRAM from the handoff completely.

Prepared patch:

- leave the current WTF-side state fixes active:
  - readiness stub
  - loader callback stub
  - UART return
  - `[service + 0x68]` bypass
  - status emulation
- change only the final wrapper handoff:
  - `0x220024c8`
  - `34 ff 2f e1` -> `81 14 00 ea`
  - branch directly to:
    - `0x220076d4`

IRAM target:

- `0x220076d4`
- local marker stub:
  - `0x22002138(12)`
  - infinite loop

Verified from raw-body dump:

- `0x24c8 = 81 14 00 ea`
- `0x76d4 = 0c 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- still active:
  - `0x1938 = 64 17 00 eb`
  - `0x197c = 1f 15 00 eb`
  - `0x19ac = c6 14 00 eb`
  - `0x19d0 = 01 10 b0 e3`
  - `0x6558 = 00 00 a0 e3 1e ff 2f e1`

Purpose:

- prove whether the late-handoff path can execute known-good IRAM code at all
  under the current patch set, independent of DRAM / OSOS entry assumptions

## 2026-04-24 DFU-context diagnostic prepared at `0x2200193c`

The strongest remaining blind spot is now the original callback at:

- `0x22001938: blx [service + 0x68]`

Reason:

- the readiness callback at `[service + 0x6c]` has a local stub that at least
  replays the two concrete scratch-global advances recovered from
  `0x200036c8`
- but `[service + 0x68]` has so far been a pure `bx lr`-style bypass and
  preserves no original side effects
- every later handoff experiment still collapses into the same DFU-resident
  state

Prepared split:

- restore original call:
  - `0x1938 = 31 ff 2f e1`
- active marker immediately after return:
  - `0x193c = 69 17 00 ea`
  - branch target:
    - `0x220076e8`

Marker stub:

- `0x220076e8`
- `0x22002138(13)`
- infinite loop

Verified from raw-body dump:

- `0x1938 = 31 ff 2f e1`
- `0x193c = 69 17 00 ea`
- `0x76e8 = 0d 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`

Interpretation goal:

- prove whether `[service + 0x68]` actually returns in the original path
- if it does, then its missing side effects are now the strongest DFU-context
  latch candidate

## 2026-04-24 Hardware result for `[service + 0x68]` return split

Observed host result with original `0x22001938` restored and the marker placed
at `0x2200193c`:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device found

Interpretation:

- the host signature changed materially from the DFU-resident baseline
- strongest reading is that `[service + 0x68]` returned and control reached the
  first instruction after the call
- the callback itself is therefore no longer the likely non-return blocker
- its original side effects remain the strongest missing DFU-context latch

Updated decision:

- **SERVICE_68_RETURNS**

## 2026-04-24 Repeat confirmation of the `[service + 0x68]` return signature

Repeated the original-`0x1938` / post-`0x193c` marker run from clean DFU.

Observed host result was identical:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- no Nano present on `lsusb` after the run
- `mks5lboot --dfuscan` found no DFU device

Interpretation:

- the “device disappears from USB entirely” behavior is stable for this split
- this reinforces that `[service + 0x68]` returns and that bypassing it was
  suppressing required side effects rather than removing a non-return blocker

## 2026-04-24 Exact `[service + 0x68]` BootROM target resolved

Recovered service-table word:

- `0x20000088 = 0x200035a8`

So the callback at:

- `0x22001938: blx [service + 0x68]`

is now tied to the specific BootROM routine:

- `0x200035a8`

Current limit:

- existing local BootROM dumps do not include the `0x200035a8` code window
- so exact memory writes / hardware writes / latch changes inside that routine
  are still blocked on a new dump covering `0x20003580..0x20003600`

What is already strong enough to keep:

- the callback must remain original
- its side effects are required
- the safest patch strategy remains:
  - keep original `[service + 0x68]`
  - continue narrowing only after it

## 2026-04-24 Preserved-`[service + 0x68]` baseline verified

Baseline chosen for the next execute-path retest:

- restore:
  - `0x1938 = 31 ff 2f e1`
  - `0x193c = 27 00 a0 e3`
  - `0x197c = 30 ff 2f e1`
  - `0x1b14 = 67 02 00 1b`
  - `0x24b8 = 10 40 2d e9`
  - `0x24c8 = 34 ff 2f e1`
- keep active:
  - `0x19ac = c6 14 00 eb`
  - `0x19d0 = 01 10 b0 e3`
  - `0x6558 = 00 00 a0 e3 1e ff 2f e1`

Interpretation:

- `[service + 0x68]` stays fully original
- readiness is now retried under the state that `0x200035a8` actually
  initializes
- all wrapper/direct-jump/DRAM/IRAM experiments are removed from the live
  baseline

## 2026-04-24 Hardware result for preserved-`[service + 0x68]` baseline

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- no Nano present on `lsusb` after the run
- `mks5lboot --dfuscan` found no DFU device

Interpretation:

- once the original `[service + 0x68]` side effects are preserved, the handoff
  no longer collapses back to the old DFU-resident state even with the original
  execute wrapper and `blx r4` restored
- this is the strongest current evidence that `[service + 0x68]` was the key
  DFU-context escape requirement

Updated classification:

- **PRESERVED_SERVICE68_EXECUTE_CHANGED**

## 2026-04-24 OSOS entry-loop probe on preserved-service68 baseline

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- no Nano present on `lsusb` after the run
- `mks5lboot --dfuscan` found no DFU device

Interpretation:

- with the preserved-service68 baseline, the OSOS entry-loop probe remains
  compatible with the post-escape signature
- this keeps the focus inside early OSOS startup rather than pushing analysis
  back into WTF handoff mechanics

Updated classification:

- **POST_ESCAPE_OSOS_ENTRY_REACHED**

## 2026-04-24 DRAM probe negative-result analysis

The earlier DRAM probe wrote a tiny redirect stub into `0x08000800` and jumped
there, but it did no cache maintenance after the write.

That means the negative result does not yet justify:

- `DRAM_NOT_EXECUTABLE`
- `DRAM_COPY_NOT_HAPPENING`

Prepared stronger probe:

- same 8-byte DRAM payload
- plus:
  - `bl 0x22000f8c`
  - `bl 0x22001124`
  before the jump

Verified bytes:

- `0x220024c8 = a2 14 00 ea`
- `0x22007758 = 14 00 9f e5 14 10 9f e5 00 10 80 e5 10 10 9f e5`
- `0x22007768 = 04 10 80 e5 06 e6 ff eb 6b e6 ff eb 04 f0 9f e5`
- `0x22007778 = 00 08 00 08 04 f0 1f e5 d4 76 00 22`

Prepared literal-only alternate destinations:

- `0x08000800`
- `0x08000000`
- `0x08001000`
- `0x22008000`

## 2026-04-24 - direct PC entry patch prepared

Prepared a direct non-returning entry patch for the final handoff.

Verified bytes:

- `0x22001b14 = 67 02 00 1b`
- `0x220024c8 = a7 14 00 ea`
- `0x2200776c = 04 f0 1f e5 00 08 00 08`

Interpretation:

- execute gate still calls the normal wrapper path
- wrapper still runs its prologue and the `0x22002138(0)` call
- the final handoff instruction now branches to a local stub
- the stub directly loads `pc = 0x08000800`
- this removes `blx r4` / call-return semantics from the final transfer

Status:

- local `wInd3x` rebuilt
- defanged WTF cache cleared
- no hardware run performed yet

## 2026-04-24 - direct PC entry run result

Run result with direct-PC handoff patch:

- host side:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF
  - device stayed on USB as `05ac:1223`
  - `mks5lboot --dfuscan` returned `LIBUSB_ERROR_OTHER`
- device side:
  - black screen for the full observation window
  - no Apple logo
  - no backlight or flicker
  - no patched text
  - no stock UI
  - no visible reset loop

Conclusion:

- direct `pc = 0x08000800` handoff still produced the same DFU-resident state
- this argues against the remaining issue being just `blx r4` semantics or the
  final target register value

## 2026-04-24 execution-context fix prepared

Static comparison against the solved S5L87xx handoff suggests the missing step
is architectural cleanup before entry, not a different target.

Key Nano 3G WTF tail result:

- `0x22001a28: bl 0x220007bc`
  - disables IRQ/FIQ
- `0x22001ae0: bl 0x220007d0`
  - restores CPSR interrupt-mask bits before execute

Helper interpretation:

- `0x22002138(0)` is a status/log helper, not a context helper
- `0x22002724` is allocator/list bookkeeping
- `0x220007d0` restores I/F bits and is the strongest candidate for undoing the
  clean-handoff interrupt state

Prepared context patch:

- `0x220024c8: 34 ff 2f e1 -> a2 14 00 ea`
- local stub at `0x22007758`
- stub sequence:
  - `bl 0x220007bc`
  - `bl 0x220010e4`
  - `bl 0x22001110`
  - `bl 0x220010f8`
  - `bl 0x22001138`
  - `bl 0x22001124`
  - `ldr pc, [pc, #-4]`
  - `.word 0x08000800`

Purpose:

- match the solved handoff pattern more closely
- avoid broad guessed hardware writes
- test whether IRQ/MMU/cache teardown is the missing requirement before OSOS
  entry

## 2026-04-24 execution-context fix run result

Run result with the context-oriented handoff patch:

- host side:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF
  - device stayed on USB as `05ac:1223`
  - `mks5lboot --dfuscan` returned `LIBUSB_ERROR_OTHER`
- device side:
  - nothing visible on screen
  - no Apple logo
  - no backlight or flicker
  - no text
  - no visible reset loop

Conclusion:

- disabling IRQ/FIQ plus MMU/cache teardown before `pc = 0x08000800` still
  produced the same DFU-resident state
- the remaining failure is blocked deeper than the minimal execution-context
  cleanup now tested

## 2026-04-24 OSOS entry dependency and DRAM probe preparation

New static finding:

- the body entry at `0x08000800` is a relocation trampoline, not the final
  runtime body
- startup quickly routes:
  - `0x08000800 -> 0x08009008 -> 0x080041c4`
- `0x080041c4` checks relocation delta and, when nonzero, enters a path that
  ultimately branches to:
  - `0x22000000`

That means OSOS entry is assuming more than “valid code at `0x08000800`”.

Prepared diagnostic probe:

- keep current Nano 3G WTF fixes
- replace final handoff with a local copy stub
- copy stub writes this 8-byte sequence into `0x08000800`:
  - `ldr pc, [pc, #-4]`
  - `.word 0x220076d4`
- then jumps to `0x08000800`
- `0x220076d4` is a known IRAM marker stub that calls `0x22002138(12)` and
  loops

Purpose:

- prove or disprove raw DRAM execution at `0x08000800` without involving OSOS
  relocation, vectors, or startup assumptions

## 2026-04-24 Forced-r4 handoff prep

Prepared a WTF-side handoff patch that forces the final jump register to the
expected entry value immediately before execution:

- `0x22001b14`
  - kept original:
    - `67 02 00 1b`
- `0x220024c8`
  - `34 ff 2f e1` -> `a7 14 00 ea`
- `0x2200776c` stub:
  - `ldr r4, [pc, #4]`
  - `blx r4`
  - `b 0x220024cc`
  - `.word 0x08000800`

Verified with the raw-body dumper only:

- `0x220024c8 = a7 14 00 ea`
- `0x2200776c = 04 40 9f e5 34 ff 2f e1 54 eb ff ea 00 08 00 08`

## 2026-04-24 Result of the forced-r4 handoff run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- forcing `r4 = 0x08000800` immediately before the jump did not change the
  host-side DFU-resident outcome
- strongest current host-side classification:
  - `TARGET_STATE_STILL_BLOCKED`

## 2026-04-24 OSOS entry verification

Confirmed again:

- IMG1 entrypoint field is:
  - `0x0`
- WTF handoff target remains:
  - `0x08000800`
- wrapper path copies:
  - `r4 = 0x08000800`
- even target means ARM mode is preserved across `blx r4`

Stock OSOS entry bytes are valid ARM code, not garbage:

- `0x08000800 = 00 22 00 ea`
- `0x08000804 = f2 0c 00 ea`
- `0x08000808 = f3 0c 00 ea`
- `0x0800080c = f4 0c 00 ea`

Disassembly at runtime VMA:

- `0x08000800: b 0x08009008`
- then a branch-table style startup sequence

Prepared a first-instruction entry probe:

- `/tmp/n3g-osos-work/n3g-osos-entry-first-loop-probe.dfu`
- `/tmp/n3g-osos-work/lcd-osos-entry-first-loop-probe.bin`

Probe patch:

- body offset `0x0`
- `00 22 00 ea` -> `fe ff ff ea`

Meaning:

- if the CPU actually executes `0x08000800`, the probe should self-loop at the
  first instruction and change behavior relative to the current baseline

## 2026-04-24 Result of the OSOS first-instruction loop probe

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the self-loop at `0x08000800` did not change the host-side signature from the
  current DFU-resident baseline
- host-side alone this does not prove that `0x08000800` is being executed

## 2026-04-24 Direct-jump wrapper-bypass prep

Prepared the wrapper-bypass variant:

- `0x22001b14`
  - original:
    - `67 02 00 1b`
  - replacement:
    - `6b 02 00 ea`
  - direct target:
    - `0x220024c8`

Verified with the raw-body dumper only:

- `0x22001b14 = 6b 02 00 ea`
- `0x220024b8 = 10 40 2d e9`
- `0x220024bc = 00 40 a0 e1`
- `0x220024c0 = 00 00 a0 e3`
- `0x220024c4 = 1b ff ff eb`
- `0x220024c8 = 34 ff 2f e1`

Status:

- image prepared
- not run yet

## 2026-04-24 Result of the direct-jump wrapper-bypass run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- direct branch from `0x22001b14` to `0x220024c8` did not change the host-side
  DFU-resident outcome
- strongest current host-side classification:
  - `DIRECT_JUMP_SAME_DFU_RESIDENT_STATE`

## 2026-04-24 Execute-wrapper entry marker prep

Prepared the next split at the execute-wrapper entry:

- `0x220024b8`
  - original:
    - `10 40 2d e9`
  - replacement:
    - `ab 14 00 ea`
  - stub:
    - `0x2200776c`

Verified with the raw-body dumper only:

- wrapper bytes:
  - `0x220024b8 = ab 14 00 ea`
  - `0x220024bc = 00 40 a0 e1`
  - `0x220024c0 = 00 00 a0 e3`
  - `0x220024c4 = 1b ff ff eb`
  - `0x220024c8 = 34 ff 2f e1`
- execute-gate bytes restored:
  - `0x22001b04 = 00 00 54 e3`
  - `0x22001b0c = 10 00 15 e3`
  - `0x22001b14 = 67 02 00 1b`

Optional direct-jump patch prepared but not active:

- `0x220024b8`
  - `10 40 2d e9` -> `02 00 00 ea`
  - direct branch to `0x220024c8`

Status:

- image prepared
- not run yet

## 2026-04-24 Result of the 0x220024c8 wrapper-pre-jump-marker run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same DFU-resident reached-marker signature seen for the wrapper
  entry and earlier execute-gate markers
- strongest current classification:
  - `WRAPPER_PREJUMP_REACHED`

## 2026-04-24 Execute-wrapper pre-jump marker prep

Prepared the next split at the final wrapper handoff site:

- `0x220024c8`
  - original:
    - `34 ff 2f e1`
  - replacement:
    - `a7 14 00 ea`
  - stub:
    - `0x2200776c`

Verified with the raw-body dumper only:

- `0x220024b8 = 10 40 2d e9`
- `0x220024bc = 00 40 a0 e1`
- `0x220024c0 = 00 00 a0 e3`
- `0x220024c4 = 1b ff ff eb`
- `0x220024c8 = a7 14 00 ea`

Execute-gate bytes remain original:

- `0x22001b04 = 00 00 54 e3`
- `0x22001b0c = 10 00 15 e3`
- `0x22001b14 = 67 02 00 1b`

Status:

- image prepared
- not run yet

## 2026-04-24 Result of the 0x220024b8 wrapper-entry-marker run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- host also printed:
  - `handle_events: error: libusb: interrupted [code -10]`
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same DFU-resident reached-marker signature seen for the previous
  execute-gate markers
- strongest current classification:
  - `WRAPPER_ENTRY_REACHED`

## 2026-04-24 Result of the 0x22001b14 execute-gate-marker run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same DFU-resident reached-marker signature seen for the previous
  confirmed execute-gate markers
- strongest current classification:
  - `MARKER_1B14_REACHED`

## 2026-04-24 Tail marker prep after confirmed 0x22001a4c reach

Prepared the next post-landing linear-tail marker at:

- `0x22001a78`
  - original:
    - `32 ff 2f e1`
  - replacement:
    - `22 17 00 ea`
  - stub:
    - `0x22007708`

Verified with the raw-body dumper only:

- `0x22001a24 = 00 40 a0 e3`
- `0x22001a28 = 63 fb ff eb`
- `0x22001a4c = 2a 06 00 eb`
- `0x22001a78 = 22 17 00 ea`

Still-active supporting WTF patches:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Also moved the defanged USB product string so it no longer overlaps the active
marker stub:

- stub at:
  - `0x22007708`
- string at:
  - `0x2200771c`

Status:

- image prepared
- not run yet

## 2026-04-24 Result of the unconditional execute-force run

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- forcing `0x22001b14` to an unconditional branch into `0x220024b8` still did
  not change the host-side outcome from the DFU-resident late-handoff pattern

Classification status:

- pending device-side screen observation

## 2026-04-24 Result of the 0x22001b04 execute-gate-marker run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same DFU-resident reached-marker signature seen for the previous
  confirmed late-tail markers
- strongest current classification:
  - `MARKER_1B04_REACHED`

Recovery status:

- manual clean-DFU reconfirmation still pending

## 2026-04-24 Result of the 0x22001b0c execute-gate-marker run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same DFU-resident reached-marker signature seen for the previous
  confirmed execute-gate and late-tail markers
- strongest current classification:
  - `MARKER_1B0C_REACHED`

## 2026-04-24 Result of the 0x22001ae8 tail-marker run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same DFU-resident reached-marker signature seen for the previous
  confirmed post-landing tail splits
- strongest current classification:
  - `MARKER_1AE8_REACHED`

Recovery status:

- manual clean-DFU reconfirmation still pending

## 2026-04-24 Execute-gate prep after proving 0x22001ae8

Prepared the next split at the first execute-gate compare:

- `0x22001b04`
  - original:
    - `00 00 54 e3`
  - replacement:
    - `18 17 00 ea`
  - stub:
    - `0x2200776c`

Verified with the raw-body dumper only:

- `0x22001ae8 = 0d 03 00 eb`
- `0x22001b04 = 18 17 00 ea`
- `0x22001b08 = 02 00 00 1a`
- `0x22001b0c = 10 00 15 e3`
- `0x22001b10 = 07 00 86 10`
- `0x22001b14 = 67 02 00 1b`

Still-active supporting WTF patches:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Status:

- image prepared
- not run yet

## 2026-04-24 Result of the 0x22001ae0 tail-marker run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same DFU-resident reached-marker signature seen for the previous
  confirmed post-landing tail splits
- strongest current classification:
  - `MARKER_1AE0_REACHED`

Recovery status:

- manual clean-DFU reconfirmation still pending

## 2026-04-24 Final tail marker prep at 0x22001ae8

Prepared the final unresolved tail marker at:

- `0x22001ae8`
  - original:
    - `0d 03 00 eb`
  - replacement:
    - `1f 17 00 ea`
  - stub:
    - `0x2200776c`

Verified with the raw-body dumper only:

- `0x22001ae0 = 3a fb ff eb`
- `0x22001ae8 = 1f 17 00 ea`

Still-active supporting WTF patches:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Status:

- image prepared
- not run yet

## 2026-04-24 Result of the 0x22001ab0 tail-marker run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same DFU-resident reached-marker signature seen for the previous
  confirmed post-landing tail splits
- strongest current classification:
  - `MARKER_1AB0_REACHED`

Recovery status:

- manual clean-DFU reconfirmation still pending

## 2026-04-24 Tail marker prep after confirmed 0x22001ab0 reach

Prepared the next post-landing linear-tail marker at:

- `0x22001ae0`
  - original:
    - `3a fb ff eb`
  - replacement:
    - `21 17 00 ea`
  - stub:
    - `0x2200776c`

Verified with the raw-body dumper only:

- `0x22001a24 = 00 40 a0 e3`
- `0x22001a28 = 63 fb ff eb`
- `0x22001a4c = 2a 06 00 eb`
- `0x22001a78 = 32 ff 2f e1`
- `0x22001ab0 = 32 ff 2f e1`
- `0x22001ae0 = 21 17 00 ea`

Still-active supporting WTF patches:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Status:

- image prepared
- not run yet

## 2026-04-24 Result of the 0x22001a78 tail-marker run

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` still showed:
  - `05ac:1223`
- post-run `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same DFU-resident reached-marker signature seen for the previous
  confirmed post-landing tail splits
- strongest current classification:
  - `MARKER_1A78_REACHED`

Recovery status:

- manual clean-DFU reconfirmation still pending

## 2026-04-24 Tail marker prep after confirmed 0x22001a78 reach

Prepared the next post-landing linear-tail marker at:

- `0x22001ab0`
  - original:
    - `32 ff 2f e1`
  - replacement:
    - `28 17 00 ea`
  - stub:
    - `0x22007758`

Verified with the raw-body dumper only:

- `0x22001a24 = 00 40 a0 e3`
- `0x22001a28 = 63 fb ff eb`
- `0x22001a4c = 2a 06 00 eb`
- `0x22001a78 = 32 ff 2f e1`
- `0x22001ab0 = 28 17 00 ea`

Still-active supporting WTF patches:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Status:

- image prepared
- not run yet

## 2026-04-24 Late-gap branch/poll analysis prep

After the `[service + 0x68]` bypass at `0x22001938` still ended in
`CHAINLOAD_BLACKSCREEN`, the next unresolved late-gap split is the status
branch/poll gate at `0x220019c8`.

Recovered late-gap logic:

- `0x220019c8`
  - `beq 0x22001a24`
  - taken when `0x38c00040 & 3 == 0`
- `0x220019cc..0x220019d4`
  - poll loop on `0x38c0000c & 1`
  - loops while the bit stays clear

Prepared local Nano 3G-only marker:

- `0x220019c8`
  - `15 00 00 0a` -> `41 17 00 ea`
  - branches to:
    - `0x220076d4`
- `0x220076d4`
  - marker stub:
    - `0x22002138(11)`
    - loop forever

Kept active:

- readiness stub at `0x2200197c`
- loader callback stub at `0x220019ac`
- UART immediate-return patch at `0x22006558`
- `[service + 0x68]` bypass at `0x22001938`

Verification:

- fresh defanger output shows:
  - `0x1938 = 64 17 00 eb`
  - `0x19c8 = 41 17 00 ea`
  - `0x76d0 = 1e ff 2f e1`
  - `0x76d4 = 0b 00 a0 e3 ... 38 21 00 22`

Status:

- image prepared
- not run yet

## 2026-04-24 Hardware result for the `0x220019d8` post-poll marker

Tested local Nano 3G-only late-tail state:

- `0x220019d8`
  - `00 00 50 e3` -> `3d 17 00 ea`
- kept active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the first post-poll tail instruction at `0x220019d8` is reached
- the next unresolved split is later:
  - `0x220019dc`
  - then `0x22001a24`

Decision:

- **MARKER_19D8_REACHED**

## 2026-04-24 Landing-block marker verified ready

The older helper verification path was stale. The trustworthy local verification
path is now:

- `go run -a ./cmd/dump_n3g_defanged.go ...`
- inspect the raw dumped body with `xxd`

Verified state:

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

Decision:

- **MARKER_1A24_VERIFIED_READY**

## 2026-04-24 Hardware result for the `0x22001a24` landing-block marker

Tested local Nano 3G-only tail state:

- `0x220019d8`
  - restored:
    - `00 00 50 e3`
- `0x220019dc`
  - restored:
    - `10 00 00 1a`
- `0x22001a24`
  - `00 40 a0 e3` -> `34 17 00 ea`
- kept active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the landing block at `0x22001a24` is reached
- the unresolved blocker is later than that landing and before the still
  unreached tail at `0x22001ae8`

Decision:

- **MARKER_1A24_REACHED**

## 2026-04-24 Prepared post-landing marker at `0x22001a28`

Post-landing tail summary:

- `0x22001a24: mov r4, #0`
- `0x22001a28: bl 0x220007bc`
- `0x22001a4c: bl 0x220032fc`
- `0x22001a78: blx [service + 0x8c]` with ID `19`
- `0x22001a88: bl 0x220032fc`
- `0x22001ab0: blx [service + 0x8c]` with ID `33`
- `0x22001ae0: bl 0x220007d0`
- `0x22001ae8: bl 0x22002724`

Prepared local Nano 3G-only next split:

- `0x22001a24`
  - restored:
    - `00 40 a0 e3`
- `0x22001a28`
  - active marker:
    - `63 fb ff eb` -> `33 17 00 ea`
  - target:
    - `0x220076fc`

Raw-body verification:

- `0x1a24 = 00 40 a0 e3`
- `0x1a28 = 33 17 00 ea`
- USB string moved to `0x7710` so it no longer overlaps the marker stub

Status:

- image prepared
- verified ready
- not run yet

## 2026-04-24 Hardware result for the `0x22001a4c` deeper tail marker

Tested local Nano 3G-only tail state:

- `0x22001a28`
  - restored:
    - `63 fb ff eb`
- `0x22001a4c`
  - `2a 06 00 eb` -> `2b 17 00 ea`
- kept active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the deeper tail call at `0x22001a4c` is reached
- the unresolved blocker is later than that call and still before the
  previously unreached `0x22001ae8`

Decision:

- **MARKER_1A4C_REACHED**

## 2026-04-24 Hardware result for the `0x22001a28` post-landing marker

Tested local Nano 3G-only tail state:

- `0x22001a24`
  - restored:
    - `00 40 a0 e3`
- `0x22001a28`
  - `63 fb ff eb` -> `33 17 00 ea`
- kept active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the first post-landing call at `0x22001a28` is reached
- the unresolved blocker is later than that call and still before the
  previously unreached `0x22001ae8`

Decision:

- **MARKER_1A28_REACHED**

## 2026-04-24 Prepared deeper tail marker at `0x22001a4c`

Prepared local Nano 3G-only next split:

- `0x22001a28`
  - restored:
    - `63 fb ff eb`
- `0x22001a4c`
  - active marker:
    - `2a 06 00 eb` -> `2b 17 00 ea`
  - target:
    - `0x22007700`

Raw-body verification:

- `0x1a24 = 00 40 a0 e3`
- `0x1a28 = 63 fb ff eb`
- `0x1a4c = 2b 17 00 ea`
- USB string no longer overlaps the active stub region

Status:

- image prepared
- verified ready
- not run yet

## 2026-04-24 Hardware result for the `0x220019dc` final-tail marker

Tested local Nano 3G-only tail state:

- `0x220019d8`
  - restored:
    - `00 00 50 e3`
- `0x220019dc`
  - `10 00 00 1a` -> `41 17 00 ea`
- kept active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the final conditional branch at `0x220019dc` is reached
- the next unresolved split is later:
  - `0x22001a24`

Decision:

- **MARKER_19DC_REACHED**

## 2026-04-24 Hardware result for the `0x220019d0` AES/status completion emulation

Tested local Nano 3G-only late-gap state:

- `0x220019d0`
  - `01 00 11 e3` -> `01 10 b0 e3`
- `0x220019d4`
  - restored to original:
    - `fc ff ff 0a`
- kept active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`
- screen stayed black for the full 90-second window

Interpretation:

- emulating the AES/status completion condition at `0x220019d0` is still not
  sufficient to produce visible boot progress
- the remaining blocker is later or orthogonal to this single late-gap status
  bit test

Decision:

- **CHAINLOAD_BLACKSCREEN**

## 2026-04-24 Prepared post-poll marker at `0x220019d8`

The next local Nano 3G-only split moves just beyond the late-gap AES/status
region.

Prepared state:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - completion-condition emulation at `0x220019d0`
- keep original:
  - `0x220019d4`
    - `fc ff ff 0a`
- active marker:
  - `0x220019d8`
  - `00 00 50 e3` -> `3d 17 00 ea`
  - branches to:
    - `0x220076d4`

Prepared but inactive:

- `0x220019dc` -> `0x220076e8`
- `0x22001a24` -> `0x220076fc`

Verification:

- fresh local-module defanger output shows:
  - `0x19d8 = 3d 17 00 ea`
  - `0x19d0 = 01 10 b0 e3`
  - `0x19d4 = fc ff ff 0a`

Status:

- image prepared
- not run yet

## 2026-04-24 Hardware result for the `0x220019d4` poll-loop bypass

Tested local Nano 3G-only late-gap state:

- `0x220019c8`
  - restored to original:
    - `15 00 00 0a`
- `0x220019d4`
  - `fc ff ff 0a` -> `00 00 a0 e1`
- kept active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`
- screen stayed black for the full 90-second window

Interpretation:

- the `0x220019d4` late-gap poll-loop back-edge is not the only remaining
  blocker
- even with the loop removed, the handoff still does not produce visible boot
  progress

Decision:

- **CHAINLOAD_BLACKSCREEN**

## 2026-04-24 Prepared `0x220019d0` AES/status completion emulation

The late-gap wait on `0x38c0000c & 1` appears to be part of the AES/decrypt
completion path, and bypassing only the loop back-edge was not sufficient.

Prepared local Nano 3G-only change:

- `0x220019d0`
  - original:
    - `01 00 11 e3`
  - replacement:
    - `01 10 b0 e3`
  - effect:
    - `tst r1, #1` -> `movs r1, #1`
- `0x220019d4`
  - restored to original:
    - `fc ff ff 0a`

Meaning:

- still perform the original read from `0x38c0000c`
- force the tested ready/completion condition to look satisfied
- keep the original branch structure instead of writing guessed MMIO state

Kept active:

- readiness stub at `0x2200197c`
- loader callback stub at `0x220019ac`
- UART immediate-return patch at `0x22006558`
- `[service + 0x68]` bypass at `0x22001938`

Verification:

- fresh local-module defanger output shows:
  - `0x19c8 = 15 00 00 0a`
  - `0x19d0 = 01 10 b0 e3`
  - `0x19d4 = fc ff ff 0a`

Status:

- image prepared
- not run yet

## 2026-04-24 Hardware result for the `0x220019c8` late-gap marker

Tested local Nano 3G-only late-gap state:

- `0x220019c8`
  - `15 00 00 0a` -> `41 17 00 ea`
  - branches to:
    - `0x220076d4`
- kept active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the `0x220019c8` branch gate is reached
- the remaining unresolved blocker is later than that branch point
- the next late-gap candidate remains:
  - `0x220019cc..0x220019d4`

Decision:

- **MARKER_19C8_REACHED**

## 2026-04-24 Prepared `0x220019d4` poll-loop bypass

After proving reachability of the late-gap branch at `0x220019c8`, the next
prepared local Nano 3G-only test targets only the poll-loop back-edge:

- `0x220019cc`
  - `ldr r1, [0x38c0000c]`
- `0x220019d0`
  - `tst r1, #1`
- `0x220019d4`
  - original:
    - `fc ff ff 0a`
  - replacement:
    - `00 00 a0 e1`

Meaning:

- keep the AES/status read and bit test
- remove only the infinite `beq 0x220019cc` back-edge
- let WTF fall through once if the bit is still clear

Kept active:

- readiness stub at `0x2200197c`
- loader callback stub at `0x220019ac`
- UART immediate-return patch at `0x22006558`
- `[service + 0x68]` bypass at `0x22001938`

Verification:

- fresh local-module defanger output shows:
  - `0x19c8 = 15 00 00 0a`
  - `0x19d4 = 00 00 a0 e1`

Status:

- image prepared
- not run yet

## 2026-04-24 Forward marker at 0x22001938

Prepared bytes:

- `0x22001938`
  - original:
    - `31 ff 2f e1`
  - replacement:
    - `64 17 00 ea`
  - branch target:
    - `0x220076d0`
- `0x220076d0`
  - stub bytes begin:
    - `0a 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`

Observed result:

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- strongest current classification:
  - `MARKER_1938_REACHED`
- this moves the unresolved blocker later than:
  - the `[service + 0x68]` callback at `0x22001938`

Recovery status:

- no separate manual recovery was required to identify the reached-marker host
  signature

## 2026-04-24 Final gap reduction and prepared bypass at `0x22001938`

Current linear reduction:

- `0x22001938`
  - first unresolved call in the remaining gap
- `0x2200197c`
  - readiness stub already forces nonzero return
- `0x22001994`
  - static fallthrough on current path because `r5 = 0x1d`
- `0x220019ac`
  - loader stub already active
- `0x220019c8`
  - later status branch
- `0x220019cc..0x220019d4`
  - later poll loop
- `0x22001ae8`
  - still the earlier not-reached tail marker

Prepared fix:

- `0x22001938`
  - original:
    - `31 ff 2f e1`
  - replacement:
    - `64 17 00 eb`
  - meaning:
    - `bl 0x220076d0`
- `0x220076d0`
  - bytes:
    - `1e ff 2f e1`
  - meaning:
    - `bx lr`

Verification from a clean-cache local defanger run:

- `0x1938`
  - `64 17 00 eb`
- `0x76d0`
  - `1e ff 2f e1`

Interpretation:

- strongest current blocker:
  - the unresolved `[service + 0x68]` callback at `0x22001938`
- strongest current next fix:
  - bypass that callback locally and re-test before touching later tail logic

## 2026-04-24 Hardware result for the `0x22001938` bypass

Tested bytes:

- `0x22001938`
  - `31 ff 2f e1` -> `64 17 00 eb`
- `0x220076d0`
  - `1e ff 2f e1`

Observed result:

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`
- screen observation:
  - black screen for 90 seconds
  - no Apple logo
  - no backlight
  - no flicker
  - no patched text
  - no stock UI
  - no visible reset loop

Interpretation:

- strongest current classification:
  - `CHAINLOAD_BLACKSCREEN`
- removing the first unresolved callback at `0x22001938` is not enough to
  produce visible boot progress

## 2026-04-24 Middle-path marker at `0x22001768` tested on hardware

This run moved the middle-path split one step later inside `0x22001698`.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x22001768`
  - `6f ff ff eb` -> `ec 15 00 ea`
  - branches to Marker B at:
    - `0x22006f20`
- Marker B:
  - calls `0x22002138(3)`
  - loops forever
- `0x22001758`
  - restored to original `blx r1`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the host-visible end state changed in the same way as the already reached
  `0x22001758` marker
- strongest current classification:
  - `MARKER_1768_REACHED`
- next narrowing target should move forward to:
  - `0x2200177c`
  - then `0x22001788`

## 2026-04-24 Middle-path marker at `0x2200177c` tested on hardware

This run moved the middle-path split one step later inside `0x22001698`.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x2200177c`
  - `ff 12 00 eb` -> `ef 15 00 ea`
  - branches to Marker C at:
    - `0x22006f40`
- Marker C:
  - calls `0x22002138(4)`
  - loops forever
- `0x22001768`
  - restored to original `bl 0x2200152c`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the host-visible end state changed in the same way as the already reached
  `0x22001758` and `0x22001768` markers
- strongest current classification:
  - `MARKER_177C_REACHED`
- next narrowing target should move forward to:
  - `0x22001788`

## 2026-04-24 Middle-path marker at `0x22001788` tested on hardware

This run moved the middle-path split one step later inside `0x22001698`.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x22001788`
  - `28 13 00 eb` -> `f4 15 00 ea`
  - branches to Marker D at:
    - `0x22006f60`
- Marker D:
  - calls `0x22002138(5)`
  - loops forever
- `0x2200177c`
  - restored to original `bl 0x22006380`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the host-visible end state changed in the same way as the already reached
  `0x22001758`, `0x22001768`, and `0x2200177c` markers
- strongest current classification:
  - `MARKER_1788_REACHED`
- this means the unresolved blocker is later than:
  - `0x22006430`

## 2026-04-24 Post-`0x1788` marker at `0x22001798` tested on hardware

This run moved the split to the earliest meaningful post-`0x1788` call.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x22001798`
  - `6e 13 00 eb` -> `f8 15 00 ea`
  - branches to Marker E at:
    - `0x22006f80`
- Marker E:
  - calls `0x22002138(6)`
  - loops forever
- `0x22001788`
  - restored to original `bl 0x22006430`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the host-visible end state changed in the same way as the already reached
  earlier markers
- strongest current classification:
  - `MARKER_1798_REACHED`
- the unresolved blocker is later than:
  - `0x22001798`

## 2026-04-24 Attempt to run the `0x220017a0` marker was aborted

This was not a valid hardware result.

Before launch:

- USB still showed the Nano in DFU (`05ac:1223`)
- but `mks5lboot --dfuscan` returned `LIBUSB_ERROR_OTHER`
- `wInd3x cfw run` then failed before the exploit with a string-descriptor
  I/O error

Conclusion:

- no `MARKER_17A0_*` classification yet
- the device needs a fresh DFU re-entry before retrying the single run
- Nano 2G remains a reference for expected clean handoff behavior, not a
  byte-for-byte handoff template

## 2026-04-24 Marker at `0x220017a0` tested on hardware

This run retried the earliest forward split after `0x22001798` from a clean DFU
session.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x220017a0`
  - `ed 08 00 eb` -> `fe 15 00 ea`
  - branches to Marker F at:
    - `0x22006fa0`
- Marker F:
  - calls `0x22002138(7)`
  - loops forever
- `0x22001798`
  - restored to original `bl 0x22006558`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the host-visible end state changed in the same way as the already reached
  earlier markers
- strongest current classification:
  - `MARKER_17A0_REACHED`
- Nano 2G remains a clean-handoff comparison reference only

## 2026-04-24 Marker at `0x2200181c` tested on hardware

This run moved the forward split to the first post-`0x17a0` callback call.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x2200181c`
  - `b6 06 00 eb` -> `e7 15 00 ea`
  - branches to Marker G at:
    - `0x22006fc0`
- Marker G:
  - calls `0x22002138(8)`
  - loops forever
- `0x220017a0`
  - restored to original `bl 0x22003b5c`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the host-visible end state changed in the same way as the already reached
  earlier markers
- strongest current classification:
  - `MARKER_181C_REACHED`
- the unresolved blocker is later than:
  - `0x220032fc`

## 2026-04-24 Marker at `0x2200182c` tested on hardware

This run moved the forward split to the next callback dispatch after
`0x2200181c`.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x2200182c`
  - `31 ff 2f e1` -> `eb 15 00 ea`
  - branches to Marker H at:
    - `0x22006fe0`
- Marker H:
  - calls `0x22002138(9)`
  - loops forever
- `0x2200181c`
  - restored to original `bl 0x220032fc`

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the host-visible end state changed in the same way as the already reached
  earlier markers
- strongest current classification:
  - `MARKER_182C_REACHED`
- the unresolved blocker is later than:
  - `0x2200182c`

## 2026-04-24 OSOS scheduler-probe image prepared

Goal:

- stop relying on blind display/audio patches
- prove whether post-handoff OSOS reaches a stable always-reached loop

Recovered startup path:

- `0x220039c4` performs the early init sequence
- control later exits through:
  - `0x22003aa4: b 0x22003af8`

Recovered always-reached stable loop:

- runtime:
  - `0x22003af8..0x22003b1c`
- body:
  - repeated `nop`
  - one cache/idle style `mcr`
  - final branch back to `0x22003af8`

Related callback wiring:

- `0x22003ac4` arms a recurring callback
- literal target:
  - `0x22003af4`
  - `b 0x22001d48`
- `0x22001d48` appears timer/tick-like, but the first proof target remains the
  unconditional loop head at `0x22003af8`

Prepared image:

- `/tmp/n3g-osos-work/n3g-osos-scheduler-probe.dfu`
- body:
  - `/tmp/n3g-osos-work/lcd-osos-scheduler-probe.bin`

Patch:

- `0x22003af8`
  - original:
    - `00 00 a0 e1`
  - replacement:
    - `3c 14 00 ea`
  - effect:
    - branch to local stub at:
      - `0x22008bf0`

Local stub at `0x22008bf0`:

- `mov r1, #0x3c800000`
- `mov r0, #0x00100000`
- `str r0, [r1]`
- `b .`

Reasoning:

- `0x22003af8` is the earliest stable loop reached after the recovered init
  path
- forcing the known watchdog reboot write there gives a proof signal that does
  not depend on backlight, LCD, or piezo
- this is lower risk than another guessed display/backlight hook

Verification:

- `0x22003af8` now disassembles as:
  - `ea00143c  b 0x22008bf0`
- `0x22008bf0` disassembles as the watchdog-reset stub above

Status:

- prepared only
- no hardware run performed yet

Classification:

- **HOOK_POINT_FOUND**

## 2026-04-24 Scheduler-probe hardware run

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-scheduler-probe.dfu`

Run conditions:

- cached defanged WTF removed before run
- DFU confirmed clean:
  - `05ac:1223`
  - state `2`

Host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- after the run:
  - no Nano present in `lsusb`
  - `mks5lboot --dfuscan` found no DFU device

Device observation:

- black screen / no change for the full 60-second window
- no immediate reboot
- no delayed reboot
- no repeated reboot loop
- no Apple logo
- no flicker
- no backlight
- no audible sound

Interpretation:

- the scheduler probe did not trigger any observable reset behavior
- current classification:
  - `SCHEDULER_PROBE_NOT_TRIGGERED`

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 Middle-path marker prepared at `0x22001758`

Goal:

- isolate the earliest unresolved middle-path call inside `0x22001698`

Active marker:

- `0x22001758`
  - original:
    - `30 ff 2f e1`
  - replacement:
    - `e8 15 00 ea`
  - effect:
    - branch to:
      - `0x22006f00`

Marker stub:

- `0x22006f00`
- bytes:
  - `02 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- disassembly:
  - `mov r0, #2`
  - `ldr r3, =0x22002138`
  - `blx r3`
  - `b .`

Restored/kept:

- `0x22001ae8`
  - restored to original:
    - `eb00030d`
- readiness stub:
  - still active
- loader stub:
  - still active
- UART return patch:
  - still active

Prepared inactive follow-ups:

- `0x22001768`
  - `6f ff ff eb` -> `ec 15 00 ea`
- `0x2200177c`
  - `ff 12 00 eb` -> `ef 15 00 ea`
- `0x22001788`
  - `28 13 00 eb` -> `f4 15 00 ea`

Offline verification:

- `0x22001758`
  - `ea0015e8  b 0x22006f00`
- `0x22006f00`
  - marker stub present as expected
- `0x22001ae8`
  - restored:
    - `eb00030d  bl 0x22002724`

Status:

- local `/tmp/wInd3x/wInd3x` rebuilt
- cached defanged WTF should be cleared before the next hardware run
- no hardware run performed yet

Classification:

- **MARKER_1758_PREPARED**

## 2026-04-24 Middle-path marker at `0x22001758` tested on hardware

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker:

- `0x22001758`
  - `30 ff 2f e1` -> `e8 15 00 ea`
  - branch to marker stub at:
    - `0x22006f00`

Run conditions:

- cached defanged WTF removed before run
- DFU confirmed clean:
  - `05ac:1223`
  - state `2`

Host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- unlike previous baseline runs, the Nano stayed present on USB as:
  - `05ac:1223`
  - DFU mode
- repeated `mks5lboot --dfuscan` attempts hit:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- the marker changed host behavior materially
- current classification:
  - `MARKER_1758_REACHED`

Implication:

- the blocker is later than:
  - `0x22001758`
- next split should move forward to:
  - `0x22001768`

Recovery status:

- device is already back in DFU on USB

## 2026-04-24 Correct WTF execute-path mapping

Important correction:

- the chainload transfer path belongs to the WTF image, not the OSOS body
- traced against:
  - `/tmp/n3g-wtf-decrypted.body.bin`

Active handoff entry:

- `0x22002fe4 -> 0x22001698`

registers on entry:

- `r0 = 0x1d`
- `r1 = 0x08000000`
- `r2 = 0x00f80000`

`0x22001698` preserves:

- `r6 = 0x08000000`
- `r7 = 0x00f80000` initially, later reduced to:
  - `0x800`
- `r5 = 0x1d`

Final execute gate in WTF:

- `0x22001b04: cmp r4, #0`
- `0x22001b0c: tst r5, #16`
- `0x22001b10: addne r0, r6, r7`
- `0x22001b14: blne 0x220024b8`

Execute wrapper:

- `0x220024b8: push {r4, lr}`
- `0x220024bc: mov r4, r0`
- `0x220024c0: mov r0, #0`
- `0x220024c4: bl 0x22002138`
- `0x220024c8: blx r4`
- `0x220024d0: mov r0, #2`
- `0x220024d4: b 0x22002138`

Actual intended OSOS jump:

- `0x220024c8: blx r4`

Target on the active path:

- `r0 = r6 + r7 = 0x08000000 + 0x800`
- `r4 = 0x08000800`

State analysis:

- ARM state
- even target address
- no Thumb mismatch
- valid stack frame in caller and wrapper

Current failure classification:

- `EXECUTE_NEVER_CALLED`

Reason:

- earlier hardware proof did not show the pre-execute gate being reached
- no later result has produced evidence that `0x22001b14` or `0x220024b8` is
  actually taken

Fix status:

- no minimal jump-forcing patch prepared
- forcing `0x22001b14` would be speculative while earlier WTF state is still
  unresolved

Classification:

- **EXECUTE_PATH_MAPPED**

## 2026-04-24 Final execute-gate reduction in current defanged WTF

Important current-build fact:

- `0x22006558` is already patched to immediate return in the active local
  defanger

So the last unresolved call before:

- `0x22001b04`

is now:

- `0x22001ae8: bl 0x22002724`

Static conditions at the final gate:

- `r4 = 0` at:
  - `0x22001a24`
- no direct writes to `r4` appear before:
  - `0x22001b04`
- `r5 = 0x1d`
  - so `r5 & 0x10` is set at:
    - `0x22001b0c`

Prepared marker in local defanger:

- callsite:
  - `0x22001ae8`
  - original:
    - `0d 03 00 eb`
  - replacement:
    - `94 14 00 ea`
  - effect:
    - branch to marker stub at:
      - `0x22006d40`
- marker stub:
  - `mov r0, #4`
  - `ldr r3, =0x22002138`
  - `blx r3`
  - `b .`

Purpose:

- prove reachability of the final unresolved tail immediately before the execute
  gate
- avoid skipping the earlier readiness/loader state that current chainload
  depends on

Status:

- local `/tmp/wInd3x/wInd3x` rebuilt
- no hardware run performed yet

Classification:

- **FINAL_GATE_MARKER_PREPARED**

## 2026-04-24 Final-tail marker at `0x22001ae8` tested on hardware

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker:

- `0x22001ae8`
  - `0d 03 00 eb` -> `94 14 00 ea`
  - branch to marker stub at:
    - `0x22006d40`

Run conditions:

- cached defanged WTF removed before run
- DFU confirmed clean:
  - `05ac:1223`
  - state `2`

Host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- after the run:
  - no Nano present in `lsusb`
  - `mks5lboot --dfuscan` found no DFU device

Interpretation:

- same post-WTF disappearance pattern as the current readiness-stub baseline
- no evidence that the final-tail marker at `0x22001ae8` was reached
- current classification:
  - `MARKER_1AE8_NOT_REACHED`

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 OSOS entry-loop probe prepared

Reason:

- the scheduler watchdog probe at `0x22003af8` did not trigger a reset
- next proof should avoid display, audio, and watchdog assumptions

Corrected startup mapping:

- `0x22000800` is not the one-time entry point
- actual startup shell is:
  - `0x22000000 -> 0x22008808 -> 0x22008814 -> 0x220039c4`

Prepared repeated-runtime candidates for later work:

- `0x22003ac4`
  - installs a recurring callback path to:
    - `0x22001d48`
- `0x22003b28`
  - enters RTXC-style service dispatch through:
    - `0x2200360c`

Prepared first structural entry probe:

- image:
  - `/tmp/n3g-osos-work/n3g-osos-entry-loop-probe.dfu`
- body:
  - `/tmp/n3g-osos-work/lcd-osos-entry-loop-probe.bin`

Patch:

- runtime:
  - `0x22008814`
- body offset:
  - `0x8814`
- original:
  - `6a ec ff ea`
  - `b 0x220039c4`
- replacement:
  - `fe ff ff ea`
  - `b 0x22008814`

Verification:

- `0x22008808`
  - still performs the stack setup
- `0x22008814`
  - now self-loops

Interpretation goal:

- if hardware behavior changes relative to the current post-handoff baseline,
  the image entry shell is being reached
- if behavior is unchanged, the result is still somewhat ambiguous because USB
  disappearance may happen before this point

Status:

- prepared only
- no hardware run performed yet

Classification:

- **OSOS_ENTRY_PROBE_PREPARED**

## 2026-04-24 OSOS entry-loop probe hardware run

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-entry-loop-probe.dfu`

Run conditions:

- cached defanged WTF removed before run
- DFU confirmed clean:
  - `05ac:1223`
  - state `2`

Host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- after the run:
  - no Nano present in `lsusb`
  - `mks5lboot --dfuscan` found no DFU device

Interpretation:

- same post-WTF disappearance pattern as the non-entry-probe baseline
- current classification:
  - `ENTRY_LOOP_NOT_REACHED`
- caveat:
  - this remains ambiguous because USB disappearance may happen before the
    probe point at `0x22008814`

Recovery status:

- pending manual DFU recovery confirmation

## 2026-04-24 OSOS backlight patch prepared

- problem:
  - after the readiness-stub fix, Nano 3G no longer stayed in WTF, but the
    runtime visibility patch still showed nothing on screen
- conclusion:
  - RetailOS likely progresses far enough that the next missing piece is an
    earlier display/backlight enable path

Selected Apple path:

- `0x22005640`
  - compact Apple "visibility on" wrapper
  - calls:
    - `0x2200374c`
    - `0x22003774`
    - `0x220073b4`
    - `0x22007610`

Selected hook:

- startup callsite:
  - runtime `0x22003a9c`
  - body offset `0x3a9c`
- original bytes:
  - `bd 00 00 eb`
- replacement bytes:
  - `43 14 00 eb`

Local stub:

- runtime `0x22008bb0`
- body offset `0x8bb0`
- bytes:
  - `04 e0 2d e5`
  - `77 ec ff eb`
  - `a0 f2 ff eb`
  - `04 f0 9d e4`
- meaning:
  - preserve original `bl 0x22003d98`
  - then call `0x22005640`
  - then return to normal startup flow

Prepared files:

- helper source:
  - `/tmp/wInd3x/cmd/patch_n3g_backlight_signal.go`
- patched DFU:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-backlight-n3g.dfu`
- patched body:
  - `/tmp/n3g-osos-work/lcd-osos-backlight-n3g.bin`

Status:

- **PATCH_READY**

## 2026-04-24 Hardware retest of OSOS backlight patch

- tested:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-backlight-n3g.dfu`
- host-side:
  - exploit and defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF mode
  - Nano disappeared from USB instead of re-enumerating as `05ac:1242`
- screen-side:
  - full black screen for 90 seconds
  - no logo, no backlight, no flicker, no text, no visible reset loop

Classification:

- **RUNTIME_BLACKSCREEN**

## 2026-04-24 Nano 3G readiness callback bypass prepared

The BootROM readiness callback at `0x200036c8` is now traced well enough to
prepare a minimal local bypass in defanged WTF.

Observed callback structure:

- loop head:
  - `0x200036d4: bl 0x20009e20`
- primary state source:
  - `*0x2203fff8`
- readiness tests:
  - `[state + 0x2c]`
  - `[state + 0x738 + 0x36]`
  - `[state + 0x738 + 0x37]`
  - `[state + 0x04]`
- helper call on one ready path:
  - `0x200034a0`
- common return helper:
  - `0x2000a26c`
- return value:
  - `r0 = 1` on success-side path
  - `r0 = 0` on one non-ready return path

Direct callback side effects observed on return:

- `*0x2203fff8 = (*0x2203fff8)->next`
- `*0x2203fffc = (*0x2203fffc + 0x2000)->0x720`

Prepared local Nano 3G patch in `/tmp/wInd3x/pkg/cfw/defang_wtf.go`:

- replace:
  - `0x2200197c`
  - `30 ff 2f e1`
  - `blx r0`
- with:
  - `1f 15 00 eb`
  - `bl 0x22006e00`

Local stub at `0x22006e00`:

- preserves:
  - `r4`
  - `lr`
- replays only the two observed scratch-global updates
- returns:
  - `r0 = 1`
- then resumes original WTF flow at:
  - `0x22001980`

Verification:

- rebuilt:
  - `/tmp/wInd3x/wInd3x`
- regenerated:
  - `/tmp/n3g-wtf-defanged-check.bin`
- verified:
  - `0x2200197c: eb00151f`
  - `0x22001980: e3500000`
  - `0x22006e00: e92d4010`
  - `0x22006e28: e3a00001`
  - `0x22006e2c: e8bd8010`
- cleared cached defanged WTF:
  - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`

Current decision:

- **READINESS_STUB_PREPARED**

Run status:

- no new hardware run yet with this readiness-stub build

## 2026-04-24 Readiness-stub hardware run result

One controlled Nano 3G hardware test was performed with the local readiness
stub active.

Pre-run:

- confirmed clean DFU:
  - `05ac:1223`
  - state `2`

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed host-side result:

- `wInd3x` completed:
  - exploit
  - defanged WTF upload
- then failed while waiting for WTF mode:
  - `Error: device did not switch to WTF mode: context deadline exceeded`
- after that the Nano was absent from USB:
  - not `05ac:1242`
  - not DFU

Recovery:

- after manual reboot, DFU recovery was confirmed:
  - `05ac:1223`
  - state `2`

Interpretation:

- the old readiness blocker is likely bypassed, because the device no longer
  returns to the previous stable WTF-failure state
- however, no direct screen observation was captured in this session, so this
  is not yet enough to claim visible RetailOS success

Current classification:

- **CHAINLOAD_BOOT_PROGRESS**

## 2026-04-24 Callback post-call marker prepared at `0x22001980`

The next isolation step switches from callback entry to callback return.

Active marker for the next run:

- `0x22001980`
  - original:
    - `00 00 50 e3`
    - `cmp r0, #0`
  - replacement:
    - `f6 14 00 ea`
    - `b 0x22006d60`
- marker target:
  - `0x22006d60`
  - calls `0x22002138(3)`
  - loops forever

Restored pre-call site:

- `0x2200197c`
  - `30 ff 2f e1`
  - `blx r0`

Restored later sites:

- `0x22001988`
  - `26 00 00 0a`
- `0x22001994`
  - `22 00 00 0a`
- `0x22001998`
  - `00 00 99 e5`
- `0x220019ac`
  - `c6 14 00 eb`
- `0x220019b0`
  - `00 00 50 e3`
- `0x22001a24`
  - `00 40 a0 e3`
- `0x22001b14`
  - `67 02 00 1b`

Verification and cache state:

- local `/tmp/wInd3x/wInd3x` rebuilt
- `/tmp/n3g-wtf-defanged-check.bin` regenerated
- verified:
  - `0x2200197c: e12fff30`
  - `0x22001980: ea0014f6`
  - `0x22001988: 0a000026`
- cached defanged WTF removed again
- no hardware run performed yet

Expected next classifications:

- `POSTCALL_REACHED`
- `POSTCALL_NOT_REACHED`

## 2026-04-24 Callback post-call `0x22001980` marker tested once on hardware

The callback-return marker was tested once with the callback entry site restored.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`
- `lsusb`
  - Nano 3G present in DFU mode

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x22001980`
  - `00 00 50 e3` -> `f6 14 00 ea`
  - branches to Marker B at:
    - `0x22006d60`
- Marker B:
  - calls `0x22002138(3)`
  - loops forever
- callback entry site:
  - remained restored to:
    - `0x2200197c = blx r0`

Observed result:

- exploit completed
- defanged WTF upload completed
- firmware upload started
- after that the host returned to the older repeated:
  - `handle_events: error: libusb: interrupted [code -10]`
  pattern
- post-run `lsusb` showed:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- the split pair now resolves the callback path:
  - pre-call marker likely reached
  - post-call marker not reached
- strongest current classification:
  - `POSTCALL_NOT_REACHED`
  - `CALLBACK_ENTERED_NO_RETURN`

Recovery status:

- manual recovery succeeded
- device is back in clean DFU

## 2026-04-24 Callback pre-call marker prepared at `0x2200197c`

The next isolation step moves the active marker onto the readiness callback
callsite itself.

Active marker for the next run:

- `0x2200197c`
  - original:
    - `30 ff 2f e1`
    - `blx r0`
  - replacement:
    - `ef 14 00 ea`
    - `b 0x22006d40`
- marker target:
  - `0x22006d40`
  - calls `0x22002138(2)`
  - loops forever

Prepared but inactive follow-up:

- `0x22001980`
  - original:
    - `00 00 50 e3`
    - `cmp r0, #0`
  - prepared replacement:
    - `f6 14 00 ea`
    - `b 0x22006d60`

Restored later sites:

- `0x22001988`
  - `26 00 00 0a`
- `0x22001994`
  - `22 00 00 0a`
- `0x22001998`
  - `00 00 99 e5`
- `0x220019ac`
  - `c6 14 00 eb`
- `0x220019b0`
  - `00 00 50 e3`
- `0x22001a24`
  - `00 40 a0 e3`
- `0x22001b14`
  - `67 02 00 1b`

Verification and cache state:

- local `/tmp/wInd3x/wInd3x` rebuilt
- `/tmp/n3g-wtf-defanged-check.bin` regenerated
- verified:
  - `0x2200197c: ea0014ef`
  - `0x22001980: e3500000`
  - `0x22001988: 0a000026`
- cached defanged WTF removed again
- no hardware run performed yet

Expected next classifications:

- `PRECALL_NOT_REACHED`
- `PRECALL_REACHED_POSTCALL_PENDING`

## 2026-04-24 Callback pre-call `0x2200197c` marker tested once on hardware

The readiness-callback pre-call marker was tested once with no RetailOS changes
and with the post-call marker still inactive.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`
- `lsusb`
  - Nano 3G present in DFU mode

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x2200197c`
  - `30 ff 2f e1` -> `ef 14 00 ea`
  - branches to Marker A at:
    - `0x22006d40`
- Marker A:
  - calls `0x22002138(2)`
  - loops forever
- post-call marker at `0x22001980`:
  - remained inactive
- later checkpoints:
  - remained restored/original

Observed result:

- exploit completed
- defanged WTF upload completed
- firmware upload started
- unlike earlier “not reached” runs, the host log then became quiet instead of
  continuing to print repeated:
  - `handle_events: error: libusb: interrupted [code -10]`
- post-run `lsusb` showed:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- this is the first run with a materially different host-side outcome
- strongest current classification:
  - `PRECALL_REACHED_POSTCALL_PENDING`
- next narrowing target should be the prepared post-call split at:
  - `0x22001980`

Repo note:

- active hardware-test code changes remain in:
  - `/tmp/wInd3x`
- the session notes remain in:
  - `RockBox_Personal-master/docs/porting`

Recovery status:

- manual recovery succeeded
- device is back in clean DFU

## 2026-04-24 Callback-result marker prepared at `0x22001988`

The next isolation step moves the active marker earlier than the predecessor
edge and directly onto the callback-result branch.

Active marker for the next run:

- `0x22001988`
  - original:
    - `26 00 00 0a`
    - `beq 0x22001a28`
  - replacement:
    - `fc 14 00 ea`
    - `b 0x22006d80`
- marker target:
  - `0x22006d80`
  - calls `0x22002138(4)`
  - loops forever

Prepared but inactive follow-up:

- `0x2200198c`
  - original:
    - `08 00 15 e3`
  - prepared replacement:
    - `f3 14 00 ea`
    - `b 0x22006d60`

Restored sites:

- `0x22001994`
  - `22 00 00 0a`
- `0x22001998`
  - `00 00 99 e5`
- `0x220019ac`
  - `c6 14 00 eb`
- `0x220019b0`
  - `00 00 50 e3`
- `0x22001a24`
  - `00 40 a0 e3`
- `0x22001b14`
  - `67 02 00 1b`

Verification and cache state:

- local `/tmp/wInd3x/wInd3x` rebuilt
- `/tmp/n3g-wtf-defanged-check.bin` regenerated
- verified:
  - `0x22001988: ea0014fc`
  - `0x22001994: 0a000022`
  - `0x220019ac: eb0014c6`
- cached defanged WTF removed again
- no hardware run performed yet

Expected next classification:

- `MARKER_1988_REACHED`
  - if the callback-result branch is reached
- `MARKER_1988_NOT_REACHED`
  - if the same no-marker WTF path remains

## 2026-04-24 Callback-result `0x22001988` marker tested once on hardware

The callback-result branch marker was tested once with no RetailOS changes and
no activation of the `0x2200198c` follow-up marker.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`
- `lsusb`
  - Nano 3G present in DFU mode

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x22001988`
  - `26 00 00 0a` -> `fc 14 00 ea`
  - branches to Marker C at:
    - `0x22006d80`
- Marker C:
  - calls `0x22002138(4)`
  - loops forever
- `0x2200198c` follow-up marker:
  - remained inactive
- later checkpoints:
  - remained restored/original

Observed result:

- exploit completed
- defanged WTF upload completed
- firmware upload started
- host remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`
- post-run `lsusb` showed:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- no host-visible evidence that control reached the `0x22001988` marker
- strongest current classification:
  - `MARKER_1988_NOT_REACHED`
- next narrowing target should move earlier than the callback-result branch

Recovery status:

- manual recovery succeeded
- device is back in clean DFU

## 2026-04-24 Predecessor-path marker prepared at `0x22001994`

Tracing backward from `0x22001998` showed the real predecessor edge:

- `0x2200197c`
  - `blx [service + 0x6c]`
- `0x22001980`
  - compare callback result against zero
- `0x22001988`
  - branch to failure if callback returned zero
- `0x2200198c`
  - test `r5 & 0x8`
- `0x22001990`
  - `moveq r7, #0`
- `0x22001994`
  - original conditional branch to `0x22001a24`
- `0x22001998`
  - loader setup block

Important correction:

- the old `0x1990/0x1994` patch made `0x22001998` unreachable
- so the previous `0x22001998` marker was not on a live edge

Active marker for the next run:

- `0x22001994`
  - original:
    - `22 00 00 0a`
  - replacement:
    - `e9 14 00 ea`
    - `b 0x22006d40`
- marker target:
  - `0x22006d40`
  - calls `0x22002138(2)`
  - loops forever

Prepared but inactive follow-ups:

- `0x2200198c`
  - original:
    - `08 00 15 e3`
  - prepared replacement:
    - `f3 14 00 ea`

- `0x22001988`
  - original:
    - `26 00 00 0a`
  - prepared replacement:
    - `fc 14 00 ea`

Restored sites:

- `0x22001990`
  - `00 70 a0 03`
- `0x22001998`
  - `00 00 99 e5`
- `0x220019ac`
  - `c6 14 00 eb`
- `0x220019b0`
  - `00 00 50 e3`
- `0x22001a24`
  - `00 40 a0 e3`
- `0x22001b14`
  - `67 02 00 1b`

Verification and cache state:

- local `/tmp/wInd3x/wInd3x` rebuilt
- `/tmp/n3g-wtf-defanged-check.bin` regenerated
- verified:
  - `0x22001990: 03 a0 70 00`
  - `0x22001994: e9 14 00 ea`
  - `0x22001998: 00 00 99 e5`
- cached defanged WTF removed again
- no hardware run performed yet

Current classification:

- **PREDECESSOR_MARKER_PREPARED**

## 2026-04-24 Predecessor `0x22001994` marker tested once on hardware

The real predecessor-edge marker was tested once with no RetailOS changes and
no activation of the `0x2200198c` / `0x22001988` follow-up markers.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`
- `lsusb`
  - Nano 3G present in DFU mode

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x22001994`
  - `22 00 00 0a` -> `e9 14 00 ea`
  - branches to Marker A at:
    - `0x22006d40`
- Marker A:
  - calls `0x22002138(2)`
  - loops forever
- `0x2200198c` / `0x22001988` follow-up markers:
  - remained inactive
- `0x22001990` and `0x22001998`:
  - remained restored to original flow
- later checkpoints:
  - remained restored/original

Observed result:

- exploit completed
- defanged WTF upload completed
- firmware upload started
- host remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`
- post-run `lsusb` showed:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- no host-visible evidence that control reached the `0x22001994` marker
- strongest current classification:
  - `MARKER_1994_NOT_REACHED`
- next narrowing target should move earlier than the predecessor edge itself

Recovery status:

- manual recovery succeeded
- device is back in clean DFU

## 2026-04-24 Pre-loader setup marker prepared at `0x22001998`

The next isolation step moves the active marker earlier than the callback
callsite and into the setup block itself.

Active marker for the next run:

- `0x22001998`
  - original:
    - `00 00 99 e5`
    - `ldr r0, [r9]`
  - replacement:
    - `e8 14 00 ea`
    - `b 0x22006d40`
- marker target:
  - `0x22006d40`
  - calls `0x22002138(2)`
  - loops forever

Prepared but inactive follow-ups:

- `0x220019a0`
  - original:
    - `74 30 90 e5`
  - prepared replacement:
    - `ee 14 00 ea`
    - `b 0x22006d60`

- `0x220019a8`
  - original:
    - `02 1b 86 e2`
  - prepared replacement:
    - `f4 14 00 ea`
    - `b 0x22006d80`

Restored later checkpoints:

- `0x220019ac`
  - `c6 14 00 eb`
- `0x220019b0`
  - `00 00 50 e3`
- `0x22001a24`
  - `00 40 a0 e3`
- `0x22001b14`
  - `67 02 00 1b`

Verification and cache state:

- local `/tmp/wInd3x/wInd3x` rebuilt
- `/tmp/n3g-wtf-defanged-check.bin` regenerated
- verified:
  - `0x22001998: ea0014e8`
  - `0x220019ac: eb0014c6`
  - `0x220019b0: e3500000`
- cached defanged WTF removed again
- no hardware run performed yet

Current classification:

- **MARKER_1998_PREPARED**

## 2026-04-24 Pre-loader `0x22001998` marker tested once on hardware

The earliest prepared pre-loader setup marker was tested once with no RetailOS
changes and no activation of the later `0x220019a0` / `0x220019a8` markers.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`
- `lsusb`
  - Nano 3G present in DFU mode

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x22001998`
  - `00 00 99 e5` -> `e8 14 00 ea`
  - branches to Marker A at:
    - `0x22006d40`
- Marker A:
  - calls `0x22002138(2)`
  - loops forever
- `0x220019a0` / `0x220019a8` follow-up markers:
  - remained inactive
- later checkpoints:
  - remained restored/original

Observed result:

- exploit completed
- defanged WTF upload completed
- firmware upload started
- host remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`
- post-run `lsusb` showed:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- no host-visible evidence that control reached the `0x22001998` marker
- strongest current classification:
  - `MARKER_1998_NOT_REACHED`
- next narrowing target should move earlier than the pre-loader setup block

Recovery status:

- manual recovery succeeded
- device is back in clean DFU

## 2026-04-24 Loader-callback precall marker prepared

The next isolation step moves the active marker from the post-callback compare
to the callback callsite itself.

Active marker for the next run:

- `0x220019ac`
  - original:
    - `c6 14 00 eb`
    - `bl 0x22006ccc`
  - replacement:
    - `e3 14 00 ea`
    - `b 0x22006d40`
- marker target:
  - `0x22006d40`
  - calls `0x22002138(2)`
  - loops forever

Prepared but inactive follow-up:

- `0x22006ccc`
  - original:
    - `f0 43 2d e9`
  - prepared replacement:
    - `23 00 00 ea`
    - `b 0x22006d60`
  - not active in this build

Restored later checkpoints:

- `0x220019b0`
  - `00 00 50 e3`
- `0x22001a24`
  - `00 40 a0 e3`
- `0x22001b14`
  - `67 02 00 1b`

## 2026-04-24 Later OSOS proof patch prepared

The early startup/display hooks are now treated as mapped but insufficient.

Known early path:

- `0x22008814 -> 0x220039c4`
- later startup helper:
  - `0x22003a9c`
  - original Apple call:
    - `bl 0x22003d98`
- compact display wrappers:
  - `0x22005620`
  - `0x22005640`
  - `0x22005660`
- connected branch:
  - `0x220057d0`

Black-screen result means:

- those early hooks do not prove later runtime/UI execution
- stop retrying `0x22005640` by itself

Selected later hook:

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

Local proof stub:

- runtime:
  - `0x22008bd0`
- body offset:
  - `0x8bd0`
- original:
  - 28 bytes of `00`
- replacement:
  - `push {r0, r1, lr}`
  - `bl 0x220ff164`
  - `ldr r0, [sp]`
  - `ldr r0, [r0, #0xc8]`
  - `add r1, sp, #12`
  - `bl 0x2219d410`
  - `pop {r0, r1, pc}`

Why this is the current best proof patch:

- it runs later than the failed startup/display hooks
- it preserves the original Apple call at the hook site
- it reuses Apple's own `Beep`/`PlayTone` runtime path
- it avoids NAND, storage, USB PHY, shutdown, and guessed raw MMIO

Prepared artifacts:

- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_later_beep_signal.go`
- patched IMG1:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-later-beep-n3g.dfu`
- patched body:
  - `/tmp/n3g-osos-work/lcd-osos-later-beep-n3g.bin`

Status:

- prepared only
- not run yet

## 2026-04-24 Later beep proof hardware outcome

Run result for:

- `/tmp/n3g-osos-work/n3g-osos-cfw-later-beep-n3g.dfu`

Observed:

- host timed out waiting for WTF
- the Nano disappeared from USB entirely
- no `05ac:1242`
- no DFU device present immediately after the run
- on-device remained black
- no audible beep/tone/click
- no visible Apple/UI/display activity

Classification:

- **RUNTIME_SIGNAL_BLOCKED**

Verification and cache state:

- local `/tmp/wInd3x/wInd3x` rebuilt
- `/tmp/n3g-wtf-defanged-check.bin` regenerated
- verified:
  - `0x220019ac: ea0014e3`
  - `0x220019b0: e3500000`
  - `0x22006ccc: f0432de9`
- cached defanged WTF removed again
- no hardware run performed yet

Current classification:

- **MARKER_PRECALL_PREPARED**

## 2026-04-24 Pre-call `0x220019ac` marker tested once on hardware

The loader-callback callsite marker was tested once with no RetailOS changes
and no activation of the stub-entry marker.

Pre-run checks:

- `./utils/mks5lboot/mks5lboot --dfuscan`
  - `05ac:1223`
  - DFU state `2`
- `lsusb`
  - Nano 3G present in DFU mode

Run command:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Patch under test:

- `0x220019ac`
  - `c6 14 00 eb` -> `e3 14 00 ea`
  - branches to Marker A at:
    - `0x22006d40`
- Marker A:
  - calls `0x22002138(2)`
  - loops forever
- `0x22006ccc` stub-entry marker:
  - remained inactive
- later checkpoints:
  - remained restored/original

Observed result:

- exploit completed
- defanged WTF upload completed
- firmware upload started
- host remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`
- post-run `lsusb` showed:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- no host-visible evidence that control reached the `0x220019ac` marker
- strongest current classification:
  - `MARKER_PRECALL_NOT_REACHED`
- next narrowing target should move earlier than the callback callsite itself

Recovery status:

- manual recovery succeeded
- device is back in clean DFU

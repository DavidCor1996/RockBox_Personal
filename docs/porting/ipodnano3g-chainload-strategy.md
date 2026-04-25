# iPod Nano 3G RetailOS chainload strategy

## 2026-04-24 RetailOS hook target discovery

### Core finding

Nano 3G RetailOS is **not** structured like the EFI/PE32 firmware volumes used
by later `wInd3x` targets.

Evidence:

- `wInd3x` has EFI patch machinery (`VisitVolume`, `VisitPE32InFile`,
  `PatchAt`, `ReplaceExact`), but that path is used for EFI-based images such
  as Nano 5G/7G WTF.
- Nano 3G WTF defanging is already raw-offset based in local `wInd3x` source:
  `pkg/cfw/defang_wtf.go` uses `defangRaw(...)` for `devices.Nano3`.
- Parsing the decrypted Nano 3G OSOS body as an EFI firmware volume from offset
  `0` fails immediately with an invalid FV GUID.
- `_FVH` firmware-volume signatures were not found in the decrypted OSOS body.

Practical implication:

- the right chainload strategy on Nano 3G is still:
  - `BootROM -> defanged WTF -> modified RetailOS`
- but the RetailOS modification itself must be treated as a **raw ARM body
  patch**, not a PE32/DXE module patch.

### Candidate post-init proof points found in RetailOS

The decrypted OSOS body exposes several post-init UI/runtime clusters:

- `TCRemoteUI`
- `TChargingModeCntlr`
- `USBDeviceTask`
- `controller.SwitchToConnected1`
- `controller.SwitchToDisconnected1`
- `controller.SwitchToLoading1`

The lowest-risk visible proof point is the USB/DiskMode connected-screen
resource cluster, because:

- it is already inside Apple’s initialized RetailOS runtime
- it is expected to be relevant while `cfw run` keeps USB attached
- it avoids new raw MMIO
- it avoids branch-hook risk on the first chainload attempt

### Selected first target

Selected target:

- **Disk/USB connected UI string cluster**

Logical owner strings:

- `controller.SwitchToConnected1`
- `controller.SwitchToDisconnected1`
- `controller.SwitchToLoading1`
- `DiskMode_ScreenLayout_Connected`
- `DiskMode_OKToDisconnect_String`
- `RemoteUI_Ok_To_Disconnect_String`

Visible string offsets in decrypted OSOS body:

- body `0x817980`: `"Connected\\0"`
- body `0x8179b8`: `"Do not disconnect.\\0"`
- body `0x8179f0`: `"OK to Disconnect\\0"`

These are the safest first proof points because they trade control-flow risk for
resource substitution only.

### Higher-risk raw code candidates

These were identified but not selected for the first patch:

1. RemoteUI wrappers
   - body address `0x22005620`
   - body address `0x22005640`
   - these call the Apple runtime display/service veneers (`0x2200374c`,
     `0x22003774`, `0x220073b4`, `0x22007610`)
   - risk: medium/high, because they rely on service objects and dispatch state

2. RemoteUI dispatcher
   - body address `0x22005660`
   - referenced from the function-pointer table around `0x22025ff8`
   - risk: high, because it is stateful and dispatch-oriented

These remain good follow-up hook candidates if the low-risk string proof works.

### Prepared first patched RetailOS artifact

Prepared only, not run:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu`

Patch method:

- raw body string replacement with exact-length substitutions

Applied substitutions:

- body `0x817980`
  - original: `"Connected\\0"`
  - patched: `"CFW mode!\\0"`
- body `0x8179b8`
  - original: `"Do not disconnect.\\0"`
  - patched: `"CFW runtime ready!\\0"`
- body `0x8179f0`
  - original: `"OK to Disconnect\\0"`
  - patched: `"CFW booted      \\0"`

Patch helper used:

- `/tmp/wInd3x/cmd/patch_n3g_connected_ui.go`

Expected user-visible behavior if the chainload reaches this UI path:

- the usual connected/disk-mode screen should show the modified strings instead
  of the stock Apple text

### Decision

- **HOOK_TARGET_FOUND_PATCH_PREPARED**

With the important Nano 3G-specific note that the prepared patch is a **raw
RetailOS string patch**, not a PE32/DXE hook, because this OSOS image does not
present as an EFI firmware volume in the same way as later targets.

## 2026-04-24 First chainload result

Controlled result of:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu`

Observed outcome:

- screen stayed black for the full 60-second window
- no patched `CFW ...` text appeared
- no original Apple connected/disk text appeared
- no normal Apple boot UI appeared

Host-side outcome:

- the device did leave plain DFU and enter `05ac:1242` (`WTF mode`)
- it did **not** transition onward into a visible RetailOS state
- `mks5lboot --dfuscan` remained busy until the host `wInd3x` process was
  terminated
- recovery to clean DFU state `2` worked after manual reset

Interpretation:

- the patched RetailOS image was not enough to prove a successful post-WTF
  RetailOS/UI chainload on Nano 3G
- the blocker is now the handoff itself, not the specific connected-screen
  string patch

Updated classification:

- **CHAINLOAD_BLACKSCREEN**

Revised next step:

- inspect why Nano 3G remains in WTF mode after `cfw run`
- treat raw UI/resource patching as insufficient alone
- move toward either:
  - a post-init raw code hook once chainload is known-good, or
  - diagnosis of Nano 3G-specific RetailOS acceptance/handoff requirements

## 2026-04-24 WTF -> RetailOS handoff failure analysis

### Result

Current root-cause classification:

- **WTF_HANDOFF_BLOCKED_BY_CHECK**

### Why this is not primarily an OSOS-image-format failure

The patched RetailOS image is structurally valid IMG1 and remains aligned with
stock Nano 3G OSOS on the key basic fields:

- magic:
  - stock: `8702`
  - patched: `8702`
- version:
  - stock: `1.0`
  - patched: `1.0`
- entrypoint:
  - stock: `0x0`
  - patched: `0x0`
- body length:
  - stock: `0xa4a570`
  - patched: `0xa4a570`

The only intentional wrapper difference is the one already introduced by local
`wInd3x` RetailOS decryption logic:

- stock OSOS:
  - IMG1 format `3`
  - encrypted / signed wrapper
- decrypted and patched chainload OSOS:
  - IMG1 format `2`
  - unsigned / unencrypted wrapper

That format change is not a custom mistake in the visible-string patcher; it is
how local `wInd3x` already repacks Nano 3G decrypted RetailOS.

### Stronger blocker: Nano 3G defanged WTF only skips one check

Nano 3G `cfw.WTFDefangers` currently applies only one functional patch:

- `pkg/cfw/defang_wtf.go`
  - offset `0x1990`
  - comment: `skip signature check`

Disassembly of the decrypted Nano 3G WTF shows that this patch only bypasses
the `tst r5, #8`-gated branch around `0x2200198c`.

Immediately after that, another intact handoff block remains:

- `0x22001998..0x220019dc`
  - loads callback from `[service + 0x74]`
  - calls it with:
    - `r0 = image base`
    - `r1 = image base + 0x800`
    - `r2 = 2`
  - then checks hardware state at:
    - `0x38c00040`
    - `0x38c0000c`

If that path does not succeed, WTF falls into the failure path and never
escapes into RetailOS.

### Comparison with later-device defangers

This asymmetry is the key evidence:

- Nano 7G defanger explicitly patches:
  - header signature check
  - data signature check
  - AES behavior
- Nano 3G defanger patches only the first signature-related condition

So Nano 3G `cfw run` currently appears incomplete for full RetailOS handoff:

- enough patching to accept and transfer firmware
- not enough patching to complete the post-transfer validator/decrypt/jump path

### Practical interpretation

The black-screen chainload result is best explained as:

1. defanged WTF accepts the transfer
2. Nano 3G remains in WTF mode
3. the remaining validator/decrypt block does not green-light the jump
4. patched RetailOS never starts, so its resource changes are never reached

### Minimal fix to try next

Do **not** change RetailOS first.

The next fix should be in Nano 3G defanged WTF:

- extend `pkg/cfw/defang_wtf.go` for `devices.Nano3`
- add a second patch that bypasses the remaining
  `0x22001998..0x220019dc` validator/decrypt gate, not just the
  `0x22001990` signature branch

Most likely minimal patch direction:

- force the block to fall through to the success path at `0x22001a24`
- or patch the callback/AES-status result handling so it always reports success

### Prepared second-stage WTF patch

Prepared in:

- `/tmp/wInd3x/pkg/cfw/defang_wtf.go`

Implemented Nano 3G raw patches:

1. existing first-stage patch
   - body offset `0x1990`
   - original bytes:
     - `00 70 a0 03 22 00 00 0a`
   - patched bytes:
     - `00 70 a0 e3 22 00 00 ea`
   - effect:
     - convert conditional signature-gated path into unconditional success-side
       flow

2. new second-stage handoff patch
   - body offset `0x19b8`
   - original bytes:
     - `08 00 00 0a`
   - patched bytes:
     - `19 00 00 ea`
   - effect:
     - replace `beq 0x220019e0` (failure path) with unconditional
       `b 0x22001a24` (success path)
     - preserves the callback call at `0x220019ac`
     - skips only the remaining failure leg after the callback result test

### Build status

Updated binary rebuilt successfully:

- `/tmp/wInd3x/wInd3x`

### Important cache note

`wInd3x` caches defanged WTF here:

- `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`

That file was generated **before** the second-stage patch, so the next test must
invalidate it first or force regeneration.

### Next test command after that fix

After invalidating the stale cached defanged WTF and using the rebuilt
`/tmp/wInd3x/wInd3x`, rerun the same first visible chainload proof:

```bash
rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin
/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu
```

## 2026-04-24 WTF runtime flow tracing and external cross-check

Additional tracing past the patched `0x22001998..0x220019dc` block shows that
`0x22001a24` is not the actual RetailOS jump. It only sets `r4 = 0`, after
which WTF still has to:

1. restore saved gate/MIU state
2. call two more callbacks from `[service + 0x8c]` for IDs `19` and `33`
3. run the service/UART phase through `0x22006558`
4. satisfy the final execute gate:
   - `0x22001b0c: tst r5, #0x10`
   - `0x22001b14: blne 0x220024b8`

That means the second-stage patch at `0x19b8` only bypasses one failure leg. It
does **not** guarantee that WTF reaches the execute wrapper.

### Full mapped checks on the traced handoff path

From the call into `0x22001698` at `0x22002fe4`:

- launch base:
  - `r1 = 0x08000000`
- launch size:
  - `r2 = 0x00f80000`

Checks/conditions along the path:

1. `[service + 0x6c]` callback result
   - `0x22001978: blx [obj + 0x6c]`
   - `0x22001980: cmp r0, #0`
   - failure:
     - `r4 = 23`
     - branch to common cleanup at `0x22001a28`

2. `r5 & 0x8` signature/decrypt mode gate
   - `0x2200198c: tst r5, #8`
   - if clear:
     - `r7 = 0`
     - jump to `0x22001a24`
   - if set:
     - continue into `[service + 0x74]`

3. `[service + 0x74]` callback result
   - called as:
     - `r0 = image base`
     - `r1 = image base + 0x800`
     - `r2 = 2`
   - `0x220019b0: cmp r0, #0`
   - original failure:
     - `r4 = 87`
     - branch to common cleanup
   - current second-stage patch forces this leg to fall through to
     `0x22001a24`

4. `0x38c00040` / `0x38c0000c` status handling
   - original code:
     - checks `0x38c00040 & 3`
     - if nonzero, polls `0x38c0000c & 1` until ready
   - this is not a standalone handoff condition anymore in the current local
     patch, because the second-stage patch skips the failure side before this
     block matters

5. Post-success callbacks from `[service + 0x8c]`
   - `0x22001a70`: ID `19`
   - `0x22001aa8`: ID `33`
   - their return values are not checked directly, but they are still executed
     and therefore still required for side effects

6. Service/UART phase through `0x22006558`
   - called at `0x22001af4`
   - return value is ignored
   - however, the function contains a polling loop at:
     - `0x220065f4..0x220065fc`
   - it waits until `[selected UART base + 0x18] & 0x200` clears
   - if this bit never clears, WTF stalls here without ever reaching
     `0x220024b8`

7. Final execute gate
   - `0x22001b04: cmp r4, #0`
   - `0x22001b0c: tst r5, #0x10`
   - `0x22001b14: blne 0x220024b8`
   - `0x220024b8` then wraps the actual execute call with `0x22002138(0)` and
     `0x22002138(2)`

### What freemyipod's documentation changes

freemyipod's Boot Process page says the second-stage bootloader/WTF initializes
DRAM/LCD/UART/interrupts and, like BootROM, still performs IMG1 signature
checking and decryption before booting the next stage. The Modes page also says
WTF is a separate recovery-stage DFU environment, not just a transparent
forwarder.

Combined with the upstream `wInd3x` README support table:

- Nano 3G:
  - Haxed DFU: yes
  - Dump/Decrypt: yes
  - CFW: soon

the most defensible interpretation is:

- Nano 3G `cfw run` is still not feature-complete upstream
- the missing piece is not just one extra compare
- WTF still expects validator/decrypt callbacks to return success **and**
  populate state used later in the handoff

### Revised root cause

- **CALLBACK_DEPENDENCY_REQUIRED**

More precisely:

- skipping `0x1990` and `0x19b8` is insufficient because the handoff still
  depends on callback-produced state from:
  - `[service + 0x6c]`
  - `[service + 0x74]`
  - likely the later `[service + 0x8c]` callbacks as well
- the post-success `0x22006558` service phase can still stall forever
- therefore the exact missing condition is not a single remaining branch, but a
  successful validator/decrypt/service state transition inside WTF

### Correct next fix direction

Do **not** keep adding outer branch-skips blindly.

The next Nano 3G-specific work should be:

1. identify the concrete callback targets behind:
   - `[service + 0x6c]`
   - `[service + 0x74]`
   - `[service + 0x8c]`
2. determine what state they set up for the later service/UART phase and the
   final execute trampoline
3. patch or stub those callback targets, or their backing service state, rather
   than only forcing control flow around them

No further exact byte patch is justified yet beyond `0x1990` and `0x19b8`,
because another blind branch skip would likely preserve the same black-screen
WTF-mode stall.

## 2026-04-24 WTF service callback mapping

The callback table behind the handoff path is now partially mapped.

### Service pointer origin

The handoff code loads its service pointer from a WTF-local global:

- `0x22001a10` literal -> `0x22007784`
- contents of `0x22007784` in the decrypted WTF body:
  - `0x20000020`

So the handoff service object is **not** the local WTF runtime registry at
`0x220073ec`. It is a BootROM-resident object/table rooted at `0x20000020`.

This matches the Nano 3G `wInd3x` design note that haxed DFU works by
overriding the BootROM `OnImage` callback inside the BootROM `State`
structure, not by installing a separate Nano 4G/5G-style verifier vtable.

### Distinct local WTF registry

The local WTF registry around `0x220073ec` is separate:

- `0x220053a8`
- `0x220053e0`
- `0x22003508`

These functions manage local WTF service records such as the `Uart$` entries at
`0x22007790+`. They are used by the later `0x22006558` service/UART phase, but
they are **not** the source of the critical handoff callbacks.

### BootROM callback slots

With base `0x20000020`, the relevant callback slots are:

- `[service + 0x6c]` -> word at `0x2000008c`
- `[service + 0x74]` -> word at `0x20000094`
- `[service + 0x8c]` -> word at `0x200000ac`

Related neighboring slots used by the same handoff path:

- `[service + 0x44]` -> word at `0x20000064`
- `[service + 0x68]` -> word at `0x20000088`
- `[service + 0x78]` -> word at `0x20000098`
- `[service + 0x7c]` -> word at `0x2000009c`
- `[service + 0x80]` -> word at `0x200000a0`
- `[service + 0x90]` -> word at `0x200000b0`

The tiny BootROM dump already taken only covered `0x20000000..0x2000003f`, so
the actual function-pointer contents at `0x20000088/8c/94/ac/b0` are not yet
available from local artifacts.

### Calling convention and likely meanings

`[service + 0x6c]`

- call site:
  - `0x22001978`
- arguments:
  - none
- return use:
  - must be nonzero
- failure:
  - sets `r4 = 23`
  - exits through common cleanup
- likely role:
  - earlier validation/state-ready gate

`[service + 0x74]`

- call site:
  - `0x220019a0`
- arguments:
  - `r0 = 0x08000000`
  - `r1 = 0x08000800`
  - `r2 = 2`
- return use:
  - originally must be nonzero
- patched:
  - current `0x19b8` patch only bypasses its immediate failure leg
- likely role:
  - validator/decrypt/copy/setup callback for the incoming RetailOS image

`[service + 0x8c]`

- call sites:
  - `0x22001884`
  - `0x2200189c`
  - `0x22001a70`
  - `0x22001aa8`
- arguments:
  - earlier phase:
    - `(19, [service + 0x78])`
    - `(33, [service + 0x9c])`
  - later phase:
    - `(19, saved_value)`
    - `(33, saved_value)`
- return use:
  - not checked directly
- likely role:
  - save/restore side effects for IDs `19` and `33`

`[service + 0x90]`

- call sites:
  - `0x22001824`
  - `0x22001860`
- arguments:
  - ID `19` or `33`
- return use:
  - result saved to stack and later passed back to `[service + 0x8c]`
- likely role:
  - getter paired with `[service + 0x8c]`

`[service + 0x80]`

- call site:
  - `0x22001750`
- arguments:
  - pointer to temporary launch descriptor at `sp + 0x14c`
- return use:
  - none directly
- likely role:
  - populate launch descriptor before `0x2200152c`

### Best current classification

- **CALLBACK_TABLE_BOOTROM_KNOWN**

More precisely:

- the table base and slot layout are now known
- the specific target addresses stored in the BootROM words at
  `0x2000008c`, `0x20000094`, and `0x200000ac` are still missing from local
  artifacts
- therefore the exact callback bodies are not yet mapped

### Correct next step

No more blind branch patches.

The next justified analysis step is to recover the BootROM words at:

- `0x20000088`
- `0x2000008c`
- `0x20000094`
- `0x200000ac`
- `0x200000b0`

and then disassemble those callback targets to determine which one actually
produces the missing handoff state.

## 2026-04-24 BootROM callback bodies recovered

BootROM service-table dump:

- `0x20000080..0x200000bf`

Recovered callback pointers:

- `[service + 0x6c]` (`0x2000008c`) -> `0x200036c8`
- `[service + 0x74]` (`0x20000094`) -> `0x200006dc`
- `[service + 0x8c]` (`0x200000ac`) -> `0x2000106c`
- `[service + 0x90]` (`0x200000b0`) -> `0x2000132c`

### Callback meanings

`0x2000106c` (`[service + 0x8c]`)

- arguments:
  - `r0 = ID`
  - `r1 = value`
- behavior:
  - pure setter into BootROM-managed tables
  - IDs `< 32` write into one array
  - IDs `>= 32` write into a second array
- no MMIO
- no decrypt/copy work

`0x2000132c` (`[service + 0x90]`)

- arguments:
  - `r0 = ID`
- behavior:
  - getter paired with `0x2000106c`
- no MMIO
- no decrypt/copy work

So the `+0x8c/+0x90` pair is now understood: it saves/restores per-ID state.
It is not the primary validator/decrypt blocker.

`0x200036c8` (`[service + 0x6c]`)

- arguments:
  - none
- behavior:
  - reads haxed-DFU scratch globals at:
    - `0x2203fff8`
    - `0x2203fffc`
  - waits on BootROM/runtime state bytes around:
    - `[state + 0x2c]`
    - `[state + 0x738 + 0x37]`
    - `[state + 4]`
  - updates scratch/global state before returning
- return:
  - `1` on success
  - `0` on timeout/not-ready path
- interpretation:
  - readiness/state-synchronization callback
  - side effects exist, so blindly forcing its return is risky

`0x200006dc` (`[service + 0x74]`)

- arguments from WTF handoff path:
  - `r0 = 0x08000000`
  - `r1 = 0x08000800`
  - `r2 = 2`
- this is the critical loader/validator callback
- in `r2 = 2` mode it branches to `0x20000830`
- there it explicitly requires image type:
  - `3` or `4`
  - any other type returns `0`

This is the decisive finding.

### Exact failing condition

Current Nano 3G patched RetailOS images produced by local `wInd3x` are:

- magic `8702`
- version `1.0`
- format `2`

Recovered from:

- stock `OSOS.fw`:
  - format `3`
- decrypted/patched Nano 3G RetailOS:
  - format `2`

But the actual BootROM loader callback at `0x200006dc` rejects type `2` in the
WTF handoff path used by `cfw run`.

This explains why:

- branch patches at `0x1990` and `0x19b8` are insufficient
- WTF stays in mode `05ac:1242`
- RetailOS never starts

### Revised root cause

- **CALLBACK_BODIES_RECOVERED_PATCH_PLAN_READY**

More specifically:

- the immediate blocker is an image-format mismatch, not another missing outer
  branch
- `+0x74` expects a type `3` or `4` IMG1 in handoff mode `2`
- local Nano 3G repack logic currently produces type `2`

### Correct minimal fix

Do **not** add another WTF branch patch.

The minimal change to try next is in image construction for Nano 3G CFW:

- stop repacking Nano 3G RetailOS as `FormatSigned` (`2`) for the `cfw run`
  path
- instead generate a Nano 3G RetailOS wrapper that matches what
  `0x200006dc(r2=2)` accepts:
  - format `4` (`FormatX509Signed`)
  - unencrypted plaintext body
  - header/footer lengths consistent with type `4`

In local `wInd3x`, that likely means changing the Nano 3G special-case in:

- `/tmp/wInd3x/pkg/image/image.go`

Current code:

- Nano 3G:
  - version `1.0`
  - format `2`
  - no signature/cert area

Next experiment should be:

- preserve Nano 3G `8702` / `1.0`
- emit type `4`-style wrapper with footer/signature layout that the recovered
  `+0x74` parser accepts

No further exact WTF byte patch is justified until that image-format mismatch is
tested.

## 2026-04-24 Nano 3G CFW image format fix

Implemented the minimal wrapper-side fix in local `wInd3x`:

- file:
  - `/tmp/wInd3x/pkg/image/image.go`
- function:
  - `MakeUnsigned(...)`

Change:

- Nano 3G no longer forces:
  - format `2`
  - zero signature area
  - zero certificate area
- Nano 3G now keeps:
  - magic `8702`
  - version `1.0`
  - standard X509-style unsigned wrapper layout:
    - format `4`
    - signature area `0x80`
    - certificate area `0x300`

Reason:

- recovered BootROM callback `0x200006dc` accepts only image type `3` or `4`
  in the WTF handoff mode
- current local Nano 3G repacks were type `2`
- this wrapper fix targets the exact recovered failing condition

Rebuilt:

- `/tmp/wInd3x/wInd3x`

Regenerated patched RetailOS artifact:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Verified header fields:

- magic:
  - `8702`
- version:
  - `1.0`
- format:
  - `4`
- entry:
  - `0x0`
- body:
  - `0xa4a570`
- data length:
  - `0xa4a8f0`
- cert offset:
  - `0xa4a5f0`
- cert length:
  - `0x300`

Comparison:

- stock `OSOS.fw`
  - format `3`
  - body `0xa4a570`
  - cert offset `0xa4a5f0`
- old local decrypted/patched Nano 3G RetailOS
  - format `2`
- new prepared chainload artifact
  - format `4`
  - same body length as stock
  - same entrypoint as stock

Current decision:

- **FORMAT4_CFW_IMAGE_PREPARED**

No `cfw run` was performed in this step.

## 2026-04-24 Format-4 chainload test result

Ran once:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Result:

- the new format-4 wrapper did not boot RetailOS
- the device immediately landed in/stayed in WTF mode:
  - `05ac:1242`
- host-side scan during the stuck state returned:
  - `LIBUSB_ERROR_BUSY`
- no visible Apple UI or patched connected strings appeared

Interpretation:

- fixing the IMG1 wrapper format was necessary, but not sufficient
- the Nano 3G still fails later in the WTF runtime handoff path, after the
  earlier format-2 rejection point

Current classification:

- **CHAINLOAD_WTF_STUCK**

## 2026-04-24 Post-format-acceptance WTF tracing

Re-checked the live inputs used by the failed format-4 `cfw run`:

- cached defanged WTF really contains both Nano 3G patches:
  - body offset `0x1990`:
    - `00 70 a0 e3 22 00 00 ea`
  - body offset `0x19b8`:
    - `19 00 00 ea`
- the actual RetailOS image used for the test is really format `4`:
  - magic `8702`
  - version `1.0`
  - format `4`
  - entry `0`
  - body `0xa4a570`
  - cert offset `0xa4a5f0`
  - cert length `0x300`

So neither of these explanations fits anymore:

- `PATCH_NOT_APPLIED`
- `CFW_IMAGE_NOT_USED`

### Next blocker inside BootROM callback `0x200006dc`

Recovered type-`4` path in the loader callback:

- `0x20000844..0x20000854`
  - requires image type `3` or `4`
- `0x20000858..0x20000890`
  - validates header-layout constraints:
    - aligned body end
    - `DataLength`
    - `FooterCertOffset`
    - `FooterCertOffset & 0xf == 0`
    - `Entrypoint <= aligned body size`
- `0x20000898..0x200008b4`
  - copies/loads header/body state
- `0x200008c8..0x200008e4`
  - prepares verification call
- `0x200008e4: bl 0x200055f0`
- `0x200008e8: cmp r0, #1`
- `0x200008ec: bne 0x2000095c`
  - failure returns `0` from the callback

This means type acceptance alone is not enough. The type-`4` path still requires
successful signature/certificate verification through `0x200055f0`.

### Why the current format-4 image still fails

Local Nano 3G `MakeUnsigned(...)` currently emits placeholder footer data:

- signature area:
  - `0x80` bytes of `'S'`
- certificate area:
  - `0x300` bytes of `'C'`

That wrapper is structurally type-`4`, but it is not Apple-signed. So the
remaining failure is best explained as:

- the BootROM loader enters the type-`4` path
- then rejects the placeholder signature/certificate payload at
  `0x200008e4..0x200008ec`
- WTF stays in mode `05ac:1242`
- RetailOS never starts

### Updated root cause

- **FORMAT4_STILL_REJECTED**

More precisely:

- the old format-`2` rejection point is gone
- the next concrete blocker is the later type-`4` verification gate inside
  BootROM callback `0x200006dc`
- later WTF execute-gate tracing is currently secondary because the handoff
  callback still returns failure first

### Correct next fix direction

Do not change the RetailOS wrapper again blindly.

Do not add another outer WTF branch skip blindly.

The next justified fix must address the BootROM callback behavior itself, most
likely by:

- redirecting `[service + 0x74]` to a WTF-local replacement callback, or
- replacing the service pointer loaded from `0x22007784` with a local table
  whose `+0x74` slot points to a custom Nano 3G loader stub

That custom stub would need to preserve the useful side effects of
`0x200006dc` without requiring valid Apple signature/certificate material.

## 2026-04-24 Nano 3G loader callback stub plan

The preferred next fix is now a **direct callsite replacement**, not a full
service-table redirect.

### Why direct replacement is safer than table redirection

The service object rooted at `0x22007784 -> 0x20000020` is still used for
other callbacks later in the WTF handoff path. Replacing the whole pointer with
a local table would require recreating more BootROM slots than are currently
necessary.

Only one slot is the concrete verified blocker right now:

- `[service + 0x74]` -> `0x200006dc`

So the minimal Nano 3G-specific change is:

- leave the BootROM service object in place
- patch only the one `blx [service + 0x74]` callsite to call a local WTF stub

### Exact patch plan

Patch the handoff callsite in WTF:

- runtime:
  - `0x220019ac`
- body offset:
  - `0x19ac`
- original bytes:
  - `33 ff 2f e1`
  - `blx r3`
- replacement bytes:
  - `c6 14 00 eb`
  - `bl 0x22006ccc`

This means:

- `[service + 0x6c]` still runs unchanged
- `[service + 0x8c]` / `[service + 0x90]` still run unchanged
- the existing Nano 3G branch patches at `0x1990` and `0x19b8` remain in place

### Stub location

Recovered free zeroed run inside decrypted Nano 3G WTF body:

- start:
  - `0x6cca`
- length:
  - `0x30a`

Chosen placement:

- stub body offset:
  - `0x6ccc`
- runtime address:
  - `0x22006ccc`
- stub size:
  - `300` bytes (`0x12c`)

This fits fully inside the confirmed unused zeroed region.

### Stub behavior

The local stub preserves the original callback calling convention:

- `r0` = IMG1 header pointer
- `r1` = IMG1 body pointer
- `r2` = mode selector

Behavior:

1. If `r2 != 2`
   - tail-call the original BootROM callback:
     - `0x200006dc`

2. If `r2 == 2`
   - replay the original type-`4` pre-verification path:
     - `0x200005dc` header precheck
     - type `3` / `4` check
     - body/data/footer layout checks
     - `0x20002574`
     - `0x2000273c`
     - `0x200020f8`
   - **skip only** the later cert/signature verification stage:
     - original failing block:
       - `0x200008e4: bl 0x200055f0`
       - `0x200008e8: cmp r0, #1`
       - `0x200008ec: bne 0x2000095c`
   - replay the original success-side metadata writes:
     - version bytes -> BootROM state offsets `+0x34..+0x36`
     - entrypoint -> BootROM state offset `+0x30`
   - return `1`

This preserves the loader-side setup that happens before the cert/signature
gate, instead of blindly forcing success from the outside.

### Implemented local `wInd3x` changes

Updated:

- `/tmp/wInd3x/pkg/cfw/defang_wtf.go`

Nano 3G defang now carries:

- `0x1990`
  - `00 70 a0 e3 22 00 00 ea`
- `0x19ac`
  - `c6 14 00 eb`
- `0x19b8`
  - `19 00 00 ea`
- `0x6ccc`
  - 300-byte loader stub

Rebuilt:

- `/tmp/wInd3x/wInd3x`

### Decision

- **LOADER_CALLBACK_STUB_PLAN_READY**

### Prepared test command

Do not run automatically.

When approved, the next controlled test is:

```bash
rm -f /home/david/.local/share/wInd3x/n3g-wtf-defanged.bin
/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu
```

## 2026-04-24 Local loader-stub failure analysis and offline fix

### Objective

Determine why the first Nano 3G WTF-local loader stub still left the device in
WTF mode `05ac:1242` instead of advancing into RetailOS.

### Patched WTF artifact verification

Verified against the extracted IMG1 body from:

- `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`

Confirmed in the original first-stub artifact:

- callsite patch at body `0x19ac`
  - original bytes:
    - `33 ff 2f e1`
    - `blx r3`
  - patched bytes:
    - `c6 14 00 eb`
    - `bl 0x22006ccc`
- branch patch at body `0x19b8`
  - original bytes:
    - `08 00 00 0a`
  - first-stub replacement:
    - `19 00 00 ea`
- local stub bytes were present at body `0x6ccc`
- original contents at `0x6ccc` were zero-filled free space

### BL encoding check

ARM BL math is correct:

- source instruction:
  - `0x220019ac`
- ARM PC during branch calculation:
  - `0x220019b4`
- destination:
  - `0x22006ccc`
- delta:
  - `0x5318`
- imm24:
  - `0x14c6`
- instruction word:
  - `0xeb0014c6`
- little-endian bytes:
  - `c6 14 00 eb`

So `0xc6 14 00 eb` really does branch to `0x22006ccc`.

### Disassembly result

Verified disassembly around the patched WTF callsite:

- `0x220019a8: e2861b02    add r1, r6, #0x800`
- `0x220019ac: eb0014c6    bl 0x22006ccc`
- `0x220019b0: e3500000    cmp r0, #0`
- `0x220019b4: e3a07b02    mov r7, #0x800`

First-stub artifact:

- `0x220019b8: ea000019    b 0x22001a24`

Revised offline-fixed artifact:

- `0x220019b8: 0a000008    beq 0x220019e0`

This rules out:

- **BAD_BRANCH_ENCODING**

### Why the first local stub was insufficient

Comparison against the BootROM callback at `0x200006dc` showed that the first
local stub did not reproduce enough of the original success path.

Missing or incorrect behavior in the first stub:

- did not preserve `r9`
- did not run the BootROM preflight helpers:
  - `0x20001ef0`
  - `0x20001fe0`
- patched `0x19b8` into an unconditional branch and removed the original
  post-callback status handling
- replayed only part of the type-`4` success path
- skipped the type-`3` tail path:
  - `0x20001f04`
  - `0x20001d48`

What was already correct:

- stub bytes were present in the expected free run
- the callsite patch targeted the right address
- the metadata-write direction was broadly correct

### Failure classification

Best fit for the first failed local-stub attempt:

- **MISSING_SIDE_EFFECT**

Rejected alternatives:

- **BAD_BRANCH_ENCODING**
  - branch math and disassembly are exact
- **BAD_RETURN**
  - not the primary blocker, because the first-stub build had already replaced
    `0x19b8` with an unconditional forward branch
- **STUB_NOT_REACHED**
  - not supported by the offline artifact evidence; the branch and stub bytes
    are internally consistent

### Revised local stub

The revised stub keeps the same target:

- body `0x19ac` -> `bl 0x22006ccc`

But now follows the BootROM callback much more closely:

- saves `{r4-r9, lr}`
- allocates the same `0x2c` stack frame
- stores arguments like BootROM:
  - `r9 = r0`
  - `r6 = r1`
  - `r4 = r2`
- runs BootROM preflight helpers:
  - `0x20001ef0`
  - `0x20001fe0`
- keeps `r7 = sp + 8`
- if `r2 != 2`, tail-calls original `0x200006dc`
- if `r2 == 2`, replays the mode-`2` / type-`4` success-side path:
  - `0x200005dc`
  - type/layout checks
  - `0x20002574`
  - `0x20002574`
  - `0x2000273c`
  - `0x200020f8`
- skips only the later verification call:
  - `0x200055f0`
- replays version and entrypoint metadata writes
- includes the type-`3` tail calls:
  - `0x20001f04`
  - `0x20001d48`
- returns `1` on success and `0` on failure

### Revised patch bytes

Offline-verified replacement plan:

- body `0x19ac`
  - original:
    - `33 ff 2f e1`
  - replacement:
    - `c6 14 00 eb`
- body `0x19b8`
  - original:
    - `08 00 00 0a`
  - first-stub replacement:
    - `19 00 00 ea`
  - revised replacement:
    - `08 00 00 0a`
    - restored to the original conditional branch
- body `0x6ccc`
  - original:
    - zero-filled free space
  - revised replacement:
    - local stub, `452` bytes

### Offline verification result

Rebuilt local `wInd3x` in `/tmp/wInd3x` and generated a fresh defanged WTF
artifact offline without running `cfw`.

Verified fresh artifact:

- `0x19ac` still encodes:
  - `c6 14 00 eb`
- `0x19b8` is restored to:
  - `08 00 00 0a`
- stub at `0x6ccc` now begins with:
  - `f0 43 2d e9 2c d0 4d e2 ...`

Decision update:

- **LOADER_CALLBACK_STUB_REVISED_OFFLINE**

Boundary for the next session:

- local `wInd3x` was rebuilt
- the replacement artifact was verified offline
- **no new `cfw run` was performed**

## 2026-04-24 Revised local-stub hardware test result

Tested once with the revised cached defanged WTF artifact copied into:

- `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`

Command run:

```bash
/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu
```

Observed host-side sequence:

- clean pre-run DFU:
  - `05ac:1223`
  - DFU state `2`
- `wInd3x` used cached Nano 3G `wtf-defanged`
- defanged WTF upload completed
- firmware upload started
- post-handoff USB state remained:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`

User-visible result:

- no RetailOS boot
- no Apple logo
- no connected-screen proof text

Interpretation:

- restoring `0x19b8`
- preserving `r9`
- adding `0x20001ef0` / `0x20001fe0`

were not sufficient by themselves to complete the WTF -> RetailOS handoff.

Updated status:

- the revised local stub did not produce an exact fix
- strongest remaining theory is still:
  - **MISSING_SIDE_EFFECT**
- but the exact missing requirement is still unresolved

Decision update:

- **REVISED_STUB_TESTED_STILL_WTF**

## 2026-04-24 BootROM callback side-effect reconstruction

### Callback trace: concrete writes recovered

The fully recovered part of `0x200006dc` shows only a small number of explicit
success-side state writes in the callback body itself.

For mode `2`, after the `0x200055f0` verification call succeeds, BootROM writes
through the state pointer stored at `*0x2203fff8`:

- `[state + 0x34] = img_hdr[4]`
- `[state + 0x35] = img_hdr[5]`
- `[state + 0x36] = img_hdr[6]`
- `[state + 0x30] = *(uint32_t *)(img_hdr + 0x08)`

Those are the exact writes at:

- `0x200008f8`
- `0x20000900`
- `0x20000908`
- `0x20000910`

For type `3`, it then also performs the tail calls:

- `0x20001f04`
- `0x20001d48`

### Related BootROM state observed

Recovered related callbacks clarify where nearby state lives:

`0x200036c8` (`[service + 0x6c]`):

- reads and gates on:
  - `[state + 0x2c]`
  - `[state + 0x04]`
  - `[state + 0x738 + 0x36]`
  - `[state + 0x738 + 0x37]`
- updates scratch globals:
  - `*0x2203fff8 = (*0x2203fff8)->next`
  - `*0x2203fffc = (*0x2203fffc + 0x2000)->0x720`

`0x2000106c` (`[service + 0x8c]`):

- pure table setter
- stores `r1` into one of two per-ID BootROM arrays depending on whether
  `r0 < 32`

`0x2000132c` (`[service + 0x90]`):

- matching per-ID table getter

### Critical state checked later by WTF

The post-callback WTF path at `0x22001998..0x22001b0c` does **not** directly
consume `[state + 0x30]` or `[state + 0x34..0x36]`.

What it does consume immediately after `[service + 0x74]` returns:

1. AES/MMIO status at:
   - `0x38c00040`
   - `0x38c0000c`
2. service callback at:
   - `[service + 0x8c]`
   - IDs `19` and `33`
3. service-owned pointer restore through:
   - `[service + 0x44]`
4. UART object setup/polling through:
   - `0x22006558`
   - which polls `[selected_uart_base + 0x18] & 0x200`
5. final execute gate:
   - `0x220024b8`

This means the version and entrypoint writes are necessary for correctness, but
they are not the immediate post-callback gate that kept the device in WTF.

### State map

Known callback-owned state writes:

- `*0x2203fff8 + 0x30`
  - image entrypoint offset
  - written from IMG1 header offset `0x08`
- `*0x2203fff8 + 0x34`
  - IMG1 version byte 0
- `*0x2203fff8 + 0x35`
  - IMG1 version byte 1
- `*0x2203fff8 + 0x36`
  - IMG1 version byte 2

Known related state used by adjacent callbacks:

- `*0x2203fff8 + 0x2c`
  - readiness / dfu-done style state used by `[service + 0x6c]`
- `*0x2203fff8 + 0x738 + 0x36`
  - nested status byte used by `[service + 0x6c]`
- `*0x2203fff8 + 0x738 + 0x37`
  - nested status byte used by `[service + 0x6c]`
- `0x38c00040`
  - immediate post-`+0x74` hardware/status poll in WTF
- `0x38c0000c`
  - immediate post-`+0x74` ready poll in WTF

### Compare to the old local stub

The previous local stub:

- replayed `0x200005dc`
- replayed `0x20002574`
- replayed `0x2000273c`
- replayed `0x200020f8`
- manually wrote `[state + 0x30]` and `[state + 0x34..0x36]`
- **skipped `0x200055f0` entirely**

That is the strongest mismatch.

If `0x200055f0` programs or clears AES-side state that WTF immediately polls at
`0x38c00040` / `0x38c0000c`, then the old stub could never recreate those side
effects with metadata writes alone.

### Minimal state patch design

The minimal justified patch is now:

1. For `r2 != 2`
   - tail-call original `0x200006dc`
2. For `r2 == 2`
   - call original `0x200006dc` first
   - if it returns `1`, return success unchanged
   - if it returns `0`, only convert that failure into success if the IMG1
     still passes the expected type/layout checks for type `3` / `4`
3. After that fallback path:
   - replay only the explicit success-side state writes:
     - `[state + 0x34..0x36]`
     - `[state + 0x30]`
   - for type `3`, replay:
     - `0x20001f04`
     - `0x20001d48`
   - return `1`

This preserves:

- `0x200005dc`
- `0x20002574`
- `0x2000273c`
- `0x200020f8`
- `0x200055f0`

and therefore preserves the BootROM helper/MMIO side effects that the old stub
was bypassing.

### Local implementation status

Implemented locally in `/tmp/wInd3x`:

- revised stub now calls original `0x200006dc` first
- only replays success-side metadata on the expected type/layout failure path
- new stub size:
  - `308` bytes

Offline verification only:

- body `0x19ac` remains:
  - `c6 14 00 eb`
- body `0x19b8` remains:
  - `08 00 00 0a`
- new stub at `0x6ccc` starts:
  - `f0 43 2d e9 2c d0 4d e2 ...`

Output:

- **CALLBACK_STATE_REQUIRED_PATCH_READY**

Boundary:

- local `wInd3x` updated and rebuilt
- offline-defanged WTF regenerated and verified
- **no new hardware run performed for this patch yet**

## 2026-04-24 Hardware retest of narrowed callback-state patch

One controlled Nano 3G hardware test was performed with the narrower local
callback-state patch.

Observed host sequence:

- cached payload used:
  - `n3g-wtf-defanged.bin`
- exploit succeeded:
  - `Haxed DFU running!`
- defanged WTF upload succeeded:
  - `Got dfuMANIFEST, image uploaded.`
- firmware upload started:
  - `Sending firmware...`
- post-handoff USB enumeration became:
  - `05ac:1242`
  - Nano 3G WTF mode
- post-run `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`

Meaning:

- preserving original `0x200006dc` execution and `0x200055f0` side effects was
  still not sufficient to complete the WTF -> RetailOS transition
- the remaining blocker is downstream of format-4 acceptance and downstream of
  the explicit success-side metadata writes already reconstructed
- the current narrowed patch is therefore still missing some additional state,
  callback interaction, or handoff precondition

Updated classification:

- **STILL_BLOCKED**

## 2026-04-24 Post-loader WTF handoff tracing

This pass deliberately resets assumptions around the loader callback:

- `[service + 0x74]` is not treated as sufficient by itself
- no new loader-callback patch is justified from this trace alone

### Post-loader path map

The active Nano 3G `cfw run` path enters `0x22001698` from:

- `0x22002fe4`

with:

- `r0 = 0x1d`
- `r1 = 0x08000000`
- `r2 = 0x00f80000`

That means the mode flags in `r5` inside `0x22001698` are:

- `r5 & 0x8` set
- `r5 & 0x10` set

So the final execute gate is statically armed on this path.

### `[service + 0x8c]`

Recovered callback body:

- `[service + 0x8c]` -> `0x2000106c`

Meaning:

- pure BootROM setter
- arguments:
  - `r0 = ID`
  - `r1 = value`
- no MMIO
- no image validation
- no execute gating on its own

In the post-loader path WTF uses it only to restore saved per-ID state:

- `0x22001a70`: `(19, saved_value)`
- `0x22001aa8`: `(33, saved_value)`

So `[service + 0x8c]` is not the next primary blocker.

### `[service + 0x44]`

`[service + 0x44]` is not a callback target.

Use in `0x22001698`:

- `0x2200173c`: load pointer from `[service + 0x44]`
- `0x22001744`: save `*ptr` to stack
- `0x22001748`: store `0x22007e88` through that pointer
- `0x22001ad4`: restore saved word through the same pointer

Meaning:

- it is a BootROM-owned pointer slot / current-context latch
- it is temporarily overridden during the handoff
- it is restored before the UART/service phase

So `[service + 0x44]` does not add a second callback gate after the loader.

### Why `0x22006558` is the next exact blocker

Once control reaches:

- `0x22001a24`

WTF does:

1. `r4 = 0`
2. restore saved per-ID state with `[service + 0x8c]`
3. restore the `[service + 0x44]` pointed word
4. call `0x22006558`
5. test:
   - `0x22001b04: cmp r4, #0`
   - `0x22001b0c: tst r5, #0x10`
   - `0x22001b14: blne 0x220024b8`

Critical finding:

- between `0x22001a24` and `0x22001b04`, `r4` is never modified
- on this path `r5 & 0x10` is already set from caller flags `0x1d`
- return value from `0x22006558` is ignored

So if `0x22006558` returns at all, WTF will still call:

- `0x220024b8`

### `0x22006558`

This is the remaining non-returning path before execute.

Service object selection:

- `0x2200177c`: `0x22006380(0, &sp+0x2c)`
- local service registry count:
  - `0x2200778c = 3`
- local entries at:
  - `0x22007790`
  - `0x220077b4`
  - `0x220077d8`
- all are tagged:
  - `Uart$`

Resolved UART bases from those records:

- ID `0` -> `0x3cc00000`
- ID `1` -> `0x3cc04000`
- ID `2` -> `0x3cc08000`

The post-loader call is:

- `0x22001af4: bl 0x22006558`

with:

- `r0 = sp + 0x2c`
- `r1 = sp + 0x08`
- `r2 = sp + 0x04`

The only hard stall inside `0x22006558` is:

- `0x220065d0..0x220065fc`

which polls:

- `[selected_uart_base + 0x18] & 0x200`

until the bit clears.

If that bit never clears, WTF never reaches:

- `0x22001b0c`
- `0x220024b8`

### Execute gate and entrypoint

If `0x22006558` returns, execute is taken through:

- `0x22001b14: blne 0x220024b8`

`0x220024b8` wraps the call with:

- `0x22002138(0)`
- direct `blx r4`
- `0x22002138(2)`

The address passed to `0x220024b8` is:

- `r0 = r6 + r7`

where:

- `r6 = 0x08000000`
- `r7 = 0x800`

So the immediate execute target is:

- `0x08000800`

This is the body entrypoint, not a later metadata-derived address.

### Handoff state machine

- Stage:
  - readiness callback
  - address: `0x22001978`
  - condition: `[service + 0x6c] != 0`
  - expected: success
  - failure: `r4 = 23`, cleanup, no execute
  - known status: still callback-dependent, outside this post-loader pass

- Stage:
  - loader callback
  - address: `0x220019ac`
  - condition: mode `2` callback path returns success
  - expected: nonzero
  - failure: original `r4 = 87` path
  - known status: already the subject of prior callback work

- Stage:
  - post-loader success landing
  - address: `0x22001a24`
  - condition: control reaches success leg
  - expected: `r4 = 0`
  - failure: none here
  - known status: mapped

- Stage:
  - restore saved BootROM state
  - addresses: `0x22001a70`, `0x22001aa8`, `0x22001ad4`
  - condition: setter-only state restore and pointer restore
  - expected: no direct branch gating
  - failure: none directly checked
  - known status: mapped, not primary blocker

- Stage:
  - UART/service phase
  - address: `0x22001af4 -> 0x22006558`
  - condition: UART ready bit clears for each transmitted byte
  - expected: `[uart_base + 0x18] & 0x200 == 0`
  - failure: infinite polling at `0x220065d0..0x220065fc`
  - known status: only remaining non-returning path before execute

- Stage:
  - execute gate
  - addresses: `0x22001b04..0x22001b14`
  - condition: `r4 == 0` and `r5 & 0x10 != 0`
  - expected: true on `cfw run` path once `0x22001a24` is reached
  - failure: skip `0x220024b8`
  - known status: statically satisfied after post-loader success landing

- Stage:
  - execute wrapper
  - address: `0x220024b8`
  - condition: reached from execute gate
  - expected: `blx 0x08000800`
  - failure: downstream of this trace
  - known status: not proven on hardware, but not statically blocked once
    `0x22006558` returns

### Decision

- **UART_SERVICE_STALL**

More precisely:

- `[service + 0x8c]` is setter-only
- `[service + 0x44]` is a temporary pointer-slot override, not a callback
- the final execute bit in `r5` is already set on the active Nano 3G path
- after post-loader success landing, the only remaining pre-execute blocking
  path is the non-returning UART poll inside `0x22006558`

### Patch status

- no new WTF patch prepared
- skipping `0x22006558` without proof would still be speculative

## 2026-04-24 UART service stall reduction

This pass focuses only on:

- `0x22006558`

and does not add any more loader-callback bypasses.

### Full trace of `0x22006558`

Inputs:

- `r0 = service record pointer`
- `r1 = byte buffer`
- `r2 = pointer to byte count`

Validation:

- `0x22006568` -> `0x220062b8(r0)`
  - verifies `service->name == "Uart$"`
  - verifies `service->size == 0x24`
  - verifies `service->active != 0`

Immediate exits:

- invalid service:
  - returns `7`
- null buffer or null count pointer:
  - returns `9`

Normal body:

- loads:
  - `service_flags = [service + 0x18]`
  - `count = *r2`
- if `service_flags & 1`:
  - runs formatting / notification helpers:
    - `0x22003f7c`
    - `0x220041bc`
  - returns `0`
- otherwise:
  - loops over `count` bytes
  - chooses UART base from `service->id`
  - waits until:
    - `[uart_base + 0x18] & 0x200 == 0`
  - writes next byte to:
    - `[uart_base + 0x20]`
  - returns `0`

So the only blocking behavior in this function is the raw transmit wait.

### Selected UART base

The active post-loader path chooses the first local `Uart$` record:

- `0x2200177c: 0x22006380(0, &sp+0x2c)`

and the first record is:

- record base:
  - `0x22007790`
- ID byte at `+0x9`:
  - `0`
- UART base at `+0xc`:
  - `0x3cc00000`

So the stalled transmit path is polling:

- `0x3cc00018`

and writing bytes to:

- `0x3cc00020`

### Meaning of bit `0x200`

Local freemyipod evidence:

- `analysis/freemyipod_src/apps/uarttest/main.c`
  - `UFSTAT` is defined at:
    - `0x3cc00018`
  - TX helper waits:
    - `while (UFSTAT & BIT(9))`
  - transmit byte goes to:
    - `UTXH = *(volatile uint8_t *)0x3cc00020`

Rockbox register definitions for the matching UART family:

- `regs/stmp3700/uartdbg.h`
  - `FR` at offset:
    - `0x18`
  - `TXFF` bit:
    - `0x20`

On S5L8702/freemyipod, the exact block used here is the Samsung-style UART:

- `UFSTAT @ +0x18`
- `UTXH @ +0x20`
- `BIT(9) == 0x200`

So the WTF loop is waiting for:

- **TX FIFO not full**

This is a UART transmit drain / debug-output path, not a NAND, USB PHY, or
RetailOS entry prerequisite.

### Writes before the poll

All setup happens earlier in:

- `0x22006430`

For the selected UART block this routine can write:

- `[uart_base + 0x0c] = 17`
  - only if the descriptor field at `[cfg + 0x4]` is nonzero
- `[uart_base + 0x28] = 12`
  - baud divisor style field
- `[uart_base + 0x04] |= 0x0c`
  - when descriptor flags bit `0` is set
- `[uart_base + 0x04] |= 0x03`
  - when descriptor flags bit `1` is set

It also stores the descriptor flags into:

- `[service + 0x18]`

Those setup writes are preserved by the patch below.

### Safe patch decision

- **BYPASS_UART_FLUSH_SAFE**

Reason:

- `0x22006558` only validates the local `Uart$` service, then transmits bytes
  to the UART
- the caller ignores its return value
- all earlier handoff setup, including `0x22006430`, remains intact
- execute remains gated only by:
  - reaching `0x22001a24`
  - `r4 == 0`
  - `r5 & 0x10`
- this patch does not touch NAND, storage, USB PHY, or the execute wrapper

### Implemented Nano 3G-only patch

Implemented in local `/tmp/wInd3x/pkg/cfw/defang_wtf.go`.

Runtime address:

- `0x22006558`

Body offset:

- `0x6558`

Original bytes:

- `f8 40 2d e9 02 70 a0 e1`

Replacement bytes:

- `00 00 a0 e3 1e ff 2f e1`

Meaning:

- `mov r0, #0`
- `bx lr`

So the patched Nano 3G defanged WTF now returns success immediately from the
UART transmit helper, preserving all prior setup while avoiding the unbounded
TX FIFO poll.

### Verification

Local rebuild only:

- rebuilt `/tmp/wInd3x/wInd3x`
- regenerated `/tmp/n3g-wtf-defanged-check.bin`
- verified parsed image body bytes:
  - `0x1990`
  - `0x19ac`
  - `0x19b8`
  - `0x6558`

Decision:

- **UART_STALL_PATCH_PREPARED**

Boundary:

- patch implemented and rebuilt locally
- no new `cfw run` performed

## 2026-04-24 Hardware retest of UART-stall bypass

One controlled Nano 3G hardware test was performed with the patched defanged
WTF that returns immediately from:

- `0x22006558`

Observed host sequence:

- DFU confirmed:
  - `05ac:1223`
  - state `2`
- cached payload refreshed from the offline-verified artifact
- exploit succeeded
- defanged WTF upload succeeded
- firmware upload started
- device re-enumerated as:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`

Meaning:

- bypassing `0x22006558` was not sufficient to complete the WTF -> RetailOS
  handoff
- the previous host-visible end state did not change
- therefore the exact remaining blocker is either:
  - earlier than the `0x22006558` call on the failing hardware path
  - later than `0x22006558` but still before visible RetailOS proof
  - or a separate prerequisite not expressed through the UART helper return

Updated classification:

- **STILL_BLOCKED_WITH_REASON**

## 2026-04-24 Execute-gate verification after UART bypass failure

This pass verifies the exact execute path after the failed UART-bypass retest.

### Patched image verification

The Nano 3G WTF image actually used still contains the expected patches in both:

- `/tmp/n3g-wtf-defanged-check.bin`
- `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`

Verified body offsets:

- `0x1990`
  - `00 70 a0 e3 22 00 00 ea`
- `0x19ac`
  - `c6 14 00 eb`
- `0x19b8`
  - `08 00 00 0a`
- `0x6558`
  - `00 00 a0 e3 1e ff 2f e1`

So the UART bypass patch was definitely applied to the tested WTF artifact.

### Exact execute wrapper behavior

`0x220024b8` is fully direct:

- input:
  - `r0 = target address`
- body:
  - save `r0` into `r4`
  - call `0x22002138(0)`
  - `blx r4`
  - call `0x22002138(2)` after return

It does **not**:

- read IMG1 entrypoint metadata
- compute a new address
- perform extra validation

So if WTF reaches `0x220024b8`, it simply jumps to the address passed in
through `r0`.

### Execute target on Nano 3G path

The call site remains:

- `0x22001b10: addne r0, r6, r7`
- `0x22001b14: blne 0x220024b8`

with:

- `r6 = 0x08000000`
- `r7 = 0x800`

Therefore the exact execute target is:

- `0x08000800`

### Entrypoint / load-address check

The uploaded Nano 3G OSOS IMG1 still has:

- `Header.Entrypoint = 0`

That matches the Nano 3G haxed-DFU hook:

- `wind3x_n3g.go` forces `g_State->entrypoint = 0`
- `DFUBoot::CopyHeaderBody` copies the IMG1 header to:
  - `0x08000000`
- and the body begins at:
  - `0x08000800`

So the execute path does **not** depend on a nonzero IMG1 entrypoint field.
The runtime target `0x08000800` is the start of the copied IMG1 body, which is
correct for this path.

### OSOS body check at runtime address

Re-disassembly with the correct runtime VMA:

- `arm-none-eabi-objdump ... --adjust-vma=0x08000800`

shows valid code starting exactly at:

- `0x08000800`

So the current payload is not obviously malformed at its runtime entry.

### What remains unresolved

Because the UART bypass did not change the host-visible outcome, the remaining
possibilities are now:

1. control still never reaches:
   - `0x22001b14`
2. control reaches `0x220024b8`, calls:
   - `0x08000800`
   and the RetailOS body immediately returns or crashes
3. some still-missing copy/cache side effect leaves the code at
   `0x08000800` unusable even though the address and wrapper are correct

Static analysis alone does not prove which of these three is happening on
hardware.

### Prepared proof-only patch

A non-invasive proof marker was prepared but **not** integrated or run.

Free space confirmed:

- body offset `0x6d40`
- runtime `0x22006d40`

Prepared proof stub:

- `0x22006d40: e3a00003`
  - `mov r0, #3`
- `0x22006d44: e59f3004`
  - `ldr r3, =0x22002138`
- `0x22006d48: e12fff33`
  - `blx r3`
- `0x22006d4c: eafffffe`
  - infinite loop

Stub bytes:

- `03 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`

Prepared call-site patch:

- runtime address:
  - `0x22001b14`
- body offset:
  - `0x1b14`
- original bytes:
  - `67 02 00 1b`
  - `blne 0x220024b8`
- proof-marker replacement bytes:
  - `89 14 00 1b`
  - `blne 0x22006d40`

Purpose:

- if the execute gate is reached, WTF would switch mode through
  `0x22002138(3)` and then loop forever in a known place
- if nothing changes, the execute gate was not reached

This is a proof marker only, not an attempted fix.

### Decision

- **STILL_BLOCKED_WITH_MAP**

## 2026-04-24 Pre-execute marker prepared for hardware proof

The proof-only execute-gate marker is now integrated into the local Nano 3G
defanged WTF build.

Applied Nano 3G-only patches now include:

- `0x1990`
  - `00 70 a0 e3 22 00 00 ea`
- `0x19ac`
  - `c6 14 00 eb`
- `0x19b8`
  - `08 00 00 0a`
- `0x1b14`
  - original:
    - `67 02 00 1b`
  - replacement:
    - `89 14 00 1b`
  - meaning:
    - redirect `blne 0x220024b8` to `blne 0x22006d40`
- `0x6558`
  - `00 00 a0 e3 1e ff 2f e1`
- `0x6d40`
  - `03 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`

Marker stub behavior:

- `mov r0, #3`
- `ldr r3, =0x22002138`
- `blx r3`
- loop forever

Expected result on hardware:

- if WTF reaches the execute-gate call site at `0x22001b14`, it will branch to
  the proof stub instead of `0x220024b8`
- that should force the known `0x22002138(3)` mode-state change and then stall
  in a known loop
- if there is no behavioral change from the previous runs, the execute gate was
  not reached

Local build status:

- rebuilt:
  - `/tmp/wInd3x/wInd3x`
- regenerated:
  - `/tmp/n3g-wtf-defanged-check.bin`
- verified parsed-image body bytes at:
  - `0x1990`
  - `0x19ac`
  - `0x19b8`
  - `0x1b14`
  - `0x6558`
  - `0x6d40`

Cache state:

- removed:
  - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`

RetailOS image status:

- unchanged

Prepared but not run:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Decision:

- **PRE_EXECUTE_MARKER_PREPARED**

## 2026-04-24 Hardware test of pre-execute marker

One controlled Nano 3G hardware test was performed with the proof-only
pre-execute marker integrated into the defanged WTF.

Observed host sequence:

- clean DFU start:
  - `05ac:1223`
  - DFU state `2`
- cache was empty, so `wInd3x` rebuilt the defanged WTF from the patched local
  source
- exploit succeeded
- defanged WTF upload succeeded
- firmware upload started
- device re-enumerated as:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`

Expected proof behavior if `0x22001b14` had been taken:

- branch to `0x22006d40`
- call `0x22002138(3)`
- loop forever in the marker stub

Observed result:

- no host-visible change relative to the non-marker runs
- no new mode transition attributable to `0x22002138(3)`

Interpretation:

- there is no evidence that the marker branch at `0x22001b14` was taken
- the strongest current conclusion is that the execute-gate call site is not
  reached on the failing hardware path

Updated decision:

- **EXECUTE_GATE_NOT_REACHED**

## 2026-04-24 Upstream path isolation before `0x22001b14`

With the `0x1b14` marker not reached, the path is now reduced from the bottom
up.

### Backward path map from `0x22001b14`

Immediate predecessors:

- `0x22001b14`
  - only predecessor:
    - `0x22001b10`
- `0x22001b10`
  - only predecessor:
    - `0x22001b0c`
- `0x22001b0c`
  - only predecessor:
    - `0x22001b08`
- `0x22001b08`
  - only predecessor:
    - `0x22001b04`
- `0x22001b04`
  - only predecessor:
    - return from `0x22006558` at `0x22001b00`
- `0x22001af4..0x22001b00`
  - only predecessor:
    - straight-line flow from `0x22001ae8`
- `0x22001a24`
  - predecessors:
    - `0x220019c8` when `0x38c00040 & 3 == 0`
    - `0x220019dc` when the status wait completes and the path remains
      successful
    - `0x22001994` only when `r5 & 0x8 == 0` on other paths

Critical conclusion:

- once control reaches `0x22001a24`, the remaining path to `0x22001b14` is
  linear under the currently tested patches:
  - `r4` is set to `0`
  - `0x22006558` is bypassed to immediate `return 0`
  - `r5 & 0x10` is already set on the active `cfw run` path

So the last mandatory block before `0x22001b14` is:

- `0x22001b04`

and the strongest current implication of the failed `0x1b14` marker is:

- `0x22001a24` is likely not reached

### Checkpoint map

- Checkpoint:
  - `0x220016fc`
  - condition: `0x220024d8(...) == 0`
  - expected: `0`
  - failure path: `bne 0x22001b24`
  - patched/tested: not patched

- Checkpoint:
  - `0x22001710`
  - condition: `0x22001c70(...) == 0`
  - expected: `0`
  - failure path: `bne 0x22001ae4`
  - patched/tested: not patched

- Checkpoint:
  - `0x2200176c`
  - condition: `0x2200152c(...) == 0`
  - expected: `0`
  - failure path: `bne 0x22001ae4`
  - patched/tested: not patched

- Checkpoint:
  - `0x22001980`
  - condition: `[service + 0x6c] != 0`
  - expected: nonzero
  - failure path:
    - `r4 = 23`
    - `0x22001a28`
  - patched/tested: traced only

- Checkpoint:
  - `0x220019b0`
  - condition: loader callback success
  - expected: nonzero
  - failure path originally:
    - `r4 = 87`
    - `0x22001a28`
  - patched/tested:
    - patched
    - hardware-tested indirectly

- Checkpoint:
  - `0x220019bc..0x220019dc`
  - condition:
    - `0x38c00040 & 3 == 0`
    - or status wait completes and path remains successful
  - expected: reach `0x22001a24`
  - failure path:
    - `0x220019e0`
    - `r4 = 87`
    - `0x22001a28`
  - patched/tested:
    - not directly marker-tested yet

- Checkpoint:
  - `0x22001a24`
  - condition: post-callback success landing reached
  - expected: yes
  - failure path:
    - any earlier branch to `0x22001a28` / `0x22001ae4` / `0x22001b24`
  - patched/tested:
    - marker prepared as next single test

- Checkpoint:
  - `0x22001b04`
  - condition: return from `0x22006558`
  - expected: yes under current bypass
  - failure path:
    - only if `0x22001a24` never reached
  - patched/tested:
    - marker prepared

- Checkpoint:
  - `0x22001b14`
  - condition: execute gate taken
  - expected: yes if `0x22001a24` reached
  - failure path:
    - not reached in hardware
  - patched/tested:
    - marker tested
    - not reached

### Staged marker patches

Shared marker block in free space:

- body offset:
  - `0x6d40`
- runtime:
  - `0x22006d40`

Marker A:

- runtime:
  - `0x22006d40`
- bytes:
  - `02 00 a0 e3 44 30 9f e5 33 ff 2f e1 fe ff ff ea`
- effect:
  - `0x22002138(2)`
  - loop forever

Marker B:

- runtime:
  - `0x22006d60`
- bytes:
  - `03 00 a0 e3 24 30 9f e5 33 ff 2f e1 fe ff ff ea`
- effect:
  - `0x22002138(3)`
  - loop forever

Marker C:

- runtime:
  - `0x22006d80`
- bytes:
  - `04 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- effect:
  - `0x22002138(4)`
  - loop forever

Prepared branch sites:

- after loader callback success / before AES-status gate:
  - runtime `0x220019bc`
  - body offset `0x19bc`
  - original:
    - `e3 a0 25 e3`
  - replacement:
    - `df 14 00 ea`
    - `b 0x22006d40`

- just after `0x22001a24`:
  - runtime `0x22001a24`
  - body offset `0x1a24`
  - original:
    - `e3 a0 40 00`
  - replacement:
    - `cd 14 00 ea`
    - `b 0x22006d60`

- immediately before the `0x22001b14` path:
  - runtime `0x22001b04`
  - body offset `0x1b04`
  - original:
    - `e3 54 00 00`
  - replacement:
    - `9d 14 00 ea`
    - `b 0x22006d80`

### Next single hardware test

Chosen next split:

- marker at `0x22001a24`

Why this one:

- it is the earliest clean post-callback success landing
- if reached, the currently patched path to `0x22001b14` should be nearly
  linear
- if not reached, the blocker is definitely earlier, most likely in:
  - `0x220019bc..0x220019dc`
  - or an even earlier pre-callback failure leg

Local preparation status:

- local defanger updated to use:
  - `0x1a24 -> cd 14 00 ea`
- previous `0x1b14` marker removed:
  - restored to `67 02 00 1b`
- staged marker block kept at:
  - `0x6d40`
- local `/tmp/wInd3x/wInd3x` rebuilt
- offline artifact regenerated and verified
- cached defanged WTF removed again

Prepared but not run:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Decision:

- **MARKER_PATCH_PREPARED**

## 2026-04-24 Hardware test of upstream marker at `0x22001a24`

The prepared upstream split was tested once without changing the RetailOS
artifact.

Pre-run state:

- `mks5lboot --dfuscan` reported:
  - `05ac:1223`
  - DFU state `2`
- `lsusb` showed:
  - Nano 3G in DFU mode

Patched defanged WTF under test:

- `0x22001a24`
  - original:
    - `e3 a0 40 00`
  - replacement:
    - `cd 14 00 ea`
    - `b 0x22006d60`
- `0x22006d60`
  - effect:
    - call `0x22002138(3)`
    - loop forever
- `0x22001b14`
  - restored to original:
    - `67 02 00 1b`

Observed run sequence:

- local `/tmp/wInd3x/wInd3x cfw run ...` rebuilt the defanged WTF from the
  cleared cache path
- exploit completed
- defanged WTF upload completed
- firmware upload started
- host then remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`

Post-run host-visible state:

- `lsusb` showed:
  - `05ac:1242`
  - Nano 3G WTF mode
- `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`

Interpretation:

- there was no host-visible change attributable to the `0x22001a24` marker
- strongest current conclusion:
  - `0x22001a24` is not reached on the active failing path
- the blocker is therefore earlier, most likely around:
  - `0x220019bc..0x220019dc`
  - or another upstream leg before the post-callback success landing

Recovery status:

- device initially re-enumerated in WTF mode
- a follow-up DFU scan later confirmed recovery:
  - `05ac:1223`
  - DFU state `2`

Decision:

- **MARKER_1A24_NOT_REACHED**

## 2026-04-24 Earlier checkpoint marker prepared before `0x22001a24`

Since the `0x22001a24` marker was not reached, the next isolation step moves
the marker earlier into the `0x22001998..0x220019dc` region.

Selected earlier checkpoints:

- immediately after loader callback return:
  - `0x220019b0`
  - original instruction:
    - `e3500000`
    - `cmp r0, #0`
- middle of the region:
  - `0x220019cc`
  - original instruction:
    - `e592100c`
    - `ldr r1, [r2, #12]`
- just before the success path to `0x22001a24`:
  - `0x220019dc`
  - original instruction:
    - `1a000010`
    - `bne 0x22001a24`

Prepared marker stubs:

- shared stub block:
  - body offset:
    - `0x6d40`
  - runtime:
    - `0x22006d40`
- marker A:
  - runtime:
    - `0x22006d40`
  - bytes:
    - `02 00 a0 e3 44 30 9f e5 33 ff 2f e1 fe ff ff ea`
  - effect:
    - call `0x22002138(2)`
    - loop forever
- marker B:
  - runtime:
    - `0x22006d60`
  - bytes:
    - `03 00 a0 e3 24 30 9f e5 33 ff 2f e1 fe ff ff ea`
  - effect:
    - call `0x22002138(3)`
    - loop forever
- marker C:
  - runtime:
    - `0x22006d80`
  - bytes:
    - `04 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
  - effect:
    - call `0x22002138(4)`
    - loop forever

Prepared branch replacements:

- active first test target:
  - `0x220019b0 -> 0x22006d40`
  - replacement bytes:
    - `e2 14 00 ea`
- prepared but inactive:
  - `0x220019cc -> 0x22006d60`
  - replacement bytes:
    - `e3 14 00 ea`
- prepared but inactive:
  - `0x220019dc -> 0x22006d80`
  - replacement bytes:
    - `e7 14 00 ea`

Current prepared local state:

- active Nano 3G defanged WTF patch:
  - `0x19b0 = e2 14 00 ea`
- later marker removed:
  - `0x1a24` restored to:
    - `00 40 a0 e3`
- `0x1b14` remains restored to original:
  - `67 02 00 1b`
- cached Nano 3G defanged WTF cleared again
- no hardware run performed yet with this earlier marker

Decision:

- **MARKER_EARLY_PREPARED**

## 2026-04-24 Hardware test of earlier marker at `0x220019b0`

The next single hardware split was run with only the earliest marker active.

Pre-run state:

- `mks5lboot --dfuscan` reported:
  - `05ac:1223`
  - DFU state `2`
- `lsusb` showed:
  - Nano 3G in DFU mode

Patch under test:

- `0x220019b0`
  - original:
    - `e3 50 00 00`
    - `cmp r0, #0`
  - replacement:
    - `e2 14 00 ea`
    - `b 0x22006d40`
- `0x22006d40`
  - effect:
    - call `0x22002138(2)`
    - loop forever
- `0x22001a24`
  - restored to original:
    - `00 40 a0 e3`
- `0x22001b14`
  - left original:
    - `67 02 00 1b`

Observed run sequence:

- local `/tmp/wInd3x/wInd3x cfw run ...` rebuilt from the cleared defanged-WTF
  cache
- exploit completed
- defanged WTF upload completed
- firmware upload started
- host then remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`

Post-run host-visible state:

- `lsusb` showed:
  - `05ac:1242`
  - Nano 3G WTF mode
- `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- there was no host-visible change attributable to the branch at:
  - `0x220019b0`
- strongest current conclusion:
  - the active failing path diverges or stalls before `0x220019b0`

Decision:

- **MARKER_19B0_NOT_REACHED**

## 2026-04-24 Loader-callback entry marker prepared before `0x220019b0`

Since `0x220019b0` was not reached, the next split moves to the loader-callback
callsite itself.

Active next test target:

- runtime:
  - `0x220019ac`
- purpose:
  - prove whether execution reaches the callsite before entering the local
    loader stub / BootROM callback path
- original bytes:
  - `c6 14 00 eb`
  - `bl 0x22006ccc`
- replacement bytes:
  - `e3 14 00 ea`
  - `b 0x22006d40`
- stub target:
  - `0x22006d40`
  - marker A calls:
    - `0x22002138(2)`
  - then loops forever

Inactive follow-up split, prepared but not enabled:

- runtime:
  - `0x22006ccc`
- purpose:
  - prove entry into the local loader stub itself if the precall marker is
    still not conclusive later
- original bytes:
  - `f0 43 2d e9`
- prepared replacement bytes:
  - `23 00 00 ea`
  - `b 0x22006d60`
- stub target:
  - `0x22006d60`
  - marker B calls:
    - `0x22002138(3)`
  - then loops forever
- status:
  - not active in this build

Restored later sites:

- `0x220019b0`
  - restored to:
    - `00 00 50 e3`
    - `cmp r0, #0`
- `0x22001a24`
  - restored to:
    - `00 40 a0 e3`
- `0x22001b14`
  - restored to:
    - `67 02 00 1b`

Verification:

- rebuilt local `/tmp/wInd3x/wInd3x`
- regenerated `/tmp/n3g-wtf-defanged-check.bin`
- verified disassembly shows:
  - `0x220019ac: ea0014e3  b 0x22006d40`
  - `0x220019b0: e3500000  cmp r0, #0`
  - `0x22006ccc: f0432de9  stmdb sp!, {r4, r5, r6, r7, r8, r9, lr}`
- cached Nano 3G defanged WTF cleared again

Expected next-run classification:

- if the device behavior changes in a way attributable to the precall marker:
  - **MARKER_PRECALL_REACHED**
- if the run still follows the same no-marker WTF path:
  - **MARKER_PRECALL_NOT_REACHED**

Decision:

- **MARKER_PRECALL_PREPARED**

## 2026-04-24 Hardware test of pre-call marker at `0x220019ac`

The next single hardware split was run with only the loader-callback callsite
marker active.

Pre-run state:

- `mks5lboot --dfuscan` reported:
  - `05ac:1223`
  - DFU state `2`
- `lsusb` showed:
  - Nano 3G in DFU mode

Patch under test:

- `0x220019ac`
  - original:
    - `c6 14 00 eb`
    - `bl 0x22006ccc`
  - replacement:
    - `e3 14 00 ea`
    - `b 0x22006d40`
- `0x22006d40`
  - effect:
    - call `0x22002138(2)`
    - loop forever
- later checkpoints remained restored:
  - `0x220019b0 = 00 00 50 e3`
  - `0x22001a24 = 00 40 a0 e3`
  - `0x22001b14 = 67 02 00 1b`

Observed run sequence:

- local `/tmp/wInd3x/wInd3x cfw run ...` rebuilt from the cleared defanged-WTF
  cache
- exploit completed
- defanged WTF upload completed
- firmware upload started
- host then remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`

Post-run host-visible state:

- `lsusb` showed:
  - `05ac:1242`
  - Nano 3G WTF mode
- `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- there was no host-visible change attributable to the branch at:
  - `0x220019ac`
- strongest current conclusion:
  - the active failing path diverges or stalls before the loader-callback
    callsite itself

Decision:

- **MARKER_PRECALL_NOT_REACHED**

## 2026-04-24 Pre-loader setup marker prepared before `0x220019ac`

Since the pre-call marker at `0x220019ac` was not reached, the next split moves
into the setup block immediately before the callback callsite.

Selected pre-loader checkpoints:

- active first test target:
  - runtime:
    - `0x22001998`
  - purpose:
    - prove whether execution reaches the start of the setup block at all
  - original bytes:
    - `00 00 99 e5`
    - `ldr r0, [r9]`
  - replacement bytes:
    - `e8 14 00 ea`
    - `b 0x22006d40`
  - stub target:
    - `0x22006d40`

- prepared but inactive follow-up:
  - runtime:
    - `0x220019a0`
  - original bytes:
    - `74 30 90 e5`
    - `ldr r3, [r0, #116]`
  - prepared replacement bytes:
    - `ee 14 00 ea`
    - `b 0x22006d60`
  - stub target:
    - `0x22006d60`

- prepared but inactive follow-up:
  - runtime:
    - `0x220019a8`
  - original bytes:
    - `02 1b 86 e2`
    - `add r1, r6, #0x800`
  - prepared replacement bytes:
    - `f4 14 00 ea`
    - `b 0x22006d80`
  - stub target:
    - `0x22006d80`

Marker stubs remain:

- `0x22006d40`
  - call `0x22002138(2)`
  - loop forever
- `0x22006d60`
  - call `0x22002138(3)`
  - loop forever
- `0x22006d80`
  - call `0x22002138(4)`
  - loop forever

Restored later sites:

- `0x220019ac`
  - restored to:
    - `c6 14 00 eb`
    - `bl 0x22006ccc`
- `0x220019b0`
  - restored to:
    - `00 00 50 e3`
- `0x22001a24`
  - restored to:
    - `00 40 a0 e3`
- `0x22001b14`
  - restored to:
    - `67 02 00 1b`

Verification:

- rebuilt local `/tmp/wInd3x/wInd3x`
- regenerated `/tmp/n3g-wtf-defanged-check.bin`
- verified disassembly shows:
  - `0x22001998: ea0014e8  b 0x22006d40`
  - `0x220019ac: eb0014c6  bl 0x22006ccc`
  - `0x220019b0: e3500000  cmp r0, #0`
- cached Nano 3G defanged WTF cleared again

Decision:

- **MARKER_1998_PREPARED**

## 2026-04-24 Hardware test of pre-loader marker at `0x22001998`

The next single hardware split was run with only the earliest pre-loader setup
marker active.

Pre-run state:

- `mks5lboot --dfuscan` reported:
  - `05ac:1223`
  - DFU state `2`
- `lsusb` showed:
  - Nano 3G in DFU mode

Patch under test:

- `0x22001998`
  - original:
    - `00 00 99 e5`
    - `ldr r0, [r9]`
  - replacement:
    - `e8 14 00 ea`
    - `b 0x22006d40`
- `0x22006d40`
  - effect:
    - call `0x22002138(2)`
    - loop forever
- later checkpoints remained restored:
  - `0x220019ac = c6 14 00 eb`
  - `0x220019b0 = 00 00 50 e3`
  - `0x22001a24 = 00 40 a0 e3`
  - `0x22001b14 = 67 02 00 1b`

Observed run sequence:

- local `/tmp/wInd3x/wInd3x cfw run ...` rebuilt from the cleared defanged-WTF
  cache
- exploit completed
- defanged WTF upload completed
- firmware upload started
- host then remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`

Post-run host-visible state:

- `lsusb` showed:
  - `05ac:1242`
  - Nano 3G WTF mode
- `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- there was no host-visible change attributable to the branch at:
  - `0x22001998`
- strongest current conclusion:
  - the active failing path diverges or stalls before the pre-loader setup block
    itself

Decision:

- **MARKER_1998_NOT_REACHED**

## 2026-04-24 Predecessor-path marker prepared before `0x22001998`

The predecessor map shows that `0x22001998` is not entered directly from some
earlier far-away branch. It is reached only by falling through the short block:

- `0x2200198c`
  - `tst r5, #8`
- `0x22001990`
  - `moveq r7, #0`
- `0x22001994`
  - original:
    - `beq 0x22001a24`
- `0x22001998`
  - start of the loader setup block

Required conditions to reach `0x22001998`:

- the earlier service callback at:
  - `0x2200197c: blx r0`
  - must return nonzero, or `0x22001988` branches to `0x22001a28`
- `r5 & 0x8` must be set so that the original conditional branch at
  `0x22001994` is not taken

Important correction:

- the old `0x1990/0x1994` bypass patch made `0x22001998` unreachable in the
  defanged build
- that means the previous `0x22001998` marker was not on a live edge
- this preparation restores the original conditional behavior and moves the
  active marker to the real immediate predecessor edge

Selected predecessor checkpoints:

- active first test target:
  - runtime:
    - `0x22001994`
  - why chosen:
    - it is the immediate predecessor edge into `0x22001998`
    - if this marker fires, the path definitely reached the loader-setup gate
  - original bytes:
    - `22 00 00 0a`
    - `beq 0x22001a24`
  - replacement bytes:
    - `e9 14 00 ea`
    - `b 0x22006d40`
  - stub target:
    - `0x22006d40`

- prepared but inactive:
  - runtime:
    - `0x2200198c`
  - original bytes:
    - `08 00 15 e3`
  - prepared replacement bytes:
    - `f3 14 00 ea`
    - `b 0x22006d60`

- prepared but inactive:
  - runtime:
    - `0x22001988`
  - original bytes:
    - `26 00 00 0a`
  - prepared replacement bytes:
    - `fc 14 00 ea`
    - `b 0x22006d80`

Restored sites:

- `0x22001990`
  - restored to:
    - `00 70 a0 03`
    - `moveq r7, #0`
- `0x22001998`
  - restored to:
    - `00 00 99 e5`
- `0x220019ac`
  - restored to:
    - `c6 14 00 eb`
- `0x220019b0`
  - restored to:
    - `00 00 50 e3`
- `0x22001a24`
  - restored to:
    - `00 40 a0 e3`
- `0x22001b14`
  - restored to:
    - `67 02 00 1b`

Verification:

- rebuilt local `/tmp/wInd3x/wInd3x`
- regenerated `/tmp/n3g-wtf-defanged-check.bin`
- verified disassembly shows:
  - `0x22001990: 03a07000  moveq r7, #0`
  - `0x22001994: ea0014e9  b 0x22006d40`
  - `0x22001998: e5990000  ldr r0, [r9]`
- cached Nano 3G defanged WTF cleared again

Decision:

- **PREDECESSOR_MARKER_PREPARED**

## 2026-04-24 Hardware test of predecessor marker at `0x22001994`

The next single hardware split was run with only the real predecessor-edge
marker active.

Pre-run state:

- `mks5lboot --dfuscan` reported:
  - `05ac:1223`
  - DFU state `2`
- `lsusb` showed:
  - Nano 3G in DFU mode

Patch under test:

- `0x22001994`
  - original:
    - `22 00 00 0a`
    - `beq 0x22001a24`
  - replacement:
    - `e9 14 00 ea`
    - `b 0x22006d40`
- `0x22006d40`
  - effect:
    - call `0x22002138(2)`
    - loop forever
- restored predecessor path bytes remained:
  - `0x22001990 = 00 70 a0 03`
  - `0x22001998 = 00 00 99 e5`
- later checkpoints remained restored:
  - `0x220019ac = c6 14 00 eb`
  - `0x220019b0 = 00 00 50 e3`
  - `0x22001a24 = 00 40 a0 e3`
  - `0x22001b14 = 67 02 00 1b`

Observed run sequence:

- local `/tmp/wInd3x/wInd3x cfw run ...` rebuilt from the cleared defanged-WTF
  cache
- exploit completed
- defanged WTF upload completed
- firmware upload started
- host then remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`

Post-run host-visible state:

- `lsusb` showed:
  - `05ac:1242`
  - Nano 3G WTF mode
- `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- there was no host-visible change attributable to the branch at:
  - `0x22001994`
- strongest current conclusion:
  - the active failing path diverges or stalls before the real predecessor edge
    into the loader-setup block

Decision:

- **MARKER_1994_NOT_REACHED**

## 2026-04-24 Callback-result marker prepared at `0x22001988`

Since the predecessor-edge marker at `0x22001994` was not reached, the next
split moves earlier into the callback-result decision path itself.

Active next test target:

- runtime:
  - `0x22001988`
- purpose:
  - prove whether the path reaches the callback-result failure branch after the
    `[service + 0x6c]` callback return
- original bytes:
  - `26 00 00 0a`
  - `beq 0x22001a28`
- replacement bytes:
  - `fc 14 00 ea`
  - `b 0x22006d80`
- stub target:
  - `0x22006d80`
  - marker C calls:
    - `0x22002138(4)`
  - then loops forever

Prepared but inactive follow-up:

- runtime:
  - `0x2200198c`
- original bytes:
  - `08 00 15 e3`
- prepared replacement bytes:
  - `f3 14 00 ea`
  - `b 0x22006d60`
- stub target:
  - `0x22006d60`

Restored sites:

- `0x22001994`
  - restored to:
    - `22 00 00 0a`
    - `beq 0x22001a24`
- `0x22001998`
  - restored to:
    - `00 00 99 e5`
- `0x220019ac`
  - restored to:
    - `c6 14 00 eb`
- `0x220019b0`
  - restored to:
    - `00 00 50 e3`
- `0x22001a24`
  - restored to:
    - `00 40 a0 e3`
- `0x22001b14`
  - restored to:
    - `67 02 00 1b`

Verification:

- rebuilt local `/tmp/wInd3x/wInd3x`
- regenerated `/tmp/n3g-wtf-defanged-check.bin`
- verified disassembly shows:
  - `0x22001988: ea0014fc  b 0x22006d80`
  - `0x22001994: 0a000022  beq 0x22001a24`
  - `0x220019ac: eb0014c6  bl 0x22006ccc`
- cached Nano 3G defanged WTF cleared again

Expected next-run classification:

- **MARKER_1988_REACHED**
  - if the callback-result branch site is actually reached
- **MARKER_1988_NOT_REACHED**
  - if the run still follows the same no-marker WTF path

Decision:

- **MARKER_1988_PREPARED**

## 2026-04-24 Hardware test of callback-result marker at `0x22001988`

The next single hardware split was run with only the callback-result branch
marker active.

Pre-run state:

- `mks5lboot --dfuscan` reported:
  - `05ac:1223`
  - DFU state `2`
- `lsusb` showed:
  - Nano 3G in DFU mode

Patch under test:

- `0x22001988`
  - original:
    - `26 00 00 0a`
    - `beq 0x22001a28`
  - replacement:
    - `fc 14 00 ea`
    - `b 0x22006d80`
- `0x22006d80`
  - effect:
    - call `0x22002138(4)`
    - loop forever
- restored sites remained:
  - `0x22001994 = 22 00 00 0a`
  - `0x22001998 = 00 00 99 e5`
  - `0x220019ac = c6 14 00 eb`
  - `0x220019b0 = 00 00 50 e3`
  - `0x22001a24 = 00 40 a0 e3`
  - `0x22001b14 = 67 02 00 1b`

Observed run sequence:

- local `/tmp/wInd3x/wInd3x cfw run ...` rebuilt from the cleared defanged-WTF
  cache
- exploit completed
- defanged WTF upload completed
- firmware upload started
- host then remained in repeated:
  - `handle_events: error: libusb: interrupted [code -10]`

Post-run host-visible state:

- `lsusb` showed:
  - `05ac:1242`
  - Nano 3G WTF mode
- `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- there was no host-visible change attributable to the branch at:
  - `0x22001988`
- strongest current conclusion:
  - the active failing path diverges or stalls before the callback-result
    branch itself

Decision:

- **MARKER_1988_NOT_REACHED**

## 2026-04-24 Callback pre-call marker prepared at `0x2200197c`

The next clean split isolates the `[service + 0x6c]` readiness callback itself.

Active first test target:

- runtime:
  - `0x2200197c`
- purpose:
  - prove whether execution reaches the callback callsite at all
- original bytes:
  - `30 ff 2f e1`
  - `blx r0`
- replacement bytes:
  - `ef 14 00 ea`
  - `b 0x22006d40`
- stub target:
  - `0x22006d40`
  - marker A calls:
    - `0x22002138(2)`
  - then loops forever

Prepared but inactive follow-up:

- runtime:
  - `0x22001980`
- purpose:
  - prove whether the readiness callback returns
- original bytes:
  - `00 00 50 e3`
  - `cmp r0, #0`
- prepared replacement bytes:
  - `f6 14 00 ea`
  - `b 0x22006d60`
- stub target:
  - `0x22006d60`
  - marker B calls:
    - `0x22002138(3)`
  - then loops forever
- status:
  - not active in this build

Restored sites:

- `0x22001988`
  - restored to:
    - `26 00 00 0a`
- `0x22001994`
  - restored to:
    - `22 00 00 0a`
- `0x22001998`
  - restored to:
    - `00 00 99 e5`
- `0x220019ac`
  - restored to:
    - `c6 14 00 eb`
- `0x220019b0`
  - restored to:
    - `00 00 50 e3`
- `0x22001a24`
  - restored to:
    - `00 40 a0 e3`
- `0x22001b14`
  - restored to:
    - `67 02 00 1b`

Verification:

- rebuilt local `/tmp/wInd3x/wInd3x`
- regenerated `/tmp/n3g-wtf-defanged-check.bin`
- verified disassembly shows:
  - `0x2200197c: ea0014ef  b 0x22006d40`
  - `0x22001980: e3500000  cmp r0, #0`
  - `0x22001988: 0a000026  beq 0x22001a28`
- cached Nano 3G defanged WTF cleared again

Expected next-run classifications:

- **PRECALL_NOT_REACHED**
  - if there is still no marker-attributable change
- **PRECALL_REACHED_POSTCALL_PENDING**
  - if the pre-call marker is reached, in which case the next test becomes the
    prepared post-call marker at `0x22001980`

Decision:

- **CALLBACK_PRECALL_MARKER_PREPARED**

## 2026-04-24 Hardware test of callback pre-call marker at `0x2200197c`

The callback isolation test was run once with only the pre-call marker active.

Pre-run state:

- `mks5lboot --dfuscan` reported:
  - `05ac:1223`
  - DFU state `2`
- `lsusb` showed:
  - Nano 3G in DFU mode

Patch under test:

- `0x2200197c`
  - original:
    - `30 ff 2f e1`
    - `blx r0`
  - replacement:
    - `ef 14 00 ea`
    - `b 0x22006d40`
- `0x22006d40`
  - effect:
    - call `0x22002138(2)`
    - loop forever
- post-call marker at `0x22001980` remained prepared but inactive

Observed run sequence:

- local `/tmp/wInd3x/wInd3x cfw run ...` rebuilt from the cleared defanged-WTF
  cache
- exploit completed
- defanged WTF upload completed
- firmware upload started
- unlike the previous no-marker runs, the host log then went quiet instead of
  continuing to print repeated:
  - `handle_events: error: libusb: interrupted [code -10]`

Post-run host-visible state:

- `lsusb` showed:
  - `05ac:1242`
  - Nano 3G WTF mode
- `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- this run did not match the earlier “not reached” pattern
- the changed host-side behavior is the first evidence that the pre-call marker
  likely executed before WTF settled
- strongest current classification:
  - the readiness callback callsite is likely reached
  - the next clean split should be the prepared post-call marker at
    `0x22001980` to determine whether the callback returns

Cross-repo state:

- active hardware-test code changes are in:
  - `/tmp/wInd3x/pkg/cfw/defang_wtf.go`
  - `/tmp/wInd3x/pkg/image/image.go`
- session notes and conclusions are recorded in:
  - `RockBox_Personal-master/docs/porting/...`

Decision:

- **PRECALL_REACHED_POSTCALL_PENDING**

## 2026-04-24 Callback post-call marker prepared at `0x22001980`

The next split activates the first instruction after the readiness callback
returns.

Active next test target:

- runtime:
  - `0x22001980`
- purpose:
  - prove whether the `[service + 0x6c]` readiness callback returns to WTF
- original bytes:
  - `00 00 50 e3`
  - `cmp r0, #0`
- replacement bytes:
  - `f6 14 00 ea`
  - `b 0x22006d60`
- stub target:
  - `0x22006d60`
  - marker B calls:
    - `0x22002138(3)`
  - then loops forever

Restored pre-call site:

- `0x2200197c`
  - restored to:
    - `30 ff 2f e1`
    - `blx r0`

Restored later sites:

- `0x22001988`
  - restored to:
    - `26 00 00 0a`
- `0x22001994`
  - restored to:
    - `22 00 00 0a`
- `0x22001998`
  - restored to:
    - `00 00 99 e5`
- `0x220019ac`
  - restored to:
    - `c6 14 00 eb`
- `0x220019b0`
  - restored to:
    - `00 00 50 e3`
- `0x22001a24`
  - restored to:
    - `00 40 a0 e3`
- `0x22001b14`
  - restored to:
    - `67 02 00 1b`

Verification:

- rebuilt local `/tmp/wInd3x/wInd3x`
- regenerated `/tmp/n3g-wtf-defanged-check.bin`
- verified disassembly shows:
  - `0x2200197c: e12fff30  blx r0`
  - `0x22001980: ea0014f6  b 0x22006d60`
  - `0x22001988: 0a000026  beq 0x22001a28`
- cached Nano 3G defanged WTF cleared again

Expected next-run classifications:

- **POSTCALL_REACHED**
  - if the readiness callback returns
- **POSTCALL_NOT_REACHED**
  - if the readiness callback never returns

Decision:

- **POSTCALL_MARKER_PREPARED**

## 2026-04-24 Hardware test of callback post-call marker at `0x22001980`

The callback-return split was run once with only the post-call marker active.

Pre-run state:

- `mks5lboot --dfuscan` reported:
  - `05ac:1223`
  - DFU state `2`
- `lsusb` showed:
  - Nano 3G in DFU mode

Patch under test:

- `0x22001980`
  - original:
    - `00 00 50 e3`
    - `cmp r0, #0`
  - replacement:
    - `f6 14 00 ea`
    - `b 0x22006d60`
- `0x22006d60`
  - effect:
    - call `0x22002138(3)`
    - loop forever
- callback entry remained restored:
  - `0x2200197c = 30 ff 2f e1`

Observed run sequence:

- local `/tmp/wInd3x/wInd3x cfw run ...` rebuilt from the cleared defanged-WTF
  cache
- exploit completed
- defanged WTF upload completed
- firmware upload started
- after that, the host returned to the older repeated:
  - `handle_events: error: libusb: interrupted [code -10]`
  pattern

Post-run host-visible state:

- `lsusb` showed:
  - `05ac:1242`
  - Nano 3G WTF mode
- `mks5lboot --dfuscan` reported:
  - `LIBUSB_ERROR_BUSY`
- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- this run behaved differently from the pre-call marker run, which had gone
  quiet after firmware upload
- strongest combined reading of the split pair:
  - pre-call marker likely reached
  - post-call marker not reached
  - therefore the `[service + 0x6c]` readiness callback is entered but does not
    return

Decision:

- **POSTCALL_NOT_REACHED**
- **CALLBACK_ENTERED_NO_RETURN**

## 2026-04-24 BootROM readiness callback `0x200036c8` traced and local bypass prepared

The BootROM readiness callback at `0x200036c8` is now traced far enough to
justify a minimal local Nano 3G bypass in defanged WTF.

Disassembly summary for `0x200036c8`:

- prologue:
  - `push {r4, r5, r6, lr}`
  - `r4 = 0x2203fff8`
  - `r5 = 0`
- main loop head:
  - `bl 0x20009e20`
  - load `state = *0x2203fff8`
  - test:
    - `[state + 0x2c]`
- ready / success-side path:
  - if `[state + 0x2c] != 0`, inspect:
    - `[state + 0x738 + 0x36]`
  - if that byte is nonzero:
    - compute `([state + 0x30] + [state + 0x738 + 0x2c])`
    - call `0x200034a0`
    - re-check `[state + 0x2c]`
  - success sets:
    - `r5 = 1`
- return path:
  - `bl 0x2000a26c`
  - explicit writes:
    - `*0x2203fff8 = (*0x2203fff8)->next`
    - `*0x2203fffc = (*0x2203fffc + 0x2000)->0x720`
  - return value:
    - `r0 = r5`
- non-returning wait path:
  - if `[state + 0x2c] == 0`, poll:
    - `[state + 0x738 + 0x37]`
  - if that byte remains `0`, branch back to the loop head
  - if it becomes nonzero:
    - test `[state + 0x04]`
    - if `[state + 0x04] != 0`, branch back to the loop head again

So `0x200036c8` is not just a boolean callback. It is a readiness wait loop
with two concrete scratch-global updates on return:

- `0x2203fff8`
- `0x2203fffc`

Current conclusion:

- the blocker is the wait loop inside `0x200036c8`, not the later WTF path
- the only directly observed persistent side effects in this callback are the
  two scratch-global writes above
- helper calls:
  - `0x20009e20`
  - `0x2000a26c`
  were not proven to provide additional state that the immediate WTF handoff
  path consumes after `0x22001980`

Prepared Nano 3G-only defanged WTF patch in local `/tmp/wInd3x/pkg/cfw/defang_wtf.go`:

- callsite:
  - runtime address:
    - `0x2200197c`
  - body offset:
    - `0x197c`
  - original bytes:
    - `30 ff 2f e1`
    - `blx r0`
  - replacement bytes:
    - `1f 15 00 eb`
    - `bl 0x22006e00`
- return site kept original:
  - `0x22001980`
    - `00 00 50 e3`
    - `cmp r0, #0`
- local stub:
  - runtime address:
    - `0x22006e00`
  - body offset:
    - `0x6e00`
  - behavior:
    - `push {r4, lr}`
    - advance `*0x2203fff8` to its next node
    - refresh `*0x2203fffc` from `(*0x2203fffc + 0x2000)->0x720`
    - `mov r0, #1`
    - `pop {r4, pc}`

Offline verification after rebuild:

- rebuilt local tool:
  - `/tmp/wInd3x/wInd3x`
- regenerated:
  - `/tmp/n3g-wtf-defanged-check.bin`
- verified body disassembly:
  - `0x2200197c: eb00151f  bl 0x22006e00`
  - `0x22001980: e3500000  cmp r0, #0`
  - `0x22006e00: e92d4010  push {r4, lr}`
  - `0x22006e28: e3a00001  mov r0, #1`
  - `0x22006e2c: e8bd8010  pop {r4, pc}`
- cleared cached defanged WTF again:
  - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`

Decision:

- **READINESS_STUB_PREPARED**

Status:

- no hardware `cfw run` was performed with this readiness-stub build yet

## 2026-04-24 One hardware run with the readiness callback stub

One controlled Nano 3G hardware run was performed with the readiness-callback
stub active.

Pre-run state:

- `mks5lboot --dfuscan` reported:
  - `05ac:1223`
  - DFU state `2`
- `lsusb` showed:
  - Nano 3G present in DFU mode

Patch under test:

- `0x2200197c`
  - `30 ff 2f e1` -> `1f 15 00 eb`
  - `blx r0` -> `bl 0x22006e00`
- `0x22006e00`
  - replay:
    - `*0x2203fff8 = (*0x2203fff8)->next`
    - `*0x2203fffc = (*0x2203fffc + 0x2000)->0x720`
  - return:
    - `r0 = 1`
- RetailOS image remained unchanged:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed host-side run result:

- exploit completed
- defanged WTF upload completed
- then local `wInd3x` failed with:
  - `Error: device did not switch to WTF mode: context deadline exceeded`
- after that:
  - no `05ac:1242` WTF device appeared
  - no DFU device appeared either
  - repeated `lsusb` and `mks5lboot --dfuscan` checks showed the Nano absent from
    USB entirely

Recovery:

- after manual reboot and DFU re-entry, recovery was confirmed:
  - `05ac:1223`
  - DFU state `2`

Interpretation:

- this is a real behavior change from the prior stuck-WTF pattern
- the readiness stub was sufficient to stop the old:
  - `05ac:1242`
  - `CHAINLOAD_WTF_STUCK`
  path
- host-side evidence now suggests one of:
  - the chainload progressed past the previous WTF blocker and dropped USB while
    booting
  - the device reset or crashed before any stable USB re-enumeration
- because no direct screen observation was captured here, the most defensible
  host-only classification is:
  - **CHAINLOAD_BOOT_PROGRESS**

Decision:

- **CHAINLOAD_BOOT_PROGRESS**

## 2026-04-24 OSOS backlight-enable patch prepared

The previous RetailOS runtime visibility patch still produced a black screen,
even though the Nano was now leaving WTF and disappearing from USB. The next
step is therefore an earlier Apple-owned display/backlight hook, not another
later connected-screen proof patch.

Recovered low-risk callable path:

- `0x22005640`
  - `bl 0x2200374c`
  - `bl 0x22003774`
  - `bl 0x220073b4`
  - `mov r1, #1`
  - `bl 0x22007610`
- this remains the best compact Apple "visibility on" wrapper visible in the
  decrypted Nano 3G OSOS body

Selected startup hook:

- callsite:
  - runtime `0x22003a9c`
  - body offset `0x3a9c`
- original bytes:
  - `bd 00 00 eb`
  - `bl 0x22003d98`
- replacement bytes:
  - `43 14 00 eb`
  - `bl 0x22008bb0`

Local stub:

- runtime `0x22008bb0`
- body offset `0x8bb0`
- original bytes:
  - `00` repeated for 16 bytes
- replacement bytes:
  - `04 e0 2d e5`
  - `77 ec ff eb`
  - `a0 f2 ff eb`
  - `04 f0 9d e4`
- disassembly:
  - `push {lr}`
  - `bl 0x22003d98`
  - `bl 0x22005640`
  - `pop {pc}`

Meaning:

- preserve the original Apple startup helper first
- then call the compact Apple "visibility on" wrapper while still in early OSOS
  startup
- avoid new raw PMU/MMIO guesses and avoid touching NAND, storage, USB PHY, or
  shutdown paths

Prepared artifacts:

- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_backlight_signal.go`
- built patcher:
  - `/tmp/patch_n3g_backlight_signal`
- patched IMG1:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-backlight-n3g.dfu`
- patched body dump:
  - `/tmp/n3g-osos-work/lcd-osos-backlight-n3g.bin`

Verification:

- `0x22003a9c` now disassembles as:
  - `eb001443  bl 0x22008bb0`
- `0x22008bb0` now contains the local startup/backlight stub above
- the earlier connected-runtime proof strings and `0x57d0 -> nop` patch were
  retained in the same image for observability

Decision:

- **PATCH_READY**

## 2026-04-24 Hardware result for early OSOS backlight hook

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-backlight-n3g.dfu`

Observed host behavior:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano disappeared from USB entirely
- it did not re-enumerate as:
  - `05ac:1242`
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Observed screen behavior:

- black screen for the full 90-second observation window
- no Apple logo
- no backlight
- no flicker
- no patched text
- no stock text
- no visible reset loop

Interpretation:

- the early `0x22005640` startup hook did not produce any visible display or
  backlight signal
- host behavior still indicates progress beyond the old WTF-stuck state
- but the runtime remains non-visible from the screen side

Classification:

- **RUNTIME_BLACKSCREEN**

## 2026-04-24 Later OSOS hook map and Apple-owned beep proof prepared

The early OSOS startup/display hooks are now mapped well enough to stop
guessing at `0x22005640`.

Startup reachability map:

- OSOS entry:
  - `0x22008814`
  - branches directly to:
    - `0x220039c4`
- early startup path:
  - `0x220039c4`
  - `bl 0x22004778`
  - `bl 0x220045b0`
  - `bl 0x22003594`
  - `bl 0x220044c4`
  - `bl 0x220045ac`
  - `bl 0x2200359c`
  - `bl 0x220035a4`
  - `bl 0x22003ac4`
  - `bl 0x22003b28`
  - later startup helper:
    - `0x22003a9c`
    - `bl 0x22003d98`
- early display/visibility cluster:
  - `0x22005620`
  - `0x22005640`
  - `0x22005660`
  - connected visibility branch:
    - `0x220057d0`

Meaning:

- the earlier `0x22003a9c -> 0x22005640` patch targeted a real early startup
  path
- but the black-screen hardware result means that path is not enough to prove
  later Apple runtime execution or visible output

Later high-confidence runtime path selected:

- controller/runtime handler:
  - `0x2210023c..0x22100324`
- selected hook site:
  - `0x22100318`
  - this is later than the early display wrappers and sits inside a controller
    path that already uses Apple-owned audio/UI helpers

Recovered Apple-owned sound path:

- `0x2219d410`
  - high-level runtime helper that resolves/creates a `Beep` object and then
    calls:
    - `0x22272058`
- supporting helpers:
  - `0x2219d1e8`
  - `0x2219d048`
  - `0x22276c98`
- relevant runtime strings:
  - `Beep`
  - `PlayTone`
  - `DiskMode_ScreenLayout_Connected`
  - `RemoteUI_Ok_To_Disconnect_String`

Prepared later-hook proof patch:

- input IMG1:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`
- output IMG1:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-later-beep-n3g.dfu`
- output body dump:
  - `/tmp/n3g-osos-work/lcd-osos-later-beep-n3g.bin`
- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_later_beep_signal.go`

Exact patch:

- hook site:
  - runtime `0x22100318`
  - body offset `0x100318`
  - original:
    - `91 fb ff eb`
    - `bl 0x220ff164`
  - replacement:
    - `2c 22 fc eb`
    - `bl 0x22008bd0`
- local stub:
  - runtime `0x22008bd0`
  - body offset `0x8bd0`
  - original:
    - 28 bytes of `00`
  - replacement disassembly:
    - `push {r0, r1, lr}`
    - `bl 0x220ff164`
    - `ldr r0, [sp]`
    - `ldr r0, [r0, #0xc8]`
    - `add r1, sp, #12`
    - `bl 0x2219d410`
    - `pop {r0, r1, pc}`

Why this hook was chosen:

- it is later than the failed early display hooks
- it preserves the original Apple call at `0x22100318`
- it reuses the caller's existing stack frame and Apple-owned runtime audio
  helper
- it does not add NAND, storage, USB PHY, shutdown, or guessed raw MMIO writes

Verification:

- `0x22100318` now disassembles as:
  - `ebfc222c  bl 0x22008bd0`
- `0x22008bd0` now contains the local later-runtime beep stub above
- no hardware run performed yet

Decision:

- **LATER_HOOK_PATCH_READY**

## 2026-04-24 Hardware result for later Apple-owned beep proof hook

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-later-beep-n3g.dfu`

Observed host behavior:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano disappeared from USB entirely
- it did not re-enumerate as:
  - `05ac:1242`
- post-run checks showed:
  - no USB Nano present
  - no DFU device present

Observed device behavior:

- black screen for the full observation window
- no audible beep
- no tone
- no click
- no Apple logo
- no backlight
- no flicker
- no patched text
- no stock text
- no visible reset loop

Interpretation:

- the later Apple-owned `Beep` proof hook did not produce any observable audio
  or display signal
- host behavior still indicates progress beyond the old WTF-stuck state
- but there is still no proof that later Apple UI/runtime is becoming visible or
  audible on Nano 3G hardware

Decision:

- **RUNTIME_SIGNAL_BLOCKED**

## 2026-04-24 OSOS scheduler / main-loop discovery

Objective:

- find the earliest always-reached post-handoff OSOS execution point that does
  not depend on display or audio
- prepare a low-risk runtime proof image without changing WTF again

Recovered entry-to-idle path:

- `0x08000800` eventually reaches:
  - `0x22008814`
  - `0x220039c4`
- the startup body at `0x220039c4` performs the early init sequence and later
  exits through:
  - `0x22003aa4: b 0x22003af8`

First stable post-init loop:

- `0x22003af8..0x22003b1c`
- disassembly:
  - `0x22003af8: nop`
  - `0x22003afc: nop`
  - `0x22003b00: nop`
  - `0x22003b04: nop`
  - `0x22003b08: mcr p15, 0, r0, c7, c0, 4`
  - `0x22003b1c: b 0x22003af8`

Why this hook point was chosen:

- it is the first stable loop reached after the recovered init sequence
- it is unconditional from the `0x220039c4` startup path
- it is not gated on display state, audio state, USB UI state, or later menu
  logic
- it is lower risk than guessing at later scheduler tasks because the loop head
  is explicit in the disassembly

Related repeated callback:

- `0x22003ac4` arms a recurring callback through:
  - `0x220035f4`
  - `0x220035fc`
- literal target:
  - `0x22003af4`
  - `b 0x22001d48`
- `0x22001d48` looks timer/tick-like, but the first proof target remains the
  unconditional loop head at `0x22003af8`

Prepared proof image:

- `/tmp/n3g-osos-work/n3g-osos-scheduler-probe.dfu`
- body:
  - `/tmp/n3g-osos-work/lcd-osos-scheduler-probe.bin`

Patch details:

- hook site:
  - runtime:
    - `0x22003af8`
  - body offset:
    - `0x3af8`
  - original:
    - `00 00 a0 e1`
    - `nop`
  - replacement:
    - `3c 14 00 ea`
    - `b 0x22008bf0`
- local stub:
  - runtime:
    - `0x22008bf0`
  - body offset:
    - `0x8bf0`
  - original:
    - 16 bytes of `00`
  - replacement disassembly:
    - `mov r1, #0x3c800000`
    - `mov r0, #0x00100000`
    - `str r0, [r1]`
    - `b .`

Verification:

- `0x22003af8` now disassembles as:
  - `ea00143c  b 0x22008bf0`
- `0x22008bf0` now disassembles as:
  - `mov r1, #0x3c800000`
  - `mov r0, #0x00100000`
  - `str r0, [r1]`
  - `b .`
- no hardware run performed yet

Decision:

- **HOOK_POINT_FOUND**

## 2026-04-24 Hardware result for scheduler / main-loop probe

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-scheduler-probe.dfu`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano disappeared from USB entirely
- post-run checks showed:
  - no Nano present in `lsusb`
  - no DFU device present in `mks5lboot --dfuscan`

Observed device behavior:

- black screen for the full observation window
- no immediate reboot
- no delayed reboot
- no repeated reboot loop
- no Apple logo
- no flicker
- no backlight
- no audible sound

Interpretation:

- the watchdog-reset stub at the recovered loop head did not produce the
  expected reboot signature
- strongest current reading:
  - either `0x22003af8` is not actually reached on hardware
  - or the assumed watchdog write is not effective in this post-handoff OSOS
    environment

Decision:

- **SCHEDULER_PROBE_NOT_TRIGGERED**

## 2026-04-24 OSOS entry-loop probe prepared after failed scheduler watchdog test

Reason for redesign:

- the scheduler watchdog probe at `0x22003af8` did not produce any reboot
  signature
- that left two plausible explanations:
  - the recovered idle shell is not actually reached on hardware
  - the watchdog write is not effective in this OSOS runtime context

Corrected entry mapping:

- `0x22000800` is ordinary body code, not the one-time OSOS reset entry
- the real startup shell is:
  - `0x22000000 -> 0x22008808 -> 0x22008814 -> 0x220039c4`

Why `0x22008814` is the right first structural cutoff:

- `0x22008808` has already established the stack pointer
- `0x22008814` is the direct branch into the full startup body at `0x220039c4`
- trapping here is safer and more targeted than clobbering unrelated code at
  `0x22000800`
- if OSOS entry is actually executed, this probe should divert execution before
  the larger RTXC/service-heavy startup chain runs

Later repeated/runtime candidates identified during the same pass:

- `0x22003ac4`
  - arms a recurring callback path through:
    - `0x220035f4`
    - `0x220035fc`
  - literal target:
    - `0x22003af4`
    - `b 0x22001d48`
- `0x22003b28`
  - issues RTXC-style service calls through:
    - `0x2200360c`

Prepared entry probe image:

- `/tmp/n3g-osos-work/n3g-osos-entry-loop-probe.dfu`
- body:
  - `/tmp/n3g-osos-work/lcd-osos-entry-loop-probe.bin`

Patch details:

- hook site:
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
  - still loads the startup stack pointer
- `0x22008814`
  - now disassembles as:
    - `eafffffe  b 0x22008814`
- no hardware run performed yet

Interpretation target:

- if hardware behavior changes relative to the current "USB disappears after
  handoff" baseline, OSOS entry was reached
- if behavior remains identical, that does not conclusively prove entry was not
  reached, because USB teardown may already happen before this branch point

Decision:

- **OSOS_ENTRY_PROBE_PREPARED**

## 2026-04-24 Hardware result for OSOS entry-loop probe

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-entry-loop-probe.dfu`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano disappeared from USB entirely
- post-run checks showed:
  - no Nano present in `lsusb`
  - no DFU device present in `mks5lboot --dfuscan`

Interpretation:

- the entry-loop probe did not produce any host-visible behavior change versus
  the current post-WTF baseline
- strongest current classification:
  - `ENTRY_LOOP_NOT_REACHED`
- but this result is still structurally ambiguous, because USB disappearance may
  happen before the probe point at `0x22008814`

Decision:

- **ENTRY_LOOP_NOT_REACHED**

## 2026-04-24 Corrected WTF -> OSOS execute path mapping

Scope correction:

- the post-loader chainload path lives in the WTF body, not the OSOS body
- all addresses below are from:
  - `/tmp/n3g-wtf-decrypted.body.bin`

### Active handoff entry into the WTF execute path

The active Nano 3G `cfw run` path enters:

- `0x22001698`

from:

- `0x22002fe4`

with:

- `r0 = 0x1d`
- `r1 = 0x08000000`
- `r2 = 0x00f80000`

Function prologue at `0x22001698`:

- `push {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, sl, fp, lr}`
- `mov r6, r1`
- `mov r7, r2`
- `mov r5, r0`
- `sub sp, sp, #0x18c`

Meaning:

- launch base is preserved in:
  - `r6 = 0x08000000`
- launch size is preserved in:
  - `r7 = 0x00f80000`
- mode flags in:
  - `r5 = 0x1d`
- stack frame is valid and ARM state is preserved through the path

### Post-loader path after readiness callback

After the readiness callback:

- `0x2200197c: blx r0`
- `0x22001980: cmp r0, #0`

the path continues through:

- optional `[service + 0x74]` callback
  - `0x220019ac: blx r3`
- BootROM setter callbacks from `[service + 0x8c]`
  - `0x22001a78: blx r2` with ID `19`
  - `0x22001ab0: blx r2` with ID `33`
- service/UART phase
  - `0x22001b00: bl 0x22006558`

Final execute gate:

- `0x22001b04: cmp r4, #0`
- `0x22001b0c: tst r5, #16`
- `0x22001b10: addne r0, r6, r7`
- `0x22001b14: blne 0x220024b8`

Because this path enters `0x22001698` with:

- `r5 = 0x1d`

the `tst r5, #16` gate is statically armed on the active chainload path.

### Exact transfer wrapper and final OSOS jump

Execute wrapper in WTF:

- `0x220024b8: push {r4, lr}`
- `0x220024bc: mov r4, r0`
- `0x220024c0: mov r0, #0`
- `0x220024c4: bl 0x22002138`
- `0x220024c8: blx r4`
- `0x220024cc: pop {r4, lr}`
- `0x220024d0: mov r0, #2`
- `0x220024d4: b 0x22002138`

This is the exact WTF -> OSOS transfer instruction:

- `0x220024c8: blx r4`

### Execute target and machine state

At the execute gate:

- `r0 = r6 + r7`

On the active path:

- `r6 = 0x08000000`
- `r7 = 0x00000800`

so:

- `r0 = 0x08000800`

Then inside the wrapper:

- `r4 = r0 = 0x08000800`
- `blx r4` branches to:
  - `0x08000800`

State at transfer:

- address is word-aligned and bit 0 is clear
- `blx r4` from ARM state to an even address keeps execution in ARM state
- stack is valid:
  - caller frame from `0x22001698`
  - wrapper frame from `0x220024b8`
- no Thumb transition is involved

### Failure class

Previous hardware proof markers established:

- pre-execute marker at `0x22001b14` did not trigger
- unchanged host behavior with the later OSOS entry-loop probe does not rescue
  that path

So the strongest current failure classification remains:

- **EXECUTE_NEVER_CALLED**

This means:

- the intended WTF -> OSOS transfer instruction is known exactly
- but current hardware behavior still gives no evidence that control reaches
  `0x22001b14` / `0x220024b8`

### Fix status

No minimal execute-forcing patch is justified yet.

Reason:

- forcing an unconditional jump at `0x22001b10/0x22001b14` would skip still
  unresolved earlier WTF/runtime state and would be speculative

Decision:

- **EXECUTE_PATH_MAPPED**

## 2026-04-24 Final execute-gate branch map and last unresolved marker

This pass traces the full active handoff path inside:

- `/tmp/n3g-wtf-decrypted.body.bin`

from:

- `0x22001698`

to the execute gate:

- `0x22001b04..0x22001b14`

### Branch/call map from `0x22001698`

1. `0x220016f8: bl 0x220024d8`
   - purpose:
     - initial setup/service stage
   - check:
     - `0x220016fc: cmp r0, #0`
   - expected:
     - `r0 = 0`
   - failure:
     - `0x22001700: bne 0x22001b24`
     - immediate function return
   - status:
     - unresolved
     - not directly patched/tested in current readiness-stub build

2. `0x2200170c: bl 0x22001c70`
   - purpose:
     - normalize/validate launch base
   - check:
     - `0x22001710: movs r4, r0`
   - expected:
     - `r0 = 0`
   - failure:
     - `0x22001714: bne 0x22001ae4`
     - skips to late cleanup tail before execute gate
   - status:
     - unresolved
     - not directly patched/tested

3. `0x22001758: blx [service + 0x80]`
4. `0x22001768: bl 0x2200152c`
   - check:
     - `0x2200176c: movs r4, r0`
   - expected:
     - `r0 = 0`
   - failure:
     - `0x22001770: bne 0x22001ae4`
   - status:
     - unresolved
     - not directly patched/tested

5. `0x2200177c: bl 0x22006380`
6. `0x22001788: bl 0x22006430`
   - purpose:
     - local service/UART object setup
   - expected:
     - return normally
   - failure:
     - no direct branch here, but bad state could affect later calls
   - status:
     - unresolved

7. `0x22001798: bl 0x22006558`
   - original risk:
     - could hang in UART TX FIFO poll
   - current build status:
     - already patched to immediate `return 0`
   - effect:
     - no longer the last unresolved blocker before `0x22001b04`

8. `0x22001828: blx [service + 0x90]` with ID `19`
9. `0x22001868: blx [service + 0x90]` with ID `33`
10. `0x220018c0: blx [service + 0x18]`
11. `0x22001930: blx [service + 0x68]`
12. `0x22001968: bl 0x22003350`
13. `0x22001970: bl 0x220007d0`
14. `0x2200197c: blx [service + 0x6c]`
   - current build status:
     - replaced with local readiness stub at `0x22006e00`
   - expected:
     - stub returns `r0 = 1`
   - tested:
     - yes; changed host behavior and got us past the old WTF-stuck pattern

15. `0x2200198c: tst r5, #8`
   - expected on active path:
     - set
   - because:
     - `r5 = 0x1d`
   - failure/bypass leg:
     - `0x22001994: beq 0x22001a24`
   - status:
     - statically satisfied

16. `0x220019ac: blx [service + 0x74]`
   - current build status:
     - local loader stub at `0x22006ccc`
   - expected:
     - success-style fallthrough to `0x22001a24`
   - tested:
     - multiple loader-side iterations done earlier

17. `0x220019b0: cmp r0, #0`
   - failure:
     - `0x220019e0: mov r4, #87`
     - then `0x22001a28`
   - current build status:
     - resolved by current loader-side patching

18. `0x22001a24: mov r4, #0`
   - key fact:
     - from here to `0x22001b04`, there are no direct writes to `r4`

19. `0x22001a4c: bl 0x220032fc`
20. `0x22001a78: blx [service + 0x8c]` with ID `19`
21. `0x22001a88: bl 0x220032fc`
22. `0x22001ab0: blx [service + 0x8c]` with ID `33`
23. `0x22001ae0: bl 0x220007d0`
24. `0x22001ae8: bl 0x22002724`
   - current status:
     - last unresolved call before the final gate in the current build
   - reason:
     - `0x22006558` is already patched to immediate return
   - failure potential:
     - can still hang or perturb state before `0x22001b04`

25. `0x22001b00: bl 0x22006558`
   - current build:
     - immediate return stub

26. Final gate:
   - `0x22001b04: cmp r4, #0`
   - expected:
     - `r4 = 0`
   - `0x22001b0c: tst r5, #16`
   - expected:
     - set because `r5 = 0x1d`
   - `0x22001b10: addne r0, r6, r7`
   - expected:
     - `r0 = 0x08000800`
   - `0x22001b14: blne 0x220024b8`

### Final-gate condition summary

Static state in the current build:

- `r4` is forced to `0` at:
  - `0x22001a24`
- `r4` is not directly written again before:
  - `0x22001b04`
- `r5 = 0x1d`, so:
  - `r5 & 0x10` is set before:
    - `0x22001b0c`
- `0x22006558` is already bypassed to immediate return

So the final gate itself is not the unresolved condition. The remaining unknown
is reachability of the tail:

- `0x22001ae8 -> 0x22001b04`

### Prepared marker at the last unresolved call

Active prepared marker in local `/tmp/wInd3x/pkg/cfw/defang_wtf.go`:

- callsite:
  - runtime:
    - `0x22001ae8`
  - original:
    - `0d 03 00 eb`
    - `bl 0x22002724`
  - replacement:
    - `94 14 00 ea`
    - `b 0x22006d40`
- stub:
  - runtime:
    - `0x22006d40`
  - bytes:
    - `04 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
  - disassembly:
    - `mov r0, #4`
    - `ldr r3, =0x22002138`
    - `blx r3`
    - `b .`

Meaning:

- if control reaches the last unresolved callsite before `0x22001b04`, WTF
  will switch to marker state `4` and loop forever
- this isolates reachability of the final unresolved tail without skipping the
  earlier readiness or loader setup already under test

Status:

- local `/tmp/wInd3x/wInd3x` rebuilt with writable Go caches
- no `cfw` run performed yet

Decision:

- **FINAL_GATE_MARKER_PREPARED**

## 2026-04-24 Hardware result for final-tail marker at `0x22001ae8`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker under test:

- `0x22001ae8`
  - `0d 03 00 eb` -> `94 14 00 ea`
  - branch to:
    - `0x22006d40`
- marker stub:
  - `mov r0, #4`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano disappeared from USB entirely
- post-run checks showed:
  - no Nano present in `lsusb`
  - no DFU device present in `mks5lboot --dfuscan`

Interpretation:

- the `0x22001ae8` marker did not produce any new host-visible mode/state
  change
- strongest current classification:
  - `MARKER_1AE8_NOT_REACHED`

Implication:

- the remaining blocker is earlier than the last unresolved tail into
  `0x22001b04`
- current readiness-stub success is still insufficient to prove reachability of
  the final execute gate region

Decision:

- **MARKER_1AE8_NOT_REACHED**

## 2026-04-24 Middle-path marker prepared at `0x22001758`

Goal:

- move the split earlier into the unresolved middle of `0x22001698`
- start with the earliest remaining candidate:
  - `0x22001758: blx [service + 0x80]`

Restored/kept state in the current local defanger:

- `0x22001ae8`
  - restored to original:
    - `0d 03 00 eb`
    - `bl 0x22002724`
- `0x2200197c`
  - readiness stub remains active
- `0x220019ac`
  - loader callback stub remains active
- `0x22006558`
  - immediate return patch remains active

Active marker under test:

- callsite:
  - runtime:
    - `0x22001758`
  - original:
    - `30 ff 2f e1`
    - `blx r1`
  - replacement:
    - `e8 15 00 ea`
    - `b 0x22006f00`
- stub target:
  - `0x22006f00`
- stub bytes:
  - `02 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- stub disassembly:
  - `mov r0, #2`
  - `ldr r3, =0x22002138`
  - `blx r3`
  - `b .`

Prepared but inactive follow-up checkpoints:

- `0x22001768`
  - original:
    - `6f ff ff eb`
    - `bl 0x2200152c`
  - prepared replacement:
    - `ec 15 00 ea`
    - `b 0x22006f20`
- `0x2200177c`
  - original:
    - `ff 12 00 eb`
    - `bl 0x22006380`
  - prepared replacement:
    - `ef 15 00 ea`
    - `b 0x22006f40`
- `0x22001788`
  - original:
    - `28 13 00 eb`
    - `bl 0x22006430`
  - prepared replacement:
    - `f4 15 00 ea`
    - `b 0x22006f60`

Offline verification against a freshly defanged WTF body:

- `0x22001758`
  - now disassembles as:
    - `ea0015e8  b 0x22006f00`
- `0x22006f00`
  - now disassembles as the marker stub above
- `0x22001ae8`
  - restored:
    - `eb00030d  bl 0x22002724`

Expected next-run classifications:

- `MARKER_1758_REACHED`
- `MARKER_1758_NOT_REACHED`

Decision:

- **MARKER_1758_PREPARED**

## 2026-04-24 Hardware result for middle-path marker at `0x22001758`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker under test:

- `0x22001758`
  - `30 ff 2f e1` -> `e8 15 00 ea`
  - branch to:
    - `0x22006f00`
- marker stub:
  - `mov r0, #2`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- unlike the previous post-WTF baseline, the Nano did **not** disappear from USB
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- repeated `mks5lboot --dfuscan` attempts failed to claim/configure the device:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is a material host-visible behavior change from the previous baseline
- strongest current classification:
  - `MARKER_1758_REACHED`

Meaning:

- control reaches at least as far as:
  - `0x22001758`
- the blocker is later than the `[service + 0x80]` callback dispatch
- next useful split should move forward to:
  - `0x22001768`
  - then `0x2200177c`
  - then `0x22001788`

Decision:

- **MARKER_1758_REACHED**

## 2026-04-24 Hardware result for middle-path marker at `0x22001768`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker under test:

- `0x22001768`
  - `6f ff ff eb` -> `ec 15 00 ea`
  - branch to:
    - `0x22006f20`
- marker stub:
  - `mov r0, #3`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano did **not** disappear from USB
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` failed to claim/configure the device:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same material host-visible behavior change seen with the reached
  `0x22001758` marker
- strongest current classification:
  - `MARKER_1768_REACHED`

Meaning:

- control reaches at least as far as:
  - `0x22001768`
- the blocker is later than:
  - `0x2200152c`
- next useful split should move forward to:
  - `0x2200177c`
  - then `0x22001788`

Decision:

- **MARKER_1768_REACHED**

## 2026-04-24 Hardware result for middle-path marker at `0x2200177c`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker under test:

- `0x2200177c`
  - `ff 12 00 eb` -> `ef 15 00 ea`
  - branch to:
    - `0x22006f40`
- marker stub:
  - `mov r0, #4`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano did **not** disappear from USB
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` failed to claim/configure the device:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same material host-visible behavior change seen with the reached
  `0x22001758` and `0x22001768` markers
- strongest current classification:
  - `MARKER_177C_REACHED`

Meaning:

- control reaches at least as far as:
  - `0x2200177c`
- the blocker is later than:
  - `0x22006380`
- next useful split should move forward to:
  - `0x22001788`

Decision:

- **MARKER_177C_REACHED**

## 2026-04-24 Hardware result for middle-path marker at `0x22001788`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker under test:

- `0x22001788`
  - `28 13 00 eb` -> `f4 15 00 ea`
  - branch to:
    - `0x22006f60`
- marker stub:
  - `mov r0, #5`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano did **not** disappear from USB
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` failed to claim/configure the device:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same material host-visible behavior change seen with the reached
  `0x22001758`, `0x22001768`, and `0x2200177c` markers
- strongest current classification:
  - `MARKER_1788_REACHED`

Meaning:

- control reaches at least as far as:
  - `0x22001788`
- so the unresolved blocker is later than:
  - `0x22006430`
- the entire current middle-chain segment from:
  - `0x22001758`
  - through `0x22001788`
  is now reached on hardware

Decision:

- **MARKER_1788_REACHED**

## 2026-04-24 Hardware result for post-`0x1788` marker at `0x22001798`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker under test:

- `0x22001798`
  - `6e 13 00 eb` -> `f8 15 00 ea`
  - branch to:
    - `0x22006f80`
- marker stub:
  - `mov r0, #6`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano did **not** disappear from USB
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` failed to claim/configure the device:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same material host-visible behavior change seen with the reached
  earlier markers
- strongest current classification:
  - `MARKER_1798_REACHED`

Meaning:

- control reaches at least as far as:
  - `0x22001798`
- so the unresolved blocker is later than:
  - `0x22006558` in this earlier post-`0x1788` slot
- the next useful forward split should move to:
  - `0x220017a0`
  - then `0x2200181c`
  - then `0x2200182c`
  - then `0x22001938`

Decision:

- **MARKER_1798_REACHED**

## 2026-04-24 Aborted attempt to test marker at `0x220017a0`

This attempt did not begin from a clean usable DFU state, so it does not
produce a valid `MARKER_17A0_*` classification.

Observed host state:

- `lsusb` still showed:
  - `05ac:1223`
  - DFU mode
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`
- `wInd3x cfw run` failed before exploit launch with:
  - string-descriptor I/O error

Interpretation:

- this is the same sticky DFU failure mode seen in earlier aborted attempts
- the Nano 2G / S5L8701 comparison remains a clean-handoff reference only,
  not a byte-level patch model for Nano 3G
- the `0x220017a0` marker remains prepared but untested on hardware

## 2026-04-24 Hardware result for marker at `0x220017a0`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker under test:

- `0x220017a0`
  - `ed 08 00 eb` -> `fe 15 00 ea`
  - branch to:
    - `0x22006fa0`
- marker stub:
  - `mov r0, #7`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano did **not** disappear from USB
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` failed to claim/configure the device:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same material host-visible behavior change seen with the reached
  earlier markers
- strongest current classification:
  - `MARKER_17A0_REACHED`

Meaning:

- control reaches at least as far as:
  - `0x220017a0`
- the Nano 2G / S5L8701 comparison still serves only as a clean-handoff
  behavior reference, not a byte-level patch template
- the unresolved blocker is later than:
  - `0x22003b5c`

Decision:

- **MARKER_17A0_REACHED**

## 2026-04-24 Hardware result for marker at `0x2200181c`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker under test:

- `0x2200181c`
  - `b6 06 00 eb` -> `e7 15 00 ea`
  - branch to:
    - `0x22006fc0`
- marker stub:
  - `mov r0, #8`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano did **not** disappear from USB
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` failed to claim/configure the device:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same material host-visible behavior change seen with the reached
  earlier markers
- strongest current classification:
  - `MARKER_181C_REACHED`

Meaning:

- control reaches at least as far as:
  - `0x2200181c`
- the unresolved blocker is later than:
  - `0x220032fc`
- the next useful forward split should move to:
  - `0x2200182c`
  - then `0x22001938`

Decision:

- **MARKER_181C_REACHED**

## 2026-04-24 Hardware result for marker at `0x2200182c`

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Active local WTF marker under test:

- `0x2200182c`
  - `31 ff 2f e1` -> `eb 15 00 ea`
  - branch to:
    - `0x22006fe0`
- marker stub:
  - `mov r0, #9`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano did **not** disappear from USB
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` failed to claim/configure the device:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this is the same material host-visible behavior change seen with the reached
  earlier markers
- strongest current classification:
  - `MARKER_182C_REACHED`

Meaning:

- control reaches at least as far as:
  - `0x2200182c`
- the unresolved blocker is later than:
  - the `[state + 0x90]` callback
- the next useful forward split is now:
  - `0x22001938`
  - versus the previously failed late tail at `0x22001ae8`

Decision:

- **MARKER_182C_REACHED**

## 2026-04-24 Forward marker at 0x22001938

Prepared marker:

- `0x22001938`
  - `31 ff 2f e1` -> `64 17 00 ea`
  - branches to marker stub at:
    - `0x220076d0`
- marker stub:
  - calls `0x22002138(10)`
  - loops forever
- kept active:
  - readiness bypass at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- the Nano did **not** disappear from USB
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` failed to claim/configure the device:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this matches the same reached-marker host signature as the earlier reached
  checkpoints
- strongest current classification:
  - `MARKER_1938_REACHED`

Meaning:

- control reaches at least as far as:
  - `0x22001938`
- the unresolved blocker is later than:
  - the `[service + 0x68]` callback
- the next unresolved region is now between:
  - `0x22001938`
  - `0x22001ae8`

Decision:

- **MARKER_1938_REACHED**

## 2026-04-24 Final gap reduction between `0x22001938` and `0x22001ae8`

The remaining unresolved region is now reduced to the first unresolved call in
that span, not the final execute gate.

### Linear branch/call summary

From the current defanged build:

- `0x22001938: blx [service + 0x68]`
  - first unresolved call in the gap
- `0x2200197c`
  - already replaced by the local readiness stub
  - returns `r0 = 1`
- `0x22001988: beq 0x22001a28`
  - not expected on current path because the readiness stub returns nonzero
- `0x2200198c: tst r5, #8`
- `0x22001994: beq 0x22001a24`
  - not expected on current path because `r5 = 0x1d`
- `0x220019ac`
  - already replaced by the local loader stub
- `0x220019b8: beq 0x220019e0`
  - not expected on current path because the loader stub is intended to return
    success-style `r0 != 0`
- `0x220019c8: beq 0x22001a24`
  - later status-dependent branch
- `0x220019cc..0x220019d4`
  - later poll loop on `0x38c0000c & 1`
- `0x22001ae8`
  - previously proven not reached

### Exact current blocker

- `BLOCKING_ADDRESS`
  - `0x22001938`
- `INSTRUCTION`
  - `blx r1`
- `CALL TARGET SOURCE`
  - `[service + 0x68]`
  - BootROM service slot at `0x20000088`
- `WHY it fails`
  - this is the first unresolved call after all proven earlier setup
  - all earlier branch conditions in the current build are either already
    patched (`0x197c`, `0x19ac`) or statically satisfied (`r5 = 0x1d`)
  - nothing later in the gap can be trusted until this callback is either
    proven to return or bypassed
  - its return value is ignored immediately afterward, so a local no-op return
    is the lowest-risk forward patch

### Required fix

Prepare a Nano 3G-only local bypass for the callback at `0x22001938`:

- callsite:
  - `0x22001938`
  - original bytes:
    - `31 ff 2f e1`
  - replacement bytes:
    - `64 17 00 eb`
  - meaning:
    - `bl 0x220076d0`
- local stub:
  - `0x220076d0`
  - bytes:
    - `1e ff 2f e1`
  - meaning:
    - `bx lr`

This bypass preserves registers and returns directly to `0x2200193c`.

Decision:

- **FINAL_BLOCKER_FOUND**
- **PATCH_READY**

## 2026-04-24 Hardware test of the `[service + 0x68]` bypass at `0x22001938`

Prepared bypass:

- `0x22001938`
  - `31 ff 2f e1` -> `64 17 00 eb`
  - `bl 0x220076d0`
- `0x220076d0`
  - `1e ff 2f e1`
  - `bx lr`

Observed host behavior:

- clean DFU start:
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

Observed screen behavior:

- black screen for the full 90-second window
- no Apple logo
- no backlight
- no flicker
- no patched text
- no stock UI
- no visible reset loop

Interpretation:

- the `[service + 0x68]` bypass did not advance the handoff into a visible
  OSOS boot
- host-side the Nano remained in the same DFU-resident reached-marker style
  state instead of progressing into visible runtime
- strongest current classification:
  - `CHAINLOAD_BLACKSCREEN`

Decision:

- **CHAINLOAD_BLACKSCREEN**

Dispatch-table reconstruction note:

- `0x2200f880` is runtime-populated, not statically initialized in the OSOS body
- writer:
  - `0x22004c08`
- clearer:
  - `0x22004bdc`
- readers:
  - `0x220085ec`
  - `0x22008620`
- slot layout:
  - `0x2200f880 + (row << 5) + (col << 2) - 0x20`
- direct seed test for slot `(1,0)` through `0x22004c08` preserved the same
  post-escape host signature:
  - USB disappeared entirely
  - no DFU device remained visible

## 2026-04-25 Forced panel-preamble candidate `0x220045b0`

The smallest remaining panel-side programming block adjacent to the corrected
LCD init path is `0x220045b0`. It is a real callable entry and issues a short
selector-style sequence through `0x2200364c` using IDs:

- `27`
- `23`
- `18`
- `17`

Prepared one-shot force patch:

- open the late display gate:
  - `0x22002f94`
  - `0b 00 00 1a` -> `00 00 a0 e1`
- replace the late visibility callback:
  - `0x22002f9c`
  - `22 11 00 eb` -> `d1 29 00 eb`
- stub at `0x2200d6e8`:
  - `push {lr}`
  - `bl 0x220045b0`
  - `mov r0, #1`
  - `pop {pc}`

Single hardware run result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed no DFU device

Interpretation:

- forcing the small panel-side preamble candidate preserves the same preserved-
  service68 post-escape signature
- host-side this means the patch does not regress the boot path
- device-side visibility still depends on screen observation

Decision:

- **PANEL_PREAMBLE_PATCH_POST_ESCAPE_SIGNATURE**

## 2026-04-25 Direct PMU register-write candidate `0x220054b0`

After the panel-preamble miss, the strongest remaining low-level Apple-backed
candidate was `0x220054b0`. Unlike `0x220045b0`, this path reaches explicit PMU
register writes:

- reg `0x1d = 0x0a`
- reg `0x1b = on/off`

Prepared one-shot force patch:

- open the late display gate:
  - `0x22002f94`
  - `0b 00 00 1a` -> `00 00 a0 e1`
- replace the late visibility callback:
  - `0x22002f9c`
  - `22 11 00 eb` -> `d1 29 00 eb`
- stub at `0x2200d6e8`:
  - `push {r4, r7, lr}`
  - `mov r7, #1`
  - `bl 0x220054b0`
  - `mov r0, #1`
  - `pop {r4, r7, pc}`

Single hardware run result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed no DFU device
- screen stayed fully black:
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Interpretation:

- directly forcing the PMU register-write candidate still preserves the same
  preserved-service68 post-escape signature
- host-side this means the patch does not regress the boot path
- device-side visibility still does not appear, despite forcing the PMU-backed
  register-write candidate

Decision:

- **BACKLIGHT_CANDIDATE_1_POST_ESCAPE_SIGNATURE**

## 2026-04-25 Direct PMU bit-0 candidate `0x220054f8`

After the direct PMU register-write block `0x220054b0` stayed black, the next
most credible Apple-backed low-level power candidate was the adjacent PMU read-
modify-write block at `0x220054f8`, which updates PMU register `0x43` bit `0`.

Prepared one-shot force patch:

- open the late display gate:
  - `0x22002f94`
  - `0b 00 00 1a` -> `00 00 a0 e1`
- replace the late visibility callback:
  - `0x22002f9c`
  - `22 11 00 eb` -> `d1 29 00 eb`
- stub at `0x2200d6e8`:
  - `push {r4, r7, lr}`
  - `mov r7, #1`
  - `bl 0x220054f8`
  - `mov r0, #1`
  - `pop {r4, r7, pc}`

Single hardware run result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed no DFU device
- screen stayed fully black:
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Interpretation:

- directly forcing the PMU bit-0 candidate still preserves the same
  preserved-service68 post-escape signature
- host-side this means the patch does not regress the boot path
- device-side visibility still does not appear, despite forcing the adjacent
  Apple-backed PMU bit-0 path

Decision:

- **BACKLIGHT_CANDIDATE_2_POST_ESCAPE_SIGNATURE**

## 2026-04-25 Direct panel-worker candidate `0x220049b0`

After both mapped Apple-backed PMU paths stayed black, the next strongest
non-PMU candidate was the real panel worker `0x220049b0` beneath
`0x220085a4`. This is lower than the selector helper `0x220085ec`, whose
static dispatch table window at `0x2200f880` appears zeroed in the current
runtime body snapshot.

Prepared one-shot force patch:

- open the late display gate:
  - `0x22002f94`
  - `0b 00 00 1a` -> `00 00 a0 e1`
- replace the late visibility callback:
  - `0x22002f9c`
  - `22 11 00 eb` -> `d1 29 00 eb`
- stub at `0x2200d6e8`:
  - `push {lr}`
  - `sub sp, sp, #8`
  - set args:
    - `r0 = 1`
    - `r1 = 0`
    - `r2 = 0`
    - `r3 = 0`
    - 5th arg = `1`
    - 6th arg = `0`
  - `bl 0x220049b0`
  - `add sp, sp, #8`
  - `mov r0, #1`
  - `pop {pc}`

Single hardware run result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed no DFU device
- screen stayed fully black:
  - no Apple logo
  - no backlight
  - no flicker
  - no text
  - no visible reset loop

Interpretation:

- directly forcing the real panel worker still preserves the same
  preserved-service68 post-escape signature
- host-side this means the patch does not regress the boot path
- device-side visibility still does not appear, despite forcing the panel
  register-programming worker beneath the selector path

Decision:

- **BACKLIGHT_CANDIDATE_3_POST_ESCAPE_SIGNATURE**

## 2026-04-25 Corrected late display-gate callback patch result

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-display-gate-callbacks-n3g.dfu`

Host result:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the corrected late display-gate callback patch preserves the
  preserved-service68 post-escape signature
- it does not fall back to the old sticky DFU-resident state
- host-side classification:
  - `TARGETED_HW_PATCH_POST_ESCAPE_SIGNATURE`

Screen result:

- black screen / no visible change
- no Apple logo
- no backlight
- no flicker
- no text
- no visible reset loop

## 2026-04-25 Low-level display/backlight control isolation

Findings:

- the true LCD-local init entry is `0x220044c4`, not `0x2200455c`
- `0x220044c4..0x22004598` contains the direct `0x38000000` controller-base
  write at:
  - `0x22004560: mov r2, #0x38000000`
  - `0x22004568: mov r0, #0x38000000`
  - `0x2200456c: bl 0x22003624`
- the panel helper real entry is still `0x2200488c`
  - observed callers:
    - `0x22008090`
    - `0x22008550`
- the direct PMU/backlight writes remain the Apple block around
  `0x22005480..0x22005540`
  - `0x220054b0` path:
    - PMU reg `0x1d = 0x0a`
    - PMU reg `0x1b = on/off`
  - `0x220054f8` path:
    - PMU reg `0x43` bit `0` read-modify-write
- but `0x220054b0` / `0x220054f8` are not safe standalone call targets:
  - they depend on live `r4/r6/r7` context from the surrounding stateful block
- the late display gate is still:
  - `0x22002f94: bne 0x22002fc8`
  - `0x22002f98: bl 0x220073b4`
  - `0x22002f9c: bl 0x2200742c`

Prepared direct low-level force patch:

- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_force_lowlevel_display.go`
- image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-force-lowlevel-display-n3g.dfu`
- body:
  - `/tmp/n3g-osos-work/lcd-osos-force-lowlevel-display-n3g.bin`

Verified patch:

- `0x22002f94: 0b 00 00 1a -> 00 00 a0 e1`
- `0x22002f9c: 22 11 00 eb -> d1 29 00 eb`
- stub at `0x2200d6e8`:
  - `push {lr}`
  - `bl 0x220044c4`
  - `mov r0, #1`
  - `pop {pc}`

Interpretation:

- this is the narrowest corrected direct LCD-controller force test prepared so
  far
- it avoids the earlier mistake of calling mid-function addresses
  `0x2200455c` and `0x220048bc`

## 2026-04-25 Direct low-level LCD init force patch result

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-force-lowlevel-display-n3g.dfu`

Host result:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- directly forcing the corrected low-level LCD init entry `0x220044c4` from the
  late display hook still preserves the preserved-service68 post-escape
  signature
- host-side classification:
  - `FORCE_DISPLAY_PATCH_POST_ESCAPE_SIGNATURE`

Screen result:

- black screen / no visible change
- no Apple logo
- no backlight
- no flicker
- no text
- no visible reset loop

## 2026-04-25 Narrow Apple-backed PMU enable candidate prepared

Current lowest-level Apple-backed power candidate:

- `0x22003078(1)`
  - this is the real PMU enable branch
  - it reaches:
    - `0x22003018(0)` -> `0x220054b0`
    - `0x2200304c(0)` -> `0x220054f8`

Apple-backed PMU register writes behind that branch:

- `0x220054b0`
  - reg `0x1d = 0x0a`
  - reg `0x1b = on/off`
- `0x220054f8`
  - reg `0x43` bit `0` read-modify-write

Why this candidate was chosen:

- no explicit Apple-backed `LEDCTL (0x20)` / `0x28` / `0x29` backlight write
  is visible in the recovered OSOS body
- `0x220054b0` / `0x220054f8` are not safe standalone call targets because they
  depend on surrounding live context
- `0x22003078(1)` is the narrowest callable Apple-owned PMU/panel-power path
  that reaches those writes exactly once

Prepared patch:

- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_force_pmu_enable.go`
- image:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-force-pmu-enable-n3g.dfu`
- body:
  - `/tmp/n3g-osos-work/lcd-osos-force-pmu-enable-n3g.bin`

Verified bytes:

- `0x22002f94: 0b 00 00 1a -> 00 00 a0 e1`
- `0x22002f9c: 22 11 00 eb -> d1 29 00 eb`

Verified stub at `0x2200d6e8`:

- `push {lr}`
- `mov r0, #1`
- `bl 0x22003078`
- `mov r0, #1`
- `pop {pc}`

## 2026-04-25 PMU enable force patch result

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-cfw-force-pmu-enable-n3g.dfu`

Host result:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- directly forcing the Apple-backed PMU enable branch `0x22003078(1)` also
  preserves the preserved-service68 post-escape signature
- host-side classification:
  - `FORCE_BACKLIGHT_PATCH_POST_ESCAPE_SIGNATURE`

Screen result:

- pending user observation

## 2026-04-25 Late OSOS display/backlight gate mapped under preserved-service68 baseline

What is now established from static OSOS analysis:

- the preserved-service68 baseline is deep enough into OSOS startup that a
  visibility-specific gate is now visible in-body around `0x22002f70`
- that gate checks two bytes from the object returned by `0x22004fa8`
- when the gate opens, startup falls into:
  - `0x220073b4`
  - `0x2200742c`
- `0x2200742c` is not a raw hardware helper:
  - it loads `[r0 + 0x20]`
  - then branches through veneer `0x220038ac -> 0x08183bdc`
- so the visible/no-visible transition still runs through an Apple object-method
  dispatch layer, not direct in-body LCD MMIO

Backlight-specific evidence:

- the compact Apple "visibility on" wrapper remains:
  - `0x22005640`
  - `bl 0x2200374c`
  - `bl 0x22003774`
  - `bl 0x220073b4`
  - `mov r1, #1`
  - `bl 0x22007610`
- Rockbox Nano 3G target code still points to PMU LED control rather than a
  framebuffer-only issue:
  - `backlight_hw_on()` enables `D1671_REG_LEDCTL`
  - it calls `lcd_awake()` first if the panel is asleep
- this keeps the best current explanation as:
  - OSOS is running
  - the display/backlight transition remains gated behind Apple service/object
    state rather than a missing jump into OSOS

Prepared targeted patch:

- hook the late visibility callsite at:
  - `0x22002f9c`
  - original bytes:
    - `22 11 00 eb`
  - replacement bytes:
    - `03 17 00 eb`
- local stub at:
  - `0x22008bb0`
- stub behavior:
  - call original `0x2200742c`
  - preserve its return value
  - then call Apple wrapper `0x22005640`
  - return original `r0` to the caller

Verified patched body:

- `0x22002f9c = eb001703`
  - `bl 0x22008bb0`
- `0x22008bb0`
  - `push {r4, lr}`
  - `bl 0x2200742c`
  - `mov r4, r0`
  - `bl 0x22005640`
  - `mov r0, r4`
  - `pop {r4, pc}`

Prepared artifacts:

- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_visibility_late.go`
- patched IMG1:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-visibility-late-n3g.dfu`
- patched body:
  - `/tmp/n3g-osos-work/lcd-osos-visibility-late-n3g.bin`

Decision:

- **DISPLAY_INIT_PATH_FOUND**
- **BACKLIGHT_CONTROL_FOUND**
- **VISIBILITY_PATCH_READY**

## 2026-04-25 Late OSOS visibility wrapper patch preserves post-escape behavior

Single hardware run result for:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visibility-late-n3g.dfu`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - no Nano present at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the late wrapper patch at `0x22002f9c` does not collapse back to the old
  DFU-resident failure class
- host-side it matches the preserved-service68 post-escape signature, so the
  display/backlight hook is at least not earlier than the DFU-context escape
  point
- whether it improves actual screen visibility still depends on direct screen
  observation, which has not yet been recorded for this run

Decision:

- **VISIBILITY_PATCH_POST_ESCAPE_SIGNATURE**

## 2026-04-25 Hardware-level LCD/backlight path prepared under preserved-service68 baseline

Static OSOS analysis now supports a hardware-level display path rather than
another UI-only wrapper:

- late startup visibility gate:
  - `0x22002f70`
  - checks two status bytes returned by `0x22004fa8`
  - if the gate opens, startup falls into:
    - `0x220073b4`
    - `0x2200742c`
- Apple LCD-local init path:
  - `0x2200455c`
  - `0x220048bc(1, 0)`
  - `0x220048bc(4, 0)`
- compact Apple visibility/backlight wrapper:
  - `0x22005640`

Hardware-facing evidence:

- `0x2200455c` is the LCD-local preamble path tied to the `0x38300000` LCD
  controller block and related startup helpers
- `0x220048bc` is the panel-group dispatch path that selects the larger init
  tables and awake tail
- Rockbox Nano 3G target code still identifies backlight control as PMU
  `D1671_REG_LEDCTL` enable, with `lcd_awake()` required before backlight on if
  the panel is asleep

Prepared targeted patch:

- hook site:
  - `0x22002f9c`
  - original bytes:
    - `22 11 00 eb`
  - replacement bytes:
    - `03 17 00 eb`
- local stub:
  - `0x22008bb0`
  - verified body:
    - `push {r4, lr}`
    - `bl 0x2200742c`
    - `mov r4, r0`
    - `bl 0x2200455c`
    - `mov r0, #1`
    - `mov r1, #0`
    - `bl 0x220048bc`
    - `mov r0, #4`
    - `mov r1, #0`
    - `bl 0x220048bc`
    - `bl 0x22005640`
    - `mov r0, r4`
    - `pop {r4, pc}`

Prepared artifacts:

- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_display_hw_init.go`
- patched IMG1:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-displayhw-n3g.dfu`
- patched body:
  - `/tmp/n3g-osos-work/lcd-osos-displayhw-n3g.bin`

Decision:

- **DISPLAY_HW_INIT_FOUND**
- **BACKLIGHT_CONTROL_FOUND**
- **HARDWARE_PATCH_READY**

## 2026-04-25 Hardware-level LCD/backlight init patch preserves post-escape behavior

Single hardware run result for:

- `/tmp/n3g-osos-work/n3g-osos-cfw-displayhw-n3g.dfu`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - no Nano present at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- forcing the Apple LCD-local preamble, panel-group init (`mode 1`), awake tail
  (`mode 4`), and compact visibility wrapper still does not collapse back to
  the old DFU-resident failure class
- host-side it matches the preserved-service68 post-escape signature
- whether this improves actual visibility is still dependent on the direct
  screen observation from the run

Decision:

- **HARDWARE_PATCH_POST_ESCAPE_SIGNATURE**

## 2026-04-25 Hardware patch failure analysis after black-screen result

Direct static review of the late hardware-init hook exposed two concrete
problems in the current patch:

- `0x2200455c` is not a function entry
  - it sits in the middle of the larger LCD-local routine spanning
    `0x220044dc..0x22004598`
  - calling it directly skips the routine prologue and enters just before the
    `0x38300000` controller preamble writes
- `0x220048bc` is also not a function entry
  - the real function start is `0x2200488c`
  - `0x220048bc` is `0x30` bytes into that function, after the real prologue
  - the real helper uses a multi-argument ABI with stacked parameters, so the
    prior direct `0x220048bc(1, 0)` / `0x220048bc(4, 0)` calls were not
    well-formed

This means the previous “hardware-level LCD/backlight init patch” preserved the
post-escape signature, but it did not prove that the intended Apple LCD path
was actually being invoked correctly.

Additional confirmed findings:

- the late visibility hook point itself is still not directly proven reached
  under the preserved-service68 baseline
  - the startup-stage trace only proved progress through `0x22002f94`
  - the late gate branch at `0x22002f94` can still skip `0x22002f98..0x22002f9c`
- no explicit Apple-backed PMU `LEDCTL` write has yet been identified in this
  late OSOS path
  - `0x22005640` / `0x22007610` remain object/service wrappers, not direct PMU
    writes

Prepared reachability probes:

- hook-entry marker:
  - helper:
    - `/tmp/wInd3x/cmd/patch_n3g_display_hook_reach.go`
  - mode:
    - `entry`
  - patch:
    - `0x22002f9c: 22 11 00 eb -> 03 17 00 eb`
    - stub at `0x22008bb0`:
      - `mov r0, #48`
      - `bl 0x22002138`
      - `b .`
- post-`0x2200742c` marker:
  - same helper, mode `after0742c`
  - stub at `0x22008bb0`:
    - `push {r4, lr}`
    - `bl 0x2200742c`
    - `mov r0, #49`
    - `bl 0x22002138`
    - `b .`

Prepared corrected lower-level patch:

- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_display_gate_callbacks.go`
- artifacts:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-display-gate-callbacks-n3g.dfu`
  - `/tmp/n3g-osos-work/lcd-osos-display-gate-callbacks-n3g.bin`
- verified changes:
  - `0x22002f94: 0b 00 00 1a -> 00 00 a0 e1`
    - force the late visibility gate open instead of branching to
      `0x22002fc8`
  - `0x22002f9c: 22 11 00 eb -> d1 29 00 eb`
    - `bl 0x2200d6e8`
- corrected stub at `0x2200d6e8`:
  - preserves the original `0x2200742c` return value
  - installs a minimal no-op service table at `0x22007398`
  - reseeds:
    - `0x2200700c = 4`
    - `0x2200739c = 0`
    - `0x220073a0 = 0`
  - calls only real Apple function entries:
    - `0x22003c98(0x22007338)`
    - `0x22003ce0(1)`
    - `0x22003ce0(4)`
    - `0x22003d14(0x22007338)`
    - `0x22003cc0()`
    - `0x22005640()`
  - returns the original `r0`

Decision:

- **HARDWARE_HOOK_REACHABILITY_READY**
- **DISPLAY_INIT_FUNCTION_WRONG**
- **BACKLIGHT_PATH_MISSING**
- **LOWER_LEVEL_HW_PATCH_READY**

## 2026-04-25 Late display-hook entry probe reaches the post-escape path

Single hardware run for:

- `/tmp/n3g-osos-work/n3g-osos-cfw-display-hook-entry-n3g.dfu`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - no Nano present at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the late display-hook entry probe does not collapse back to the old
  DFU-resident stall
- this matches the preserved-service68 post-escape signature
- under the current working classification scheme, that keeps the late
  `0x22002f9c` display-hook site relevant on hardware

Decision:

- **DISPLAY_HOOK_ENTRY_REACHED**

## 2026-04-25 Caller-side second-stage dispatcher exit reached under preserved-service68 baseline

Prepared OSOS marker:

- hook site:
  - `0x22002f60`
  - `f8 8f bd e8` -> `26 19 00 ea`
  - branch to:
    - `0x22009400`
- local stub:
  - `mov r0, #29`
  - `blx 0x22002138`
  - `b .`

Preserved-service68 WTF baseline kept active:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 27 00 a0 e3`
- `0x2200197c = 30 ff 2f e1`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the full second-stage dispatcher at `0x22002ea4..0x22002f60` completes and
  reaches its return/exit point under the preserved-service68 baseline
- the next useful split should move forward into the next startup stage at
  `0x22002f70`, not back into the second-stage callback loop

Decision:

- **CALLER_STAGE_NEXT7_MARKER_REACHED**

## 2026-04-25 Post-`0x22002f60` startup-stage entry reached under preserved-service68 baseline

Prepared OSOS marker:

- hook site:
  - `0x22002f70`
  - `10 40 2d e9` -> `22 19 00 ea`
  - branch to:
    - `0x22009400`
- local stub:
  - `mov r0, #30`
  - `blx 0x22002138`
  - `b .`

Preserved-service68 WTF baseline kept active:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 27 00 a0 e3`
- `0x2200197c = 30 ff 2f e1`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- execution moves past the second-stage dispatcher return at `0x22002f60`
  and reaches the next caller-owned startup stage at `0x22002f70`
- the next useful split should move forward inside the `0x22002f70`
  startup stage, starting with the first call at `0x22002f74`

Decision:

- **STARTUP_STAGE_AFTER_2F60_MARKER_REACHED**

## 2026-04-25 `0x22002f70` startup-stage first call reached under preserved-service68 baseline

Prepared OSOS marker:

- hook site:
  - `0x22002f74`
  - `26 03 00 eb` -> `21 19 00 ea`
  - branch to:
    - `0x22009400`
- local stub:
  - `mov r0, #31`
  - `blx 0x22002138`
  - `b .`

Preserved-service68 WTF baseline kept active:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 27 00 a0 e3`
- `0x2200197c = 30 ff 2f e1`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- execution moves into the `0x22002f70` startup stage and reaches its first
  call at `0x22002f74`
- the next useful split should move forward to the next call in that stage at
  `0x22002f78`

Decision:

- **STARTUP_STAGE_2F74_MARKER_REACHED**

## 2026-04-25 `0x22002f70` startup-stage second call reached under preserved-service68 baseline

Prepared OSOS marker:

- hook site:
  - `0x22002f78`
  - `74 02 00 eb` -> `20 19 00 ea`
  - branch to:
    - `0x22009400`
- local stub:
  - `mov r0, #32`
  - `blx 0x22002138`
  - `b .`

Preserved-service68 WTF baseline kept active:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 27 00 a0 e3`
- `0x2200197c = 30 ff 2f e1`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- execution stays live inside the `0x22002f70` startup stage and reaches the
  second call at `0x22002f78`
- the next useful split should move forward to the third call in that stage at
  `0x22002f7c`

Decision:

- **STARTUP_STAGE_2F78_MARKER_REACHED**

## 2026-04-25 `0x22002f70` startup-stage third call reached under preserved-service68 baseline

Prepared OSOS marker:

- hook site:
  - `0x22002f7c`
  - `6d 02 00 eb` -> `1f 19 00 ea`
  - branch to:
    - `0x22009400`
- local stub:
  - `mov r0, #33`
  - `blx 0x22002138`
  - `b .`

Preserved-service68 WTF baseline kept active:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 27 00 a0 e3`
- `0x2200197c = 30 ff 2f e1`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- execution stays live inside the `0x22002f70` startup stage and reaches the
  third call at `0x22002f7c`
- the next useful split should move forward to the first state-setting step
  after the call trio at `0x22002f84`

Decision:

- **STARTUP_STAGE_2F7C_MARKER_REACHED**

## 2026-04-25 First post-call state-transition step in `0x22002f70` stage reached

Prepared OSOS marker:

- hook site:
  - `0x22002f84`
  - `25 ff ff eb` -> `1d 19 00 ea`
  - branch to:
    - `0x22009400`
- local stub:
  - `mov r0, #34`
  - `blx 0x22002138`
  - `b .`

Preserved-service68 WTF baseline kept active:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 27 00 a0 e3`
- `0x2200197c = 30 ff 2f e1`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- execution gets past the three-call prelude in the `0x22002f70` startup stage
  and reaches the first control-state transition call at `0x22002f84`
- that helper is not just a trivial marker:
  - it is `bl 0x22002c20` with `r0 = 1`
  - `0x22002c20` updates the control word at the global behind
    `0x22002dbc`, ORs in `0x1000 | 0x8`, seeds fields at `+0x200/+0x204`,
    and continues setup work
- the next useful split should move forward to `0x22002f88`

Decision:

- **STARTUP_STAGE_2F84_MARKER_REACHED**

## 2026-04-25 Next call after the first post-call state step in `0x22002f70` stage reached

Prepared OSOS marker:

- hook site:
  - `0x22002f88`
  - `7a 01 00 eb` -> `1c 19 00 ea`
  - branch to:
    - `0x22009400`
- local stub:
  - `mov r0, #35`
  - `blx 0x22002138`
  - `b .`

Preserved-service68 WTF baseline kept active:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 27 00 a0 e3`
- `0x2200197c = 30 ff 2f e1`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- execution continues past the first post-call state transition and reaches the
  next call at `0x22002f88`
- the next useful split should move forward to the second state-transition
  sequence starting at `0x22002f90`

Decision:

- **STARTUP_STAGE_2F88_MARKER_REACHED**

## 2026-04-25 Second state-transition sequence in `0x22002f70` stage reached

Prepared OSOS marker:

- hook site:
  - `0x22002f90`
  - `22 ff ff eb` -> `1a 19 00 ea`
  - branch to:
    - `0x22009400`
- local stub:
  - `mov r0, #36`
  - `blx 0x22002138`
  - `b .`

Preserved-service68 WTF baseline kept active:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 27 00 a0 e3`
- `0x2200197c = 30 ff 2f e1`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- execution continues into the second control-state transition sequence at
  `0x22002f90`
- this is `bl 0x22002c20` with `r0 = 3`
- `0x22002c20` on the `r5 == 3` path clears bit `0x00100000` in the control
  word behind the global loaded from `0x22002dbc`, then returns
- the next useful split should move forward to `0x22002f94`

Decision:

- **STARTUP_STAGE_2F90_MARKER_REACHED**

## 2026-04-25 Next call after the second state-transition sequence in `0x22002f70` stage reached

Prepared OSOS marker:

- hook site:
  - `0x22002f94`
  - `58 02 00 eb` -> `19 19 00 ea`
  - branch to:
    - `0x22009400`
- local stub:
  - `mov r0, #37`
  - `blx 0x22002138`
  - `b .`

Preserved-service68 WTF baseline kept active:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 27 00 a0 e3`
- `0x2200197c = 30 ff 2f e1`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- execution continues past both control-state transition calls in the
  `0x22002f70` startup stage and reaches the next call at `0x22002f94`
- the next useful split should move forward to the stage exit/return sequence
  at `0x22002f98`

Decision:

- **STARTUP_STAGE_2F94_MARKER_REACHED**

## 2026-04-24 Hardware run with caller-side startup-stage wrapper at `0x220042f4`

Context:

- preserved original `[service + 0x68]` at `0x22001938`
- keep only the preserved-service68 WTF baseline patches:
  - loader callback stub at `0x220019ac`
  - status emulation at `0x220019d0`
  - UART immediate-return patch at `0x22006558`
- stop instrumenting inside helper `0x22002548..0x220025fc`

Prepared OSOS-side wrapper:

- patch:
  - `0x220042f4`
  - `93 f8 ff ea` -> `3d 14 00 ea`
- wrapper stub:
  - `0x220093f0`
  - load `0x22002548`
  - `blx` helper
  - call `0x22002138(22)`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the caller-side dispatch entry at `0x220042f4` is also relevant on hardware
- the helper entered via `0x22002548..0x220025fc` appears to return far enough
  for the wrapper-side post-return marker to matter
- this matches the preserved-service68 post-escape signature, not the old
  DFU-resident stall

Classification:

- **CALLER_STAGE_MARKER_REACHED**

## 2026-04-24 Hardware run with next caller-side startup-stage marker at `0x22002e28`

Context:

- keep preserved original `[service + 0x68]` at `0x22001938`
- keep the preserved-service68 WTF baseline patches:
  - loader callback stub at `0x220019ac`
  - status emulation at `0x220019d0`
  - UART immediate-return patch at `0x22006558`
- stop instrumenting inside helper `0x22002548..0x220025fc`
- move to the real caller-side dispatch engine continuation after the callback
  return at `0x22002e24: blxne r0`

Prepared OSOS-side probe:

- patch:
  - `0x22002e28`
  - `00 00 96 e5` -> `70 19 00 ea`
- marker stub:
  - `0x220093f0`
  - `mov r0, #23`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the real caller-side post-return continuation at `0x22002e28` is also
  relevant on hardware
- this confirms the early OSOS startup path continues past the first dispatch
  callback return inside the runtime image’s caller-side state machine
- behavior still matches the preserved-service68 post-escape signature, not the
  old DFU-resident stall

Classification:

- **CALLER_STAGE_NEXT_MARKER_REACHED**

## 2026-04-24 Hardware run with second caller-side startup-stage marker at `0x22002e4c`

Context:

- keep preserved original `[service + 0x68]` at `0x22001938`
- keep the preserved-service68 WTF baseline patches:
  - loader callback stub at `0x220019ac`
  - status emulation at `0x220019d0`
  - UART immediate-return patch at `0x22006558`
- continue in the runtime-image caller-side dispatch engine after the second
  callback return at `0x22002e48: blxne r0`

Prepared OSOS-side probe:

- patch:
  - `0x22002e4c`
  - `00 00 96 e5` -> `69 19 00 ea`
- marker stub:
  - `0x220093f8`
  - `mov r0, #24`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the caller-side continuation immediately after the second dispatch callback
  return at `0x22002e4c` is also relevant on hardware
- this confirms startup continues through the second callback stage in the
  runtime OSOS caller-side state machine
- behavior still matches the preserved-service68 post-escape signature, not the
  old DFU-resident stall

Classification:

- **CALLER_STAGE_NEXT2_MARKER_REACHED**

## 2026-04-24 Hardware run with third caller-side startup-stage marker at `0x22002e70`

Context:

- keep preserved original `[service + 0x68]` at `0x22001938`
- keep the preserved-service68 WTF baseline patches:
  - loader callback stub at `0x220019ac`
  - status emulation at `0x220019d0`
  - UART immediate-return patch at `0x22006558`
- continue in the runtime-image caller-side dispatch engine after the third
  callback return at `0x22002e6c: blxne r0`

Prepared OSOS-side probe:

- patch:
  - `0x22002e70`
  - `00 00 96 e5` -> `62 19 00 ea`
- marker stub:
  - `0x22009400`
  - `mov r0, #25`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the caller-side continuation immediately after the third dispatch callback
  return at `0x22002e70` is also relevant on hardware
- this confirms startup continues through the third callback stage in the
  runtime OSOS caller-side state machine
- behavior still matches the preserved-service68 post-escape signature, not the
  old DFU-resident stall

Classification:

- **CALLER_STAGE_NEXT3_MARKER_REACHED**

## 2026-04-25 Hardware run with caller-side second-stage dispatcher marker at `0x22002ea4`

Context:

- keep preserved original `[service + 0x68]` at `0x22001938`
- keep the preserved-service68 WTF baseline patches:
  - loader callback stub at `0x220019ac`
  - status emulation at `0x220019d0`
  - UART immediate-return patch at `0x22006558`
- continue in the runtime-image caller-side startup path after the first
  three-slot callback loop exits and the second-stage dispatcher begins

Prepared OSOS-side probe:

- patch:
  - `0x22002ea4`
  - `b8 50 9f e5` -> `55 19 00 ea`
- marker stub:
  - `0x22009400`
  - `mov r0, #26`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the second-stage dispatcher entry at `0x22002ea4` is also relevant on
  hardware
- this confirms startup exits the first callback loop cleanly and advances into
  the next caller-side startup stage
- behavior still matches the preserved-service68 post-escape signature, not the
  old DFU-resident stall

Classification:

- **CALLER_STAGE_NEXT4_MARKER_REACHED**

## 2026-04-25 Hardware run with second-stage first-callback-return marker at `0x22002ee0`

Context:

- keep preserved original `[service + 0x68]` at `0x22001938`
- keep the preserved-service68 WTF baseline patches:
  - loader callback stub at `0x220019ac`
  - status emulation at `0x220019d0`
  - UART immediate-return patch at `0x22006558`
- continue in the runtime-image caller-side startup path immediately after the
  first callback return inside the second-stage dispatcher at
  `0x22002edc: blxne r0`

Prepared OSOS-side probe:

- patch:
  - `0x22002ee0`
  - `00 00 95 e5` -> `46 19 00 ea`
- marker stub:
  - `0x22009400`
  - `mov r0, #27`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the continuation immediately after the first callback return inside the
  second-stage dispatcher at `0x22002ee0` is also relevant on hardware
- this confirms startup continues through the first callback slot of the
  second-stage dispatcher body
- behavior still matches the preserved-service68 post-escape signature, not the
  old DFU-resident stall

Classification:

- **CALLER_STAGE_NEXT5_MARKER_REACHED**

## 2026-04-25 Hardware run with second-stage second-callback-return marker at `0x22002f48`

Context:

- keep preserved original `[service + 0x68]` at `0x22001938`
- keep the preserved-service68 WTF baseline patches:
  - loader callback stub at `0x220019ac`
  - status emulation at `0x220019d0`
  - UART immediate-return patch at `0x22006558`
- continue in the runtime-image caller-side startup path immediately after the
  second callback return inside the second-stage dispatcher at
  `0x22002f44: blxne r0`

Prepared OSOS-side probe:

- patch:
  - `0x22002f48`
  - `00 00 95 e5` -> `2c 19 00 ea`
- marker stub:
  - `0x22009400`
  - `mov r0, #28`
  - `blx 0x22002138`
  - loop forever

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device present

Interpretation:

- the continuation immediately after the second callback return inside the
  second-stage dispatcher at `0x22002f48` is also relevant on hardware
- this confirms startup continues through the second callback slot of the
  second-stage dispatcher body
- behavior still matches the preserved-service68 post-escape signature, not the
  old DFU-resident stall

Classification:

- **CALLER_STAGE_NEXT6_MARKER_REACHED**

## 2026-04-24 Hardware run with early OSOS state marker at `0x22002558`

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-early-state-marker-2558.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano present
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- interpretation:
  - this again matches the preserved-service68 post-escape signature, not the
    old DFU-resident failure class
  - the deeper early-OSOS state-machine body entry at `0x22002558` is now also
    relevant on hardware
- classification:
  - **EARLY_OSOS_STATE_MARKER_REACHED**
- recovery status:
  - pending manual DFU recovery confirmation

## 2026-04-24 Hardware run with early OSOS state-next marker at `0x220025a8`

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-early-state-next-marker-25a8.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano present
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- interpretation:
  - this again matches the preserved-service68 post-escape signature, not the
    old DFU-resident failure class
  - the post-`0x220025a4` fallthrough path at `0x220025a8` is now also
    relevant on hardware
- classification:
  - **EARLY_OSOS_STATE_NEXT_MARKER_REACHED**
- recovery status:
  - pending manual DFU recovery confirmation

## 2026-04-24 Hardware run with early OSOS state-next2 marker at `0x220025c8`

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-early-state-next2-marker-25c8.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano present
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- interpretation:
  - this again matches the preserved-service68 post-escape signature, not the
    old DFU-resident failure class
  - the taken path after `0x220025c4` at `0x220025c8` is now also relevant on
    hardware
- classification:
  - **EARLY_OSOS_STATE_NEXT2_MARKER_REACHED**
- recovery status:
  - pending manual DFU recovery confirmation

## 2026-04-24 Hardware run with early OSOS state-next3 marker at `0x220025f4`

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-early-state-next3-marker-25f4.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano present
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- interpretation:
  - this again matches the preserved-service68 post-escape signature, not the
    old DFU-resident failure class
  - the success/armed path at `0x220025f4` is now also relevant on hardware
- classification:
  - **EARLY_OSOS_STATE_NEXT3_MARKER_REACHED**
- recovery status:
  - pending manual DFU recovery confirmation

## 2026-04-24 Hardware run with early OSOS stage-next marker at `0x220025fc`

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-early-stage-next-marker-25fc.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano present
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- interpretation:
  - this again matches the preserved-service68 post-escape signature, not the
    old DFU-resident failure class
  - the helper return at `0x220025fc` is now also relevant on hardware
- classification:
  - **EARLY_OSOS_STAGE_NEXT_MARKER_REACHED**
- recovery status:
  - pending manual DFU recovery confirmation

## 2026-04-24 Early OSOS startup split prepared after `0x220039c4`

- preserved-service68 baseline kept for WTF:
  - `0x22001938 = 31 ff 2f e1`
  - `0x2200193c = 27 00 a0 e3`
  - `0x2200197c = 30 ff 2f e1`
  - `0x220019ac = c6 14 00 eb`
  - `0x220019d0 = 01 10 b0 e3`
  - `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
  - `0x22001b14 = 67 02 00 1b`
  - `0x220024b8 = 10 40 2d e9`
  - `0x220024c8 = 34 ff 2f e1`
- stock OSOS entry bytes restored in the new probe image:
  - `0x22008814 = 6a ec ff ea`
- early startup trace update:
  - `0x22008814` is `bl 0x2200558c`
  - `0x220039c4..0x22003a34` is a CP15 region/MMU setup helper and is not
    itself a good marker site
  - the first useful continuation point after that helper chain returns is the
    common startup dispatcher at `0x220024e4`
- prepared OSOS marker image:
  - `/tmp/n3g-osos-work/n3g-osos-early-marker-024e4.dfu`
  - body dump:
    - `/tmp/n3g-osos-work/lcd-osos-early-marker-024e4.bin`
- active OSOS marker:
  - `0x220024e4`
  - `f4 37 1f e5 -> c1 1b 00 ea`
  - branches to stub at `0x220093f0`
- stub:
  - `0x220093f0`
  - `mov r0, #14`
  - `ldr r3, =0x22002138`
  - `blx r3`
  - `b .`
- raw-body verification:
  - `0x22008814 = 6a ec ff ea`
  - `0x220024e4 = c1 1b 00 ea`
  - `0x220093f0 = 0e 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- status:
  - prepared only
  - not run yet

## 2026-04-24 Hardware run with early OSOS next2 marker at `0x22002548`

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-early-next2-marker-2548.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano present
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- interpretation:
  - this again matches the preserved-service68 post-escape signature, not the
    old DFU-resident failure class
  - the post-`0x22002540` fallthrough path at `0x22002548` is now also
    relevant on hardware
- classification:
  - **EARLY_OSOS_NEXT2_MARKER_REACHED**
- recovery status:
  - pending manual DFU recovery confirmation

## 2026-04-24 Hardware run with early OSOS next marker at `0x22002540`

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
- tested image:
  - `/tmp/n3g-osos-work/n3g-osos-early-next-marker-2540.dfu`
- host result:
  - exploit completed
  - defanged WTF upload completed
  - `wInd3x` timed out waiting for WTF:
    - `device did not switch to WTF mode: context deadline exceeded`
  - post-run `lsusb` showed no Nano present
  - post-run `mks5lboot --dfuscan` showed:
    - no DFU device present
- interpretation:
  - this again matches the preserved-service68 post-escape signature, not the
    old DFU-resident failure class
  - the conditional dispatch at `0x22002540` is now also relevant on hardware
- classification:
  - **EARLY_OSOS_NEXT_MARKER_REACHED**
- recovery status:
  - pending manual DFU recovery confirmation

## 2026-04-24 Hardware run with early OSOS marker at `0x220024e4`

- pre-run DFU was clean:
  - `05ac:1223`
  - state `2`
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
- interpretation:
  - this again matches the preserved-service68 post-escape signature, not the
    old DFU-resident failure class
  - the earliest useful post-`0x220039c4` continuation point at `0x220024e4`
    is now relevant on hardware under the preserved-service68 baseline
- classification:
  - **EARLY_OSOS_MARKER_REACHED**
- recovery status:
  - pending manual DFU recovery confirmation

## 2026-04-24 Next early OSOS dispatcher split prepared after `0x220024e4`

- `0x220024e4` restored to stock in the new image:
  - `f4 37 1f e5`
- traced meaningful early dispatcher points after `0x220024e4`:
  - `0x220024f8: msr CPSR_c, #0x93`
  - `0x22002518: bxne lr`
  - `0x22002540: bne 0x22003c94`
  - `0x22002544: bx lr`
  - `0x22002548: ldr r3, [pc, #-0x86c]`
- prepared next OSOS probe image:
  - `/tmp/n3g-osos-work/n3g-osos-early-next-marker-2540.dfu`
  - body:
    - `/tmp/n3g-osos-work/lcd-osos-early-next-marker-2540.bin`
- active marker:
  - `0x22002540`
  - `d3 05 00 1a -> aa 1b 00 1a`
  - meaning:
    - `bne 0x22003c94 -> bne 0x220093f0`
- stub:
  - `0x220093f0`
  - `0f 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- preserved-service68 WTF baseline remains:
  - `0x22001938 = 31 ff 2f e1`
  - `0x2200193c = 27 00 a0 e3`
  - `0x2200197c = 30 ff 2f e1`
  - `0x220019ac = c6 14 00 eb`
  - `0x220019d0 = 01 10 b0 e3`
  - `0x22006558 = 00 00 a0 e3 1e ff 2f e1`
  - `0x22001b14 = 67 02 00 1b`
  - `0x220024b8 = 10 40 2d e9`
  - `0x220024c8 = 34 ff 2f e1`
- status:
  - prepared only
  - not run yet

## 2026-04-24 Cache-aware DRAM execution probe hardware result

The stronger DRAM probe was run after adding explicit cache maintenance to the
WTF-side copy stub.

Probe structure:

- copy 8 bytes into `0x08000800`:
  - `ldr pc, [pc, #-4]`
  - `.word 0x220076d4`
- then:
  - clean dcache
  - invalidate icache
  - load `pc = 0x08000800`
- `0x220076d4` is an IRAM marker stub:
  - calls `0x22002138(12)`
  - loops forever

Observed host behavior:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` still showed:
  - `05ac:1223`
  - DFU mode
- post-run `mks5lboot --dfuscan` still failed with:
  - `LIBUSB_ERROR_OTHER`

Observed screen behavior:

- black screen / no visible change
- no Apple logo
- no backlight
- no flicker
- no text
- no visible reset loop

Interpretation:

- even after self-modifying the DRAM target and cleaning/invalidating caches,
  the handoff still produces the same DFU-resident late-handoff state
- this is still not enough to prove that CPU execution from `0x08000800` is
  actually happening
- the stronger result rules out the earlier weak-probe explanation that stale
  instruction cache alone was the whole problem

Updated decision:

- **STILL_BLOCKED_WITH_REASON**

## 2026-04-24 Preserved-`[service + 0x68]` execute baseline prepared

Prepared baseline for the next execute-path retest:

- preserve original `[service + 0x68]`:
  - `0x22001938 = 31 ff 2f e1`
- remove the temporary post-call marker:
  - `0x2200193c = 27 00 a0 e3`
- restore original readiness dispatch:
  - `0x2200197c = 30 ff 2f e1`
- keep active:
  - loader callback stub:
    - `0x220019ac = c6 14 00 eb`
  - status emulation:
    - `0x220019d0 = 01 10 b0 e3`
  - UART immediate-return patch:
    - `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Execute wrapper/jump restored:

- `0x22001b14 = 67 02 00 1b`
- `0x220024b8 = 10 40 2d e9`
- `0x220024c8 = 34 ff 2f e1`

Verified with raw-body dumper only:

- `0x1938 = 31 ff 2f e1`
- `0x193c = 27 00 a0 e3`
- `0x197c = 30 ff 2f e1`
- `0x19ac = c6 14 00 eb`
- `0x19d0 = 01 10 b0 e3`
- `0x6558 = 00 00 a0 e3 1e ff 2f e1`
- `0x1b14 = 67 02 00 1b`
- `0x24b8 = 10 40 2d e9`
- `0x24c8 = 34 ff 2f e1`

Prepared decision:

- **PRESERVED_SERVICE68_BASELINE_READY**

## 2026-04-24 Hardware run with preserved-`[service + 0x68]` baseline

Tested baseline:

- original `[service + 0x68]` preserved
- original readiness dispatch restored
- original execute wrapper/jump restored
- still active:
  - loader callback stub
  - status emulation
  - UART immediate-return patch

Observed host behavior:

- clean pre-run DFU:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - no Nano present
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device found

Interpretation:

- this does not match the old DFU-resident late-handoff baseline
- preserving `[service + 0x68]` while restoring the original wrapper/jump path
  still yields full USB disappearance
- strongest current host-side classification:
  - `PRESERVED_SERVICE68_EXECUTE_CHANGED`

## 2026-04-24 Hardware run with OSOS entry-loop probe on preserved-service68 baseline

Tested image:

- `/tmp/n3g-osos-work/n3g-osos-entry-loop-probe.dfu`

WTF baseline under test:

- original `[service + 0x68]`
- original readiness dispatch
- original execute wrapper/jump
- active only:
  - loader callback stub
  - status emulation
  - UART immediate-return patch

Observed host behavior:

- clean pre-run DFU:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - no Nano present
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device found

Interpretation:

- this again matches the preserved-service68 post-escape signature, not the old
  DFU-resident failure class
- strongest current host-side reading:
  - the OSOS startup handoff region remains relevant on hardware once
    `0x200035a8` is preserved

Updated classification:

- **POST_ESCAPE_OSOS_ENTRY_REACHED**

## 2026-04-24 IRAM execution control probe prepared

To separate “late handoff path executes code” from any DRAM/OSOS dependency,
the final handoff was changed to jump directly into a known-good IRAM marker
stub.

Prepared control path:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - status emulation at `0x220019d0`
- patch final wrapper handoff:
  - `0x220024c8`
  - `34 ff 2f e1` -> `81 14 00 ea`
  - branch target:
    - `0x220076d4`

IRAM control stub:

- `0x220076d4`
- bytes:
  - `0c 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- behavior:
  - call `0x22002138(12)`
  - loop forever

Meaning:

- if the same late-handoff path can execute known-good IRAM code, this control
  probe should preserve the reached-marker / DFU-resident style signature while
  avoiding DRAM and OSOS entirely
- if even this does not behave distinctly, the remaining issue is deeper than
  DRAM target contents

Prepared decision:

- **IRAM_EXECUTION_CONTROL_READY**

## 2026-04-24 DFU-context diagnostic after the `[service + 0x68]` bypass

Current strongest suspicion:

- the readiness stub at `0x2200197c` replays concrete scratch-global writes from
  `0x200036c8`
- but the live `[service + 0x68]` bypass at `0x22001938` is still a pure
  no-op that preserves no side effects at all
- since every later handoff experiment still ends in the same DFU-resident
  state, this callback is now the strongest remaining candidate for a missing
  DFU-exit / execution-latch side effect

Prepared diagnostic:

- restore original callback dispatch:
  - `0x22001938`
  - `31 ff 2f e1`
  - `blx r1`
- activate a marker immediately after the call:
  - `0x2200193c`
  - `27 00 a0 e3` -> `69 17 00 ea`
  - branch target:
    - `0x220076e8`

Marker stub:

- `0x220076e8`
- bytes:
  - `0d 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- behavior:
  - call `0x22002138(13)`
  - loop forever

Verified with raw-body dumper:

- `0x1938 = 31 ff 2f e1`
- `0x193c = 69 17 00 ea`
- `0x76e8 = 0d 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`

Meaning of the next run:

- if the marker is reached, `[service + 0x68]` returns under original semantics
  and the remaining problem is more likely its missing side effects from the old
  no-op bypass
- if the marker is not reached, then the callback itself is another non-return /
  stall point in the DFU-exit path

Prepared decision:

- **SERVICE_68_SIDE_EFFECT_REQUIRED**

## 2026-04-24 Hardware test of original `[service + 0x68]` with post-call marker

Tested state:

- restore original callback dispatch:
  - `0x22001938 = 31 ff 2f e1`
- active post-call marker:
  - `0x2200193c = 69 17 00 ea`
  - branch to `0x220076e8`
- keep active:
  - readiness stub
  - loader callback stub
  - UART immediate-return patch
  - status emulation

Observed host behavior:

- clean pre-run DFU:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - no Nano present at all
- post-run `mks5lboot --dfuscan` showed:
  - no DFU device found

Interpretation:

- this is not the old DFU-resident late-handoff signature
- strongest reading is that `[service + 0x68]` returned and control advanced to
  the immediate post-call marker
- therefore the callback itself is not the remaining non-return point
- its side effects are now the strongest remaining missing DFU-exit /
  execution-latch candidate

Updated decision:

- **SERVICE_68_RETURNS**

## 2026-04-24 Repeat hardware confirmation for original `[service + 0x68]`

The `[service + 0x68]` preserved-path test was repeated from clean DFU with the
same patch set:

- `0x22001938 = 31 ff 2f e1`
- `0x2200193c = 69 17 00 ea`

Observed host behavior matched the first run exactly:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run `lsusb` showed no Nano present
- post-run `mks5lboot --dfuscan` showed no DFU device found

Meaning:

- the “USB disappears entirely” signature is repeatable
- that strengthens the conclusion that `[service + 0x68]` returns and that its
  original side effects are the remaining missing DFU-exit / execute-latch
  candidate

## 2026-04-24 Resolved `[service + 0x68]` target and current limit

The recovered BootROM service table at `0x20000080` now resolves the exact
callback pointer for `[service + 0x68]`:

- `0x20000088 -> 0x200035a8`

That sharpens the current model:

- the missing DFU-context escape state is tied to the BootROM routine at
  `0x200035a8`
- preserving that callback changes behavior materially
- bypassing it collapses back to the DFU-resident state

Current limitation:

- the existing local BootROM dumps cover:
  - `0x20000600`
  - `0x20000800`
  - `0x20001000`
  - `0x20001200`
  - `0x20003600`
- they do **not** include the code window containing `0x200035a8`

So the exact side effects are not yet disassemblable from local artifacts.

Updated decision:

- **STILL_BLOCKED_WITH_REASON**

## 2026-04-24 DRAM probe negative-result analysis

The first DRAM probe result is not strong enough to conclude that DRAM at
`0x08000800` is non-executable.

Why the original negative result was weak:

- the probe copied only 8 bytes into `0x08000800`
- it jumped there immediately
- it did not clean dcache
- it did not invalidate icache

So that negative result does not distinguish:

- DRAM execution failure
- stale icache / missing cache maintenance after self-modifying code
- immediate silent failure after successful DRAM fetch

Static finding that sharpens the diagnosis:

- `0x08000800` is not a final OSOS runtime body
- it is a relocation trampoline that quickly routes toward:
  - `0x22000000`

Prepared stronger cache-aware probe:

- preserve wrapper entry and `0x22002138(0)`
- at `0x220024c8`, branch to local copy stub
- copy stub writes into `0x08000800`:
  - `ldr pc, [pc, #-4]`
  - `.word 0x220076d4`
- then:
  - clean dcache via `0x22000f8c`
  - invalidate icache via `0x22001124`
  - jump to `0x08000800`
- `0x220076d4` remains a known IRAM marker stub:
  - `0x22002138(12)`
  - loop forever

Verified bytes:

- `0x220024c8 = a2 14 00 ea`
- `0x220076d4 = 0c 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- `0x22007758 = 14 00 9f e5 14 10 9f e5 00 10 80 e5 10 10 9f e5`
- `0x22007768 = 04 10 80 e5 06 e6 ff eb 6b e6 ff eb 04 f0 9f e5`
- `0x22007778 = 00 08 00 08 04 f0 1f e5 d4 76 00 22`

Prepared alternate destination literals for the same probe:

- `0x08000800`
  - `00 08 00 08`
- `0x08000000`
  - `00 00 00 08`
- `0x08001000`
  - `00 10 00 08`
- `0x22008000`
  - `00 80 00 22`

Decision:

- **ALT_EXECUTION_PROBES_READY**

## 2026-04-24 - direct PC entry patch prepared

Direct non-returning entry patch prepared to bypass `blx r4` call semantics while
preserving the wrapper prologue and the `0x22002138(0)` call.

Verified with the raw-body dumper only:

- `0x22001b14 = 67 02 00 1b`
  - original `blne 0x220024b8`
- `0x220024c8 = a7 14 00 ea`
  - branch to local stub at `0x2200776c`
- `0x2200776c = 04 f0 1f e5 00 08 00 08`
  - `ldr pc, [pc, #-4]`
  - literal `0x08000800`

Meaning:

- WTF still enters the original wrapper at `0x220024b8`
- WTF still executes the wrapper prologue and `0x22002138(0)`
- final handoff no longer uses `blx r4`
- handoff becomes a direct non-returning `pc` load to `0x08000800`

Status:

- `/tmp/wInd3x/wInd3x` rebuilt
- cache cleared at:
  - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
- no hardware run performed yet

## 2026-04-24 - direct PC entry run result

The direct non-returning `pc` load to `0x08000800` did not change the observed
result.

Observed host-side result:

- exploit and defanged WTF upload completed
- `wInd3x` timed out waiting for WTF with:
  - `device did not switch to WTF mode: context deadline exceeded`
- USB remained:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Observed device-side result:

- black screen / no visible change
- no Apple logo
- no backlight
- no flicker
- no patched text
- no stock UI
- no visible reset loop

Interpretation:

- replacing `blx r4` with a direct `pc = 0x08000800` transfer does not change
  the DFU-resident late-handoff behavior
- the unresolved failure is no longer plausibly the wrapper prologue, the final
  `blx` call semantics, or the final target register value alone
- strongest current classification:
  - `DIRECT_PC_SAME_DFU_RESIDENT_STATE`

## 2026-04-24 execution-context fix prepared

Comparison against the solved S5L87xx Rockbox handoff points to execution
context, not target address:

- Nano 2G bootloader does:
  - `disable_irq()`
  - BootROM remap restore
  - cache/protection teardown
  - direct DRAM branch
- shared S5L87xx bootloader does:
  - `disable_irq()`
  - `commit_discard_idcache()`
  - direct kernel entry call

Current Nano 3G WTF tail differs in one critical way:

- `0x22001a28: bl 0x220007bc`
  - disables IRQ/FIQ
- `0x22001ae0: bl 0x220007d0`
  - restores CPSR interrupt-mask bits immediately before the execute gate

Relevant helper meanings:

- `0x22002138(0)`
  - BootROM status/log helper
  - writes a small state record and string pointer
  - not an MMU/cache/vector helper
- `0x22002724`
  - heap/list bookkeeping
  - not an execute-context helper
- `0x220007d0`
  - restores CPSR I/F bits from saved state
  - likely undoes the earlier `0x220007bc` interrupt disable

Prepared structural fix:

- preserve the execute gate and wrapper prologue
- preserve `0x22002138(0)`
- replace final `blx r4` path with a local context stub that:
  - disables IRQ/FIQ
  - disables MMU
  - disables dcache
  - disables icache
  - invalidates/drains caches
  - loads `pc = 0x08000800`

Verified bytes:

- `0x22001b14 = 67 02 00 1b`
- `0x220024c8 = a2 14 00 ea`
- `0x22007758 = 17 e4 ff eb 60 e6 ff eb 6a e6 ff eb 63 e6 ff eb`
- `0x22007768 = 72 e6 ff eb 6c e6 ff eb`
- `0x22007770 = 04 f0 1f e5 00 08 00 08`

Status:

- local `wInd3x` rebuilt
- verified with raw-body dumper only
- no hardware run performed yet

## 2026-04-24 execution-context fix run result

The structural handoff-context patch still did not change the observed result.

Observed host-side result:

- exploit and defanged WTF upload completed
- `wInd3x` timed out waiting for WTF with:
  - `device did not switch to WTF mode: context deadline exceeded`
- USB remained:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Observed device-side result:

- nothing visible on screen
- no Apple logo
- no backlight
- no flicker
- no text
- no visible reset loop

Interpretation:

- even after disabling IRQ/FIQ, disabling MMU and caches, invalidating them,
  and then loading `pc = 0x08000800`, the device still remains in the same
  DFU-resident late-handoff state
- the remaining blocker is therefore still not explained by wrapper semantics,
  final target register value, direct `pc` load semantics, or this minimal
  architectural cleanup alone

## 2026-04-24 OSOS entry dependency and DRAM probe preparation

The first concrete OSOS-side dependency is now identified.

At `0x08000800`, the loaded OSOS body does **not** begin with a final runtime
entry. It begins with a vector/trampoline block:

- `0x08000800: b 0x08009008`
- `0x08009008: ldr sp, [pc, #8]`
- `0x08009010: ldreq sp, [pc, #48]`
- `0x08009014: b 0x080041c4`

And the first real startup block at `0x080041c4` immediately does:

- relocation-delta check via `0x0800901c`
- if nonzero, branch to `0x08004e70`
- `0x08004e70` ultimately `bx 0x22000000`

So the body at `0x08000800` is a relocation trampoline that expects to move
execution toward the linked IRAM image around `0x22000000`. It also very early:

- writes to `0x22000000`
- changes CPSR mode
- calls many Apple service veneers by fixed entry stubs

This means the remaining failure is plausibly an OSOS entry-environment
dependency, not just a bad final branch.

Prepared diagnostic:

- preserve wrapper prologue and `0x22002138(0)`
- at `0x220024c8`, branch to a local copy stub
- copy stub writes a tiny known executable sequence into `0x08000800`:
  - `ldr pc, [pc, #-4]`
  - `.word 0x220076d4`
- then loads `pc = 0x08000800`
- `0x220076d4` is a known marker stub:
  - `0x22002138(12)`
  - loop forever

Verified bytes:

- `0x220024c8 = a2 14 00 ea`
- `0x220076d4 = 0c 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
- `0x22007758 = 10 00 9f e5 10 10 9f e5 00 10 80 e5 0c 10 9f e5`
- `0x22007768 = 04 10 80 e5 08 f0 9f e5 00 08 00 08 04 f0 1f e5`
- `0x22007778 = d4 76 00 22`

Status:

- local `wInd3x` rebuilt
- verified with raw-body dumper only
- no hardware run performed yet

## 2026-04-24 Forced-r4 entry patch prepared

Wrapper re-trace:

- `0x220024b8: push {r4, lr}`
- `0x220024bc: mov r4, r0`
- `0x220024c0: mov r0, #0`
- `0x220024c4: bl 0x22002138`
- `0x220024c8: blx r4`

So inside the original wrapper:

- `r4` is loaded directly from entry `r0`
- the only instruction between `mov r4, r0` and `blx r4` is the call to
  `0x22002138`
- there is no explicit overwrite of `r4` in the wrapper body itself

Prepared forced-target patch:

- `0x22001b14`
  - restored original:
    - `67 02 00 1b`
- `0x220024c8`
  - patched to:
    - `a7 14 00 ea`
  - branch target:
    - `0x2200776c`
- `0x2200776c` stub:
  - `ldr r4, [pc, #4]`
  - `blx r4`
  - `b 0x220024cc`
  - literal:
    - `0x08000800`

Verified raw-body bytes:

- `0x22001b14 = 67 02 00 1b`
- `0x220024c8 = a7 14 00 ea`
- `0x2200776c = 04 40 9f e5 34 ff 2f e1 54 eb ff ea 00 08 00 08`

## 2026-04-24 Single hardware run with forced-r4 handoff patch

Prepared forced-r4 change under test:

- `0x220024c8`
  - original bytes:
    - `34 ff 2f e1`
  - replacement bytes:
    - `a7 14 00 ea`
- `0x2200776c` stub:
  - `ldr r4, [pc, #4]`
  - `blx r4`
  - `b 0x220024cc`
  - literal:
    - `0x08000800`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- host also emitted once during upload:
  - `handle_events: error: libusb: interrupted [code -10]`
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- forcing the final target register to `0x08000800` immediately before the jump
  does not change the host-side DFU-resident outcome
- strongest current host-side classification:
  - `TARGET_STATE_STILL_BLOCKED`

## 2026-04-24 OSOS entry verification and first-instruction probe

Load / entry state re-checked:

- IMG1 header entrypoint:
  - `0x0`
- WTF execute path target:
  - `r0 = 0x08000800`
  - wrapper copies:
    - `r4 = 0x08000800`
- expected load base:
  - `0x08000000`
- expected body entry:
  - `0x08000800`

CPU-state conclusions:

- target address is even:
  - `0x08000800`
- so the final `blx r4` stays in ARM mode, not Thumb
- wrapper-local stack use is normal:
  - `push {r4, lr}`
  - `pop {r4, lr}`
- direct-jump wrapper-bypass produced the same host-side result, so the wrapper
  prologue and `0x22002138(0)` call are not the distinguishing factor

Runtime body content at `0x08000800`:

- stock bytes:
  - `00 22 00 ea`
  - `f2 0c 00 ea`
  - `f3 0c 00 ea`
  - `f4 0c 00 ea`
- stock disassembly:
  - `0x08000800: b 0x08009008`
  - followed by valid ARM branch-table startup code

That rules out a trivially malformed or obviously unloaded entry body.

Prepared first-instruction entry probe:

- image:
  - `/tmp/n3g-osos-work/n3g-osos-entry-first-loop-probe.dfu`
- body:
  - `/tmp/n3g-osos-work/lcd-osos-entry-first-loop-probe.bin`
- patch:
  - offset `0x0` in the OSOS body
  - `00 22 00 ea` -> `fe ff ff ea`
  - runtime effect:
    - `0x08000800: b 0x08000800`

Probe meaning:

- if CPU really executes `0x08000800`, this should self-loop immediately at the
  first OSOS instruction and change behavior relative to the current baseline

Updated decision:

- **FIX_READY**

## 2026-04-24 Single hardware run with OSOS first-instruction loop probe

Prepared probe under test:

- OSOS body offset `0x0`
  - original bytes:
    - `00 22 00 ea`
  - replacement bytes:
    - `fe ff ff ea`
  - runtime effect:
    - `0x08000800: b 0x08000800`

Observed host behavior:

- clean DFU start:
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

- the first-instruction self-loop probe did not produce a new host-side
  signature versus the current DFU-resident baseline
- host-side alone this does not prove entry execution
- strongest current host-side reading:
  - entry execution is still unproven

## 2026-04-24 Direct-jump wrapper-bypass test prepared

Prepared direct-jump handoff change:

- `0x22001b14`
  - original bytes:
    - `67 02 00 1b`
  - replacement bytes:
    - `6b 02 00 ea`
  - effect:
    - branch directly to `0x220024c8`
    - skip wrapper prologue and `0x22002138(0)`

Wrapper preserved in the body for comparison:

- `0x220024b8 = 10 40 2d e9`
- `0x220024bc = 00 40 a0 e1`
- `0x220024c0 = 00 00 a0 e3`
- `0x220024c4 = 1b ff ff eb`
- `0x220024c8 = 34 ff 2f e1`

Other active Nano 3G WTF patches still present:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Expected next-run classification:

- `DIRECT_JUMP_BEHAVIOR_CHANGED`
- `DIRECT_JUMP_BLACKSCREEN`
- `DIRECT_JUMP_SAME_DFU_RESIDENT_STATE`

## 2026-04-24 Single hardware run with direct-jump wrapper-bypass test

Prepared direct-jump change under test:

- `0x22001b14`
  - original bytes:
    - `67 02 00 1b`
  - replacement bytes:
    - `6b 02 00 ea`
  - effect:
    - branch directly to `0x220024c8`

Observed host behavior:

- clean DFU start:
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

- bypassing the wrapper prologue and the `0x22002138(0)` call does not change
  the host-side outcome
- strongest current host-side classification:
  - `DIRECT_JUMP_SAME_DFU_RESIDENT_STATE`

## 2026-04-24 Execute-wrapper entry marker prepared at 0x220024b8

Wrapper disassembly from the raw WTF body:

- `0x220024b8: e92d4010`
  - `push {r4, lr}`
- `0x220024bc: e1a04000`
  - `mov r4, r0`
- `0x220024c0: e3a00000`
  - `mov r0, #0`
- `0x220024c4: ebffff1b`
  - `bl 0x22002138`
- `0x220024c8: e12fff34`
  - `blx r4`
- `0x220024cc: e8bd4010`
  - `pop {r4, lr}`
- `0x220024d0: e3a00002`
  - `mov r0, #2`

Prepared wrapper-entry split:

- `0x220024b8`
  - original bytes:
    - `10 40 2d e9`
  - replacement bytes:
    - `ab 14 00 ea`
  - active branch target:
    - `0x2200776c`

Execute-gate state restored ahead of the wrapper:

- `0x22001b04 = 00 00 54 e3`
- `0x22001b0c = 10 00 15 e3`
- `0x22001b14 = 67 02 00 1b`

Other active Nano 3G WTF patches still present:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Optional direct-jump patch prepared but not applied:

- wrapper-internal bypass:
  - `0x220024b8`
  - `10 40 2d e9` -> `02 00 00 ea`
  - effect:
    - direct branch to `0x220024c8`
    - skips the wrapper prologue and `0x22002138(0)` call

Expected next-run classification:

- `WRAPPER_ENTRY_REACHED`
- `WRAPPER_BLOCKED`
- if entry is reached, the next split moves to the pre-jump site at `0x220024c8`

## 2026-04-24 Execute-wrapper pre-jump marker prepared at 0x220024c8

Wrapper disassembly from the verified raw body:

- `0x220024b8`
  - `push {r4, lr}`
- `0x220024bc`
  - `mov r4, r0`
- `0x220024c0`
  - `mov r0, #0`
- `0x220024c4`
  - `bl 0x22002138`
- `0x220024c8`
  - `blx r4`
- `0x220024cc`
  - `pop {r4, lr}`
- `0x220024d0`
  - `mov r0, #2`

Prepared wrapper pre-jump split:

- `0x220024c8`
  - original bytes:
    - `34 ff 2f e1`
  - replacement bytes:
    - `a7 14 00 ea`
  - active branch target:
    - `0x2200776c`

Wrapper entry restored:

- `0x220024b8 = 10 40 2d e9`

Other active Nano 3G WTF patches still present:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Expected next-run classification:

- `WRAPPER_PREJUMP_REACHED`
- `WRAPPER_BLOCKED`

## 2026-04-24 Single hardware run with wrapper pre-jump marker at 0x220024c8

Prepared wrapper pre-jump split under test:

- `0x220024c8`
  - original bytes:
    - `34 ff 2f e1`
  - replacement bytes:
    - `a7 14 00 ea`
  - marker stub:
    - `0x2200776c`

Observed host behavior:

- clean DFU start:
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

- this matches the same DFU-resident reached-marker host signature seen for the
  wrapper-entry and execute-gate markers
- strongest current classification:
  - `WRAPPER_PREJUMP_REACHED`

## 2026-04-24 Single hardware run with wrapper-entry marker at 0x220024b8

Prepared wrapper-entry split under test:

- `0x220024b8`
  - original bytes:
    - `10 40 2d e9`
  - replacement bytes:
    - `ab 14 00 ea`
  - marker stub:
    - `0x2200776c`

Observed host behavior:

- clean DFU start:
  - `05ac:1223`
  - state `2`
- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF:
  - `device did not switch to WTF mode: context deadline exceeded`
- host also emitted:
  - `handle_events: error: libusb: interrupted [code -10]`
- post-run `lsusb` showed:
  - `05ac:1223`
  - DFU mode still present
- post-run `mks5lboot --dfuscan` showed:
  - `LIBUSB_ERROR_OTHER`

Interpretation:

- this matches the same DFU-resident reached-marker host signature seen for the
  earlier execute-gate markers
- strongest current classification:
  - `WRAPPER_ENTRY_REACHED`

## 2026-04-24 Single hardware run with execute-gate marker at 0x22001b14

Prepared execute-gate split under test:

- `0x22001b14`
  - original bytes:
    - `67 02 00 1b`
  - replacement bytes:
    - `14 17 00 ea`
  - marker stub:
    - `0x2200776c`

Observed host behavior:

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

- this matches the same DFU-resident reached-marker host signature seen for the
  earlier execute-gate markers
- strongest current classification:
  - `MARKER_1B14_REACHED`

## 2026-04-24 Post-landing tail marker prepared at 0x22001a78

The next linear tail split after the confirmed `0x22001a4c` reach is the first
`[service + 0x8c]` callback site:

- `0x22001a78`
  - original bytes:
    - `32 ff 2f e1`
  - replacement bytes:
    - `22 17 00 ea`
  - active branch target:
    - `0x22007708`

The post-landing verification state is now:

- restored:
  - `0x22001a24 = 00 40 a0 e3`
  - `0x22001a28 = 63 fb ff eb`
  - `0x22001a4c = 2a 06 00 eb`
- active:
  - `0x22001a78 = 22 17 00 ea`

Other active Nano 3G-specific WTF patches remain:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Verification method:

- use only the raw-body dumper:
  - `go run -a ./cmd/dump_n3g_defanged.go ...`
- do not trust the older helper-verification path for this late-tail stage

Layout fix:

- moved the USB product string out of the active stub area
- marker stub now starts at:
  - `0x22007708`
- product string now starts at:
  - `0x2200771c`

Expected next-run classification:

- `MARKER_1A78_REACHED`
- `MARKER_1A78_NOT_REACHED`

## 2026-04-24 Single hardware run with tail marker at 0x22001a78

Prepared tail split under test:

- `0x22001a78`
  - original bytes:
    - `32 ff 2f e1`
  - replacement bytes:
    - `22 17 00 ea`
  - marker stub:
    - `0x22007708`

Observed host behavior:

- clean DFU start:
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

- this matches the same DFU-resident reached-marker host signature seen for the
  earlier confirmed checkpoints
- strongest current classification:
  - `MARKER_1A78_REACHED`

Recovery status:

- clean manual DFU re-entry not yet re-confirmed after this run

## 2026-04-24 Single hardware run with execute-gate marker at 0x22001b0c

Prepared execute-gate split under test:

- `0x22001b0c`
  - original bytes:
    - `10 00 15 e3`
  - replacement bytes:
    - `16 17 00 ea`
  - marker stub:
    - `0x2200776c`

Observed host behavior:

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

- this matches the same DFU-resident reached-marker host signature seen for the
  earlier execute-gate and late-tail splits
- strongest current classification:
  - `MARKER_1B0C_REACHED`

## 2026-04-24 Execute-gate marker prepared at 0x22001b04

The tail is now restored through `0x22001ae8`, and the next split moves into
the execute-gate region itself:

- `0x22001ae8`
  - restored original bytes:
    - `0d 03 00 eb`
- `0x22001b04`
  - original bytes:
    - `00 00 54 e3`
  - replacement bytes:
    - `18 17 00 ea`
  - active branch target:
    - `0x2200776c`

Current execute-gate bytes from the verified raw body:

- `0x22001b04 = 18 17 00 ea`
- `0x22001b08 = 02 00 00 1a`
- `0x22001b0c = 10 00 15 e3`
- `0x22001b10 = 07 00 86 10`
- `0x22001b14 = 67 02 00 1b`

Other active Nano 3G WTF patches still present:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Verification:

- trusted path remains the raw-body dumper only
- relevant bytes confirmed from the dumped body before any hardware run

Expected next-run classification:

- `MARKER_1B04_REACHED`
- `MARKER_1B04_NOT_REACHED`

## 2026-04-24 Single hardware run with unconditional execute-force patch

Prepared execute-force change under test:

- `0x22001b14`
  - original bytes:
    - `67 02 00 1b`
  - replacement bytes:
    - `67 02 00 ea`

Observed host behavior:

- clean DFU start:
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

- even forcing the final branch to `0x220024b8` does not change the host-side
  outcome from the DFU-resident reached-marker pattern
- host-side alone this looks like the same late-handoff non-transition state as
  the execute-gate markers

Classification status:

- pending screen observation to distinguish blackscreen from any visible boot
  progress

## 2026-04-24 Single hardware run with execute-gate marker at 0x22001b04

Prepared execute-gate split under test:

- `0x22001b04`
  - original bytes:
    - `00 00 54 e3`
  - replacement bytes:
    - `18 17 00 ea`
  - marker stub:
    - `0x2200776c`

Observed host behavior:

- clean DFU start:
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

- this matches the same DFU-resident reached-marker host signature seen for the
  confirmed late-tail markers
- strongest current classification:
  - `MARKER_1B04_REACHED`

Recovery status:

- clean manual DFU re-entry not yet re-confirmed after this run

## 2026-04-24 Final tail marker prepared at 0x22001ae8

The final unresolved tail split is now active:

- `0x22001ae8`
  - original bytes:
    - `0d 03 00 eb`
  - replacement bytes:
    - `1f 17 00 ea`
  - active branch target:
    - `0x2200776c`

The verified late-tail state is now:

- restored:
  - `0x22001ae0 = 3a fb ff eb`
- active:
  - `0x22001ae8 = 1f 17 00 ea`

Other active Nano 3G WTF patches still present:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Verification:

- trusted path remains the raw-body dumper only
- relevant bytes confirmed from the dumped body before any hardware run

Expected next-run classification:

- `MARKER_1AE8_REACHED`
- `MARKER_1AE8_NOT_REACHED`

## 2026-04-24 Single hardware run with final tail marker at 0x22001ae8

Prepared tail split under test:

- `0x22001ae8`
  - original bytes:
    - `0d 03 00 eb`
  - replacement bytes:
    - `1f 17 00 ea`
  - marker stub:
    - `0x2200776c`

Observed host behavior:

- clean DFU start:
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

- this matches the same DFU-resident reached-marker host signature seen for the
  earlier confirmed checkpoints in the post-landing tail
- strongest current classification:
  - `MARKER_1AE8_REACHED`

Recovery status:

- clean manual DFU re-entry not yet re-confirmed after this run

## 2026-04-24 Post-landing tail marker prepared at 0x22001ae0

The next and last meaningful tail split before the still-unreached
`0x22001ae8` is:

- `0x22001ae0`
  - original bytes:
    - `3a fb ff eb`
  - replacement bytes:
    - `21 17 00 ea`
  - active branch target:
    - `0x2200776c`

The verified post-landing state is now:

- restored:
  - `0x22001a24 = 00 40 a0 e3`
  - `0x22001a28 = 63 fb ff eb`
  - `0x22001a4c = 2a 06 00 eb`
  - `0x22001a78 = 32 ff 2f e1`
  - `0x22001ab0 = 32 ff 2f e1`
- active:
  - `0x22001ae0 = 21 17 00 ea`

Other active Nano 3G WTF patches still present:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Verification:

- trusted path remains the raw-body dumper only
- relevant bytes confirmed from the dumped body before any hardware run

Expected next-run classification:

- `MARKER_1AE0_REACHED`
- `MARKER_1AE0_NOT_REACHED`

## 2026-04-24 Single hardware run with tail marker at 0x22001ae0

Prepared tail split under test:

- `0x22001ae0`
  - original bytes:
    - `3a fb ff eb`
  - replacement bytes:
    - `21 17 00 ea`
  - marker stub:
    - `0x2200776c`

Observed host behavior:

- clean DFU start:
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

- this matches the same DFU-resident reached-marker host signature seen for the
  earlier confirmed checkpoints in the post-landing tail
- strongest current classification:
  - `MARKER_1AE0_REACHED`

Recovery status:

- clean manual DFU re-entry not yet re-confirmed after this run

## 2026-04-24 Post-landing tail marker prepared at 0x22001ab0

The next linear tail split after the confirmed `0x22001a78` reach is the second
`[service + 0x8c]` callback site:

- `0x22001ab0`
  - original bytes:
    - `32 ff 2f e1`
  - replacement bytes:
    - `28 17 00 ea`
  - active branch target:
    - `0x22007758`

The verified post-landing state is now:

- restored:
  - `0x22001a24 = 00 40 a0 e3`
  - `0x22001a28 = 63 fb ff eb`
  - `0x22001a4c = 2a 06 00 eb`
  - `0x22001a78 = 32 ff 2f e1`
- active:
  - `0x22001ab0 = 28 17 00 ea`

Other active Nano 3G WTF patches still present:

- `0x22001938 = 64 17 00 eb`
- `0x2200197c = 1f 15 00 eb`
- `0x220019ac = c6 14 00 eb`
- `0x220019d0 = 01 10 b0 e3`
- `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Verification:

- trusted path remains the raw-body dumper only
- relevant bytes confirmed from the dumped body before any hardware run

Expected next-run classification:

- `MARKER_1AB0_REACHED`
- `MARKER_1AB0_NOT_REACHED`

## 2026-04-24 Single hardware run with tail marker at 0x22001ab0

Prepared tail split under test:

- `0x22001ab0`
  - original bytes:
    - `32 ff 2f e1`
  - replacement bytes:
    - `28 17 00 ea`
  - marker stub:
    - `0x22007758`

Observed host behavior:

- clean DFU start:
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

- this matches the same DFU-resident reached-marker host signature seen for the
  earlier confirmed checkpoints in the post-landing tail
- strongest current classification:
  - `MARKER_1AB0_REACHED`

Recovery status:

- clean manual DFU re-entry not yet re-confirmed after this run

## 2026-04-24 Prepared post-poll tail marker at `0x220019d8`

The late-gap AES/status gate has now been tested both by removing the
`0x220019d4` back-edge and by forcing the completion condition at
`0x220019d0`. Both still ended in `CHAINLOAD_BLACKSCREEN`, so the next split is
the first instruction after the poll region.

Prepared local Nano 3G-only state:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- keep original:
  - `0x220019d4`
    - `fc ff ff 0a`
- active post-poll marker:
  - `0x220019d8`
  - original bytes:
    - `00 00 50 e3`
  - replacement bytes:
    - `3d 17 00 ea`
  - effect:
    - `b 0x220076d4`

Prepared but inactive follow-ups:

- `0x220019dc`
  - original:
    - `10 00 00 1a`
  - prepared branch target:
    - `0x220076e8`
- `0x22001a24`
  - original:
    - `00 40 a0 e3`
  - prepared branch target:
    - `0x220076fc`

Marker stubs:

- `0x220076d4`
  - `0x22002138(12)`
  - loop forever
- `0x220076e8`
  - `0x22002138(13)`
  - loop forever
- `0x220076fc`
  - `0x22002138(14)`
  - loop forever

Verification from fresh local-module defanger output:

- `0x19c8 = 15 00 00 0a`
- `0x19d0 = 01 10 b0 e3`
- `0x19d4 = fc ff ff 0a`
- `0x19d8 = 3d 17 00 ea`
- `0x1938 = 64 17 00 eb`

Decision:

- **POST_POLL_MARKER_PREPARED**

## 2026-04-24 Hardware run with post-poll marker at `0x220019d8`

Tested local Nano 3G-only WTF state:

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

Observed host behavior:

- clean DFU start:
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

- this matches the same reached-marker DFU-resident host signature seen on the
  earlier mid/late-path marker runs
- strongest current classification:
  - `MARKER_19D8_REACHED`
- therefore control reaches the first post-poll tail instruction at
  `0x220019d8`
- the next unresolved split is now:
  - `0x220019dc`
  - then `0x22001a24`

Decision:

- **MARKER_19D8_REACHED**

## 2026-04-24 Resolved local verification mismatch for the landing-block split

The older `tmp_defang_check.go` + `tmp_verify_marker.go` path remained one step
behind after the final-tail edits. The reliable local verification path is:

- force a full local rebuild:
  - `go run -a ./cmd/dump_n3g_defanged.go <in> <out-img1> <out-body>`
- inspect the raw dumped body directly with `xxd`

Using that path, the generated Nano 3G defanged WTF body now matches the
landing-block source intent.

Verified local Nano 3G-only state:

- `0x220019d8`
  - restored:
    - `00 00 50 e3`
- `0x220019dc`
  - restored original branch:
    - `10 00 00 1a`
- `0x22001a24`
  - active landing-block marker:
    - `34 17 00 ea`
- still active:
  - `0x22001938 = 64 17 00 eb`
  - `0x2200197c = 1f 15 00 eb`
  - `0x220019ac = c6 14 00 eb`
  - `0x220019d0 = 01 10 b0 e3`
  - `0x22006558 = 00 00 a0 e3 1e ff 2f e1`

Decision:

- **MARKER_1A24_VERIFIED_READY**

## 2026-04-24 Hardware run with landing-block marker at `0x22001a24`

Tested local Nano 3G-only WTF state:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- restored:
  - `0x220019d8`
    - `00 00 50 e3`
  - `0x220019dc`
    - `10 00 00 1a`
- active marker:
  - `0x22001a24`
  - `00 40 a0 e3` -> `34 17 00 ea`

Observed host behavior:

- clean DFU start:
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

- this matches the same reached-marker DFU-resident host signature seen on the
  earlier tail splits
- strongest current classification:
  - `MARKER_1A24_REACHED`
- therefore control reaches the landing block at `0x22001a24`
- the next unresolved region is now later than that landing, toward the still
  unreached tail around `0x22001ae8`

Decision:

- **MARKER_1A24_REACHED**

## 2026-04-24 Prepared post-landing tail marker after `0x22001a24`

Linear tail trace from `0x22001a24` to `0x22001ae8`:

- `0x22001a24: mov r4, #0`
- `0x22001a28: bl 0x220007bc`
- `0x22001a2c..0x22001a48`
  - store/update state pointers and stack scratch
- `0x22001a4c: bl 0x220032fc`
- `0x22001a78: blx [service + 0x8c]` with ID `19`
- `0x22001a88: bl 0x220032fc`
- `0x22001ab0: blx [service + 0x8c]` with ID `33`
- `0x22001ab4..0x22001adc`
  - store saved words / restore service slot state
- `0x22001ae0: bl 0x220007d0`
- `0x22001ae8: bl 0x22002724`

There are no new conditional branches in this band before `0x22001ae8`; the
next meaningful split after the reached landing block is therefore the very
first call at `0x22001a28`.

Prepared local Nano 3G-only state:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- restore:
  - `0x22001a24`
    - `00 40 a0 e3`
- active post-landing marker:
  - `0x22001a28`
  - original bytes:
    - `63 fb ff eb`
  - replacement bytes:
    - `33 17 00 ea`
  - effect:
    - `b 0x220076fc`

Raw-body verification (trusted path):

- `0x1a24 = 00 40 a0 e3`
- `0x1a28 = 33 17 00 ea`
- `0x1938 = 64 17 00 eb`
- `0x197c = 1f 15 00 eb`
- `0x19ac = c6 14 00 eb`
- `0x19d0 = 01 10 b0 e3`
- `0x6558 = 00 00 a0 e3 1e ff 2f e1`
- marker stub area:
  - `0x76fc` contains the marker body
  - USB string moved to `0x7710` to avoid overlap

Why this checkpoint:

- it is the earliest real call after the confirmed landing at `0x22001a24`
- the tail from `0x1a24` forward is linear, so this is the cleanest next split
  before the previously failed tail endpoint at `0x22001ae8`

Decision:

- **POST_1A24_MARKER_PREPARED**

## 2026-04-24 Hardware run with post-landing marker at `0x22001a28`

Tested local Nano 3G-only WTF state:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- restored:
  - `0x22001a24`
    - `00 40 a0 e3`
- active marker:
  - `0x22001a28`
  - `63 fb ff eb` -> `33 17 00 ea`

Observed host behavior:

- clean DFU start:
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

- this matches the same reached-marker DFU-resident host signature seen on the
  earlier tail splits
- strongest current classification:
  - `MARKER_1A28_REACHED`
- therefore control reaches the first post-landing call at `0x22001a28`
- the unresolved blocker is later in the tail, still before the previously
  failed `0x22001ae8` marker

Decision:

- **MARKER_1A28_REACHED**

## 2026-04-24 Prepared deeper post-landing tail marker at `0x22001a4c`

The tail remains linear after the reached `0x22001a28` call, so the next
meaningful split is the following call at `0x22001a4c`.

Prepared local Nano 3G-only state:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- restore:
  - `0x22001a28`
    - `63 fb ff eb`
- active marker:
  - `0x22001a4c`
  - original bytes:
    - `2a 06 00 eb`
  - replacement bytes:
    - `2b 17 00 ea`
  - effect:
    - `b 0x22007700`

Raw-body verification (trusted path):

- `0x1a24 = 00 40 a0 e3`
- `0x1a28 = 63 fb ff eb`
- `0x1a4c = 2b 17 00 ea`
- `0x1938 = 64 17 00 eb`
- `0x197c = 1f 15 00 eb`
- `0x19ac = c6 14 00 eb`
- `0x19d0 = 01 10 b0 e3`
- `0x6558 = 00 00 a0 e3 1e ff 2f e1`
- active stub area:
  - `0x7700` contains the marker body
  - USB string moved farther down and no longer overlaps it

Why chosen:

- it is the next real call after the reached `0x22001a28`
- the band remains linear toward the still-failed `0x22001ae8`, so this is the
  cleanest next split

Decision:

- **MARKER_1A4C_PREPARED**

## 2026-04-24 Hardware run with deeper post-landing marker at `0x22001a4c`

Tested local Nano 3G-only WTF state:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- restored:
  - `0x22001a28`
    - `63 fb ff eb`
- active marker:
  - `0x22001a4c`
  - `2a 06 00 eb` -> `2b 17 00 ea`

Observed host behavior:

- clean DFU start:
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

- this matches the same reached-marker DFU-resident host signature seen on the
  earlier tail splits
- strongest current classification:
  - `MARKER_1A4C_REACHED`
- therefore control reaches the call at `0x22001a4c`
- the unresolved blocker is later in the linear tail, still before the
  previously failed `0x22001ae8`

Decision:

- **MARKER_1A4C_REACHED**

## 2026-04-24 Hardware run with final-tail marker at `0x220019dc`

Tested local Nano 3G-only WTF state:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
  - AES/status completion emulation at `0x220019d0`
- restored:
  - `0x220019d8`
    - `00 00 50 e3`
- active marker:
  - `0x220019dc`
  - `10 00 00 1a` -> `41 17 00 ea`

Observed host behavior:

- clean DFU start:
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

- this matches the same reached-marker DFU-resident host signature seen on the
  earlier tail splits
- strongest current classification:
  - `MARKER_19DC_REACHED`
- therefore control reaches the final conditional branch at `0x220019dc`
- the next unresolved split is now the landing block at:
  - `0x22001a24`

Decision:

- **MARKER_19DC_REACHED**

## 2026-04-24 Prepared AES/status completion emulation at `0x220019d0`

The narrow `0x220019d4` poll-loop bypass still ended in
`CHAINLOAD_BLACKSCREEN`, which means simply removing the back-edge is not
enough. The late-gap state still needs to look "complete" to WTF.

Register meaning found:

- `0x38c0000c`
  - lives in the S5L8702 AES/status block
  - late-gap code samples it at:
    - `0x220019cc: ldr r1, [r2, #0x0c]`
  - original completion check:
    - `0x220019d0: tst r1, #1`
    - `0x220019d4: beq 0x220019cc`
- practical interpretation:
  - bit `0` is treated by WTF as a completion/ready indication for the
    AES/decrypt-status path before it continues toward `0x22001a24`

Prepared minimal emulation:

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
  - original bytes:
    - `01 00 11 e3`
  - replacement bytes:
    - `01 10 b0 e3`
  - effect:
    - replace `tst r1, #1` with `movs r1, #1`

Why this is safer than a raw MMIO write:

- it preserves the existing read from `0x38c0000c`
- it preserves the original branch at `0x220019d4`
- it does not guess a hardware write value for the AES block
- it only forces the tested ready/completion condition to pass once

Verification from fresh local-module defanger output:

- `0x19c8 = 15 00 00 0a`
- `0x19d0 = 01 10 b0 e3`
- `0x19d4 = fc ff ff 0a`
- `0x1938 = 64 17 00 eb`
- `0x197c = 1f 15 00 eb`
- `0x19ac = c6 14 00 eb`

Decision:

- **STATUS_FLAG_PATCH_READY**

## 2026-04-24 Hardware run with AES/status completion emulation at `0x220019d0`

Tested local Nano 3G-only WTF state:

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

Observed host behavior:

- clean DFU start:
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

Observed screen behavior:

- black screen for the full 90-second window
- no Apple logo
- no backlight
- no flicker
- no patched text
- no stock UI
- no visible reset loop

Interpretation:

- forcing the late-gap completion condition to pass at `0x220019d0` was still
  not sufficient to produce visible chainload/runtime progress
- host-side the device remained in the same DFU-resident reached-marker style
  state
- strongest current classification:
  - `CHAINLOAD_BLACKSCREEN`

Decision:

- **CHAINLOAD_BLACKSCREEN**

## 2026-04-24 Late-gap branch/poll marker after `[service + 0x68]` bypass

After the `0x22001938` local bypass still produced `CHAINLOAD_BLACKSCREEN`, the
next unresolved decision point in the same late gap is the status branch at
`0x220019c8`.

Late-gap disassembly:

- `0x220019bc: mov r2, #0x38c00000`
- `0x220019c0: ldr r1, [r2, #0x40]`
- `0x220019c4: tst r1, #3`
- `0x220019c8: beq 0x22001a24`
- `0x220019cc: ldr r1, [r2, #0x0c]`
- `0x220019d0: tst r1, #1`
- `0x220019d4: beq 0x220019cc`

Meaning:

- the direct branch at `0x220019c8` reaches the later success path when
  `0x38c00040 & 3 == 0`
- otherwise WTF falls into the poll loop at `0x220019cc..0x220019d4`
- that loop waits until `0x38c0000c & 1 != 0`

Prepared next split:

- keep active:
  - readiness stub at `0x2200197c`
  - loader callback stub at `0x220019ac`
  - UART immediate-return patch at `0x22006558`
  - `[service + 0x68]` bypass at `0x22001938`
- active marker:
  - `0x220019c8`
  - original bytes:
    - `15 00 00 0a`
  - replacement bytes:
    - `41 17 00 ea`
  - effect:
    - `b 0x220076d4`
- marker stub:
  - `0x220076d4`
  - bytes:
    - `0b 00 a0 e3 04 30 9f e5 33 ff 2f e1 fe ff ff ea 38 21 00 22`
  - behavior:
    - `0x22002138(11)`
    - loop forever

Verification:

- fresh clean-cache local defanger output shows:
  - `0x1938 = 64 17 00 eb`
  - `0x19c8 = 41 17 00 ea`
  - `0x76d0 = 1e ff 2f e1`
  - `0x76d4 = 0b 00 a0 e3 ... 38 21 00 22`

Decision:

- **LATE_GAP_MARKER_PREPARED**

## 2026-04-24 Hardware run with late-gap marker at `0x220019c8`

Tested local Nano 3G-only WTF state:

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

Observed host behavior:

- clean DFU start:
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

- this matches the same reached-marker DFU-resident host signature seen at the
  earlier middle-chain markers
- strongest current classification:
  - `MARKER_19C8_REACHED`
- therefore execution reaches the late-gap branch gate at `0x220019c8`
- the remaining unresolved blocker is later than that branch point, with the
  next obvious candidate still being the poll loop at `0x220019cc..0x220019d4`

Decision:

- **MARKER_19C8_REACHED**

## 2026-04-24 Prepared poll-loop bypass at `0x220019d4`

With `0x220019c8` now proven reached, the first unresolved late-gap hang
candidate is the back-edge in the poll loop:

- `0x220019cc: ldr r1, [r2, #0x0c]`
- `0x220019d0: tst r1, #1`
- `0x220019d4: beq 0x220019cc`

Interpretation:

- `r2` is already set to the AES/status block base at `0x38c00000`
- the loop waits for `0x38c0000c & 1 != 0`
- if that bit never sets, WTF never reaches the later `cmp r0, #0` /
  `mov r4, #0x57` path and therefore never advances toward the execute gate

Prepared minimal bypass:

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
  - original bytes:
    - `fc ff ff 0a`
  - replacement bytes:
    - `00 00 a0 e1`
  - effect:
    - `nop`

Why safe enough for one controlled test:

- it preserves the existing late-gap reads at:
  - `0x220019cc`
  - `0x220019d0`
- it only removes the infinite back-edge, so WTF samples the AES/status register
  once and then falls through to the next logic
- this is narrower than skipping the entire late-gap block or forcing the branch
  at `0x220019c8`

Verification from fresh local-module defanger output:

- `0x19c8 = 15 00 00 0a`
- `0x19d4 = 00 00 a0 e1`
- `0x1938 = 64 17 00 eb`
- `0x197c = 1f 15 00 eb`
- `0x19ac = c6 14 00 eb`

Decision:

- **POLL_LOOP_BYPASS_PREPARED**

## 2026-04-24 Hardware run with late-gap poll-loop bypass at `0x220019d4`

Tested local Nano 3G-only WTF state:

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

Observed host behavior:

- clean DFU start:
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

Observed screen behavior:

- black screen for the full 90-second window
- no Apple logo
- no backlight
- no flicker
- no patched text
- no stock UI
- no visible reset loop

Interpretation:

- removing the late-gap poll-loop back-edge was not sufficient to produce
  visible OSOS/chainload success
- host-side the device remained in the same DFU-resident reached-marker state
- strongest current classification:
  - `CHAINLOAD_BLACKSCREEN`

Decision:

- **CHAINLOAD_BLACKSCREEN**

# iPod Nano 3G OSOS Display Sequence

Date: 2026-04-24
Source artifact: `/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin`
Base address: `0x22000000`

## Goal

Extract the first higher-level Apple-backed sequence in RetailOS (`OSOS`) that
appears to make the Nano 3G display visible after the lower-level WTF LCD init
path.

## Analysis Method

- `arm-elf-eabi-objdump -D -b binary -m arm --adjust-vma=0x22000000`
- `strings -t x -n 6`
- source cross-check against:
  - `firmware/target/arm/s5l8702/ipodnano3g/backlight-nano3g.c`
  - `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c`
  - `firmware/target/arm/s5l8702/ipod6g/backlight-6g.c`

No Ghidra is installed on this host, so this reduction is from disassembly and
cross-references only.

## What OSOS Adds Over WTF

The decrypted OSOS image contains:

- user-facing backlight strings and menu assets:
  - `HandleCycleBacklightSetting`
  - `HandleBacklightSelected`
  - `Backlight`
  - `Backlight On`
  - `Backlight Off`
  - `SetBacklight_AlwaysOff`
  - `SetBacklight_AlwaysOn`
  - `TCSlideshowLCD`
- a distinct high-level display/backlight service region around:
  - `0x22005084..0x2200565c`

This is stronger evidence than WTF for a real screen-visible path, because WTF
only exposed low-level LCD and PMU fragments while OSOS contains explicit
backlight UI/state logic.

## Confirmed OSOS Service Veneers

Several tiny import veneers branch into fixed Apple ROM/service code:

| OSOS veneer | ROM target | Role in observed sequence | Evidence |
| --- | --- | --- | --- |
| `0x2200367c` | `0x080646b4` | first subcall inside `0x22004c5c` | `0x22004c68` |
| `0x22003684` | `0x08064790` | second subcall inside `0x22004c5c` | `0x22004c7c` |
| `0x2200374c` | `0x080db704` | display/backlight service getter | `0x22005624`, `0x22005644`, `0x220057d4`, `0x22005c68` |
| `0x2200376c` | `0x080dbd8c` | paired "off" helper on that service | `0x22005628` |
| `0x22003774` | `0x080dbe58` | paired "on" helper on that service | `0x22005648`, `0x220057e0` |

These are not guessed addresses: they are direct veneer targets from the
decrypted OSOS body.

## Full OSOS Display-On Sequence Seen In Body

The clearest higher-level "make it visible" segment is inside
`0x220057d4..0x220057fc`:

1. `0x220057d4`: `bl 0x2200374c`
   - get display/backlight service object
2. `0x220057d8`: `bl 0x22004c5c`
   - prepare/reset that service object
3. `0x220057dc`: `bl 0x2200374c`
   - get the service object again
4. `0x220057e0`: `bl 0x22003774`
   - visibility-on helper
5. `0x220057f8`: `bl 0x2200378c`
   - post-on mode write to `[r4->0x1c] + 0x84`, with mode `1` or `2`

This runs immediately before later display payload/canvas setup at
`0x22005800..0x22005918`.

## Supporting On/Off Wrappers

OSOS also exposes tiny wrappers:

### `0x22005620`

- `bl 0x2200374c`
- `bl 0x2200376c`
- `bl 0x220073b4`
- `mov r1, #0`
- `bl 0x22007610`

Interpretation:

- compact "visibility off" wrapper around the same service object

### `0x22005640`

- `bl 0x2200374c`
- `bl 0x22003774`
- `bl 0x220073b4`
- `mov r1, #1`
- `bl 0x22007610`

Interpretation:

- compact "visibility on" wrapper around the same service object

This strongly reinforces that `0x22003774` is the "on" side of the pair.

## `0x22004c5c` Body-Visible Behavior

`0x22004c5c` itself is small and fully visible in OSOS:

1. add `0x44` to the service-object pointer
2. call imported helper `0x2200367c`
3. clear byte `[service + 0x05]`
4. add `0x44` again
5. tail-call imported helper `0x22003684`

The exact MMIO performed by `0x2200367c` / `0x22003684` is not visible in the
OSOS body because those are ROM/service imports.

## Minimal Candidate Sequence

The smallest Apple-backed sequence with direct evidence for "turn visible" is:

1. existing LCD init path from the prepared Nano 3G payload:
   - startup/pregate
   - LCD local init
2. `service = import_0x2200374c()`
3. `apple_0x22004c5c(service)`
4. `service = import_0x2200374c()`
5. `import_0x22003774(service)`
6. loop

Why this sequence:

- it is the shortest OSOS body-visible segment that appears directly before the
  higher-level display payload path
- it avoids unrelated backlight menu logic, storage paths, wheel-side paths,
  and rejected PMU guesses
- it keeps the one new dependency bounded to the Apple display/backlight
  service calls actually observed in OSOS

Why `0x2200378c` is not included in the first payload:

- it writes through an application object field (`[r4->0x1c] + 0x84`), not the
  service object returned by `0x2200374c`
- that caller context is not yet reduced enough for a standalone payload

## Register / MMIO Evidence

### Directly visible in OSOS body

- `0x22004c70`: clear byte `[service + 0x05]`

### Not directly visible in OSOS body

The hardware-facing writes of the candidate sequence are encapsulated in the
Apple ROM/service imports:

- `0x080646b4`
- `0x08064790`
- `0x080db704`
- `0x080dbe58`

So the exact PMU/GPIO/MMIO register list for the minimal on-sequence is still
opaque from OSOS body analysis alone.

This is still preferable to guessed MMIO because:

- every callsite is Apple-authored and observed in decrypted OSOS
- the sequence is copied from an actual OSOS display-on path
- no invented PMU register or GPIO ownership is introduced

## Risk Assessment

| Step | Risk | Reason |
| --- | --- | --- |
| Existing pregate + LCD-local init | Medium | previously executed safely but with black screen |
| `0x2200374c` service getter | Medium | imported ROM/service call; exact body unknown |
| `0x22004c5c` | Medium | Apple body code plus two imported service calls |
| `0x22003774` | Medium | imported ROM/service call; likely hardware-visible on helper |

## Decision

- **OSOS_DECRYPT_COMPLETE_CANDIDATE_FOUND**

Candidate chosen:

- **USE_OTHER_SINGLE_ACTION**
- exact new visibility action:
  - OSOS display/backlight service sequence
    - `0x2200374c`
    - `0x22004c5c`
    - `0x2200374c`
    - `0x22003774`

Prepared artifact:

- `tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.bin`
- prepared only, not executed

## Single Hardware Run Result

Date: 2026-04-24

`lcd-osos-visible-n3g.bin` was run once on real hardware.

### Host-side result

- clean pre-send baseline:
  - `05ac:1223`
  - DFU state `2`
- upload succeeded through `wInd3x run`
- post-send state:
  - `lsusb` still showed `05ac:1223`
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- recovery succeeded after manual reset:
  - DFU state `2` restored

### Visual result

- no reliable screen observation was captured during the intended 20-second
  observation window

### Classification

- **EXECUTION ONLY**
- note:
  - **no visual observation captured**

### Interpretation

The OSOS-derived service-call sequence is strong enough to reproduce the same
DFU takeover signature as prior executing payloads, but this single run does
not prove visible display/backlight success because the screen observation was
missed.

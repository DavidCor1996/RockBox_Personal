# iPod Nano 3G Ghidra HW-Init Triage

This note explains how to run the Nano 3G Ghidra triage script after firmware decryption.

## Script

- Path: `tools/ghidra/Nano3GHwInitTriage.py`
- Goal: quickly surface hardware-init code by tagging:
  - ARM926EJ-S low-level patterns (CP15 and CPSR interactions)
  - memory-mapped I/O usage
  - repeated MMIO writes
  - polling loops
  - bitmask-heavy register setup
  - candidate init functions for NAND, LCD, and power management

## Use After Decryption

1. Decrypt firmware image (outside this script).
2. Import into Ghidra as ARM little-endian.
   - Typical S5L8702 runtime bases from Rockbox headers:
     - `IRAM0_ORIG = 0x22000000`
     - `DRAM_ORIG = 0x08000000`
     - MMIO region starts at `0x38000000`
3. If you know the exact load address for your dumped blob, set image base accordingly before running analysis.
4. Run auto-analysis normally.
5. Run script:
   - `Window -> Script Manager`
   - add the directory containing `Nano3GHwInitTriage.py` if needed
   - run `Nano3GHwInitTriage.py`

## What the Script Marks

- Bookmarks (type `Analysis`) in categories:
  - `Nano3G-ARM926`
  - `Nano3G-IO`
  - `Nano3G-Poll`
  - `Nano3G-Bitmask`
  - `Nano3G-Candidate`
- Comments:
  - EOL comments on individual instructions
  - plate comments on function entry points for candidate subsystems
- Labels created when candidate thresholds are met:
  - `cand_lcd_init_<addr>`
  - `cand_nand_init_<addr>`
  - `cand_power_mgmt_<addr>`

## Heuristics Used

- ARM926EJ-S pattern hints:
  - `MCR/MRC p15` (cache/MMU/system control style operations)
  - `MSR/MRS CPSR` (mode/IRQ control style operations)
- MMIO detection:
  - immediate constants, literal-pool constants, and simple `[reg,#off]` effective addresses
  - S5L8702-centric ranges (LCD, clock/power, MIU, I2C, I2S, USB OTG/PHY, GPIO, VIC, wheel/NAND-collision address window)
- Repeated writes:
  - same MMIO address written multiple times in one function
- Polling loops:
  - backward branch + compare/test + MMIO read in loop window
- Bitmask operations:
  - `and/orr/bic/eor/tst/cmp` style instruction hotspots in MMIO-relevant functions

## Nano 3G Specific Notes

- `0x3C200000` is clickwheel on S5L8702 and overlaps historical NAND FMC address used on older S5L variants.
- Therefore, NAND candidates are intentionally conservative and should be reviewed manually with cross-references.
- For LCD and power init, strongest clues are usually clustered MMIO writes plus polling loops and bitmask setup.

## Practical Workflow

1. Sort bookmarks by category.
2. Start with `Nano3G-Candidate` labels.
3. For each candidate, inspect nearby xrefs/callers to find top-level init sequencing.
4. Confirm by matching known register neighborhoods:
   - LCD IF: `0x38300000` range
   - USB PHY: `0x3C400000` range
   - clock/power gates: `0x3C500000` range
   - I2C buses: `0x3C600000` / `0x3C900000`
5. Rename refined functions manually once confidence is high.

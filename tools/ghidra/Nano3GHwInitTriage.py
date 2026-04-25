# Nano3G hardware-init triage script for Ghidra
#@author OpenCode
#@category Rockbox.Nano3G
#@keybinding
#@menupath
#@toolbar

from ghidra.program.model.lang import Register
from ghidra.program.model.scalar import Scalar


MMIO_RANGES = [
    ("LCD_IF", 0x38300000, 0x383000ff),
    ("LCD_CLCD", 0x39200000, 0x39200100),
    ("CLK", 0x3c500000, 0x3c500100),
    ("MIU", 0x38100000, 0x38100400),
    ("I2C0", 0x3c600000, 0x3c600040),
    ("I2C1", 0x3c900000, 0x3c900040),
    ("I2S0", 0x3ca00000, 0x3ca00080),
    ("SPI0", 0x3c300000, 0x3c300100),
    ("GPIO", 0x3cf00000, 0x3cf00400),
    ("USB_OTG", 0x38400000, 0x38410000),
    ("USB_PHY", 0x3c400000, 0x3c400080),
    ("VIC", 0x38e00000, 0x38e02000),
    ("WHEEL_OR_NAND_COLLISION", 0x3c200000, 0x3c200100),
    ("NAND_FMC_8701", 0x39400000, 0x39400200),
    ("ECC", 0x39e00000, 0x39e00100),
]

LCD_GROUPS = set(["LCD_IF", "LCD_CLCD"])
NAND_GROUPS = set(["NAND_FMC_8701", "WHEEL_OR_NAND_COLLISION", "ECC"])
POWER_GROUPS = set(["CLK", "I2C0", "I2C1", "GPIO", "USB_PHY", "MIU", "VIC"])

BITMASK_MNEMS = set([
    "and", "ands", "orr", "orrs", "eor", "eors", "bic", "bics",
    "tst", "teq", "cmp",
])

READ_MNEMS = set(["ldr", "ldrb", "ldrh", "ldrsb", "ldrsh"])
WRITE_MNEMS = set(["str", "strb", "strh"])

MAX_LOOP_WINDOW = 0x80
REPEATED_WRITE_THRESHOLD = 3
MAX_BITMASK_BOOKMARKS = 800


def u32(x):
    return x & 0xffffffff


def fmt_u32(x):
    return "0x%08X" % u32(x)


def classify_mmio(addr_u32):
    for name, start, end in MMIO_RANGES:
        if addr_u32 >= start and addr_u32 <= end:
            return name
    return None


def parse_int(text):
    t = text.strip().lower()
    if t.startswith("+"):
        t = t[1:]
    if t.startswith("-0x"):
        return -int(t[3:], 16)
    if t.startswith("0x"):
        return int(t, 16)
    if t.startswith("-"):
        return -int(t[1:], 10)
    return int(t, 10)


def parse_mem_operand(op_repr):
    s = op_repr.lower().replace(" ", "")
    if "[" not in s or "]" not in s:
        return None
    try:
        inner = s[s.index("[") + 1:s.index("]")]
    except Exception:
        return None

    parts = inner.split(",")
    if len(parts) < 1:
        return None

    base = parts[0]
    if base not in ["r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
                    "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"]:
        return None

    if len(parts) == 1:
        return (base, 0)

    off = parts[1]
    if not off.startswith("#"):
        return None
    try:
        return (base, parse_int(off[1:]))
    except Exception:
        return None


def get_dest_reg(instr):
    if instr.getNumOperands() < 1:
        return None
    objs = instr.getOpObjects(0)
    if len(objs) == 1 and isinstance(objs[0], Register):
        return objs[0].getName().lower()
    return None


def get_immediates(instr):
    vals = []
    for i in range(instr.getNumOperands()):
        objs = instr.getOpObjects(i)
        for obj in objs:
            if isinstance(obj, Scalar):
                vals.append(u32(obj.getUnsignedValue()))
    return vals


def try_read_dword(addr):
    mem = currentProgram.getMemory()
    if not mem.contains(addr):
        return None
    try:
        return u32(mem.getInt(addr))
    except Exception:
        return None


def append_eol(addr, msg):
    old = getEOLComment(addr)
    if old is None or len(old) == 0:
        setEOLComment(addr, msg)
    elif msg not in old:
        setEOLComment(addr, old + " | " + msg)


def append_plate(addr, msg):
    old = getPlateComment(addr)
    if old is None or len(old) == 0:
        setPlateComment(addr, msg)
    elif msg not in old:
        setPlateComment(addr, old + "\n" + msg)


def make_unique_label(addr, base):
    label = base
    for i in range(0, 10):
        try:
            createLabel(addr, label, True)
            return label
        except Exception:
            label = "%s_%d" % (base, i + 1)
    return None


def add_bookmark(addr, category, comment):
    bm = currentProgram.getBookmarkManager()
    bm.setBookmark(addr, "Analysis", category, comment)


def is_branch_mnem(m):
    if m == "bl" or m == "blx":
        return False
    return m.startswith("b")


def mark_arm926_pattern(instr):
    m = instr.getMnemonicString().lower()
    txt = instr.toString().lower()
    if (m == "mcr" or m == "mrc") and "p15" in txt:
        add_bookmark(instr.getAddress(), "Nano3G-ARM926", "CP15 access pattern")
        append_eol(instr.getAddress(), "ARM926 pattern: CP15")
        return True
    if (m == "msr" or m == "mrs") and "cpsr" in txt:
        add_bookmark(instr.getAddress(), "Nano3G-ARM926", "CPSR mode/IRQ pattern")
        append_eol(instr.getAddress(), "ARM926 pattern: CPSR")
        return True
    return False


def analyze_function(func, stats):
    body = func.getBody()
    entry = func.getEntryPoint()
    inst = getInstructionAt(entry)

    reg_consts = {}

    mmio_events = []
    read_events = []
    write_events = []
    bitmask_events = []
    compare_offsets = []
    back_edges = []
    range_hits = {}

    while inst is not None and body.contains(inst.getAddress()):
        monitor.checkCanceled()

        addr = inst.getAddress()
        off = u32(addr.getOffset())
        mnem = inst.getMnemonicString().lower()

        if mark_arm926_pattern(inst):
            stats["arm926_patterns"] += 1

        if mnem in ["cmp", "tst", "teq", "ands", "subs"]:
            compare_offsets.append(off)

        if is_branch_mnem(mnem):
            flows = inst.getFlows()
            if flows is not None and len(flows) > 0:
                t = flows[0]
                toff = u32(t.getOffset())
                if toff < off and (off - toff) <= MAX_LOOP_WINDOW:
                    back_edges.append((off, toff, addr, t, mnem))

        # gather MMIO hits from immediate scalars
        for imm in get_immediates(inst):
            rng = classify_mmio(imm)
            if rng is not None:
                mmio_events.append((off, rng, imm, "imm", addr, mnem))
                range_hits[rng] = range_hits.get(rng, 0) + 1

        # gather MMIO hits from [reg,#off] with lightweight reg-value propagation
        for opi in range(inst.getNumOperands()):
            op_repr = inst.getDefaultOperandRepresentation(opi)
            parsed = parse_mem_operand(op_repr)
            if parsed is None:
                continue
            base, disp = parsed
            if base in reg_consts and reg_consts[base] is not None:
                eff = u32(reg_consts[base] + disp)
                rng = classify_mmio(eff)
                if rng is not None:
                    mmio_events.append((off, rng, eff, "mem", addr, mnem))
                    range_hits[rng] = range_hits.get(rng, 0) + 1

                    if mnem in READ_MNEMS:
                        read_events.append((off, eff, addr))
                    elif mnem in WRITE_MNEMS:
                        write_events.append((off, eff, addr))

        # literal-pool reads for address constants
        if mnem.startswith("ldr"):
            refs = inst.getReferencesFrom()
            if refs is not None:
                for ref in refs:
                    to_addr = ref.getToAddress()
                    if to_addr is None:
                        continue
                    val = try_read_dword(to_addr)
                    if val is None:
                        continue
                    rng = classify_mmio(val)
                    if rng is not None:
                        mmio_events.append((off, rng, val, "lit", addr, mnem))
                        range_hits[rng] = range_hits.get(rng, 0) + 1

        # bitmask operations
        if mnem in BITMASK_MNEMS:
            imms = get_immediates(inst)
            for imm in imms:
                if imm != 0:
                    bitmask_events.append((off, imm, addr, mnem))

        # very lightweight register constant tracking
        dst = get_dest_reg(inst)
        imms = get_immediates(inst)
        known = None

        if dst is not None:
            if mnem.startswith("movw") and len(imms) > 0:
                old = reg_consts.get(dst)
                low = imms[0] & 0xffff
                if old is None:
                    known = low
                else:
                    known = (old & 0xffff0000) | low
            elif mnem.startswith("movt") and len(imms) > 0:
                old = reg_consts.get(dst)
                high = (imms[0] & 0xffff) << 16
                if old is None:
                    known = high
                else:
                    known = high | (old & 0xffff)
            elif mnem.startswith("mov") and len(imms) > 0:
                known = imms[0]
            elif mnem.startswith("ldr") and len(imms) > 0:
                # frequent literal-pool fallback heuristic
                for imm in imms:
                    if classify_mmio(imm) is not None:
                        known = imm
                        break
            elif (mnem.startswith("add") or mnem.startswith("sub") or
                  mnem.startswith("orr") or mnem.startswith("bic") or
                  mnem.startswith("and") or mnem.startswith("eor")):
                if instr.getNumOperands() >= 2:
                    op1 = instr.getOpObjects(1)
                    src = None
                    if len(op1) == 1 and isinstance(op1[0], Register):
                        src = op1[0].getName().lower()
                    if src is not None and src in reg_consts and reg_consts[src] is not None and len(imms) > 0:
                        v = reg_consts[src]
                        imm = imms[0]
                        if mnem.startswith("add"):
                            known = u32(v + imm)
                        elif mnem.startswith("sub"):
                            known = u32(v - imm)
                        elif mnem.startswith("orr"):
                            known = u32(v | imm)
                        elif mnem.startswith("bic"):
                            known = u32(v & (~imm))
                        elif mnem.startswith("and"):
                            known = u32(v & imm)
                        elif mnem.startswith("eor"):
                            known = u32(v ^ imm)

            reg_consts[dst] = known

        inst = inst.getNext()

    if len(mmio_events) == 0:
        return

    stats["functions_with_mmio"] += 1

    # repeated writes
    writes_by_addr = {}
    for _, waddr, iaddr in write_events:
        if waddr not in writes_by_addr:
            writes_by_addr[waddr] = []
        writes_by_addr[waddr].append(iaddr)

    repeated_write_count = 0
    for waddr, addrs in writes_by_addr.items():
        if len(addrs) >= REPEATED_WRITE_THRESHOLD:
            repeated_write_count += 1
            rng = classify_mmio(waddr)
            msg = "Repeated MMIO writes: %s %s (%d writes)" % (rng, fmt_u32(waddr), len(addrs))
            add_bookmark(addrs[0], "Nano3G-IO", msg)
            append_eol(addrs[0], msg)
            stats["repeated_write_sites"] += 1

    # polling loops: back-edge + compare + mmio read in loop window
    polling_hits = 0
    for boff, toff, baddr, taddr, bmnem in back_edges:
        has_cmp = False
        for coff in compare_offsets:
            if coff >= toff and coff <= boff:
                has_cmp = True
                break

        if not has_cmp:
            continue

        loop_read = None
        for roff, raddr, riaddr in read_events:
            if roff >= toff and roff <= boff:
                loop_read = (raddr, riaddr)
                break

        if loop_read is None:
            continue

        raddr, riaddr = loop_read
        rng = classify_mmio(raddr)
        msg = "Polling loop candidate (%s): %s %s" % (bmnem, rng, fmt_u32(raddr))
        add_bookmark(baddr, "Nano3G-Poll", msg)
        append_eol(baddr, msg)
        polling_hits += 1
        stats["polling_loops"] += 1

    # bitmask ops: highlight in MMIO-relevant functions
    bitmask_marked = 0
    if stats["bitmask_marked"] < MAX_BITMASK_BOOKMARKS:
        for _, mask, iaddr, mnem in bitmask_events:
            if stats["bitmask_marked"] >= MAX_BITMASK_BOOKMARKS:
                break
            msg = "Bitmask op %s mask=%s" % (mnem, fmt_u32(mask))
            add_bookmark(iaddr, "Nano3G-Bitmask", msg)
            append_eol(iaddr, msg)
            stats["bitmask_marked"] += 1
            bitmask_marked += 1

    # pre-label candidates
    lcd_score = 0
    nand_score = 0
    power_score = 0
    for rng, hits in range_hits.items():
        if rng in LCD_GROUPS:
            lcd_score += hits
        if rng in NAND_GROUPS:
            nand_score += hits
        if rng in POWER_GROUPS:
            power_score += hits

    lcd_score += repeated_write_count
    nand_score += repeated_write_count
    power_score += repeated_write_count

    lcd_score += polling_hits
    nand_score += polling_hits
    power_score += polling_hits

    # conservative thresholds to reduce false-positive labeling
    if lcd_score >= 4:
        label = make_unique_label(entry, "cand_lcd_init_%08X" % u32(entry.getOffset()))
        note = "Candidate LCD init (score=%d, ranges=%s)" % (lcd_score, ", ".join(sorted([k for k in range_hits.keys() if k in LCD_GROUPS])))
        add_bookmark(entry, "Nano3G-Candidate", note)
        append_plate(entry, note)
        if label is not None:
            stats["cand_lcd"] += 1

    if nand_score >= 3:
        label = make_unique_label(entry, "cand_nand_init_%08X" % u32(entry.getOffset()))
        note = "Candidate NAND init (score=%d, ranges=%s)" % (nand_score, ", ".join(sorted([k for k in range_hits.keys() if k in NAND_GROUPS])))
        add_bookmark(entry, "Nano3G-Candidate", note)
        append_plate(entry, note)
        if label is not None:
            stats["cand_nand"] += 1

    if power_score >= 6:
        label = make_unique_label(entry, "cand_power_mgmt_%08X" % u32(entry.getOffset()))
        note = "Candidate power mgmt (score=%d, ranges=%s)" % (power_score, ", ".join(sorted([k for k in range_hits.keys() if k in POWER_GROUPS])))
        add_bookmark(entry, "Nano3G-Candidate", note)
        append_plate(entry, note)
        if label is not None:
            stats["cand_power"] += 1


def main():
    if currentProgram is None:
        println("No program open.")
        return

    lang = currentProgram.getLanguage().getLanguageDescription().toString()
    println("[Nano3G] Program: %s" % currentProgram.getName())
    println("[Nano3G] Language: %s" % lang)
    println("[Nano3G] Starting ARM926/MMIO triage...")

    stats = {
        "functions_total": 0,
        "functions_with_mmio": 0,
        "arm926_patterns": 0,
        "repeated_write_sites": 0,
        "polling_loops": 0,
        "bitmask_marked": 0,
        "cand_lcd": 0,
        "cand_nand": 0,
        "cand_power": 0,
    }

    fm = currentProgram.getFunctionManager()
    funcs = fm.getFunctions(True)
    for func in funcs:
        monitor.checkCanceled()
        stats["functions_total"] += 1
        analyze_function(func, stats)

    println("[Nano3G] Done.")
    println("[Nano3G] functions scanned      : %d" % stats["functions_total"])
    println("[Nano3G] functions with MMIO    : %d" % stats["functions_with_mmio"])
    println("[Nano3G] ARM926 patterns        : %d" % stats["arm926_patterns"])
    println("[Nano3G] repeated write sites   : %d" % stats["repeated_write_sites"])
    println("[Nano3G] polling loop candidates: %d" % stats["polling_loops"])
    println("[Nano3G] bitmask highlights     : %d" % stats["bitmask_marked"])
    println("[Nano3G] candidate labels       : LCD=%d NAND=%d POWER=%d" % (
        stats["cand_lcd"], stats["cand_nand"], stats["cand_power"]
    ))


main()

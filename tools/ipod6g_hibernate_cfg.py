"""Conservative ARM control-flow checks for the retained-resume audit.

Track integer constants and the zero flag so a display-failed branch cannot
spuriously become a successful display/tick branch. Unknown values fork both
ways. Calls invalidate caller-saved registers and flags. This is a local
control-flow check, not an ARM emulator or a hardware qualification.
"""
import re


class UnsafePath(RuntimeError):
    pass


def require_boundary(body, prerequisites, targets, owner):
    instructions = []
    for line in body.splitlines():
        match = re.match(r"\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+"
                         r"([a-z][a-z0-9.]*)\s*(.*)$", line)
        if match:
            instructions.append((int(match[1], 16), int(match[2], 16),
                                 match[3], match[4]))
    if not instructions:
        raise UnsafePath(f"cannot build control-flow graph for {owner}")
    addresses = {item[0]: i for i, item in enumerate(instructions)}
    aliases = {"sl": 10, "fp": 11, "ip": 12, "sp": 13, "lr": 14, "pc": 15}

    def reg(name):
        name = name.strip()
        if re.fullmatch(r"r(?:[0-9]|1[0-5])", name):
            return int(name[1:])
        return aliases.get(name)

    def value(operand, registers):
        if operand.startswith("#"):
            return int(operand[1:], 0) & 0xffffffff
        number = reg(operand)
        return registers[number] if number is not None else None

    # PC, known registers, Z, register last compared against zero.
    work = [(0, (None,) * 16, None, None)]
    seen = set()
    while work:
        state = work.pop()
        if state in seen:
            continue
        seen.add(state)
        if len(seen) > 100000:
            raise UnsafePath(f"control-flow state limit in {owner}")
        index, known, zero, compared = state
        address, word, opcode, operands = instructions[index]
        condition = word >> 28
        predicate = zero if condition == 0 else (
            None if zero is None else not zero) if condition == 1 else (
            True if condition in (14, 15) else None)
        for execute in ((False, True) if predicate is None else (predicate,)):
            registers = list(known)
            z, cmpreg = zero, compared
            if condition in (0, 1):
                z = execute if condition == 0 else not execute
                if z and cmpreg is not None:
                    registers[cmpreg] = 0
            if not execute:
                if index + 1 < len(instructions):
                    work.append((index + 1, tuple(registers), z, cmpreg))
                continue

            # Decode branches from the instruction word, not "bl*": ble and
            # bls are conditional branches, not branch-with-link calls.
            immediate_branch = (word & 0x0e000000) == 0x0a000000
            exchange = (word & 0x0ffffff0) in (0x012fff10, 0x012fff30)
            call = (immediate_branch and (word & 0x01000000) != 0) or (
                exchange and (word & 0x20) != 0)
            if condition == 15 and immediate_branch:
                call = True  # BLX immediate
            if immediate_branch or exchange:
                for target in targets:
                    if f"<{target}>" in operands:
                        raise UnsafePath(f"{owner} can call {target} before "
                                         + " or ".join(prerequisites))
                if any(f"<{name}>" in operands for name in prerequisites):
                    continue  # This path crossed the required boundary.
                if call:
                    for number in (0, 1, 2, 3, 12, 14):
                        registers[number] = None
                    z = cmpreg = None
                elif immediate_branch:
                    target = int(operands.split()[0], 16)
                    if target in addresses:
                        work.append((addresses[target], tuple(registers), z, cmpreg))
                    continue
                else:
                    # A return or a tail call leaves the containing function.
                    continue
            elif (opcode.startswith(("pop", "ldm")) and
                  re.search(r"\bpc\b", operands)):
                continue
            else:
                args = [part.strip() for part in operands.split(";")[0].split(",")]
                base = opcode[:-2] if condition < 14 else opcode
                # Exact register/immediate operations only. Shifted operands
                # and unsupported instructions invalidate their destination.
                if base in ("cmp", "tst") and len(args) == 2:
                    left, right = (value(arg, registers) for arg in args)
                    cmpreg = reg(args[0]) if base == "cmp" and right == 0 else None
                    if base == "tst" and (left == 0 or right == 0):
                        z = True
                    elif left is None or right is None:
                        z = None
                    else:
                        z = (left == right) if base == "cmp" else not (left & right)
                elif args and reg(args[0]) is not None and not base.startswith(
                        ("str", "stm", "push", "msr")):
                    destination = reg(args[0])
                    result = None
                    operation = base[:-1] if base.endswith("s") else base
                    if operation == "mov" and len(args) == 2:
                        result = value(args[1], registers)
                    elif operation in ("and", "orr", "eor", "add", "sub", "bic") and len(args) == 3:
                        a, b = value(args[1], registers), value(args[2], registers)
                        if operation == "and" and (a == 0 or b == 0):
                            result = 0
                        elif a is not None and b is not None:
                            result = {"and": lambda: a & b, "orr": lambda: a | b,
                                      "eor": lambda: a ^ b, "add": lambda: a + b,
                                      "sub": lambda: a - b, "bic": lambda: a & ~b}[operation]() & 0xffffffff
                    registers[destination] = result
                    if destination == cmpreg:
                        cmpreg = None
                    if base.endswith("s") or base in ("teq", "cmn"):
                        z = None if result is None else result == 0
                        cmpreg = None
            if index + 1 < len(instructions):
                work.append((index + 1, tuple(registers), z, cmpreg))

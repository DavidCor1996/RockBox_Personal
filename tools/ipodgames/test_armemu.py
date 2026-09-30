"""Execute synthetic ARMv5 instructions in the real iPod Games CPU core.

No game executable or Apple assets are required. Expectations use Python's
unbounded integers, independently of the interpreter's arithmetic helpers.
"""

import ctypes
import random
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "apps/plugins/ipodgames/armemu"
MASK = (1 << 32) - 1
N, Z, C, V = (1 << 31, 1 << 30, 1 << 29, 1 << 28)

STUB = r"""
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define ICODE_ATTR
struct plugin_api { void *(*memset)(void *, int, size_t); };
extern const struct plugin_api *rb;
static inline uint16_t load_le16_aligned(const void *p)
{
    const unsigned char *b = p;
    return b[0] | (uint16_t)b[1] << 8;
}
static inline uint32_t load_le32_aligned(const void *p)
{
    const unsigned char *b = p;
    return load_le16_aligned(b) | (uint32_t)load_le16_aligned(b + 2) << 16;
}
static inline void store_le16_aligned(void *p, uint16_t v)
{
    unsigned char *b = p;
    b[0] = v; b[1] = v >> 8;
}
static inline void store_le32_aligned(void *p, uint32_t v)
{
    unsigned char *b = p;
    store_le16_aligned(b, v); store_le16_aligned(b + 2, v >> 16);
}
"""

HARNESS = r"""
#include "armemu.h"
static const struct plugin_api api = { .memset = memset };
const struct plugin_api *rb = &api;
void run_instruction(u32 instruction, u32 *registers, int batched)
{
    u32 memory[4] = {0};
    cpu_t cpu = {0};
    machine_t machine = {0};
    machine.dram = memory;
    machine.drambase = 0x18000000;
    machine.dramsize = sizeof(memory);
    memcpy(cpu.r, registers, 16 * sizeof(u32));
    cpu.r[CPSR] = registers[16] | CPSR_mode_usr;
    cpu.r[PC] = machine.drambase + 8;
    store_le32_aligned(memory, instruction);
    if (batched)
        execute_until(&machine, &cpu, machine.drambase + 4, 1);
    else
        execute(&machine, &cpu);
    memcpy(registers, cpu.r, 16 * sizeof(u32));
    registers[16] = cpu.r[CPSR];
    registers[17] = machine.instructions;
    registers[18] = machine.fault;
}
"""


def signed(value):
    return value if value < (1 << 31) else value - (1 << 32)


def nz(value, bits=32):
    return (N if value & (1 << (bits - 1)) else 0) | (Z if value == 0 else 0)


class ArmArithmeticTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="ipodgames-armemu-")
        cls.addClassCleanup(cls.directory.cleanup)
        path = Path(cls.directory.name)
        (path / "plugin.h").write_text(STUB)
        (path / "harness.c").write_text(HARNESS)
        subprocess.run([
            "gcc", "-shared", "-fPIC", "-O2", "-std=gnu99", "-DSIMULATOR",
            "-I", str(path), "-I", str(CORE), str(path / "harness.c"),
            str(CORE / "execute.c"), str(CORE / "runtime.c"),
            "-o", str(path / "core.so"),
        ], check=True, capture_output=True, text=True)
        cls.library = ctypes.CDLL(str(path / "core.so"))
        cls.library.run_instruction.argtypes = [
            ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32), ctypes.c_int,
        ]
        cls.library.run_instruction.restype = None

    def run_arm(self, instruction, inputs, flags, batched):
        registers = (ctypes.c_uint32 * 19)()
        for register, value in inputs.items():
            registers[register] = value
        registers[16] = flags
        self.library.run_instruction(instruction, registers, batched)
        self.assertEqual(registers[17], 1)
        self.assertEqual(registers[18], 0)
        self.assertEqual(registers[15], 0x1800000c)
        return list(registers)

    def test_signed_long_multiply(self):
        # SMULL/SMLAL r2,r3,r0,r1, including modulo-2^64 accumulation.
        rng = random.Random(2563290)
        cases = [(0xffffffff, 2, 0), (0x80000000, 0xffffffff, 0),
                 (0xffffffff, 1, 1), (1, 1, 0x7fffffffffffffff)]
        cases += [(rng.getrandbits(32), rng.getrandbits(32),
                   rng.getrandbits(64)) for _ in range(100)]
        for a, b, accumulator in cases:
            for accumulate in (False, True):
                for set_flags in (False, True):
                    instruction = (0xe0c32190 | (int(accumulate) << 21)
                                   | (int(set_flags) << 20))
                    expected = (signed(a) * signed(b) +
                                (accumulator if accumulate else 0)) % (1 << 64)
                    for batched in (False, True):
                        with self.subTest(a=a, b=b, accumulate=accumulate,
                                          set_flags=set_flags, batched=batched):
                            result = self.run_arm(instruction, {
                                0: a, 1: b, 2: accumulator & MASK,
                                3: accumulator >> 32,
                            }, N | Z | C | V, batched)
                            self.assertEqual(result[2] | result[3] << 32,
                                             expected)
                            self.assertEqual(result[16], 0x10 | C | V |
                                             (nz(expected, 64) if set_flags
                                              else N | Z))

    def test_multiply_flags(self):
        for instruction, bits in ((0xe0100192, 32), (0xe0932190, 64)):
            # MULS r0,r2,r1; UMULLS r2,r3,r0,r1.
            for a, b in ((0, 2), (0x80000000, 1), (0xffffffff, 0xffffffff)):
                expected = (a * b) % (1 << bits)
                for batched in (False, True):
                    with self.subTest(instruction=hex(instruction), a=a,
                                      b=b, batched=batched):
                        result = self.run_arm(instruction, {0: a, 1: b, 2: a},
                                              N | Z | C | V, batched)
                        actual = result[0] if bits == 32 else result[2] | result[3] << 32
                        self.assertEqual(actual, expected)
                        self.assertEqual(result[16], 0x10 | C | V | nz(expected, bits))

    def test_arithmetic_carry_and_overflow(self):
        rng = random.Random(12345)
        values = (0, 1, 0x7fffffff, 0x80000000, 0xfffffffe, MASK)
        pairs = [(a, b) for a in values for b in values]
        pairs += [(rng.getrandbits(32), rng.getrandbits(32)) for _ in range(100)]
        for opcode in (2, 3, 4, 5, 6, 7, 10, 11):
            instruction = 0xe0102001 | (opcode << 21)
            for a, b in pairs:
                for carry in (0, 1):
                    left, right = (b, a) if opcode in (3, 7) else (a, b)
                    if opcode in (4, 5, 11):
                        extra = carry if opcode == 5 else 0
                        full = left + right + extra
                        signed_full = signed(left) + signed(right) + extra
                        carry_out = full > MASK
                    else:
                        borrow = 1 - carry if opcode in (6, 7) else 0
                        full = left - right - borrow
                        signed_full = signed(left) - signed(right) - borrow
                        carry_out = full >= 0
                    expected = full & MASK
                    flags = nz(expected) | (C if carry_out else 0)
                    if not -(1 << 31) <= signed_full < (1 << 31):
                        flags |= V
                    for batched in (False, True):
                        with self.subTest(opcode=opcode, a=a, b=b, carry=carry,
                                          batched=batched):
                            result = self.run_arm(instruction, {0: a, 1: b, 2: 42},
                                                  carry * C | N | Z | V, batched)
                            self.assertEqual(result[2], 42 if opcode in (10, 11)
                                             else expected)
                            self.assertEqual(result[16], 0x10 | flags)


if __name__ == "__main__":
    unittest.main()

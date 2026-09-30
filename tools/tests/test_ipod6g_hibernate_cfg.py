#!/usr/bin/env python3
"""Regression cases for conditional ARM paths in the linked-image gate."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ipod6g_hibernate_cfg import UnsafePath, require_boundary


class ControlFlowTest(unittest.TestCase):
    def check(self, body):
        require_boundary(body, ("boundary",), ("callback",), "test")

    def test_backward_safe_join(self):
        self.check("""
100: ea000002 b 110 <test+0x10>
104: eb000000 bl 300 <callback>
108: e12fff1e bx lr
110: eb000000 bl 200 <boundary>
114: eafffffa b 104 <test+0x4>
""")

    def test_conditional_boundary_cannot_cover_skipped_call(self):
        with self.assertRaises(UnsafePath):
            self.check("""
100: 0b000000 bleq 200 <boundary>
104: eb000000 bl 300 <callback>
108: e12fff1e bx lr
""")

    def test_ble_and_bls_are_branches(self):
        for word, op in (("da000000", "ble"), ("9a000000", "bls")):
            with self.assertRaises(UnsafePath):
                self.check(f"""
100: {word} {op} 10c <test+0xc>
104: eb000000 bl 200 <boundary>
108: e12fff1e bx lr
10c: eb000000 bl 300 <callback>
110: e12fff1e bx lr
""")

    def test_conditional_return_keeps_fallthrough(self):
        with self.assertRaises(UnsafePath):
            self.check("""
100: 08bd8010 popeq {r4, pc}
104: eb000000 bl 300 <callback>
108: e12fff1e bx lr
""")

    def test_failed_display_cannot_become_success(self):
        self.check("""
100: e3550000 cmp r5, #0
104: 01a0a005 moveq sl, r5
108: 0a000001 beq 114 <test+0x14>
10c: eb000000 bl 200 <boundary>
110: e3a0a001 mov sl, #1
114: e005600a and r6, r5, sl
118: e21660ff ands r6, r6, #255
11c: 08bd8010 popeq {r4, pc}
120: eb000000 bl 300 <callback>
124: e12fff1e bx lr
""")

    def test_caller_saved_values_are_invalidated(self):
        with self.assertRaises(UnsafePath):
            self.check("""
100: e3a00000 mov r0, #0
104: eb000000 bl 400 <unrelated>
108: e3500000 cmp r0, #0
10c: 08bd8010 popeq {r4, pc}
110: eb000000 bl 300 <callback>
114: e12fff1e bx lr
""")


if __name__ == "__main__":
    unittest.main()

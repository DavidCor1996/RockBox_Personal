from pathlib import Path
import struct

base = Path("tmp/nano3g-source-small-fast-irqon-20260524aj.dfu")
stub = Path("tmp/n3g_postearly_step7_abs.bin").read_bytes()
out = Path("tmp/nano3g-source-small-fast-irqon-postearly-step7-20260526.dfu")
stub_off = 0x5800
branch_off = 0x91f4

data = bytearray(base.read_bytes())
if len(data) != 124832:
    raise SystemExit(f"unexpected dfu size {len(data)}")
if len(stub) > 0x3000:
    raise SystemExit(f"stub too large {len(stub)}")
if any(data[2048 + stub_off + i] for i in range(len(stub))):
    raise SystemExit("stub destination is not empty")

def put32_body(off, value):
    data[2048 + off:2048 + off + 4] = struct.pack("<I", value)

def patch_bytes_body(off, blob):
    data[2048 + off:2048 + off + len(blob)] = blob

def arm_b(src_body, dst_body):
    imm = (dst_body - (src_body + 8)) >> 2
    return 0xEA000000 | (imm & 0x00FFFFFF)

patch_bytes_body(stub_off, stub)
put32_body(branch_off, arm_b(branch_off, stub_off))
out.write_bytes(data)
print(out)
print(f"size={len(data)} stub={len(stub)} off=0x{stub_off:x} branch=0x{arm_b(branch_off, stub_off):08x}")

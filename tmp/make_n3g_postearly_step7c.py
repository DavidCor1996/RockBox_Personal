from pathlib import Path
import struct

base = Path("tmp/nano3g-source-small-fast-irqon-20260524aj.dfu")
stub = Path("tmp/n3g_postearly_step7c_abs.bin").read_bytes()
out = Path("tmp/nano3g-source-small-fast-irqon-postearly-step7c-20260526.dfu")

data = bytearray(base.read_bytes())
if len(data) != 124832:
    raise SystemExit(f"unexpected dfu size {len(data)}")
if len(stub) > (0x91f4 - 0x8e28):
    raise SystemExit(f"stub too large {len(stub)}")

def put32_body(off, value):
    data[2048 + off:2048 + off + 4] = struct.pack("<I", value)

def patch_bytes_body(off, blob):
    data[2048 + off:2048 + off + len(blob)] = blob

def arm_b(src_body, dst_body):
    imm = (dst_body - (src_body + 8)) >> 2
    return 0xEA000000 | (imm & 0x00FFFFFF)

patch_bytes_body(0x8e28, stub)
put32_body(0x91f4, arm_b(0x91f4, 0x8e28))
out.write_bytes(data)
print(out)
print(f"size={len(data)} stub={len(stub)} branch=0x{arm_b(0x91f4, 0x8e28):08x}")

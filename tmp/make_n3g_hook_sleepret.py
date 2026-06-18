from pathlib import Path
import struct

base = Path("tmp/nano3g-source-small-fast-irqon-20260524aj.dfu")
stub = Path("tmp/n3g_early_loader_marker.bin").read_bytes()
out = Path("tmp/nano3g-source-small-fast-irqon-hook-sleepret-marker-20260526.dfu")

data = bytearray(base.read_bytes())
if len(data) != 124832:
    raise SystemExit(f"unexpected dfu size {len(data)}")
if len(stub) > 68:
    raise SystemExit(f"stub too large {len(stub)}")

def put32_body(off, value):
    data[2048 + off:2048 + off + 4] = struct.pack("<I", value)

def patch_bytes_body(off, blob):
    data[2048 + off:2048 + off + len(blob)] = blob

def arm_b(src_body, dst_body):
    imm = (dst_body - (src_body + 8)) >> 2
    if not -(1 << 23) <= imm < (1 << 23):
        raise SystemExit("branch out of range")
    return 0xEA000000 | (imm & 0x00FFFFFF)

# Visible marker strings already referenced by the loader-stub literal pool.
patch_bytes_body(0x17ebe, b"N3G_HOOKED! ")
patch_bytes_body(0x17ecb, b"sleep hook!")

# Put the tiny loader-LCD marker stub in the known spare window.
patch_bytes_body(0x922c, stub)

# Hook the code path that normally clears/prints N3G_SLEEPRET.
put32_body(0x91f4, arm_b(0x91f4, 0x922c))

out.write_bytes(data)
print(out)
print(f"size={len(data)} branch=0x{arm_b(0x91f4, 0x922c):08x} stub={len(stub)}")

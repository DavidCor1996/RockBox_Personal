#!/usr/bin/env python3
"""Verify stock pitch acceptance and its exact horizontal crop encoding."""
import json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ipod6g_stock_vp_probe import (verified_body,probe_format8,make_emulator,
    STACK_BASE,VP_BASE,pack_u32,read_u32,UC_ARM_REG_R0,UC_ARM_REG_R7,
    UC_ARM_REG_R11,UC_ARM_REG_SP,UC_ARM_REG_PC)
body=verified_body(Path(sys.argv[1]).read_bytes())
planes=probe_format8(body,704,480)
assert planes['image_w']==704 and planes['image_h']==480
assert planes['y_stride']==704 and planes['c_stride']==352
# Exact stock auto-crop basic block: (image_width-visible_width)/2 << 4.
# This checks register encoding, not the analog behavior of filter taps.
uc=make_emulator(body);sp=STACK_BASE+0x8000
uc.reg_write(UC_ARM_REG_SP,sp);uc.mem_write(sp+12,pack_u32(704))
uc.reg_write(UC_ARM_REG_R0,640);uc.reg_write(UC_ARM_REG_R7,0)
uc.reg_write(UC_ARM_REG_R11,VP_BASE)
uc.emu_start(0x0815e460,0x0815e478,count=20)
assert uc.reg_read(UC_ARM_REG_PC)==0x0815e478
assert read_u32(uc,VP_BASE+0x44)==512
assert read_u32(uc,VP_BASE+0x48)==0
print(json.dumps({'stock_image_and_planes':planes,'stock_crop_x_register':512,
 'visible_width':640,'guard_per_side':32,'hardware_result':'pending'}))

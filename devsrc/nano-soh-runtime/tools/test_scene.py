#!/usr/bin/env python3
"""Run native ARM scene setup with real external assets and trapped services.

This executes Play_InitScene and room commands, not Play_Init, actors, frames,
GPU submission or physical audio. Unexpected calls stop execution immediately.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import zipfile
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_INVALID
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
    UC_ARM_REG_R3, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_SP,
    UC_ARM_REG_FPEXC, UC_ARM_REG_C1_C0_2)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/scene-tests'
TRAPS, STOP, STACK = 0x1000000, 0x1010000, 0x2000000
ENTRIES = ['test_scene_resource', 'test_room_resource', 'test_scene_cleanup', 'test_audio_sample']


def decode_adpcm(data):
    """Independent scalar oracle; no ARM code and no game mixer calls."""
    codec = data[64]
    size = struct.unpack_from('<I', data, 68)[0]
    payload = data[72:72+size]
    pos = 72 + size + 12
    states = struct.unpack_from('<I', data, pos)[0]
    pos += 4 + states * 2
    order, predictors, n = struct.unpack_from('<III', data, pos)
    assert order == 2 and n == 8*order*predictors
    book = struct.unpack_from('<'+'h'*n, data, pos+12)
    output, cursor = [0]*16, 0
    def signed(v, bits):
        v &= (1 << bits)-1
        return v-(1 << bits) if v & (1 << (bits-1)) else v
    for frame in range(10):
        control = payload[cursor]
        cursor += 1
        shift, predictor = control >> 4, control & 15
        assert predictor < predictors
        table = book[predictor*16:predictor*16+16]
        for half in range(2):
            values = []
            bits = 2 if codec == 3 else 4
            for _ in range(bits):
                byte = payload[cursor]
                cursor += 1
                for bit in range(8-bits, -1, -bits):
                    values.append(signed(signed(byte >> bit, bits) << shift, 16))
            prev1, prev2 = output[-1], output[-2]
            for j in range(8):
                value = table[j]*prev2 + table[8+j]*prev1 + values[j]*2048
                value += sum(table[8+j-k-1]*values[k] for k in range(j))
                value = signed(value, 32) >> 11
                output.append(max(-32768, min(32767, value)))
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack', type=Path)
    parser.add_argument('--archive', type=Path)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    flags = [x.replace('@ROOT@', str(ROOT)) for x in
             json.loads((ROOT/'platform/compile_flags.json').read_text())]
    subprocess.run(['arm-none-eabi-gcc', *flags, '-Wall', '-Wextra', '-Werror',
                    '-c', ROOT/'tools/test_scene_arm.c', '-o', OUT/'harness.o'], check=True)
    imports = {s.split()[-1] for s in (ROOT/'build/unresolved.txt').read_text().splitlines()}
    imports.update(('test_read_file', '_sbrk'))
    libc = {'memset', 'memcpy', 'memchr', 'strcmp', 'strlen', 'strcpy', 'snprintf',
            'fabsf', 'sqrt', 'sqrtf', 'cosf', 'sinf', 'sin', 'atanf', 'floorf',
            'ceilf', 'truncf', 'roundf', 'nearbyintf', 'abs'}
    imports = sorted(n for n in imports if n not in libc and not n.startswith('__aeabi_'))
    traps = {TRAPS+i*4: n for i, n in enumerate(imports)}
    script = OUT/'test.ld'
    script.write_text('\n'.join(f'{n} = {a+1};' for a, n in traps.items()) + '''
SECTIONS {
 . = 0x10000;
 .text : { *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) *(COMMON) }
}
''')
    subprocess.run(['arm-none-eabi-gcc', '-mcpu=cortex-a8', '-mthumb', '-mfpu=neon',
                    '-nostdlib', '-Wl,--gc-sections', '-Wl,--no-fix-cortex-a8',
                    '-Wl,-e,test_scene_resource',
                    *[f'-Wl,--undefined={e}' for e in ENTRIES],
                    OUT/'harness.o', ROOT/'build/oot-core.rel.o',
                    f'-Wl,-T,{script}', '-lm', '-lc', '-lgcc', '-o', OUT/'scene.elf'], check=True)
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    uc.mem_map(0x10000, 8*1024*1024-0x10000)
    uc.mem_map(TRAPS, 0x20000)
    uc.mem_map(STACK, 0x100000)
    symbols = {}
    with (OUT/'scene.elf').open('rb') as f:
        elf = ELFFile(f)
        for segment in elf.iter_segments():
            if segment['p_type'] == 'PT_LOAD':
                uc.mem_write(segment['p_vaddr'], segment.data())
        for sym in elf.get_section_by_name('.symtab').iter_symbols():
            symbols[sym.name] = sym['st_value']
    uc.reg_write(UC_ARM_REG_C1_C0_2, 0xf << 20)
    uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
    errors, calls = [], {}
    def c_string(addr):
        result = bytearray()
        for i in range(256):
            c = uc.mem_read(addr+i, 1)[0]
            if c == 0: return result.decode()
            result.append(c)
        raise ValueError('unterminated target string')
    def hook(cpu, address, size, user):
        if address == STOP:
            cpu.emu_stop()
            return
        if address == (symbols['nano_memory_fail'] & ~1):
            errors.append(('resource/arena failure', *(cpu.reg_read(r) for r in
                           (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2))))
            cpu.emu_stop()
            return
        if address not in traps: return
        name = traps[address]
        calls[name] = calls.get(name, 0)+1
        if name == 'test_read_file':
            filename = c_string(cpu.reg_read(UC_ARM_REG_R1))
            path = args.pack/filename
            assert path.resolve().is_relative_to(args.pack.resolve()) and path.is_file(), path
            data = path.read_bytes()
            assert len(data) <= cpu.reg_read(UC_ARM_REG_R3)
            cpu.mem_write(cpu.reg_read(UC_ARM_REG_R2), data)
            got = struct.unpack('<I', cpu.mem_read(cpu.reg_read(UC_ARM_REG_SP), 4))[0]
            cpu.mem_write(got, struct.pack('<I', len(data)))
            result = 0
        elif name in ('CVarGetInteger', 'CVarGetFloat'):
            # Test configuration uses precisely the call site's default.
            result = cpu.reg_read(UC_ARM_REG_R1)
        elif name in ('osSetIntMask', 'osGetThreadId', 'osSyncPrintf'):
            result = 0
        else:
            errors.append(('unexpected service', name))
            cpu.emu_stop()
            return
        cpu.reg_write(UC_ARM_REG_R0, result)
        cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))
    def trap_read(cpu, access, address, size, value, user):
        errors.append(('unresolved external data', traps.get(address & ~3, hex(address))))
        cpu.emu_stop()
    uc.hook_add(UC_HOOK_CODE, hook, begin=TRAPS, end=STOP)
    uc.hook_add(UC_HOOK_CODE, hook, begin=symbols['nano_memory_fail'] & ~1,
                end=symbols['nano_memory_fail'] & ~1)
    uc.hook_add(UC_HOOK_MEM_READ, trap_read, begin=TRAPS, end=STOP-1)
    def invalid(cpu, access, address, size, value, user):
        pc = cpu.reg_read(UC_ARM_REG_PC)
        near = sorted((addr, name) for name, addr in symbols.items() if addr <= pc)
        print('Invalid memory', access, hex(address), size, 'PC', hex(pc), near[-1], flush=True)
        return False
    uc.hook_add(UC_HOOK_MEM_INVALID, invalid)
    def call(name, argument=0):
        errors.clear()
        uc.reg_write(UC_ARM_REG_SP, STACK+0xff000)
        uc.reg_write(UC_ARM_REG_LR, STOP | 1)
        uc.reg_write(UC_ARM_REG_R0, argument)
        uc.emu_start(symbols[name], STOP, count=150000000)
        assert not errors, (name, errors)
        assert uc.reg_read(UC_ARM_REG_PC) == STOP, 'instruction budget exceeded'
        assert uc.reg_read(UC_ARM_REG_R0) == 0, (name, uc.reg_read(UC_ARM_REG_R0))
    def results():
        return struct.unpack('<24I', uc.mem_read(symbols['scene_results'], 96))
    call('test_scene_resource')
    scene = results()
    print('Kokiri scene initialized:', scene[:11], flush=True)
    rooms = []
    for room in range(scene[0]):
        call('test_room_resource', room)
        rooms.append(results()[11:16])
    call('test_room_resource', 0)
    audio_samples = 0
    if args.archive:
        with zipfile.ZipFile(args.archive) as archive:
            chosen = {}
            for name in sorted(set(archive.namelist())):
                if not name.startswith('audio/samples/'):
                    continue
                data = archive.read(name)
                if len(data) < 72 or struct.unpack_from('<I', data, 4)[0] != 0x4f534d50:
                    continue
                if chosen.get(data[64], 0) >= 8:
                    continue
                uc.mem_write(symbols['sample_name'], name.encode()+b'\0')
                call('test_audio_sample')
                actual = struct.unpack('<176h', uc.mem_read(symbols['sample_decoded'], 352))
                assert list(actual) == decode_adpcm(data), ('ADPCM mismatch', name)
                chosen[data[64]] = chosen.get(data[64], 0)+1
                audio_samples += 1
    call('test_scene_cleanup')
    report = dict(scene_results=scene[:11], room_results=rooms, service_calls=calls,
                  verified_audio_samples=audio_samples,
                  device_execution=False, gameplay_frames=0, runnable_game=False)
    (OUT/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS: actual ARM Play_InitScene, collision setup, room requests and resource cleanup.')


if __name__ == '__main__':
    main()

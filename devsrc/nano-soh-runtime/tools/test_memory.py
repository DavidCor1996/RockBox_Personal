#!/usr/bin/env python3
"""Execute the real ARM heap/scene routines; trap all unimplemented services.

Requires pyelftools and Unicorn 2.1.4 (host verification only). This does not
execute the N64 ROM, implement a renderer, or claim to exercise physical audio.
"""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

from elftools.elf.elffile import ELFFile
try:
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_READ
    from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                                  UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_SP,
                                  UC_ARM_REG_FPEXC, UC_ARM_REG_C1_C0_2)
except ImportError:
    sys.exit('Install Unicorn 2.1.4 in a host test environment (see MEMORY_REWORK.md).')

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/memory-tests'
TRAPS = 0x1000000
STOP = 0x1010000
STACK = 0x2000000
ENTRIES = ['test_audio', 'test_audio_short', 'test_pool', 'test_scene',
           'test_arena_init', 'test_arena_start', 'test_arena_end', 'test_pool_invalid']


def build():
    OUT.mkdir(exist_ok=True, parents=True)
    flags = [x.replace('@ROOT@', str(ROOT)) for x in
             json.loads((ROOT/'platform/compile_flags.json').read_text())]
    host_flags = []
    previous = ''
    for flag in flags:
        if previous == '-include' or flag == '-include' or flag.startswith(('-I', '-D')):
            host_flags.append(flag)
        previous = flag
    subprocess.run(['cc', '-std=gnu11', '-g', '-O1', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections',
                    '-Wno-error=int-conversion', *host_flags,
                    ROOT/'tools/test_memory_guard.c', ROOT/'platform/nano_memory.c',
                    ROOT/'Shipwright/soh/src/code/TwoHeadArena.c',
                    '-Wl,--gc-sections', '-o', OUT/'guard-test'], check=True)
    subprocess.run([OUT/'guard-test'], check=True)
    subprocess.run(['arm-none-eabi-gcc', *flags, '-c', ROOT/'tools/test_memory_arm.c',
                    '-o', OUT/'harness.o'], check=True)
    imports = [s.split()[-1] for s in (ROOT/'build/unresolved.txt').read_text().splitlines()]
    # Test harness provides actual byte copies and the emulated TV mode. libgcc
    # provides real ARM math helpers. Everything else has a unique trap address.
    imports = [n for n in imports if n not in ('memset', 'memcpy', 'osTvType')
               and not n.startswith('__aeabi_') and n not in ('__divsi3', '__udivsi3')]
    traps = {TRAPS + i*4: n for i, n in enumerate(imports)}
    defs = OUT/'traps.ld'
    defs.write_text('\n'.join(f'{n} = {a+1};' for a, n in traps.items()) + '''
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
                    '-Wl,-e,test_audio',
                    *[f'-Wl,--undefined={e}' for e in ENTRIES],
                    str(OUT/'harness.o'), str(ROOT/'build/oot-core.rel.o'),
                    f'-Wl,-T,{defs}', '-lgcc', '-o', str(OUT/'memory.elf')], check=True)
    return traps


def main():
    traps = build()
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    uc.mem_map(0x10000, 8*1024*1024-0x10000)
    uc.mem_map(TRAPS, 0x20000)
    uc.mem_map(STACK, 0x100000)
    symbols, symbol_sizes = {}, {}
    with (OUT/'memory.elf').open('rb') as f:
        elf = ELFFile(f)
        for segment in elf.iter_segments():
            if segment['p_type'] == 'PT_LOAD':
                uc.mem_write(segment['p_vaddr'], segment.data())
        for sym in elf.get_section_by_name('.symtab').iter_symbols():
            symbols[sym.name] = sym['st_value']
            symbol_sizes[sym.name] = sym['st_size']
    uc.reg_write(UC_ARM_REG_C1_C0_2, 0xf << 20)
    uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
    calls, errors, failures = {}, [], []
    # These services have no allocation effects. Unexpected resource loads,
    # output, malloc, or other imports fail the test instead of becoming stubs.
    allowed = {'osAiSetFrequency', 'osWritebackDCache', 'osWritebackDCacheAll',
               'osInvalDCache', 'osSetIntMask', 'osCartRomInit', 'osSyncPrintf',
               'osGetThreadId', 'osGetTime'}

    def hook(cpu, addr, size, data):
        if addr == STOP:
            cpu.emu_stop()
        elif addr == (symbols['nano_memory_fail'] & ~1):
            failures.append(tuple(cpu.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2)))
            cpu.emu_stop()
        elif addr in traps:
            name = traps[addr]
            calls[name] = calls.get(name, 0) + 1
            if name not in allowed:
                errors.append('unexpected service call: '+name)
                cpu.emu_stop()
                return
            if name != 'osAiSetFrequency':
                cpu.reg_write(UC_ARM_REG_R0, 0)
                cpu.reg_write(UC_ARM_REG_R1, 0)
            cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))

    def trap_read(cpu, access, addr, size, value, data):
        errors.append(f'read of unresolved external storage at {addr:#x}: '+
                      traps.get(addr & ~3, 'unknown'))
        cpu.emu_stop()

    uc.hook_add(UC_HOOK_CODE, hook, begin=TRAPS, end=STOP)
    uc.hook_add(UC_HOOK_CODE, hook, begin=symbols['nano_memory_fail'] & ~1,
                end=symbols['nano_memory_fail'] & ~1)
    uc.hook_add(UC_HOOK_MEM_READ, trap_read, begin=TRAPS, end=STOP-1)

    def call(name, a=0, b=0, fail=False):
        errors.clear()
        failures.clear()
        uc.reg_write(UC_ARM_REG_SP, STACK+0xff000)
        uc.reg_write(UC_ARM_REG_LR, STOP | 1)
        uc.reg_write(UC_ARM_REG_R0, a)
        uc.reg_write(UC_ARM_REG_R1, b)
        uc.emu_start(symbols[name], STOP, count=15000000)
        assert not errors, errors
        assert bool(failures) == fail, (name, a, b, failures)
        assert failures or uc.reg_read(UC_ARM_REG_PC) == STOP, 'instruction budget exceeded'
        return uc.reg_read(UC_ARM_REG_R0)

    def words(name, n):
        return struct.unpack('<'+'I'*n, uc.mem_read(symbols[name], n*4))

    results = []
    for rate in (50, 60):
        for ident in range(18):
            assert call('test_audio', ident, rate) == 0, (ident, rate, words('test_results', 8), words('test_plan', 8))
            p = words('test_plan', 8)
            results.append(dict(spec=ident, refresh=rate, notes_bytes=p[0], cache_bytes=p[1],
                                session_bytes=p[2], init_bytes=p[3], permanent_bytes=p[4],
                                heap_bytes=p[5], updates=p[6], dma_count=p[7]))
            # One byte short must reject before modifying any engine context.
            call('test_audio_short', ident, rate, fail=True)
            assert failures[0][0] == 3
            n = symbol_sizes['gAudioContext']
            assert uc.mem_read(symbols['gAudioContext'], n) == uc.mem_read(symbols['test_audio_before'], n)
    for offset in range(16):
        for size in range(33):
            call('test_pool', offset, size)
            start, capacity, got = words('test_results', 3)
            padding = (-offset) & 15
            if size < padding:
                assert (start, capacity, got) == (0, 0, 0)
            else:
                assert start % 16 == 0 and capacity == size-padding
                assert bool(got) == (not padding and ((size+15)&~15) <= size)
    assert call('test_pool_invalid') == 0
    for _ in range(100):
        assert call('test_scene') == 0
    for size in range(1, 65):
        call('test_arena_init', size)
        before = words('test_arena', 4)
        call('test_arena_end', 0xffffffff, fail=True)
        assert words('test_arena', 4) == before
        call('test_arena_end', size, fail=size % 16 != 0)
        call('test_arena_init', size)
        call('test_arena_start', size)
        before = words('test_arena', 4)
        call('test_arena_start', 1, fail=True)
        assert words('test_arena', 4) == before
    report = {'audio_profiles': results, 'max_audio_heap_bytes': max(p['heap_bytes'] for p in results),
              'scene_iterations': 100, 'scene_results': words('test_results', 11)[8:11],
              'system_heap_bytes': 1280*1024, 'scene_arena_bytes': 1024*1024,
              'service_calls': calls, 'device_execution': False, 'runnable_game': False}
    (OUT/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'PASS: 36 actual ARM audio initializations, 36 admission failures, 528 pool alignment cases, '
          f'100 scene allocations and 64 arena boundary cases. Audio heap: {report["max_audio_heap_bytes"]:,} bytes.')


if __name__ == '__main__':
    main()

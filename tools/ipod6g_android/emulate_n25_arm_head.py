#!/usr/bin/env python3
"""Execute the N25 ARM926 Linux head with no iPod hardware attached.

This is a deliberately narrow S5L8702 model.  It proves the raw Image boot
protocol on ARM926, and can additionally verify the N25 Timer B/VIC path and
the exact first-frame PIO transaction used by the diagnostic LCD driver.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from unicorn import Uc, UcError
from unicorn.arm_const import (
    UC_CPU_ARM_926,
    UC_ARM_REG_CPSR,
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_SP,
    UC_ARM_REG_SPSR,
)
from unicorn.unicorn_const import (
    UC_ARCH_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_READ,
    UC_HOOK_MEM_WRITE,
    UC_HOOK_MEM_INVALID,
    UC_MEM_FETCH_UNMAPPED,
    UC_MEM_READ_UNMAPPED,
    UC_MEM_WRITE_UNMAPPED,
    UC_MODE_ARM,
    UC_PROT_ALL,
)


RAM_BASE = 0x08000000
RAM_SIZE = 0x04000000
IO_BASE = 0x38000000
IO_SIZE = 0x08000000
IRAM_BASE = 0x22000000
IRAM_SIZE = 0x00040000
ROCKBOX_DFU_BODY_ADDRESS = 0x22020800
KERNEL_ENTRY = 0x08008000
DTB_ADDRESS = 0x0AD00000
INITRD_ADDRESS = 0x09C00000
PAGE_SIZE = 0x1000
N25_TIMER_E_COUNT = 0x3C7000B4
N25_TIMER_B_CON = 0x3C700020
N25_TIMER_B_CMD = 0x3C700024
N25_TIMER_B_DATA0 = 0x3C700028
N25_TIMER_B_PRE = 0x3C700030
N25_TIMER_B_PERIODIC_WRITES = (
    (N25_TIMER_B_CMD, 2),
    (N25_TIMER_B_PRE, 74),
    (N25_TIMER_B_CON, 0x1240),
    (N25_TIMER_B_DATA0, 100),
    (N25_TIMER_B_CMD, 1),
)
N25_VIC0_BASE = 0x38E00000
N25_VIC1_BASE = 0x38E01000
N25_VIC_EDGE_BASE = 0x38E02000
N25_VIC_IRQ_STATUS = N25_VIC0_BASE
N25_VIC_INT_ENABLE = N25_VIC0_BASE + 0x10
N25_VIC_INT_ENABLE_CLEAR = N25_VIC0_BASE + 0x14
N25_VIC_ADDRESS0 = N25_VIC0_BASE + 0xF00
N25_VIC_ADDRESS1 = N25_VIC1_BASE + 0xF00
N25_TIMER_IRQ = 8
ARM_IRQ_VECTOR = 0xFFFF0018
N25_LCD_BASE = 0x38300000
N25_LCD_CONFIG = N25_LCD_BASE + 0x00
N25_LCD_WCMD = N25_LCD_BASE + 0x04
N25_LCD_STATUS = N25_LCD_BASE + 0x1C
N25_LCD_PHTIME = N25_LCD_BASE + 0x20
N25_LCD_WDATA = N25_LCD_BASE + 0x40
N25_LCD_PANEL_STRAP = 0x3CF000C4
N25_LCD_MODE_P8 = 0x80000C20
N25_LCD_MODE_P16 = 0x80100DB0
N25_LCD_MODE_P18 = 0x80000DA8
N25_LCD_AMBER = 0xFD20
N25_LCD_PIXELS = 320 * 240
N25_LCD_BAND_PIXELS = 320 * 48
N25_LCD_FIRST_FRAME_SYMBOL = "s5l_lcd_n25_first_frame_complete"
N25_DMAC0_BASE = 0x38200000
N25_DMAC0_CONFIG = N25_DMAC0_BASE + 0x30
N25_DMAC0_TC_CLEAR = N25_DMAC0_BASE + 0x08
N25_DMAC0_ERROR_CLEAR = N25_DMAC0_BASE + 0x10
N25_DMAC0_CHANNEL_CONFIGS = tuple(
    N25_DMAC0_BASE + 0x100 + channel * 0x20 + 0x10
    for channel in range(8)
)
N25_HANDOFF_STACK = IRAM_BASE + IRAM_SIZE - 0x1000


def load_symbols(path: Path) -> dict[str, int]:
    symbols: dict[str, int] = {}
    pattern = re.compile(r"^([0-9a-fA-F]+)\s+\S\s+(\S+)$")
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            symbols[match.group(2)] = int(match.group(1), 16)
    return symbols


def load_elf_symbols(path: Path, nm: str) -> dict[str, int]:
    output = subprocess.check_output(
        [nm, "-n", "--defined-only", str(path)], text=True
    )
    symbols: dict[str, int] = {}
    pattern = re.compile(r"^([0-9a-fA-F]+)\s+\S\s+(\S+)$")
    for line in output.splitlines():
        match = pattern.match(line)
        if match:
            symbols[match.group(2)] = int(match.group(1), 16)
    return symbols


def aligned_size(size: int) -> int:
    return (size + PAGE_SIZE - 1) & -PAGE_SIZE


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--dtb", required=True, type=Path)
    parser.add_argument("--system-map", required=True, type=Path)
    parser.add_argument("--loader-bin", type=Path)
    parser.add_argument("--loader-elf", type=Path)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("--initramfs", type=Path)
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--max-instructions", type=int, default=2_000_000)
    parser.add_argument("--stop-at", default="start_kernel")
    parser.add_argument(
        "--trace-symbol",
        action="append",
        default=[],
        help="record an additional System.map symbol without stopping there",
    )
    parser.add_argument(
        "--model-n25-timer",
        action="store_true",
        help="model only the free-running Timer E counter",
    )
    parser.add_argument(
        "--verify-n25-periodic",
        action="store_true",
        help="execute and verify Rockbox's exact 100 Hz Timer B setup",
    )
    parser.add_argument(
        "--verify-n25-irq",
        action="store_true",
        help=(
            "model Timer B IRQ 8 and require the N25 PL192 handshake, "
            "timer acknowledgement, and requested post-calibration milestone"
        ),
    )
    parser.add_argument(
        "--verify-n25-lcd",
        action="store_true",
        help=(
            "model one N25 panel strap and require the complete first-frame "
            "LCD PIO transaction"
        ),
    )
    parser.add_argument(
        "--panel-strap",
        type=int,
        choices=range(4),
        help="N25 PDAT6 panel strap value (0, 1, 2, or 3)",
    )
    args = parser.parse_args()
    if args.verify_n25_periodic and not args.model_n25_timer:
        raise SystemExit("--verify-n25-periodic requires --model-n25-timer")
    if args.verify_n25_periodic and args.stop_at != "s5l8702_clkevt_set_periodic":
        raise SystemExit(
            "--verify-n25-periodic requires "
            "--stop-at s5l8702_clkevt_set_periodic"
        )
    if args.verify_n25_irq and not args.model_n25_timer:
        raise SystemExit("--verify-n25-irq requires --model-n25-timer")
    if args.verify_n25_irq and args.verify_n25_periodic:
        raise SystemExit(
            "--verify-n25-irq executes beyond periodic setup; "
            "do not combine it with --verify-n25-periodic"
        )
    if args.verify_n25_lcd and not args.verify_n25_irq:
        raise SystemExit("--verify-n25-lcd requires --verify-n25-irq")
    if args.verify_n25_lcd and args.panel_strap is None:
        raise SystemExit("--verify-n25-lcd requires --panel-strap")
    if args.panel_strap is not None and not args.verify_n25_lcd:
        raise SystemExit("--panel-strap requires --verify-n25-lcd")
    if args.verify_n25_lcd and args.stop_at != N25_LCD_FIRST_FRAME_SYMBOL:
        raise SystemExit(
            f"--verify-n25-lcd requires --stop-at {N25_LCD_FIRST_FRAME_SYMBOL}"
        )
    if bool(args.loader_bin) != bool(args.loader_elf):
        raise SystemExit("--loader-bin and --loader-elf must be supplied together")

    image = args.image.read_bytes()
    dtb = args.dtb.read_bytes()
    initramfs = args.initramfs.read_bytes() if args.initramfs else b""
    loader = args.loader_bin.read_bytes() if args.loader_bin else b""
    expected_visible_entry = (
        0xE59F3068, 0xE59F4068, 0xE3A05E7E, 0xE3A06B0F,
        0xE5937000, 0xE3170010, 0x1AFFFFFC, 0xE5845000,
        0xE2566001, 0x1AFFFFF9, 0xE321F0D3,
    )
    actual_entry = tuple(
        int.from_bytes(image[offset:offset + 4], "little")
        for offset in range(0, len(expected_visible_entry) * 4, 4)
    )
    zimage_magic = int.from_bytes(image[0x24:0x28], "little")
    zimage_end = int.from_bytes(image[0x2C:0x30], "little")
    if actual_entry == expected_visible_entry:
        image_format = "breadcrumb-enabled-Image"
    elif zimage_magic == 0x016F2818 and zimage_end == len(image):
        image_format = "zImage"
    elif int.from_bytes(image[:4], "little") == 0xE321F0D3:
        image_format = "Image"
    else:
        raise SystemExit("unexpected ARM Image/zImage entry")
    if args.verify_n25_lcd and image_format != "breadcrumb-enabled-Image":
        raise SystemExit("N25 LCD trace gate requires the breadcrumb-enabled ARM Image")
    if dtb[:4] != bytes.fromhex("d00dfeed"):
        raise SystemExit("invalid flattened device tree")
    if KERNEL_ENTRY + len(image) >= INITRD_ADDRESS:
        raise SystemExit("kernel overlaps initramfs staging")
    if initramfs and INITRD_ADDRESS + len(initramfs) >= DTB_ADDRESS:
        raise SystemExit("initramfs overlaps DTB staging")
    if loader and ROCKBOX_DFU_BODY_ADDRESS + len(loader) > IRAM_BASE + IRAM_SIZE:
        raise SystemExit("Rockbox DFU body exceeds the modeled S5L8702 IRAM")

    symbols = load_symbols(args.system_map)
    required = ["stext", "__lookup_processor_type", "__turn_mmu_on",
                "start_kernel", args.stop_at]
    if args.verify_n25_irq:
        required.extend(("s5l8702_clkevt_set_periodic",
                         "s5l8702_timer_interrupt"))
    required = tuple(dict.fromkeys(required))
    missing = [name for name in required if name not in symbols]
    if missing:
        raise SystemExit(f"missing symbols: {', '.join(missing)}")
    unknown_traces = [name for name in args.trace_symbol if name not in symbols]
    if unknown_traces:
        raise SystemExit(f"missing trace symbols: {', '.join(unknown_traces)}")

    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.ctl_set_cpu_model(UC_CPU_ARM_926)
    uc.mem_map(RAM_BASE, RAM_SIZE, UC_PROT_ALL)
    if loader:
        uc.mem_map(IRAM_BASE, IRAM_SIZE, UC_PROT_ALL)
        uc.mem_write(ROCKBOX_DFU_BODY_ADDRESS, loader)
    # Unmodelled MMIO remains zero-filled. Every peripheral used as a
    # qualification milestone has explicit read/write hooks below.
    uc.mem_map(IO_BASE, IO_SIZE, UC_PROT_ALL)
    # Samsung UART UTRSTAT: transmitter empty/ready.  This prevents printk's
    # early console poll loop from becoming the CPU gate's stopping point.
    uc.mem_write(0x3CC00010, (0x6).to_bytes(4, "little"))
    if args.verify_n25_lcd or loader:
        uc.mem_write(N25_LCD_STATUS, (0x2).to_bytes(4, "little"))
        uc.mem_write(
            N25_LCD_PANEL_STRAP,
            ((args.panel_strap or 0) << 4).to_bytes(4, "little"),
        )
    uc.mem_write(KERNEL_ENTRY, image)
    uc.mem_write(DTB_ADDRESS, dtb)
    if initramfs:
        uc.mem_write(INITRD_ADDRESS, initramfs)

    loader_jump = None
    loader_handoff = None
    if loader:
        loader_symbols = load_elf_symbols(args.loader_elf, args.nm)
        loader_jump = loader_symbols.get("n25_android_linux_jump")
        loader_handoff = loader_symbols.get("n25_android_handoff")
        if loader_jump is None:
            raise SystemExit("loader ELF has no n25_android_linux_jump symbol")
        if loader_handoff is None:
            raise SystemExit("loader ELF has no n25_android_handoff symbol")
        for name, address in (
            ("jump", loader_jump), ("handoff", loader_handoff)
        ):
            if not ROCKBOX_DFU_BODY_ADDRESS <= address < (
                ROCKBOX_DFU_BODY_ADDRESS + len(loader)
            ):
                raise SystemExit(
                    f"loader {name} symbol is outside the DFU body"
                )

    reached: list[str] = []
    hit_counts: dict[str, int] = {}
    periodic_entered = False
    instructions = 0
    stop_reason = ""
    next_irq_instruction = 0
    loader_handoff_active = loader_handoff is not None
    loader_handoff_writes: list[tuple[int, int]] = []
    loader_entry_state: dict[str, int] | None = None
    kernel_early_marker_active = False
    wanted: dict[int, str] = {}
    traced = list(required)
    if args.verify_n25_irq:
        traced.extend(
            name for name in (
                "rest_init",
                "kernel_init",
                "kernel_init_freeable",
                "do_one_initcall",
                "of_platform_default_populate_init",
                "s5l_lcd_driver_init",
                "s5l_lcd_probe",
                "platform_driver_register",
                "driver_register",
                "bus_add_driver",
                "driver_attach",
            )
            if name in symbols
        )
    traced.extend(args.trace_symbol)
    for name in dict.fromkeys(traced):
        virtual = symbols[name]
        wanted[virtual] = name
        if 0xC0000000 <= virtual < 0xC4000000:
            wanted[virtual - 0xC0000000 + RAM_BASE] = name
    idle_wait_addresses: set[int] = set()
    idle_wake_addresses: set[int] = set()
    if args.verify_n25_irq and "cpu_arm926_do_idle" in symbols:
        idle_wait = symbols["cpu_arm926_do_idle"] + 0x20
        idle_wake = symbols["cpu_arm926_do_idle"] + 0x24
        idle_wait_addresses.add(idle_wait)
        idle_wake_addresses.add(idle_wake)
        if 0xC0000000 <= idle_wait < 0xC4000000:
            idle_wait_addresses.add(idle_wait - 0xC0000000 + RAM_BASE)
        if 0xC0000000 <= idle_wake < 0xC4000000:
            idle_wake_addresses.add(idle_wake - 0xC0000000 + RAM_BASE)

    def code_hook(emulator: Uc, address: int, size: int, data: object) -> None:
        nonlocal instructions, next_irq_instruction, periodic_entered, stop_reason
        nonlocal timer_irq_pending, loader_handoff_active, loader_entry_state
        nonlocal kernel_early_marker_active
        del size, data
        instructions += 1
        if loader_handoff_active and address == KERNEL_ENTRY:
            loader_entry_state = {
                "r0": emulator.reg_read(UC_ARM_REG_R0),
                "r1": emulator.reg_read(UC_ARM_REG_R1),
                "r2": emulator.reg_read(UC_ARM_REG_R2),
                "cpsr": emulator.reg_read(UC_ARM_REG_CPSR),
            }
            loader_handoff_active = False
            kernel_early_marker_active = True
        name = wanted.get(address)
        if name == "__lookup_processor_type":
            kernel_early_marker_active = False
        if name:
            hit_counts[name] = hit_counts.get(name, 0) + 1
            if name not in reached:
                reached.append(name)
        if name == "s5l8702_clkevt_set_periodic":
            periodic_entered = True
        # The periodic gate must execute the function body and observe its
        # MMIO writes, rather than passing merely by reaching its first
        # instruction.
        if name == args.stop_at and not args.verify_n25_periodic:
            stop_reason = "target"
            emulator.emu_stop()
            return
        if (
            args.verify_n25_irq
            and periodic_verified
            and not timer_irq_pending
            and vic_enabled & (1 << N25_TIMER_IRQ)
            and (
                instructions >= next_irq_instruction
                or address in idle_wait_addresses
            )
        ):
            timer_irq_pending = True
            emulator.mem_write(
                N25_VIC_IRQ_STATUS,
                (1 << N25_TIMER_IRQ).to_bytes(4, "little"),
            )
        if (
            args.verify_n25_irq
            and timer_irq_pending
            and address in idle_wait_addresses
        ):
            # Unicorn does not have an external line capable of releasing
            # ARM926 WFI while CPSR.I is set. Real ARM hardware wakes from WFI
            # on the pending line and services it as the idle path reenables
            # IRQs. Skip the WFI instruction and make that transition explicit
            # before exception injection.
            emulator.reg_write(UC_ARM_REG_PC, address + 4)
            emulator.reg_write(
                UC_ARM_REG_CPSR,
                emulator.reg_read(UC_ARM_REG_CPSR) & ~0x80,
            )
        elif (
            args.verify_n25_irq
            and timer_irq_pending
            and address in idle_wake_addresses
        ):
            emulator.reg_write(
                UC_ARM_REG_CPSR,
                emulator.reg_read(UC_ARM_REG_CPSR) & ~0x80,
            )
        if (
            args.verify_n25_irq
            and timer_irq_pending
            and not (emulator.reg_read(UC_ARM_REG_CPSR) & 0x80)
        ):
            stop_reason = "inject-irq"
            emulator.emu_stop()
            return
        if instructions >= args.max_instructions:
            stop_reason = "budget"
            emulator.emu_stop()

    def invalid_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> bool:
        del emulator, size, value, data
        kind = {
            UC_MEM_READ_UNMAPPED: "read",
            UC_MEM_WRITE_UNMAPPED: "write",
            UC_MEM_FETCH_UNMAPPED: "fetch",
        }.get(access, "access")
        raise RuntimeError(f"unexpected unmapped {kind} at 0x{address:08x}")

    def loader_handoff_write_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> None:
        del emulator, access, data
        if loader_handoff_active and size == 4:
            loader_handoff_writes.append((address, value & 0xFFFFFFFF))

    timer_count = 0
    timer_b_writes: list[tuple[int, int]] = []
    periodic_verified = False
    timer_irq_pending = False
    timer_irq_injections = 0
    timer_irq_acks = 0
    vic_enabled = 0
    vic_address_reads = 0
    vic_address_completions = 0
    vic_edge_writes: set[tuple[int, int]] = set()
    lcd_config_writes: list[int] = []
    lcd_command_writes: list[int] = []
    lcd_setup_data_writes: list[int] = []
    lcd_phtime_writes: list[int] = []
    lcd_frame_active = False
    lcd_pixel_writes = 0
    lcd_pixel_mismatches = 0
    loader_marker_pixels = 0
    loader_marker_mismatches = 0
    kernel_entry_marker_pixels = 0
    kernel_entry_marker_mismatches = 0
    timer_source_marker_pixels = 0
    timer_irq_marker_pixels = 0

    def timer_read_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> None:
        nonlocal timer_count
        del access, size, value, data
        if address == N25_TIMER_E_COUNT:
            timer_count = (timer_count + 1) & 0xFFFFFFFF
            emulator.mem_write(address, timer_count.to_bytes(4, "little"))

    def timer_write_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> None:
        nonlocal next_irq_instruction, periodic_verified, stop_reason
        nonlocal timer_irq_acks, timer_irq_pending
        del access, data
        if timer_irq_pending and address == N25_TIMER_B_CON and size == 4:
            timer_irq_pending = False
            timer_irq_acks += 1
            emulator.mem_write(
                N25_VIC_IRQ_STATUS, (0).to_bytes(4, "little")
            )
            next_irq_instruction = instructions + 20_000
            return
        if not (args.verify_n25_periodic or args.verify_n25_irq) or not periodic_entered:
            return
        if address not in {
            N25_TIMER_B_CON,
            N25_TIMER_B_CMD,
            N25_TIMER_B_DATA0,
            N25_TIMER_B_PRE,
        }:
            return
        if size != 4:
            raise RuntimeError(
                f"unexpected Timer B write size {size} at 0x{address:08x}"
            )
        timer_b_writes.append((address, value & 0xFFFFFFFF))
        expected_index = len(timer_b_writes) - 1
        if expected_index >= len(N25_TIMER_B_PERIODIC_WRITES):
            raise RuntimeError("extra Timer B write during periodic setup")
        expected = N25_TIMER_B_PERIODIC_WRITES[expected_index]
        if timer_b_writes[-1] != expected:
            raise RuntimeError(
                "unexpected Timer B periodic write "
                f"#{expected_index + 1}: got "
                f"0x{address:08x}=0x{value & 0xFFFFFFFF:x}, expected "
                f"0x{expected[0]:08x}=0x{expected[1]:x}"
            )
        if len(timer_b_writes) == len(N25_TIMER_B_PERIODIC_WRITES):
            periodic_verified = True
            if args.verify_n25_periodic:
                stop_reason = "periodic"
                emulator.emu_stop()

    def vic_read_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> None:
        nonlocal vic_address_reads
        del access, size, value, data
        if address == N25_VIC_IRQ_STATUS:
            status = 1 << N25_TIMER_IRQ if timer_irq_pending else 0
            emulator.mem_write(address, status.to_bytes(4, "little"))
        elif address in (N25_VIC_ADDRESS0, N25_VIC_ADDRESS1):
            vic_address_reads += 1

    def vic_write_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> None:
        nonlocal vic_address_completions, vic_enabled
        del emulator, access, data
        if size != 4:
            return
        value &= 0xFFFFFFFF
        if address == N25_VIC_INT_ENABLE:
            vic_enabled |= value
        elif address == N25_VIC_INT_ENABLE_CLEAR:
            vic_enabled &= ~value
        elif address in (N25_VIC_ADDRESS0, N25_VIC_ADDRESS1) and value == 0:
            vic_address_completions += 1
        elif address in (N25_VIC_EDGE_BASE + 0x08, N25_VIC_EDGE_BASE + 0x0C):
            vic_edge_writes.add((address, value))

    def lcd_read_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> None:
        del access, size, value, data
        if address == N25_LCD_STATUS:
            emulator.mem_write(address, (0x2).to_bytes(4, "little"))

    def lcd_write_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> None:
        nonlocal lcd_frame_active, lcd_pixel_mismatches, lcd_pixel_writes
        nonlocal loader_marker_pixels, loader_marker_mismatches
        nonlocal kernel_entry_marker_pixels, kernel_entry_marker_mismatches
        nonlocal timer_source_marker_pixels, timer_irq_marker_pixels
        del emulator, access, data
        if address not in {
            N25_LCD_CONFIG,
            N25_LCD_WCMD,
            N25_LCD_WDATA,
            N25_LCD_PHTIME,
        }:
            return
        if size != 4:
            raise RuntimeError(
                f"unexpected N25 LCD write size {size} at 0x{address:08x}"
            )
        value &= 0xFFFFFFFF
        if address == N25_LCD_CONFIG:
            lcd_config_writes.append(value)
            if value == N25_LCD_MODE_P16:
                if lcd_frame_active:
                    raise RuntimeError("N25 LCD frame mode entered more than once")
                lcd_frame_active = True
        elif address == N25_LCD_WCMD:
            if lcd_frame_active:
                raise RuntimeError("N25 LCD command written after frame start")
            lcd_command_writes.append(value)
        elif address == N25_LCD_WDATA:
            if not lcd_frame_active:
                lcd_setup_data_writes.append(value)
            elif loader_handoff_active:
                loader_marker_pixels += 1
                if value != 0xF800:
                    loader_marker_mismatches += 1
            elif kernel_early_marker_active:
                kernel_entry_marker_pixels += 1
                if value != 0x07E0:
                    kernel_entry_marker_mismatches += 1
            elif (
                value == 0xFFE0
                and timer_source_marker_pixels < N25_LCD_BAND_PIXELS
            ):
                timer_source_marker_pixels += 1
            elif (
                value == 0x001F
                and timer_source_marker_pixels == N25_LCD_BAND_PIXELS
                and timer_irq_marker_pixels < N25_LCD_BAND_PIXELS
            ):
                timer_irq_marker_pixels += 1
            elif lcd_frame_active:
                lcd_pixel_writes += 1
                if value != N25_LCD_AMBER:
                    lcd_pixel_mismatches += 1
                if lcd_pixel_writes > N25_LCD_BAND_PIXELS:
                    raise RuntimeError("N25 LCD amber trace band exceeded 320x48")
        elif address == N25_LCD_PHTIME:
            lcd_phtime_writes.append(value)

    uc.hook_add(UC_HOOK_CODE, code_hook)
    uc.hook_add(UC_HOOK_MEM_INVALID, invalid_hook)
    if loader_handoff is not None:
        uc.hook_add(UC_HOOK_MEM_WRITE, loader_handoff_write_hook)
    if args.model_n25_timer:
        uc.hook_add(UC_HOOK_MEM_READ, timer_read_hook)
    if args.verify_n25_periodic or args.verify_n25_irq:
        uc.hook_add(UC_HOOK_MEM_WRITE, timer_write_hook)
    if args.verify_n25_irq:
        uc.hook_add(UC_HOOK_MEM_READ, vic_read_hook)
        uc.hook_add(UC_HOOK_MEM_WRITE, vic_write_hook)
    if args.verify_n25_lcd or loader_handoff is not None:
        uc.hook_add(UC_HOOK_MEM_READ, lcd_read_hook)
        uc.hook_add(UC_HOOK_MEM_WRITE, lcd_write_hook)
    if loader_handoff is None:
        uc.reg_write(UC_ARM_REG_R0, 0)
        uc.reg_write(UC_ARM_REG_R1, 0xFFFFFFFF)
        uc.reg_write(UC_ARM_REG_R2, DTB_ADDRESS)
        uc.reg_write(UC_ARM_REG_CPSR, 0xD3)
        uc.reg_write(UC_ARM_REG_PC, KERNEL_ENTRY)
    else:
        # Enter the complete linked Rockbox handoff from an intentionally
        # non-SVC, interrupt-enabled state with a real IRAM stack. This must
        # execute DMA/VIC/timer quiescence and the cache transition before the
        # trampoline is allowed to branch to Image.
        uc.reg_write(UC_ARM_REG_R0, 0xA0A0A0A0)
        uc.reg_write(UC_ARM_REG_R1, 0xA1A1A1A1)
        uc.reg_write(UC_ARM_REG_R2, 0xA5A5A5A5)
        uc.reg_write(UC_ARM_REG_CPSR, 0x1F)
        uc.reg_write(UC_ARM_REG_SP, N25_HANDOFF_STACK)
        uc.reg_write(UC_ARM_REG_PC, loader_handoff)

    while instructions < args.max_instructions:
        stop_reason = ""
        start = uc.reg_read(UC_ARM_REG_PC)
        try:
            uc.emu_start(start, RAM_BASE + RAM_SIZE,
                         count=args.max_instructions - instructions)
        except (UcError, RuntimeError) as error:
            pc = uc.reg_read(UC_ARM_REG_PC)
            raise SystemExit(f"ARM head emulation failed at 0x{pc:08x}: {error}")

        if stop_reason == "inject-irq":
            old_pc = uc.reg_read(UC_ARM_REG_PC)
            old_cpsr = uc.reg_read(UC_ARM_REG_CPSR)
            timer_irq_injections += 1
            # ARM926 IRQ exception entry: save CPSR/return address in the
            # banked IRQ registers, enter ARM IRQ mode with IRQs masked, then
            # branch through the high-vector table installed by Linux.
            irq_cpsr = (old_cpsr & ~0x3F) | 0x12 | 0x80
            uc.reg_write(UC_ARM_REG_CPSR, irq_cpsr)
            uc.reg_write(UC_ARM_REG_SPSR, old_cpsr)
            uc.reg_write(UC_ARM_REG_LR, (old_pc + 4) & 0xFFFFFFFF)
            uc.reg_write(UC_ARM_REG_PC, ARM_IRQ_VECTOR)
            continue
        break

    if args.stop_at not in reached:
        pc = uc.reg_read(UC_ARM_REG_PC)
        irq_state = ""
        if args.verify_n25_irq:
            final_cpsr = uc.reg_read(UC_ARM_REG_CPSR)
            irq_state = (
                f"; IRQs injected={timer_irq_injections}, "
                f"acked={timer_irq_acks}, VIC reads={vic_address_reads}, "
                f"completions={vic_address_completions}, "
                f"VIC enabled=0x{vic_enabled:08x}, CPSR=0x{final_cpsr:08x}; "
                f"trace counts={hit_counts}"
            )
        raise SystemExit(
            f"instruction budget exhausted at 0x{pc:08x}; reached {reached}"
            f"{irq_state}"
        )
    if loader_handoff is not None:
        entry_registers_valid = loader_entry_state is not None and {
            key: loader_entry_state[key] for key in ("r0", "r1", "r2")
        } == {
            "r0": 0,
            "r1": 0xFFFFFFFF,
            "r2": DTB_ADDRESS,
        }
        entry_control_valid = (
            loader_entry_state is not None
            and loader_entry_state["cpsr"] & 0xFF == 0xD3
        )
        if not entry_registers_valid or not entry_control_valid:
            raise SystemExit(
                "Rockbox complete handoff produced the wrong Linux entry "
                f"state: {loader_entry_state}"
            )
        required_handoff_writes = {
            (N25_DMAC0_CONFIG, 1),
            (N25_DMAC0_CONFIG, 0),
            (N25_DMAC0_TC_CLEAR, 0xFF),
            (N25_DMAC0_ERROR_CLEAR, 0xFF),
            (N25_VIC0_BASE + 0x14, 0xFFFFFFFF),
            (N25_VIC1_BASE + 0x14, 0xFFFFFFFF),
            (N25_VIC0_BASE + 0x1C, 0xFFFFFFFF),
            (N25_VIC1_BASE + 0x1C, 0xFFFFFFFF),
            (N25_VIC_ADDRESS0, 0xFFFFFFFF),
            (N25_VIC_ADDRESS1, 0xFFFFFFFF),
            (N25_TIMER_B_CMD, 0),
        }
        required_handoff_writes.update(
            (address, 0) for address in N25_DMAC0_CHANNEL_CONFIGS
        )
        missing_handoff_writes = required_handoff_writes.difference(
            loader_handoff_writes
        )
        if missing_handoff_writes:
            raise SystemExit(
                "Rockbox complete handoff missed quiescence writes: "
                f"{sorted(missing_handoff_writes)}"
            )
    if args.verify_n25_periodic and not periodic_verified:
        raise SystemExit(
            "Timer B periodic sequence was incomplete: "
            f"observed {timer_b_writes}"
        )
    if args.verify_n25_irq:
        expected_edges = {
            (N25_VIC_EDGE_BASE + 0x08, 0xFFFFFFFF),
            (N25_VIC_EDGE_BASE + 0x0C, 0xFFFFFFFF),
        }
        if not periodic_verified:
            raise SystemExit("Timer B periodic setup was not verified")
        if timer_irq_injections == 0 or not (
            timer_irq_injections <= timer_irq_acks <= timer_irq_injections + 1
        ):
            raise SystemExit(
                "Timer B IRQ delivery incomplete: "
                f"injected={timer_irq_injections}, acked={timer_irq_acks}"
            )
        if vic_address_reads < timer_irq_injections * 2:
            raise SystemExit(
                "PL192 VICADDRESS entry handshake incomplete: "
                f"reads={vic_address_reads}, IRQs={timer_irq_injections}"
            )
        if vic_address_completions < timer_irq_injections * 2:
            raise SystemExit(
                "PL192 VICADDRESS completion handshake incomplete: "
                f"writes={vic_address_completions}, IRQs={timer_irq_injections}"
            )
        if not expected_edges.issubset(vic_edge_writes):
            raise SystemExit(
                "S5L8702 VIC edge initialization incomplete: "
                f"observed={sorted(vic_edge_writes)}"
            )
    if args.verify_n25_lcd:
        if args.panel_strap < 2:
            expected_configs = [N25_LCD_MODE_P8, N25_LCD_MODE_P16]
            expected_commands = [0x2A, 0x2B, 0x2C]
            expected_setup_data = [0, 0, 1, 0x3F, 0, 0, 0, 0xEF]
        else:
            expected_configs = [N25_LCD_MODE_P18, N25_LCD_MODE_P16]
            expected_commands = [
                0x210, 0x211, 0x212, 0x213, 0x200, 0x201, 0x202
            ]
            expected_setup_data = [0, 319, 0, 239, 0, 0]
        if lcd_phtime_writes != [0x33]:
            raise SystemExit(
                f"N25 LCD phase timing mismatch: {lcd_phtime_writes}"
            )
        if lcd_config_writes != expected_configs:
            raise SystemExit(
                f"N25 LCD config sequence mismatch: {lcd_config_writes}"
            )
        if lcd_command_writes != expected_commands:
            raise SystemExit(
                f"N25 LCD command sequence mismatch: {lcd_command_writes}"
            )
        if lcd_setup_data_writes != expected_setup_data:
            raise SystemExit(
                f"N25 LCD setup data mismatch: {lcd_setup_data_writes}"
            )
        if lcd_pixel_writes != N25_LCD_BAND_PIXELS or lcd_pixel_mismatches:
            raise SystemExit(
                "N25 LCD amber trace band incomplete: "
                f"pixels={lcd_pixel_writes}, mismatches={lcd_pixel_mismatches}"
            )
        if loader_marker_pixels != N25_LCD_BAND_PIXELS or loader_marker_mismatches:
            raise SystemExit(
                "N25 loader trace band incomplete: "
                f"pixels={loader_marker_pixels}, "
                f"mismatches={loader_marker_mismatches}"
            )
        if kernel_entry_marker_pixels != N25_LCD_BAND_PIXELS or (
            kernel_entry_marker_mismatches
        ):
            raise SystemExit(
                "N25 pre-MMU kernel trace band incomplete: "
                f"pixels={kernel_entry_marker_pixels}, "
                f"mismatches={kernel_entry_marker_mismatches}"
            )
        if timer_source_marker_pixels != N25_LCD_BAND_PIXELS:
            raise SystemExit(
                "N25 Timer E trace band incomplete: "
                f"pixels={timer_source_marker_pixels}"
            )
        if timer_irq_marker_pixels != N25_LCD_BAND_PIXELS:
            raise SystemExit(
                "N25 Timer B IRQ trace band incomplete: "
                f"pixels={timer_irq_marker_pixels}"
            )
        trace_pixels = (
            loader_marker_pixels
            + kernel_entry_marker_pixels
            + timer_source_marker_pixels
            + timer_irq_marker_pixels
            + lcd_pixel_writes
        )
        if trace_pixels != N25_LCD_PIXELS:
            raise SystemExit(
                "N25 cumulative trace did not fill exactly one panel frame: "
                f"pixels={trace_pixels}"
            )

    print("ARM926 N25 head gate passed")
    if loader_handoff is not None:
        print(f"Rockbox complete handoff: 0x{loader_handoff:08x}")
        print(f"Rockbox Linux trampoline: 0x{loader_jump:08x}")
    print(f"entry: 0x{KERNEL_ENTRY:08x}")
    print(f"dtb: 0x{DTB_ADDRESS:08x}")
    print(f"milestones: {', '.join(reached)}")
    if args.model_n25_timer:
        print(f"modeled Timer E reads: {timer_count}")
    if args.verify_n25_periodic:
        formatted = ", ".join(
            f"0x{address:08x}=0x{value:x}"
            for address, value in timer_b_writes
        )
        print(f"verified Timer B periodic writes: {formatted}")
    if args.verify_n25_irq:
        print(
            "verified N25 Timer B IRQ delivery: "
            f"injected={timer_irq_injections}, acked={timer_irq_acks}, "
            f"VICADDRESS reads={vic_address_reads}, "
            f"completions={vic_address_completions}"
        )
    if args.verify_n25_lcd:
        print(
            "verified N25 cumulative LCD trace: "
            f"strap={args.panel_strap}, pixels={lcd_pixel_writes}, "
            f"RGB565=0x{N25_LCD_AMBER:04x}"
        )
        print(
            "verified N25 persistent trace bands: "
            f"loader-red={loader_marker_pixels}, "
            f"kernel-green={kernel_entry_marker_pixels}, "
            f"timer-yellow={timer_source_marker_pixels}, "
            f"irq-blue={timer_irq_marker_pixels}"
        )
    if args.json_output:
        report = {
            "gate_passed": True,
            "cpu": "ARM926EJ-S",
            "image_format": image_format,
            "image_sha256": sha256(args.image),
            "dtb_sha256": sha256(args.dtb),
            "initramfs_sha256": sha256(args.initramfs) if args.initramfs else None,
            "rockbox_bootloader_sha256": (
                sha256(args.loader_bin) if args.loader_bin else None
            ),
            "stop_at": args.stop_at,
            "model_n25_timer": args.model_n25_timer,
            "verify_n25_periodic": args.verify_n25_periodic,
            "verify_n25_irq": args.verify_n25_irq,
            "verify_n25_lcd": args.verify_n25_lcd,
            "panel_strap": args.panel_strap,
            "rockbox_handoff_executed": loader_handoff is not None,
            "rockbox_handoff_address": (
                f"0x{loader_handoff:08x}"
                if loader_handoff is not None else None
            ),
            "rockbox_trampoline_address": (
                f"0x{loader_jump:08x}" if loader_jump is not None else None
            ),
            "rockbox_linux_entry_state": loader_entry_state,
            "rockbox_handoff_mmio_write_count": len(loader_handoff_writes),
            "milestones": reached,
            "instructions": instructions,
            "timer_e_reads": timer_count,
            "timer_b_periodic_writes": [
                {"address": f"0x{address:08x}", "value": value}
                for address, value in timer_b_writes
            ],
            "timer_irq_injections": timer_irq_injections,
            "timer_irq_acks": timer_irq_acks,
            "vic_address_reads": vic_address_reads,
            "vic_address_completions": vic_address_completions,
            "lcd_config_writes": [
                f"0x{value:08x}" for value in lcd_config_writes
            ],
            "lcd_command_writes": [
                f"0x{value:04x}" for value in lcd_command_writes
            ],
            "lcd_setup_data_writes": lcd_setup_data_writes,
            "lcd_phtime_writes": lcd_phtime_writes,
            "lcd_pixel_writes": lcd_pixel_writes,
            "lcd_pixel_mismatches": lcd_pixel_mismatches,
            "lcd_pixel_rgb565": f"0x{N25_LCD_AMBER:04x}",
            "lcd_trace_band_pixels": N25_LCD_BAND_PIXELS,
            "lcd_trace_total_pixels": (
                loader_marker_pixels
                + kernel_entry_marker_pixels
                + timer_source_marker_pixels
                + timer_irq_marker_pixels
                + lcd_pixel_writes
            ),
            "loader_marker_pixels": loader_marker_pixels,
            "loader_marker_mismatches": loader_marker_mismatches,
            "loader_marker_rgb565": "0xf800",
            "kernel_entry_marker_pixels": kernel_entry_marker_pixels,
            "kernel_entry_marker_mismatches": kernel_entry_marker_mismatches,
            "kernel_entry_marker_rgb565": "0x07e0",
            "timer_source_marker_pixels": timer_source_marker_pixels,
            "timer_source_marker_rgb565": "0xffe0",
            "timer_irq_marker_pixels": timer_irq_marker_pixels,
            "timer_irq_marker_rgb565": "0x001f",
            "artifacts": {
                "image": sha256(args.image),
                "dtb": sha256(args.dtb),
                "system_map": sha256(args.system_map),
                "initramfs": sha256(args.initramfs) if args.initramfs else None,
                "loader_bin": sha256(args.loader_bin) if args.loader_bin else None,
                "loader_elf": sha256(args.loader_elf) if args.loader_elf else None,
            },
        }
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

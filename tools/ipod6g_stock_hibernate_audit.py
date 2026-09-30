#!/usr/bin/env python3
"""Verify the iPod Classic 6G/7G RetailOS 2.0.4 hibernate contract.

The input is either the complete decrypted ``osos.fw.decrypted`` image or its
body with the 0x800-byte firmware header removed.  This is a read-only audit:
it checks the known image identity, applies the stock relocator's IRAM/DRAM
layout, and verifies the instruction-level behaviors Rockbox relies on.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from dataclasses import dataclass
from pathlib import Path


OSOS_SHA256 = "f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4"
OSOS_SIZE = 10_599_920
OSOS_HEADER_SIZE = 0x800


class AuditError(RuntimeError):
    pass


@dataclass(frozen=True)
class Segment:
    body_offset: int
    address: int
    size: int


SEGMENTS = (
    Segment(0x000000, 0x22000000, 0x00AED8),
    Segment(0x00AED8, 0x08000000, 0xA0FC88),
    Segment(0xA1AB60, 0x08A0FC88, 0x000A84),
)


def body_offset(address: int) -> int:
    for segment in SEGMENTS:
        relative = address - segment.address
        if 0 <= relative <= segment.size - 4:
            return segment.body_offset + relative
    raise AuditError(f"address is outside the RetailOS load map: 0x{address:08x}")


def word(body: bytes, address: int) -> int:
    return struct.unpack_from("<I", body, body_offset(address))[0]


def require_word(body: bytes, address: int, expected: int, purpose: str) -> None:
    observed = word(body, address)
    if observed != expected:
        raise AuditError(
            f"{purpose}: 0x{address:08x} is 0x{observed:08x}, "
            f"expected 0x{expected:08x}"
        )


def require_bytes(body: bytes, address: int, expected: bytes, purpose: str) -> None:
    offset = body_offset(address)
    observed = body[offset:offset + len(expected)]
    if observed != expected:
        raise AuditError(
            f"{purpose}: bytes at 0x{address:08x} do not match the "
            "RetailOS 2.0.4 record"
        )


def branch_target(body: bytes, address: int) -> int:
    instruction = word(body, address)
    condition = instruction >> 28
    if condition != 0xF and (instruction & 0x0E000000) == 0x0A000000:
        displacement = (instruction & 0x00FFFFFF) << 2
        if displacement & 0x02000000:
            displacement -= 0x04000000
    elif (instruction & 0xFE000000) == 0xFA000000:
        displacement = ((instruction & 0x00FFFFFF) << 2) | (
            (instruction >> 23) & 0x2
        )
        if displacement & 0x02000000:
            displacement -= 0x04000000
    else:
        raise AuditError(f"0x{address:08x} is not an ARM immediate branch")
    return (address + 8 + displacement) & 0xFFFFFFFF


def require_branch(body: bytes, address: int, target: int, purpose: str) -> None:
    observed = branch_target(body, address)
    if observed != target:
        raise AuditError(
            f"{purpose}: branch at 0x{address:08x} targets "
            f"0x{observed:08x}, expected 0x{target:08x}"
        )


def require_no_branch(
    body: bytes, start: int, stop: int, target: int, purpose: str
) -> None:
    for address in range(start, stop, 4):
        try:
            observed = branch_target(body, address)
        except AuditError:
            continue
        if observed == target:
            raise AuditError(
                f"{purpose}: unexpected branch at 0x{address:08x} to "
                f"0x{target:08x}"
            )


def audit(body: bytes) -> None:
    expected_size = OSOS_SIZE - OSOS_HEADER_SIZE
    required_size = max(s.body_offset + s.size for s in SEGMENTS)
    if len(body) != expected_size:
        raise AuditError(
            f"RetailOS body is 0x{len(body):x} bytes, expected 0x{expected_size:x}"
        )
    if len(body) < required_size:
        raise AuditError("RetailOS body does not contain every loaded segment")

    # The click-wheel layer publishes distinct down, up, press-and-hold, and
    # long-press-and-hold phases. The global action resource binds ordinary
    # Play/Pause to up and retained poweroff to long-press-and-hold; down is
    # recognizer input only. This is the primary proof that deep sleep must not
    # synthesize Pause/Play or let the long gesture mutate the current screen.
    for address, expected, purpose in (
        (0x083E2DA8, 0x6B44776E, "click-wheel kDwn selector"),
        (0x083E2DAC, 0x08105A9C, "click-wheel down handler"),
        (0x083E2DB4, 0x6B417574, "click-wheel kAut selector"),
        (0x083E2DB8, 0x08105DA4, "click-wheel up handler"),
        (0x083E2DE4, 0x6B502648, "click-wheel kP&H selector"),
        (0x083E2DE8, 0x08105E3C, "click-wheel press-hold handler"),
        (0x083E2DF0, 0x6B512648, "click-wheel kQ&H selector"),
        (0x083E2DF4, 0x08105E3C, "click-wheel long-hold handler"),
        (0x08105AB4, 0xE3A01006, "semantic input down type 6"),
        (0x08105DBC, 0xE3A01007, "semantic input up type 7"),
        (0x08105E64, 0xE3A01008, "semantic press-hold type 8"),
        (0x08105E78, 0xE3A01009, "semantic long-hold type 9"),
    ):
        require_word(body, address, expected, purpose)
    for address in (0x08105AC4, 0x08105DCC, 0x08105E88):
        require_branch(body, address, 0x08105F74,
                       "semantic click-wheel event router")

    action = b"button.playpause.longpressandhold1"
    handler = b"HandlePlayPausePressAndHold"
    require_bytes(
        body, 0x08418092,
        struct.pack("<I", 0x21) + action +
        struct.pack("<I", len(handler)) + handler,
        "long-Play action binding",
    )
    require_bytes(body, 0x081C3240, b"HandlePlayPause\0",
                  "normal Play/Pause handler name")
    require_bytes(body, 0x081C3254, b"HandlePlayPausePressAndHold\0",
                  "long-Play handler name")
    require_branch(body, 0x081C30C8, 0x081C4778,
                   "normal Play/Pause transport dispatch")
    require_branch(body, 0x081C30EC, 0x0819A628,
                   "long-Play UI singleton dispatch")
    require_branch(body, 0x081C30F0, 0x0819AB50,
                   "long-Play deep-sleep event dispatch")
    require_word(body, 0x0819AB50, 0xE3A0100F,
                 "long-Play deep-sleep event 15")

    # Device-manager mode 3 does not synchronously query the five-button
    # bitmap. Retail instead cold-initializes the wheel, recovers its private
    # 0x63a state, and gates later raw edges by power state. Pin the exclusive
    # mode-0 query route and the fact that the complete image has no hidden
    # post-mode3 0x23a call. Rockbox's post-mode3 query is explicitly an
    # architectural adaptation, not a claimed literal Apple call.
    button_query = struct.pack("<I", 0x8000023A)
    if body.count(button_query) != 2:
        raise AuditError("unexpected RetailOS five-button query literal count")
    require_word(body, 0x0805F690, 0x8000023A,
                 "async five-button response identity")
    require_word(body, 0x08362B3C, 0x8000023A,
                 "synchronous five-button query identity")
    require_branch(body, 0x08362C84, 0x08362A9C,
                   "mode-0 five-button query route")
    require_word(body, 0x08362CC4, 0xE3A00019,
                 "mode-3 25 ms delay argument")
    require_branch(body, 0x08362CCC, 0x0806DBA0,
                   "mode-3 full wheel initialization")
    require_branch(body, 0x08362CD4, 0x08362B44,
                   "mode-3 private state recovery")
    require_word(body, 0x0835EC38, 0xE3A00003,
                 "device-manager wheel mode 3")
    require_branch(body, 0x0835EC3C, 0x08362C58,
                   "device-manager wheel wake dispatch")

    # The UI's deep-sleep action enters a broad power-state transaction, not
    # the retained coordinator directly.  That transaction quiesces services,
    # saves/masks IRQ+FIQ through IRAM, calls the small coordinator, restores
    # the old interrupt mask, and then restarts the outer services.
    require_word(body, 0x0819B2E0, 0xE3A00001, "UI deep-sleep mode")
    require_branch(body, 0x0819B2E4, 0x0805D024, "UI power-state wrapper")

    # The UI asks TPodMediaPlayer to enter mode 1 before the retained power
    # wrapper and invokes the paired post-wake method after it returns.  The
    # exact primary vtable entries bind those virtual calls to the functions
    # audited below without assigning guessed names to stripped symbols.
    require_branch(body, 0x0819B2AC, 0x08171FDC,
                   "UI media-player singleton")
    require_word(body, 0x08989244, 0x0816F900,
                 "TPodMediaPlayer pre-sleep vmethod")
    require_branch(body, 0x0819B314, 0x08171FDC,
                   "UI wake media-player singleton")
    require_word(body, 0x08989248, 0x0816F9FC,
                 "TPodMediaPlayer post-wake vmethod")

    # The registry surrounding the media calls is a real synchronous service
    # fanout: +0x1c calls each child vmethod +0x0c, +0x20 calls +0x10, and
    # +0x28 stores the requested mode byte.  Pin the stripped primary vtable
    # and the indirect child dispatches without inventing class names.
    for address, expected, purpose in (
        (0x089A8018, 0x08250DCC, "registry pre-sleep fanout vmethod"),
        (0x089A801C, 0x08250D50, "registry post-wake fanout vmethod"),
        (0x089A8024, 0x08250C0C, "registry mode vmethod"),
        (0x08250C0C, 0xE5C0103D, "registry mode-byte store"),
        (0x08250DFC, 0xE5901000, "registry pre child vtable load"),
        (0x08250E00, 0xE591100C, "registry pre child +0x0c load"),
        (0x08250E08, 0xE0004004, "registry pre result accumulation"),
        (0x08250D80, 0xE5901000, "registry post child vtable load"),
        (0x08250D84, 0xE5911010, "registry post child +0x10 load"),
        (0x08250D8C, 0xE0004004, "registry post result accumulation"),
    ):
        require_word(body, address, expected, purpose)

    # The pre-sleep vmethod posts opcode 8 to the media worker and then waits
    # for both queue completion and the worker's completion byte.  Crucially,
    # this is an acknowledgement loop with a 100 ms polling interval and no
    # timeout; it is not a test for raw audio-DMA inactivity.  The wake method
    # posts the paired opcode 9 and uses the same completion wait.
    for address, target, purpose in (
        (0x0816F910, 0x081D3E3C, "pre-sleep media worker singleton"),
        (0x0816F918, 0x081D4184, "pre-sleep media opcode post"),
        (0x0816F91C, 0x081D3E3C, "pre-sleep media wait singleton"),
        (0x0816F924, 0x081D3E6C, "pre-sleep media completion wait"),
        (0x0816FA20, 0x081D3E3C, "post-wake media worker singleton"),
        (0x0816FA24, 0x081D41C4, "post-wake media opcode post"),
        (0x0816FA28, 0x081D3E3C, "post-wake media wait singleton"),
        (0x0816FA30, 0x081D3E6C, "post-wake media completion wait"),
        (0x081D3E7C, 0x080DEFD8, "media completion polling delay"),
        (0x081D3E84, 0x08393768, "media worker queue completion test"),
        (0x081D3E94, 0x081D3E78, "media completion wait back-edge"),
        (0x081D4194, 0x0829FEFC, "media opcode allocation"),
        (0x081D41AC, 0x08393720, "media opcode queue post"),
        (0x081D41B4, 0x081D3E9C, "media worker start"),
        (0x081D41D0, 0x0829FEFC, "wake media opcode allocation"),
        (0x081D41E4, 0x08393720, "wake media opcode queue post"),
        (0x081D41EC, 0x081D3E9C, "wake media worker start"),
        (0x081D40C8, 0x081A5CCC, "media service pre-sleep dispatch"),
        (0x081D40DC, 0x081A5D20, "media service post-wake dispatch"),
        (0x081D4110, 0x081A5FB4, "media transport opcode 0 dispatch"),
        (0x081D4124, 0x081A5FB4, "media transport opcode 1 dispatch"),
    ):
        require_branch(body, address, target, purpose)
    for address, expected, purpose in (
        (0x081D3E78, 0xE3A00064, "media wait 100 ms interval"),
        (0x081D3E88, 0xE3500000, "media queue completion compare"),
        (0x081D3E8C, 0x15D40026, "media completion-byte load"),
        (0x081D3E90, 0x13500000, "media completion-byte compare"),
        (0x081D418C, 0xE3A0000C, "media opcode message size"),
        (0x081D419C, 0xE3A00008, "media pre-sleep opcode"),
        (0x081D41A0, 0xE5810000, "media opcode store"),
        (0x081D41CC, 0xE3A0000C, "wake media opcode message size"),
        (0x081D41D8, 0xE3A00009, "media post-wake opcode"),
        (0x081D41DC, 0xE5810000, "wake media opcode store"),
        (0x081D3FA4, 0xE3510009, "media worker opcode bound"),
        (0x081D4068, 0xE3A01001, "pre-sleep service dispatch flag"),
        (0x081D407C, 0xE3A01001, "post-wake service dispatch flag"),
    ):
        require_word(body, address, expected, purpose)

    # Follow opcode 8/9 through the DRAM veneers into the IRAM output-service
    # pair.  The selected backend's +0x10 method is used for suspend and its
    # +0x0c method for resume.  The primary backend vtable resolves those
    # slots to the concrete synchronous queue/hardware routines below.
    require_branch(body, 0x081A5D7C, 0x0802D260,
                   "media opcode 8 output-service tail")
    require_word(body, 0x0802D260, 0xE51FF004,
                 "output-service suspend veneer")
    require_word(body, 0x0802D264, 0x22007F88,
                 "output-service suspend IRAM target")
    require_branch(body, 0x081A5DDC, 0x0802D270,
                   "media opcode 9 output-service tail")
    require_word(body, 0x0802D270, 0xE51FF004,
                 "output-service resume veneer")
    require_word(body, 0x0802D274, 0x22007FCC,
                 "output-service resume IRAM target")
    for address, expected, purpose in (
        (0x22007F90, 0xEBFFEE5E, "output backend selector call"),
        (0x22007F98, 0xE5911010, "output suspend backend +0x10 load"),
        (0x22007F9C, 0xE12FFF31, "output suspend backend call"),
        (0x22007FD0, 0xEBFFEE4E, "wake output backend selector call"),
        (0x22007FD8, 0xE591100C, "output resume backend +0x0c load"),
        (0x22007FE0, 0xE12FFF11, "output resume backend tail call"),
        (0x22003910, 0xE51FF004, "output backend selector veneer"),
        (0x22003914, 0x0818C740, "output backend selector target"),
        (0x089901AC, 0x081E99BC, "primary backend resume vmethod"),
        (0x089901B0, 0x081E9904, "primary backend suspend vmethod"),
        (0x081E9934, 0xEBFC0E47, "primary output queue 0/1 stop"),
        (0x081E9948, 0xE3A010C8, "primary output drain wait 200"),
        (0x081E9958, 0xEBFC0E4A, "primary output disable"),
        (0x081E9A10, 0xE594002C, "primary output handle restore"),
        (0x081E9A14, 0xEBFC0DD3, "primary output reinitialize"),
    ):
        require_word(body, address, expected, purpose)

    # The concrete backend carries one service lock across Standby. Suspend
    # takes object +0x3c through the lock wrapper and returns without calling
    # its paired unlock wrapper. Resume rebuilds output first and releases
    # that exact object lock only at its end. This cross-boundary lifetime is
    # part of the opcode-8/opcode-9 contract, not two balanced critical
    # sections on opposite sides of Standby.
    for address, target, purpose in (
        (0x081E9910, 0x080CC22C, "primary output cross-Standby lock"),
        (0x080CC22C, 0x080746EC, "output lock wrapper"),
        (0x081E9A68, 0x080CC234, "primary output paired unlock"),
        (0x080CC234, 0x080747C8, "output unlock wrapper"),
    ):
        require_branch(body, address, target, purpose)
    require_word(body, 0x081E990C, 0xE280003C,
                 "primary output lock-object address")
    require_word(body, 0x081E99B8, 0xE8BD8070,
                 "primary output suspend return with lock held")
    require_word(body, 0x081E9A64, 0xE284003C,
                 "primary output paired lock-object address")
    require_word(body, 0x081E9A70, 0xE8BD8038,
                 "primary output resume return after unlock")
    require_no_branch(
        body, 0x081E9904, 0x081E99BC, 0x080CC234,
        "primary output suspend must carry its service lock",
    )
    require_branch(body, 0x0805D0D4, 0x0802D000,
                   "outer interrupt save/mask thunk")
    require_branch(body, 0x0805D118, 0x0807B270,
                   "outer retained coordinator call")
    require_branch(body, 0x0805D148, 0x0802D028,
                   "outer interrupt restore thunk")

    # RetailOS does not issue ATA commands directly from its disk-power
    # choreography.  DiskMgrTask tracks the aggregate client state, and the
    # PCF power worker serializes state 1 (wake) and state 2 (sleep) through
    # the disk manager.  At the ATA layer, both FLUSH CACHE (E7) and STANDBY
    # IMMEDIATE (E0) enter the same request routine, which rejects a missing
    # or inactive device object before register access.  This is the primary
    # instruction boundary for repeat sleep: an already-invalid controller is
    # a state-machine no-op, not a second unguarded command transaction.
    for address, expected, purpose in (
        (0x08388190, 0xE3500004, "PCF disk-power message bound"),
        (0x083881C0, 0xE1A00005, "PCF disk wake manager argument"),
        (0x083881CC, 0xE1A00005, "PCF disk sleep manager argument"),
        (0x08143010, 0xE5911018, "disk wake underlying +0x18 method"),
        (0x08142DBC, 0xE591101C, "disk sleep underlying +0x1c method"),
        (0x089CAB0C, 0x08360EFC, "ATA FLUSH CACHE vmethod"),
        (0x089CAB10, 0x08361ECC, "ATA STANDBY IMMEDIATE vmethod"),
        (0x08360F20, 0xE3A010E7, "ATA FLUSH CACHE command"),
        (0x08361EF4, 0xE3A060E0, "ATA STANDBY IMMEDIATE command"),
        (0x08360FB8, 0xE1B04000, "ATA request null-object guard"),
        (0x08360FC4, 0xE5940000, "ATA request object-state load"),
        (0x08360FCC, 0xE1500001, "ATA request object-state compare"),
        (0x08360FD4, 0xE5940044, "ATA request live-device load"),
        (0x08360FD8, 0xE3500000, "ATA request live-device compare"),
        (0x08360FE4, 0xE3A00007, "ATA invalid-device return"),
    ):
        require_word(body, address, expected, purpose)
    for address, target, purpose in (
        (0x083881C4, 0x08142FF4, "PCF disk wake dispatch"),
        (0x083881D0, 0x08142D98, "PCF disk sleep dispatch"),
        (0x08360F80, 0x08360FB4, "ATA flush guarded request"),
        (0x08360F9C, 0x08360FB4, "ATA direct flush guarded request"),
        (0x08361F50, 0x08360FB4, "ATA standby guarded request"),
        (0x08361F90, 0x08360FB4, "ATA direct standby guarded request"),
    ):
        require_branch(body, address, target, purpose)

    # The outer coordinator issues one synchronous type-4 force-off request.
    # DiskMgr converts it to worker state 2, waits for the acknowledgement,
    # records the aggregate inactive state, and does not enqueue state 1 on
    # the ordered retained-return path. The physical vtable confirms the
    # concrete sleep/wake methods reached by worker states 2/1.
    for address, expected, purpose in (
        (0x0803D0EC, 0xE3A00004, "DiskMgr force-off request type 4"),
        (0x083883A0, 0xE3A00002, "DiskMgr force-off worker state 2"),
        (0x08388438, 0xE5876004, "DiskMgr aggregate inactive store"),
        (0x08388498, 0xE35A0000, "DiskMgr later client activity test"),
        (0x0838849C, 0x03A00002, "DiskMgr inactive client state 2"),
        (0x083884A0, 0x13A00001, "DiskMgr active client state 1"),
        (0x089A6108, 0x0826E68C, "physical disk wake vmethod +0x18"),
        (0x089A610C, 0x082703D0, "physical disk sleep vmethod +0x1c"),
        (0x0826E844, 0xE3A00001, "physical disk wake completion"),
    ):
        require_word(body, address, expected, purpose)
    for address, target, purpose in (
        (0x0803D114, 0x0802CF40, "synchronous DiskMgr force-off send"),
        (0x083883D0, 0x0802CF40, "synchronous worker state-2 send"),
        (0x0803D1A4, 0x0803D128, "later client active request"),
        (0x083884D0, 0x0802CF40, "synchronous later worker send"),
        (0x0826E6D4, 0x081146D0, "physical disk inner wake setup"),
    ):
        require_branch(body, address, target, purpose)

    # State 12 runs the retained coordinator, performs its ordered post-wake
    # service/display fanout, then queues event 17. No direct DiskMgr state-1
    # request or physical wake occurs before that UI event. State 0 consumes
    # the transition by destroying its transient object and rearming timers.
    require_branch(body, 0x0819B2E4, 0x0805D024,
                   "state-12 retained coordinator")
    require_word(body, 0x0819B36C, 0x03A01011,
                 "state-12 normal wake event 17")
    require_word(body, 0x0819B370, 0x13A01009,
                 "state-12 alternate wake event 9")
    require_branch(body, 0x0819B37C, 0x081D3398,
                   "state-12 wake event queue")
    for start, stop, owner in (
        (0x0805D024, 0x0805D1A4, "retained coordinator"),
        (0x0819B254, 0x0819B380, "state-12 wake handler"),
    ):
        for target in (0x0803D128, 0x0803D18C, 0x08142FF4, 0x0826E68C):
            require_no_branch(
                body, start, stop, target,
                f"{owner} must leave disk wake demand-driven",
            )
    for address, expected, purpose in (
        (0x0819ABB0, 0xE58400C8, "state-0 transient object clear"),
        (0x0819ABB8, 0xE5C40068, "state-0 active flag +0x68"),
        (0x0819ABBC, 0xE5C40069, "state-0 active flag +0x69"),
        (0x0819ABC0, 0xE5C4006A, "state-0 active flag +0x6a"),
    ):
        require_word(body, address, expected, purpose)
    require_branch(body, 0x0819ABC8, 0x081217D8,
                   "state-0 primary timer cancel")
    require_branch(body, 0x0819ABD0, 0x081217D8,
                   "state-0 secondary timer cancel")

    # Pin the complete direct-call skeleton of the outer transaction.  Several
    # targets are C++ service/virtual-dispatch helpers whose public names are
    # unavailable, so the audit deliberately records addresses rather than
    # guessing labels.  This still proves that the pre-entry and post-wake
    # service choreography surrounding the retained coordinator has not been
    # reduced to the three previously mapped calls.
    for address, target, purpose in (
        (0x0805D040, 0x08143E08, "outer pre-service 01"),
        (0x0805D044, 0x08143684, "outer pre-service 02"),
        (0x0805D048, 0x0815A648, "outer pre-service 03"),
        (0x0805D064, 0x080C6DD4, "outer conditional pre-service"),
        (0x0805D06C, 0x0815A648, "outer alternate pre-service"),
        (0x0805D088, 0x080609A8, "outer pre-service 04"),
        (0x0805D090, 0x0802CF30, "outer metric begin 21"),
        (0x0805D098, 0x0838D894, "outer pre-service 05"),
        (0x0805D0A0, 0x0802CF38, "outer metric end 21"),
        (0x0805D0A4, 0x0804AFB4, "outer pre-service 06"),
        (0x0805D0AC, 0x08156128, "outer pre-service 07"),
        (0x0805D0B0, 0x0803D0D0, "outer pre-service 08"),
        (0x0805D0B4, 0x0805F5D8, "deep-sleep metric begin wrapper"),
        (0x0805D0B8, 0x0805F5C8, "nested metric begin wrapper"),
        (0x0805D0C0, 0x0802CFF0, "outer state capture"),
        (0x0805D0D0, 0x0802CFF8, "outer state transition"),
        (0x0805D0DC, 0x0802D008, "outer masked service 01"),
        (0x0805D0E0, 0x0802D010, "outer masked service 02"),
        (0x0805D0E4, 0x08148514, "outer masked service 03"),
        (0x0805D0EC, 0x0814865C, "outer masked service 04"),
        (0x0805D104, 0x0802D018, "outer masked service 05"),
        (0x0805D130, 0x082B1624, "outer conditional wake delay"),
        (0x0805D138, 0x080EA1B0, "outer alternate inner state"),
        (0x0805D140, 0x0802D020, "outer masked wake service"),
        (0x0805D154, 0x0802CFF8, "outer state restoration"),
        (0x0805D158, 0x080596D0, "nested metric end wrapper"),
        (0x0805D15C, 0x0805972C, "deep-sleep metric end wrapper"),
        (0x0805D164, 0x080559B4, "outer post-service 01"),
        (0x0805D168, 0x082D9CDC, "outer post-service 02"),
        (0x0805D16C, 0x08039B6C, "outer post-service 03"),
        (0x0805D174, 0x082D9CF0, "outer conditional post-service"),
        (0x0805D180, 0x08143E08, "outer post-service 04"),
        (0x0805D190, 0x08143E08, "outer post-service 05"),
        (0x0805D194, 0x08143E64, "outer post-service 06"),
        (0x0805D198, 0x082A7D64, "outer post-service 07"),
    ):
        require_branch(body, address, target, purpose)

    # Event id 17 is the stock image's "Enter Deep Sleep" measurement.  Its
    # paired wrappers bracket the retained transaction at 0x0805d0b4/15c.
    require_word(body, 0x0805F5D8, 0xE3A00011,
                 "deep-sleep metric id")
    require_branch(body, 0x0805F5DC, 0x0802CF30,
                   "deep-sleep metric begin")
    require_word(body, 0x0805972C, 0xE3A00011,
                 "deep-sleep metric end id")
    require_branch(body, 0x08059730, 0x0802CF38,
                   "deep-sleep metric end")

    # Resolve both thunks through the main-image veneer table into the IRAM
    # implementations and verify their CPSR semantics instruction by
    # instruction.
    require_word(body, 0x0802D000, 0xE51FF004,
                 "interrupt-save veneer")
    require_word(body, 0x0802D004, 0x22001E70,
                 "interrupt-save IRAM target")
    require_word(body, 0x22001E70, 0xE10F1000,
                 "interrupt-save CPSR read")
    require_word(body, 0x22001E74, 0xE20100C0,
                 "interrupt-save old IRQ/FIQ mask")
    require_word(body, 0x22001E78, 0xE38110C0,
                 "interrupt-save mask both sources")
    require_word(body, 0x22001E7C, 0xE121F001,
                 "interrupt-save CPSR write")
    require_word(body, 0x0802D028, 0xE51FF004,
                 "interrupt-restore veneer")
    require_word(body, 0x0802D02C, 0x22001E84,
                 "interrupt-restore IRAM target")
    require_word(body, 0x22001E84, 0xE10F1000,
                 "interrupt-restore CPSR read")
    require_word(body, 0x22001E88, 0xE3C110C0,
                 "interrupt-restore clear current mask")
    require_word(body, 0x22001E8C, 0xE1810000,
                 "interrupt-restore merge saved mask")
    require_word(body, 0x22001E90, 0xE121F000,
                 "interrupt-restore CPSR write")

    # High-level coordinator: pre-sleep manager, IRQ save/mask, retained
    # wrapper, IRQ restore, then the post-wake manager.
    require_word(body, 0x0807B278, 0xE3A00002, "pre-sleep manager mode")
    require_branch(body, 0x0807B27C, 0x0835EB94, "pre-sleep manager")
    require_branch(body, 0x0807B280, 0x080D84E4, "interrupt save/mask")
    require_branch(body, 0x0807B288, 0x0802D050, "retained IRAM wrapper")
    require_branch(body, 0x0807B28C, 0x080D8E94, "interrupt restore")
    require_word(body, 0x0807B290, 0xE3A00005, "post-wake manager mode")
    require_branch(body, 0x0807B294, 0x0835EB94, "post-wake manager")

    # The interrupt transaction saves and restores both VIC enable words and
    # every EIC group's enable/level/type triple.  For Standby it replaces the
    # live topology with the exact PMU/group-6/USB policy, clears pending EIC
    # groups 3 and 6, and on wake clears them again before restoring VIC then
    # EIC state.  Generate the exact unrolled ARM encodings so the audit covers
    # all seven groups, not merely the first and last entries.
    for address, expected, purpose in (
        (0x080D84EC, 0xE5931010, "VIC0 enable snapshot read"),
        (0x080D84F4, 0xE5801008, "VIC0 enable snapshot store"),
        (0x080D84FC, 0xE5911010, "VIC1 enable snapshot read"),
        (0x080D8504, 0xE580100C, "VIC1 enable snapshot store"),
        (0x080D85F0, 0xE3A00008, "sleep EIC group 3 bit 3 literal"),
        (0x080D85F4, 0xE58C00CC, "sleep EIC group 3 enable"),
        (0x080D85F8, 0xE3A01201, "sleep EIC group 6 bit 28 literal"),
        (0x080D85FC, 0xE58C10D8, "sleep EIC group 6 enable"),
        (0x080D8600, 0xE3A01000, "sleep EIC group 3 low-level literal"),
        (0x080D8604, 0xE58C108C, "sleep EIC group 3 low level"),
        (0x080D8608, 0xE58C00EC, "sleep EIC group 3 level type"),
        (0x080D860C, 0xE3E00000, "sleep VIC clear literal"),
        (0x080D8610, 0xE5830014, "sleep VIC0 enable clear"),
        (0x080D8618, 0xE5830010, "sleep VIC0 source enable"),
        (0x080D8620, 0xEAFFFC44, "sleep pending-clear tail branch"),
        (0x080D8634, 0x00080009, "sleep VIC0 EXT0/EXT3/USB mask"),
        (0x080D7738, 0xE59F1038, "EIC base load for pending clear"),
        (0x080D773C, 0xE3E00000, "EIC all-pending clear value"),
        (0x080D7740, 0xE58100AC, "EIC group 3 pending clear"),
        (0x080D7744, 0xE58100B8, "EIC group 6 pending clear"),
        (0x080D8EEC, 0xEBFFFA11, "pre-restore pending clear call"),
        (0x080D8EF8, 0xE5901008, "VIC0 enable snapshot load"),
        (0x080D8EFC, 0xE5821010, "VIC0 enable restore"),
        (0x080D8F00, 0xE590100C, "VIC1 enable snapshot load"),
        (0x080D8F08, 0xE5821010, "VIC1 enable restore"),
    ):
        require_word(body, address, expected, purpose)

    for group in range(7):
        save = 0x080D8508 + group * 0x18
        restore = 0x080D8F0C + group * 0x18 + (4 if group else 0)
        record = 0x10 + group * 0x0C
        for index, (register, suffix) in enumerate((
            (0xC0 + group * 4, "enable"),
            (0x80 + group * 4, "level"),
            (0xE0 + group * 4, "type"),
        )):
            source = save + index * 8
            destination = record + index * 4
            require_word(
                body, source, 0xE59C1000 | register,
                f"EIC group {group} {suffix} snapshot read",
            )
            require_word(
                body, source + 4, 0xE5801000 | destination,
                f"EIC group {group} {suffix} snapshot store",
            )
            restore_load = restore + index * 8
            restore_store = restore_load + 4
            if group == 0:
                if index > 0:
                    restore_load += 4
                    restore_store += 4
                elif index == 0:
                    restore_store += 4
            restore_register = 0 if (group == 6 and index == 2) else 2
            require_word(
                body, restore_load,
                0xE5900000 | (restore_register << 12) | destination,
                f"EIC group {group} {suffix} restore load",
            )
            require_word(
                body, restore_store,
                0xE5810000 | (restore_register << 12) | register,
                f"EIC group {group} {suffix} restore write",
            )

    # Non-returning retained resume substrate.
    for address, target, purpose in (
        (0x080D24DC, 0x0802DDA8, "retained SP activation"),
        (0x080D24E0, 0x083600B8, "Timer E initialization"),
        (0x080D24E8, 0x082B1B60, "MIU resume mode"),
        (0x080D24EC, 0x080AA328, "IRAM restoration"),
        (0x080D24F0, 0x0802D0E8, "banked stack restoration"),
        (0x080D24F4, 0x0802D0F0, "cache/MMU reconstruction"),
        (0x080D24F8, 0x0807DB74, "dynamic mapping repair"),
        (0x080D2504, 0x0802DDB0, "direct continuation handoff"),
    ):
        require_branch(body, address, target, purpose)

    # Timer E is the delay source brought up before the continuation.
    for address, expected, purpose in (
        (0x083600BC, 0xE3A01D11, "Timer E control literal"),
        (0x083600C0, 0xE58010A0, "Timer E control write"),
        (0x083600C4, 0xE3A0100B, "Timer E prescaler literal"),
        (0x083600C8, 0xE58010B0, "Timer E prescaler write"),
        (0x083600CC, 0xE3E01000, "Timer E data literal"),
        (0x083600D0, 0xE58010A8, "Timer E data write"),
        (0x083600D4, 0xE3A01003, "Timer E command literal"),
        (0x083600D8, 0xE58010A4, "Timer E command write"),
    ):
        require_word(body, address, expected, purpose)

    # Device-manager mode 2 saves GPIO state before click-wheel suspend;
    # mode 5 resumes the wheel and restores GPIO before display services.
    require_branch(body, 0x0835EC0C, 0x0835EB10, "GPIO snapshot")
    require_branch(body, 0x0835EC20, 0x08362C58, "click-wheel suspend")
    require_branch(body, 0x0835EC3C, 0x08362C58, "click-wheel resume")
    require_branch(body, 0x0835EC44, 0x0835EAD0, "GPIO restore")

    # Each of 16 groups stores normalized PCON plus one byte from PUNB/PUNC.
    for address, expected, purpose in (
        (0x0835EB24, 0xE592C00C, "GPIO PUNB load"),
        (0x0835EB2C, 0xE5C1C004, "GPIO PUNB snapshot byte"),
        (0x0835EB30, 0xE592C010, "GPIO PUNC load"),
        (0x0835EB34, 0xE5C1C005, "GPIO PUNC snapshot byte"),
        (0x0835EB38, 0xE5921000, "GPIO PCON load"),
        (0x0835EB3C, 0xE592E004, "GPIO PDAT load"),
        (0x0835EB54, 0xE3570001, "GPIO output-mode test"),
        (0x0835EB68, 0x01811C15, "GPIO output-low normalization"),
        (0x0835EB6C, 0x11811C14, "GPIO output-high normalization"),
        (0x0835EB84, 0xE353000F, "GPIO 16-group bound"),
        (0x0835EAE0, 0xE5D3C004, "GPIO restored PUNB byte"),
        (0x0835EAE8, 0xE582C00C, "GPIO restored PUNB write"),
        (0x0835EAEC, 0xE5D33005, "GPIO restored PUNC byte"),
        (0x0835EAF0, 0xE5823010, "GPIO restored PUNC write"),
        (0x0835EAF4, 0xE7903181, "GPIO restored PCON word"),
        (0x0835EAF8, 0xE5823000, "GPIO restored PCON write"),
        (0x0835EB00, 0xE351000F, "GPIO restore 16-group bound"),
    ):
        require_word(body, address, expected, purpose)

    # Click-wheel mode 2 does not stop or clock-gate the controller: it drives
    # GPIO E4 low, waits 1 ms, then drives E2 low.  Mode 3 always waits 25 ms,
    # performs the full controller init, and runs the sideband command-state
    # recovery.  This detail matters: replacing the transaction with one
    # generic receive drain leaves the wheel in an unproven command epoch.
    for address, expected, purpose in (
        (0x08362C88, 0xE3A02000, "click-wheel suspend E4 low value"),
        (0x08362C8C, 0xE3A01001, "click-wheel suspend E4 output mode"),
        (0x08362C90, 0xE3A00074, "click-wheel suspend E4 pin"),
        (0x08362C98, 0xE3A00001, "click-wheel suspend inter-pin delay"),
        (0x08362CA0, 0xE3A02000, "click-wheel suspend E2 low value"),
        (0x08362CA4, 0xE3A01001, "click-wheel suspend E2 output mode"),
        (0x08362CA8, 0xE3A00072, "click-wheel suspend E2 pin"),
    ):
        require_word(body, address, expected, purpose)
    require_branch(body, 0x08362C94, 0x083606D8,
                   "click-wheel suspend E4 GPIO write")
    require_branch(body, 0x08362C9C, 0x0802CF18,
                   "click-wheel suspend 1 ms delay")
    require_branch(body, 0x08362CC0, 0x083606D8,
                   "click-wheel suspend E2 GPIO write")

    # Click-wheel resume waits 25 ms, performs a full controller init, and
    # then runs the command-state transaction.
    require_word(body, 0x08362CC4, 0xE3A00019, "click-wheel resume delay")
    require_branch(body, 0x08362CC8, 0x0802CF18, "click-wheel delay call")
    require_branch(body, 0x08362CCC, 0x0806DBA0, "click-wheel full init")
    require_branch(body, 0x08362CD4, 0x08362B44, "click-wheel command restore")

    # Full init restores alternate function 2 on GPIO E2..E5, with a 1 ms
    # delay after E2, then programs the exact controller values before the
    # sideband exchange.
    for address, expected, purpose in (
        (0x0806DBA4, 0xE3A02000, "click-wheel init E2 value"),
        (0x0806DBA8, 0xE3A01002, "click-wheel init E2 alternate mode"),
        (0x0806DBAC, 0xE3A00072, "click-wheel init E2 pin"),
        (0x0806DBB4, 0xE3A00001, "click-wheel init inter-pin delay"),
        (0x0806DBBC, 0xE3A02000, "click-wheel init E3 value"),
        (0x0806DBC0, 0xE3A01002, "click-wheel init E3 alternate mode"),
        (0x0806DBC4, 0xE3A00073, "click-wheel init E3 pin"),
        (0x0806DBCC, 0xE3A02000, "click-wheel init E4 value"),
        (0x0806DBD0, 0xE3A01002, "click-wheel init E4 alternate mode"),
        (0x0806DBD4, 0xE3A00074, "click-wheel init E4 pin"),
        (0x0806DBDC, 0xE3A02000, "click-wheel init E5 value"),
        (0x0806DBE0, 0xE3A01002, "click-wheel init E5 alternate mode"),
        (0x0806DBE4, 0xE3A00075, "click-wheel init E5 pin"),
        (0x0806DBF0, 0xE5901000, "click-wheel init WHEEL00 low-word load"),
        (0x0806DBF4, 0xE1A01821, "click-wheel init WHEEL00 low-word shift 1"),
        (0x0806DBF8, 0xE1A01801, "click-wheel init WHEEL00 low-word shift 2"),
        (0x0806DBFC, 0xE5801000, "click-wheel init WHEEL00 low-word clear"),
        (0x0806DC04, 0xE3C11807, "click-wheel init WHEEL00 0x70000 clear"),
        (0x0806DC08, 0xE5801000, "click-wheel init WHEEL00 edge 2"),
        (0x0806DC10, 0xE3C11702, "click-wheel init WHEEL00 0x80000 clear"),
        (0x0806DC14, 0xE5801000, "click-wheel init WHEEL00 edge 3"),
        (0x0806DC1C, 0xE3C11501, "click-wheel init WHEEL00 0x400000 clear"),
        (0x0806DC20, 0xE5801000, "click-wheel init WHEEL00 edge 4"),
        (0x0806DC24, 0xE3A01007, "click-wheel init pending clear"),
        (0x0806DC28, 0xE5801014, "click-wheel init pending clear write"),
        (0x0806DC34, 0xE5801010, "click-wheel init control enable"),
        (0x0806DC3C, 0xE5801008, "click-wheel init timing write"),
        (0x0806DC44, 0xE3811601, "click-wheel init WHEEL00 bit 20"),
        (0x0806DC48, 0xE5801000, "click-wheel init WHEEL00 edge 5"),
        (0x0806DC50, 0xE3811602, "click-wheel init WHEEL00 bit 21"),
        (0x0806DC54, 0xE5801000, "click-wheel init WHEEL00 edge 6"),
        (0x0806DC60, 0x3C200000, "click-wheel controller base"),
        (0x0806DC64, 0x0003A980, "click-wheel timing value"),
    ):
        require_word(body, address, expected, purpose)
    for address, target, purpose in (
        (0x0806DBB0, 0x083606D8, "click-wheel init E2 GPIO write"),
        (0x0806DBB8, 0x0802CF18, "click-wheel init 1 ms delay"),
        (0x0806DBC8, 0x083606D8, "click-wheel init E3 GPIO write"),
        (0x0806DBD8, 0x083606D8, "click-wheel init E4 GPIO write"),
        (0x0806DBE8, 0x083606D8, "click-wheel init E5 GPIO write"),
    ):
        require_branch(body, address, target, purpose)

    # 0x08362b44 allows five zero-response timeouts for query 0x8000063a.
    # Nonzero replies with the wrong identity are ignored without consuming a
    # timeout allowance.  A matching reply contributes bits 16..30 to the
    # stored state and triggers acknowledgement 0x8000062a when nonzero.
    for address, expected, purpose in (
        (0x08362B4C, 0xE3A04005, "click-wheel command retry count"),
        (0x08362B58, 0xE1A07000, "click-wheel outer control save"),
        (0x08362B64, 0xE3500000, "click-wheel zero-response test"),
        (0x08362B68, 0x02444001, "click-wheel zero-only retry decrement"),
        (0x08362B70, 0xE3C0147F, "click-wheel response high mask"),
        (0x08362B74, 0xE3C118FF, "click-wheel response middle mask"),
        (0x08362B7C, 0x1A000008, "click-wheel nonzero mismatch retry"),
        (0x08362B80, 0xE1A00080, "click-wheel state extraction shift 1"),
        (0x08362B84, 0xE1A018A0, "click-wheel state extraction shift 2"),
        (0x08362BAC, 0xE3540000, "click-wheel retry exhaustion test"),
        (0x08362BB8, 0xE5807010, "click-wheel outer control restore"),
        (0x08362BC0, 0x8000063A, "click-wheel state query"),
        (0x08362BC8, 0x8000062A, "click-wheel state acknowledgement"),
    ):
        require_word(body, address, expected, purpose)
    require_branch(body, 0x08362B50, 0x080C9C54,
                   "click-wheel outer control clear")
    require_branch(body, 0x08362B60, 0x08362A0C,
                   "click-wheel command exchange")
    require_branch(body, 0x08362B9C, 0x08362BD0,
                   "click-wheel state acknowledgement send")

    # The outer state restore saves and clears WHEEL10 across the complete
    # retry epoch.  The shared command primitive independently saves/clears
    # WHEEL10, clears receive
    # interrupt 0 twice, issues the command, polls up to 2 ms for WHEEL0C RX
    # ready, consumes WHEELRX, clears the edge, and restores WHEEL10.  Its
    # sender toggles WHEEL00 bit 21, waits 1 ms, encodes the command into
    # WHEELTX, starts it through WHEEL04, and polls WHEEL0C bit 3 for 1.5 ms.
    for address, expected, purpose in (
        (0x08362A2C, 0xE5865014, "click-wheel receive clear 1"),
        (0x08362A30, 0xE5865014, "click-wheel receive clear 2"),
        (0x08362A68, 0xE3A01E7D, "click-wheel receive 2 ms timeout"),
        (0x08362A7C, 0xE596000C, "click-wheel receive status read"),
        (0x08362A84, 0x15967018, "click-wheel receive data read"),
        (0x08362A88, 0xE5865014, "click-wheel receive final clear"),
        (0x08362A8C, 0xE5869010, "click-wheel control restore"),
        (0x08362BE4, 0xE3C00602, "click-wheel command bit-21 clear"),
        (0x08362BF8, 0xE3800602, "click-wheel command bit-21 set"),
        (0x08362C00, 0xE3A00102, "click-wheel command bit-31 literal"),
        (0x08362C04, 0xE18000A6, "click-wheel command encoding"),
        (0x08362C08, 0xE585001C, "click-wheel command transmit"),
        (0x08362C14, 0xE5850004, "click-wheel command start"),
        (0x08362C54, 0x000005DC, "click-wheel command 1.5 ms timeout"),
        (0x080C9C58, 0xE5910010, "click-wheel control helper save"),
        (0x080C9C5C, 0xE3A02000, "click-wheel control helper zero"),
        (0x080C9C60, 0xE5812010, "click-wheel control helper clear"),
    ):
        require_word(body, address, expected, purpose)
    require_branch(body, 0x08362A1C, 0x080C9C54,
                   "click-wheel exchange control clear")
    require_branch(body, 0x08362A38, 0x08362BD0,
                   "click-wheel exchange sender")
    require_branch(body, 0x08362BF0, 0x0802CF18,
                   "click-wheel sender 1 ms delay")

    # Exact LCD controller substrate reached from the post-wake manager.
    require_branch(body, 0x083602EC, 0x080C9C00, "LCD controller init")
    for address, expected, purpose in (
        (0x080C9C04, 0xE3A01102, "LCD reset literal"),
        (0x080C9C08, 0xE5801000, "LCD reset write"),
        (0x080C9C0C, 0xE59F1020, "LCD enable literal load"),
        (0x080C9C10, 0xE5801000, "LCD enable write"),
        (0x080C9C14, 0xE3A01401, "LCD clock literal"),
        (0x080C9C18, 0xE5801088, "LCD clock write"),
        (0x080C9C1C, 0xE3A01033, "LCD timing literal"),
        (0x080C9C20, 0xE5801020, "LCD timing write"),
        (0x080C9C24, 0xE59F100C, "LCD final literal load"),
        (0x080C9C28, 0xE580107C, "LCD final write"),
        (0x080C9C34, 0x80100DB1, "LCD enable value"),
        (0x080C9C38, 0x00000804, "LCD final value"),
    ):
        require_word(body, address, expected, purpose)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("osos", type=Path)
    args = parser.parse_args()

    image = args.osos.read_bytes()
    image_sha256 = hashlib.sha256(image).hexdigest()
    if len(image) == OSOS_SIZE:
        if image_sha256 != OSOS_SHA256:
            raise AuditError(
                f"unexpected full-image SHA-256: {image_sha256}; "
                f"expected {OSOS_SHA256}"
            )
        body = image[OSOS_HEADER_SIZE:]
    else:
        body = image

    audit(body)
    print("PASS: Apple iPod Classic 6G/7G RetailOS 2.0.4 hibernate audit")
    print(f"  input SHA-256: {image_sha256}")
    print("  outer power state: UI -> 35-call service transaction -> retained coordinator")
    print("  input: down/up/hold phases; Play-up toggles; long Play queues event 15")
    print("  media contract: opcode 8 carries output lock; opcode 9 rebuilds then unlocks")
    print("  coordinator: pre-sleep -> IRQ mask -> retained entry -> IRQ restore -> post-wake")
    print("  interrupts: exact sleep VIC/EIC policy; both VIC enables and all 7 EIC groups restored")
    print("  resume: Timer E -> MIU/IRAM/MMU -> direct continuation")
    print("  devices: 16-group GPIO; exact mode-2/mode-3 wheel transaction; LCD controller")
    print("  disk: synchronous state 2; event 17 precedes demand-driven state 1")
    print("        FLUSH/STANDBY share the live-device guard")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AuditError as error:
        print(f"FAIL: {error}")
        raise SystemExit(1)

#!/usr/bin/env python3
"""Sync Sitekick drops, codes and trades between RockPod and an iPod.

Examples:
    sitekick_sync.py status  --pack assets/ipodjs/rockbox/sitekick
    sitekick_sync.py drop    --device /run/media/$USER/IPOD --code SPRING24
    sitekick_sync.py code    --device /run/media/$USER/IPOD --code Blackery
    sitekick_sync.py trade   --device /run/media/$USER/IPOD --give 129,214
    sitekick_sync.py deploy  --device /run/media/$USER/IPOD
"""

from __future__ import annotations

import argparse
import sys
from datetime import date
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from rockpod.services import sitekick  # noqa: E402

DEFAULT_PACK = Path("assets/ipodjs/rockbox/sitekick")


def _device_pack(device: Path) -> Path:
    """Resolve the on-device asset root from a mount point."""
    if (device / sitekick.CHIPS_FILE).is_file():
        return device
    candidate = device / ".rockbox" / "sitekick"
    if (candidate / sitekick.CHIPS_FILE).is_file():
        return candidate
    raise sitekick.SitekickError(
        f"no Sitekick asset pack on {device}; run 'deploy' first")


def cmd_status(args) -> int:
    root = _device_pack(Path(args.device)) if args.device else Path(args.pack)
    chips = sitekick.load_chips(root)
    worn = sum(1 for chip in chips.values() if chip.worn)
    by_rarity = {name: 0 for name in sitekick.RARITIES}
    by_slot: dict[str, int] = {}
    for chip in chips.values():
        by_rarity[chip.rarity] = by_rarity.get(chip.rarity, 0) + 1
        by_slot[chip.slot] = by_slot.get(chip.slot, 0) + 1

    print(f"chips        : {len(chips)} ({worn} with worn art)")
    print("rarity       : " + "  ".join(
        f"{name}={by_rarity.get(name, 0)}" for name in sitekick.RARITIES))
    print("slots        : " + "  ".join(
        f"{name}={count}" for name, count in sorted(by_slot.items())))
    print(f"overrides    : {len(sitekick.load_overrides(root))}")
    print(f"codes        : {len(sitekick.load_codes(root))}")

    when = date.today()
    daily = sitekick.daily_chip(chips, when)
    print(f"chip of day  : {daily}")
    print("chips of week: " +
          ", ".join(str(c) for c in sitekick.weekly_chips(chips, when)))

    offers = sitekick.read_outbox(root)
    if offers:
        print("staged offer : " +
              ", ".join(f"{o.chip_id}" for o in offers))
    return 0


def cmd_drop(args) -> int:
    root = _device_pack(Path(args.device)) if args.device else Path(args.pack)
    grants = sitekick.build_drop(
        root,
        when=date.fromisoformat(args.date) if args.date else None,
        code=args.code,
        include_weekly=not args.no_weekly,
        include_monthly=args.monthly,
    )
    if not grants:
        print("nothing to drop")
        return 0
    path, added = sitekick.merge_inbox(root, grants)
    for grant in added:
        print(f"  {grant.kind:6s} {grant.value:6d}  {grant.label}")
    print(f"wrote {len(added)} new grants to {path}")
    return 0


def cmd_code(args) -> int:
    root = _device_pack(Path(args.device)) if args.device else Path(args.pack)
    state = sitekick.load_state(root)
    grants = sitekick.stage_code(root, args.code, state.owned)
    if not grants:
        print("all code rewards are already owned or pending")
        return 0
    for grant in grants:
        print(f"  {grant.kind:6s} {grant.value:6d}  {grant.label}")
    print(f"staged {len(grants)} rewards for the next Sitekick launch")
    return 0


def cmd_trade(args) -> int:
    root = _device_pack(Path(args.device))
    offers = sitekick.read_outbox(root)
    incoming = [int(x) for x in args.give.split(",") if x.strip()] \
        if args.give else []

    if not offers and not incoming:
        print("no staged offer and nothing to give")
        return 1
    for offer in offers:
        print(f"  device offers {offer.chip_id} {offer.name}")

    grants = sitekick.settle_trade(offers, incoming)
    if grants:
        path, added = sitekick.merge_inbox(root, grants)
        print(f"wrote {len(added)} new grants to {path}")
    if offers and not args.keep:
        sitekick.clear_outbox(root)
        print("cleared staged offer")
    return 0


def cmd_overrides(args) -> int:
    root = _device_pack(Path(args.device)) if args.device else Path(args.pack)
    changed = sitekick.apply_overrides(root)
    print(f"applied {changed} overrides")
    return 0


def cmd_deploy(args) -> int:
    copied = sitekick.deploy(Path(args.pack), Path(args.device))
    for name, count in sorted(copied.items()):
        print(f"  {name:8s} {count} files")
    print(f"deployed to {args.device}")
    return 0


def main(argv=None) -> int:
    # --pack is accepted either before or after the subcommand.
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--pack", default=str(DEFAULT_PACK),
                        help="packaged asset tree on the host")

    parser = argparse.ArgumentParser(description=__doc__, parents=[common])
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("status", parents=[common],
                       help="show catalogue and pending drops")
    p.add_argument("--device")
    p.set_defaults(func=cmd_status)

    p = sub.add_parser("drop", parents=[common],
                       help="stage daily/weekly/code grants")
    p.add_argument("--device")
    p.add_argument("--code")
    p.add_argument("--date", help="ISO date to generate for")
    p.add_argument("--monthly", action="store_true")
    p.add_argument("--no-weekly", action="store_true")
    p.set_defaults(func=cmd_drop)

    p = sub.add_parser("code", parents=[common],
                       help="stage one secret code without timed drops")
    p.add_argument("--device")
    p.add_argument("--code", required=True)
    p.set_defaults(func=cmd_code)

    p = sub.add_parser("trade", parents=[common],
                       help="settle a staged trade offer")
    p.add_argument("--device", required=True)
    p.add_argument("--give", help="comma separated chip ids to grant")
    p.add_argument("--keep", action="store_true",
                   help="leave the staged offer in place")
    p.set_defaults(func=cmd_trade)

    p = sub.add_parser("overrides", parents=[common],
                       help="fold overrides into chips.v1.tsv")
    p.add_argument("--device")
    p.set_defaults(func=cmd_overrides)

    p = sub.add_parser("deploy", parents=[common],
                       help="copy the asset pack to a device")
    p.add_argument("--device", required=True)
    p.set_defaults(func=cmd_deploy)

    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except sitekick.SitekickError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

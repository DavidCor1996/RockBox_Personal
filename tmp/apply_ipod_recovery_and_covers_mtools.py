#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import subprocess
import sys
import unicodedata
import re
from pathlib import Path


def ascii_part(part: str, is_file: bool = False) -> str:
    part = unicodedata.normalize("NFKD", part).encode("ascii", "ignore").decode("ascii")
    part = re.sub(r'[<>:"/\\|?*;\x00-\x1f]', "_", part)
    part = re.sub(r"\s+", " ", part).strip(" .")
    if is_file and "." in part:
        stem, ext = part.rsplit(".", 1)
        ext = "." + ext[:8]
        stem = stem[: max(1, 96 - len(ext))].rstrip(" .")
        return (stem + ext) or "Unknown"
    return part[:96] or "Unknown"


def normalize_dest(path: str) -> str:
    parts = [part for part in Path(path).parts if part not in ("", ".")]
    return "/".join(ascii_part(part, index == len(parts) - 1) for index, part in enumerate(parts))


def mpath(path: str) -> str:
    return "::" + path


def run(cmd: list[str], timeout: int = 120) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    if proc.returncode:
        print(proc.stdout, file=sys.stderr, end="")
        raise subprocess.CalledProcessError(proc.returncode, cmd, proc.stdout)
    return proc


def ensure_dirs(device: str, dest: str, made: set[str]) -> None:
    parts = dest.split("/")[:-1]
    current = ""
    for part in parts:
        current = f"{current}/{part}" if current else part
        if current in made:
            continue
        probe = subprocess.run(
            ["mdir", "-i", device, mpath(current)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=60,
        )
        if probe.returncode:
            run(["mmd", "-i", device, mpath(current)])
        made.add(current)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("device")
    parser.add_argument("report", type=Path)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--covers-only", action="store_true")
    parser.add_argument("--recs-only", action="store_true")
    args = parser.parse_args()

    made: set[str] = set()
    copied = 0
    moved = 0
    skipped = 0
    failed = 0

    with args.report.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        for row in reader:
            kind = row.get("kind", "")
            status = row.get("status", "")
            target = normalize_dest(row.get("target", ""))
            if not target:
                skipped += 1
                continue
            if kind == "cover":
                if args.recs_only or status != "staged":
                    skipped += 1
                    continue
                source = args.stage / target
                if not source.is_file():
                    print(f"missing staged cover: {source}", file=sys.stderr)
                    failed += 1
                    continue
                try:
                    ensure_dirs(args.device, target, made)
                    run(["mcopy", "-o", "-i", args.device, str(source), mpath(target)])
                except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
                    print(f"failed cover {source} -> {target}: {exc}", file=sys.stderr)
                    failed += 1
                    continue
                copied += 1
                print(f"copied\t{target}", flush=True)
            elif kind == "rec":
                if args.covers_only:
                    skipped += 1
                    continue
                source = row.get("source", "")
                if not source:
                    skipped += 1
                    continue
                try:
                    ensure_dirs(args.device, target, made)
                    proc = subprocess.run(
                        ["mren", "-D", "s", "-i", args.device, mpath(source), mpath(target)],
                        text=True,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        timeout=60,
                    )
                    if proc.returncode:
                        text = proc.stdout.lower()
                        if "not found" in text or "no match" in text:
                            skipped += 1
                            continue
                        print(proc.stdout, file=sys.stderr, end="")
                        failed += 1
                        continue
                except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
                    print(f"failed rec {source} -> {target}: {exc}", file=sys.stderr)
                    failed += 1
                    continue
                moved += 1
                if moved % 100 == 0:
                    print(f"progress copied={copied} moved={moved} skipped={skipped} failed={failed}", flush=True)
            else:
                skipped += 1

    print(f"summary copied={copied} moved={moved} skipped={skipped} failed={failed}", flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())

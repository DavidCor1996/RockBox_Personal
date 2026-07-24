#!/usr/bin/env python3
"""Validate a complete RockAchievements installation on a device or simdisk."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "rockpod"))

from services.achievements import (  # noqa: E402
    AchievementSyncService,
    OFFLINE_RUNTIME_EXTENSIONS,
    _runtime_definition,
)


RUNTIME_PLUGINS = {
    ".gb": ".rockbox/rocks/viewers/rockboy.rock",
    ".gbc": ".rockbox/rocks/viewers/rockboy.rock",
    ".nes": ".rockbox/rocks/viewers/infones.rock",
    ".sfc": ".rockbox/rocks/games/snes_lite.rock",
    ".smc": ".rockbox/rocks/games/snes_lite.rock",
    ".sms": ".rockbox/rocks/games/smsgg.rock",
    ".sg": ".rockbox/rocks/games/smsgg.rock",
    ".gg": ".rockbox/rocks/games/smsgg.rock",
    ".md": ".rockbox/rocks/games/picodrive.rock",
    ".gen": ".rockbox/rocks/games/picodrive.rock",
    ".bin": ".rockbox/rocks/games/picodrive.rock",
    ".smd": ".rockbox/rocks/games/picodrive.rock",
    ".min": ".rockbox/rocks/viewers/pokemini.rock",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", errors="strict") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def device_path(mount: Path, value: str) -> Path:
    return mount / value.lstrip("/")


def validate(mount: Path, repo_root: Path) -> list[str]:
    errors: list[str] = []
    root = mount / ".rockbox" / "achievements"
    current = root / "current"
    if not current.is_file():
        return ["missing .rockbox/achievements/current"]
    generation_id = current.read_text(encoding="utf-8").strip()
    if not generation_id or "/" in generation_id or ".." in generation_id:
        return ["invalid active generation identifier"]
    generation = root / "generations" / generation_id
    catalog_path = generation / "catalog.tsv"
    coverage_path = generation / "coverage.json"
    provenance_path = generation / "provenance.tsv"
    for required in (catalog_path, coverage_path, provenance_path):
        if not required.is_file():
            errors.append(f"missing {required.relative_to(mount)}")
    if errors:
        return errors

    catalog = rows(catalog_path)
    coverage = json.loads(coverage_path.read_text(encoding="utf-8"))
    if coverage.get("generation") != generation_id:
        errors.append("coverage generation does not match current")
    if coverage.get("games") != len(catalog):
        errors.append("coverage game count does not match catalog")
    if coverage.get("uncovered") != 0:
        errors.append(f"coverage reports {coverage.get('uncovered')} uncovered games")

    discovered = AchievementSyncService(repo_root).discover_games(mount)
    discovered_targets = {game.launch_target.casefold() for game in discovered}
    catalog_targets = {item.get("launch_target", "").casefold()
                       for item in catalog}
    for target in sorted(discovered_targets - catalog_targets):
        errors.append(f"launchable game missing from catalog: {target}")
    for target in sorted(catalog_targets - discovered_targets):
        errors.append(f"catalog game is not launchable: {target}")

    for game in catalog:
        key = game.get("game_key", "")
        title = game.get("title", key)
        achievement_path = device_path(mount, game.get("achievement_file", ""))
        cover_path = device_path(mount, game.get("cover_path", ""))
        if not achievement_path.is_file():
            errors.append(f"{title}: missing achievement table")
            continue
        if not cover_path.is_file():
            errors.append(f"{title}: missing real cover asset")
        achievement_rows = rows(achievement_path)
        if str(len(achievement_rows)) != game.get("total"):
            errors.append(f"{title}: catalog total does not match achievement table")
        official = game.get("set_kind") == "retroachievements"
        extension = Path(game.get("launch_target", "")).suffix.casefold()
        if official:
            if extension not in OFFLINE_RUNTIME_EXTENSIONS:
                errors.append(f"{title}: official set has no offline runtime mapping")
            plugin = RUNTIME_PLUGINS.get(extension)
            if not plugin or not (mount / plugin).is_file():
                errors.append(f"{title}: offline runtime plugin is missing")
            if len(game.get("ra_hash", "")) != 32:
                errors.append(f"{title}: missing official ROM hash")
        for achievement in achievement_rows:
            achievement_id = achievement.get("id", "")
            for column in ("badge_unlocked", "badge_locked"):
                badge = device_path(mount, achievement.get(column, ""))
                if not badge.is_file():
                    errors.append(f"{title}/{achievement_id}: missing {column}")
            if official and not _runtime_definition(
                    achievement.get("memaddr")):
                errors.append(f"{title}/{achievement_id}: missing live memory definition")

    for item in rows(provenance_path):
        output = generation / "games" / item.get("game_key", "") / item.get("asset", "")
        if not output.is_file():
            errors.append(f"provenance output missing: {output.relative_to(mount)}")
        elif sha256(output) != item.get("output_sha256"):
            errors.append(f"provenance checksum mismatch: {output.relative_to(mount)}")
        if not item.get("source") or not item.get("source_sha256"):
            errors.append(f"incomplete provenance row for {item.get('asset', '')}")

    for name in ("sessions.v1.tsv", "unlocks.v1.tsv", "events.v1.tsv"):
        if not (root / "state" / name).is_file():
            errors.append(f"missing durable state file: {name}")
    for path in (current, catalog_path, coverage_path, provenance_path):
        if path.is_file():
            sample = path.read_bytes()[:65536].lower()
            if b"rockpod_ra_web_api_key" in sample or b"web_api_key" in sample:
                errors.append(f"desktop credential marker leaked to device: {path}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mount", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    args = parser.parse_args()
    mount = args.mount.expanduser().resolve()
    errors = validate(mount, args.repo_root.expanduser().resolve())
    if errors:
        print("RockAchievements coverage: FAIL")
        for error in errors:
            print(f"- {error}")
        return 1
    print("RockAchievements coverage: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

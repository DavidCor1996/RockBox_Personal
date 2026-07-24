import csv
import io
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest
from PIL import Image

from services.achievements import (
    AchievementSyncService,
    _has_complete_runtime_set,
    _runtime_definition,
)


def _write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _fixture(tmp_path):
    mount = tmp_path / "device"
    repo = tmp_path / "repo"
    rom = mount / "gameboy" / "Test Game.gb"
    rom.parent.mkdir(parents=True)
    rom.write_bytes(b"test-rom")
    cover = mount / ".rockbox/games/library/covers/gameboy/Test Game.bmp"
    cover.parent.mkdir(parents=True)
    Image.new("RGB", (80, 120), (20, 90, 180)).save(cover, "BMP")
    manifest = mount / ".rockbox/rocks/games/rockboy_launcher/games.tsv"
    _write(
        manifest,
        "# title\\trom_path\\tcover_path\\t...\n"
        "Test Game\t/gameboy/Test Game.gb\t"
        "/.rockbox/games/library/covers/gameboy/Test Game.bmp\t0\t\t"
        "1990\tAction\tPublisher\tDeveloper\tDescription\t\n",
    )
    sphere = repo / "assets/ipodjs/sources/xbox360/xbox360-sphere-official.jpg"
    sphere.parent.mkdir(parents=True)
    Image.new("RGB", (120, 117), (90, 150, 30)).save(sphere, "JPEG")
    return repo, mount


def test_discovers_and_deduplicates_launcher_and_system_manifests(tmp_path):
    repo, mount = _fixture(tmp_path)
    system = mount / ".rockbox/games/gameboy/games.tsv"
    _write(
        system,
        "id\ttitle\tfile\tcover\tfavorite\tlast_played\thaptic_profile\n"
        "test\tTest Game\t/gameboy/Test Game.gb\t"
        "/.rockbox/games/library/covers/gameboy/Test Game.bmp\t0\t\t\n",
    )
    games = AchievementSyncService(repo).discover_games(mount)
    assert len(games) == 1
    assert games[0].console == "Game Boy"
    assert games[0].cover_abs.endswith("Test Game.bmp")


def test_builds_complete_atomic_baseline_generation(tmp_path):
    repo, mount = _fixture(tmp_path)
    stage = tmp_path / "stage"
    assets, coverage = AchievementSyncService(repo).build_sync_assets(
        mount, stage)
    assert coverage == {
        **coverage,
        "games": 1,
        "official": 0,
        "baseline": 1,
        "uncovered": 0,
        "notifications": "deferred",
    }
    assert coverage["achievement_mode"] == "ipod-hardcore"
    assert coverage["verification"] == "local-device"
    assert coverage["official_ra_hardcore_mastery"] is False
    current_asset = next(
        item for item in assets if item["destination_rel"].endswith("/current"))
    generation = open(current_asset["source_abs"], encoding="utf-8").read().strip()
    catalog_asset = next(
        item for item in assets if item["destination_rel"].endswith("/catalog.tsv"))
    with open(catalog_asset["source_abs"], encoding="utf-8") as handle:
        catalog = list(csv.DictReader(handle, delimiter="\t"))
    assert len(catalog) == 1
    assert catalog[0]["set_kind"] == "local-baseline"
    assert catalog[0]["total"] == "4"
    assert f"/generations/{generation}/" in catalog[0]["achievement_file"]
    achievement_asset = next(
        item for item in assets
        if item["destination_rel"].endswith("/achievements.tsv"))
    with open(achievement_asset["source_abs"], encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert [row["title"] for row in rows] == [
        "First Play", "Getting Started", "Settled In", "Regular Player"]
    assert all(row["state"] == "locked" for row in rows)
    coverage_asset = next(
        item for item in assets if item["destination_rel"].endswith("/coverage.json"))
    assert json.load(open(coverage_asset["source_abs"], encoding="utf-8"))[
        "uncovered"] == 0


def test_existing_progress_state_is_never_seeded_over(tmp_path):
    repo, mount = _fixture(tmp_path)
    state = mount / ".rockbox/achievements/state/sessions.v1.tsv"
    _write(state, "launch_target\tsessions\tseconds\tlast_played\n/game\t2\t50\t1\n")
    assets, _ = AchievementSyncService(repo).build_sync_assets(
        mount, tmp_path / "stage")
    destinations = {item["destination_rel"] for item in assets}
    assert ".rockbox/achievements/state/sessions.v1.tsv" not in destinations


def test_missing_game_cover_uses_real_checked_in_xbox_asset(tmp_path):
    repo, mount = _fixture(tmp_path)
    manifest = mount / ".rockbox/rocks/games/rockboy_launcher/games.tsv"
    text = manifest.read_text(encoding="utf-8").replace(
        "/.rockbox/games/library/covers/gameboy/Test Game.bmp", "")
    manifest.write_text(text, encoding="utf-8")
    assets, coverage = AchievementSyncService(repo).build_sync_assets(
        mount, tmp_path / "stage")
    assert coverage["uncovered"] == 0
    assert any(item["destination_rel"].endswith("/badge.bmp")
               for item in assets)


def test_official_hash_match_installs_live_definition_and_real_badges(tmp_path):
    repo, mount = _fixture(tmp_path)
    badge = io.BytesIO()
    Image.new("RGB", (64, 64), (44, 160, 28)).save(badge, "PNG")
    badge_data = badge.getvalue()

    class Response(io.BytesIO):
        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, traceback):
            self.close()

    def opener(request, timeout):
        assert timeout == 30
        url = request.full_url
        if "API_GetGameList.php" in url:
            payload = [{"id": 123, "hashes": ["a" * 32]}]
            return Response(json.dumps(payload).encode("utf-8"))
        if "API_GetGameExtended.php" in url:
            payload = {
                "ID": 123,
                "NumAchievements": 1,
                "Achievements": {
                    "99": {
                        "ID": 99, "BadgeName": "00099",
                        "Title": "Live Test", "Description": "Set RAM byte.",
                        "Points": 5, "MemAddr": "0xH0001=1",
                    },
                },
            }
            return Response(json.dumps(payload).encode("utf-8"))
        assert "media.retroachievements.org/Badge/" in url
        return Response(badge_data)

    service = AchievementSyncService(repo, api_key="test-key", opener=opener)
    service._rhash = lambda console_id, path: "a" * 32
    assets, coverage = service.build_sync_assets(
        mount, tmp_path / "stage-official")
    assert coverage["official"] == 1
    catalog_asset = next(
        item for item in assets if item["destination_rel"].endswith("catalog.tsv"))
    with open(catalog_asset["source_abs"], encoding="utf-8") as handle:
        catalog = list(csv.DictReader(handle, delimiter="\t"))
    assert catalog[0]["set_kind"] == "retroachievements"
    assert catalog[0]["ra_hash"] == "a" * 32
    achievement_asset = next(
        item for item in assets
        if item["destination_rel"].endswith("achievements.tsv"))
    with open(achievement_asset["source_abs"], encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert rows[0]["memaddr"] == "0xH0001=1"
    assert rows[0]["source"] == "retroachievements"
    assert any(item["destination_rel"].endswith("/99.bmp") for item in assets)
    assert any(item["destination_rel"].endswith("/99_lock.bmp")
               for item in assets)


def test_api_definition_hash_is_not_installed_as_runtime_rule():
    assert _runtime_definition("0123456789abcdef0123456789abcdef") == ""
    assert _runtime_definition("0xH0001=1") == "0xH0001=1"
    assert not _has_complete_runtime_set({
        "Achievements": {"1": {"MemAddr": "a" * 32}},
    })
    assert _has_complete_runtime_set({
        "Achievements": {"1": {"MemAddr": "0xH0001=1"}},
    })


def test_emulators_preflight_rules_before_reserving_runtime_workspace():
    root = Path(__file__).resolve().parents[2]
    sources = (
        "apps/plugins/infones/infones_rockbox.c",
        "apps/plugins/rockboy/rockboy.c",
        "apps/plugins/pokemini/pokemini_rockbox.c",
        "apps/plugins/smsgg/smsgg.c",
        "apps/plugins/snes_lite/snes_lite.c",
        "apps/plugins/picodrive/picodrive.c",
    )

    for source in sources:
        text = (root / source).read_text(encoding="utf-8")
        preflight = text.index("rockachievements_available(")
        allocation = text.index("ROCKACHIEVEMENTS_WORKSPACE_TARGET", preflight)
        assert preflight < allocation, source


def test_official_set_rejects_partial_api_payload(tmp_path):
    repo, mount = _fixture(tmp_path)
    service = AchievementSyncService(repo, api_key="test-key")
    game = service.discover_games(mount)[0]
    details = {
        "ID": 123,
        "NumAchievements": 2,
        "Achievements": {
            "99": {
                "ID": 99, "BadgeName": "00099", "Title": "Only One",
            },
        },
    }
    with pytest.raises(ValueError, match="partial set"):
        service._official_achievements(
            tmp_path / "game", "generation", game, details
        )


def test_verified_official_cache_works_without_api_credentials(tmp_path):
    repo, mount = _fixture(tmp_path)
    cache = repo / "rockpod/.cache/retroachievements"
    _write(cache / "console-4.json", json.dumps([
        {"ID": 123, "Hashes": ["a" * 32]},
    ]))
    _write(cache / "game-123.json", json.dumps({
        "ID": 123,
        "Title": "Test Game",
        "NumAchievements": 1,
        "Achievements": {
            "99": {
                "ID": 99, "BadgeName": "00099", "Title": "Cached",
                "Description": "Loaded from the verified cache.",
                "Points": 5, "MemAddr": "0xH0001=1",
            },
        },
    }))
    badge = io.BytesIO()
    Image.new("RGB", (64, 64), (44, 160, 28)).save(badge, "PNG")

    class Response(io.BytesIO):
        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, traceback):
            self.close()

    def opener(request, timeout):
        assert "media.retroachievements.org/Badge/" in request.full_url
        return Response(badge.getvalue())

    service = AchievementSyncService(repo, opener=opener)
    service._rhash = lambda console_id, path: "a" * 32
    assets, coverage = service.build_sync_assets(
        mount, tmp_path / "stage-cached-official"
    )
    assert coverage["official"] == 1
    achievement_asset = next(
        item for item in assets
        if item["destination_rel"].endswith("achievements.tsv")
    )
    with open(achievement_asset["source_abs"], encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert [row["title"] for row in rows] == ["Cached"]


def test_coverage_gate_accepts_complete_baseline_install(tmp_path):
    repo, mount = _fixture(tmp_path)
    assets, _ = AchievementSyncService(repo).build_sync_assets(
        mount, tmp_path / "stage-coverage")
    for asset in assets:
        destination = mount / asset["destination_rel"]
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(asset["source_abs"], destination)
    tool = os.path.abspath(os.path.join(
        os.path.dirname(__file__), "..", "..", "tools",
        "rockpod_achievements_coverage.py"))
    result = subprocess.run(
        [sys.executable, tool, "--mount", str(mount),
         "--repo-root", str(repo)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "coverage: PASS" in result.stdout


def test_ipod_hardcore_blocks_state_loading_in_supported_emulators():
    root = Path(__file__).resolve().parents[2]
    smsgg = root / "apps/plugins/smsgg/smsgg.c"
    picodrive = root / "apps/plugins/picodrive/picodrive.c"
    rockboy_menu = root / "apps/plugins/rockboy/menu.c"
    rockboy_main = root / "apps/plugins/rockboy/rockboy.c"

    assert smsgg.read_text().count(
        "rockachievements_hardcore_active(&achievements)"
    ) >= 2
    assert "iPod Hardcore: state load blocked" in picodrive.read_text()
    assert "rockachievements_any_hardcore_active()" in rockboy_menu.read_text()
    assert "!rockachievements_hardcore_active(&rockboy_achievements)" in \
        rockboy_main.read_text()

import os
import zipfile
from pathlib import Path

from app.config import Config
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_games import RockboxGameService
from services.rockbox_profiles import RockboxProfileStore


def _temp_names(path):
    return [name for name in os.listdir(path) if name.startswith("tmp")]


def _make_file(path, content=b"x"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    mode = "wb" if isinstance(content, bytes) else "w"
    with open(path, mode) as handle:
        handle.write(content)


def _make_store(tmp_dir, repo_root):
    config = Config(os.path.join(tmp_dir, "games-config.json"))
    config.cache_dir = os.path.join(tmp_dir, "cache")
    os.makedirs(config.cache_dir, exist_ok=True)
    return config, RockboxProfileStore(config, repo_root)


def test_rom_discovery_and_extension_filtering(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    _make_file(os.path.join(roms, "Pokemon.gb"), b"gb")
    _make_file(os.path.join(roms, "Mario.nes"), b"nes")
    _make_file(os.path.join(roms, "Sonic.gg"), b"gg")
    _make_file(os.path.join(roms, "Zelda.gbc"), b"gbc")
    _make_file(os.path.join(roms, "notes.txt"), b"ignore")
    # A metadata touch represents a ROM newly added to the managed folder.
    os.utime(os.path.join(roms, "Pokemon.gb"), None)

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile = store.save_profile(profile)

    service = RockboxGameService()
    games = service.list_games(profile)

    assert games[0]["filename"] == "Pokemon.gb"
    assert {game["filename"] for game in games} == {"Mario.nes", "Pokemon.gb", "Sonic.gg", "Zelda.gbc"}
    assert all(game["added_time"] > 0 for game in games)
    assert {game["platform"] for game in games} == {"Game Boy", "Game Boy Color", "NES", "Game Gear"}


def test_snes_rom_metadata_and_sync_bundle(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    rom = bytearray(0x10000)
    header = 0xFFC0
    rom[header:header + 21] = b"KILLER INSTINCT      "
    rom[header + 0x15] = 0x31
    rom[header + 0x19] = 0x01
    rom[header + 0x1C:header + 0x1E] = (0xEDCB).to_bytes(2, "little")
    rom[header + 0x1E:header + 0x20] = (0x1234).to_bytes(2, "little")
    rom[header + 0x3D] = 0x80
    _make_file(os.path.join(roms, "Killer Instinct.sfc"), bytes(rom))
    _make_file(
        os.path.join(repo_root, "build-hw-ipod6g", "apps", "plugins", "snes_lite", "snes_lite.rock"),
        b"plugin",
    )
    os.makedirs(device, exist_ok=True)

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    games = service.list_games(profile)
    assert len(games) == 1
    assert games[0]["platform"] == "Super Nintendo"
    assert games[0]["internal_title"] == "KILLER INSTINCT"
    assert games[0]["mapper"] == "HiROM"
    assert games[0]["special_chip"] == ""
    assert len(games[0]["rom_hash"]) == 64

    bundle = service.build_sync_bundle(profile, games, "device")
    destinations = {asset["destination_rel"] for asset in bundle["assets"]}
    assert ".rockbox/roms/snes/Killer Instinct.sfc" in destinations
    assert ".rockbox/rocks/games/snes_lite.rock" in destinations
    assert ".rockbox/config/snes_lite.cfg" in destinations
    assert ".rockbox/config/snes_lite/Killer Instinct.cfg" in destinations
    assert ".rockbox/games/snes/games.tsv" in destinations
    assert ".rockbox/games/library/systems.tsv" in destinations
    assert ".rockbox/rocks/plugin.dat" in destinations
    game_config = next(
        asset for asset in bundle["assets"]
        if asset["destination_rel"] == ".rockbox/config/snes_lite/Killer Instinct.cfg"
    )
    with open(game_config["source_abs"], "r", encoding="utf-8") as handle:
        game_config_text = handle.read()
    assert "# type=fighting" in game_config_text
    assert "input_profile=3" in game_config_text
    assert "audio=auto" in game_config_text


def test_snes_control_recommendations_cover_ipod_profiles():
    service = RockboxGameService()

    expected = {
        "Donkey Kong Country 3": ("platformer", 0),
        "Secret of Mana": ("action", 2),
        "Super Metroid": ("action", 2),
        "Killer Instinct": ("fighting", 3),
        "Super Mario Kart": ("racing", 4),
        "NHL '94": ("sports", 5),
    }
    for title, (game_type, profile) in expected.items():
        recommendation = service.recommend_snes_controls(title)
        assert recommendation["game_type"] == game_type
        assert recommendation["input_profile"] == profile


def test_snes_special_chip_and_save_backup(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    rom = bytearray(0x8000)
    header = 0x7FC0
    rom[header:header + 21] = b"SUPER MARIO RPG      "
    rom[header + 0x15] = 0x23
    rom[header + 0x16] = 0x34
    rom[header + 0x1C:header + 0x1E] = (0xEDCB).to_bytes(2, "little")
    rom[header + 0x1E:header + 0x20] = (0x1234).to_bytes(2, "little")
    rom[header + 0x3D] = 0x80
    _make_file(os.path.join(roms, "Super Mario RPG.sfc"), bytes(rom))
    _make_file(os.path.join(device, ".rockbox", "saves", "snes", "Super Mario RPG.srm"), b"sram")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    games = service.list_games(profile)
    assert games[0]["special_chip"] == "SA-1"
    assert games[0]["compatibility"] == "Unsupported special chip"
    assert games[0]["device_save_exists"] is True
    backup = service.backup_saves(profile, games, "device")
    assert backup["success"] is True
    assert backup["copied"][0].endswith(os.path.join("snes", "Super Mario RPG.srm"))


def test_profiles_default_games_library_to_documents_gameboy(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()

    expected = str(Path.home() / "Documents" / "Gameboy")
    assert config.get("games_library_path") == expected
    assert profile["games_library_path"] == expected


def test_game_deploy_diff_and_repeat_apply_device(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(roms, "Tetris.gb"), b"rom")
    os.makedirs(device, exist_ok=True)

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    deploy = RockboxDeployService()
    games = service.list_games(profile)
    deploy_profile = service.deploy_profile(profile, "device")
    bundle = service.build_sync_bundle(profile, games, "device")
    diff = deploy.build_diff(deploy_profile, bundle)

    assert diff["summary"]["add"] == 2
    assert diff["items"][0]["destination_abs"].startswith(os.path.join(device, "gameboy"))

    result = deploy.apply_diff(deploy_profile, diff)
    assert result["success"] is True

    diff2 = deploy.build_diff(deploy_profile, bundle)
    assert diff2["summary"]["unchanged"] == 2
    assert diff2["summary"]["add"] == 0


def test_game_deploy_and_remove_simulator_preserve_unrelated_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    simdisk = os.path.join(tmp_dir, "build-sim", "simdisk")
    _make_file(os.path.join(roms, "Kirby.gbc"), b"rom")
    _make_file(os.path.join(simdisk, ".rockbox", "themes", "keep.cfg"), "leave alone\n")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["simulator_simdisk_path"] = simdisk
    profile["simulator_target"] = "build-sim"
    profile = store.save_profile(profile)

    simulator_target = {"id": "build-sim", "simdisk_path": simdisk}
    service = RockboxGameService()
    deploy = RockboxDeployService()
    games = service.list_games(profile, simulator_target)
    deploy_profile = service.deploy_profile(profile, "simulator", simulator_target)
    sync_diff = deploy.build_diff(deploy_profile, service.build_sync_bundle(profile, games, "simulator", simulator_target))
    result = deploy.apply_diff(deploy_profile, sync_diff)
    assert result["success"] is True

    remove_diff = deploy.build_diff(deploy_profile, service.build_remove_bundle(profile, games, "simulator"))
    assert remove_diff["summary"]["remove"] == 2
    removed = deploy.apply_diff(deploy_profile, remove_diff)
    assert removed["success"] is True

    with open(os.path.join(simdisk, ".rockbox", "themes", "keep.cfg"), "r", encoding="utf-8") as handle:
        assert handle.read() == "leave alone\n"


def test_deleted_local_game_remains_removable_from_device_library(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    rom_path = os.path.join(roms, "Tetris.gb")
    os.makedirs(device, exist_ok=True)
    _make_file(rom_path, b"rom")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    deploy = RockboxDeployService()
    deploy_profile = service.deploy_profile(profile, "device")

    sync_games = service.list_games(profile)
    sync_diff = deploy.build_diff(deploy_profile, service.build_sync_bundle(profile, sync_games, "device"))
    applied = deploy.apply_diff(deploy_profile, sync_diff)
    assert applied["success"] is True
    assert os.path.isfile(os.path.join(device, "gameboy", "Tetris.gb"))

    os.remove(rom_path)

    stale_games = service.list_games(profile)
    assert len(stale_games) == 1
    assert stale_games[0]["filename"] == "Tetris.gb"
    assert stale_games[0]["missing_source"] is True
    assert stale_games[0]["on_device"] is True

    remove_diff = deploy.build_diff(deploy_profile, service.build_remove_bundle(profile, stale_games, "device"))
    removed = deploy.apply_diff(deploy_profile, remove_diff)
    assert removed["success"] is True
    assert not os.path.exists(os.path.join(device, "gameboy", "Tetris.gb"))
    assert not os.path.exists(os.path.join(device, ".rockbox", "rocks", "games", "rockboy_launcher", "games.tsv"))


def test_game_save_backup_behavior(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(roms, "Metroid.gb"), b"rom")
    _make_file(os.path.join(device, ".rockbox", "rockboy", "Metroid.sav"), b"save")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    games = service.list_games(profile)
    result = service.backup_saves(profile, games, "device")

    assert result["success"] is True
    assert len(result["copied"]) == 1
    assert os.path.isfile(result["copied"][0])


def test_game_save_restore_export_import(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(roms, "Metroid.gb"), b"rom")
    save_path = os.path.join(device, ".rockbox", "rockboy", "Metroid.sav")
    _make_file(save_path, b"save-v1")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    games = service.list_games(profile)
    backup = service.backup_saves(profile, games, "device")
    assert backup["success"] is True

    _make_file(save_path, b"save-v2")
    restored = service.restore_saves(profile, games, "device")
    assert restored["success"] is True
    with open(save_path, "rb") as handle:
        assert handle.read() == b"save-v1"

    archive = os.path.join(tmp_dir, "exports", "saves.zip")
    exported = service.export_save_bundle(profile, games, "device", archive_path=archive)
    assert exported["success"] is True
    assert os.path.isfile(archive)

    _make_file(save_path, b"old")
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
        bundle.writestr("Metroid.sav", b"save-import")
    imported = service.import_save_bundle(profile, "device", archive_path=archive)
    assert imported["success"] is True
    with open(save_path, "rb") as handle:
        assert handle.read() == b"save-import"


def test_game_cover_prefers_local_sidecar(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    rom_path = os.path.join(roms, "Tetris.gb")
    cover_path = os.path.join(roms, "Tetris.png")
    _make_file(rom_path, b"rom")
    _make_file(cover_path, b"png")

    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile = store.save_profile(profile)

    service = RockboxGameService()
    games = service.list_games(profile, config=config)

    assert games[0]["cover_path"] == cover_path


def test_game_cover_fetches_from_libretro(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    _make_file(os.path.join(roms, "Pokemon Blue (USA, Europe).gb"), b"rom")

    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile = store.save_profile(profile)

    service = RockboxGameService()
    games = service.list_games(profile, config=config)

    class _Response:
        def __init__(self, data):
            self._data = data

        def read(self):
            return self._data

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

    requested = []

    def fake_open(request, timeout=10):
        requested.append(request.full_url)
        return _Response(b"png-data")

    result = service.fetch_cover_for_game(games[0], config, opener=fake_open)

    assert result["success"] is True
    assert os.path.isfile(result["path"])
    assert requested[0].endswith("Pokemon%20Blue%20%28USA%2C%20Europe%29.png")


def test_game_cover_fetches_nes_boxart_from_libretro(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    _make_file(os.path.join(roms, "Super Mario Bros.nes"), b"rom")

    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile = store.save_profile(profile)

    service = RockboxGameService()
    games = service.list_games(profile, config=config)

    class _Response:
        def __init__(self, data):
            self._data = data

        def read(self):
            return self._data

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

    requested = []

    def fake_open(request, timeout=10):
        requested.append(request.full_url)
        return _Response(b"png-data")

    result = service.fetch_cover_for_game(games[0], config, opener=fake_open)

    assert result["success"] is True
    assert os.path.isfile(result["path"])
    assert "/Nintendo%20-%20Nintendo%20Entertainment%20System/" in requested[0]
    assert requested[0].endswith("Super%20Mario%20Bros.png")


def test_launcher_index_parsing_handles_relative_paths_and_favorites(tmp_dir):
    index_root = os.path.join(tmp_dir, "launcher-index")
    index_path = os.path.join(index_root, "games.tsv")
    _make_file(os.path.join(index_root, "roms", "Tetris.gb"), b"rom")
    _make_file(os.path.join(index_root, "covers", "Tetris.jpg"), b"jpg")
    _make_file(os.path.join(index_root, "roms", "Kirby.gbc"), b"rom")
    _make_file(os.path.join(index_root, "roms", "Super Mario Bros.nes"), b"rom")
    _make_file(
        index_path,
        "\n".join(
            [
                "# title\trom_path\tcover_path\tfavorite\tsave_hint",
                "Tetris\troms/Tetris.gb\tcovers/Tetris.jpg\tfavorite\tyes\t1989\tPuzzle video game\tNintendo\tNintendo\tTile puzzle classic",
                "Kirby\troms/Kirby.gbc\t\t0\tno\t1995\tPlatform game\tNintendo\tHAL Laboratory\tPink puffball platformer",
                "Super Mario Bros\troms/Super Mario Bros.nes\t\t0\tno\t1985\tPlatform game\tNintendo\tNintendo\tNES platformer",
                "Ignore Me\troms/readme.txt\t\t1\tyes",
            ]
        ),
    )

    service = RockboxGameService()
    entries = service.parse_launcher_index(index_path)

    assert [entry["title"] for entry in entries] == ["Kirby", "Super Mario Bros", "Tetris"]
    assert entries[0]["rom_path"] == os.path.join(index_root, "roms", "Kirby.gbc")
    assert entries[0]["cover_path"] == ""
    assert entries[0]["favorite"] is False
    assert entries[0]["year"] == "1995"
    assert entries[0]["genre"] == "Platform game"
    assert entries[0]["publisher"] == "Nintendo"
    assert entries[1]["rom_path"] == os.path.join(index_root, "roms", "Super Mario Bros.nes")
    assert entries[2]["cover_path"] == os.path.join(index_root, "covers", "Tetris.jpg")
    assert entries[2]["favorite"] is True
    assert entries[2]["save_hint"] == "yes"
    assert entries[2]["description"] == "Tile puzzle classic"


def test_launcher_index_ignores_missing_rom_entries(tmp_dir):
    index_root = os.path.join(tmp_dir, "launcher-index")
    index_path = os.path.join(index_root, "games.tsv")
    _make_file(os.path.join(index_root, "roms", "Pokemon.gb"), b"rom")
    _make_file(
        index_path,
        "\n".join(
            [
                "# title\trom_path\tcover_path\tfavorite\tsave_hint",
                "Removed\troms/Removed.gb\t\t0\t",
                "Pokemon\troms/Pokemon.gb\t\t1\t",
            ]
        ),
    )

    service = RockboxGameService()
    entries = service.parse_launcher_index(index_path)

    assert [entry["title"] for entry in entries] == ["Pokemon"]


def test_launcher_library_falls_back_to_scan_when_index_only_has_stale_entries(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    device = os.path.join(tmp_dir, "device")
    index_path = os.path.join(device, ".rockbox", "rocks", "games", "rockboy_launcher", "games.tsv")
    _make_file(os.path.join(device, "gameboy", "Pokemon.gb"), b"rom")
    _make_file(os.path.join(device, "gameboy", "Pokemon.jpg"), b"jpg")
    _make_file(
        index_path,
        "\n".join(
            [
                "# title\trom_path\tcover_path\tfavorite\tsave_hint",
                "Removed\t/gameboy/Removed.gb\t\t0\t",
            ]
        ),
    )

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    entries = service.launcher_library(profile, "device")

    assert [entry["title"] for entry in entries] == ["Pokemon"]
    assert entries[0]["rom_path"] == os.path.join(device, "gameboy", "Pokemon.gb")
    assert entries[0]["cover_path"] == os.path.join(device, "gameboy", "Pokemon.jpg")


def test_game_metadata_fetch_is_cached_and_written_to_launcher_index(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(roms, "Pokemon Blue.gb"), b"rom")
    os.makedirs(device, exist_ok=True)

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    deploy = RockboxDeployService()
    games = service.list_games(profile)

    class _Lookup:
        def fetch_game_metadata(self, title, platform_hint="", opener=None):
            assert title == "Pokemon Blue"
            assert platform_hint == ".gb"
            return {
                "year": "1996",
                "genre": "Role-playing video game",
                "publisher": "Nintendo",
                "developer": "Game Freak",
                "description": "Monster-collecting RPG",
                "source": "test",
                "source_url": "https://example.com/game",
                "entity_id": "Q1",
            }

    fetched = service.fetch_metadata_for_game(profile, games[0], lookup_client=_Lookup())
    assert fetched["success"] is True
    cache_dir = os.path.join(repo_root, "rockpod", ".generated", "game_metadata", profile["id"])
    assert not _temp_names(cache_dir)
    assert service.list_games(profile)[0]["publisher"] == "Nintendo"

    deploy_profile = service.deploy_profile(profile, "device")
    sync_diff = deploy.build_diff(deploy_profile, service.build_sync_bundle(profile, games, "device"))
    applied = deploy.apply_diff(deploy_profile, sync_diff)
    assert applied["success"] is True

    index_entries = service.parse_launcher_index(os.path.join(device, ".rockbox", "rocks", "games", "rockboy_launcher", "games.tsv"))
    assert len(index_entries) == 1
    assert index_entries[0]["year"] == "1996"
    assert index_entries[0]["genre"] == "Role-playing video game"
    assert index_entries[0]["publisher"] == "Nintendo"
    stage_dir = os.path.join(repo_root, "rockpod", ".generated", "games", profile["id"], "device")
    assert not _temp_names(stage_dir)


def test_nes_metadata_fetch_uses_nes_platform_hint(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    _make_file(os.path.join(roms, "Super Mario Bros.nes"), b"rom")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile = store.save_profile(profile)

    service = RockboxGameService()
    games = service.list_games(profile)

    class _Lookup:
        def fetch_game_metadata(self, title, platform_hint="", opener=None):
            assert title == "Super Mario Bros"
            assert platform_hint == ".nes"
            return {
                "year": "1985",
                "genre": "Platform game",
                "publisher": "Nintendo",
                "developer": "Nintendo",
                "description": "NES platformer",
                "platform": "Nintendo Entertainment System",
                "source": "test",
                "source_url": "https://example.com/game",
                "entity_id": "Q2",
            }

    fetched = service.fetch_metadata_for_game(profile, games[0], lookup_client=_Lookup())

    assert fetched["success"] is True
    assert fetched["metadata"]["platform"] == "Nintendo Entertainment System"


def test_launcher_library_falls_back_to_gameboy_scan(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(device, "gameboy", "Pokemon.gb"), b"rom")
    _make_file(os.path.join(device, "gameboy", "Pokemon.jpg"), b"jpg")
    _make_file(os.path.join(device, "gameboy", "RPG", "Zelda.gbc"), b"rom")
    _make_file(os.path.join(device, "gameboy", "Super Mario Bros.nes"), b"rom")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    entries = service.launcher_library(profile, "device")

    assert [entry["title"] for entry in entries] == ["Pokemon", "Super Mario Bros", "Zelda"]
    assert entries[0]["cover_path"] == os.path.join(device, "gameboy", "Pokemon.jpg")
    assert entries[1]["cover_path"] == ""


def test_game_save_detection_includes_sav_rtc_and_sn(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(roms, "Battery.gb"), b"rom")
    _make_file(os.path.join(roms, "Clock.gbc"), b"rom")
    _make_file(os.path.join(roms, "State.gb"), b"rom")
    _make_file(os.path.join(roms, "Empty.gb"), b"rom")
    _make_file(os.path.join(device, ".rockbox", "rockboy", "Battery.sav"), b"save")
    _make_file(os.path.join(device, ".rockbox", "rockboy", "Clock.rtc"), b"rtc")
    _make_file(os.path.join(device, ".rockbox", "rockboy", "State.sn"), b"state")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    saves = {game["filename"]: game["device_save_exists"] for game in service.list_games(profile)}

    assert saves["Battery.gb"] is True
    assert saves["Clock.gbc"] is True
    assert saves["State.gb"] is True
    assert saves["Empty.gb"] is False


def test_game_cover_path_falls_back_to_empty_without_sidecar_or_cache(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    rom_path = os.path.join(roms, "NoCover.gb")
    _make_file(rom_path, b"rom")

    config, _store = _make_store(tmp_dir, repo_root)
    service = RockboxGameService()

    assert service.cover_path_for_game({"filename": "NoCover.gb", "source_path": rom_path}, config=config) == ""


def test_game_sync_bundle_stages_cover_sidecars_for_launcher_scan(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    rom_path = os.path.join(roms, "Tetris.gb")
    cover_path = os.path.join(roms, "Tetris.png")
    _make_file(rom_path, b"rom")

    from PIL import Image
    os.makedirs(roms, exist_ok=True)
    Image.new("RGB", (512, 512), color="blue").save(cover_path, format="PNG")
    os.makedirs(device, exist_ok=True)

    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    deploy = RockboxDeployService()
    games = service.list_games(profile, config=config)
    bundle = service.build_sync_bundle(profile, games, "device")
    cover_assets = [asset for asset in bundle["assets"] if asset["kind"] == "cover"]

    assert len(cover_assets) == 1
    assert cover_assets[0]["destination_rel"] == "gameboy/Tetris.bmp"
    assert os.path.isfile(cover_assets[0]["source_abs"])

    diff = deploy.build_diff(service.deploy_profile(profile, "device"), bundle)
    result = deploy.apply_diff(service.deploy_profile(profile, "device"), diff)

    assert result["success"] is True
    assert os.path.isfile(os.path.join(device, "gameboy", "Tetris.bmp"))


def test_nes_game_sync_writes_device_system_manifest(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    rom_path = os.path.join(roms, "Super Mario Bros.nes")
    cover_path = os.path.join(roms, "Super Mario Bros.png")
    _make_file(rom_path, b"rom")

    from PIL import Image
    os.makedirs(roms, exist_ok=True)
    Image.new("RGB", (256, 256), color="red").save(cover_path, format="PNG")
    os.makedirs(device, exist_ok=True)

    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    deploy = RockboxDeployService()
    games = service.list_games(profile, config=config)
    bundle = service.build_sync_bundle(profile, games, "device")

    assert any(
        asset["destination_rel"] == ".rockbox/games/nes/games.tsv"
        for asset in bundle["assets"]
    )

    diff = deploy.build_diff(service.deploy_profile(profile, "device"), bundle)
    result = deploy.apply_diff(service.deploy_profile(profile, "device"), diff)

    assert result["success"] is True
    manifest_path = os.path.join(device, ".rockbox", "games", "nes", "games.tsv")
    entries = service.parse_system_manifest(manifest_path)
    assert entries[0]["title"] == "Super Mario Bros"
    assert entries[0]["file_path"] == os.path.join(device, "gameboy", "Super Mario Bros.nes")
    assert entries[0]["cover_path"] == os.path.join(device, "gameboy", "Super Mario Bros.bmp")


def test_smsgg_game_sync_uses_plugin_launcher_entry(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    rom_path = os.path.join(roms, "Sonic The Hedgehog 2 (World).gg")
    cover_path = os.path.join(roms, "Sonic The Hedgehog 2 (World).png")
    _make_file(rom_path, b"rom")

    from PIL import Image
    os.makedirs(roms, exist_ok=True)
    Image.new("RGB", (256, 256), color="blue").save(cover_path, format="PNG")
    _make_file(os.path.join(device, ".rockbox", "rocks", "games", "smsgg.rock"), b"plugin")

    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    deploy = RockboxDeployService()
    games = service.list_games(profile, config=config)
    bundle = service.build_sync_bundle(profile, games, "device")

    assert any(
        asset["destination_rel"] == ".rockbox/games/smsgg/roms/Sonic The Hedgehog 2 (World).gg"
        for asset in bundle["assets"]
    )

    diff = deploy.build_diff(service.deploy_profile(profile, "device"), bundle)
    result = deploy.apply_diff(service.deploy_profile(profile, "device"), diff)

    assert result["success"] is True
    index_path = os.path.join(device, ".rockbox", "rocks", "games", "rockboy_launcher", "games.tsv")
    entries = service.parse_launcher_index(index_path)
    assert entries[0]["rom_path"] == os.path.join(device, ".rockbox", "rocks", "games", "smsgg.rock")
    assert entries[0]["plugin_param"] == os.path.join(device, ".rockbox", "games", "smsgg", "roms", "Sonic The Hedgehog 2 (World).gg")
    manifest_path = os.path.join(device, ".rockbox", "games", "smsgg", "games.tsv")
    system_entries = service.parse_system_manifest(manifest_path)
    assert system_entries[0]["title"] == "Sonic The Hedgehog 2 (World)"
    assert system_entries[0]["file_path"] == os.path.join(device, ".rockbox", "games", "smsgg", "roms", "Sonic The Hedgehog 2 (World).gg")
    assert system_entries[0]["cover_path"] == os.path.join(device, ".rockbox", "games", "smsgg", "roms", "Sonic The Hedgehog 2 (World).bmp")


def test_gwatch_game_sync_uses_console_manifest_and_plugin_launcher_entry(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    rom_path = os.path.join(roms, "Parachute (Nintendo, Wide Screen).mgw")
    cover_path = os.path.join(roms, "Parachute (Nintendo, Wide Screen).png")
    _make_file(rom_path, b"mgw")

    from PIL import Image
    os.makedirs(roms, exist_ok=True)
    Image.new("RGB", (256, 256), color="white").save(cover_path, format="PNG")
    _make_file(os.path.join(device, ".rockbox", "rocks", "games", "gwatch.rock"), b"plugin")

    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    deploy = RockboxDeployService()
    games = service.list_games(profile, config=config)
    bundle = service.build_sync_bundle(profile, games, "device")

    assert [game["filename"] for game in games] == ["Parachute (Nintendo, Wide Screen).mgw"]
    assert any(
        asset["destination_rel"] == ".rockbox/games/gwatch/roms/Parachute (Nintendo, Wide Screen).mgw"
        for asset in bundle["assets"]
    )
    assert any(
        asset["destination_rel"] == ".rockbox/games/gwatch/games.tsv"
        for asset in bundle["assets"]
    )

    diff = deploy.build_diff(service.deploy_profile(profile, "device"), bundle)
    result = deploy.apply_diff(service.deploy_profile(profile, "device"), diff)

    assert result["success"] is True
    index_path = os.path.join(device, ".rockbox", "rocks", "games", "rockboy_launcher", "games.tsv")
    entries = service.parse_launcher_index(index_path)
    assert entries[0]["rom_path"] == os.path.join(device, ".rockbox", "rocks", "games", "gwatch.rock")
    assert entries[0]["plugin_param"] == os.path.join(device, ".rockbox", "games", "gwatch", "roms", "Parachute (Nintendo, Wide Screen).mgw")

    manifest_path = os.path.join(device, ".rockbox", "games", "gwatch", "games.tsv")
    system_entries = service.parse_system_manifest(manifest_path)
    assert system_entries[0]["title"] == "Parachute (Nintendo, Wide Screen)"
    assert system_entries[0]["file_path"] == os.path.join(device, ".rockbox", "games", "gwatch", "roms", "Parachute (Nintendo, Wide Screen).mgw")
    assert system_entries[0]["cover_path"] == os.path.join(device, ".rockbox", "games", "gwatch", "roms", "Parachute (Nintendo, Wide Screen).bmp")


def test_smsgg_game_remove_deletes_rom_cover_and_index_row(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    device = os.path.join(tmp_dir, "device")
    rom_path = os.path.join(roms, "Columns.sms")
    _make_file(rom_path, b"rom")
    _make_file(os.path.join(device, ".rockbox", "rocks", "games", "smsgg.rock"), b"plugin")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxGameService()
    deploy = RockboxDeployService()
    games = service.list_games(profile)
    deploy_profile = service.deploy_profile(profile, "device")
    deploy.apply_diff(deploy_profile, deploy.build_diff(deploy_profile, service.build_sync_bundle(profile, games, "device")))

    assert os.path.isfile(os.path.join(device, ".rockbox", "games", "smsgg", "roms", "Columns.sms"))

    remove_diff = deploy.build_diff(deploy_profile, service.build_remove_bundle(profile, games, "device"))
    result = deploy.apply_diff(deploy_profile, remove_diff)

    assert result["success"] is True
    assert not os.path.exists(os.path.join(device, ".rockbox", "games", "smsgg", "roms", "Columns.sms"))
    assert not os.path.exists(os.path.join(device, ".rockbox", "rocks", "games", "rockboy_launcher", "games.tsv"))
    assert not os.path.exists(os.path.join(device, ".rockbox", "games", "smsgg", "games.tsv"))


def test_launcher_config_bundle_controls_builtin_games(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    device = os.path.join(tmp_dir, "device")
    os.makedirs(device, exist_ok=True)

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["device_mount_path"] = device
    profile["games_show_builtin_doom"] = False
    profile["games_show_builtin_stickrpg"] = True
    profile["games_show_builtin_runescape"] = False
    profile = store.save_profile(profile)

    service = RockboxGameService()
    deploy = RockboxDeployService()
    bundle = service.build_launcher_config_bundle(profile, "device")
    diff = deploy.build_diff(service.deploy_profile(profile, "device"), bundle)
    result = deploy.apply_diff(service.deploy_profile(profile, "device"), diff)

    assert result["success"] is True
    with open(os.path.join(device, ".rockbox", "rocks", "games", "rockboy_launcher", "config.cfg"), "r", encoding="utf-8") as handle:
        text = handle.read()
    assert "show_builtin_doom=0" in text
    assert "show_builtin_stickrpg=1" in text
    assert "show_builtin_runescape=0" in text


def test_game_performance_and_cover_optimization_report(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    roms = os.path.join(tmp_dir, "roms")
    rom_path = os.path.join(roms, "BigGame.gbc")
    cover_path = os.path.join(roms, "BigGame.png")
    _make_file(rom_path, b"x" * (2 * 1024 * 1024))

    from PIL import Image
    os.makedirs(roms, exist_ok=True)
    image = Image.new("RGB", (800, 800), color="red")
    image.save(cover_path, format="PNG")

    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["games_library_path"] = roms
    profile = store.save_profile(profile)

    service = RockboxGameService()
    game = service.list_games(profile, config=config)[0]

    assert game["performance"]["level"] in {"warn", "critical"}
    assert game["performance"]["cover"]["oversized"] is True

    optimized = service.optimize_cover_asset(game)
    assert optimized["success"] is True

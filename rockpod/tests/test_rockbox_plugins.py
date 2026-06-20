import os

from app.config import Config
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_plugins import RockboxPluginService
from services.rockbox_profiles import RockboxProfileStore


def _make_file(path, content="x"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    mode = "wb" if isinstance(content, bytes) else "w"
    with open(path, mode) as handle:
        handle.write(content)


def _make_store(tmp_dir, repo_root):
    config = Config(os.path.join(tmp_dir, "plugins-config.json"))
    return config, RockboxProfileStore(config, repo_root)


def _make_repo(repo_root):
    _make_file(
        os.path.join(repo_root, "apps", "plugins", "CATEGORIES"),
        "pocketcatch,games\npocketcatch_simple,games\nminishcap,games\nclock,apps\n",
    )
    _make_file(os.path.join(repo_root, "apps", "plugins", "pocketcatch.c"), "/* pocketcatch */\n")
    _make_file(os.path.join(repo_root, "apps", "plugins", "pocketcatch_simple.c"), "/* pocketcatch simple */\n")
    _make_file(os.path.join(repo_root, "apps", "plugins", "minishcap.c"), "/* minishcap */\n")
    _make_file(os.path.join(repo_root, "apps", "plugins", "clock", "clock.c"), "/* clock */\n")
    _make_file(os.path.join(repo_root, "apps", "plugins", "POCKETCATCH_PLAN.md"), "# plan\n")
    _make_file(os.path.join(repo_root, "apps", "plugins", "POCKETCATCH_ASSET_PACK_SPEC.md"), "# spec\n")
    _make_file(os.path.join(repo_root, "apps", "plugins", "MINISHCAP_USED_ASSETS.md"), "# assets\n")
    _make_file(os.path.join(repo_root, "apps", "plugins", "minishcap_smith_real.bmp"), b"smith")
    _make_file(os.path.join(repo_root, "apps", "plugins", "minishcap_zelda_real.bmp"), b"zelda")
    _make_file(os.path.join(repo_root, "apps", "plugins", "minishcap_south_hyrule_full.1008x688x24.bmp"), b"map")


def _make_build_outputs(repo_root):
    for build_dir in ("build-hw-ipod6g", "build-hw-ipodvideo-5g", "build-sim-video-5g"):
        for plugin_id in ("pocketcatch", "pocketcatch_simple", "minishcap"):
            _make_file(
                os.path.join(repo_root, build_dir, "apps", "plugins", f"{plugin_id}.rock"),
                f"{build_dir}:{plugin_id}\n",
            )
    os.makedirs(os.path.join(repo_root, "build-sim-video-5g", "simdisk"), exist_ok=True)


def test_plugin_discovery_and_custom_priority(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    _make_build_outputs(repo_root)
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["target_device_model"] = "iPod Classic / Video"
    profile = store.save_profile(profile)
    simulator_target = {"id": "build-sim-video-5g", "build_dir": os.path.join(repo_root, "build-sim-video-5g")}

    service = RockboxPluginService()
    plugins = service.list_plugins(repo_root, profile, simulator_target, "device")

    assert plugins[0]["id"] in {"minishcap", "pocketcatch", "pocketcatch_simple"}
    pocket = next(item for item in plugins if item["id"] == "pocketcatch")
    assert pocket["display_name"] == "Podemon Go"
    assert pocket["status"] == "experimental"
    assert pocket["category"] == "games"
    assert pocket["binary_exists"] is True
    assert pocket["destination_rel"] == ".rockbox/rocks/games/pocketcatch.rock"


def test_pocketcatch_deploy_bundle_preserves_asset_pack_tree(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    _make_build_outputs(repo_root)
    _make_file(os.path.join(repo_root, "rockpod", "assets", "pocketcatch", "pack.json"), "{}")
    _make_file(
        os.path.join(repo_root, "rockpod", "assets", "pocketcatch", "sprites", "creatures", "creature_001_idle_0.bmp"),
        b"creature",
    )
    _make_file(
        os.path.join(repo_root, "rockpod", "assets", "pocketcatch", "sprites", "balls", "ball_default_idle_0.bmp"),
        b"ball",
    )
    _make_file(
        os.path.join(repo_root, "rockpod", "assets", "pocketcatch", "backgrounds", "new_bark_town_hgss.bmp"),
        b"background",
    )
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["target_device_model"] = "iPod Classic / Video"
    profile = store.save_profile(profile)

    service = RockboxPluginService()
    metadata = service.plugin_details(repo_root, "pocketcatch", profile, None, "device")
    bundle = service.build_deploy_bundle(metadata)
    destinations = {item["destination_rel"] for item in bundle["assets"]}

    assert ".rockbox/rocks/games/pocketcatch.rock" in destinations
    assert ".rockbox/rocks/games/pocketcatch/pack.json" in destinations
    assert ".rockbox/rocks/games/pocketcatch/sprites/creatures/creature_001_idle_0.bmp" in destinations
    assert ".rockbox/rocks/games/pocketcatch/sprites/balls/ball_default_idle_0.bmp" in destinations
    assert ".rockbox/rocks/games/pocketcatch/backgrounds/new_bark_town_hgss.bmp" in destinations
    assert ".rockbox/rocks/games/pocketcatch/backgrounds/scene_day_layer0.bmp" in destinations


def test_plugin_deploy_diff_device_and_repeat_apply(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    _make_build_outputs(repo_root)
    _config, store = _make_store(tmp_dir, repo_root)
    device_mount = os.path.join(tmp_dir, "device")
    os.makedirs(device_mount, exist_ok=True)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = device_mount
    profile["target_device_model"] = "iPod Classic / Video"
    profile = store.save_profile(profile)

    service = RockboxPluginService()
    deploy = RockboxDeployService()
    metadata = service.plugin_details(repo_root, "pocketcatch", profile, None, "device")
    diff = deploy.build_diff(service.deploy_profile(profile, "device"), service.build_deploy_bundle(metadata))

    assert diff["summary"]["add"] == 1
    assert all(item["destination_rel"].startswith(".rockbox/rocks/") for item in diff["items"])

    result = deploy.apply_diff(service.deploy_profile(profile, "device"), diff)
    assert result["success"] is True

    diff2 = deploy.build_diff(service.deploy_profile(profile, "device"), service.build_deploy_bundle(metadata))
    assert diff2["summary"]["unchanged"] == 1
    assert diff2["summary"]["add"] == 0


def test_plugin_simulator_target_handling_and_asset_bundle(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    _make_build_outputs(repo_root)
    _config, store = _make_store(tmp_dir, repo_root)
    simdisk = os.path.join(repo_root, "build-sim-video-5g", "simdisk")
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["simulator_simdisk_path"] = simdisk
    profile["simulator_target"] = "build-sim-video-5g"
    profile = store.save_profile(profile)
    simulator_target = {"id": "build-sim-video-5g", "build_dir": os.path.join(repo_root, "build-sim-video-5g"), "simdisk_path": simdisk}

    service = RockboxPluginService()
    deploy = RockboxDeployService()
    metadata = service.plugin_details(repo_root, "minishcap", profile, simulator_target, "simulator")
    bundle = service.build_deploy_bundle(metadata)
    diff = deploy.build_diff(service.deploy_profile(profile, "simulator", simulator_target), bundle)

    assert metadata["simulator_supported"] is True
    assert len(bundle["assets"]) == 4
    assert all(item["destination_abs"].startswith(os.path.join(simdisk, ".rockbox", "rocks")) for item in diff["items"])


def test_plugin_remove_and_rollback_preserve_unrelated_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    _make_build_outputs(repo_root)
    _config, store = _make_store(tmp_dir, repo_root)
    device_mount = os.path.join(tmp_dir, "device")
    plugin_dest = os.path.join(device_mount, ".rockbox", "rocks", "games", "minishcap.rock")
    asset_dest = os.path.join(device_mount, ".rockbox", "rocks", "games", "minishcap_smith_real.bmp")
    unrelated = os.path.join(device_mount, ".rockbox", "themes", "keep.cfg")
    _make_file(plugin_dest, "old plugin\n")
    _make_file(asset_dest, "old asset\n")
    _make_file(unrelated, "leave alone\n")

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = device_mount
    profile["target_device_model"] = "iPod Classic / Video"
    profile = store.save_profile(profile)

    service = RockboxPluginService()
    deploy = RockboxDeployService()
    metadata = service.plugin_details(repo_root, "minishcap", profile, None, "device")
    remove_bundle = service.build_remove_bundle(metadata)
    remove_diff = deploy.build_diff(service.deploy_profile(profile, "device"), remove_bundle)

    assert remove_diff["summary"]["remove"] >= 2

    result = deploy.apply_diff(service.deploy_profile(profile, "device"), remove_diff)
    assert result["success"] is True
    assert not os.path.exists(plugin_dest)
    assert not os.path.exists(asset_dest)

    restore = deploy.restore_latest_backup(service.deploy_profile(profile, "device"))
    assert restore["success"] is True
    with open(plugin_dest, "r", encoding="utf-8") as handle:
        assert handle.read() == "old plugin\n"
    with open(asset_dest, "r", encoding="utf-8") as handle:
        assert handle.read() == "old asset\n"
    with open(unrelated, "r", encoding="utf-8") as handle:
        assert handle.read() == "leave alone\n"

import os

from app.config import Config
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_profiles import RockboxProfileStore
from services.rockbox_themes import RockboxThemeService


def _repo_root():
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def _make_store(tmp_dir):
    config = Config(os.path.join(tmp_dir, "phase1-config.json"))
    return config, RockboxProfileStore(config, _repo_root())


def test_profile_persistence(tmp_dir):
    config, store = _make_store(tmp_dir)
    profile = store.current_profile()
    profile["device_mount_path"] = "/tmp/ipod-test"
    profile["selected_theme"] = "iPone"
    store.save_profile(profile)

    reloaded = RockboxProfileStore(Config(config._path), _repo_root())
    current = reloaded.current_profile()
    assert current["device_mount_path"] == "/tmp/ipod-test"
    assert current["selected_theme"] == "iPone"


def test_theme_filtering_by_resolution(tmp_dir):
    _config, store = _make_store(tmp_dir)
    service = RockboxThemeService()

    desktop = service.list_themes(_repo_root(), "320x240")
    classic3g = service.list_themes(_repo_root(), "160x128")
    nano = service.list_themes(_repo_root(), "176x132")

    assert {item["id"] for item in desktop} == {"iPone"}
    assert {item["id"] for item in classic3g} == {"Galaxy", "iPone_3g"}
    assert {item["id"] for item in nano} == {"iPone_nano2g"}


def test_default_profiles_include_ipod_3g(tmp_dir):
    _config, store = _make_store(tmp_dir)

    profiles = {item["id"]: item for item in store.profiles()}

    assert "ipod-3g" in profiles
    assert profiles["ipod-3g"]["screen_resolution"] == "160x128"
    assert profiles["ipod-3g"]["selected_theme"] == "Galaxy"


def test_deterministic_ipone_stack_contents():
    service = RockboxThemeService()
    bundle = service.bundle_for_theme("iPone", _repo_root())
    rels = {item["source_rel"] for item in bundle["assets"]}

    assert "themes/iPone.cfg" in rels
    assert "wps/iPone.wps" in rels
    assert "wps/iPone.sbs" in rels
    assert "wps/iPone.fms" in rels
    assert "backdrops/iPone_bd.bmp" in rels
    assert "icons/iPone.bmp" in rels
    assert "fonts/24 iLike.fnt" in rels
    assert any(path.startswith("wps/iPone/") for path in rels)


def test_deploy_diff_generation_and_repeat_apply(tmp_dir):
    _config, store = _make_store(tmp_dir)
    mount_path = os.path.join(tmp_dir, "device")
    os.makedirs(mount_path, exist_ok=True)
    profile = store.current_profile()
    profile["device_mount_path"] = mount_path
    profile["backup_location"] = os.path.join(tmp_dir, ".backups", profile["id"])
    profile = store.save_profile(profile)

    theme_service = RockboxThemeService()
    deploy_service = RockboxDeployService()
    bundle = theme_service.bundle_for_theme("iPone", _repo_root())

    diff = deploy_service.build_diff(profile, bundle)
    assert diff["summary"]["add"] > 0
    assert diff["summary"]["overwrite"] == 0
    assert diff["summary"]["missing_source"] == 0
    assert all(
        item["destination_rel"].startswith(".rockbox/")
        for item in diff["items"]
    )

    result = deploy_service.apply_diff(profile, diff)
    assert result["success"] is True

    diff2 = deploy_service.build_diff(profile, bundle)
    assert diff2["summary"]["unchanged"] >= diff["summary"]["add"]
    assert diff2["summary"]["add"] == 0
    assert diff2["summary"]["overwrite"] == 0


def test_backup_creation_path_and_rollback_restore(tmp_dir):
    _config, store = _make_store(tmp_dir)
    mount_path = os.path.join(tmp_dir, "device")
    os.makedirs(os.path.join(mount_path, ".rockbox", "themes"), exist_ok=True)
    os.makedirs(os.path.join(mount_path, ".rockbox", "wps"), exist_ok=True)
    os.makedirs(os.path.join(mount_path, ".rockbox", "playlists"), exist_ok=True)

    unrelated = os.path.join(mount_path, ".rockbox", "playlists", "keep.m3u8")
    with open(unrelated, "w", encoding="utf-8") as handle:
        handle.write("do not touch\n")

    existing_theme = os.path.join(mount_path, ".rockbox", "themes", "iPone.cfg")
    with open(existing_theme, "w", encoding="utf-8") as handle:
        handle.write("old theme data\n")

    profile = store.current_profile()
    profile["device_mount_path"] = mount_path
    profile["backup_location"] = os.path.join(tmp_dir, ".backups", profile["id"])
    profile = store.save_profile(profile)

    theme_service = RockboxThemeService()
    deploy_service = RockboxDeployService()
    bundle = theme_service.bundle_for_theme("iPone", _repo_root())
    diff = deploy_service.build_diff(profile, bundle)

    changed = {item["destination_rel"]: item["status"] for item in diff["items"]}
    assert changed[".rockbox/themes/iPone.cfg"] == "overwrite"
    assert ".rockbox/playlists/keep.m3u8" not in changed

    result = deploy_service.apply_diff(profile, diff)
    assert result["success"] is True
    assert os.path.isdir(result["backup_dir"])
    assert result["backup_dir"].startswith(profile["backup_location"])
    backup_theme = os.path.join(result["backup_dir"], ".rockbox", "themes", "iPone.cfg")
    assert os.path.isfile(backup_theme)

    with open(unrelated, "r", encoding="utf-8") as handle:
        assert handle.read() == "do not touch\n"

    restore = deploy_service.restore_latest_backup(profile)
    assert restore["success"] is True
    with open(existing_theme, "r", encoding="utf-8") as handle:
        assert handle.read() == "old theme data\n"
    with open(unrelated, "r", encoding="utf-8") as handle:
        assert handle.read() == "do not touch\n"

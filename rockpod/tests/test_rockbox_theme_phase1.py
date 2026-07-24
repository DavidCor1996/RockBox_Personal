import json
import os

from app.config import Config
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_profiles import RockboxProfileStore
from services.rockbox_themes import RockboxThemeService, THEME_DEFINITIONS


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


def test_profile_preserves_all_authentic_avatar_motion_choices(tmp_dir):
    _config, store = _make_store(tmp_dir)
    for clip in ("jump", "throw", "faint", "sit-idle", "punch", "kick", "walk"):
        profile = store.current_profile()
        profile["xbox_avatar_favorite_clip"] = clip
        assert store.save_profile(profile)["xbox_avatar_favorite_clip"] == clip


def test_theme_filtering_by_resolution(tmp_dir):
    _config, store = _make_store(tmp_dir)
    service = RockboxThemeService()

    desktop = service.list_themes(_repo_root(), "320x240")
    video5g = service.list_themes(_repo_root(), "320x240", "iPod Video 5G")
    classic6g = service.list_themes(_repo_root(), "320x240", "iPod Classic 6G")
    classic3g = service.list_themes(_repo_root(), "160x128")
    nano = service.list_themes(_repo_root(), "176x132")

    assert {item["id"] for item in desktop} == {"Blackery", "iPone", "iPoneCustom"}
    assert {item["id"] for item in video5g} == {"Blackery", "iPone", "iPoneCustom", "SpringPod3"}
    assert {item["id"] for item in classic6g} == {"Blackery", "iPone", "iPoneCustom"}
    assert {item["id"] for item in classic3g} == {"Galaxy", "CoverPod_3g", "iPone_3g"}
    assert {item["id"] for item in nano} == {"iPone_nano2g"}


def test_default_profiles_include_ipod_3g(tmp_dir):
    _config, store = _make_store(tmp_dir)

    profiles = {item["id"]: item for item in store.profiles()}

    assert "ipod-3g" in profiles
    assert profiles["ipod-3g"]["screen_resolution"] == "160x128"
    assert profiles["ipod-3g"]["selected_theme"] == "CoverPod_3g"


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


def test_springpod3_stack_is_ipod_video_only():
    service = RockboxThemeService()
    bundle = service.bundle_for_theme("SpringPod3", _repo_root())
    rels = {item["source_rel"] for item in bundle["assets"]}

    assert "themes/SpringPod3.cfg" in rels
    assert "wps/SpringPod3.wps" in rels
    assert "wps/SpringPod3.sbs" in rels
    assert "wps/SpringPod3.fms" in rels
    assert "backdrops/SpringPod3_bd.bmp" in rels
    assert "icons/SpringPod3.bmp" in rels
    assert "fonts/24 iLike.fnt" in rels
    assert any(path.startswith("wps/SpringPod3/") for path in rels)
    assert "iPod Video 5G" in bundle["compatible_device_models"]


def test_theme_skin_references_cover_wps_sbs_and_fms_assets():
    service = RockboxThemeService()

    for theme_id in THEME_DEFINITIONS:
        result = service.validate_skin_references(theme_id, _repo_root())
        assert result["success"] is True, result["missing"]
        assert {f"wps/{theme_id}.wps", f"wps/{theme_id}.sbs", f"wps/{theme_id}.fms"} <= set(result["checked"])


def test_theme_skin_reference_validation_reports_missing_bitmap(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(os.path.join(repo_root, "themes"), exist_ok=True)
    os.makedirs(os.path.join(repo_root, "wps", "iPone"), exist_ok=True)
    with open(os.path.join(repo_root, "themes", "iPone.cfg"), "w", encoding="utf-8") as handle:
        handle.write("wps: /.rockbox/wps/iPone.wps\n")
    with open(os.path.join(repo_root, "wps", "iPone.wps"), "w", encoding="utf-8") as handle:
        handle.write("%xl(Existing,Existing.bmp)\n%xl(Missing,Missing.bmp)\n")
    with open(os.path.join(repo_root, "wps", "iPone.sbs"), "w", encoding="utf-8") as handle:
        handle.write("%xl(SbsExisting,SbsExisting.bmp)\n")
    with open(os.path.join(repo_root, "wps", "iPone.fms"), "w", encoding="utf-8") as handle:
        handle.write("")
    with open(os.path.join(repo_root, "wps", "iPone", "Existing.bmp"), "wb") as handle:
        handle.write(b"BM")
    with open(os.path.join(repo_root, "wps", "iPone", "SbsExisting.bmp"), "wb") as handle:
        handle.write(b"BM")

    result = RockboxThemeService().validate_skin_references("iPone", repo_root)

    assert result["success"] is False
    assert result["missing"] == [{"skin": "wps/iPone.wps", "reference": "Missing.bmp"}]


def test_springpod3_deploy_rejects_non_5g_320x240_devices(tmp_dir):
    _config, store = _make_store(tmp_dir)
    mount_path = os.path.join(tmp_dir, "device")
    os.makedirs(mount_path, exist_ok=True)
    profile = store.current_profile()
    profile["device_mount_path"] = mount_path
    profile["target_device_model"] = "iPod Classic 6G"
    profile["selected_theme"] = "SpringPod3"
    profile["backup_location"] = os.path.join(tmp_dir, ".backups", profile["id"])
    profile = store.save_profile(profile)

    theme_service = RockboxThemeService()
    deploy_service = RockboxDeployService()
    bundle = theme_service.bundle_for_theme("SpringPod3", _repo_root())

    try:
        deploy_service.build_diff(profile, bundle)
    except ValueError as exc:
        assert "SpringPod3 can only be deployed" in str(exc)
    else:
        raise AssertionError("SpringPod3 deployed to non-5G profile")


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


def test_theme_remove_bundle_deletes_deployed_theme_files(tmp_dir):
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
    deploy_service.apply_diff(profile, deploy_service.build_diff(profile, bundle))
    cfg_path = os.path.join(mount_path, ".rockbox", "themes", "iPone.cfg")
    assert os.path.isfile(cfg_path)

    remove_bundle = theme_service.remove_bundle_for_theme("iPone", _repo_root())
    assert remove_bundle["id"] == "iPone-remove"
    assert all(item.get("action") == "remove" for item in remove_bundle["assets"])
    remove_diff = deploy_service.build_diff(profile, remove_bundle)
    assert remove_diff["summary"]["remove"] > 0
    removed = deploy_service.apply_diff(profile, remove_diff)

    assert removed["success"] is True
    assert not os.path.exists(cfg_path)


def test_device_only_theme_is_listed_and_can_be_removed(tmp_dir):
    _config, store = _make_store(tmp_dir)
    mount_path = os.path.join(tmp_dir, "device")
    theme_root = os.path.join(mount_path, ".rockbox")
    os.makedirs(os.path.join(theme_root, "themes"), exist_ok=True)
    os.makedirs(os.path.join(theme_root, "wps", "Unsaved"), exist_ok=True)
    os.makedirs(os.path.join(theme_root, "backdrops"), exist_ok=True)
    os.makedirs(os.path.join(theme_root, "icons"), exist_ok=True)
    with open(os.path.join(theme_root, "themes", "Unsaved.cfg"), "w", encoding="utf-8") as handle:
        handle.write(
            "wps: /.rockbox/wps/Unsaved.wps\n"
            "sbs: /.rockbox/wps/Unsaved.sbs\n"
            "backdrop: /.rockbox/backdrops/Unsaved_bd.bmp\n"
            "iconset: /.rockbox/icons/Unsaved.bmp\n"
        )
    for rel in (
        "wps/Unsaved.wps",
        "wps/Unsaved.sbs",
        "wps/Unsaved/Panel.bmp",
        "backdrops/Unsaved_bd.bmp",
        "icons/Unsaved.bmp",
    ):
        with open(os.path.join(theme_root, rel), "wb") as handle:
            handle.write(b"asset")

    profile = store.current_profile()
    profile["device_mount_path"] = mount_path
    profile["backup_location"] = os.path.join(tmp_dir, ".backups", profile["id"])
    profile = store.save_profile(profile)

    theme_service = RockboxThemeService()
    deploy_service = RockboxDeployService()
    listed = theme_service.list_themes(_repo_root(), "320x240", "", mount_path)
    assert "Unsaved" in {item["id"] for item in listed}

    details = theme_service.inspect_device_theme("Unsaved", mount_path)
    rels = {item["destination_rel"] for item in details["assets"]}
    assert ".rockbox/themes/Unsaved.cfg" in rels
    assert ".rockbox/wps/Unsaved/Panel.bmp" in rels

    remove_bundle = theme_service.remove_bundle_for_theme("Unsaved", _repo_root(), mount_path)
    remove_diff = deploy_service.build_diff(profile, remove_bundle)
    assert remove_diff["summary"]["remove"] >= 5
    removed = deploy_service.apply_diff(profile, remove_diff)

    assert removed["success"] is True
    assert not os.path.exists(os.path.join(theme_root, "themes", "Unsaved.cfg"))
    assert not os.path.exists(os.path.join(theme_root, "wps", "Unsaved", "Panel.bmp"))


def test_deploy_rejects_unsafe_device_root(tmp_dir):
    _config, store = _make_store(tmp_dir)
    profile = store.current_profile()
    profile["device_mount_path"] = os.path.abspath(os.sep)
    profile["backup_location"] = os.path.join(tmp_dir, ".backups", profile["id"])
    profile = store.save_profile(profile)

    theme_service = RockboxThemeService()
    deploy_service = RockboxDeployService()
    bundle = theme_service.bundle_for_theme("iPone", _repo_root())

    try:
        deploy_service.build_diff(profile, bundle)
    except ValueError as exc:
        assert "unsafe device root" in str(exc)
    else:
        raise AssertionError("Deploy accepted an unsafe device root")


def test_deploy_rejects_theme_destination_that_escapes_device_root(tmp_dir):
    _config, store = _make_store(tmp_dir)
    mount_path = os.path.join(tmp_dir, "device")
    os.makedirs(mount_path, exist_ok=True)
    source_path = os.path.join(tmp_dir, "source.cfg")
    with open(source_path, "w", encoding="utf-8") as handle:
        handle.write("source\n")

    profile = store.current_profile()
    profile["device_mount_path"] = mount_path
    profile["backup_location"] = os.path.join(tmp_dir, ".backups", profile["id"])
    profile = store.save_profile(profile)

    bundle = {
        "id": "bad",
        "compatible_device_models": [],
        "assets": [
            {
                "kind": "cfg",
                "source_rel": "source.cfg",
                "source_abs": source_path,
                "destination_rel": "../../escaped.cfg",
                "exists": True,
            }
        ],
    }

    try:
        RockboxDeployService().build_diff(profile, bundle)
    except ValueError as exc:
        assert "escapes device root" in str(exc)
    else:
        raise AssertionError("Deploy accepted a path traversal destination")


def test_apply_diff_rejects_destination_outside_device_root(tmp_dir):
    _config, store = _make_store(tmp_dir)
    mount_path = os.path.join(tmp_dir, "device")
    os.makedirs(mount_path, exist_ok=True)
    source_path = os.path.join(tmp_dir, "source.cfg")
    with open(source_path, "w", encoding="utf-8") as handle:
        handle.write("source\n")

    profile = store.current_profile()
    profile["device_mount_path"] = mount_path
    profile["backup_location"] = os.path.join(tmp_dir, ".backups", profile["id"])
    profile = store.save_profile(profile)

    diff_report = {
        "profile_id": profile["id"],
        "theme_id": "bad",
        "mount_path": mount_path,
        "items": [
            {
                "action": "copy",
                "kind": "cfg",
                "source_abs": source_path,
                "destination_rel": ".rockbox/themes/bad.cfg",
                "destination_abs": os.path.join(tmp_dir, "escaped.cfg"),
                "status": "add",
                "preserve_metadata": True,
            }
        ],
    }

    result = RockboxDeployService().apply_diff(profile, diff_report)

    assert result["success"] is False
    assert result["failure_count"] == 1
    assert "escapes device root" in result["failures"][0]
    assert not os.path.exists(os.path.join(tmp_dir, "escaped.cfg"))


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
    assert os.path.isfile(result["manifest_path"])
    assert not [
        name for name in os.listdir(result["backup_dir"])
        if name.startswith("tmp") and name != "manifest.json"
    ]
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


def test_restore_rejects_tampered_manifest_destination(tmp_dir):
    _config, store = _make_store(tmp_dir)
    mount_path = os.path.join(tmp_dir, "device")
    backup_root = os.path.join(tmp_dir, ".backups", "profile")
    backup_dir = os.path.join(backup_root, "20260420-000000")
    os.makedirs(mount_path, exist_ok=True)
    os.makedirs(backup_dir, exist_ok=True)
    escaped = os.path.join(tmp_dir, "escaped.cfg")
    manifest = {
        "profile_id": "profile",
        "theme_id": "bad",
        "mount_path": mount_path,
        "items": [
            {
                "destination_rel": "../escaped.cfg",
                "destination_abs": escaped,
                "backup_rel": ".rockbox/themes/iPone.cfg",
                "existed": False,
            }
        ],
    }
    with open(os.path.join(backup_dir, "manifest.json"), "w", encoding="utf-8") as handle:
        json.dump(manifest, handle)

    profile = store.current_profile()
    profile["device_mount_path"] = mount_path
    profile["backup_location"] = backup_root
    profile = store.save_profile(profile)

    restore = RockboxDeployService().restore_latest_backup(profile)

    assert restore["success"] is False
    assert restore["restored_count"] == 0
    assert "escapes device root" in restore["failures"][0]
    assert not os.path.exists(escaped)

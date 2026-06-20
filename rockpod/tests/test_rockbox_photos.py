import os

from PIL import Image
from PySide6.QtWidgets import QApplication

from app.config import Config
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_photos import RockboxPhotoService
from services.rockbox_profiles import RockboxProfileStore
from ui.photo_manager import PhotoManagerWidget


def _make_file(path, content=b"x"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(content)


def _make_image(path, size=(64, 48), color=(20, 80, 140)):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.new("RGB", size, color).save(path)


def _make_store(tmp_dir, repo_root):
    config = Config(os.path.join(tmp_dir, "photos-config.json"))
    return config, RockboxProfileStore(config, repo_root)


def test_photo_discovery_filters_supported_images_and_preserves_folders(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    photos = os.path.join(tmp_dir, "photos")
    _make_file(os.path.join(photos, "Trip", "IMG_0001.jpg"), b"jpg")
    _make_file(os.path.join(photos, "Trip", "IMG_0002.png"), b"png")
    _make_file(os.path.join(photos, "Trip", "notes.txt"), b"ignore")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["photos_library_path"] = photos
    profile = store.save_profile(profile)

    listed = RockboxPhotoService().list_photos(profile)

    assert [item["relative_path"] for item in listed] == [
        "Trip/IMG_0001.jpg",
        "Trip/IMG_0002.png",
    ]


def test_hidden_photos_are_filtered_until_included(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    photos = os.path.join(tmp_dir, "photos")
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(photos, "Trip", "Public.jpg"), b"public")
    _make_file(os.path.join(photos, "Trip", "Private.png"), b"private")
    _make_file(os.path.join(device, "Photos", "Trip", "Private.jpg"), b"synced")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["photos_library_path"] = photos
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxPhotoService()
    private = next(item for item in service.list_photos(profile) if item["relative_path"] == "Trip/Private.png")

    assert service.set_hidden(profile, [private], True) == 1
    assert [item["relative_path"] for item in service.list_photos(profile)] == ["Trip/Public.jpg"]

    revealed = service.list_photos(profile, include_hidden=True)
    hidden_photo = next(item for item in revealed if item["relative_path"] == "Trip/Private.png")
    assert hidden_photo["hidden"] is True

    os.remove(os.path.join(photos, "Trip", "Private.png"))
    assert [item["relative_path"] for item in service.list_photos(profile)] == ["Trip/Public.jpg"]

    revealed_stale = service.list_photos(profile, include_hidden=True)
    hidden_stale = next(item for item in revealed_stale if item["relative_path"] == "Trip/Private.jpg")
    assert hidden_stale["missing_source"] is True
    assert hidden_stale["hidden"] is True


def test_photo_manager_marks_hidden_items_and_toggles_hide_button(tmp_dir):
    QApplication.instance() or QApplication([])
    widget = PhotoManagerWidget()
    visible = {
        "id": "visible",
        "relative_path": "Visible.jpg",
        "source_path": "",
        "size": 12,
        "modified_time": 0,
        "on_device": True,
        "on_simulator": False,
        "missing_source": False,
        "hidden": False,
    }
    hidden = {
        **visible,
        "id": "hidden",
        "relative_path": "Hidden.jpg",
        "hidden": True,
    }

    widget.set_photos([visible, hidden])
    assert widget._photo_list.item(1).text().startswith("[Hidden/D] ")

    widget.set_selection_details([visible])
    assert widget._hide_btn.isEnabled()
    assert widget._hide_btn.text() == "Hide Selected"

    widget.set_selection_details([hidden])
    assert widget._hide_btn.text() == "Unhide Selected"


def test_photo_sync_and_remove_device_preserves_unrelated_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    photos = os.path.join(tmp_dir, "photos")
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(photos, "Trip", "IMG_0001.jpg"), b"photo")
    _make_file(os.path.join(device, ".rockbox", "themes", "keep.cfg"), b"leave alone\n")
    os.makedirs(device, exist_ok=True)

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["photos_library_path"] = photos
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxPhotoService()
    deploy = RockboxDeployService()
    selected = service.list_photos(profile)
    deploy_profile = service.deploy_profile(profile, "device")
    sync_diff = deploy.build_diff(deploy_profile, service.build_sync_bundle(profile, selected, "device"))
    synced = deploy.apply_diff(deploy_profile, sync_diff)

    assert synced["success"] is True
    assert os.path.isfile(os.path.join(device, "Photos", "Trip", "IMG_0001.jpg"))

    remove_diff = deploy.build_diff(deploy_profile, service.build_remove_bundle(profile, selected, "device"))
    assert remove_diff["summary"]["remove"] == 1
    removed = deploy.apply_diff(deploy_profile, remove_diff)

    assert removed["success"] is True
    assert not os.path.exists(os.path.join(device, "Photos", "Trip", "IMG_0001.jpg"))
    assert os.path.isfile(os.path.join(device, ".rockbox", "themes", "keep.cfg"))


def test_photo_sync_converts_images_and_adds_thumbnails(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    photos = os.path.join(tmp_dir, "photos")
    device = os.path.join(tmp_dir, "device")
    _make_image(os.path.join(photos, "Trip", "IMG_0001.png"), size=(1600, 1200))
    os.makedirs(device, exist_ok=True)

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["photos_library_path"] = photos
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxPhotoService()
    deploy = RockboxDeployService()
    selected = service.list_photos(profile)
    deploy_profile = service.deploy_profile(profile, "device")
    sync_diff = deploy.build_diff(deploy_profile, service.build_sync_bundle(profile, selected, "device"))
    synced = deploy.apply_diff(deploy_profile, sync_diff)

    photo_path = os.path.join(device, "Photos", "Trip", "IMG_0001.jpg")
    thumb_path = os.path.join(device, "Photos", ".photo_thumbs", "Trip", "IMG_0001.jpg.bmp")
    assert synced["success"] is True
    assert os.path.isfile(photo_path)
    assert os.path.isfile(thumb_path)
    with Image.open(photo_path) as image:
        assert image.format == "JPEG"
        assert image.size[0] <= 800
        assert image.size[1] <= 800
    with Image.open(thumb_path) as image:
        assert image.format == "BMP"
        assert image.size[0] <= 87
        assert image.size[1] <= 70


def test_photo_sync_removes_stale_preconversion_photo_and_thumb(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    photos = os.path.join(tmp_dir, "photos")
    device = os.path.join(tmp_dir, "device")
    _make_image(os.path.join(photos, "Trip", "IMG_0001.png"), size=(400, 300))
    _make_file(os.path.join(device, "Photos", "Trip", "IMG_0001.png"), b"old")
    _make_file(os.path.join(device, "Photos", ".photo_thumbs", "Trip", "IMG_0001.png.bmp"), b"old-thumb")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["photos_library_path"] = photos
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxPhotoService()
    deploy = RockboxDeployService()
    selected = service.list_photos(profile)
    assert len(selected) == 1
    assert selected[0]["device_relative_path"] == "Trip/IMG_0001.jpg"

    deploy_profile = service.deploy_profile(profile, "device")
    sync_diff = deploy.build_diff(deploy_profile, service.build_sync_bundle(profile, selected, "device"))
    assert sync_diff["summary"]["remove"] == 2
    synced = deploy.apply_diff(deploy_profile, sync_diff)

    assert synced["success"] is True
    assert os.path.isfile(os.path.join(device, "Photos", "Trip", "IMG_0001.jpg"))
    assert os.path.isfile(os.path.join(device, "Photos", ".photo_thumbs", "Trip", "IMG_0001.jpg.bmp"))
    assert not os.path.exists(os.path.join(device, "Photos", "Trip", "IMG_0001.png"))
    assert not os.path.exists(os.path.join(device, "Photos", ".photo_thumbs", "Trip", "IMG_0001.png.bmp"))


def test_photo_sync_repairs_missing_device_thumbnails(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    photos = os.path.join(tmp_dir, "photos")
    device = os.path.join(tmp_dir, "device")
    _make_image(os.path.join(photos, "new.jpg"), size=(320, 240))
    _make_image(os.path.join(device, "Photos", "Existing.jpg"), size=(320, 240))
    _make_image(os.path.join(device, "Photos", ".photo_thumbs", "wp", "Existing.jpg.bmp"), size=(64, 48))

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["photos_library_path"] = photos
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxPhotoService()
    deploy = RockboxDeployService()
    selected = service.list_photos(profile)
    deploy_profile = service.deploy_profile(profile, "device")
    bundle = service.build_sync_bundle(profile, selected, "device")
    repair_assets = [
        item for item in bundle["assets"]
        if item["kind"] == "photo_thumb_repair"
    ]
    assert [item["destination_rel"] for item in repair_assets] == [
        "Photos/.photo_thumbs/Existing.jpg.bmp"
    ]

    synced = deploy.apply_diff(
        deploy_profile,
        deploy.build_diff(deploy_profile, bundle),
    )

    assert synced["success"] is True
    assert os.path.isfile(os.path.join(device, "Photos", ".photo_thumbs", "Existing.jpg.bmp"))


def test_photo_sync_repairs_oversized_device_thumbnails(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    photos = os.path.join(tmp_dir, "photos")
    device = os.path.join(tmp_dir, "device")
    _make_image(os.path.join(photos, "new.jpg"), size=(320, 240))
    _make_image(os.path.join(device, "Photos", "Tall.jpg"), size=(800, 700))
    _make_image(os.path.join(device, "Photos", ".photo_thumbs", "Tall.jpg.bmp"), size=(87, 70))

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["photos_library_path"] = photos
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxPhotoService()
    deploy = RockboxDeployService()
    selected = service.list_photos(profile)
    deploy_profile = service.deploy_profile(profile, "device")
    bundle = service.build_sync_bundle(profile, selected, "device")
    repair_assets = [
        item for item in bundle["assets"]
        if item["kind"] == "photo_thumb_repair"
    ]
    assert [item["destination_rel"] for item in repair_assets] == [
        "Photos/.photo_thumbs/Tall.jpg.bmp"
    ]

    synced = deploy.apply_diff(
        deploy_profile,
        deploy.build_diff(deploy_profile, bundle),
    )

    assert synced["success"] is True
    with Image.open(os.path.join(device, "Photos", ".photo_thumbs", "Tall.jpg.bmp")) as image:
        assert image.size[0] <= 80
        assert image.size[1] <= 60


def test_deleted_local_photo_remains_removable_from_device(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    photos = os.path.join(tmp_dir, "photos")
    device = os.path.join(tmp_dir, "device")
    source = os.path.join(photos, "IMG_0001.jpg")
    _make_file(source, b"photo")
    os.makedirs(device, exist_ok=True)

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["photos_library_path"] = photos
    profile["device_mount_path"] = device
    profile = store.save_profile(profile)

    service = RockboxPhotoService()
    deploy = RockboxDeployService()
    deploy_profile = service.deploy_profile(profile, "device")
    selected = service.list_photos(profile)
    synced = deploy.apply_diff(
        deploy_profile,
        deploy.build_diff(deploy_profile, service.build_sync_bundle(profile, selected, "device")),
    )
    assert synced["success"] is True

    os.remove(source)
    stale = service.list_photos(profile)

    assert len(stale) == 1
    assert stale[0]["missing_source"] is True
    assert stale[0]["on_device"] is True

    removed = deploy.apply_diff(
        deploy_profile,
        deploy.build_diff(deploy_profile, service.build_remove_bundle(profile, stale, "device")),
    )
    assert removed["success"] is True
    assert not os.path.exists(os.path.join(device, "Photos", "IMG_0001.jpg"))

import os

from PIL import Image

from app.config import Config
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_photos import RockboxPhotoService
from services.rockbox_profiles import RockboxProfileStore


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

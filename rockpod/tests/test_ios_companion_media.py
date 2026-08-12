from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
IOS = ROOT / "tools" / "ios" / "RockPodLink"


def test_video_sync_registers_catalog_only_after_media_and_artwork():
    controller = (IOS / "RPRootViewController.m").read_text(encoding="utf-8")
    converter = (IOS / "RPVideoConverter.m").read_text(encoding="utf-8")

    assert "[self queueVideoConversion:result index:0]" in controller
    assert "index:index + 1" in controller
    assert "sync stopped at" in controller
    assert "registered in Netflix/Videos" in controller
    assert converter.index("arrayWithObject:marker") < converter.index(
        '@"/.rockbox/rockpod/phone/video-row.tsv"'
    )
    assert "lengthOfBytesUsingEncoding:NSUTF8StringEncoding] >= 1023" in converter


def test_simulator_launch_does_not_eagerly_duplicate_phone_library():
    controller = (IOS / "RPRootViewController.m").read_text(encoding="utf-8")

    launch = controller[controller.index("- (void)launchSimulator") :]
    launch = launch[: launch.index("- (UIView *)internetPage")]
    assert "preparePhoneLibraryForSimulator" not in controller
    assert "importPhoneMusicForSimulator" not in controller
    assert "importPhonePhotosForSimulator" not in controller
    assert "startWithCompletion" in launch


def test_companion_music_and_video_paths_cover_device_and_simulator():
    controller = (IOS / "RPRootViewController.m").read_text(encoding="utf-8")
    host = (IOS / "RPSimulatorHost.m").read_text(encoding="utf-8")

    assert '@"/Music/RockPodLink/%@/%@"' in controller
    assert "AVAssetExportPresetAppleM4A" in controller
    assert "RPPickerPurposeSimulatorMusic" in controller
    assert "persistentID" in controller
    assert "convertAndInstallSimulatorVideoURL" in controller
    assert "installVideoConversionInSimulator" in controller
    assert "registerSimulatorVideoRowAtURL" in controller
    assert 'simulatorMediaURLForFolder:@".rockbox/videolist"' in controller
    assert "ready in simulator Netflix/Videos" in controller
    assert "prepareForMediaWithCompletion" in host
    assert "pendingSimulatorImports" in controller
    assert "Converted video is missing from the simulator media root" in controller


def test_ios_photo_decoders_are_arm64_dynamic_code():
    resources = IOS / "Resources"
    manifest = (
        resources / "SimulatorInstall/.rockbox/rockpod-dynamic-code.tsv"
    ).read_text(encoding="utf-8")

    for decoder in ("bmp", "gif", "jpeg", "jpegp", "png", "ppm"):
        framework = resources / "Frameworks" / f"RockPodOverlay__{decoder}.dylib"
        assert (
            f".rockbox/rocks/viewers/{decoder}.ovl\t"
            f"Frameworks/RockPodOverlay__{decoder}.dylib"
        ) in manifest
        assert framework.read_bytes()[:4] == b"\xcf\xfa\xed\xfe"
    assert not list(
        (resources / "SimulatorInstall/.rockbox/rocks/viewers").glob("*.ovl")
    )
    config = (resources / "SimulatorInstall/.rockbox/config.cfg").read_text(
        encoding="utf-8"
    )
    assert "tagcache_autoupdate: on" in config


def test_ios_simulator_shell_matches_earth_brown_ipod_geometry():
    shell = Image.open(IOS / "Resources" / "UI256.bmp").convert("RGB")

    assert shell.size == (350, 591)
    assert shell.getpixel((14, 12)) == (0, 0, 0)
    assert shell.getpixel((333, 251)) == (0, 0, 0)
    wheel = shell.getpixel((175, 340))
    center = shell.getpixel((175, 432))
    face = shell.getpixel((20, 290))
    assert min(wheel) > 210
    assert center[0] > center[2] and center[1] > center[2]
    assert face[0] > face[2] and face[1] > face[2]

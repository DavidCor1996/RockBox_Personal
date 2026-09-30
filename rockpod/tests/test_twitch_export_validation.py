import json
from pathlib import Path
from types import SimpleNamespace

import pytest
from PIL import Image
from services import twitch_app


ROOT = Path(__file__).resolve().parents[2]


def streams(h264=True, audio=True, video_count=300000, audio_count=450000):
    result = [{"codec_type": "video", "codec_name": "h264" if h264 else "mpeg2video",
               "nb_frames": str(video_count)}]
    if audio:
        result.append({"codec_type": "audio", "codec_name": "aac" if h264 else "mp2",
                       "profile": "LC", "sample_rate": "44100", "channels": 2,
                       "nb_frames": str(audio_count)})
    return result


def probe_result(value):
    return SimpleNamespace(stdout=json.dumps({"streams": value}), returncode=0)


@pytest.mark.parametrize("size,profile,target,accepted", [
    (2_545_089_450, "h264_apple_exact", "ipod6g", True),
    (2_545_089_450, "quality", "ipod6g", False),
    (2_545_089_450, "h264_apple_exact", "ipodvideo", False),
    (0xffffffff + 1, "h264_apple_exact", "ipod6g", False),
    (0, "h264_apple_exact", "ipod6g", False),
])
def test_export_file_limits(tmp_path, monkeypatch, size, profile, target, accepted):
    path = tmp_path / "vod.m4v"
    with path.open('wb') as handle:
        handle.truncate(size)  # Sparse fixture; no multi-GB allocation.
    monkeypatch.setattr(twitch_app.subprocess, "run",
                        lambda *a, **k: probe_result(streams()))
    kwargs = dict(config={}, profile=profile, require_audio=True, device_target=target)
    if accepted:
        twitch_app._validate_export(path, **kwargs)
    else:
        with pytest.raises(ValueError, match="size"):
            twitch_app._validate_export(path, **kwargs)


@pytest.mark.parametrize("value,message", [
    (streams(audio=False), "required audio"),
    (streams(video_count=400001), "video sample count"),
    (streams(audio_count=600001), "audio sample count"),
    (streams(video_count="N/A"), "video sample count"),
    (streams(h264=False), "supported video"),
])
def test_export_stream_limits(tmp_path, monkeypatch, value, message):
    path = tmp_path / "vod.m4v"
    path.write_bytes(b"fixture")
    monkeypatch.setattr(twitch_app.subprocess, "run",
                        lambda *a, **k: probe_result(value))
    with pytest.raises(ValueError, match=message):
        twitch_app._validate_export(path, config={}, profile="h264_apple_exact",
                                    require_audio=True, device_target="ipod6g")


def test_sync_revalidates_cached_outputs_and_falls_back(
    db, config, mock_device, tmp_dir, monkeypatch
):
    source = Path(tmp_dir) / "source.mp4"
    source.write_bytes(b"download")
    thumbnail = Path(tmp_dir) / "thumb.png"
    Image.new("RGB", (96, 54), "purple").save(thumbnail)
    service = twitch_app.TwitchAppService(db, config, ROOT)
    creator = service.add_creator("Emma", "Emma", cycle_epoch=100)
    vod = service.add_vod(source, creator_key=creator["creator_key"],
                          twitch_id="12345", title="Future VOD",
                          duration_ms=10000, thumbnail_path=thumbnail)
    monkeypatch.setattr(twitch_app, "_probe_stream_types", lambda *a: {"video", "audio"})
    invalid_h264 = False
    invalid_mpeg = False

    def stage(source, destination, **kwargs):
        Path(destination).write_bytes(b"h264" if kwargs["profile"] == "h264_apple_exact" else b"mpeg")

    def probe(command, **kwargs):
        h264 = Path(command[-1]).read_bytes() == b"h264"
        return probe_result(streams(h264=h264,
                                   audio=not (invalid_h264 if h264 else invalid_mpeg)))

    monkeypatch.setattr(twitch_app, "stage_app_video", stage)
    monkeypatch.setattr(twitch_app.subprocess, "run", probe)
    kwargs = dict(device=SimpleNamespace(rockbox_target="ipod6g"),
                  refresh_creators=False, video_profile="h264_apple_exact")
    first = service.sync(mock_device, **kwargs)
    assert first["media_updated"] == 1
    assert first["mpeg_backups_updated"] == 1

    # Both source and destination signatures are unchanged: cached media
    # must still pass validation before being listed as playable.
    invalid_h264 = True
    second = service.sync(mock_device, **kwargs)
    assert second["mpeg_fallbacks"] == 1
    catalogue = Path(mock_device) / ".rockbox/twitch/vods.tsv"
    assert f"{vod['id']}.mpg" in catalogue.read_text()
    assert f"{vod['id']}.m4v" not in catalogue.read_text()

    invalid_mpeg = True
    third = service.sync(mock_device, **kwargs)
    assert third["media_failed"] == 1
    assert third["vods_exported"] == 0
    assert third["media_preserved"] == 0
    assert vod["id"] not in catalogue.read_text()
    assert any("required audio" in error for error in third["errors"])

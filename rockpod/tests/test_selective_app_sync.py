import csv
from pathlib import Path

from PIL import Image

import services.tiktok_app as tiktok_module
from services.livetv import _merge_channel_rows
from services.onlyfans_app import OnlyFansAppService, ONLYFANS_LIBRARY
from services.tiktok_app import TikTokAppService, TIKTOK_DEVICE_ROOT
from ui.video_sync import mark_tracks_for_device


ROOT = Path(__file__).resolve().parents[2]


def _ids(path):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        return {row["id"] for row in csv.DictReader(stream, delimiter="\t")}


def test_selected_onlyfans_posts_and_creators_stay_on_their_own_ipod(tmp_path):
    cache = tmp_path / "cache"
    service = OnlyFansAppService({"cache_dir": str(cache)}, ROOT)
    profiles = []
    for username, color, item_id in (("alice", "red", "ofp_a"),
                                     ("bob", "blue", "ofp_b")):
        source = tmp_path / f"{item_id}.png"
        Image.new("RGB", (50, 50), color).save(source)
        profiles.append({
            "username": username, "display_name": username,
            "media": [{"id": item_id, "type": "photo", "title": item_id,
                       "source_path": str(source), "post_id": item_id}],
        })
    second_photo = tmp_path / "ofp_b2.png"
    Image.new("RGB", (50, 50), "cyan").save(second_photo)
    profiles[1]["media"].append({
        "id": "ofp_b2", "type": "photo", "title": "ofp_b2",
        "source_path": str(second_photo), "post_id": "ofp_b",
    })
    service._save({"profiles": profiles})
    ipod_a = tmp_path / "ipod-a"
    ipod_b = tmp_path / "ipod-b"
    ipod_a.mkdir()
    ipod_b.mkdir()

    service.sync(ipod_a, usernames={"alice"})
    service.sync(ipod_b, usernames={"bob"})
    assert _ids(ipod_a / ONLYFANS_LIBRARY) == {"ofp_a"}
    assert _ids(ipod_b / ONLYFANS_LIBRARY) == {"ofp_b", "ofp_b2"}

    service.sync(ipod_a, post_ids={"ofp_b"})
    assert _ids(ipod_a / ONLYFANS_LIBRARY) == {"ofp_a", "ofp_b", "ofp_b2"}
    assert _ids(ipod_b / ONLYFANS_LIBRARY) == {"ofp_b", "ofp_b2"}


def test_selected_tiktoks_preserve_other_clips_per_ipod(
    db, config, tmp_path, monkeypatch,
):
    service = TikTokAppService(db, config, ROOT)
    quality = []

    def fake_stage(_source, target, **kwargs):
        Path(target).write_bytes(b"video")
        quality.append(kwargs["quality"])

    def fake_preview(_row, target, _staging):
        Path(target).write_bytes(b"preview")
        return True

    monkeypatch.setattr(tiktok_module, "stage_app_video", fake_stage)
    monkeypatch.setattr(service, "_render_menu_preview", fake_preview)
    thumbnail = tmp_path / "thumb.png"
    Image.new("RGB", (96, 72), "green").save(thumbnail)
    ids = []
    for name in ("first", "second"):
        source = tmp_path / f"{name}.mpg"
        source.write_bytes(name.encode())
        row = service.add_video(source, title=name, thumbnail_path=thumbnail)
        ids.append(row["id"])
    ipod_a = tmp_path / "ipod-a"
    ipod_b = tmp_path / "ipod-b"
    ipod_a.mkdir()
    ipod_b.mkdir()

    service.sync(ipod_a, refresh_accounts=False, video_ids={ids[0]},
                 video_quality="space")
    service.sync(ipod_b, refresh_accounts=False, video_ids={ids[1]})
    manifest = Path(TIKTOK_DEVICE_ROOT) / "library.tsv"
    assert _ids(ipod_a / manifest) == {ids[0]}
    assert _ids(ipod_b / manifest) == {ids[1]}

    service.sync(ipod_a, refresh_accounts=False, video_ids={ids[1]},
                 video_quality="space")
    assert _ids(ipod_a / manifest) == set(ids)
    assert _ids(ipod_b / manifest) == {ids[1]}
    assert quality == ["space", "tv", "space"]

    def unavailable(_source, _target, **_kwargs):
        raise OSError("source unavailable")

    monkeypatch.setattr(tiktok_module, "stage_app_video", unavailable)
    report = service.sync(ipod_a, refresh_accounts=False, video_ids={ids[0]},
                          video_quality="tv")
    assert report["videos_skipped"] == 1
    assert _ids(ipod_a / manifest) == set(ids)


def test_live_tv_selected_channel_merge_is_per_device(tmp_path):
    header = "# chan\tday\tstart\tdur\tkind\ttitle\trating\tdesc\tpath\tblockstart\tblockdur\n"
    a = tmp_path / "a.tsv"
    b = tmp_path / "b.tsv"
    fresh = tmp_path / "fresh.tsv"
    a.write_text(header + "100\t0\t0\t60\tshow\tA\t--\t--\ta.mpg\t0\t60\n")
    b.write_text(header + "300\t0\t0\t60\tshow\tB\t--\t--\tb.mpg\t0\t60\n")
    fresh.write_text(header + "200\t0\t0\t60\tshow\tNew\t--\t--\tnew.mpg\t0\t60\n")

    rows_a, text_a = _merge_channel_rows(a, fresh, {200}, 11)
    rows_b, text_b = _merge_channel_rows(b, fresh, {200}, 11)
    assert {row[0] for row in rows_a} == {"100", "200"}
    assert {row[0] for row in rows_b} == {"200", "300"}
    assert "300\t" not in text_a
    assert "100\t" not in text_b


def test_video_status_follows_selected_ipod_not_library_flag():
    library = [{"id": 7, "synced_to_device": True,
                "device_path": "/old-ipod/movie.mpg"}]
    device_a = [{"local_track_id": 7, "device_path": "/a/movie.mpg"}]
    on_a = mark_tracks_for_device([dict(library[0])], (), device_a, True)
    on_b = mark_tracks_for_device([dict(library[0])], {7}, (), True)
    unknown = mark_tracks_for_device([dict(library[0])], (), (), False)
    assert on_a[0]["synced_to_device"] and on_a[0]["device_path"] == "/a/movie.mpg"
    assert not on_b[0]["synced_to_device"] and not on_b[0]["device_path"]
    assert unknown[0]["device_status_unknown"] and not unknown[0]["synced_to_device"]

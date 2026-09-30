import json
import subprocess
from pathlib import Path
from types import SimpleNamespace

from PIL import Image

import services.tiktok_app as tiktok_app_module
from services.android_media import build_ffmpeg_command
from services.tiktok_app import (
    TikTokAppService,
    _parse_tiktok_embed,
    _parse_tiktok_profile_embed,
    _parse_tiktok_profile_embed_videos,
    _parse_tiktok_profile_page,
    _parse_tiktok_video_author_embed,
    _tiktok_url,
)
from scripts.tiktok_profile_archive import pinned_video_order


ROOT = Path(__file__).resolve().parents[2]


def test_tiktok_url_validation_and_aspect_preserving_conversion():
    url = (
        "https://www.tiktok.com/@samtemp1e/video/"
        "7299327548671003947?lang=en"
    )
    assert _tiktok_url(url) == url
    assert _tiktok_url("https://www.tiktok.com/@samtemp1e", account=True) == (
        "https://www.tiktok.com/@samtemp1e"
    )
    command = build_ffmpeg_command("portrait.mp4", "portrait.mpg", "video")
    video_filter = command[command.index("-vf") + 1]
    assert "force_original_aspect_ratio=decrease" in video_filter
    assert "pad=320:240" in video_filter
    service_source = (ROOT / "rockpod/services/tiktok_app.py").read_text()
    assert '"tiktok-320x240-30fps-v2"' in service_source
    assert '"pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=30"' in service_source


def test_tiktok_library_can_be_recovered_from_synced_device(
    db, config, mock_device
):
    service = TikTokAppService(db, config, ROOT)
    root = Path(mock_device) / ".rockbox/tiktok"
    media = Path(mock_device) / "TikTok/videos"
    thumbs = root / "thumbnails"
    apps = Path(mock_device) / ".rockbox/rocks/apps"
    for path in (root, media, thumbs, apps):
        path.mkdir(parents=True, exist_ok=True)
    (media / "tt_123.mpg").write_bytes(b"mpeg")
    (thumbs / "tt_123.bmp").write_bytes(b"bmp")
    (root / "library.tsv").write_text(
        "id\ttitle\tcreator\tsource_type\taccount_url\tupload_date\tpath"
        "\tthumbnail\tmenu_preview\n"
        "tt_123\tClip\t@creator\tarchive\thttps://www.tiktok.com/@creator"
        "\t20260101\t/TikTok/videos/tt_123.mpg"
        "\t/.rockbox/tiktok/thumbnails/tt_123.bmp\t\n",
        encoding="utf-8",
    )
    (root / "profiles.tsv").write_text(
        "account_url\tusername\tdisplay_name\tbio\tavatar\tfollowers"
        "\tfollowing\tlikes\tvideos\tfollowed\tverified\n"
        "https://www.tiktok.com/@creator\tcreator\tCreator\tBio\t\t100"
        "\t20\t300\t4\t1\t1\n",
        encoding="utf-8",
    )
    (apps / ".ipodtiktok_feed.tsv").write_text(
        "id\ttitle\tpath\tsource_type\tcreator\tdescription\tthumbnail"
        "\tlikes\tcomments\tpin_order\n"
        "tt_123\tClip\t/TikTok/videos/tt_123.mpg\tarchive\t@creator"
        "\tCaption\t/.rockbox/tiktok/thumbnails/tt_123.bmp\t50\t6\t2\n",
        encoding="utf-8",
    )

    report = service.recover_from_device(mock_device)

    assert report == {"videos": 1, "profiles": 1, "accounts": 1}
    video = service.get_video("tt_123")
    assert video["title"] == "Clip"
    assert video["like_count"] == 50
    assert video["pin_order"] == 2
    assert video["source_type"] == "archive"
    account = service.list_account_syncs()[0]
    assert account["sync_mode"] == "archive"
    assert account["follower_count"] == 100


def test_official_embed_fallback_maps_video_and_profile_metadata():
    state = {
        "source": {"data": {"/embed/v2/7316329075033836842": {
            "videoData": {
                "itemInfos": {
                    "id": "7316329075033836842",
                    "text": "Slay by day",
                    "video": {"urls": ["https://cdn.example/video.mp4"],
                              "videoMeta": {"duration": 10}},
                    "covers": ["https://cdn.example/cover.jpg"],
                    "diggCount": 7681,
                    "commentCount": 121,
                },
                "authorInfos": {
                    "uniqueId": "samtemp1e", "nickName": "Sam",
                    "signature": "Let me cook",
                    "secUid": "MS4wLjABAAAA-sam-stable-key",
                    "userId": "123456789",
                },
                "authorStats": {
                    "followerCount": 58400, "followingCount": 306,
                    "heartCount": "1000000", "videoCount": 80,
                },
            }
        }}}
    }
    page = (
        '<script id="__FRONTITY_CONNECT_STATE__" type="application/json">'
        + json.dumps(state) + "</script>"
    )
    media_url, thumbnail_url, info = _parse_tiktok_embed(
        page, "7316329075033836842"
    )

    assert media_url == "https://cdn.example/video.mp4"
    assert thumbnail_url == "https://cdn.example/cover.jpg"
    assert info["uploader"] == "samtemp1e"
    assert info["channel_id"] == "MS4wLjABAAAA-sam-stable-key"
    assert info["channel_follower_count"] == 58400
    assert info["profile_like_count"] == "1000000"
    assert info["like_count"] == 7681


def test_profile_page_fallback_maps_secondary_id_and_real_stats():
    profile = {
        "userInfo": {
            "user": {
                "id": "215720304600436736",
                "uniqueId": "thiq.loser",
                "nickname": "Gremmie",
                "signature": "Stay cringe",
                "secUid": "MS4wLjABAAAA-secondary-user-id",
                "avatarLarger": "https://cdn.example/avatar.jpg",
            },
            "statsV2": {
                "followerCount": "408780",
                "followingCount": "2369",
                "heartCount": "12294276",
                "videoCount": "1644",
            },
        },
        "statusCode": 0,
    }
    page = (
        '<script id="__UNIVERSAL_DATA_FOR_REHYDRATION__" '
        'type="application/json">'
        + json.dumps({"__DEFAULT_SCOPE__": {"webapp.user-detail": profile}})
        + "</script>"
    )

    info = _parse_tiktok_profile_page(page)

    assert info["id"] == "MS4wLjABAAAA-secondary-user-id"
    assert info["channel_id"] == "215720304600436736"
    assert info["uploader"] == "thiq.loser"
    assert info["channel_follower_count"] == "408780"
    assert info["profile_like_count"] == "12294276"


def test_official_embeds_recover_profile_secondary_id_and_stats():
    profile_record = {
        "userInfo": {
            "id": "6786117537953694726",
            "uniqueId": "taylortomlinsoncomedy",
            "nickname": "taylortomlinson",
            "followerCount": 3300000,
            "heartCount": 84200000,
        },
        "videoList": [{"id": "7675427255434104094"}],
        "isError": False,
    }
    video_record = {"videoData": {
        "authorInfos": {
            "secUid": "MS4wLjABAAAA-taylor-secondary-id",
            "userId": "6786117537953694726",
            "uniqueId": "taylortomlinsoncomedy",
            "nickName": "taylortomlinson",
        },
        "authorStats": {"followerCount": 3300000, "videoCount": 607},
    }}

    def page(route, record):
        state = {"source": {"data": {route: record}}}
        return (
            '<script id="__FRONTITY_CONNECT_STATE__" type="application/json">'
            + json.dumps(state) + "</script>"
        )

    profile, video_id = _parse_tiktok_profile_embed(
        page("/embed/@taylortomlinsoncomedy", profile_record)
    )
    author = _parse_tiktok_video_author_embed(
        page("/embed/v2/7675427255434104094", video_record)
    )

    assert video_id == "7675427255434104094"
    assert profile["channel_follower_count"] == 3300000
    assert author["id"] == "MS4wLjABAAAA-taylor-secondary-id"
    assert author["video_count"] == 607


def test_profile_embed_preserves_ordered_following_video_list():
    record = {
        "userInfo": {"uniqueId": "aquajuniper", "nickname": "Aqua"},
        "videoList": [
            {"id": "7530000000000000003"},
            {"id": "7530000000000000002"},
            {"id": "7530000000000000003"},
            {"id": "7530000000000000001"},
        ],
        "isError": False,
    }
    page = (
        '<script id="__FRONTITY_CONNECT_STATE__" type="application/json">'
        + json.dumps({"source": {"data": {"/embed/@aquajuniper": record}}})
        + "</script>"
    )

    profile, video_ids = _parse_tiktok_profile_embed_videos(page)

    assert profile["uploader"] == "aquajuniper"
    assert video_ids == [
        "7530000000000000003",
        "7530000000000000002",
        "7530000000000000001",
    ]


def test_following_account_falls_back_to_official_profile_embed(
    db, config, monkeypatch
):
    service = TikTokAppService(db, config, ROOT)
    account = "https://www.tiktok.com/@aquajuniper"
    monkeypatch.setattr(
        tiktok_app_module.subprocess,
        "run",
        lambda *_args, **_kwargs: (_ for _ in ()).throw(
            subprocess.CalledProcessError(1, ["yt-dlp"], stderr="no secUid")
        ),
    )

    def embed_fallback(account_url):
        service._account_embed_uploads[account_url] = [
            f"{account_url}/video/7530000000000000003",
            f"{account_url}/video/7530000000000000002",
        ]
        return {
            "uploader": "aquajuniper",
            "channel": "Aqua Juniper",
            "channel_follower_count": 251800,
        }

    monkeypatch.setattr(service, "_account_profile_from_embed", embed_fallback)

    uploads = service._account_uploads(account, 10)

    assert uploads == [
        f"{account}/video/7530000000000000003",
        f"{account}/video/7530000000000000002",
    ]
    assert service._account_profiles[account]["uploader"] == "aquajuniper"


def test_profile_embed_identifies_leading_pinned_posts():
    state = {
        "source": {"data": {"/embed/@creator": {"videoList": [
            {"id": "old-pin-1"}, {"id": "old-pin-2"},
            {"id": "newest"}, {"id": "next-newest"},
        ]}}}
    }
    page = (
        '<script id="__FRONTITY_CONNECT_STATE__" type="application/json">'
        + json.dumps(state) + "</script>"
    )

    assert pinned_video_order(page, "newest") == {
        "old-pin-1": 2,
        "old-pin-2": 1,
    }


def test_archive_uses_profile_page_secondary_id_when_extractor_fails(
    db, config, tmp_dir, monkeypatch
):
    service = TikTokAppService(db, config, ROOT)
    sec_uid = "MS4wLjABAAAA-secondary-user-id"
    service._account_uploads = lambda *_args: (_ for _ in ()).throw(
        ValueError("Unable to extract secondary user ID")
    )
    service._account_profile_from_web = lambda _url: {
        "id": sec_uid,
        "uploader": "thiq.loser",
        "channel": "Gremmie",
        "channel_follower_count": 408780,
        "video_count": 1644,
    }
    service.register_account_archive = lambda *_args: 1
    monkeypatch.setattr(Path, "home", lambda: Path(tmp_dir))
    calls = []

    def fake_run(command, **_kwargs):
        calls.append(command)
        return SimpleNamespace(returncode=0)

    monkeypatch.setattr(subprocess, "run", fake_run)
    report = service.prepare_account_archive(
        "https://www.tiktok.com/@thiq.loser"
    )

    assert report == {"videos": 1, "returncode": 0}
    assert "--sec-uid" in calls[0]
    assert calls[0][calls[0].index("--sec-uid") + 1] == sec_uid
    account = service.list_account_syncs()[0]
    assert account["sync_mode"] == "archive"
    assert account["account_key"] == sec_uid


def test_archive_reuses_saved_profile_key_before_fragile_network_lookup(
    db, config, tmp_dir, monkeypatch
):
    service = TikTokAppService(db, config, ROOT)
    sec_uid = "MS4wLjABAAAA-natalie-stable-key"
    monkeypatch.setattr(Path, "home", lambda: Path(tmp_dir))
    managed = (
        Path(tmp_dir) / ".rockpod/tiktok/following/nataliecuomo"
    )
    managed.mkdir(parents=True)
    (managed / ".profile.json").write_text(
        json.dumps({
            "sec_uid": sec_uid,
            "uploader": "nataliecuomo",
            "channel": "Natalie",
            "video_count": 1801,
        }),
        encoding="utf-8",
    )
    service._account_uploads = lambda *_args: (_ for _ in ()).throw(
        AssertionError("saved account key should bypass profile extraction")
    )
    service.register_account_archive = lambda *_args: 1
    calls = []

    def fake_run(command, **_kwargs):
        calls.append(command)
        return SimpleNamespace(returncode=0)

    monkeypatch.setattr(subprocess, "run", fake_run)
    report = service.prepare_account_archive(
        "https://www.tiktok.com/@nataliecuomo"
    )

    assert report == {"videos": 1, "returncode": 0}
    assert calls[0][calls[0].index("--sec-uid") + 1] == sec_uid
    assert service.list_account_syncs()[0]["account_key"] == sec_uid


def test_tiktok_manual_video_is_stable_and_never_pruned(db, config, tmp_dir):
    service = TikTokAppService(db, config, ROOT)
    source = Path(tmp_dir) / "manual.mpg"
    source.write_bytes(b"manual")
    first = service.add_video(source, title="Manual TikTok")
    second = service.add_video(source, title="Edited TikTok")

    assert first["id"] == second["id"]
    assert service.get_video(first["id"])["source_type"] == "manual"
    assert service.get_video(first["id"])["title"] == "Edited TikTok"


def test_archived_profile_orders_pins_then_newest_posts(db, config, tmp_dir):
    service = TikTokAppService(db, config, ROOT)
    account = "https://www.tiktok.com/@creator"
    fixtures = [
        ("pin-first", "2020-01-01", 2),
        ("pin-second", "2021-01-01", 1),
        ("newest", "2026-08-20", 0),
        ("older", "2026-08-19", 0),
    ]
    for video_id, upload_date, pin_order in fixtures:
        source = Path(tmp_dir) / f"{video_id}.mp4"
        source.write_bytes(video_id.encode())
        service.add_video(
            source,
            remote_id=video_id,
            source_type="archive",
            account_url=account,
            upload_date=upload_date,
            pin_order=pin_order,
        )

    archived = [
        row for row in service.list_videos() if row["source_type"] == "archive"
    ]

    assert [row["title"] for row in archived] == [
        "pin-first", "pin-second", "newest", "older"
    ]


def test_url_import_captures_real_creator_profile_metadata(db, config, tmp_dir):
    service = TikTokAppService(db, config, ROOT)
    source = Path(tmp_dir) / "real.mp4"
    source.write_bytes(b"video")
    thumbnail = Path(tmp_dir) / "real.jpg"
    Image.new("RGB", (90, 160), "red").save(thumbnail)
    info = {
        "id": "7299327548671003947",
        "title": "So skibidi",
        "uploader": "samtemp1e",
        "uploader_url": "https://www.tiktok.com/@samtemp1e",
        "channel": "Sam",
        "channel_follower_count": 1234,
        "following_count": 55,
        "profile_like_count": 9876,
        "video_count": 42,
    }
    service._download = lambda _url, _managed: (source, info, thumbnail)
    row = service.import_url(
        "https://www.tiktok.com/@samtemp1e/video/7299327548671003947"
    )
    profile = service.list_profiles()[0]

    assert row["account_url"] == "https://www.tiktok.com/@samtemp1e"
    assert profile["username"] == "samtemp1e"
    assert profile["display_name"] == "Sam"
    assert profile["follower_count"] == 1234
    assert profile["likes_count"] == 9876


def test_sparse_video_metadata_does_not_erase_real_profile_counts(
    db, config
):
    service = TikTokAppService(db, config, ROOT)
    account = "https://www.tiktok.com/@hollycd17"
    service._upsert_profile(
        {
            "uploader": "hollycd17",
            "channel_follower_count": 122800,
            "following_count": 27,
            "profile_like_count": 1000000,
            "video_count": 852,
        },
        account,
    )

    service._upsert_profile({"uploader": "hollycd17"}, account)
    profile = service.list_profiles()[0]

    assert profile["follower_count"] == 122800
    assert profile["following_count"] == 27
    assert profile["likes_count"] == 1000000
    assert profile["video_count"] == 852


def test_zero_stat_profile_is_authoritatively_backfilled(db, config):
    service = TikTokAppService(db, config, ROOT)
    profile = service._upsert_profile(
        {"uploader": "samtemp1e"},
        "https://www.tiktok.com/@samtemp1e",
    )
    calls = []

    def refresh(account_url):
        calls.append(account_url)
        updated = service._upsert_profile(
            {
                "uploader": "samtemp1e",
                "channel_follower_count": 58400,
                "following_count": 306,
                "profile_like_count": 1000000,
                "video_count": 80,
            },
            account_url,
        )
        return {"profile": updated}

    service.refresh_account_profile = refresh
    updated = service._ensure_profile_counts(profile)

    assert calls == ["https://www.tiktok.com/@samtemp1e"]
    assert updated["follower_count"] == 58400
    assert updated["likes_count"] == 1000000


def test_profile_backfill_repairs_older_url_imports(db, config, tmp_dir):
    service = TikTokAppService(db, config, ROOT)
    source = Path(tmp_dir) / "7252110364777696555.mp4"
    source.write_bytes(b"video")
    source.with_suffix(".info.json").write_text(
        '{"id":"7252110364777696555","uploader":"ellaluvsalisa",'
        '"uploader_url":"https://www.tiktok.com/@ellaluvsalisa",'
        '"channel":"ella🪬"}'
    )
    row = service.add_video(source, remote_id="7252110364777696555")

    profiles = service.list_profiles()
    repaired = service.get_video(row["id"])
    assert repaired["creator"] == "@ellaluvsalisa"
    assert repaired["account_url"] == "https://www.tiktok.com/@ellaluvsalisa"
    assert profiles[0]["display_name"] == "ella🪬"


def test_tiktok_download_retries_with_alternate_mobile_client(
    db, config, tmp_dir, monkeypatch
):
    service = TikTokAppService(db, config, ROOT)
    managed = Path(tmp_dir) / "downloads"
    calls = []

    def fake_run(command, **_kwargs):
        calls.append(command)
        if len(calls) == 1:
            raise subprocess.CalledProcessError(1, command, stderr="hydration error")
        media = managed / "7188154528959696134.mp4"
        media.write_bytes(b"video")
        (managed / "7188154528959696134.info.json").write_text(
            json.dumps({"id": "7188154528959696134", "uploader": "jomini"})
        )
        thumbnail = managed / "7188154528959696134.jpg"
        thumbnail.write_bytes(b"jpg")
        return SimpleNamespace(stdout=str(media) + "\n")

    monkeypatch.setattr(subprocess, "run", fake_run)
    monkeypatch.setattr(tiktok_app_module, "_has_playable_streams", lambda *_args: True)
    monkeypatch.setattr(
        tiktok_app_module, "urlopen",
        lambda *_args, **_kwargs: (_ for _ in ()).throw(OSError("offline")),
    )
    media, info, _thumbnail = service._download(
        "https://www.tiktok.com/@jomini/video/7188154528959696134",
        managed,
    )

    assert media.name == "7188154528959696134.mp4"
    assert info["uploader"] == "jomini"
    assert calls[0][0].endswith("rockpod/.venv/bin/yt-dlp")
    assert "--extractor-args" in calls[1]
    assert "tiktok:app_info=" in " ".join(calls[1])


def test_tiktok_download_accepts_complete_files_from_failed_attempt(
    db, config, tmp_dir, monkeypatch
):
    service = TikTokAppService(db, config, ROOT)
    managed = Path(tmp_dir) / "downloads"
    video_id = "7674711434432974111"

    def fake_run(command, **_kwargs):
        managed.mkdir(parents=True, exist_ok=True)
        (managed / f"{video_id}.mp4").write_bytes(b"video")
        (managed / f"{video_id}.info.json").write_text(
            json.dumps({"id": video_id, "uploader": "calypso.solara"})
        )
        (managed / f"{video_id}.jpg").write_bytes(b"jpg")
        raise subprocess.CalledProcessError(
            1, command, stderr="webpage challenge failed after download"
        )

    monkeypatch.setattr(subprocess, "run", fake_run)
    monkeypatch.setattr(tiktok_app_module, "_has_playable_streams", lambda *_args: True)
    monkeypatch.setattr(
        tiktok_app_module, "urlopen",
        lambda *_args, **_kwargs: (_ for _ in ()).throw(OSError("offline")),
    )
    media, info, thumbnail = service._download(
        f"https://www.tiktok.com/@calypso.solara/video/{video_id}", managed
    )

    assert media.name == f"{video_id}.mp4"
    assert thumbnail.name == f"{video_id}.jpg"
    assert info["uploader"] == "calypso.solara"


def test_following_refresh_keeps_newest_ten_without_touching_manual(
    db, config, tmp_dir
):
    service = TikTokAppService(db, config, ROOT)
    account = "https://www.tiktok.com/@samtemp1e"
    service.add_account_sync(account)
    manual_source = Path(tmp_dir) / "manual.mpg"
    manual_source.write_bytes(b"manual")
    manual = service.add_video(manual_source, title="Keep me")
    old_source = Path(tmp_dir) / "old.mpg"
    old_source.write_bytes(b"old")
    old = service.add_video(
        old_source,
        remote_id="7000000000000000000",
        source_type="following",
        account_url=account,
    )
    ids = [str(7299327548671003947 + index) for index in range(10)]
    service._account_uploads = lambda _account, _limit: [
        f"{account}/video/{video_id}" for video_id in ids
    ]

    def import_video(url, source_type="manual", account_url=""):
        video_id = url.rsplit("/", 1)[-1]
        source = Path(tmp_dir) / f"{video_id}.mpg"
        source.write_bytes(video_id.encode())
        return service.add_video(
            source,
            remote_id=video_id,
            title=video_id,
            source_type=source_type,
            account_url=account_url,
        )

    service.import_url = import_video
    progress = []
    report = service.sync_account_uploads(
        progress_callback=lambda done, total, label: progress.append(
            (done, total, label)
        )
    )
    followed = [
        row for row in service.list_videos() if row["source_type"] == "following"
    ]

    assert report == {
        "accounts": 1, "videos_added": 10, "videos_removed": 1,
        "errors": 0,
    }
    assert len(followed) == 10
    assert service.get_video(old["id"]) is None
    assert service.get_video(manual["id"]) is not None
    assert progress[0][0:2] == (0, 0)
    assert "Checking Following account 1/1" in progress[0][2]
    assert any(total == 10 and "Downloading newest posts" in label
               for _, total, label in progress)
    assert progress[-1] == (10, 10, "Following account 1/1 refreshed")


def test_tiktok_sync_exports_real_feed_fields_and_letterboxed_thumbnail(
    db, config, mock_device, tmp_dir
):
    service = TikTokAppService(db, config, ROOT)
    source = Path(tmp_dir) / "portrait.mpg"
    source.write_bytes(b"mpeg")
    thumbnail = Path(tmp_dir) / "portrait.png"
    Image.new("RGB", (90, 160), "red").save(thumbnail)
    row = service.add_video(
        source,
        remote_id="7299327548671003947",
        title="Test TikTok",
        creator="@samtemp1e",
        description="Real URL test",
        like_count=123,
        comment_count=4,
        thumbnail_path=thumbnail,
    )
    progress = []
    report = service.sync(
        mock_device,
        device=SimpleNamespace(mount_path=mock_device),
        refresh_accounts=False,
        progress_callback=lambda done, total, label: progress.append(
            (done, total, label)
        ),
    )

    assert report["videos"] == 1
    assert progress[0] == (0, 0, "Checking the connected iPod…")
    assert (0, 1, "Preparing 1 TikTok clips…") in progress
    assert any("Checking clip 1/1" in label for _, _, label in progress)
    assert any("Syncing clip 1/1" in label for _, _, label in progress)
    assert progress[-1] == (1, 1, "TikTok sync complete")
    feed = (
        Path(mock_device) / ".rockbox/rocks/apps/.ipodtiktok_feed.tsv"
    ).read_text()
    assert feed.splitlines()[0].endswith("\tcomments\tpin_order")
    assert feed.splitlines()[1].endswith("\t4\t0")
    assert "@samtemp1e\tReal URL test" in feed
    assert "\tmanual\t" in feed
    exported = Image.open(
        Path(mock_device) / f".rockbox/tiktok/thumbnails/{row['id']}.bmp"
    )
    assert exported.size == (96, 72)
    assert exported.getpixel((0, 36)) == (0, 0, 0)
    assert exported.getpixel((48, 36))[0] > 200
    menu_preview = Image.open(
        Path(mock_device) / f".rockbox/tiktok/previews/{row['id']}.bmp"
    )
    assert menu_preview.size == (160, 240)
    assert menu_preview.getpixel((0, 120))[0] > 200
    assert menu_preview.getpixel((159, 120))[0] > 200
    library_line = (
        Path(mock_device) / ".rockbox/tiktok/library.tsv"
    ).read_text().splitlines()[1]
    assert library_line.endswith(f"/.rockbox/tiktok/previews/{row['id']}.bmp")

    media = Path(mock_device) / f"TikTok/videos/{row['id']}.mpg"
    thumb = Path(mock_device) / f".rockbox/tiktok/thumbnails/{row['id']}.bmp"
    feed_path = Path(mock_device) / ".rockbox/rocks/apps/.ipodtiktok_feed.tsv"
    mtimes = (media.stat().st_mtime_ns, thumb.stat().st_mtime_ns,
              feed_path.stat().st_mtime_ns)
    second = service.sync(mock_device, refresh_accounts=False)

    assert second["media_updated"] == 0
    assert second["media_unchanged"] == 1
    assert second["thumbnails_updated"] == 0
    assert second["thumbnails_unchanged"] == 1
    assert second["previews_updated"] == 0
    assert second["previews_unchanged"] == 1
    assert second["state_files_updated"] == 0
    assert mtimes == (media.stat().st_mtime_ns, thumb.stat().st_mtime_ns,
                      feed_path.stat().st_mtime_ns)

    # Libraries upgraded from schema v21 already have a trustworthy media
    # signature but no separate thumbnail signature. Seed it without making
    # every existing thumbnail go through a one-time rebuild.
    db.execute(
        "UPDATE tiktok_videos SET last_synced_thumbnail_hash='' WHERE id=?",
        (row["id"],),
    )
    third = service.sync(mock_device, refresh_accounts=False)
    migrated = service.get_video(row["id"])
    assert third["media_updated"] == 0
    assert third["thumbnails_updated"] == 0
    assert third["thumbnails_unchanged"] == 1
    assert third["state_files_updated"] == 0
    assert migrated["last_synced_thumbnail_hash"]
    assert mtimes == (media.stat().st_mtime_ns, thumb.stat().st_mtime_ns,
                      feed_path.stat().st_mtime_ns)


def test_tiktok_h264_request_still_syncs_mpeg(
    db, config, mock_device, tmp_dir, monkeypatch,
):
    service = TikTokAppService(db, config, ROOT)
    source = Path(tmp_dir) / "switch.mpg"
    source.write_bytes(b"source")
    thumbnail = Path(tmp_dir) / "switch.png"
    Image.new("RGB", (90, 160), "cyan").save(thumbnail)
    row = service.add_video(
        source, title="Switch Me", creator="@creator",
        thumbnail_path=thumbnail,
    )

    def fake_stage(_source, target, **kwargs):
        Path(target).write_bytes(kwargs["profile"].encode("ascii"))

    monkeypatch.setattr(tiktok_app_module, "stage_app_video", fake_stage)
    monkeypatch.setattr(
        tiktok_app_module.subprocess, "run", lambda *a, **k: None,
    )
    media = Path(mock_device) / "TikTok/videos"

    service.sync(
        mock_device, refresh_accounts=False,
        video_profile="h264_apple_exact",
    )

    assert (media / f"{row['id']}.mpg").read_bytes() == b"quality"
    assert not (media / f"{row['id']}.m4v").exists()
    assert (media / f"{row['id']}.ttm").is_file()
    feed = (
        Path(mock_device) / ".rockbox/rocks/apps/.ipodtiktok_feed.tsv"
    ).read_text(encoding="utf-8")
    assert f"/TikTok/videos/{row['id']}.mpg" in feed


def test_tiktok_is_standalone_stock_clickwheel_app():
    plugin = (ROOT / "apps/plugins/ipodtiktok.c").read_text()
    player = (ROOT / "apps/plugins/mpegplayer/mpegplayer.c").read_text()
    video_out = (
        ROOT / "apps/plugins/mpegplayer/video_out_rockbox.c"
    ).read_text()
    menu = (ROOT / "apps/root_menu.c").read_text()
    offlineweb = (ROOT / "apps/plugins/offlineweb.c").read_text()

    assert 'PLUGIN_APPS_DIR "/ipodtiktok.rock"' in menu
    assert '{ "tiktok", &tiktok_item }' in menu
    assert "IPODJS_PREVIEW_TIKTOK" in menu
    assert "root_menu_video_preview_load_tiktok_paths" in menu
    assert "fields[8][0] ? fields[8] : fields[7]" in menu
    assert "TIKTOK_PREVIEW_ROOT" in (
        ROOT / "rockpod/services/tiktok_app.py"
    ).read_text()
    assert "FEED_SWIPE_STEPS        2" in player
    assert "FEED_SWIPE_ANIM_TIME    MAX(1, HZ / 5)" in player
    assert "VIDEO_REPEAT" in player
    assert "reaching EOS keeps the same card" in player
    assert "goto feed_repeat_playback" in player
    assert "exposes the YUV key colour as a green flash" in player
    assert "progress * progress * (768 - 2 * progress)" in player
    assert "feed_yuv_mode_active" in player
    assert "feed_swipe_allowed(" in player
    assert "feed_animate_swipe();" in player
    assert "transition_enter_pending" in player
    assert "mpegplayer_yuv_overlay_offset" in player
    assert "vo_push_yuv_overlay" in video_out
    assert "vo_transition_y" in video_out
    assert "old card exits upward, new card enters from below" in video_out
    assert "feed_profile_items[ordinal]" in player
    assert "feed_profile_thumb_start + 1" in player
    assert "feed.select_armed" in player
    assert "feed_cycle_section(-1)" in player
    assert 'source_type, "archive"' in player
    assert "feed_show_current_profile(false)" in player
    assert "FEED_SAVED_PROFILE_MAX" in player
    assert "feed_build_saved_profiles" in player
    assert "feed_show_current_profile(true)" in player
    assert "feed_leave_saved_profiles_for_section(-1)" in player
    assert "History <-> Saved loop" in player
    assert "feed.profile_saved_only && !feed.items[i].saved" in player
    assert '"Saved profiles"' in player
    assert "IPODTIKTOK_PROFILES_PATH" in player
    assert "feed.profile_visible" in player
    assert "feed.profile_cursor" in player
    assert "feed_profile_move(+1)" in player
    assert "feed_profile_move(-1)" in player
    assert "feed_profile_item_index(feed.profile_cursor)" in player
    assert "Strip unsupported UTF-8" in player
    assert "profile_selection_pending" in player
    assert "profile_feed_active" in player
    assert "creator page owns the swipe queue" in player
    assert "FEED_PROFILE_THUMB_SLOTS" in player
    assert "feed_profile_prepare_thumbnails" in player
    assert "feed_profile_service_assets" in player
    assert "rb->button_queue_count() != 0" in player
    assert "feed_flush_pending" in player
    assert "feed.items[index].thumbnail" in player
    assert "A second RGB post-frame redraw" in player
    assert "FEED_RECENT_MAX         16" in player
    assert "FEED_RECENT_CREATORS_MAX 12" in player
    assert "feed_recommend_next_index()" in player
    assert "feed_creator_was_recent" in player
    assert "feed_prepare_neighbors" in player
    assert "feed_prepared_neighbors_valid" in player
    assert "feed.swipe_committed" in player
    assert "feed_prepared_boundary" in player
    assert "feed_animate_edge_resistance" in player
    assert "mpegplayer_yuv_overlay_resistance_offset" in player
    assert "vo_resist_yuv_overlay" in video_out
    assert "feed_release_primed_transition" in player
    assert "stream_play_primed()" in player
    assert "stream_primed_audio_ready()" in player
    assert "feed_remember_section" in player
    assert "feed_restore_section" in player
    assert 'rb->fdprintf(fd, "C\\t%d\\t%s\\n"' in player
    assert "Keeping\n         * them out made every newly followed account invisible" in player
    assert "FEED_SECTION_SAVED" in player
    assert "FEED_SECTION_HISTORY" in player
    assert ".ipodtiktok_activity.dat" in player
    assert "feed.items[i].not_interested" in player
    assert "feed_show_actions();" in player
    assert '"Not interested"' in player
    assert "feed_profile_avatar_valid" in player
    assert '"PINNED"' in player
    assert "liked_creators[FEED_RECENT_MAX]" in player
    assert "rescanning the entire feed per candidate stalls" in player
    assert "vo.output_y < overlay_bottom" in video_out
    assert "only LCD blit for this frame" in video_out
    assert "!mpegplayer_livetv_desktop && !feed.active" in player
    assert "appears as a green flash" in player
    assert "ipodtiktok_heart_outline" in player
    assert "MPEG_VIDEO_DISPLAY_FIT" in player
    assert "ACTION_STD_CANCEL" in player
    assert "IPODTIKTOK_PARAM_PREFIX" in plugin
    assert "IPODTIKTOK_PLAYER_PATH" in plugin
    assert "plugin_video_player_for" not in plugin
    assert 'ROCKBOX_DIR "/tiktok"' not in offlineweb


def test_rockpod_tiktok_page_has_direct_url_and_profile_controls():
    panel = (ROOT / "rockpod/ui/tiktok_panel.py").read_text()
    assert 'QLabel("TikTok video URL:")' in panel
    assert 'QPushButton("Import URL")' in panel
    assert 'QLabel("Account URL:")' in panel
    assert 'QPushButton("Follow + fetch profile")' in panel
    assert 'self.tabs.addTab(self._build_profiles_tab(), "Profiles")' in panel
    assert "self.archive_table.setUpdatesEnabled(False)" in panel
    assert "0, QHeaderView.Stretch" in panel
    assert "self.archive_table.resizeColumnsToContents()" not in panel
    assert "TikTokArchiveJob" in panel
    assert "QProgressDialog" in panel
    assert "dict(config) raises TypeError" in panel
    assert "archive_storage_estimate" in panel
    assert "TikTokSyncJob" in panel
    assert 'Signal(int, int, str)' in panel
    assert 'QProgressDialog(' in panel
    assert 'QPushButton("Sync Selected")' in panel
    assert 'QPushButton("Sync All")' in panel
    assert 'QPushButton("Sync as H.264")' not in panel
    assert "self.sync_mpeg_button.setEnabled(False)" in panel
    assert "sync_h264_button" not in panel
    assert "progress_callback=self.signals.progress.emit" in panel


def test_sync_worker_snapshots_config_object(config):
    from ui.tiktok_panel import TikTokSyncJob

    job = TikTokSyncJob(
        config.db_path, config, ROOT, "/tmp/mock-ipod",
    )

    assert job.config["db_path"] == config.db_path
    assert job.config["cache_dir"] == config.cache_dir
    assert job.mount_path == "/tmp/mock-ipod"
    assert not hasattr(job, "video_profile")


def test_archive_worker_snapshots_config_object(config):
    from ui.tiktok_panel import TikTokArchiveJob

    job = TikTokArchiveJob(
        config.db_path,
        config,
        ROOT,
        "https://www.tiktok.com/@taylortomlinsoncomedy",
    )

    assert job.config["db_path"] == config.db_path
    assert job.config["cache_dir"] == config.cache_dir


def test_archive_storage_estimate_reuses_existing_media(
    db, config, tmp_dir, monkeypatch
):
    monkeypatch.setattr(Path, "home", lambda: Path(tmp_dir))
    service = TikTokAppService(db, config, ROOT)
    account_url = "https://www.tiktok.com/@creator"
    db.execute(
        "INSERT INTO tiktok_profiles (account_url, username, video_count) "
        "VALUES (?, ?, ?)",
        (account_url, "creator", 5),
    )
    managed = Path(tmp_dir) / ".rockpod/tiktok/following/creator"
    managed.mkdir(parents=True)
    (managed / "one.mp4").write_bytes(b"x" * 100)
    (managed / "two.webm").write_bytes(b"x" * 300)

    estimate = service.archive_storage_estimate(account_url)

    assert estimate["archived"] == 2
    assert estimate["remaining"] == 3
    assert estimate["downloaded_bytes"] == 400
    assert estimate["estimated_remaining_bytes"] == 600

"""Tests for online artwork lookup and device cover export."""

import io
import os
import threading
import time
import xml.etree.ElementTree as ET

from PIL import Image

from services.artwork_manager import ArtworkManager
from services.device_detector import DeviceDetector, DeviceInfo, create_mock_device
from services.device_inventory import device_record_from_info
from services.online_artwork import ITunesArtworkLookup, OnlineArtworkRateLimitError
from services.sync_engine import SyncEngine


def _temp_names(path):
    return [name for name in os.listdir(path) if name.startswith("tmp")]


class FakeLookupClient:
    def __init__(self, succeed=True):
        self.succeed = succeed
        self.search_calls = []
        self.video_search_calls = []
        self.download_calls = []

    def search_album_art(self, album_title, artist_name, limit=8):
        self.search_calls.append((album_title, artist_name, limit))
        if not self.succeed:
            return None
        return {
            "album": album_title,
            "artist": artist_name,
            "collection_id": 1,
            "artwork_url": "https://example.com/1200.jpg",
            "preview_url": "https://example.com/100.jpg",
            "source": "itunes_search",
        }

    def search_video_art(self, title, video_kind="movie", season_label="", limit=8):
        self.video_search_calls.append((title, video_kind, season_label, limit))
        if not self.succeed:
            return None
        return {
            "title": title,
            "artist": season_label,
            "collection_id": 1,
            "artwork_url": "https://example.com/video-1200.jpg",
            "preview_url": "https://example.com/video-100.jpg",
            "source": "itunes_video_search",
        }

    def search_video_art_relaxed(self, title, video_kind="movie", season_label="", limit=20):
        self.video_search_calls.append((title, video_kind, season_label, limit, "relaxed"))
        if not self.succeed:
            return None
        return {
            "title": title,
            "artist": season_label,
            "collection_id": 1,
            "artwork_url": "https://example.com/video-1200.jpg",
            "preview_url": "https://example.com/video-100.jpg",
            "source": "itunes_video_search_relaxed",
        }

    def download_image(self, url):
        self.download_calls.append(url)
        img = Image.new("RGB", (1200, 1200), "#557799")
        buf = io.BytesIO()
        img.save(buf, "JPEG")
        return buf.getvalue(), "image/jpeg"


class RateLimitedLookupClient(FakeLookupClient):
    def search_album_art(self, album_title, artist_name, limit=8):
        self.search_calls.append((album_title, artist_name, limit))
        raise OnlineArtworkRateLimitError("HTTP 429 Too Many Requests")


class VariantLookupClient(FakeLookupClient):
    def search_album_art(self, album_title, artist_name, limit=8):
        self.search_calls.append((album_title, artist_name, limit))
        if album_title == "WHEN WE ALL FALL ASLEEP WHERE DO WE GO" and artist_name == "Billie Eilish":
            return {
                "album": album_title,
                "artist": artist_name,
                "collection_id": 1,
                "artwork_url": "https://example.com/1200.jpg",
                "preview_url": "https://example.com/100.jpg",
                "source": "itunes_search",
            }
        return None


class VideoInfoLookupClient(FakeLookupClient):
    def __init__(self, succeed=True):
        super().__init__(succeed=succeed)
        self.video_info_calls = []

    def search_video_art_from_info(self, video_info, limit=8, relaxed=False):
        self.video_info_calls.append((video_info, limit, relaxed))
        if not self.succeed:
            return None
        return {
            "title": video_info.get("album", ""),
            "artist": video_info.get("artist", ""),
            "collection_id": 1,
            "artwork_url": "https://example.com/video-1200.jpg",
            "preview_url": "https://example.com/video-100.jpg",
            "source": "plex_search",
        }


def _album_info(audio_path):
    return {
        "group_key": "artist\0album",
        "album": "Album",
        "artist": "Artist",
        "tracks": [
            {"file_path": audio_path, "album": "Album", "artist": "Artist", "album_artist": "Artist", "has_embedded_artwork": 0}
        ],
    }


def _album_info_named(audio_path, album, artist):
    return {
        "group_key": f"{artist}\0{album}",
        "album": album,
        "artist": artist,
        "tracks": [
            {"file_path": audio_path, "album": album, "artist": artist, "album_artist": artist, "has_embedded_artwork": 0}
        ],
    }


def _video_info_named(video_path, title, video_kind="movie", season_label=""):
    track = {
        "file_path": video_path,
        "media_type": "video",
        "video_kind": video_kind,
        "title": title,
        "show_title": title if video_kind == "show" else "",
        "album": season_label,
        "_video_scope": "show" if video_kind == "show" else video_kind,
    }
    return {
        "group_key": f"{video_kind}\0{title}\0{season_label}",
        "album": title,
        "artist": season_label,
        "tracks": [track],
        "media_type": "video",
        "video_kind": video_kind,
        "video_scope": "show" if video_kind == "show" else video_kind,
    }


def test_missing_local_artwork_triggers_online_lookup_and_caches_hires(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        path = manager.fetch_online_artwork_now(_album_info(audio))

        assert os.path.exists(path)
        meta = manager._load_album_meta("artist\0album")
        assert meta["source"] == "online"
        assert meta["desktop_source_type"] == "online"
        assert os.path.exists(meta["desktop_source_art_path"])
        assert meta["desktop_source_resolution"] == [1200, 1200]
        assert meta["online_selected_url"] == "https://example.com/1200.jpg"
        assert not _temp_names(os.path.dirname(manager._meta_path("artist\0album")))
        assert lookup.search_calls
        assert lookup.download_calls
    finally:
        manager.shutdown()


def test_missing_video_poster_triggers_online_lookup_and_caches_hires(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        video = os.path.join(tmp_dir, "Shows", "The Show", "Season 1", "Episode 1.mkv")
        os.makedirs(os.path.dirname(video), exist_ok=True)
        with open(video, "wb") as f:
            f.write(b"video")

        path = manager.fetch_online_artwork_now(_video_info_named(video, "The Show", "show", "Season 1"))
        poster = manager.get_video_poster(_video_info_named(video, "The Show", "show", "Season 1"), "thumb", allow_online=False)

        assert os.path.exists(path)
        assert os.path.exists(poster)
        meta = manager._load_album_meta("show\0The Show\0Season 1")
        assert meta["source"] == "online"
        assert meta["desktop_source_type"] == "online"
        assert meta["desktop_source_resolution"] == [1200, 1200]
        assert os.path.basename(meta["desktop_thumb_path"]).startswith("video_")
        assert os.path.basename(meta["desktop_display_path"]).startswith("video_")
        assert meta["desktop_thumb_resolution"] == [180, 270]
        assert meta["desktop_display_resolution"] == [360, 540]
        assert lookup.video_search_calls
        with Image.open(poster) as img:
            assert img.size == (180, 270)
    finally:
        manager.shutdown()


def test_get_video_poster_does_not_auto_queue_online_lookup(config, tmp_dir, monkeypatch):
    config.enable_online_artwork_lookup = True
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        video = os.path.join(tmp_dir, "Shows", "The Show", "Season 1", "Episode 1.mkv")
        os.makedirs(os.path.dirname(video), exist_ok=True)
        with open(video, "wb") as f:
            f.write(b"video")

        queued = []

        def fake_queue(*args, **kwargs):
            queued.append((args, kwargs))
            return True

        monkeypatch.setattr(manager, "queue_online_lookup", fake_queue)

        poster = manager.get_video_poster(_video_info_named(video, "The Show", "show", "Season 1"), "thumb")

        assert poster == ""
        assert queued == []
    finally:
        manager.shutdown()


def test_get_video_poster_repairs_stale_square_video_meta(config, tmp_dir):
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient())
    try:
        video = os.path.join(tmp_dir, "Shows", "The Show", "Season 1", "Episode 1.mkv")
        os.makedirs(os.path.dirname(video), exist_ok=True)
        with open(video, "wb") as f:
            f.write(b"video")

        source_path = os.path.join(tmp_dir, "art", "albums", "originals", "source.jpg")
        os.makedirs(os.path.dirname(source_path), exist_ok=True)
        Image.new("RGB", (1200, 1200), "#446688").save(source_path, "JPEG")
        stale_display = os.path.join(tmp_dir, "art", "display", "album_stale.jpg")
        os.makedirs(os.path.dirname(stale_display), exist_ok=True)
        Image.new("RGB", (300, 300), "#557799").save(stale_display, "JPEG")

        manager._save_album_meta(
            "show\0The Show\0Season 1",
            {
                "desktop_source_art_path": source_path,
                "desktop_source_type": "online",
                "desktop_source_resolution": [1200, 1200],
                "desktop_display_path": stale_display,
                "desktop_display_resolution": [300, 300],
            },
        )

        poster = manager.get_video_poster(
            _video_info_named(video, "The Show", "show", "Season 1"),
            "thumb",
            allow_online=False,
        )

        meta = manager._load_album_meta("show\0The Show\0Season 1")
        assert os.path.exists(poster)
        assert os.path.basename(meta["desktop_thumb_path"]).startswith("video_")
        assert os.path.basename(meta["desktop_display_path"]).startswith("video_")
        assert meta["desktop_thumb_resolution"] == [180, 270]
        assert meta["desktop_display_resolution"] == [360, 540]
    finally:
        manager.shutdown()


def test_get_artwork_path_for_video_uses_show_poster_target_not_album_lookup(config, tmp_dir, monkeypatch):
    config.enable_online_artwork_lookup = True
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient())
    try:
        video = os.path.join(tmp_dir, "Shows", "The Show", "Season 1", "Episode 1.mkv")
        os.makedirs(os.path.dirname(video), exist_ok=True)
        with open(video, "wb") as f:
            f.write(b"video")

        calls = []

        def fake_get_video_poster(video_info, size="thumb", allow_online=None):
            calls.append((video_info, size, allow_online))
            return ""

        def fail_queue(*args, **kwargs):
            raise AssertionError("video artwork path should not queue album artwork lookups")

        monkeypatch.setattr(manager, "get_video_poster", fake_get_video_poster)
        monkeypatch.setattr(manager, "queue_online_lookup", fail_queue)

        path = manager.get_artwork_path(
            {
                "file_path": video,
                "media_type": "video",
                "video_kind": "show",
                "show_title": "The Show",
                "album": "Season 1",
                "artist": "The Show",
            },
            "thumb",
        )

        assert path
        assert calls
        assert calls[0][0]["group_key"] == "show:the show"
        assert calls[0][0]["album"] == "The Show"
        assert calls[0][0]["artist"] == ""
        assert calls[0][2] is False
    finally:
        manager.shutdown()


def test_search_video_art_tries_episode_fallback_for_show(monkeypatch):
    lookup = ITunesArtworkLookup()
    calls = []

    def fake_search(term, title, season_label, media, entity, video_kind, limit=8, relaxed=False):
        calls.append((term, media, entity, video_kind))
        if entity == "tvEpisode":
            return {
                "trackName": "Pilot",
                "artistName": "The Show",
                "trackId": 1,
                "artworkUrl100": "https://example.com/video-100.jpg",
            }
        return None

    monkeypatch.setattr(lookup, "_search_video_term", fake_search)

    result = lookup.search_video_art("The Show", video_kind="show")

    assert result is not None
    assert ("The Show", "tvShow", "tvEpisode", "show") in calls


def test_search_video_art_from_info_prefers_plex_exact_file_match(monkeypatch, tmp_dir):
    monkeypatch.setattr(ITunesArtworkLookup, "_discover_plex_token", staticmethod(lambda: "plex-token"))
    lookup = ITunesArtworkLookup(timeout=0.1)
    episode_path = os.path.join(tmp_dir, "Shows", "6teen", "Season 1", "Episode 1.mkv")
    os.makedirs(os.path.dirname(episode_path), exist_ok=True)
    with open(episode_path, "wb") as f:
        f.write(b"video")

    root = ET.fromstring(
        f"""
        <MediaContainer size="2">
          <Directory type="show" title="6teen" thumb="/library/metadata/1/thumb/1" ratingKey="1" />
          <Video type="episode" title="Take This Job and Squeeze It" grandparentTitle="6teen"
                 parentTitle="Season 1" grandparentThumb="/library/metadata/1/thumb/1" ratingKey="2">
            <Media><Part file="{episode_path}" /></Media>
          </Video>
        </MediaContainer>
        """
    )

    monkeypatch.setattr(lookup, "_plex_search", lambda query, limit=8: root)

    result = lookup.search_video_art_from_info(
        _video_info_named(episode_path, "6teen", "show", "Season 1"),
        limit=8,
        relaxed=False,
    )

    assert result is not None
    assert result["source"] == "plex_search"
    assert result["artwork_url"] == "http://127.0.0.1:32400/library/metadata/1/thumb/1?X-Plex-Token=plex-token"


def test_search_video_art_from_info_follows_show_to_all_leaves_for_exact_file_match(monkeypatch, tmp_dir):
    monkeypatch.setattr(ITunesArtworkLookup, "_discover_plex_token", staticmethod(lambda: "plex-token"))
    lookup = ITunesArtworkLookup(timeout=0.1)
    episode_path = os.path.join(tmp_dir, "Shows", "6teen", "Season 1", "Episode 1.mkv")
    os.makedirs(os.path.dirname(episode_path), exist_ok=True)
    with open(episode_path, "wb") as f:
        f.write(b"video")

    search_root = ET.fromstring(
        """
        <MediaContainer size="1">
          <Directory type="show" title="6teen" ratingKey="4272" thumb="/library/metadata/4272/thumb/1" />
        </MediaContainer>
        """
    )
    leaves_root = ET.fromstring(
        f"""
        <MediaContainer size="1">
          <Video type="episode" title="Take This Job And Squeeze It" grandparentTitle="6teen"
                 grandparentThumb="/library/metadata/4272/thumb/1" ratingKey="4278">
            <Media><Part file="{episode_path}" /></Media>
          </Video>
        </MediaContainer>
        """
    )

    def fake_fetch(path, params=None):
        if path == "/search":
            return search_root
        if path == "/library/metadata/4272/allLeaves":
            return leaves_root
        raise AssertionError(path)

    monkeypatch.setattr(lookup, "_plex_fetch_metadata_xml", fake_fetch)

    result = lookup.search_video_art_from_info(
        _video_info_named(episode_path, "6teen", "show", "Season 1"),
        limit=8,
        relaxed=False,
    )

    assert result is not None
    assert result["source"] == "plex_search"
    assert result["artwork_url"] == "http://127.0.0.1:32400/library/metadata/4272/thumb/1?X-Plex-Token=plex-token"


def test_artwork_manager_prefers_search_video_art_from_info(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    lookup = VideoInfoLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        video = os.path.join(tmp_dir, "Shows", "The Show", "Season 1", "Episode 1.mkv")
        os.makedirs(os.path.dirname(video), exist_ok=True)
        with open(video, "wb") as f:
            f.write(b"video")

        path = manager.fetch_online_artwork_now(_video_info_named(video, "The Show", "show", "Season 1"))

        assert os.path.exists(path)
        assert lookup.video_info_calls
        assert lookup.video_info_calls[0][0]["album"] == "The Show"
        assert lookup.video_info_calls[0][0]["tracks"][0]["file_path"] == video
    finally:
        manager.shutdown()


def test_pick_best_video_match_accepts_show_match_without_season_label():
    lookup = ITunesArtworkLookup()

    result = lookup._pick_best_video_match(
        "Death Note (2006)",
        "",
        [
            {
                "artistName": "Death Note",
                "collectionName": "Season 1",
                "trackName": "Episode 1",
                "kind": "tv season",
                "wrapperType": "track",
            }
        ],
        "show",
    )

    assert result is not None
    assert result["artistName"] == "Death Note"


def test_pick_best_video_match_relaxed_accepts_partial_word_overlap():
    lookup = ITunesArtworkLookup()

    result = lookup._pick_best_video_match(
        "Death Note (2006)",
        "",
        [
            {
                "artistName": "Death Note",
                "collectionName": "",
                "trackName": "",
                "kind": "tv season",
                "wrapperType": "track",
            }
        ],
        "show",
        relaxed=True,
    )

    assert result is not None
    assert result["artistName"] == "Death Note"


def test_force_video_lookup_uses_relaxed_fallback_when_strict_match_fails(config, tmp_dir):
    config.enable_online_artwork_lookup = True

    class RelaxedOnlyLookup(FakeLookupClient):
        def search_video_art(self, title, video_kind="movie", season_label="", limit=8):
            self.video_search_calls.append((title, video_kind, season_label, limit, "strict"))
            return None

    lookup = RelaxedOnlyLookup()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        video = os.path.join(tmp_dir, "Shows", "The Show", "Season 1", "Episode 1.mkv")
        os.makedirs(os.path.dirname(video), exist_ok=True)
        with open(video, "wb") as f:
            f.write(b"video")

        path = manager.fetch_online_artwork_now(_video_info_named(video, "The Show", "show", "Season 1"), force=True)

        assert os.path.exists(path)
        assert any(call[-1] == "strict" for call in lookup.video_search_calls)
        assert any(call[-1] == "relaxed" for call in lookup.video_search_calls)
    finally:
        manager.shutdown()


def test_video_lookup_preserves_explicit_blank_artist_from_ui_target(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        video = os.path.join(tmp_dir, "Shows", "The Show", "Season 1", "Episode 1.mkv")
        os.makedirs(os.path.dirname(video), exist_ok=True)
        with open(video, "wb") as f:
            f.write(b"video")

        target = {
            "group_key": "show:the show",
            "album": "The Show",
            "artist": "",
            "tracks": [
                {
                    "file_path": video,
                    "media_type": "video",
                    "video_kind": "show",
                    "show_title": "The Show",
                    "artist": "Wrong Artist",
                    "album_artist": "Wrong Artist",
                    "_video_scope": "show",
                }
            ],
            "media_type": "video",
            "video_kind": "show",
            "video_scope": "show",
        }

        manager.fetch_online_artwork_now(target, force=True)

        assert lookup.video_search_calls
        assert lookup.video_search_calls[0] == ("The Show", "show", "", 8)
    finally:
        manager.shutdown()


def test_standard_cover_generated_for_ipod(config, tmp_dir):
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        manager.fetch_online_artwork_now(_album_info(audio), force=True)
        cover_path, cover_hash = manager.export_device_cover(_album_info(audio))

        assert os.path.exists(cover_path)
        assert cover_hash
        with Image.open(cover_path) as img:
            assert img.format == "JPEG"
            assert img.width <= 320
            assert img.height <= 320
    finally:
        manager.shutdown()


def test_album_list_thumbnail_and_manifest_generated_for_ipod(config, tmp_dir):
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config)
    try:
        album_dir = os.path.join(tmp_dir, "Album")
        audio = os.path.join(album_dir, "01.mp3")
        os.makedirs(album_dir, exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")
        Image.new("RGB", (500, 300), "#225588").save(os.path.join(album_dir, "folder.jpg"), "JPEG")

        thumb_path, thumb_hash, device_name, album_id = manager.export_album_list_thumbnail(_album_info(audio))

        assert os.path.exists(thumb_path)
        assert thumb_hash
        assert device_name == f"{album_id}.bmp"
        with Image.open(thumb_path) as img:
            assert img.format == "BMP"
            assert img.size == (32, 32)

        manifest_path, manifest_hash = manager.export_album_list_manifest(
            [
                {
                    "album_id": album_id,
                    "thumb": os.path.join("thumbs", device_name),
                    "artist": "Artist",
                    "album": "Album",
                    "group_key": "artist\0album",
                    "device_dirs": "Music/Artist/Album",
                }
            ]
        )

        assert os.path.exists(manifest_path)
        assert manifest_hash
        assert not _temp_names(os.path.dirname(manifest_path))
        with open(manifest_path, "r", encoding="utf-8") as handle:
            data = handle.read()
        assert "album_id\tthumb\tartist\talbum\tgroup_key\tdevice_dirs" in data
        assert f"{album_id}\tthumbs/{device_name}\tArtist\tAlbum" in data
    finally:
        manager.shutdown()


def test_existing_valid_cover_not_rewritten_unnecessarily(config, tmp_dir):
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        manager.fetch_online_artwork_now(_album_info(audio), force=True)
        cover_path, _ = manager.export_device_cover(_album_info(audio))
        before = os.path.getmtime(cover_path)
        cover_path_2, _ = manager.export_device_cover(_album_info(audio))
        after = os.path.getmtime(cover_path_2)

        assert cover_path == cover_path_2
        assert before == after
    finally:
        manager.shutdown()


def test_artwork_sync_can_happen_without_recopying_music(config, db, tmp_dir):
    config.export_device_cover_jpg = True
    device_path = os.path.join(tmp_dir, "ipod")
    create_mock_device(device_path)
    device = DeviceInfo(device_path)
    device.name = "Test iPod"
    device.is_rockbox = True

    music_path = os.path.join(tmp_dir, "Music", "Artist", "Album", "01 - Song.mp3")
    os.makedirs(os.path.dirname(music_path), exist_ok=True)
    with open(music_path, "wb") as f:
        f.write(b"audio")
    cover = os.path.join(os.path.dirname(music_path), "folder.jpg")
    Image.new("RGB", (500, 500), "#884422").save(cover, "JPEG")

    db.upsert_track(
        {
            "file_path": music_path,
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "file_size": os.path.getsize(music_path),
            "duration": 120.0,
            "metadata_hash": "same",
            "synced_to_device": 1,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "last_synced_metadata_hash": "same",
        }
    )
    db.commit()
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "local_track_id": db.get_track_by_path(music_path)["id"],
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "file_size": os.path.getsize(music_path),
            "duration": 120.0,
            "metadata_hash": "same",
            "present_on_device": 1,
        }
    )
    db.commit()

    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config)
    try:
        detector = DeviceDetector(config)
        detector._current_device = device
        engine = SyncEngine(db, config, detector, manager)
        engine.set_current_device(device)

        plan = engine.build_sync_plan()

        assert plan.copy_count == 0
        assert plan.resync_count == 0
        rel_paths = {item[1] for item in plan.artwork_to_copy}
        assert "Music/Artist/Album/cover.jpg" in rel_paths
        assert ".rockbox/albumlist/index.tsv" in rel_paths
        assert any(path.startswith(".rockbox/albumlist/thumbs/") and path.endswith(".bmp") for path in rel_paths)
    finally:
        manager.shutdown()


def test_failed_online_lookup_degrades_gracefully(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient(succeed=False))
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        result = manager.fetch_online_artwork_now(_album_info(audio))
        art_path = manager.get_artwork_for_album(_album_info(audio), "thumb", allow_online=False)

        assert result == ""
        assert os.path.exists(art_path)
        assert "placeholder" in os.path.basename(art_path)
    finally:
        manager.shutdown()


def test_folder_cover_remains_desktop_source_after_online_fetch(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        album_dir = os.path.join(tmp_dir, "Album")
        audio = os.path.join(album_dir, "01.mp3")
        os.makedirs(album_dir, exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")
        Image.new("RGB", (120, 120), "#664422").save(os.path.join(album_dir, "folder.jpg"), "JPEG")

        before = manager.get_artwork_for_album(_album_info(audio), "display", allow_online=False)
        before_info = manager.inspect_artwork(_album_info(audio))
        assert os.path.exists(before)
        assert before_info["desktop_source_type"] == "folder"
        assert before_info["desktop_source_resolution"] == [120, 120]

        manager.fetch_online_artwork_now(_album_info(audio), force=True)
        after = manager.get_artwork_for_album(_album_info(audio), "display", allow_online=False)
        after_info = manager.inspect_artwork(_album_info(audio))

        assert os.path.exists(after)
        assert after_info["desktop_source_type"] == "folder"
        assert after_info["desktop_source_resolution"] == [120, 120]
        assert after_info["display_is_hi_res_thumb"] is False
    finally:
        manager.shutdown()


def test_lookup_variants_keep_album_only_search_when_artist_missing(config, tmp_dir):
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient())
    try:
        variants = manager._lookup_variants("Riot!", "")

        assert ("Riot!", "") in variants
        assert ("Riot!", "Unknown Artist") not in variants
    finally:
        manager.shutdown()


def test_fetch_online_artwork_now_splits_artist_album_combo_when_artist_missing(config, tmp_dir):
    config.enable_online_artwork_lookup = True

    class CombinedArtistAlbumLookup(FakeLookupClient):
        def search_album_art(self, album_title, artist_name, limit=8):
            self.search_calls.append((album_title, artist_name, limit))
            if album_title == "Riot" and artist_name == "Paramore":
                return {
                    "album": "Riot!",
                    "artist": "Paramore",
                    "collection_id": 1,
                    "artwork_url": "https://example.com/1200.jpg",
                    "preview_url": "https://example.com/100.jpg",
                    "source": "itunes_search",
                }
            return None

    lookup = CombinedArtistAlbumLookup()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        path = manager.fetch_online_artwork_now(_album_info_named(audio, "Paramore - Riot", ""))

        assert os.path.exists(path)
        assert ("Riot", "Paramore", 8) in lookup.search_calls
    finally:
        manager.shutdown()


def test_album_render_queues_online_lookup_for_low_res_local_art(config, tmp_dir, monkeypatch):
    config.enable_online_artwork_lookup = True
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        album_dir = os.path.join(tmp_dir, "Album")
        audio = os.path.join(album_dir, "01.mp3")
        os.makedirs(album_dir, exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")
        Image.new("RGB", (120, 120), "#446688").save(os.path.join(album_dir, "folder.jpg"), "JPEG")

        queued = []

        def fake_queue(album_or_tracks, force=False, priority="high"):
            queued.append((album_or_tracks, force, priority))
            return True

        monkeypatch.setattr(manager, "queue_online_lookup", fake_queue)
        manager.get_artwork_for_album(_album_info(audio), "thumb")

        assert queued
    finally:
        manager.shutdown()


def test_track_artwork_uses_album_desktop_cache_not_device_cover_for_display(config, tmp_dir):
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        manager.fetch_online_artwork_now(_album_info(audio), force=True)
        cover_path, _ = manager.export_device_cover(_album_info(audio), force=True)
        thumb_path = manager.get_artwork_path(
            {
                "file_path": audio,
                "album": "Album",
                "artist": "Artist",
                "album_artist": "Artist",
                "album_group_key": "artist\0album",
                "has_embedded_artwork": 0,
            },
            "thumb",
        )
        info = manager.inspect_artwork(_album_info(audio))

        assert os.path.exists(thumb_path)
        assert os.path.exists(cover_path)
        assert os.path.basename(thumb_path) != os.path.basename(cover_path)
        assert info["desktop_source_type"] == "online"
        assert info["device_cover_from_high_res"] is True
    finally:
        manager.shutdown()


def test_rate_limited_lookup_sets_backoff_and_pause_status(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=RateLimitedLookupClient())
    try:
        messages = []
        manager.artwork_lookup_status.connect(messages.append)
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        result = manager.fetch_online_artwork_now(_album_info(audio))
        meta = manager._load_album_meta("artist\0album")

        assert result == ""
        assert meta["online_status"] == "rate_limited"
        assert float(meta["next_retry_at"]) > 0
        assert any("rate limiting" in msg for msg in messages)
    finally:
        manager.shutdown()


def test_background_queue_respects_background_toggle(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    config.background_artwork_lookup_enabled = False
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient())
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        queued = manager.queue_online_lookup(_album_info(audio), priority="low")
        assert queued is False
    finally:
        manager.shutdown()


def test_manual_queue_online_lookup_bypasses_disabled_toggle(config, tmp_dir):
    config.enable_online_artwork_lookup = False
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient())
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        queued, reason = manager.queue_online_lookup(
            _album_info(audio),
            priority="high",
            with_reason=True,
            manual=True,
        )

        assert queued is True
        assert reason == "queued"
    finally:
        manager.shutdown()


def test_manual_queue_online_lookup_promotes_existing_background_item(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient())
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        queued, reason = manager.queue_online_lookup(
            _album_info(audio),
            priority="low",
            with_reason=True,
        )
        assert queued is True
        assert reason == "queued"

        queued, reason = manager.queue_online_lookup(
            _album_info(audio),
            priority="high",
            with_reason=True,
            manual=True,
        )

        assert queued is True
        assert reason == "promoted"
        with manager._fetch_lock:
            payload = manager._high_queue[0]
        assert payload[0]["group_key"] == "artist\0album"
    finally:
        manager.shutdown()


def test_start_online_lookup_now_bypasses_queue_and_fetches(config, tmp_dir):
    config.enable_online_artwork_lookup = False
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        started, reason = manager.start_online_lookup_now(_album_info(audio), force=True, with_reason=True)
        assert started is True
        assert reason in {"started", "already_running"}

        deadline = time.time() + 2.0
        meta = {}
        while time.time() < deadline:
            meta = manager._load_album_meta("artist\0album")
            if meta.get("online_status") == "success":
                break
            time.sleep(0.01)

        assert meta.get("online_status") == "success"
        assert lookup.search_calls
        assert lookup.download_calls
    finally:
        manager.shutdown()


def test_start_online_lookup_now_removes_queued_item_and_fetches(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        queued, reason = manager.queue_online_lookup(_album_info(audio), priority="low", with_reason=True)
        assert queued is True
        assert reason == "queued"

        started, reason = manager.start_online_lookup_now(_album_info(audio), force=True, with_reason=True)
        assert started is True
        assert reason in {"started", "already_running"}

        deadline = time.time() + 2.0
        meta = {}
        while time.time() < deadline:
            meta = manager._load_album_meta("artist\0album")
            if meta.get("online_status") == "success":
                break
            time.sleep(0.01)

        assert meta.get("online_status") == "success"
    finally:
        manager.shutdown()


def test_refresh_missing_artwork_reports_background_disabled(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    config.background_artwork_lookup_enabled = False
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient())
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        result = manager.refresh_missing_artwork([_album_info(audio)], force=False)

        assert result["queued"] == 0
        assert result["reason"] == "background_disabled"
        assert result["skip_reasons"]["background_disabled"] == 1
    finally:
        manager.shutdown()


def test_queue_online_lookup_returns_skip_reason_for_cooldown(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient(succeed=False))
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        manager._save_album_meta("artist\0album", {"last_attempt_at": manager._timestamp(), "next_retry_at": str(manager._timestamp_float() + 300)})
        queued, reason = manager.queue_online_lookup(_album_info(audio), priority="low", with_reason=True)

        assert queued is False
        assert reason == "cooldown"
    finally:
        manager.shutdown()


def test_query_normalization_strips_suffixes_and_quotes(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    lookup = FakeLookupClient()
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=lookup)
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        variants = manager._lookup_variants('Cheap Thrills -Janis Joplin', 'Paul "Bunyan" Rosenberg feat. Guest')

        assert ("Cheap Thrills", "Paul Bunyan Rosenberg") in variants
    finally:
        manager.shutdown()


def test_effective_artwork_interval_clamps_old_saved_fast_values(config):
    config.online_artwork_min_interval_seconds = 0.5
    assert ArtworkManager._effective_artwork_interval(config) == 60.0


def test_lookup_tries_multiple_query_variants(config, tmp_dir):
    config.enable_online_artwork_lookup = True
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=VariantLookupClient())
    try:
        audio = os.path.join(tmp_dir, "Album", "01.mp3")
        os.makedirs(os.path.dirname(audio), exist_ok=True)
        with open(audio, "wb") as f:
            f.write(b"audio")

        result = manager.fetch_online_artwork_now(
            _album_info_named(audio, "WHEN WE ALL FALL ASLEEP_ WHERE DO WE GO_", "Billie Eilish")
        )

        assert result
        assert len(manager._lookup.search_calls) >= 1
        assert any(call[0] == "WHEN WE ALL FALL ASLEEP WHERE DO WE GO" for call in manager._lookup.search_calls)
    finally:
        manager.shutdown()


def test_queue_respects_max_active_queue_size(config, tmp_dir, monkeypatch):
    config.enable_online_artwork_lookup = True
    config.online_artwork_max_queue_size = 2
    manager = ArtworkManager(os.path.join(tmp_dir, "art"), config=config, lookup_client=FakeLookupClient())
    try:
        gate = threading.Event()

        def slow_lookup(album_info, force=False):
            gate.wait(timeout=0.2)
            return ""

        monkeypatch.setattr(manager, "_perform_lookup", slow_lookup)
        for idx in range(4):
            audio = os.path.join(tmp_dir, f"Album{idx}", "01.mp3")
            os.makedirs(os.path.dirname(audio), exist_ok=True)
            with open(audio, "wb") as f:
                f.write(b"audio")
            manager.queue_online_lookup(_album_info_named(audio, f"Album {idx}", f"Artist {idx}"), priority="low")

        with manager._fetch_lock:
            active = manager._queue_size_locked(include_overflow=False)
            total = manager._queue_size_locked(include_overflow=True)

        assert active <= 2
        assert total >= 3
        gate.set()
    finally:
        manager.shutdown()

import json
from pathlib import Path
from types import SimpleNamespace

import pytest
from aiohttp.test_utils import TestClient, TestServer

from services.store_companion import Catalog, CompanionState, StoreCompanion


class ConfigStub:
    def __init__(self, root):
        self.cache_dir = str(root / "cache")
        self.music_dir = str(root / "music")
        self.db_path = str(root / "library.db")
        self.video_dirs = []
        self.values = {"streamrip_binary": "/bin/false", "streamrip_preferred_format": "flac", "streamrip_quality": 3}

    def get(self, key, default=None):
        return self.values.get(key, default)


def test_state_stores_hash_not_bearer_token(tmp_path):
    state = CompanionState(tmp_path)
    token = state.issue_token("Test iPod")
    assert state.accepts(token)
    assert token not in state.path.read_text()


def test_catalog_removes_host_paths_and_marks_owned_from_device_only(tmp_path):
    config = ConfigStub(tmp_path)
    cover = tmp_path / "cover.jpg"
    cover.write_bytes(b"jpeg")
    state = CompanionState(tmp_path / "state")
    state.data["device_libraries"]["ios6"] = {"tracks": [
        {"title": "Song", "artist": "Artist", "album": "Album", "album_artist": "Artist"}
    ]}
    catalog = Catalog(config, state)
    result = catalog.decorate([{
        "media_type": "album", "title": "Album", "artist": "Artist", "tracks": 1,
        "cover_path": str(cover),
    }], "ios6")[0]
    assert result["owned"] is True
    assert "cover_path" not in result
    assert result["artwork_url"].startswith("/v1/store/artwork/")
    assert catalog.decorate({"media_type": "track", "title": "Song", "artist": "Artist"}, "ios6")["owned"] is True
    assert catalog.decorate({"media_type": "track", "title": "Song", "artist": "Artist"}, "ios7")["owned"] is False


@pytest.mark.asyncio
async def test_pair_then_authorized_request(tmp_path):
    companion = StoreCompanion(ConfigStub(tmp_path), pair_code="123456")
    async with TestClient(TestServer(companion.app())) as client:
        denied = await client.get("/v1/account")
        assert denied.status == 401
        paired = await client.post("/v1/pair", json={"code": "123456", "name": "iPod touch"})
        token = (await paired.json())["data"]["token"]
        account = await client.get("/v1/account", headers={"Authorization": "Bearer " + token})
        assert account.status == 200
        assert (await account.json())["data"]["source"] == "tidal"


@pytest.mark.asyncio
async def test_featured_livetv_scope_is_accepted(tmp_path):
    companion = StoreCompanion(ConfigStub(tmp_path), pair_code="123456")
    requested = []

    def create(scope):
        requested.append(scope)
        return ({
            "id": "featured-job", "scope": scope, "state": "queued",
            "done": 0, "total": 0, "label": "Waiting", "error": "",
            "created_at": 1, "manifest": None,
        }, True)

    companion.livetv_jobs.create = create
    async with TestClient(TestServer(companion.app())) as client:
        paired = await client.post("/v1/pair", json={"code": "123456"})
        token = (await paired.json())["data"]["token"]
        headers = {"Authorization": "Bearer " + token}
        featured = await client.post(
            "/v1/livetv/syncs", headers=headers, json={"scope": "featured"}
        )
        assert featured.status == 202
        assert (await featured.json())["data"]["scope"] == "featured"
        assert requested == ["featured"]

        rejected = await client.post(
            "/v1/livetv/syncs", headers=headers, json={"scope": "unknown"}
        )
        assert rejected.status == 400


@pytest.mark.asyncio
async def test_import_idempotency(tmp_path):
    companion = StoreCompanion(ConfigStub(tmp_path), pair_code="123456")
    async with TestClient(TestServer(companion.app())) as client:
        paired = await client.post("/v1/pair", json={"code": "123456"})
        token = (await paired.json())["data"]["token"]
        headers = {
            "Authorization": "Bearer " + token,
            "Idempotency-Key": "buy-1",
            "X-RockPod-Library-ID": "ios6",
        }
        body = {"item": {"title": "A", "artist": "B", "url": "https://tidal.com/album/1"}, "format": "flac"}
        first = await client.post("/v1/imports", headers=headers, json=body)
        second = await client.post("/v1/imports", headers=headers, json=body)
        assert (await first.json())["data"]["id"] == (await second.json())["data"]["id"]


@pytest.mark.asyncio
async def test_device_library_isolated_by_ios_installation(tmp_path):
    companion = StoreCompanion(ConfigStub(tmp_path), pair_code="123456")
    async with TestClient(TestServer(companion.app())) as client:
        paired = await client.post("/v1/pair", json={"code": "123456"})
        token = (await paired.json())["data"]["token"]
        base = {"Authorization": "Bearer " + token}
        ios6 = dict(base, **{"X-RockPod-Library-ID": "ios6"})
        ios7 = dict(base, **{"X-RockPod-Library-ID": "ios7"})
        saved = await client.post("/v1/device/library", headers=ios6, json={"tracks": [{
            "persistent_id": "1", "title": "A", "artist": "B", "album": "C", "album_artist": "B"
        }]})
        assert saved.status == 200
        assert len(companion.state.data["device_libraries"]["ios6"]["tracks"]) == 1
        assert "ios7" not in companion.state.data["device_libraries"]

        body = {"item": {"title": "A", "artist": "B", "url": "https://tidal.com/album/1"}, "format": "mp3"}
        first = await client.post("/v1/imports", headers=dict(ios6, **{"Idempotency-Key": "same"}), json=body)
        second = await client.post("/v1/imports", headers=dict(ios7, **{"Idempotency-Key": "same"}), json=body)
        assert (await first.json())["data"]["id"] != (await second.json())["data"]["id"]


@pytest.mark.asyncio
async def test_download_history_delete_and_clear_only_remove_terminal_jobs(tmp_path):
    companion = StoreCompanion(ConfigStub(tmp_path), pair_code="123456")
    companion.state.data["jobs"] = [
        {"id": "done", "library_id": "ios6", "state": "completed", "files": []},
        {"id": "failed", "library_id": "ios6", "state": "failed", "files": []},
        {"id": "active", "library_id": "ios6", "state": "transferring", "files": []},
        {"id": "other", "library_id": "ios7", "state": "completed", "files": []},
    ]
    async with TestClient(TestServer(companion.app())) as client:
        paired = await client.post("/v1/pair", json={"code": "123456"})
        token = (await paired.json())["data"]["token"]
        headers = {"Authorization": "Bearer " + token, "X-RockPod-Library-ID": "ios6"}

        active = await client.delete("/v1/imports/active", headers=headers)
        assert active.status == 409
        removed = await client.delete("/v1/imports/done", headers=headers)
        assert removed.status == 200
        cleared = await client.post("/v1/imports/clear", headers=headers, json={})
        assert (await cleared.json())["data"]["removed"] == 1
        assert {job["id"] for job in companion.state.data["jobs"]} == {"active", "other"}


@pytest.mark.asyncio
async def test_device_transfer_progress_is_sanitized_and_exposed(tmp_path):
    companion = StoreCompanion(ConfigStub(tmp_path), pair_code="123456")
    companion.state.data["jobs"] = [{
        "id": "active", "library_id": "ios6", "state": "ready", "files": [],
        "progress": 1.0, "progress_done": 2, "progress_total": 2,
    }]
    async with TestClient(TestServer(companion.app())) as client:
        paired = await client.post("/v1/pair", json={"code": "123456"})
        token = (await paired.json())["data"]["token"]
        headers = {"Authorization": "Bearer " + token, "X-RockPod-Library-ID": "ios6"}
        response = await client.post("/v1/imports/active/state", headers=headers, json={
            "state": "transferring", "progress": 1.5, "progress_done": 1,
            "progress_total": 2, "progress_label": "Transferring 2 of 2",
        })
        job = (await response.json())["data"]
        assert job["progress"] == 1.0
        assert job["progress_done"] == 1
        assert job["progress_total"] == 2
        assert job["progress_label"] == "Transferring 2 of 2"


@pytest.mark.asyncio
async def test_device_inventory_reconciles_completed_album_and_exposes_track_order(tmp_path):
    companion = StoreCompanion(ConfigStub(tmp_path), pair_code="123456")
    song1 = tmp_path / "01. First.mp3"
    song2 = tmp_path / "02. Second.mp3"
    song1.write_bytes(b"one")
    song2.write_bytes(b"two")
    companion.state.data["jobs"] = [{
        "id": "album-job", "library_id": "ios6", "state": "device_failed",
        "title": "Album", "artist": "Artist", "media_type": "album",
        "files": [str(song1), str(song2)], "error": "old importer error",
        "file_metadata": [
            {"title": "First", "track_number": 1, "track_count": 2, "album_artist": "Artist", "disc_number": 1},
            {"title": "Second", "track_number": 2, "track_count": 2, "album_artist": "Artist", "disc_number": 1},
        ],
    }]
    async with TestClient(TestServer(companion.app())) as client:
        paired = await client.post("/v1/pair", json={"code": "123456"})
        token = (await paired.json())["data"]["token"]
        headers = {"Authorization": "Bearer " + token, "X-RockPod-Library-ID": "ios6"}
        inventory = {"tracks": [
            {"persistent_id": "1", "title": "First", "artist": "Artist", "album": "Album", "album_artist": "Artist", "track_number": 1, "track_count": 2, "disc_number": 1, "disc_count": 1},
            {"persistent_id": "2", "title": "Second", "artist": "Artist", "album": "Album", "album_artist": "Artist", "track_number": 2, "track_count": 2, "disc_number": 1, "disc_count": 1},
        ]}
        saved = await client.post("/v1/device/library", headers=headers, json=inventory)
        assert saved.status == 200
        assert companion.state.data["jobs"][0]["state"] == "completed"
        jobs = (await (await client.get("/v1/imports", headers=headers)).json())["data"]
        assert [item["track_number"] for item in jobs[0]["files"]] == [1, 2]
        assert [item["track_count"] for item in jobs[0]["files"]] == [2, 2]
        assert all("album_artist" in item and "disc_number" in item for item in jobs[0]["files"])


@pytest.mark.asyncio
async def test_duplicate_title_does_not_complete_album_with_missing_track(tmp_path):
    companion = StoreCompanion(ConfigStub(tmp_path), pair_code="123456")
    song1 = tmp_path / "01. First.mp3"
    song2 = tmp_path / "02. Second.mp3"
    song1.write_bytes(b"one")
    song2.write_bytes(b"two")
    companion.state.data["jobs"] = [{
        "id": "album-job", "library_id": "ios6", "state": "transferring",
        "title": "Album", "artist": "Artist", "media_type": "album",
        "files": [str(song1), str(song2)],
        "file_metadata": [{"title": "First"}, {"title": "Second"}],
    }]
    async with TestClient(TestServer(companion.app())) as client:
        paired = await client.post("/v1/pair", json={"code": "123456"})
        token = (await paired.json())["data"]["token"]
        headers = {"Authorization": "Bearer " + token, "X-RockPod-Library-ID": "ios6"}
        inventory = {"tracks": [
            {"persistent_id": "1", "title": "First", "artist": "Artist", "album": "Album", "album_artist": "Artist"},
            {"persistent_id": "2", "title": "First", "artist": "Artist", "album": "Album", "album_artist": "Artist"},
        ]}
        saved = await client.post("/v1/device/library", headers=headers, json=inventory)
        assert saved.status == 200
        assert companion.state.data["jobs"][0]["state"] == "transferring"


@pytest.mark.asyncio
async def test_album_with_all_titles_but_wrong_track_order_stays_incomplete(tmp_path):
    companion = StoreCompanion(ConfigStub(tmp_path), pair_code="123456")
    songs = [tmp_path / "01.mp3", tmp_path / "02.mp3"]
    for song in songs:
        song.write_bytes(b"audio")
    companion.state.data["jobs"] = [{
        "id": "album-job", "library_id": "ios6", "state": "device_failed",
        "title": "Album", "artist": "Artist", "media_type": "album",
        "files": [str(song) for song in songs],
        "file_metadata": [
            {"title": "First", "track_number": 1, "track_count": 2},
            {"title": "Second", "track_number": 2, "track_count": 2},
        ],
    }]
    async with TestClient(TestServer(companion.app())) as client:
        paired = await client.post("/v1/pair", json={"code": "123456"})
        token = (await paired.json())["data"]["token"]
        headers = {"Authorization": "Bearer " + token, "X-RockPod-Library-ID": "ios6"}
        inventory = {"tracks": [
            {"persistent_id": "1", "title": "First", "artist": "Artist", "album": "Album", "album_artist": "Artist", "track_number": 0, "track_count": 0, "disc_number": 1, "disc_count": 0},
            {"persistent_id": "2", "title": "Second", "artist": "Artist", "album": "Album", "album_artist": "Artist", "track_number": 0, "track_count": 0, "disc_number": 1, "disc_count": 0},
        ]}
        saved = await client.post("/v1/device/library", headers=headers, json=inventory)
        assert saved.status == 200
        assert companion.state.data["jobs"][0]["state"] == "device_failed"

from pathlib import Path

from PIL import Image

from services.rockbox_maps import (
    MAPS_RELATIVE_PATH, WORLD_ATLAS_RELATIVE_DIR, RockboxMapsService,
    RockboxWorldAtlasService,
)


def test_native_map_uses_full_world_atlas_and_safe_geographic_math():
    source = (Path(__file__).parents[2] / "apps/plugins/nb_maps.c").read_text(
        encoding="utf-8"
    )

    assert "#define MAR_ZOOM_COUNT 9" in source
    assert "#define MAR_GLOBE_FRAMES 64" in source
    assert "MAR_WORLD_GLOBE_PATH" in source
    assert "MAR_WORLD_TILE_HI_PATH" in source
    assert "MAR_WORLD_TILE_HI2_PATH" in source
    assert "Explore changes the zoom and centre" in source
    assert "(void)mar_load_satellite(&view);" in source
    assert "(long long)(longitude_e6 + 180000000)" in source
    assert "mar_mercator_y_by_lat" in source
    assert "while (view->center_x < 0)" in source
    assert "{ 46087800, -64778200 }" in source
    assert "MAR_GLOBE_TURN_FRAMES 3" in source
    assert "mpegplayer.rock" in source


def test_writes_location_bundle_with_route(tmp_path):
    mount = tmp_path / "ipod"
    mount.mkdir()
    photos = tmp_path / "photos"
    photos.mkdir()
    service = RockboxMapsService()

    result = service.write_bundle(
        {"device_mount_path": str(mount), "photos_library_path": str(photos)},
        "Moncton, NB", 46.0878, -64.7782, "Morning ride",
        [(46.0878, -64.7782), (46.10, -64.75)],
    )

    contents = (mount / MAPS_RELATIVE_PATH).read_text(encoding="utf-8")
    assert "location\t46087800\t-64778200\tMoncton, NB" in contents
    assert "route\tMorning ride" in contents
    assert contents.count("point\t") == 2
    assert result["route_points"] == 2


def test_photo_map_bundle_excludes_locked_photo_and_writes_thumb(tmp_path, monkeypatch):
    mount = tmp_path / "ipod"
    photos = tmp_path / "photos"
    mount.mkdir()
    photos.mkdir()
    Image.new("RGB", (120, 80), "red").save(photos / "shown.jpg")
    Image.new("RGB", (120, 80), "blue").save(photos / "locked.jpg")
    locks = mount / ".rockbox" / "rocks" / "apps" / "data"
    locks.mkdir(parents=True)
    (locks / "photos.locks").write_text("locked.jpg|1234\n", encoding="utf-8")
    service = RockboxMapsService()
    monkeypatch.setattr(
        "services.rockbox_maps._gps_from_photo", lambda _path: (46.0878, -64.7782)
    )

    result = service.write_bundle(
        {"device_mount_path": str(mount), "photos_library_path": str(photos),
         "source_repo_path": str(tmp_path), "id": "test"},
        "Moncton", 46.0878, -64.7782,
    )

    manifest = (mount / MAPS_RELATIVE_PATH).read_text(encoding="utf-8")
    thumb = mount / ".rockbox" / "maps" / "photos" / "photo_00.r16"
    assert result["photos"] == 1
    assert "shown.jpg" in manifest
    assert "locked.jpg" not in manifest
    assert thumb.stat().st_size == 40 * 30 * 2


def test_parses_and_bounds_gpx_route(tmp_path):
    points = "".join(f'<trkpt lat="46.{index:04d}" lon="-64.7"/>' for index in range(30))
    path = Path(tmp_path) / "route.gpx"
    path.write_text(f'<gpx><trk><trkseg>{points}</trkseg></trk></gpx>', encoding="utf-8")

    route = RockboxMapsService().parse_gpx(path)

    assert len(route) <= 24
    assert route[0] == (46.0, -64.7)


def test_world_atlas_converts_standard_tile_tree(tmp_path):
    source = tmp_path / "source" / "2" / "1"
    source.mkdir(parents=True)
    Image.new("RGB", (16, 16), "red").save(source / "3.png")
    mount = tmp_path / "ipod"
    mount.mkdir()

    result = RockboxWorldAtlasService().sync_atlas(
        {"device_mount_path": str(mount)}, tmp_path / "source", maximum_zoom=2
    )

    tile = mount / WORLD_ATLAS_RELATIVE_DIR / "2" / "1_3.r16"
    assert result["tiles"] == 1
    assert tile.stat().st_size == 320 * 160 * 2

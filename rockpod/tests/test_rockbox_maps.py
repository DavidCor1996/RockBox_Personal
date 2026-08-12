import math
import re
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

    assert "#define MAR_ZOOM_COUNT 19" in source
    assert "#define MAR_START_LOCATION_ZOOM 14" in source
    assert "#define MAR_REFERENCE_SCALE 8192" in source
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
    assert "view->center_y += step" in source
    assert "mar_sync.location_x" in source
    assert '"poi\\t"' in source
    assert "nb_maps_markers.h" in source
    assert "if (zoom >= 9)" in source
    assert "MAR_SCENE_MONCTON_DASHCAM" in source
    assert "scene == MAR_SCENE_MONCTON_DASHCAM ? 8" in source
    assert "moncton_dashcam_7.rgb" in source
    assert '"moncton_full_drive.mpg"' in source
    assert "mar_return_to_synced_location(&view);" in source
    assert "(BUTTON_PLAY | BUTTON_SELECT)" in source
    assert "#define MAR_POI_ICON_SIZE 9" in source
    assert 'const char *label = "My Location"' in source
    assert '#include "nb_maps_mercator.h"' in source
    assert "MAR_MERCATOR_STEP_E6" in source
    assert "int drawmode = rb->lcd_get_drawmode();" in source
    assert "rb->lcd_set_drawmode(DRMODE_FG);" in source
    assert '"My Location"' in source
    assert "mar_sync.location_name" in source
    assert "view->zoom >= 12 ? 13 : view->zoom >= 9 ? 8 : 5" in source
    assert "rb->wheel_status()" in source
    assert "clean == BUTTON_SCROLL_FWD" in source
    assert "view.zoom++" in source
    assert "view.zoom--" in source
    assert '"310 m", "155 m"' in source


def test_mercator_table_keeps_synced_location_within_two_metres():
    header = (
        Path(__file__).parents[2] / "apps/plugins/nb_maps_mercator.h"
    ).read_text(encoding="utf-8")
    values = [int(value) for value in re.findall(r"(\d+)u", header)]
    latitude = 46.0878
    position = (latitude + 85.0) * 10.0
    index = int(position)
    fraction = position - index
    interpolated = (
        values[index] + (values[index + 1] - values[index]) * fraction
    ) / (1 << 30)
    radians = math.radians(latitude)
    exact = (1.0 - math.asinh(math.tan(radians)) / math.pi) / 2.0

    assert len(values) == 1701
    assert abs(interpolated - exact) * 40_075_017 < 2.0


def test_dominion_location_stays_aligned_at_every_flat_zoom():
    """The marker and raster viewport must share one centre at z1 through z18."""
    world_width = 10_000 * 8192
    world_height = 6_600 * 8192
    latitude = 46.093520297636
    longitude = -64.7934201321174
    location_x = int((longitude + 180.0) * world_width / 360.0)
    latitude_radians = math.radians(latitude)
    location_y = int(
        (1.0 - math.asinh(math.tan(latitude_radians)) / math.pi)
        * world_height / 2.0 + 0.5
    )

    for zoom in range(1, 19):
        tiles = 1 << zoom
        span_x = world_width // tiles
        span_y = world_height // tiles
        origin_x = location_x - span_x // 2
        origin_y = location_y - span_y // 2
        tile_x = origin_x * tiles // world_width
        tile_y = origin_y * tiles // world_height
        tile_x0 = tile_x * world_width // tiles
        tile_x1 = (tile_x + 1) * world_width // tiles
        tile_y0 = tile_y * world_height // tiles
        tile_y1 = (tile_y + 1) * world_height // tiles
        cropped_x = (origin_x - tile_x0) * 320 // (tile_x1 - tile_x0)
        cropped_y = (origin_y - tile_y0) * 160 // (tile_y1 - tile_y0)
        raster_x = ((location_x - tile_x0) * 320 /
                    (tile_x1 - tile_x0) - cropped_x)
        raster_y = (22 + (location_y - tile_y0) * 160 /
                    (tile_y1 - tile_y0) - cropped_y)

        assert abs(raster_x - 160) <= 1.0, zoom
        assert abs(raster_y - 102) <= 1.0, zoom


def test_dominion_location_uses_globe_projection_at_zoom_zero():
    source = (Path(__file__).parents[2] / "apps/plugins/nb_maps.c").read_text(
        encoding="utf-8"
    )

    assert "mar_draw_globe_location();" in source
    assert "Correctly hidden on the far side of the Earth" in source
    assert "MAR_GLOBE_RADIUS * cos_latitude" in source
    assert "MAR_GLOBE_RADIUS * sin_latitude" in source


def test_all_marker_paths_share_wrapped_and_clamped_viewport_math():
    source = (Path(__file__).parents[2] / "apps/plugins/nb_maps.c").read_text(
        encoding="utf-8"
    )

    assert "delta > MAR_WORLD_W / 2" in source
    assert "delta < -MAR_WORLD_W / 2" in source
    assert "view->center_y < half_span" in source
    assert "view->center_y > MAR_WORLD_H - half_span" in source
    assert source.count("mar_clamp(&view);") >= 6
    assert "mar_load_nb_atlas_tile" in source
    assert "nb_z%02d_x%05d.r16p" in source


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


def test_writes_bounded_nearby_hotspots(tmp_path):
    mount = tmp_path / "ipod"
    mount.mkdir()
    service = RockboxMapsService()
    hotspots = [
        {"name": "Cafe Codiac", "latitude": 46.09,
         "longitude": -64.78, "kind": "f"},
        {"name": "Riverfront Park", "latitude": 46.087,
         "longitude": -64.77, "kind": "a"},
    ]

    result = service.write_bundle(
        {"device_mount_path": str(mount)}, "Moncton, NB",
        46.0878, -64.7782, hotspots=hotspots,
    )

    contents = (mount / MAPS_RELATIVE_PATH).read_text(encoding="utf-8")
    assert "poi\t46090000\t-64780000\tf\tCafe Codiac" in contents
    assert "poi\t46087000\t-64770000\ta\tRiverfront Park" in contents
    assert result["hotspots"] == 2


def test_automatic_location_uses_saved_weather_coordinates():
    service = RockboxMapsService()

    location = service.resolve_location({
        "weather_location_name": "Moncton, NB",
        "weather_latitude": 46.0878,
        "weather_longitude": -64.7782,
    })

    assert location == ("Moncton, NB", 46.0878, -64.7782)


def test_hotspots_are_named_deduplicated_and_distance_sorted(monkeypatch):
    service = RockboxMapsService()
    monkeypatch.setattr(service, "_request_json", lambda *_args, **_kwargs: {
        "elements": [
            {"lat": 46.10, "lon": -64.80,
             "tags": {"name": "Far Pub", "amenity": "pub"}},
            {"lat": 46.088, "lon": -64.778,
             "tags": {"name": "Near Cafe", "amenity": "cafe"}},
            {"lat": 46.088, "lon": -64.778,
             "tags": {"name": "Near Cafe", "amenity": "cafe"}},
            {"lat": 46.089, "lon": -64.779,
             "tags": {"amenity": "restaurant"}},
        ]
    })

    hotspots = service.nearby_hotspots(46.0878, -64.7782)

    assert [item["name"] for item in hotspots] == ["Near Cafe", "Far Pub"]
    assert [item["kind"] for item in hotspots] == ["f", "d"]


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

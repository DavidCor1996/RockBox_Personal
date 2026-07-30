"""Headless checks for the Maker Lite creator and v3 directional art."""

import json
from pathlib import Path

import pytest
from PySide6.QtCore import QEvent, QMimeData, QPointF, Qt
from PySide6.QtGui import (
    QDragEnterEvent,
    QDragMoveEvent,
    QDropEvent,
    QKeyEvent,
    QMouseEvent,
)
from PySide6.QtWidgets import QApplication, QGraphicsView

from services.maker_lite_projects import MakerLiteProjectStore
from tools.maker_lite_stage_sim_fixtures import diagnostic_art
from ui.maker_lite_canvas import ASSET_MIME_TYPE
from ui.maker_lite_creator import MakerLiteCreator
from ui.maker_lite_preview import _load_art


ROOT = Path(__file__).resolve().parents[2]


def _application():
    return QApplication.instance() or QApplication([])


def _palette_item(creator, label):
    for index in range(creator.palette.count()):
        item = creator.palette.item(index)
        if item.text() == label:
            return item
    raise AssertionError(f"palette item not found: {label}")


def _write_diagnostic_kit(private_root, kit_id, ruleset, catalog=None):
    kit_dir = private_root / "kits" / kit_id
    kit_dir.mkdir(parents=True)
    art_path = kit_dir / "art.mla"
    art_path.write_bytes(diagnostic_art(kit_id, ruleset))
    if catalog is None:
        catalog = [
            {
                "id": "ground",
                "label": "Ground",
                "category": "Terrain",
                "type": "terrain",
                "cell": 20,
                "collision": ["solid"],
            },
            {
                "id": "enemy",
                "label": "Enemy",
                "category": "Enemies",
                "type": "entity",
                "cell": 34,
                "kind": "enemy",
                "params": [16, 16, 0, 0],
                "flags": 0,
            },
            {
                "id": "checkpoint",
                "label": "Checkpoint",
                "category": "Objects",
                "type": "entity",
                "cell": 35,
                "kind": "checkpoint",
                "params": [16, 32, 0, 0],
                "flags": 0,
            },
            {
                "id": "carryable-shell",
                "label": "Carryable Shell",
                "category": "Items",
                "type": "entity",
                "cell": 42,
                "kind": "pot",
                "params": [16, 16, 0, 0],
                "flags": 0,
            },
        ]
    (kit_dir / "kit.mlk").write_text(
        json.dumps({"ruleset": ruleset, "asset_catalog": catalog}),
        encoding="utf-8",
    )
    return art_path


def _mouse_event(
    canvas,
    event_type,
    view_point,
    button,
    buttons,
    modifiers=Qt.NoModifier,
):
    return QMouseEvent(
        event_type,
        QPointF(view_point),
        QPointF(canvas.viewport().mapToGlobal(view_point)),
        button,
        buttons,
        modifiers,
    )


def test_v3_directional_art_loads_and_precomputes_creator_palette(tmp_path):
    _application()
    private_root = tmp_path / "maker-lite"
    kit_id = "diagnostic-mario"
    art_path = _write_diagnostic_kit(
        private_root,
        kit_id,
        "mario",
    )

    cells, player_base, entity_cells, animations, frames = _load_art(art_path)
    assert len(cells) == 96
    assert player_base == 0
    assert entity_cells[4] == 34
    assert len(animations) == 14
    assert all(len(directions) == 4 for directions in animations)
    assert animations[1][0] == (1, 1, 2)
    assert animations[1][2] == (1, 1, 0x82)
    assert frames == []

    store = MakerLiteProjectStore(private_root)
    record = store.create("mario", kit_id)
    creator = MakerLiteCreator(
        str(private_root),
        str(tmp_path),
    )
    try:
        assert creator.record.project_id == record.project_id
        assert creator.canvas._kit_error == ""
        labels = [
            creator.palette.item(index).text()
            for index in range(creator.palette.count())
        ]
        assert "Select / Move" in labels
        assert "Carryable Shell" in labels
        assert not _palette_item(creator, "Ground").icon().isNull()
        assert bool(
            _palette_item(creator, "Ground").flags() & Qt.ItemIsDragEnabled
        )
        assert not bool(
            _palette_item(creator, "Select / Move").flags()
            & Qt.ItemIsDragEnabled
        )
        creator.canvas.set_tool({"type": "terrain", "tile": 1})
        creator.canvas.keyPressEvent(
            QKeyEvent(QEvent.KeyPress, Qt.Key_Space, Qt.NoModifier)
        )
        assert creator.canvas.dragMode() == QGraphicsView.ScrollHandDrag
        creator.canvas.keyReleaseEvent(
            QKeyEvent(QEvent.KeyRelease, Qt.Key_Space, Qt.NoModifier)
        )
        assert creator.canvas.dragMode() == QGraphicsView.NoDrag
        test_requests = []
        creator.canvas.test_requested.disconnect()
        creator.canvas.test_requested.connect(lambda: test_requests.append(True))
        creator.canvas.keyPressEvent(
            QKeyEvent(QEvent.KeyPress, Qt.Key_P, Qt.NoModifier)
        )
        assert test_requests == [True]

        original_count = len(creator.record.source["entities"])
        creator._apply_canvas_operation(
            {
                "kind": "entity",
                "value": {
                    "kind": "enemy",
                    "x": 80,
                    "y": 96,
                    "params": [16, 16, 0, 0],
                },
            }
        )
        assert len(creator.record.source["entities"]) == original_count + 1
        creator._apply_canvas_operation(
            {
                "kind": "move_entity",
                "index": original_count,
                "x": 112,
                "y": 128,
            }
        )
        assert creator.record.source["entities"][-1]["x"] == 112
        creator._apply_canvas_operation(
            {"kind": "delete_entity", "index": original_count}
        )
        assert len(creator.record.source["entities"]) == original_count
        creator.undo_stack.undo()
        assert creator.record.source["entities"][-1]["x"] == 112

        creator.palette_search.setText("shell")
        visible = [
            creator.palette.item(index).text()
            for index in range(creator.palette.count())
            if not creator.palette.item(index).isHidden()
        ]
        assert visible == ["Carryable Shell"]
    finally:
        creator.close()


def test_large_authentic_catalog_uses_paged_parts_browser(tmp_path):
    app = _application()
    private_root = tmp_path / "private"
    kit_id = "mario-paged-kit"
    catalog = [
        {
            "id": f"part-{index:03d}",
            "label": f"Part {index:03d}",
            "category": "Source Library",
            "type": "terrain",
            "cell": 20,
            "collision": [],
        }
        for index in range(90)
    ]
    _write_diagnostic_kit(private_root, kit_id, "mario", catalog)
    MakerLiteProjectStore(private_root).create("mario", kit_id)
    creator = MakerLiteCreator(str(private_root), str(tmp_path))
    creator.show()
    app.processEvents()
    try:
        visible = [
            creator.palette.item(index)
            for index in range(creator.palette.count())
            if not creator.palette.item(index).isHidden()
        ]
        assert len(visible) == creator.PALETTE_PAGE_SIZE
        assert "Page 1 of 2" in creator.palette_page_label.text()
        creator.palette_next.click()
        app.processEvents()
        assert "Page 2 of 2" in creator.palette_page_label.text()
        creator.palette_search.setText("Part 089")
        app.processEvents()
        assert "Page 1 of 1" in creator.palette_page_label.text()
        assert [
            creator.palette.item(index).text()
            for index in range(creator.palette.count())
            if not creator.palette.item(index).isHidden()
        ] == ["Part 089"]
    finally:
        creator.close()


def test_palette_asset_drag_drop_places_snapped_undoable_object(tmp_path):
    app = _application()
    private_root = tmp_path / "maker-lite"
    kit_id = "diagnostic-mario"
    _write_diagnostic_kit(private_root, kit_id, "mario")
    store = MakerLiteProjectStore(private_root)
    store.create("mario", kit_id)
    creator = MakerLiteCreator(str(private_root), str(tmp_path))
    creator.resize(1180, 720)
    creator.show()
    app.processEvents()
    try:
        enemy = _palette_item(creator, "Enemy")
        mime_data = creator.palette.mimeData([enemy])
        assert mime_data.hasFormat(ASSET_MIME_TYPE)
        assert json.loads(
            bytes(mime_data.data(ASSET_MIME_TYPE)).decode("utf-8")
        ) == {
            "type": "entity",
            "kind": "enemy",
            "cell": 34,
            "asset_id": "enemy",
            "params": [16, 16, 0, 0],
            "flags": 0,
        }

        scene_point = QPointF(168, 72)
        view_point = creator.canvas.mapFromScene(scene_point)
        enter = QDragEnterEvent(
            view_point,
            Qt.CopyAction,
            mime_data,
            Qt.LeftButton,
            Qt.NoModifier,
        )
        creator.canvas.dragEnterEvent(enter)
        assert enter.isAccepted()
        assert creator.canvas._drop_preview_item is not None
        assert creator.canvas._drop_preview_item.opacity() == 0.72

        move = QDragMoveEvent(
            view_point,
            Qt.CopyAction,
            mime_data,
            Qt.LeftButton,
            Qt.NoModifier,
        )
        creator.canvas.dragMoveEvent(move)
        assert move.isAccepted()

        original_count = len(creator.record.source["entities"])
        drop = QDropEvent(
            QPointF(view_point),
            Qt.CopyAction,
            mime_data,
            Qt.LeftButton,
            Qt.NoModifier,
        )
        creator.canvas.dropEvent(drop)
        assert drop.isAccepted()
        assert len(creator.record.source["entities"]) == original_count + 1
        placed = creator.record.source["entities"][-1]
        assert placed["kind"] == "enemy"
        assert placed["render_cell"] == 34
        assert placed["x"] == 168
        assert placed["y"] == 80
        assert creator.canvas._drop_preview_item is None
        assert creator.palette.mimeData([enemy]).hasFormat(ASSET_MIME_TYPE)

        grab_point = creator.canvas.mapFromScene(QPointF(168, 72))
        move_point = creator.canvas.mapFromScene(QPointF(200, 104))
        creator.canvas.mousePressEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseButtonPress,
                grab_point,
                Qt.LeftButton,
                Qt.LeftButton,
            )
        )
        assert creator.canvas._selected_entity == original_count
        creator.canvas.mouseMoveEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseMove,
                move_point,
                Qt.NoButton,
                Qt.LeftButton,
            )
        )
        assert creator.canvas._drop_preview_item is not None
        creator.canvas.mouseReleaseEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseButtonRelease,
                move_point,
                Qt.LeftButton,
                Qt.NoButton,
            )
        )
        assert creator.record.source["entities"][-1]["x"] == 200
        assert creator.record.source["entities"][-1]["y"] == 112

        copy_grab = creator.canvas.mapFromScene(QPointF(200, 104))
        copy_target = creator.canvas.mapFromScene(QPointF(232, 104))
        creator.canvas.mousePressEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseButtonPress,
                copy_grab,
                Qt.LeftButton,
                Qt.LeftButton,
                Qt.ControlModifier,
            )
        )
        creator.canvas.mouseMoveEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseMove,
                copy_target,
                Qt.NoButton,
                Qt.LeftButton,
                Qt.ControlModifier,
            )
        )
        creator.canvas.mouseReleaseEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseButtonRelease,
                copy_target,
                Qt.LeftButton,
                Qt.NoButton,
                Qt.ControlModifier,
            )
        )
        assert len(creator.record.source["entities"]) == original_count + 2
        assert creator.record.source["entities"][-1]["x"] == 232
        assert creator.record.source["entities"][-1]["y"] == 112

        creator.undo_stack.undo()
        assert len(creator.record.source["entities"]) == original_count + 1
        creator.undo_stack.undo()
        assert creator.record.source["entities"][-1]["x"] == 168
        assert creator.record.source["entities"][-1]["y"] == 80
        creator.undo_stack.undo()
        assert len(creator.record.source["entities"]) == original_count

        ground = _palette_item(creator, "Ground")
        terrain_mime = creator.palette.mimeData([ground])
        terrain_count = len(creator.record.source["terrain"])
        terrain_point = creator.canvas.mapFromScene(QPointF(64, 64))
        terrain_drop = QDropEvent(
            QPointF(terrain_point),
            Qt.CopyAction,
            terrain_mime,
            Qt.LeftButton,
            Qt.NoModifier,
        )
        creator.canvas.dropEvent(terrain_drop)
        assert terrain_drop.isAccepted()
        assert len(creator.record.source["terrain"]) == terrain_count + 1
        assert creator.record.source["terrain"][-1] == {
            "point": [4, 4],
            "tile": 20,
            "collision": ["solid"],
        }
        creator.undo_stack.undo()
        assert len(creator.record.source["terrain"]) == terrain_count
    finally:
        creator.close()


def test_canvas_rejects_unknown_and_out_of_bounds_asset_drops(tmp_path):
    app = _application()
    private_root = tmp_path / "maker-lite"
    kit_id = "diagnostic-mario"
    _write_diagnostic_kit(private_root, kit_id, "mario")
    store = MakerLiteProjectStore(private_root)
    store.create("mario", kit_id)
    creator = MakerLiteCreator(str(private_root), str(tmp_path))
    creator.resize(1180, 720)
    creator.show()
    app.processEvents()
    try:
        original_count = len(creator.record.source["entities"])
        forged = QMimeData()
        forged.setData(
            ASSET_MIME_TYPE,
            json.dumps({"type": "entity", "kind": "not-real"}).encode(),
        )
        forged_drop = QDropEvent(
            QPointF(100, 100),
            Qt.CopyAction,
            forged,
            Qt.LeftButton,
            Qt.NoModifier,
        )
        creator.canvas.dropEvent(forged_drop)
        assert not forged_drop.isAccepted()

        enemy = _palette_item(creator, "Enemy")
        mime_data = creator.palette.mimeData([enemy])
        outside = creator.canvas.mapFromScene(QPointF(-32, -32))
        outside_move = QDragMoveEvent(
            outside,
            Qt.CopyAction,
            mime_data,
            Qt.LeftButton,
            Qt.NoModifier,
        )
        creator.canvas.dragMoveEvent(outside_move)
        assert not outside_move.isAccepted()
        assert creator.canvas._drop_outline_item.pen().color().name() == "#ff5f57"
        outside_drop = QDropEvent(
            QPointF(outside),
            Qt.CopyAction,
            mime_data,
            Qt.LeftButton,
            Qt.NoModifier,
        )
        creator.canvas.dropEvent(outside_drop)
        assert not outside_drop.isAccepted()
        assert len(creator.record.source["entities"]) == original_count
    finally:
        creator.close()


def test_mouse_paint_stroke_is_live_and_commits_with_one_rebuild(tmp_path):
    app = _application()
    private_root = tmp_path / "maker-lite"
    kit_id = "diagnostic-mario"
    _write_diagnostic_kit(private_root, kit_id, "mario")
    store = MakerLiteProjectStore(private_root)
    store.create("mario", kit_id)
    creator = MakerLiteCreator(str(private_root), str(tmp_path))
    creator.resize(1180, 720)
    creator.show()
    app.processEvents()
    try:
        ground = _palette_item(creator, "Ground")
        creator.palette.setCurrentItem(ground)
        original_count = len(creator.record.source["terrain"])
        rebuilds = []
        original_set_project = creator.canvas.set_project

        def counted_set_project(source):
            rebuilds.append(True)
            original_set_project(source)

        creator.canvas.set_project = counted_set_project
        points = [
            creator.canvas.mapFromScene(QPointF(x, 96))
            for x in (96, 112, 128, 144, 160)
        ]
        creator.canvas.mousePressEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseButtonPress,
                points[0],
                Qt.LeftButton,
                Qt.LeftButton,
            )
        )
        creator.canvas.mouseMoveEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseMove,
                points[-1],
                Qt.NoButton,
                Qt.LeftButton,
            )
        )
        assert len(creator.canvas._stroke_items) == len(points)
        assert len(creator.record.source["terrain"]) == original_count
        assert rebuilds == []

        creator.canvas.mouseReleaseEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseButtonRelease,
                points[-1],
                Qt.LeftButton,
                Qt.NoButton,
            )
        )
        assert len(creator.record.source["terrain"]) == (
            original_count + len(points)
        )
        assert rebuilds == [True]
        assert creator.undo_stack.undoText() == f"Paint {len(points)} cells"
        creator.canvas.keyPressEvent(
            QKeyEvent(QEvent.KeyPress, Qt.Key_E, Qt.NoModifier)
        )
        assert creator.canvas._tool == {"type": "erase"}
        creator.canvas.keyPressEvent(
            QKeyEvent(QEvent.KeyPress, Qt.Key_E, Qt.NoModifier)
        )
        assert creator.canvas._tool["type"] == "terrain"
        creator.undo_stack.undo()
        assert len(creator.record.source["terrain"]) == original_count
    finally:
        creator.close()


def test_shape_brush_eyedropper_and_object_keyboard_workflow(tmp_path):
    app = _application()
    private_root = tmp_path / "maker-lite"
    kit_id = "diagnostic-mario"
    _write_diagnostic_kit(private_root, kit_id, "mario")
    MakerLiteProjectStore(private_root).create("mario", kit_id)
    creator = MakerLiteCreator(str(private_root), str(tmp_path))
    creator.resize(1180, 720)
    creator.show()
    app.processEvents()
    try:
        ground = _palette_item(creator, "Ground")
        creator.palette.setCurrentItem(ground)
        creator.brush_shape.setCurrentIndex(
            creator.brush_shape.findData("rectangle")
        )
        assert creator.canvas._brush_shape == "rectangle"

        first = creator.canvas.mapFromScene(QPointF(48, 48))
        last = creator.canvas.mapFromScene(QPointF(80, 80))
        terrain_count = len(creator.record.source["terrain"])
        creator.canvas.mousePressEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseButtonPress,
                first,
                Qt.LeftButton,
                Qt.LeftButton,
            )
        )
        creator.canvas.mouseMoveEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseMove,
                last,
                Qt.NoButton,
                Qt.LeftButton,
            )
        )
        creator.canvas.mouseReleaseEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseButtonRelease,
                last,
                Qt.LeftButton,
                Qt.NoButton,
            )
        )
        assert len(creator.record.source["terrain"]) == terrain_count + 9
        assert creator.undo_stack.undoText() == "Paint 9 cells"

        creator.palette.setCurrentItem(_palette_item(creator, "Enemy"))
        sample = creator.canvas.mapFromScene(QPointF(48, 48))
        creator.canvas.mousePressEvent(
            _mouse_event(
                creator.canvas,
                QEvent.MouseButtonPress,
                sample,
                Qt.LeftButton,
                Qt.LeftButton,
                Qt.AltModifier,
            )
        )
        assert creator.palette.currentItem().text() == "Ground"

        enemy_index = len(creator.record.source["entities"])
        creator._apply_canvas_operation(
            {
                "kind": "entity",
                "value": {
                    "kind": "enemy",
                    "x": 120,
                    "y": 112,
                    "params": [16, 16, 0, 0],
                },
            }
        )
        creator.canvas._selected_entity = enemy_index
        creator.canvas.keyPressEvent(
            QKeyEvent(QEvent.KeyPress, Qt.Key_Right, Qt.NoModifier)
        )
        assert creator.record.source["entities"][enemy_index]["x"] == 121
        creator.canvas.keyPressEvent(
            QKeyEvent(
                QEvent.KeyPress,
                Qt.Key_D,
                Qt.ControlModifier,
            )
        )
        assert len(creator.record.source["entities"]) == enemy_index + 2
        assert creator.record.source["entities"][-1]["x"] == 136
    finally:
        creator.close()


def test_legacy_kit_never_guesses_gameplay_parts_from_unknown_cells(tmp_path):
    _application()
    private_root = tmp_path / "maker-lite"
    kit_id = "legacy-mario"
    kit_dir = private_root / "kits" / kit_id
    kit_dir.mkdir(parents=True)
    (kit_dir / "art.mla").write_bytes(diagnostic_art(kit_id, "mario"))
    MakerLiteProjectStore(private_root).create("mario", kit_id)
    creator = MakerLiteCreator(str(private_root), str(tmp_path))
    try:
        labels = {
            creator.palette.item(index).text()
            for index in range(creator.palette.count())
        }
        assert "Ground" not in labels
        assert "Enemy" not in labels
        assert {"Select / Move", "Eraser"} <= labels
        assert "guessed art is hidden" in creator.palette_hint.text()
    finally:
        creator.close()


@pytest.mark.parametrize(
    ("ruleset", "catalog", "expected_labels"),
    [
        (
            "mario",
            [
                {
                    "id": "smw-ground",
                    "label": "SMW Ground",
                    "category": "Terrain",
                    "type": "terrain",
                    "cell": 20,
                    "collision": ["solid"],
                },
                {
                    "id": "goomba",
                    "label": "Goomba",
                    "category": "Enemies",
                    "type": "entity",
                    "cell": 34,
                    "kind": "enemy",
                    "params": [16, 16, 96, 0],
                    "flags": 0,
                },
            ],
            {"SMW Ground", "Goomba"},
        ),
        (
            "zelda",
            [
                {
                    "id": "dungeon-floor",
                    "label": "Dungeon Floor",
                    "category": "Terrain",
                    "type": "terrain",
                    "cell": 21,
                    "collision": [],
                },
                {
                    "id": "octorok",
                    "label": "Octorok",
                    "category": "Enemies",
                    "type": "entity",
                    "cell": 34,
                    "kind": "enemy",
                    "params": [16, 16, 128, 0],
                    "flags": 0,
                },
                {
                    "id": "small-key",
                    "label": "Small Key",
                    "category": "Items",
                    "type": "entity",
                    "cell": 36,
                    "kind": "key",
                    "params": [],
                    "flags": 0,
                },
            ],
            {"Dungeon Floor", "Octorok", "Small Key"},
        ),
        (
            "sonic",
            [
                {
                    "id": "green-hill-ground",
                    "label": "Green Hill Ground",
                    "category": "Terrain",
                    "type": "terrain",
                    "cell": 22,
                    "collision": ["solid"],
                },
                {
                    "id": "badnik",
                    "label": "Badnik",
                    "category": "Enemies",
                    "type": "entity",
                    "cell": 34,
                    "kind": "enemy",
                    "params": [16, 16, 160, 0],
                    "flags": 0,
                },
                {
                    "id": "ring",
                    "label": "Ring",
                    "category": "Items",
                    "type": "entity",
                    "cell": 33,
                    "kind": "collectible",
                    "params": [12, 12, 100, 0],
                    "flags": 0,
                },
            ],
            {"Green Hill Ground", "Badnik", "Ring"},
        ),
    ],
)
def test_each_ruleset_uses_its_private_catalog_with_categories_and_pins(
    tmp_path,
    ruleset,
    catalog,
    expected_labels,
):
    app = _application()
    private_root = tmp_path / f"maker-lite-{ruleset}"
    kit_id = f"diagnostic-{ruleset}"
    _write_diagnostic_kit(
        private_root,
        kit_id,
        ruleset,
        catalog,
    )
    store = MakerLiteProjectStore(private_root)
    store.create(ruleset, kit_id)
    creator = MakerLiteCreator(str(private_root), str(tmp_path))
    creator.resize(1180, 720)
    creator.show()
    app.processEvents()
    try:
        labels = {
            creator.palette.item(index).text()
            for index in range(creator.palette.count())
        }
        assert expected_labels <= labels
        assert "Enemy" not in labels
        category_values = {
            creator.palette_category.itemData(index)
            for index in range(creator.palette_category.count())
        }
        assert {"Terrain", "Enemies", "__pinned__", "__recent__"} <= (
            category_values
        )

        first = _palette_item(creator, catalog[0]["label"])
        assert not first.icon().isNull()
        assert creator.palette.mimeData([first]).hasFormat(ASSET_MIME_TYPE)
        creator.palette.setCurrentItem(first)
        creator.pin_part.click()
        assert catalog[0]["id"] in creator.record.source["metadata"][
            "favorite_assets"
        ]
        creator.palette_category.setCurrentIndex(
            creator.palette_category.findData("__pinned__")
        )
        pinned = {
            creator.palette.item(index).text()
            for index in range(creator.palette.count())
            if not creator.palette.item(index).isHidden()
        }
        assert pinned == {catalog[0]["label"]}

        creator.palette_category.setCurrentIndex(
            creator.palette_category.findData("")
        )
        mime_data = creator.palette.mimeData([first])
        view_point = creator.canvas.mapFromScene(QPointF(96, 64))
        creator.canvas.dropEvent(
            QDropEvent(
                QPointF(view_point),
                Qt.CopyAction,
                mime_data,
                Qt.LeftButton,
                Qt.NoModifier,
            )
        )
        assert creator._recent_asset_ids[0] == catalog[0]["id"]
        creator.palette_category.setCurrentIndex(
            creator.palette_category.findData("__recent__")
        )
        recent = {
            creator.palette.item(index).text()
            for index in range(creator.palette.count())
            if not creator.palette.item(index).isHidden()
        }
        assert recent == {catalog[0]["label"]}
    finally:
        creator.close()


def test_neon_nook_toolbar_install_creates_playable_starter(tmp_path):
    app = _application()
    private_root = tmp_path / "maker-lite"
    creator = MakerLiteCreator(str(private_root), str(ROOT))
    try:
        creator._install_neon_nook()
        app.processEvents()

        assert creator.record is not None
        assert creator.record.source["kit_id"] == "zelda-neon-nook-v1"
        assert creator.record.source["title"] == "Neon Nook"
        assert creator.record.source["metadata"]["show_in_steam"] is True
        assert creator.record.source["metadata"]["genre"] == (
            "Cozy cyberpunk life sim"
        )
        assert creator.record.source["gameplay"] == "life_sim"
        assert creator.record.source["size"] == [80, 56]
        assert creator.record.source["city_revision"] == 2
        assert creator.record.source["apartment"]["owned"] is True
        kinds = {
            entity["kind"] for entity in creator.record.source["entities"]
        }
        assert {
            "house",
            "shop",
            "car",
            "npc",
            "collectible",
            "furniture",
            "door",
        } <= kinds
        assert "goal" not in kinds
        assert sum(
            entity["kind"] == "npc"
            for entity in creator.record.source["entities"]
        ) >= 20
        assert sum(
            entity["kind"] == "furniture"
            for entity in creator.record.source["entities"]
        ) == 10
        player = next(
            entity
            for entity in creator.record.source["entities"]
            if entity["kind"] == "player"
        )
        assert player["x"] < 16 * 16 and player["y"] < 14 * 16
        assert len(creator.canvas.asset_catalog()) >= 180
        assert creator.store.compile(creator.record).startswith(b"RPML")
    finally:
        creator.close()

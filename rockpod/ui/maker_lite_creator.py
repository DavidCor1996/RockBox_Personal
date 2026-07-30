"""Modern three-pane Maker Lite creator embedded in Rockpod."""

from __future__ import annotations

import copy
import json
import os

from PySide6.QtCore import QMimeData, QSize, Qt, Signal
from PySide6.QtGui import (
    QAction,
    QDrag,
    QIcon,
    QPixmap,
    QUndoCommand,
    QUndoStack,
)
from PySide6.QtWidgets import (
    QAbstractItemView,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFileDialog,
    QFormLayout,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QInputDialog,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QSplitter,
    QToolBar,
    QVBoxLayout,
    QWidget,
)

from services.maker_lite_assets import (
    BUILTIN_EXTRACTION_REVISIONS,
    MakerLiteAssetError,
    discover_extraction_recipes,
    import_extraction_bundle,
    import_extraction_recipe,
    inspect_source,
    install_bundled_neon_nook_kit,
    install_private_asset_suite,
    list_private_kits,
    scan_supported_sources,
    supported_revision,
)
from services.maker_lite_projects import GAME_TYPES, MakerLiteProjectStore
from services.maker_lite_neon_nook import create_neon_nook_city
from services.maker_lite_brawl import (
    compose_private_guest_kit,
    install_bundled_brawl_kits,
)
from services.maker_lite_runtime import MakerLiteRuntime, MakerLiteRuntimeError
from services.maker_lite_settings import (
    ACTIONS,
    PHYSICAL_BUTTONS,
    load_settings,
    save_settings,
)
from services.maker_lite_open_surge import (
    import_open_surge_subset,
    load_open_surge_brick_mapping,
)
from ui.maker_lite_canvas import ASSET_MIME_TYPE, MakerLiteCanvas
from ui.maker_lite_cover_dialog import MakerLiteCoverDialog
from ui.maker_lite_preview import MakerLitePreviewDialog


TOOL_ROLE = Qt.UserRole
DRAGGABLE_ROLE = Qt.UserRole + 1
ASSET_ID_ROLE = Qt.UserRole + 2
CATEGORY_ROLE = Qt.UserRole + 3


class _ProjectMutation(QUndoCommand):
    def __init__(self, creator, before, after, label):
        super().__init__(label)
        self.creator = creator
        self.before = before
        self.after = after

    def undo(self):
        self.creator._replace_source(self.before)

    def redo(self):
        self.creator._replace_source(self.after)


class MakerLiteAssetPalette(QListWidget):
    """Copy-only drag source for verified Maker Lite asset cards."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setDragEnabled(True)
        self.setDragDropMode(QAbstractItemView.DragOnly)
        self.setDefaultDropAction(Qt.CopyAction)
        self.setSelectionMode(QAbstractItemView.SingleSelection)
        self.setIconSize(QSize(32, 32))
        self.setSpacing(3)
        self.setUniformItemSizes(True)

    def mimeData(self, items):
        mime_data = QMimeData()
        if len(items) != 1:
            return mime_data
        item = items[0]
        tool = item.data(TOOL_ROLE)
        if not bool(item.data(DRAGGABLE_ROLE)) or not isinstance(tool, dict):
            return mime_data
        payload = json.dumps(
            tool,
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
        mime_data.setData(ASSET_MIME_TYPE, payload)
        mime_data.setText(item.text())
        return mime_data

    def startDrag(self, _supported_actions):
        item = self.currentItem()
        if item is None:
            return
        mime_data = self.mimeData([item])
        if not mime_data.hasFormat(ASSET_MIME_TYPE):
            return
        drag = QDrag(self)
        drag.setMimeData(mime_data)
        pixmap = item.icon().pixmap(self.iconSize())
        if not pixmap.isNull():
            drag.setPixmap(pixmap)
            drag.setHotSpot(pixmap.rect().center())
        drag.exec(Qt.CopyAction, Qt.CopyAction)


class MakerLiteCreator(QWidget):
    status_changed = Signal(str)
    sync_requested = Signal(str)
    PALETTE_PAGE_SIZE = 72

    def __init__(self, private_root: str, repo_root: str, parent=None):
        super().__init__(parent)
        self.private_root = os.path.abspath(private_root)
        self.repo_root = os.path.abspath(repo_root)
        self.store = MakerLiteProjectStore(self.private_root)
        self.runtime = MakerLiteRuntime(repo_root, self.private_root)
        self.record = None
        self._kit_signature = None
        self._palette_signature = None
        self._recent_asset_ids = []
        self._palette_page = 0
        self.undo_stack = QUndoStack(self)
        self._build_ui()
        self.refresh()

    def _build_ui(self):
        self.setObjectName("maker_lite_creator")
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        toolbar = QToolBar()
        toolbar.setObjectName("maker_lite_toolbar")
        for text, callback in (
            ("New", self._new_project),
            ("Import Project", self._import_project),
            ("Install Neon Nook", self._install_neon_nook),
            ("Install Maker Brawl", self._install_maker_brawl),
            ("Create Guest Matchup", self._create_guest_matchup),
            ("Install Game Assets", self._install_asset_suite),
            ("Import Private Kit", self._import_kit),
            ("Import Open Surge", self._import_open_surge),
            ("Add Room", self._add_room),
            ("Link Rooms", self._link_rooms),
            ("Resize Map", self._resize_map),
            ("Save", self._save),
            ("Test", self._test),
            ("Change Cover", self._change_cover),
            ("Sync", self._sync),
        ):
            action = QAction(text, self)
            action.triggered.connect(callback)
            toolbar.addAction(action)
        toolbar.addSeparator()
        toolbar.addAction(self.undo_stack.createUndoAction(self, "Undo"))
        toolbar.addAction(self.undo_stack.createRedoAction(self, "Redo"))
        self.brush_shape = QComboBox()
        self.brush_shape.setObjectName("maker_lite_brush_shape")
        for label, value in (
            ("Brush: Freehand", "freehand"),
            ("Brush: Line", "line"),
            ("Brush: Rectangle", "rectangle"),
            ("Brush: Fill", "fill"),
        ):
            self.brush_shape.addItem(label, value)
        self.brush_shape.setToolTip(
            "Terrain brush shape. Shortcuts: B freehand, L line, "
            "R rectangle, F fill."
        )
        self.brush_shape.currentIndexChanged.connect(
            lambda index: self.canvas.set_brush_shape(
                self.brush_shape.itemData(index)
            )
        )
        toolbar.addWidget(self.brush_shape)
        self.target = QComboBox()
        self.target.addItems(["Connected iPod", "Bound Simulator"])
        toolbar.addWidget(self.target)
        layout.addWidget(toolbar)

        splitter = QSplitter()
        browser_frame = QFrame()
        browser_frame.setObjectName("maker_lite_browser")
        browser_layout = QVBoxLayout(browser_frame)
        browser_layout.addWidget(QLabel("PROJECTS"))
        self.projects = QListWidget()
        self.projects.setObjectName("maker_lite_projects")
        self.projects.currentItemChanged.connect(self._project_selected)
        browser_layout.addWidget(self.projects)
        splitter.addWidget(browser_frame)

        self.canvas = MakerLiteCanvas()
        self.canvas.operation_requested.connect(self._apply_canvas_operation)
        self.canvas.test_requested.connect(self._test)
        self.canvas.tool_picked.connect(self._canvas_tool_picked)
        self.canvas.brush_shape_changed.connect(self._canvas_brush_shape_changed)
        splitter.addWidget(self.canvas)

        inspector = QFrame()
        inspector.setObjectName("maker_lite_inspector")
        inspector_layout = QVBoxLayout(inspector)
        inspector_layout.addWidget(QLabel("PARTS"))
        self.palette_search = QLineEdit()
        self.palette_search.setPlaceholderText("Search objects…")
        self.palette_search.setClearButtonEnabled(True)
        self.palette_search.textChanged.connect(self._palette_query_changed)
        inspector_layout.addWidget(self.palette_search)
        self.palette_hint = QLabel(
            "Drag a verified asset onto the level, or click it to paint."
        )
        self.palette_hint.setWordWrap(True)
        self.palette_hint.setObjectName("maker_lite_palette_hint")
        inspector_layout.addWidget(self.palette_hint)
        palette_controls = QHBoxLayout()
        self.palette_category = QComboBox()
        self.palette_category.addItem("All Parts", "")
        self.palette_category.currentIndexChanged.connect(
            self._palette_category_changed
        )
        self.pin_part = QPushButton("☆ Pin")
        self.pin_part.setEnabled(False)
        self.pin_part.clicked.connect(self._toggle_pin)
        palette_controls.addWidget(self.palette_category, 1)
        palette_controls.addWidget(self.pin_part)
        inspector_layout.addLayout(palette_controls)
        self.palette = MakerLiteAssetPalette()
        self.palette.setObjectName("maker_lite_parts")
        self.palette.setToolTip(
            "Drag terrain and objects onto the canvas. "
            "Tools without artwork remain click tools."
        )
        self.palette.currentItemChanged.connect(self._palette_selected)
        inspector_layout.addWidget(self.palette)
        page_controls = QHBoxLayout()
        self.palette_previous = QPushButton("‹")
        self.palette_previous.setObjectName("maker_lite_page_button")
        self.palette_previous.setToolTip("Previous parts page")
        self.palette_previous.clicked.connect(
            lambda: self._change_palette_page(-1)
        )
        self.palette_page_label = QLabel("Page 1 of 1")
        self.palette_page_label.setObjectName("maker_lite_page_label")
        self.palette_page_label.setAlignment(Qt.AlignCenter)
        self.palette_next = QPushButton("›")
        self.palette_next.setObjectName("maker_lite_page_button")
        self.palette_next.setToolTip("Next parts page")
        self.palette_next.clicked.connect(
            lambda: self._change_palette_page(1)
        )
        page_controls.addWidget(self.palette_previous)
        page_controls.addWidget(self.palette_page_label, 1)
        page_controls.addWidget(self.palette_next)
        inspector_layout.addLayout(page_controls)
        self._populate_palette("mario")
        inspector_layout.addWidget(QLabel("PROJECT"))
        form = QFormLayout()
        self.title_edit = QLineEdit()
        self.title_edit.editingFinished.connect(self._update_metadata)
        self.author_edit = QLineEdit()
        self.author_edit.editingFinished.connect(self._update_metadata)
        self.ruleset_label = QLabel("—")
        self.kit_label = QLabel("—")
        controls_row = QHBoxLayout()
        self.controls = QComboBox()
        self.controls.addItem("Stock", "stock")
        self.controls.addItem("Left-handed", "left_handed")
        self.controls.addItem("Custom", "custom")
        self.controls.currentIndexChanged.connect(self._controls_changed)
        customize = QPushButton("Customize…")
        customize.clicked.connect(self._customize_controls)
        controls_row.addWidget(self.controls, 1)
        controls_row.addWidget(customize)
        form.addRow("Title", self.title_edit)
        form.addRow("Author", self.author_edit)
        form.addRow("Rules", self.ruleset_label)
        form.addRow("Private kit", self.kit_label)
        form.addRow("Controls", controls_row)
        inspector_layout.addLayout(form)
        inspector_layout.addStretch(1)
        self.validation = QLabel("Choose or create a project")
        self.validation.setWordWrap(True)
        inspector_layout.addWidget(self.validation)
        splitter.addWidget(inspector)
        splitter.setSizes([190, 700, 250])
        layout.addWidget(splitter, 1)

    def _install_neon_nook(self):
        """Install the original bundled kit and create its gameplay city."""

        try:
            kit = install_bundled_neon_nook_kit(self.private_root)
            record, _created, manifest = create_neon_nook_city(
                self.store, kit
            )
        except (OSError, ValueError, MakerLiteAssetError) as exc:
            QMessageBox.warning(self, "Neon Nook Installation Failed", str(exc))
            return

        self.record = record
        self._kit_signature = None
        self._palette_signature = None
        self.refresh()
        self.status_changed.emit(
            f"Installed Neon Nook: {kit.cell_count} cells and "
            f"{len(manifest.get('asset_catalog', []))} draggable parts"
        )

    def _install_maker_brawl(self):
        try:
            manifests = install_bundled_brawl_kits(
                self.private_root,
                self.repo_root,
            )
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            QMessageBox.warning(
                self,
                "Maker Brawl Installation Failed",
                str(exc),
            )
            return
        self._kit_signature = None
        self._palette_signature = None
        self.refresh()
        self.status_changed.emit(
            f"Installed {len(manifests)} original Maker Brawl fighters"
        )

    def _create_guest_matchup(self):
        kits = list_private_kits(self.private_root)
        if len(kits) < 2:
            QMessageBox.information(
                self,
                "Two Fighter Kits Required",
                "Install or privately import at least two Maker Lite kits.",
            )
            return
        labels = [
            f"{kit.get('player_name', kit['ruleset'].title())} — "
            f"{kit['kit_id']}"
            for kit in kits
        ]
        player_label, accepted = QInputDialog.getItem(
            self,
            "Create Guest Matchup",
            "Player fighter kit",
            labels,
            0,
            False,
        )
        if not accepted:
            return
        player_index = labels.index(player_label)
        opponent_labels = [
            label
            for index, label in enumerate(labels)
            if index != player_index
        ]
        opponent_label, accepted = QInputDialog.getItem(
            self,
            "Create Guest Matchup",
            "CPU fighter kit",
            opponent_labels,
            0,
            False,
        )
        if not accepted:
            return
        opponent_index = labels.index(opponent_label)
        try:
            manifest = compose_private_guest_kit(
                self.private_root,
                kits[player_index]["kit_id"],
                kits[opponent_index]["kit_id"],
            )
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            QMessageBox.warning(
                self,
                "Guest Matchup Failed",
                str(exc),
            )
            return
        self._kit_signature = None
        self._palette_signature = None
        self.refresh()
        self.status_changed.emit(
            f"Created private guest kit {manifest['kit_id']}"
        )

    def refresh(self):
        current = self.record.project_id if self.record else ""
        self.projects.clear()
        for record in self.store.list():
            item = QListWidgetItem(record.source["title"])
            item.setData(0x0100, record.project_id)
            self.projects.addItem(item)
            if record.project_id == current:
                self.projects.setCurrentItem(item)
        if self.projects.count() and self.projects.currentRow() < 0:
            self.projects.setCurrentRow(0)

    def _project_selected(self, item):
        if not item:
            return
        self.record = self.store.load(item.data(0x0100))
        self.undo_stack.clear()
        self._replace_source(self.record.source)
        if self.record.recovered:
            self.status_changed.emit("Recovered project from autosave")

    def _replace_source(self, source):
        if not self.record:
            return
        self.record.source = copy.deepcopy(source)
        art_path = os.path.join(
            self.private_root,
            "kits",
            self.record.source["kit_id"],
            "art.mla",
        )
        try:
            art_stat = os.stat(art_path)
            manifest_path = os.path.join(
                os.path.dirname(art_path),
                "kit.mlk",
            )
            try:
                manifest_stat = os.stat(manifest_path)
                manifest_signature = (
                    int(manifest_stat.st_mtime_ns),
                    int(manifest_stat.st_size),
                )
            except OSError:
                manifest_signature = (None, None)
            kit_signature = (
                art_path,
                int(art_stat.st_mtime_ns),
                int(art_stat.st_size),
                *manifest_signature,
            )
        except OSError:
            kit_signature = (art_path, None, None, None, None)
        kit_changed = kit_signature != self._kit_signature
        if kit_changed:
            self.canvas.set_asset_kit(art_path)
            self._kit_signature = kit_signature
        self.canvas.set_project(self.record.source)
        self.title_edit.setText(self.record.source["title"])
        metadata = self.record.source.get("metadata", {})
        self.author_edit.setText(metadata.get("author", ""))
        self.ruleset_label.setText(self.record.source["ruleset"].title())
        self.kit_label.setText(self.record.source["kit_id"])
        settings = load_settings(self.private_root, self.record.project_id)
        self.controls.blockSignals(True)
        self.controls.setCurrentIndex(
            max(0, self.controls.findData(settings["preset"]))
        )
        self.controls.blockSignals(False)
        palette_signature = (
            self.record.source["ruleset"],
            self._kit_signature,
        )
        if palette_signature != self._palette_signature:
            self._populate_palette(self.record.source["ruleset"])
            self._palette_signature = palette_signature
        self._filter_palette(self.palette_search.text())
        self._update_pin_button()
        self.store.autosave(self.record)
        self.validation.setText(
            f"{len(self.canvas.asset_catalog())} verified draggable parts · "
            f"{len(self.record.source.get('terrain', []))} terrain operations · "
            f"{len(self.record.source.get('entities', []))} entities · "
            f"{len(self.record.source.get('paths', []))} paths · "
            f"{len(self.record.source.get('events', []))} events · "
            f"{len(self.record.source.get('rooms', []))} rooms · "
            f"{len(self.record.source.get('room_links', []))} links · "
            "60 Hz deterministic preview"
        )

    def _new_project(self):
        kits = [
            kit
            for kit in list_private_kits(self.private_root)
            if kit.get("asset_catalog")
        ]
        if not kits:
            QMessageBox.information(
                self, "Private Kit Required",
                "Install Neon Nook or import a locally extracted Mario, Zelda, "
                "or Sonic kit with a verified draggable parts catalog first.",
            )
            return
        choices = [
            f"{kit['ruleset'].title()} — {kit['kit_id']}" for kit in kits
        ]
        choice, accepted = QInputDialog.getItem(
            self, "New Maker Lite Project", "Verified asset kit", choices, 0, False
        )
        if not accepted:
            return
        kit = kits[choices.index(choice)]
        game_types = GAME_TYPES[kit["ruleset"]]
        type_labels = [label for _value, label in game_types]
        type_choice, accepted = QInputDialog.getItem(
            self,
            "Choose Game Type",
            "Starter layout",
            type_labels,
            0,
            False,
        )
        if not accepted:
            return
        game_type = game_types[type_labels.index(type_choice)][0]
        opponent = None
        if game_type == "brawl":
            fighters = [
                asset
                for asset in kit.get("asset_catalog", [])
                if asset.get("type") == "entity"
                and asset.get("kind") == "enemy"
                and "fighter" in asset.get("category", "").casefold()
            ]
            if not fighters:
                fighters = [
                    asset
                    for asset in kit.get("asset_catalog", [])
                    if asset.get("type") == "entity"
                    and asset.get("kind") == "enemy"
                ]
            if fighters:
                labels = [asset["label"] for asset in fighters]
                opponent_name, accepted = QInputDialog.getItem(
                    self,
                    "Choose CPU Fighter",
                    "Opponent",
                    labels,
                    0,
                    False,
                )
                if not accepted:
                    return
                opponent = fighters[labels.index(opponent_name)]
        record = self.store.create(
            kit["ruleset"],
            kit["kit_id"],
            game_type=game_type,
        )
        solid_ground = next(
            (
                asset
                for asset in kit.get("asset_catalog", [])
                if asset.get("type") == "terrain"
                and "solid" in asset.get("collision", [])
            ),
            None,
        )
        if solid_ground:
            for terrain in record.source.get("terrain", []):
                if int(terrain.get("tile", 0)) == 1:
                    terrain["tile"] = int(solid_ground["cell"])
                    terrain["asset_id"] = solid_ground["id"]
        entity_assets = {}
        for asset in kit.get("asset_catalog", []):
            if asset.get("type") == "entity":
                entity_assets.setdefault(asset.get("kind"), asset)
        for entity in record.source.get("entities", []):
            asset = entity_assets.get(entity.get("kind"))
            if asset:
                entity["asset_id"] = asset["id"]
                entity["render_cell"] = int(asset["cell"])
        if opponent:
            enemy = next(
                entity
                for entity in record.source["entities"]
                if entity["kind"] == "enemy"
            )
            enemy["asset_id"] = opponent["id"]
            enemy["render_cell"] = int(
                opponent.get("frame", opponent["cell"])
            )
            enemy["params"] = list(
                opponent.get("params", [14, 26, 100, 3])
            )
            enemy["params"].extend([0] * (4 - len(enemy["params"])))
            enemy["params"][3] = max(1, enemy["params"][3] or 3)
            record.source["metadata"]["brawl_opponent"] = opponent["id"]
        source_cover_file = str(kit.get("source_cover_file", ""))
        source_cover = os.path.join(
            self.private_root,
            "kits",
            kit["kit_id"],
            os.path.basename(source_cover_file),
        )
        if source_cover_file and os.path.isfile(source_cover):
            record.source["metadata"]["cover_source"] = source_cover
            record.source["metadata"]["show_in_steam"] = True
        self.store.save(record)
        self.record = record
        self.refresh()

    def _canvas_tool_picked(self, tool):
        asset_id = str(tool.get("asset_id", ""))
        for index in range(self.palette.count()):
            item = self.palette.item(index)
            candidate = item.data(TOOL_ROLE)
            if (
                (asset_id and item.data(ASSET_ID_ROLE) == asset_id)
                or (
                    not asset_id
                    and isinstance(candidate, dict)
                    and candidate == tool
                )
            ):
                self.palette_category.setCurrentIndex(0)
                self.palette_search.clear()
                self.palette.setCurrentItem(item)
                self.palette.scrollToItem(item)
                self.status_changed.emit(f"Picked {item.text()}")
                return

    def _canvas_brush_shape_changed(self, shape):
        index = self.brush_shape.findData(shape)
        if index >= 0 and index != self.brush_shape.currentIndex():
            self.brush_shape.blockSignals(True)
            self.brush_shape.setCurrentIndex(index)
            self.brush_shape.blockSignals(False)

    def _install_asset_suite(self):
        source_root = QFileDialog.getExistingDirectory(
            self,
            "Choose Folder Containing Your Owned Game Images",
        )
        if not source_root:
            return
        recipe_root = QFileDialog.getExistingDirectory(
            self,
            "Choose Folder Containing Maker Lite Extraction Recipes",
            source_root,
        )
        try:
            sources = scan_supported_sources([source_root])
            recipes = discover_extraction_recipes(
                [recipe_root] if recipe_root else []
            )
        except (OSError, MakerLiteAssetError) as exc:
            QMessageBox.warning(self, "Game Asset Scan Failed", str(exc))
            return
        source_lines = [
            f"• {match.revision.ruleset.title()}: "
            f"{match.revision.revision_id}"
            for match in sources
        ]
        recipe_keys = {
            (recipe.ruleset, recipe.revision_id) for recipe in recipes
        }
        ready = [
            match
            for match in sources
            if (
                match.revision.ruleset,
                match.revision.revision_id,
            )
            in recipe_keys
            or match.revision.revision_id in BUILTIN_EXTRACTION_REVISIONS
        ]
        if not ready:
            QMessageBox.information(
                self,
                "No Installable Game Assets",
                "\n".join(
                    [
                        "No verified source/extractor pair was found.",
                        "",
                        *(source_lines or ["No supported game images found."]),
                        "",
                        "Expected SMW US 1.0, ALttP US 1.0, and "
                        "Genesis Sonic 2 Rev 00/01 or Sonic 3 US.",
                    ]
                ),
            )
            return
        answer = QMessageBox.question(
            self,
            "Install Authentic Private Game Assets",
            "\n".join(
                [
                    "Verified owned sources:",
                    *source_lines,
                    "",
                    f"{len(ready)} verified extractor(s) are ready.",
                    "Exact pixels will be installed in Rockpod's private data.",
                    "The source images will not be copied or synced to the iPod.",
                    "",
                    "Continue?",
                ]
            ),
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if answer != QMessageBox.Yes:
            return
        report = install_private_asset_suite(
            [source_root],
            [recipe_root] if recipe_root else [],
            self.private_root,
        )
        summary = [
            f"Installed {len(report.installed)} private authentic kit(s)."
        ]
        if report.missing_sources:
            summary.append(
                "Missing owned sources: " + ", ".join(report.missing_sources)
            )
        if report.missing_recipes:
            summary.append(
                "Missing recipes: " + ", ".join(report.missing_recipes)
            )
        summary.extend(report.errors)
        if report.installed:
            self.refresh()
            self.status_changed.emit(summary[0])
        QMessageBox.information(
            self,
            "Private Game Asset Installation",
            "\n".join(summary),
        )

    def _import_kit(self):
        modes = [
            "Extraction recipe (exact PNG/native 4bpp)",
            "Prepared atlas bundle",
        ]
        mode, accepted = QInputDialog.getItem(
            self,
            "Import Authentic Private Kit",
            "Local extraction format",
            modes,
            0,
            False,
        )
        if not accepted:
            return
        source, _ = QFileDialog.getOpenFileName(
            self, "Choose your reference game", "", "Game images (*.sfc *.smc *.md *.gen *.bin)"
        )
        if not source:
            return
        try:
            if mode == modes[0]:
                recipe, _ = QFileDialog.getOpenFileName(
                    self,
                    "Choose maker-lite-extract.json from the local extraction workspace",
                    "",
                    "Maker Lite extraction recipes (*.json)",
                )
                if not recipe:
                    return
                with open(recipe, "r", encoding="utf-8") as manifest_file:
                    manifest = json.load(manifest_file)
                if not isinstance(manifest, dict):
                    raise MakerLiteAssetError(
                        "Extraction recipe root must be an object"
                    )
                ruleset = manifest.get("ruleset", "")
                if not self._confirm_private_source(source, ruleset):
                    return
                kit = import_extraction_recipe(
                    source, recipe, self.private_root
                )
            else:
                bundle = QFileDialog.getExistingDirectory(
                    self, "Choose the matching extracted-pixel bundle"
                )
                if not bundle:
                    return
                with open(
                    os.path.join(bundle, "kit.json"),
                    "r",
                    encoding="utf-8",
                ) as manifest_file:
                    manifest = json.load(manifest_file)
                if not isinstance(manifest, dict):
                    raise MakerLiteAssetError("kit.json root must be an object")
                ruleset = manifest.get("ruleset", "")
                if not self._confirm_private_source(source, ruleset):
                    return
                kit = import_extraction_bundle(
                    source, bundle, self.private_root
                )
        except (OSError, ValueError, MakerLiteAssetError) as exc:
            QMessageBox.warning(self, "Kit Import Failed", str(exc))
            return
        self.status_changed.emit(
            f"Imported {kit.ruleset.title()} private kit {kit.kit_id}; source ROM was not copied"
        )
        self._new_project()

    def _confirm_private_source(self, source, ruleset):
        info = inspect_source(source)
        revision = supported_revision(info, str(ruleset).lower())
        size_mib = info.size / (1024 * 1024)
        answer = QMessageBox.question(
            self,
            "Confirm Authentic Source",
            "\n".join(
                [
                    f"Title: {info.title}",
                    f"Platform: {info.platform}",
                    f"Revision: {revision.revision_id}",
                    f"Size: {size_mib:.2f} MiB",
                    f"Canonical SHA-256: {info.canonical_sha256}",
                    "",
                    "Extract exact private cells from this local source?",
                ]
            ),
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        return answer == QMessageBox.Yes

    def _import_project(self):
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Import Maker Lite Project",
            "",
            "Maker Lite source projects (*.json)",
        )
        if not path:
            return
        try:
            record = self.store.import_file(path)
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "Project Import Failed", str(exc))
            return
        self.record = record
        self.refresh()
        self.status_changed.emit(f"Imported {record.source['title']}")

    def _import_open_surge(self):
        if not self.record:
            return
        path, _ = QFileDialog.getOpenFileName(
            self, "Import Open Surge 0.6 Level Subset", "", "Open Surge levels (*.lev);;Text files (*)"
        )
        if not path:
            return
        try:
            mapping_path = os.path.splitext(path)[0] + ".maker-lite-bricks.json"
            brick_collision = (
                load_open_surge_brick_mapping(mapping_path)
                if os.path.isfile(mapping_path)
                else None
            )
            with open(path, "r", encoding="utf-8") as source:
                imported, diagnostics = import_open_surge_subset(
                    source.read(),
                    self.record.source,
                    brick_collision=brick_collision,
                )
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "Open Surge Import", str(exc))
            return
        before = copy.deepcopy(self.record.source)
        self.undo_stack.push(
            _ProjectMutation(self, before, imported, "Import Open Surge subset")
        )
        if diagnostics:
            summary = "\n".join(
                f"Line {item.line}: {item.severity}: {item.message}"
                for item in diagnostics[:20]
            )
            QMessageBox.information(self, "Open Surge Import Diagnostics", summary)
        self.status_changed.emit(
            "Imported supported Open Surge bricks/entities "
            f"with {len(diagnostics)} diagnostic(s)"
            + (" and a reviewed collision sidecar" if brick_collision else "")
        )

    def _add_room(self):
        if not self.record or self.record.source.get("ruleset") != "zelda":
            QMessageBox.information(
                self, "Zelda Rooms", "Room editing is available for Zelda projects."
            )
            return
        title, accepted = QInputDialog.getText(
            self,
            "Add Zelda Room",
            "Room title",
            text=f"Room {len(self.record.source.get('rooms', [])) + 1}",
        )
        if not accepted or not title.strip():
            return
        before = copy.deepcopy(self.record.source)
        after = copy.deepcopy(before)
        rooms = after.setdefault("rooms", [])
        if len(rooms) >= 64:
            QMessageBox.warning(
                self, "Zelda Rooms", "A project can contain at most 64 rooms."
            )
            return
        room_width = int(after["view"][0]) // int(after.get("tile_size", 16))
        room_height = int(after["view"][1]) // int(after.get("tile_size", 16))
        next_x = max(
            (int(room["rect"][0]) + int(room["rect"][2]) for room in rooms),
            default=0,
        )
        if next_x + room_width > 512:
            QMessageBox.warning(
                self,
                "Zelda Rooms",
                "The new room would exceed the 512-tile map limit.",
            )
            return
        room_id = f"room-{len(rooms) + 1}"
        existing_ids = {str(room.get("id", "")) for room in rooms}
        while room_id in existing_ids:
            room_id += "-new"
        rooms.append(
            {
                "id": room_id,
                "title": title.strip(),
                "rect": [next_x, 0, room_width, room_height],
            }
        )
        after["size"][0] = max(int(after["size"][0]), next_x + room_width)
        after["size"][1] = max(int(after["size"][1]), room_height)
        self.undo_stack.push(
            _ProjectMutation(self, before, after, f"Add {title.strip()}")
        )

    def _link_rooms(self):
        if not self.record or self.record.source.get("ruleset") != "zelda":
            QMessageBox.information(
                self, "Zelda Rooms", "Room links are available for Zelda projects."
            )
            return
        rooms = self.record.source.get("rooms", [])
        if len(rooms) < 2:
            QMessageBox.information(
                self, "Link Rooms", "Create at least two rooms first."
            )
            return
        labels = [
            f"{room.get('title', room['id'])} — {room['id']}" for room in rooms
        ]
        from_label, accepted = QInputDialog.getItem(
            self, "Link Rooms", "From room", labels, 0, False
        )
        if not accepted:
            return
        from_index = labels.index(from_label)
        destination_labels = [
            label for index, label in enumerate(labels) if index != from_index
        ]
        to_label, accepted = QInputDialog.getItem(
            self, "Link Rooms", "To room", destination_labels, 0, False
        )
        if not accepted:
            return
        to_index = labels.index(to_label)
        from_room = rooms[from_index]
        to_room = rooms[to_index]
        tile_size = int(self.record.source.get("tile_size", 16))

        def doorway(room, toward_right):
            x, y, width, height = map(int, room["rect"])
            return [
                (x + width) * tile_size - 8 if toward_right else x * tile_size + 8,
                (y + height // 2) * tile_size,
            ]

        before = copy.deepcopy(self.record.source)
        after = copy.deepcopy(before)
        links = after.setdefault("room_links", [])
        if len(links) >= 64:
            QMessageBox.warning(
                self, "Link Rooms", "A project can contain at most 64 room links."
            )
            return
        links.append(
            {
                "id": f"link-{len(links) + 1}",
                "from_room": from_room["id"],
                "to_room": to_room["id"],
                "from_position": doorway(
                    from_room,
                    int(from_room["rect"][0]) <= int(to_room["rect"][0]),
                ),
                "to_position": doorway(
                    to_room,
                    int(to_room["rect"][0]) < int(from_room["rect"][0]),
                ),
                "reciprocal": True,
                "locked": False,
            }
        )
        self.undo_stack.push(
            _ProjectMutation(
                self,
                before,
                after,
                f"Link {from_room['id']} to {to_room['id']}",
            )
        )

    def _populate_palette(self, ruleset):
        common_tools = [
            ("Select / Move", {"type": "select"}),
            ("Eraser", {"type": "erase"}),
            (
                "Moving Platform Path",
                {"type": "path", "entity_kind": "block", "speed": 64},
            ),
            (
                "Checkpoint Region",
                {"type": "event", "action": "set_checkpoint"},
            ),
            (
                "Effect Region",
                {"type": "event", "action": "play_effect", "effect": 4},
            ),
        ]
        catalog_entries = []
        for asset in self.canvas.asset_catalog():
            tool = {
                "type": asset["type"],
                "cell": int(asset["cell"]),
                "asset_id": asset["id"],
            }
            if asset["type"] == "terrain":
                tool.update(
                    {
                        "tile": int(asset["cell"]),
                        "collision": list(asset.get("collision", [])),
                    }
                )
            else:
                tool.update(
                    {
                        "kind": asset["kind"],
                        "params": list(asset.get("params", [])),
                        "flags": int(asset.get("flags", 0)),
                        **(
                            {"frame": int(asset["frame"])}
                            if "frame" in asset
                            else {}
                        ),
                    }
                )
            catalog_entries.append(
                (
                    asset["label"],
                    tool,
                    asset["category"],
                    asset["id"],
                )
            )
        entries = catalog_entries
        if catalog_entries:
            self.palette_hint.setText(
                f"{len(catalog_entries)} verified parts from this "
                f"{ruleset.title()} kit. Drag one onto the level."
            )
        elif self.record:
            self.palette_hint.setText(
                "This legacy kit has no verified parts catalog. "
                "Re-import it with asset_catalog entries; guessed art is hidden."
            )
        else:
            self.palette_hint.setText(
                "Choose a project with a verified asset kit."
            )
        entries.extend(
            (label, tool, "Tools", "")
            for label, tool in common_tools
        )
        selected_id = (
            self.palette.currentItem().data(ASSET_ID_ROLE)
            if self.palette.currentItem()
            else ""
        )
        selected_label = (
            self.palette.currentItem().text()
            if self.palette.currentItem()
            else ""
        )
        self.palette.blockSignals(True)
        self.palette.clear()
        categories = []
        for label, tool, category, asset_id in entries:
            if (
                tool.get("type") == "entity"
                and tool.get("kind") not in self.canvas.available_entity_kinds()
            ):
                continue
            if (
                tool.get("type") == "path"
                and tool.get("entity_kind")
                and tool.get("entity_kind") not in self.canvas.available_entity_kinds()
            ):
                continue
            if (
                tool.get("type") == "terrain"
                and int(tool.get("tile", 0)) > 0
                and self.canvas.tool_preview(tool) is None
            ):
                continue
            item = QListWidgetItem(label)
            item.setData(TOOL_ROLE, tool)
            draggable = self.canvas.tool_is_draggable(tool)
            item.setData(DRAGGABLE_ROLE, draggable)
            item.setData(ASSET_ID_ROLE, asset_id)
            item.setData(CATEGORY_ROLE, category)
            if category not in categories:
                categories.append(category)
            if draggable:
                preview = self.canvas.tool_preview(tool)
                item.setIcon(
                    QIcon(
                        QPixmap.fromImage(preview).scaled(
                            32,
                            32,
                            Qt.KeepAspectRatio,
                            Qt.FastTransformation,
                        )
                    )
                )
                item.setToolTip(f"Drag {label} onto the level")
            else:
                item.setFlags(item.flags() & ~Qt.ItemIsDragEnabled)
                item.setToolTip(f"Select {label}, then use it on the canvas")
            item.setSizeHint(QSize(0, 40))
            self.palette.addItem(item)
            if (
                (selected_id and asset_id == selected_id)
                or (not selected_id and label == selected_label)
            ):
                self.palette.setCurrentItem(item)
        if self.palette.currentRow() < 0:
            self.palette.setCurrentRow(0)
        self.palette.blockSignals(False)
        if self.palette.currentItem():
            self.canvas.set_tool(self.palette.currentItem().data(TOOL_ROLE))
        selected_category = self.palette_category.currentData()
        self.palette_category.blockSignals(True)
        self.palette_category.clear()
        self.palette_category.addItem("All Parts", "")
        self.palette_category.addItem("★ Pinned", "__pinned__")
        self.palette_category.addItem("Recent", "__recent__")
        for category in categories:
            self.palette_category.addItem(category, category)
        selected_index = self.palette_category.findData(selected_category)
        self.palette_category.setCurrentIndex(max(0, selected_index))
        self.palette_category.blockSignals(False)
        self._filter_palette(self.palette_search.text())
        self._update_pin_button()

    def _filter_palette(self, text):
        query = str(text or "").strip().casefold()
        category = self.palette_category.currentData()
        favorites = self._favorite_asset_ids()
        matches = []
        for index in range(self.palette.count()):
            item = self.palette.item(index)
            asset_id = str(item.data(ASSET_ID_ROLE) or "")
            item_category = str(item.data(CATEGORY_ROLE) or "")
            category_matches = (
                not category
                or (category == "__pinned__" and asset_id in favorites)
                or (
                    category == "__recent__"
                    and asset_id in self._recent_asset_ids
                )
                or item_category == category
            )
            if category_matches and (
                not query or query in item.text().casefold()
            ):
                matches.append(index)
        page_count = max(
            1,
            (len(matches) + self.PALETTE_PAGE_SIZE - 1)
            // self.PALETTE_PAGE_SIZE,
        )
        self._palette_page = max(0, min(self._palette_page, page_count - 1))
        page_start = self._palette_page * self.PALETTE_PAGE_SIZE
        visible_indexes = set(
            matches[page_start : page_start + self.PALETTE_PAGE_SIZE]
        )
        first_visible = None
        for index in range(self.palette.count()):
            item = self.palette.item(index)
            visible = index in visible_indexes
            item.setHidden(not visible)
            if visible and first_visible is None:
                first_visible = item
        self.palette_page_label.setText(
            f"Page {self._palette_page + 1} of {page_count} · "
            f"{len(matches)} parts"
        )
        self.palette_previous.setEnabled(self._palette_page > 0)
        self.palette_next.setEnabled(self._palette_page + 1 < page_count)
        if (
            first_visible is not None
            and (
                self.palette.currentItem() is None
                or self.palette.currentItem().isHidden()
            )
        ):
            self.palette.setCurrentItem(first_visible)
        self._update_pin_button()

    def _palette_query_changed(self, text):
        self._palette_page = 0
        self._filter_palette(text)

    def _palette_category_changed(self, _index):
        self._palette_page = 0
        self._filter_palette(self.palette_search.text())

    def _change_palette_page(self, delta):
        self._palette_page = max(0, self._palette_page + int(delta))
        self._filter_palette(self.palette_search.text())

    def _favorite_asset_ids(self):
        if not self.record:
            return set()
        values = self.record.source.get("metadata", {}).get(
            "favorite_assets",
            [],
        )
        if not isinstance(values, list):
            return set()
        return {
            str(value)
            for value in values
            if isinstance(value, str)
        }

    def _palette_selected(self, item):
        if item:
            self.canvas.set_tool(item.data(TOOL_ROLE))
        self._update_pin_button()

    def _update_pin_button(self):
        item = self.palette.currentItem()
        asset_id = str(item.data(ASSET_ID_ROLE) or "") if item else ""
        self.pin_part.setEnabled(bool(asset_id and self.record))
        self.pin_part.setText(
            "★ Unpin"
            if asset_id and asset_id in self._favorite_asset_ids()
            else "☆ Pin"
        )

    def _toggle_pin(self):
        if not self.record or not self.palette.currentItem():
            return
        asset_id = str(
            self.palette.currentItem().data(ASSET_ID_ROLE) or ""
        )
        if not asset_id:
            return
        before = copy.deepcopy(self.record.source)
        after = copy.deepcopy(before)
        favorites = list(
            dict.fromkeys(
                str(value)
                for value in after.setdefault("metadata", {}).get(
                    "favorite_assets",
                    [],
                )
                if isinstance(value, str)
            )
        )
        if asset_id in favorites:
            favorites.remove(asset_id)
            label = "Unpin part"
        else:
            favorites.append(asset_id)
            label = "Pin part"
        after["metadata"]["favorite_assets"] = favorites[:32]
        self.undo_stack.push(
            _ProjectMutation(self, before, after, label)
        )

    def _resize_map(self):
        if not self.record:
            return
        source = self.record.source
        dialog = QDialog(self)
        dialog.setWindowTitle("Resize Maker Lite Map")
        layout = QVBoxLayout(dialog)
        form = QFormLayout()
        width = QSpinBox()
        height = QSpinBox()
        tile_size = int(source.get("tile_size", 16))
        width.setRange(
            (int(source["view"][0]) + tile_size - 1) // tile_size,
            512,
        )
        height.setRange(
            (int(source["view"][1]) + tile_size - 1) // tile_size,
            64,
        )
        width.setValue(int(source["size"][0]))
        height.setValue(int(source["size"][1]))
        form.addRow("Width (tiles)", width)
        form.addRow("Height (tiles)", height)
        layout.addLayout(form)
        note = QLabel(
            "Growing preserves the level. Shrinking clips terrain but is "
            "blocked if an object, path, event, or Zelda room would be lost."
        )
        note.setWordWrap(True)
        layout.addWidget(note)
        buttons = QDialogButtonBox(
            QDialogButtonBox.Cancel | QDialogButtonBox.Ok
        )
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)
        if dialog.exec() != QDialog.Accepted:
            return
        new_width = width.value()
        new_height = height.value()
        old_width, old_height = map(int, source["size"])
        if new_width == old_width and new_height == old_height:
            return
        pixel_width = new_width * tile_size
        pixel_height = new_height * tile_size
        offenders = []
        for entity in source.get("entities", []):
            if (
                int(entity["x"]) >= pixel_width
                or int(entity["y"]) > pixel_height + 32
            ):
                offenders.append(f"{entity['kind']} object")
        for path in source.get("paths", []):
            if any(
                int(point[0]) >= pixel_width or int(point[1]) > pixel_height
                for point in path.get("points", [])
            ):
                offenders.append(f"path {path.get('id', '?')}")
        for event in source.get("events", []):
            x, y, event_width, event_height = map(
                int, event.get("region", [0, 0, 0, 0])
            )
            if x + event_width > pixel_width or y + event_height > pixel_height:
                offenders.append("event region")
        for room in source.get("rooms", []):
            x, y, room_width, room_height = map(int, room["rect"])
            if x + room_width > new_width or y + room_height > new_height:
                offenders.append(f"room {room['id']}")
        if offenders:
            QMessageBox.warning(
                self,
                "Resize Map",
                "Move these inside the new bounds first: "
                + ", ".join(offenders[:8]),
            )
            return
        before = copy.deepcopy(source)
        after = copy.deepcopy(before)
        after["size"] = [new_width, new_height]
        clipped = []
        for operation in after.get("terrain", []):
            value = copy.deepcopy(operation)
            if "point" in value:
                x, y = map(int, value["point"])
                if x < new_width and y < new_height:
                    clipped.append(value)
                continue
            x, y, rect_width, rect_height = map(int, value["rect"])
            rect_width = min(rect_width, new_width - x)
            rect_height = min(rect_height, new_height - y)
            if x < new_width and y < new_height and rect_width > 0 and rect_height > 0:
                value["rect"] = [x, y, rect_width, rect_height]
                clipped.append(value)
        after["terrain"] = clipped
        self.undo_stack.push(
            _ProjectMutation(
                self,
                before,
                after,
                f"Resize map to {new_width}×{new_height}",
            )
        )

    def _apply_canvas_operation(self, operation):
        if not self.record:
            return
        asset_id = str(operation.get("asset_id", ""))
        available_ids = {
            item["id"]
            for item in self.canvas.asset_catalog()
        }
        if asset_id in available_ids:
            self._recent_asset_ids = [
                asset_id,
                *(
                    existing
                    for existing in self._recent_asset_ids
                    if existing != asset_id
                ),
            ][:12]
        before = copy.deepcopy(self.record.source)
        after = copy.deepcopy(before)
        if operation.get("kind") == "stroke":
            stroke = operation.get("operations")
            if (
                not isinstance(stroke, list)
                or not stroke
                or len(stroke) > 4096
                or any(
                    not isinstance(child, dict)
                    or child.get("kind") not in {"terrain", "erase"}
                    for child in stroke
                )
            ):
                return
            erased = False
            for child in stroke:
                if child["kind"] == "terrain":
                    after.setdefault("terrain", []).append(child["value"])
                    continue
                erased = True
                tile_x, tile_y = child["tile"]
                pixel_x, pixel_y = child["pixel"]
                tile_size = int(after.get("tile_size", 16))
                after.setdefault("terrain", []).append(
                    {
                        "point": [tile_x, tile_y],
                        "tile": 0,
                        "collision": [],
                    }
                )
                after["entities"] = [
                    entity
                    for entity in after.get("entities", [])
                    if entity["kind"] in {"player", "goal"}
                    or abs(int(entity["x"]) - pixel_x) > tile_size
                    or abs(int(entity["y"]) - pixel_y) > tile_size
                ]
            action = "Erase" if erased else "Paint"
            self.undo_stack.push(
                _ProjectMutation(
                    self,
                    before,
                    after,
                    f"{action} {len(stroke)} cells",
                )
            )
            return
        if operation["kind"] == "terrain":
            after.setdefault("terrain", []).append(operation["value"])
        elif operation["kind"] == "erase":
            tile_x, tile_y = operation["tile"]
            pixel_x, pixel_y = operation["pixel"]
            tile_size = int(after.get("tile_size", 16))
            after.setdefault("terrain", []).append(
                {
                    "point": [tile_x, tile_y],
                    "tile": 0,
                    "collision": [],
                }
            )
            after["entities"] = [
                entity
                for entity in after.get("entities", [])
                if entity["kind"] in {"player", "goal"}
                or abs(int(entity["x"]) - pixel_x) > tile_size
                or abs(int(entity["y"]) - pixel_y) > tile_size
            ]
        elif operation["kind"] == "path":
            value = copy.deepcopy(operation["value"])
            surface = bool(value.get("surface", False))
            entity_kind = value.pop("entity_kind", "block")
            path_id = max(
                [int(path.get("id", 0)) for path in after.get("paths", [])],
                default=0,
            ) + 1
            value["id"] = path_id
            after.setdefault("paths", []).append(value)
            if not surface:
                start_x, start_y = value["points"][0]
                after.setdefault("entities", []).append(
                    {
                        "kind": entity_kind,
                        "x": start_x,
                        "y": start_y,
                        "params": [32, 8, 0, path_id],
                    }
                )
        elif operation["kind"] == "event":
            after.setdefault("events", []).append(operation["value"])
        elif operation["kind"] == "move_entity":
            index = int(operation["index"])
            if not 0 <= index < len(after.get("entities", [])):
                return
            after["entities"][index]["x"] = int(operation["x"])
            after["entities"][index]["y"] = int(operation["y"])
        elif operation["kind"] == "delete_entity":
            index = int(operation["index"])
            entities = after.get("entities", [])
            if not 0 <= index < len(entities):
                return
            kind = entities[index].get("kind")
            if kind == "player" or (
                kind == "goal"
                and sum(entity.get("kind") == "goal" for entity in entities) <= 1
            ):
                QMessageBox.information(
                    self,
                    "Required Object",
                    "Every level needs exactly one player and at least one goal.",
                )
                return
            del entities[index]
        else:
            after.setdefault("entities", []).append(operation["value"])
        labels = {
            "erase": "Erase cell",
            "path": "Create path",
            "event": "Create event region",
            "move_entity": "Move object",
            "delete_entity": "Delete object",
        }
        label = labels.get(operation["kind"], "Place object")
        self.undo_stack.push(_ProjectMutation(self, before, after, label))

    def _update_metadata(self):
        if not self.record:
            return
        before = copy.deepcopy(self.record.source)
        after = copy.deepcopy(before)
        after["title"] = self.title_edit.text().strip() or before["title"]
        after.setdefault("metadata", {})["author"] = self.author_edit.text().strip()
        if after != before:
            self.undo_stack.push(_ProjectMutation(self, before, after, "Edit metadata"))

    def _controls_changed(self, index):
        if not self.record or index < 0:
            return
        preset = self.controls.itemData(index)
        if preset == "custom":
            self._customize_controls()
            return
        save_settings(
            self.private_root,
            self.record.project_id,
            {"preset": preset},
        )
        self.status_changed.emit(f"Controls set to {self.controls.currentText()}")

    def _customize_controls(self):
        if not self.record:
            return
        current = load_settings(self.private_root, self.record.project_id)
        mapping = current["mapping"]
        dialog = QDialog(self)
        dialog.setWindowTitle("Custom Click-Wheel Controls")
        layout = QVBoxLayout(dialog)
        form = QFormLayout()
        choices = {}
        labels = {
            "select": "Select button",
            "play": "Play/Pause button",
            "previous": "Previous button",
            "next": "Next button",
        }
        action_labels = {
            "primary": "Primary action",
            "secondary": "Secondary action",
            "previous": "Previous / item left",
            "next": "Next / item right",
        }
        for button in PHYSICAL_BUTTONS:
            choice = QComboBox()
            for action in ACTIONS:
                choice.addItem(action_labels[action], action)
            choice.setCurrentIndex(max(0, choice.findData(mapping[button])))
            choices[button] = choice
            form.addRow(labels[button], choice)
        layout.addLayout(form)
        buttons = QDialogButtonBox(
            QDialogButtonBox.Cancel | QDialogButtonBox.Save
        )
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)
        if dialog.exec() != QDialog.Accepted:
            self.controls.blockSignals(True)
            self.controls.setCurrentIndex(
                max(0, self.controls.findData(current["preset"]))
            )
            self.controls.blockSignals(False)
            return
        selected = {
            button: choice.currentData()
            for button, choice in choices.items()
        }
        if len(set(selected.values())) != len(ACTIONS):
            QMessageBox.warning(
                self,
                "Custom Controls",
                "Assign each action to exactly one physical button.",
            )
            self.controls.blockSignals(True)
            self.controls.setCurrentIndex(
                max(0, self.controls.findData(current["preset"]))
            )
            self.controls.blockSignals(False)
            return
        save_settings(
            self.private_root,
            self.record.project_id,
            {"preset": "custom", "mapping": selected},
        )
        self.controls.blockSignals(True)
        self.controls.setCurrentIndex(self.controls.findData("custom"))
        self.controls.blockSignals(False)
        self.status_changed.emit("Saved custom click-wheel controls")

    def _save(self):
        if not self.record:
            return
        try:
            self.store.save(self.record)
        except ValueError as exc:
            QMessageBox.warning(self, "Project Validation", str(exc))
            return
        self.refresh()
        self.status_changed.emit(f"Saved {self.record.source['title']}")

    def _test(self):
        if not self.record:
            return
        try:
            output = self.runtime.test_project(self.record.source)
            dialog = MakerLitePreviewDialog(
                self.record.source,
                self.runtime,
                os.path.join(
                    self.private_root,
                    "kits",
                    self.record.source["kit_id"],
                    "art.mla",
                ),
                load_settings(self.private_root, self.record.project_id),
                self,
            )
        except (OSError, ValueError, MakerLiteRuntimeError) as exc:
            QMessageBox.warning(self, "Runtime Test Failed", str(exc))
            return
        self.validation.setText(
            "Deterministic preflight passed · " + output
        )
        self.status_changed.emit(
            "Live test uses the same fixed-point C core as the iPod"
        )
        dialog.exec()

    def _change_cover(self):
        if not self.record:
            return
        metadata = self.record.source.get("metadata", {})
        kit_directory = os.path.join(
            self.private_root, "kits", self.record.source["kit_id"]
        )
        restore_source = ""
        try:
            with open(
                os.path.join(kit_directory, "kit.mlk"),
                "r",
                encoding="utf-8",
            ) as source:
                source_cover_file = json.load(source).get("source_cover_file", "")
            candidate = os.path.join(
                kit_directory, os.path.basename(source_cover_file)
            )
            if source_cover_file and os.path.isfile(candidate):
                restore_source = candidate
        except (OSError, ValueError):
            pass
        dialog = MakerLiteCoverDialog(
            metadata.get("cover_source", ""),
            metadata.get("cover_fit", "contain"),
            metadata.get("cover_background", "#000000"),
            restore_source,
            self,
        )
        if dialog.exec() != MakerLiteCoverDialog.Accepted:
            return
        before = copy.deepcopy(self.record.source)
        after = copy.deepcopy(before)
        after.setdefault("metadata", {}).update(dialog.values())
        after["metadata"]["show_in_steam"] = True
        self.undo_stack.push(_ProjectMutation(self, before, after, "Change cover"))

    def _sync(self):
        if not self.record:
            return
        self._save()
        self.sync_requested.emit("simulator" if self.target.currentIndex() else "device")

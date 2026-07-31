"""RockPod comic/manga CBZ/CBR import and sync panel."""

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QAbstractItemView,
    QComboBox,
    QFrame,
    QHBoxLayout,
    QLabel,
    QInputDialog,
    QPushButton,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)


class ComicSyncWidget(QWidget):
    upload_requested = Signal()
    prepare_requested = Signal()
    sync_requested = Signal()
    remove_requested = Signal()
    refresh_requested = Signal()
    selection_changed = Signal()
    category_assign_requested = Signal(str)
    category_rename_requested = Signal(str, str)
    category_delete_requested = Signal(str)
    lock_requested = Signal(bool)
    reading_direction_requested = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("comic_sync")
        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        header = QFrame()
        header.setObjectName("theme_hub_header")
        header_layout = QVBoxLayout(header)
        title = QLabel("Comic / Manga Sync")
        title.setObjectName("section_title")
        description = QLabel(
            "Upload CBZ/CBR comics and manga, prepare them for the iPod "
            "screen, and sync them to Extras → Comics."
        )
        description.setWordWrap(True)
        self._paths = QLabel("")
        self._paths.setWordWrap(True)
        self._status = QLabel("No comics imported.")
        self._status.setObjectName("theme_hub_status")
        header_layout.addWidget(title)
        header_layout.addWidget(description)
        header_layout.addWidget(self._paths)
        header_layout.addWidget(self._status)
        layout.addWidget(header)

        profile_row = QHBoxLayout()
        profile_row.addWidget(QLabel("Preparation profile:"))
        self._profile = QComboBox()
        self._profile.addItem("Standard · 480×640", "standard")
        self._profile.addItem(
            "Fine Text · cropped 720×960", "fine-text"
        )
        self._profile.setToolTip(
            "Fine Text uses more storage but preserves smaller print and "
            "automatically crops light page margins."
        )
        profile_row.addWidget(self._profile)
        profile_row.addSpacing(16)
        profile_row.addWidget(QLabel("Reading direction:"))
        self._direction = QComboBox()
        self._direction.addItem("Left to right (Western)", "ltr")
        self._direction.addItem("Right to left (Manga)", "rtl")
        self._direction.setToolTip(
            "Right to left mirrors the wheel's page-turn direction to match "
            "how a printed manga volume is read back-to-front."
        )
        self._apply_direction = QPushButton("Apply to Selected")
        self._apply_direction.clicked.connect(
            lambda: self.reading_direction_requested.emit(
                self._direction.currentData()
            )
        )
        profile_row.addWidget(self._direction)
        profile_row.addWidget(self._apply_direction)
        profile_row.addStretch(1)
        layout.addLayout(profile_row)

        category_row = QHBoxLayout()
        category_row.addWidget(QLabel("Category:"))
        self._category = QComboBox()
        self._category.setEditable(True)
        self._category.setMinimumWidth(190)
        self._category.setToolTip(
            "Choose a category, or type a new name, then assign it to the "
            "selected comics."
        )
        self._category.currentTextChanged.connect(
            lambda _text: self._update_actions()
        )
        self._assign_category = QPushButton("Assign / Create")
        self._rename_category = QPushButton("Rename…")
        self._delete_category = QPushButton("Delete")
        self._lock = QPushButton("Lock")
        self._assign_category.clicked.connect(self._assign_current_category)
        self._rename_category.clicked.connect(self._rename_current_category)
        self._delete_category.clicked.connect(
            lambda: self.category_delete_requested.emit(
                self.current_category()
            )
        )
        self._lock.clicked.connect(self._toggle_selected_lock)
        category_row.addWidget(self._category)
        category_row.addWidget(self._assign_category)
        category_row.addWidget(self._rename_category)
        category_row.addWidget(self._delete_category)
        category_row.addSpacing(12)
        category_row.addWidget(self._lock)
        category_row.addStretch(1)
        layout.addLayout(category_row)

        self._tree = QTreeWidget()
        self._tree.setHeaderLabels(
            [
                "Comic",
                "Category",
                "Direction",
                "Lock",
                "Pages",
                "Profile",
                "Archive",
                "Prepared",
                "On Target",
            ]
        )
        self._tree.setRootIsDecorated(False)
        self._tree.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self._tree.itemSelectionChanged.connect(self.selection_changed)
        self._tree.setColumnWidth(0, 300)
        self._tree.setColumnWidth(1, 130)
        layout.addWidget(self._tree, 1)

        actions = QHBoxLayout()
        self._upload = QPushButton("Upload CBZ/CBR…")
        self._prepare = QPushButton("Prepare Selected")
        self._sync = QPushButton("Sync Selected")
        self._remove = QPushButton("Remove from Target")
        self._refresh = QPushButton("Refresh")
        self._upload.clicked.connect(self.upload_requested)
        self._prepare.clicked.connect(self.prepare_requested)
        self._sync.clicked.connect(self.sync_requested)
        self._remove.clicked.connect(self.remove_requested)
        self._refresh.clicked.connect(self.refresh_requested)
        for button in (
            self._upload,
            self._prepare,
            self._sync,
            self._remove,
            self._refresh,
        ):
            actions.addWidget(button)
        actions.addStretch(1)
        layout.addLayout(actions)
        self._update_actions()

    def set_issues(self, issues, selected_ids=None):
        selected_ids = set(selected_ids or [])
        self._tree.blockSignals(True)
        self._tree.clear()
        for issue in issues:
            direction = issue.get("reading_direction", "ltr")
            item = QTreeWidgetItem(
                [
                    issue["title"],
                    issue.get("category", "Uncategorized"),
                    "Manga (R→L)" if direction == "rtl" else "Western (L→R)",
                    "Locked" if issue.get("locked") else "Open",
                    str(issue["page_count"] or "—"),
                    issue.get("prepare_profile", "—"),
                    "Yes" if issue["archive_path"] else "No",
                    "Ready" if issue["prepared"] else "Needed",
                    "Yes" if issue["on_target"] else "No",
                ]
            )
            item.setData(0, Qt.UserRole, issue)
            self._tree.addTopLevelItem(item)
            if issue["id"] in selected_ids:
                item.setSelected(True)
        self._tree.blockSignals(False)
        self._update_actions()

    def set_state(self, library_path, target_path, message, busy=False):
        self._paths.setText(
            f"Library: {library_path}\nTarget: {target_path or 'Not connected'}"
        )
        self._status.setText(message)
        self._upload.setEnabled(not busy)
        self._refresh.setEnabled(not busy)
        self._tree.setEnabled(not busy)
        self._update_actions(busy)

    def set_categories(self, categories):
        current = self.current_category()
        self._category.blockSignals(True)
        self._category.clear()
        self._category.addItems(categories or ["Uncategorized"])
        index = self._category.findText(current)
        self._category.setCurrentIndex(max(0, index))
        self._category.blockSignals(False)
        self._update_actions()

    def selected_issues(self):
        return [
            item.data(0, Qt.UserRole)
            for item in self._tree.selectedItems()
        ]

    def set_message(self, message):
        self._status.setText(message)

    def preparation_profile(self):
        return str(self._profile.currentData() or "standard")

    def reading_direction(self):
        return str(self._direction.currentData() or "ltr")

    def current_category(self):
        return " ".join(self._category.currentText().split())

    def _assign_current_category(self):
        self.category_assign_requested.emit(self.current_category())

    def _rename_current_category(self):
        old_name = self.current_category()
        new_name, accepted = QInputDialog.getText(
            self, "Rename Comic Category", "New category name:",
            text=old_name,
        )
        if accepted:
            self.category_rename_requested.emit(old_name, str(new_name))

    def _toggle_selected_lock(self):
        selected = self.selected_issues()
        self.lock_requested.emit(
            not selected or not all(item.get("locked") for item in selected)
        )

    def _update_actions(self, busy=False):
        selected = self.selected_issues()
        self._prepare.setEnabled(
            not busy and any(item.get("archive_path") for item in selected)
        )
        self._sync.setEnabled(
            not busy and any(item.get("prepared") for item in selected)
        )
        self._remove.setEnabled(
            not busy and any(item.get("on_target") for item in selected)
        )
        has_selection = bool(selected)
        self._assign_category.setEnabled(not busy and has_selection)
        self._rename_category.setEnabled(
            not busy and self.current_category() != "Uncategorized"
        )
        self._delete_category.setEnabled(
            not busy and self.current_category() != "Uncategorized"
        )
        self._lock.setEnabled(not busy and has_selection)
        self._lock.setText(
            "Unlock"
            if selected and all(item.get("locked") for item in selected)
            else "Lock"
        )
        self._apply_direction.setEnabled(not busy and has_selection)

"""RockPod Store tab: search and download public-domain magazine PDFs from
Internet Archive's magazine_rack collection. Not a browser for copyrighted-
content mirror sites."""

from pathlib import Path

from PySide6.QtCore import QSize, Qt, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QAbstractItemView,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListView,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


COVER_TILE_SIZE = QSize(140, 210)
COVER_ICON_SIZE = QSize(120, 160)
PLACEHOLDER_COVER_PATH = (
    Path(__file__).resolve().parents[1]
    / "assets" / "icons" / "magazine-cover-placeholder.png"
)
_PLACEHOLDER_COVER = None


def _placeholder_cover():
    global _PLACEHOLDER_COVER
    if _PLACEHOLDER_COVER is None:
        pixmap = QPixmap(str(PLACEHOLDER_COVER_PATH))
        if pixmap.isNull():
            pixmap = QPixmap(COVER_ICON_SIZE)
            pixmap.fill(Qt.darkGray)
        _PLACEHOLDER_COVER = pixmap
    return _PLACEHOLDER_COVER


class MagazineStorePanel(QWidget):
    magazine_search_requested = Signal(str)
    magazine_download_requested = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("magazine_store_panel")
        self._results = {}
        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        header = QFrame()
        header.setObjectName("theme_hub_header")
        header_layout = QVBoxLayout(header)
        title = QLabel("Magazines")
        title.setObjectName("section_title")
        description = QLabel(
            "Search public-domain magazines on the Internet Archive and "
            "download them straight into your Magazine library."
        )
        description.setWordWrap(True)
        self._search_status = QLabel("")
        self._search_status.setObjectName("theme_hub_status")
        header_layout.addWidget(title)
        header_layout.addWidget(description)
        header_layout.addWidget(self._search_status)
        layout.addWidget(header)

        search_row = QHBoxLayout()
        self._query = QLineEdit()
        self._query.setPlaceholderText(
            "Search by title, publisher, or topic…"
        )
        self._query.returnPressed.connect(self._emit_search)
        self._search_button = QPushButton("Search")
        self._search_button.clicked.connect(self._emit_search)
        search_row.addWidget(self._query, 1)
        search_row.addWidget(self._search_button)
        layout.addLayout(search_row)

        self._list = QListWidget()
        self._list.setViewMode(QListView.IconMode)
        self._list.setResizeMode(QListView.Adjust)
        self._list.setMovement(QListView.Static)
        self._list.setWrapping(True)
        self._list.setUniformItemSizes(True)
        self._list.setSpacing(10)
        self._list.setIconSize(COVER_ICON_SIZE)
        self._list.setGridSize(COVER_TILE_SIZE)
        self._list.setWordWrap(True)
        self._list.setSelectionMode(QAbstractItemView.SingleSelection)
        self._list.itemSelectionChanged.connect(self._update_actions)
        layout.addWidget(self._list, 1)

        actions = QHBoxLayout()
        self._download = QPushButton("Download to Magazine Library")
        self._download.setEnabled(False)
        self._download.clicked.connect(self._emit_download)
        self._download_status = QLabel("")
        actions.addWidget(self._download)
        actions.addWidget(self._download_status, 1)
        layout.addLayout(actions)

    def _emit_search(self):
        text = self._query.text().strip()
        if text:
            self.magazine_search_requested.emit(text)

    def _emit_download(self):
        item = self._list.currentItem()
        if item is None:
            return
        identifier = item.data(Qt.UserRole)
        if identifier:
            self.magazine_download_requested.emit(identifier)

    def _update_actions(self):
        self._download.setEnabled(self._list.currentItem() is not None)

    def set_magazine_search_status(self, text, running=False):
        self._search_status.setText(text)
        self._search_button.setEnabled(not running)

    def set_magazine_results(self, results, header=""):
        self._results = {}
        self._list.clear()
        if header:
            self._search_status.setText(header)
        for result in results or []:
            identifier = str(result.get("identifier") or "")
            if not identifier:
                continue
            self._results[identifier] = result
            title = str(result.get("title") or identifier)
            creator = str(result.get("creator") or "")
            year = str(result.get("year") or "")
            subtitle = " · ".join(part for part in (creator, year) if part)
            label = f"{title}\n{subtitle}" if subtitle else title
            list_item = QListWidgetItem(label)
            list_item.setData(Qt.UserRole, identifier)
            list_item.setTextAlignment(Qt.AlignHCenter | Qt.AlignTop)
            cover_path = result.get("cover_path")
            pixmap = QPixmap(cover_path) if cover_path else None
            if not pixmap or pixmap.isNull():
                pixmap = _placeholder_cover()
            list_item.setIcon(pixmap.scaled(
                COVER_ICON_SIZE, Qt.KeepAspectRatio, Qt.SmoothTransformation
            ))
            self._list.addItem(list_item)
        self._update_actions()

    def magazine_result_for_identifier(self, identifier):
        return self._results.get(str(identifier or ""))

    def begin_magazine_download(self, identifier):
        self._download.setEnabled(False)
        self._download_status.setText(f"Downloading {identifier}…")

    def update_magazine_download(self, phase, progress):
        text = phase
        if progress:
            text = f"{phase}: {progress}"
        self._download_status.setText(text)

    def finish_magazine_download(self, output_path=None, success=True):
        self._download.setEnabled(self._list.currentItem() is not None)
        if success and output_path:
            self._download_status.setText(f"Downloaded: {output_path}")
        elif success:
            self._download_status.setText("Download complete.")
        else:
            self._download_status.setText("Download failed.")

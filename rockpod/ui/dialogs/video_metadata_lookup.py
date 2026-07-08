"""Dialog for searching and matching video metadata online (IMDb/TMDb/iTunes)."""

import os
from PySide6.QtCore import Qt, QSize, Signal, Slot
from PySide6.QtGui import QPixmap, QIcon
from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QFormLayout, QLabel, QLineEdit,
    QPushButton, QTableWidget, QTableWidgetItem, QHeaderView, QTextEdit,
    QDialogButtonBox, QWidget, QFrame, QSpinBox, QMessageBox
)

from services.online_video_metadata import VideoMetadataService
from services.artwork_manager import ArtworkManager

class VideoMetadataLookupDialog(QDialog):
    """Dialog for matching video metadata with OMDb/iTunes."""

    metadata_matched = Signal(dict, str)  # matched_metadata, artwork_url

    def __init__(self, track_row, metadata_service, artwork_manager, parent=None):
        super().__init__(parent)
        self._track = dict(track_row)
        self._service = metadata_service
        self._artwork_manager = artwork_manager
        self._results = []

        self.setWindowTitle("Match Video Metadata")
        self.setMinimumSize(780, 500)
        self.setModal(True)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(10)

        # Search Bar
        search_frame = QFrame()
        search_frame.setStyleSheet("background: #f7f7f7; border: 1px solid #e0e0e0; border-radius: 4px;")
        search_layout = QHBoxLayout(search_frame)
        search_layout.setContentsMargins(8, 6, 8, 6)

        self._query_edit = QLineEdit()
        initial_query = str(self._track.get("show_title") or self._track.get("title") or self._track.get("album") or "")
        self._query_edit.setText(initial_query)
        self._query_edit.setPlaceholderText("Search title...")
        
        self._year_spin = QSpinBox()
        self._year_spin.setRange(0, 9999)
        self._year_spin.setValue(int(self._track.get("year") or 0))
        self._year_spin.setSpecialValueText("Any Year")

        self._search_btn = QPushButton("Search")
        self._search_btn.clicked.connect(self._on_search)

        search_layout.addWidget(QLabel("Title:"))
        search_layout.addWidget(self._query_edit, 1)
        search_layout.addWidget(QLabel("Year:"))
        search_layout.addWidget(self._year_spin)
        search_layout.addWidget(self._search_btn)
        layout.addWidget(search_frame)

        # Main horizontal Split
        main_layout = QHBoxLayout()
        layout.addLayout(main_layout)

        # Left side: Results Table
        self._table = QTableWidget(0, 4)
        self._table.setHorizontalHeaderLabels(["Title", "Year", "Genre", "Type"])
        self._table.setSelectionBehavior(QTableWidget.SelectRows)
        self._table.setSelectionMode(QTableWidget.SingleSelection)
        self._table.verticalHeader().setVisible(False)
        self._table.setEditTriggers(QTableWidget.NoEditTriggers)
        self._table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self._table.itemSelectionChanged.connect(self._on_selection_changed)
        main_layout.addWidget(self._table, 3)

        # Right side: Detail Panel
        self._detail_panel = QFrame()
        self._detail_panel.setFrameShape(QFrame.StyledPanel)
        self._detail_panel.setMinimumWidth(300)
        self._detail_panel.setMaximumWidth(320)
        detail_layout = QVBoxLayout(self._detail_panel)
        detail_layout.setContentsMargins(8, 8, 8, 8)
        detail_layout.setSpacing(6)

        self._art_preview = QLabel("No Poster")
        self._art_preview.setFixedSize(140, 210)
        self._art_preview.setAlignment(Qt.AlignCenter)
        self._art_preview.setStyleSheet("background: #f0f0f0; border: 1px solid #cccccc; border-radius: 4px;")
        detail_layout.addWidget(self._art_preview, 0, Qt.AlignHCenter)

        self._plot_edit = QTextEdit()
        self._plot_edit.setReadOnly(True)
        self._plot_edit.setPlaceholderText("No details available.")
        self._plot_edit.setStyleSheet("background: transparent; border: none;")
        detail_layout.addWidget(self._plot_edit, 1)

        main_layout.addWidget(self._detail_panel, 2)

        # OK / Cancel Buttons
        self._button_box = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self._button_box.accepted.connect(self._on_accepted)
        self._button_box.rejected.connect(self.reject)
        self._button_box.button(QDialogButtonBox.Ok).setEnabled(False)
        layout.addWidget(self._button_box)

        # Initial trigger
        self._on_search()

    def _on_search(self):
        query = self._query_edit.text().strip()
        if not query:
            return
        year = self._year_spin.value() or None
        self._search_btn.setEnabled(False)
        self._search_btn.setText("Searching...")
        self._table.setRowCount(0)
        self._results = []
        self._plot_edit.clear()
        self._art_preview.setText("Searching...")
        self._button_box.button(QDialogButtonBox.Ok).setEnabled(False)

        try:
            kind = str(self._track.get("video_kind") or "movie")
            self._results = self._service.search(query, media_type=kind, year=year)
            
            # If nothing returned, try a general search or fallback
            if not self._results:
                alt_kind = "tv_show" if kind == "movie" else "movie"
                self._results = self._service.search(query, media_type=alt_kind, year=year)

            self._table.setRowCount(len(self._results))
            for row, item in enumerate(self._results):
                title_item = QTableWidgetItem(item.get("title", ""))
                year_item = QTableWidgetItem(str(item.get("year") or ""))
                genre_item = QTableWidgetItem(item.get("genre", ""))
                type_item = QTableWidgetItem(item.get("media_type", "movie").replace("_", " ").title())
                
                for col, it in enumerate([title_item, year_item, genre_item, type_item]):
                    self._table.setItem(row, col, it)

            if not self._results:
                self._art_preview.setText("No results found.")
            else:
                self._table.selectRow(0)

        except Exception as e:
            QMessageBox.critical(self, "Search Error", f"Metadata search failed: {str(e)}")
            self._art_preview.setText("Error searching.")
        finally:
            self._search_btn.setEnabled(True)
            self._search_btn.setText("Search")

    def _on_selection_changed(self):
        selected = self._table.selectedItems()
        if not selected:
            self._button_box.button(QDialogButtonBox.Ok).setEnabled(False)
            return
        row = selected[0].row()
        item = self._results[row]

        # Load plot/synopsis
        plot = str(item.get("plot_short") or item.get("plot_long") or "")
        self._plot_edit.setPlainText(plot)

        # Try to load/display artwork
        art_url = item.get("artwork_url") or ""
        self._art_preview.clear()
        if art_url:
            self._art_preview.setText("Loading Poster...")
            # We can download it using artwork manager
            threading.Thread(target=self._download_preview_art, args=(art_url,), daemon=True).start()
        else:
            self._art_preview.setText("No Poster Available")

        self._button_box.button(QDialogButtonBox.Ok).setEnabled(True)

    def _download_preview_art(self, url):
        try:
            data, mime = self._artwork_manager._lookup.download_image(url)
            if data:
                # Post display update back to main thread safely
                self.meta_art_downloaded(data)
        except Exception:
            pass

    @Slot(bytes)
    def meta_art_downloaded(self, data):
        px = QPixmap()
        px.loadFromData(data)
        if not px.isNull():
            self._art_preview.setPixmap(px.scaled(138, 208, Qt.KeepAspectRatio, Qt.SmoothTransformation))
        else:
            self._art_preview.setText("Failed to load poster")

    def _on_accepted(self):
        selected = self._table.selectedItems()
        if not selected:
            return
        row = selected[0].row()
        item = self._results[row]
        
        # If it's a TV episode, we also want to fetch the exact episode details if season and episode are specified
        if item.get("media_type") == "tv_show" and self._track.get("season_number") and self._track.get("episode_number"):
            try:
                ep_data = self._service.lookup_episode(
                    item["title"],
                    self._track["season_number"],
                    self._track["episode_number"]
                )
                if ep_data:
                    item.update({
                        "episode_title": ep_data.get("title") or "",
                        "plot_short": ep_data.get("plot_short") or item.get("plot_short") or "",
                        "plot_long": ep_data.get("plot_long") or item.get("plot_long") or "",
                        "release_date": ep_data.get("release_date") or item.get("release_date") or "",
                        "runtime_seconds": ep_data.get("runtime_seconds") or item.get("runtime_seconds") or 0,
                    })
            except Exception as e:
                logger.warning(f"Could not retrieve precise episode match: {str(e)}")

        self.metadata_matched.emit(item, item.get("artwork_url", ""))
        self.accept()

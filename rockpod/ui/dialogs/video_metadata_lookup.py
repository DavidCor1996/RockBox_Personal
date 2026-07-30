"""Dialog for searching and matching video metadata online (IMDb/TMDb/iTunes)."""

from PySide6.QtCore import Qt, Signal, Slot, QThreadPool
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QComboBox, QDialog, QVBoxLayout, QHBoxLayout, QFormLayout, QLabel,
    QLineEdit, QPushButton, QTableWidget, QTableWidgetItem, QHeaderView,
    QTextEdit, QDialogButtonBox, QWidget, QFrame, QMessageBox
)

from services.video_metadata_jobs import (
    PosterDownloadJob,
    VideoEpisodeLookupJob,
    VideoMetadataSearchJob,
)

class VideoMetadataLookupDialog(QDialog):
    """Dialog for matching video metadata with OMDb/iTunes."""

    metadata_matched = Signal(dict, str)  # matched_metadata, artwork_url

    def __init__(self, track_row, metadata_service, artwork_manager, parent=None,
                 is_group_match=False):
        super().__init__(parent)
        self._track = dict(track_row)
        self._service = metadata_service
        self._artwork_manager = artwork_manager
        self._results = []
        self._is_group_match = bool(is_group_match)
        self._jobs = set()
        self._selected_art_url = ""
        self._selected_art_data = b""
        self._pending_accept_item = None

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
        
        # A row's video_kind is only a filename guess, so the search must not
        # be locked to it - a movie filed as a show would otherwise never show
        # a single movie result. "All" is the default for that reason.
        self._type_combo = QComboBox()
        self._type_combo.addItem("All", "any")
        self._type_combo.addItem("Movie", "movie")
        self._type_combo.addItem("TV Show", "tv_show")
        self._type_combo.setCurrentIndex(0)
        self._type_combo.currentIndexChanged.connect(self._on_search)

        self._search_btn = QPushButton("Search")
        self._search_btn.clicked.connect(self._on_search)

        search_layout.addWidget(QLabel("Title:"))
        search_layout.addWidget(self._query_edit, 1)
        search_layout.addWidget(QLabel("Type:"))
        search_layout.addWidget(self._type_combo)
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

    def _on_search(self, *_args):
        query = self._query_edit.text().strip()
        if not query:
            return
        self._search_btn.setEnabled(False)
        self._search_btn.setText("Searching...")
        self._table.setRowCount(0)
        self._results = []
        self._plot_edit.clear()
        self._art_preview.setText("Searching...")
        self._button_box.button(QDialogButtonBox.Ok).setEnabled(False)

        kind = str(self._type_combo.currentData() or "any")
        # Manual matching searches by title across movies and TV. The result
        # year remains visible for disambiguation, but is never required input.
        job = VideoMetadataSearchJob(self._service, query, kind, year=None)
        job.signals.result.connect(self._on_search_results, Qt.QueuedConnection)
        job.signals.error.connect(self._on_search_error, Qt.QueuedConnection)
        self._start_job(job)

    @Slot(object)
    def _on_search_results(self, results):
        self._results = self._ordered_results(results)
        self._table.setRowCount(len(self._results))
        for row, item in enumerate(self._results):
            values = [
                QTableWidgetItem(item.get("title", "")),
                QTableWidgetItem(str(item.get("year") or "")),
                QTableWidgetItem(item.get("genre", "")),
                QTableWidgetItem(item.get("media_type", "movie").replace("_", " ").title()),
            ]
            for col, table_item in enumerate(values):
                self._table.setItem(row, col, table_item)
        if self._results:
            self._table.selectRow(0)
        else:
            self._art_preview.setText("No results found.")

    def _ordered_results(self, results):
        """Lead with the kind the library row claims to be, keeping the other
        kind reachable underneath so a misfiled title can still be corrected.
        """
        results = list(results or [])
        claimed = str(self._track.get("video_kind") or "").strip().casefold()
        if claimed not in {"movie", "show"}:
            return results
        prefer_show = claimed == "show"

        def rank(item):
            is_show = str(item.get("media_type") or "") in {
                "tv_show", "tv_episode"
            }
            return 0 if is_show == prefer_show else 1

        return sorted(results, key=rank)

    @Slot(str)
    def _on_search_error(self, message):
        QMessageBox.critical(self, "Search Error", f"Metadata search failed: {message}")
        self._art_preview.setText("Error searching.")

    def _start_job(self, job):
        self._jobs.add(job)
        job.signals.finished.connect(
            lambda current=job: self._finish_job(current),
            Qt.QueuedConnection,
        )
        QThreadPool.globalInstance().start(job)

    def _finish_job(self, job):
        self._jobs.discard(job)
        if isinstance(job, VideoMetadataSearchJob):
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
        self._selected_art_url = art_url
        self._selected_art_data = b""
        self._art_preview.clear()
        if art_url:
            self._art_preview.setText("Loading Poster...")
            job = PosterDownloadJob(art_url, self._artwork_manager._lookup.download_image)
            job.signals.result.connect(self._on_poster_downloaded, Qt.QueuedConnection)
            job.signals.error.connect(
                lambda message, url=art_url: self._on_poster_error(url, message),
                Qt.QueuedConnection,
            )
            self._start_job(job)
        else:
            self._art_preview.setText("No Poster Available")

        self._button_box.button(QDialogButtonBox.Ok).setEnabled(True)

    @Slot(object)
    def _on_poster_downloaded(self, result):
        url, data = result
        if url != self._selected_art_url or not data:
            return
        px = QPixmap()
        px.loadFromData(data)
        if not px.isNull():
            self._selected_art_data = bytes(data)
            self._art_preview.setPixmap(px.scaled(138, 208, Qt.KeepAspectRatio, Qt.SmoothTransformation))
        else:
            self._art_preview.setText("Failed to load poster")

    @Slot(str, str)
    def _on_poster_error(self, url, _message):
        if url == self._selected_art_url:
            self._art_preview.setText("Poster unavailable")

    def _on_accepted(self):
        selected = self._table.selectedItems()
        if not selected:
            return
        row = selected[0].row()
        item = self._results[row]
        
        # Exact episode lookup can involve another network request, so finish it
        # in the pool before accepting the dialog.
        if item.get("media_type") == "tv_show" and self._track.get("season_number") and self._track.get("episode_number"):
            self._pending_accept_item = dict(item)
            self._button_box.setEnabled(False)
            job = VideoEpisodeLookupJob(
                self._service,
                item["title"],
                self._track["season_number"],
                self._track["episode_number"],
            )
            job.signals.result.connect(self._on_episode_result, Qt.QueuedConnection)
            job.signals.error.connect(self._on_episode_error, Qt.QueuedConnection)
            self._start_job(job)
            return
        self._emit_match_and_accept(item)

    @Slot(object)
    def _on_episode_result(self, episode):
        item = dict(self._pending_accept_item or {})
        # The pending item is the series result, so keep its synopsis before
        # the episode summary replaces plot_short/plot_long. A show screen
        # needs the series blurb, not whichever episode was matched.
        item["show_plot"] = str(
            item.get("plot_long") or item.get("plot_short") or ""
        ).strip()
        if episode:
            item.update({
                "episode_title": episode.get("title") or "",
                "plot_short": episode.get("plot_short") or item.get("plot_short") or "",
                "plot_long": episode.get("plot_long") or item.get("plot_long") or "",
                "release_date": episode.get("release_date") or item.get("release_date") or "",
                "runtime_seconds": episode.get("runtime_seconds") or item.get("runtime_seconds") or 0,
            })
        self._emit_match_and_accept(item)

    @Slot(str)
    def _on_episode_error(self, _message):
        self._emit_match_and_accept(dict(self._pending_accept_item or {}))

    def _emit_match_and_accept(self, item):
        matched = dict(item)
        if self._selected_art_data:
            matched["_artwork_data"] = self._selected_art_data
        self.metadata_matched.emit(matched, item.get("artwork_url", ""))
        self.accept()

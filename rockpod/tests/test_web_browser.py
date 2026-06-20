import os
import zipfile

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPixmap
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication, QLabel

from scripts.youtube_movie_browse import _download_thumbnail, _thumbnail_url
from ui.web_browser import BrowserPanel, MovieStorePanel, extract_downloaded_archive, should_block_browser_url


def test_extract_downloaded_archive_unzips_zip_and_preserves_files(tmp_dir):
    library = os.path.join(tmp_dir, "library")
    archive = os.path.join(tmp_dir, "bundle.zip")
    with zipfile.ZipFile(archive, "w") as bundle:
        bundle.writestr("games/Tetris.gb", b"rom")
        bundle.writestr("games/LinksAwakening.gbc", b"rom2")

    extracted = extract_downloaded_archive(archive, library)

    assert len(extracted) == 2
    assert os.path.isfile(os.path.join(library, "games", "Tetris.gb"))
    assert os.path.isfile(os.path.join(library, "games", "LinksAwakening.gbc"))


def test_extract_downloaded_archive_blocks_zip_slip(tmp_dir):
    library = os.path.join(tmp_dir, "library")
    archive = os.path.join(tmp_dir, "bad.zip")
    with zipfile.ZipFile(archive, "w") as bundle:
        bundle.writestr("../escape.gb", b"bad")
        bundle.writestr("good/Inside.gb", b"ok")

    extracted = extract_downloaded_archive(archive, library)

    assert len(extracted) == 1
    assert os.path.isfile(os.path.join(library, "good", "Inside.gb"))
    assert not os.path.exists(os.path.join(tmp_dir, "escape.gb"))


def test_extract_downloaded_archive_ignores_non_zip(tmp_dir):
    library = os.path.join(tmp_dir, "library")
    archive = os.path.join(tmp_dir, "bundle.7z")
    with open(archive, "wb") as handle:
        handle.write(b"not-a-zip")

    extracted = extract_downloaded_archive(archive, library)

    assert extracted == []
    assert not os.path.exists(library)


def test_should_block_browser_url_blocks_common_ad_hosts():
    assert should_block_browser_url("https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js") is True
    assert should_block_browser_url("https://www.googleadservices.com/pagead/aclk?x=1") is True


def test_should_block_browser_url_blocks_tracking_paths():
    assert should_block_browser_url("https://example.com/assets/banner/top.js") is True
    assert should_block_browser_url("https://store.example.com/api/tracking/pixel.gif") is True


def test_should_block_browser_url_allows_normal_store_pages():
    assert should_block_browser_url("https://themes.rockbox.org/index.php") is False
    assert should_block_browser_url("https://example.com/store/downloads/theme.zip") is False


def test_browser_panel_shows_streamrip_download_rows(tmp_dir):
    QApplication.instance() or QApplication([])
    panel = BrowserPanel()
    track = os.path.join(tmp_dir, "Artist - Album", "01. Song.flac")

    panel.begin_store_import_download("https://tidal.com/album/123", tmp_dir)
    panel.update_store_import_downloads([track], running=True)

    assert panel._downloads.topLevelItem(0).text(1) == "Downloading"
    assert panel._downloads.topLevelItem(0).text(2) == "1 track"
    assert panel._downloads.topLevelItem(1).text(0) == "01. Song.flac"
    assert panel._downloads.topLevelItem(1).text(1) == "Downloading"

    panel.finish_store_import_downloads([track], success=True)

    assert panel._downloads.topLevelItem(0).text(1) == "Completed"
    assert panel._downloads.topLevelItem(1).text(1) == "Completed"
    assert panel._downloads.topLevelItem(1).text(2) == "100%"


def test_browser_panel_store_results_buy_button_emits_album_url(tmp_dir):
    QApplication.instance() or QApplication([])
    panel = BrowserPanel()
    emitted = []
    panel.store_result_import_requested.connect(lambda result, fmt: emitted.append((result, fmt)))

    panel.set_store_results(
        [
            {
                "source": "tidal",
                "media_type": "album",
                "id": "123",
                "title": "Album",
                "artist": "Artist",
                "date": "2026-01-01",
                "tracks": 10,
                "url": "https://tidal.com/album/123",
                "cover_path": "",
            }
        ]
    )

    tile = panel._store_results_grid.itemAt(0).widget()
    buy_button = tile.findChildren(type(panel._store_import_btn))[-1]
    buy_button.click()

    assert emitted[0][0]["url"] == "https://tidal.com/album/123"
    assert emitted[0][0]["title"] == "Album"
    assert emitted[0][1] == "flac"


def test_browser_panel_store_result_tile_opens_album_detail_page(tmp_dir):
    app = QApplication.instance() or QApplication([])
    panel = BrowserPanel()
    emitted = []
    details_requested = []
    panel.store_result_import_requested.connect(lambda result, fmt: emitted.append((result, fmt)))
    panel.store_album_details_requested.connect(lambda result: details_requested.append(result))
    panel.set_store_results(
        [
            {
                "source": "tidal",
                "media_type": "album",
                "id": "123",
                "title": "Detail Album",
                "artist": "Detail Artist",
                "date": "2026-01-01",
                "tracks": 12,
                "url": "https://tidal.com/album/123",
                "cover_path": "",
            }
        ]
    )
    panel.show()
    app.processEvents()

    tile = panel._store_results_grid.itemAt(0).widget()
    QTest.mouseClick(tile, Qt.LeftButton)

    assert panel._store_results_header.text() == "Album Details"
    assert details_requested[0]["id"] == "123"
    detail = panel._store_results_grid.itemAt(0).widget()
    assert detail.objectName() == "itunes_store_album_detail"
    assert any(label.text() == "Detail Album" for label in detail.findChildren(type(panel._store_status)))
    assert any(label.text() == "Detail Artist" for label in detail.findChildren(type(panel._store_status)))
    assert any(label.text() == "Loading track list..." for label in detail.findChildren(type(panel._store_status)))

    buy_button = next(button for button in detail.findChildren(type(panel._store_import_btn)) if button.text() == "Buy Album")
    buy_button.click()
    assert emitted[0][0]["url"] == "https://tidal.com/album/123"
    assert emitted[0][1] == "flac"

    back_button = next(button for button in detail.findChildren(type(panel._store_import_btn)) if button.text() == "Back")
    back_button.click()
    assert panel._store_results_header.text() == "New Music"
    assert panel._store_results_grid.itemAt(0).widget().objectName() == "itunes_store_album_tile"


def test_browser_panel_album_details_show_ordered_tracks_and_single_buy_buttons():
    QApplication.instance() or QApplication([])
    panel = BrowserPanel()
    emitted = []
    panel.store_result_import_requested.connect(lambda result, fmt: emitted.append((result, fmt)))

    panel.set_store_results(
        [
            {
                "source": "tidal",
                "media_type": "album",
                "id": "123",
                "title": "Album",
                "artist": "Artist",
                "url": "https://tidal.com/album/123",
            }
        ]
    )
    panel.update_store_result_details(
        {
            "source": "tidal",
            "media_type": "album",
            "id": "123",
            "title": "Album",
            "artist": "Artist",
            "url": "https://tidal.com/album/123",
            "track_items": [
                {
                    "source": "tidal",
                    "media_type": "track",
                    "id": "2",
                    "title": "Second Song",
                    "artist": "Artist",
                    "track_number": 2,
                    "disc_number": 1,
                    "url": "https://tidal.com/track/2",
                },
                {
                    "source": "tidal",
                    "media_type": "track",
                    "id": "1",
                    "title": "First Song",
                    "artist": "Artist",
                    "track_number": 1,
                    "disc_number": 1,
                    "url": "https://tidal.com/track/1",
                },
            ],
        }
    )
    panel.show_store_result_details(panel._store_results[0])

    detail = panel._store_results_grid.itemAt(0).widget()
    labels = [label.text() for label in detail.findChildren(type(panel._store_status))]
    assert labels.index("First Song") < labels.index("Second Song")

    buy_button = next(button for button in detail.findChildren(type(panel._store_import_btn)) if button.text() == "Buy")
    buy_button.click()
    assert emitted[0][0]["media_type"] == "track"
    assert emitted[0][0]["url"] == "https://tidal.com/track/1"


def test_browser_panel_album_details_preview_button_emits_album_preview_queue():
    QApplication.instance() or QApplication([])
    panel = BrowserPanel()
    emitted = []
    panel.store_preview_requested.connect(lambda track, queue: emitted.append((track, queue)))

    result = {
        "source": "tidal",
        "media_type": "album",
        "id": "123",
        "title": "Album",
        "artist": "Artist",
        "cover_path": "",
        "url": "https://tidal.com/album/123",
        "track_items": [
            {
                "source": "tidal",
                "media_type": "track",
                "id": "1",
                "title": "First Song",
                "track_number": 1,
                "url": "https://tidal.com/track/1",
                "preview_url": "https://example.com/first.mp3",
            },
            {
                "source": "tidal",
                "media_type": "track",
                "id": "2",
                "title": "Second Song",
                "track_number": 2,
                "url": "https://tidal.com/track/2",
            },
        ],
    }
    panel.show_store_result_details(result)

    detail = panel._store_results_grid.itemAt(0).widget()
    preview_button = next(button for button in detail.findChildren(type(panel._store_import_btn)) if button.text() == "Preview")
    disabled_preview = next(button for button in detail.findChildren(type(panel._store_import_btn)) if button.text() == "No Preview")

    assert preview_button.isEnabled() is True
    assert disabled_preview.isEnabled() is False

    preview_button.click()
    assert emitted[0][0]["title"] == "First Song"
    assert emitted[0][0]["stream_url"] == "https://example.com/first.mp3"
    assert emitted[0][0]["album"] == "Album"
    assert len(emitted[0][1]) == 1
    assert emitted[0][1][0]["preview_url"] == "https://example.com/first.mp3"


def test_browser_panel_browser_only_mode_hides_music_store_controls():
    QApplication.instance() or QApplication([])
    panel = BrowserPanel(music_store=False, title="iPod Games", web_title="iPod Games Browser")

    assert panel._store_search_bar.isHidden()
    assert panel._store_import_bar.isHidden()
    assert panel._downloads.isHidden()
    assert not panel._url_edit.isHidden()
    assert getattr(panel, "_store_results_grid", None) is None
    assert panel._web_frame is not None


def test_browser_panel_browser_only_mode_can_show_downloads():
    QApplication.instance() or QApplication([])
    panel = BrowserPanel(
        music_store=False,
        title="iPod Games",
        web_title="iPod Games Browser",
        show_downloads=True,
    )

    assert panel._store_search_bar.isHidden()
    assert panel._store_import_bar.isHidden()
    assert not panel._downloads.isHidden()
    assert not panel._url_edit.isHidden()
    assert panel._web_frame is not None


def test_browser_panel_music_store_does_not_embed_web_store():
    QApplication.instance() or QApplication([])
    panel = BrowserPanel()

    assert panel._web_frame is None
    assert panel._web is None
    assert panel._url_edit.isHidden()
    assert panel._open_external_btn.isHidden()
    assert not panel._store_import_bar.isHidden()
    assert "Spotify playlist URL" in panel._store_url_edit.placeholderText()


def test_browser_panel_music_home_tabs_emit_section_requests():
    QApplication.instance() or QApplication([])
    panel = BrowserPanel()
    requested = []
    panel.store_home_tab_requested.connect(requested.append)

    panel._store_home_tab_buttons["rock"].click()

    assert requested == ["rock"]
    assert panel._store_home_tab == "rock"
    assert panel._store_home_tab_buttons["rock"].isChecked()
    assert panel._store_home_tab_buttons["featured"].isChecked() is False


def test_browser_panel_store_home_results_update_homepage_chrome(tmp_dir):
    QApplication.instance() or QApplication([])
    panel = BrowserPanel()
    emitted = []
    panel.store_result_import_requested.connect(lambda result, fmt: emitted.append((result, fmt)))

    panel.set_store_home_results(
        [
            {
                "source": "tidal",
                "media_type": "album",
                "id": "123",
                "title": "Popular Album",
                "artist": "Popular Artist",
                "date": "2026-06-01",
                "tracks": 11,
                "url": "https://tidal.com/album/123",
                "cover_path": "",
                "section": "Popular albums",
            }
        ],
        "top_albums",
    )

    assert panel._store_results_header.text() == "Top Albums on TIDAL"
    assert panel._store_hero_kicker.text() == "TIDAL Store - Top Albums"
    assert panel._store_hero_headline.text() == "Popular albums"
    assert panel._store_chart_buttons[0].text() == "1. Popular Album"

    buy_button = panel._store_results_grid.itemAt(0).widget().findChildren(type(panel._store_import_btn))[-1]
    buy_button.click()
    assert emitted[0][0]["url"] == "https://tidal.com/album/123"


def test_browser_panel_store_result_owned_album_disables_buy_button():
    QApplication.instance() or QApplication([])
    panel = BrowserPanel()

    panel.set_store_results(
        [
            {
                "source": "tidal",
                "media_type": "album",
                "id": "123",
                "title": "Owned Album",
                "artist": "Owned Artist",
                "url": "https://tidal.com/album/123",
                "owned": True,
            }
        ]
    )

    tile = panel._store_results_grid.itemAt(0).widget()
    buy_button = tile.findChildren(type(panel._store_import_btn))[-1]
    assert buy_button.text() == "Owned"
    assert buy_button.isEnabled() is False

    panel.show_store_result_details(panel._store_results[0])
    detail = panel._store_results_grid.itemAt(0).widget()
    owned_button = next(button for button in detail.findChildren(type(panel._store_import_btn)) if button.text() == "Owned")
    assert owned_button.isEnabled() is False


def test_movie_store_panel_emits_youtube_url_and_tracks_status():
    QApplication.instance() or QApplication([])
    panel = MovieStorePanel()
    emitted = []
    browsed = []
    panel.movie_import_requested.connect(emitted.append)
    panel.movie_browse_requested.connect(browsed.append)

    panel._movie_browse_edit.setText("public domain")
    panel._movie_browse_btn.click()
    panel.set_movie_results(
        [
            {
                "title": "Public Domain Movie",
                "uploader": "Archive Channel",
                "duration_text": "1:20:00",
                "url": "https://www.youtube.com/watch?v=movie",
            }
        ],
        "Browse: public domain",
    )
    buy_button = panel._movie_results_grid.itemAt(0).widget().findChildren(type(panel._movie_import_btn))[-1]
    buy_button.click()
    panel._movie_url_edit.setText("https://www.youtube.com/watch?v=abc")
    panel._movie_import_btn.click()
    panel.begin_movie_import("https://www.youtube.com/watch?v=abc", "/tmp/videos")
    panel.update_movie_import("Converting", "ffmpeg")
    panel.finish_movie_import("/tmp/videos/Movie.mpg", success=True)

    assert browsed == ["public domain"]
    assert emitted == ["https://www.youtube.com/watch?v=movie", "https://www.youtube.com/watch?v=abc"]
    assert panel._movie_results_header.text() == "Browse: public domain"
    assert panel._movie_downloads.topLevelItem(0).text(0) == "Movie.mpg"
    assert panel._movie_downloads.topLevelItem(0).text(1) == "Completed"


def test_movie_store_panel_shows_movie_details_and_imports_selected_result():
    QApplication.instance() or QApplication([])
    panel = MovieStorePanel()
    emitted = []
    panel.movie_import_requested.connect(emitted.append)
    panel.set_movie_results(
        [
            {
                "title": "Feature Film",
                "uploader": "Archive Channel",
                "duration_text": "1:20:00",
                "url": "https://www.youtube.com/watch?v=feature",
                "id": "feature",
            }
        ],
        "Browse: archive",
    )

    panel.show_movie_result_details(panel._movie_results[0])
    detail = panel._movie_results_grid.itemAt(0).widget()
    assert panel._movie_results_header.text() == "Movie Details"
    assert any(label.text() == "Feature Film" for label in detail.findChildren(QLabel))

    add_button = next(button for button in detail.findChildren(type(panel._movie_import_btn)) if button.text() == "Add Movie")
    add_button.click()
    assert emitted == ["https://www.youtube.com/watch?v=feature"]

    back_button = next(button for button in detail.findChildren(type(panel._movie_import_btn)) if button.text() == "Back")
    back_button.click()
    assert panel._movie_results_header.text() == "Browse: archive"
    assert panel._movie_results_grid.count() == 1


def test_movie_store_panel_uses_result_thumbnail_path(tmp_dir):
    QApplication.instance() or QApplication([])
    thumb_path = os.path.join(tmp_dir, "movie-thumb.png")
    pixmap = QPixmap(32, 18)
    pixmap.fill(QColor("#336699"))
    assert pixmap.save(thumb_path)

    panel = MovieStorePanel()
    panel.set_movie_results(
        [
            {
                "title": "Movie With Thumbnail",
                "uploader": "Archive Channel",
                "url": "https://www.youtube.com/watch?v=movie",
                "thumbnail": "https://example.test/remote.jpg",
                "thumbnail_path": thumb_path,
            }
        ]
    )

    tile = panel._movie_results_grid.itemAt(0).widget()
    thumbnail = tile.findChild(type(panel._movie_results_header), "itunes_store_movie_thumbnail")
    assert thumbnail is not None
    assert thumbnail.pixmap() is not None
    assert not thumbnail.pixmap().isNull()


def test_youtube_movie_browse_builds_fallback_thumbnail_url():
    assert _thumbnail_url({"id": "abc123"}) == "https://i.ytimg.com/vi/abc123/hqdefault.jpg"
    assert _thumbnail_url({"thumbnail": "https://example.test/thumb.jpg"}) == "https://example.test/thumb.jpg"


def test_youtube_movie_browse_downloads_thumbnail_to_cache(tmp_dir, monkeypatch):
    class FakeResponse:
        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

        def read(self):
            return b"thumbnail-bytes"

    monkeypatch.setattr("scripts.youtube_movie_browse.urlopen", lambda request, timeout=15: FakeResponse())

    path = _download_thumbnail("https://example.test/thumb.jpg", tmp_dir, "video-id")

    assert path.startswith(tmp_dir)
    assert path.endswith(".jpg")
    assert open(path, "rb").read() == b"thumbnail-bytes"

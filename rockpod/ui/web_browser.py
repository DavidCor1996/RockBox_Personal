"""Generic store panel with optional embedded Qt WebEngine view."""

from __future__ import annotations

import os
import zipfile
from urllib.parse import urlparse

from PySide6.QtCore import QUrl, Signal
from PySide6.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

def _load_webengine_view():
    if os.environ.get("QT_QPA_PLATFORM", "").lower() == "offscreen":
        return None
    try:  # pragma: no cover - optional dependency
        from PySide6.QtWebEngineWidgets import QWebEngineView  # type: ignore
        return QWebEngineView
    except Exception:
        return None


_AD_HOST_KEYWORDS = (
    "doubleclick",
    "googlesyndication",
    "googleadservices",
    "adservice",
    "adnxs",
    "adsystem",
    "taboola",
    "outbrain",
    "zedo",
    "ads.",
    ".ads",
    "tracking",
    "analytics",
    "pixel.",
    "scorecardresearch",
    "quantserve",
    "amazon-adsystem",
)

_AD_PATH_KEYWORDS = (
    "/ads",
    "/ads/",
    "doubleclick",
    "adservice",
    "banner",
    "prebid",
    "googlesyndication",
    "taboola",
    "outbrain",
    "analytics",
    "tracking",
    "pixel",
)


def should_block_browser_url(url):
    parsed = urlparse(str(url or ""))
    host = (parsed.netloc or "").lower()
    path = (parsed.path or "").lower()
    query = (parsed.query or "").lower()
    if not host:
        return False
    if any(token in host for token in _AD_HOST_KEYWORDS):
        return True
    haystack = f"{path}?{query}"
    return any(token in haystack for token in _AD_PATH_KEYWORDS)


def _install_adblock(profile):
    try:  # pragma: no cover - optional dependency
        from PySide6.QtWebEngineCore import QWebEngineUrlRequestInterceptor  # type: ignore
    except Exception:
        return None

    class _AdBlockInterceptor(QWebEngineUrlRequestInterceptor):
        def interceptRequest(self, info):  # noqa: N802 - Qt API
            if should_block_browser_url(info.requestUrl().toString()):
                info.block(True)

    interceptor = _AdBlockInterceptor(profile)
    profile.setUrlRequestInterceptor(interceptor)
    return interceptor


def _is_supported_archive(path):
    return str(path or "").lower().endswith(".zip")


def extract_downloaded_archive(archive_path, destination_dir):
    archive_path = os.path.abspath(str(archive_path))
    destination_dir = os.path.abspath(str(destination_dir))
    if not _is_supported_archive(archive_path):
        return []
    extracted = []
    os.makedirs(destination_dir, exist_ok=True)
    with zipfile.ZipFile(archive_path, "r") as bundle:
        for member in bundle.infolist():
            name = member.filename or ""
            if not name or member.is_dir():
                continue
            normalized = os.path.normpath(name).lstrip(os.sep)
            if normalized.startswith(".."):
                continue
            target_path = os.path.abspath(os.path.join(destination_dir, normalized))
            if os.path.commonpath([destination_dir, target_path]) != destination_dir:
                continue
            os.makedirs(os.path.dirname(target_path), exist_ok=True)
            with bundle.open(member, "r") as source, open(target_path, "wb") as dest:
                dest.write(source.read())
            extracted.append(target_path)
    return extracted


class BrowserPanel(QWidget):
    open_external_requested = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("browser_panel")
        self._home_url = ""
        self._download_dir = ""
        self._download_items = {}
        self._finalized_downloads = set()
        self._auto_accept_cookies = True
        self._cookie_accept_attempted_hosts = set()

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        chrome = QFrame()
        chrome.setObjectName("theme_hub_header")
        chrome_layout = QHBoxLayout(chrome)
        chrome_layout.setContentsMargins(10, 8, 10, 8)
        chrome_layout.setSpacing(6)

        self._url_edit = QLineEdit()
        self._url_edit.returnPressed.connect(self._go_to_entered_url)
        self._home_btn = QPushButton("Home")
        self._back_btn = QPushButton("Back")
        self._forward_btn = QPushButton("Forward")
        self._reload_btn = QPushButton("Reload")
        self._open_external_btn = QPushButton("Open in Browser")
        self._home_btn.clicked.connect(self.go_home)
        self._back_btn.clicked.connect(self.go_back)
        self._forward_btn.clicked.connect(self.go_forward)
        self._reload_btn.clicked.connect(self.reload)
        self._open_external_btn.clicked.connect(self._emit_open_external)

        chrome_layout.addWidget(self._back_btn)
        chrome_layout.addWidget(self._forward_btn)
        chrome_layout.addWidget(self._reload_btn)
        chrome_layout.addWidget(self._home_btn)
        chrome_layout.addWidget(self._url_edit, 1)
        chrome_layout.addWidget(self._open_external_btn)
        layout.addWidget(chrome)

        webengine_view_cls = _load_webengine_view()
        if webengine_view_cls is not None:
            self._notice = None
            self._web = webengine_view_cls()
            self._web.urlChanged.connect(self._on_url_changed)
            self._web.loadFinished.connect(self._on_load_finished)
            profile = self._web.page().profile()
            self._adblock = _install_adblock(profile)
            if hasattr(profile, "downloadRequested"):
                profile.downloadRequested.connect(self._on_download_requested)
            layout.addWidget(self._web, 1)
        else:
            self._web = None
            self._adblock = None
            self._notice = QLabel(
                "Embedded store view unavailable in this build.\n"
                "Use Open in Browser to open the configured URL in your system browser."
            )
            self._notice.setWordWrap(True)
            self._notice.setObjectName("theme_hub_status")
            layout.addWidget(self._notice, 1)

        self._downloads = QTreeWidget()
        self._downloads.setObjectName("theme_hub_assets")
        self._downloads.setHeaderLabels(["Download", "Status", "Progress", "Folder"])
        self._downloads.setRootIsDecorated(False)
        self._downloads.setMinimumHeight(120)
        layout.addWidget(self._downloads)

    def set_home_url(self, url):
        self._home_url = self._normalize_url(url)
        self._url_edit.setText(self._home_url)
        if self._web is not None and not self._web.url().isValid():
            self._web.setUrl(QUrl(self._home_url))

    def set_download_directory(self, path):
        self._download_dir = os.path.abspath(path) if path else ""

    def set_store_context(self, home_url, download_dir):
        self.set_home_url(home_url)
        self.set_download_directory(download_dir)

    def set_auto_accept_cookies(self, enabled):
        self._auto_accept_cookies = bool(enabled)

    def go_home(self):
        if not self._home_url:
            return
        if self._web is not None:
            self._web.setUrl(QUrl(self._home_url))
        self._url_edit.setText(self._home_url)

    def go_back(self):
        if self._web is not None:
            self._web.back()

    def go_forward(self):
        if self._web is not None:
            self._web.forward()

    def reload(self):
        if self._web is not None:
            self._web.reload()

    def current_url(self):
        if self._web is not None and self._web.url().isValid():
            return self._web.url().toString()
        return self._normalize_url(self._url_edit.text())

    def _go_to_entered_url(self):
        url = self._normalize_url(self._url_edit.text())
        self._url_edit.setText(url)
        if self._web is not None:
            self._web.setUrl(QUrl(url))

    def _on_url_changed(self, url):
        self._url_edit.setText(url.toString())

    def _on_load_finished(self, ok):  # pragma: no cover - depends on Qt WebEngine runtime
        if not ok or self._web is None or not self._auto_accept_cookies:
            return
        current = self.current_url()
        if not current or not self._home_url:
            return
        current_host = urlparse(current).netloc.lower()
        home_host = urlparse(self._home_url).netloc.lower()
        if not current_host or current_host != home_host or current_host in self._cookie_accept_attempted_hosts:
            return
        self._cookie_accept_attempted_hosts.add(current_host)
        self._web.page().runJavaScript(_COOKIE_CONSENT_SCRIPT)

    def _emit_open_external(self):
        self.open_external_requested.emit(self.current_url() or self._home_url)

    def _on_download_requested(self, request):  # pragma: no cover - depends on Qt WebEngine runtime
        if not self._download_dir:
            return
        os.makedirs(self._download_dir, exist_ok=True)
        suggested = request.downloadFileName() if hasattr(request, "downloadFileName") else ""
        if not suggested and hasattr(request, "suggestedFileName"):
            suggested = request.suggestedFileName()
        if hasattr(request, "setDownloadDirectory"):
            request.setDownloadDirectory(self._download_dir)
        if hasattr(request, "setDownloadFileName") and suggested:
            request.setDownloadFileName(suggested)

        item = QTreeWidgetItem(
            [
                suggested or "download",
                "Starting",
                "0%",
                self._download_dir,
            ]
        )
        self._downloads.insertTopLevelItem(0, item)
        self._download_items[id(request)] = item
        if hasattr(request, "receivedBytesChanged"):
            request.receivedBytesChanged.connect(lambda r=request: self._update_download_item(r))
        if hasattr(request, "totalBytesChanged"):
            request.totalBytesChanged.connect(lambda r=request: self._update_download_item(r))
        if hasattr(request, "stateChanged"):
            request.stateChanged.connect(lambda _state, r=request: self._update_download_item(r))
        if hasattr(request, "isFinishedChanged"):
            request.isFinishedChanged.connect(lambda r=request: self._update_download_item(r))
        request.accept()
        self._update_download_item(request)

    def _update_download_item(self, request):  # pragma: no cover - depends on Qt WebEngine runtime
        item = self._download_items.get(id(request))
        if item is None:
            return
        received = request.receivedBytes() if hasattr(request, "receivedBytes") else 0
        total = request.totalBytes() if hasattr(request, "totalBytes") else 0
        if total and total > 0:
            progress = f"{int((received / total) * 100)}%"
        elif received > 0:
            progress = f"{received} bytes"
        else:
            progress = "0%"
        state_text = "Downloading"
        if hasattr(request, "state"):
            state = request.state()
            state_name = getattr(state, "name", str(state))
            if "Completed" in state_name:
                state_text = "Completed"
            elif "Cancelled" in state_name:
                state_text = "Cancelled"
            elif "Interrupted" in state_name:
                state_text = "Failed"
        item.setText(1, state_text)
        item.setText(2, progress)
        item.setText(3, self._download_dir)
        if state_text == "Completed":
            self._finalize_download(request, item)
        for column in range(4):
            self._downloads.resizeColumnToContents(column)

    def _finalize_download(self, request, item):  # pragma: no cover - depends on Qt WebEngine runtime
        request_id = id(request)
        if request_id in self._finalized_downloads:
            return
        self._finalized_downloads.add(request_id)
        file_name = item.text(0)
        archive_path = os.path.join(self._download_dir, file_name)
        if not _is_supported_archive(archive_path) or not os.path.exists(archive_path):
            return
        item.setText(1, "Extracting")
        try:
            extracted = extract_downloaded_archive(archive_path, self._download_dir)
        except Exception:
            item.setText(1, "Extract Failed")
            return
        if extracted:
            os.remove(archive_path)
            item.setText(1, f"Extracted ({len(extracted)})")
            item.setText(2, "100%")
        else:
            item.setText(1, "Completed")

    @staticmethod
    def _normalize_url(url):
        text = str(url or "").strip()
        if not text:
            return "https://www.rockbox.org/"
        if "://" not in text:
            return f"https://{text}"
        return text


_COOKIE_CONSENT_SCRIPT = r"""
(function() {
  const acceptWords = [
    'accept', 'agree', 'allow all', 'accept all', 'ok', 'got it',
    'i agree', 'yes, i agree', 'accept cookies', 'allow cookies'
  ];
  const selectors = [
    'button', 'a', '[role="button"]', 'input[type="button"]', 'input[type="submit"]'
  ];
  function isVisible(el) {
    const rect = el.getBoundingClientRect();
    const style = window.getComputedStyle(el);
    return rect.width > 0 && rect.height > 0 && style.visibility !== 'hidden' && style.display !== 'none';
  }
  function textFor(el) {
    return ((el.innerText || el.value || el.getAttribute('aria-label') || '') + '').trim().toLowerCase();
  }
  for (const selector of selectors) {
    const nodes = document.querySelectorAll(selector);
    for (const node of nodes) {
      const text = textFor(node);
      if (!text || !isVisible(node)) continue;
      if (acceptWords.some(word => text === word || text.includes(word))) {
        node.click();
        return 'clicked';
      }
    }
  }
  return 'no-match';
})();
"""

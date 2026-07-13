"""Small background jobs for video metadata and poster network operations."""

import logging

from PySide6.QtCore import QObject, QRunnable, Signal, Slot

logger = logging.getLogger(__name__)


class JobSignals(QObject):
    result = Signal(object)
    error = Signal(str)
    finished = Signal()


class VideoMetadataSearchJob(QRunnable):
    def __init__(self, service, query, media_type, year=None):
        super().__init__()
        self.signals = JobSignals()
        self._service = service
        self._query = query
        self._media_type = media_type
        self._year = year

    @Slot()
    def run(self):
        try:
            kinds = [self._media_type]
            alternate = "tv_show" if self._media_type == "movie" else "movie"
            if alternate not in kinds:
                kinds.append(alternate)

            results = []
            seen = set()
            failures = []
            for kind in kinds:
                try:
                    matches = self._service.search(
                        self._query,
                        media_type=kind,
                        year=self._year,
                    )
                except Exception as exc:
                    failures.append(exc)
                    continue
                for item in matches or []:
                    key = (
                        str(item.get("media_type") or kind),
                        str(item.get("title") or "").casefold(),
                        item.get("year"),
                        str(item.get("provider_id") or item.get("imdb_id") or ""),
                    )
                    if key in seen:
                        continue
                    seen.add(key)
                    results.append(item)

            if not results and failures:
                raise failures[0]
            self.signals.result.emit(results)
        except Exception as exc:
            logger.exception("Video metadata search failed")
            self.signals.error.emit(str(exc))
        finally:
            self.signals.finished.emit()


class VideoEpisodeLookupJob(QRunnable):
    def __init__(self, service, show_title, season_number, episode_number):
        super().__init__()
        self.signals = JobSignals()
        self._service = service
        self._show_title = show_title
        self._season_number = season_number
        self._episode_number = episode_number

    @Slot()
    def run(self):
        try:
            result = self._service.lookup_episode(
                self._show_title,
                self._season_number,
                self._episode_number,
            )
            self.signals.result.emit(dict(result or {}))
        except Exception as exc:
            logger.warning("Could not retrieve precise episode match: %s", exc)
            self.signals.error.emit(str(exc))
        finally:
            self.signals.finished.emit()


class PosterDownloadJob(QRunnable):
    def __init__(self, url, download_image):
        super().__init__()
        self.signals = JobSignals()
        self._url = url
        self._download_image = download_image

    @Slot()
    def run(self):
        try:
            data, _mime = self._download_image(self._url)
            self.signals.result.emit((self._url, data or b""))
        except Exception as exc:
            logger.debug("Poster preview download failed: %s", exc)
            self.signals.error.emit(str(exc))
        finally:
            self.signals.finished.emit()


class YoutubeImportMetadataJob(QRunnable):
    def __init__(self, track_data, pending_item, config):
        super().__init__()
        self.signals = JobSignals()
        self._track_data = dict(track_data)
        self._pending_item = dict(pending_item)
        self._config = config

    @Slot()
    def run(self):
        try:
            from services.online_video_metadata import VideoMetadataService
            from services.youtube_import_metadata import build_youtube_import_metadata_payload

            service = VideoMetadataService(config=self._config)
            payload = build_youtube_import_metadata_payload(
                self._track_data,
                self._pending_item,
                service,
            )
            self.signals.result.emit(
                {
                    "track": self._track_data,
                    "pending": self._pending_item,
                    "payload": payload,
                }
            )
        except Exception as exc:
            logger.exception("YouTube import metadata lookup failed")
            self.signals.error.emit(str(exc))
        finally:
            self.signals.finished.emit()

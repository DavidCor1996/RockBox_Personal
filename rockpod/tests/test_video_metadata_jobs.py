from services.video_metadata_jobs import VideoMetadataSearchJob


class _Service:
    def __init__(self):
        self.calls = []

    def search(self, query, media_type, year=None):
        self.calls.append((query, media_type, year))
        if media_type == "movie":
            return []
        return [{"title": "A Show", "media_type": "tv_show"}]


def test_video_metadata_search_job_uses_alternate_kind_without_ui_blocking():
    service = _Service()
    job = VideoMetadataSearchJob(service, "A Show", "movie", 2004)
    results = []
    errors = []
    job.signals.result.connect(results.append)
    job.signals.error.connect(errors.append)

    job.run()

    assert errors == []
    assert results == [[{"title": "A Show", "media_type": "tv_show"}]]
    assert service.calls == [
        ("A Show", "movie", 2004),
        ("A Show", "tv_show", 2004),
    ]


def test_video_metadata_search_includes_tv_results_when_movie_results_exist():
    class MixedService:
        def __init__(self):
            self.calls = []

        def search(self, query, media_type, year=None):
            self.calls.append((query, media_type, year))
            if media_type == "movie":
                return [{"title": "A Film", "media_type": "movie"}]
            return [{"title": "A Show", "media_type": "tv_show"}]

    service = MixedService()
    job = VideoMetadataSearchJob(service, "A Title", "movie")
    results = []
    job.signals.result.connect(results.append)

    job.run()

    assert [item["media_type"] for item in results[0]] == ["movie", "tv_show"]
    assert service.calls == [
        ("A Title", "movie", None),
        ("A Title", "tv_show", None),
    ]

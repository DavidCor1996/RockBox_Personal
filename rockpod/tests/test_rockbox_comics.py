import zipfile
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication

from services.rockbox_comics import ComicSyncError, RockboxComicService
from services.comics_store import parse_comic_download_progress
from ui.comic_sync import ComicSyncWidget
from ui.sidebar import Sidebar


REPO_ROOT = Path(__file__).resolve().parents[2]


def _prepared_issue(root, issue_id="sample", pages=2, reading_direction="ltr"):
    issue = Path(root) / issue_id
    (issue / "pages").mkdir(parents=True)
    (issue / "cover.jpg").write_bytes(b"cover")
    for page in range(1, pages + 1):
        (issue / "pages" / f"{page:04d}.jpg").write_bytes(b"page")
    (issue / "issue.mgi").write_text(
        "\n".join(
            [
                "schema=1",
                f"id={issue_id}",
                "title=Sample Comic",
                f"page_count={pages}",
                "cover=cover.jpg",
                "page_pattern=pages/%04d.jpg",
                f"reading_direction={reading_direction}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    return issue


def _make_cbz(path):
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("001.jpg", b"page one")
        archive.writestr("002.jpg", b"page two")


def test_comic_import_and_prepare_command(config, tmp_dir):
    config.set("comics_library_path", str(Path(tmp_dir) / "comics"))
    service = RockboxComicService(config, REPO_ROOT)
    source = Path(tmp_dir) / "My Comic.cbz"
    _make_cbz(source)

    issue_id = service.import_archive(source)
    duplicate_id = service.import_archive(source)
    command = service.preparation_command(issue_id)

    assert issue_id == "my-comic"
    assert duplicate_id == issue_id
    assert len(list(Path(service.archive_root).glob("*.cbz"))) == 1
    assert Path(service.archive_root, "my-comic.cbz").is_file()
    assert command[2:4] == [
        "--archive",
        str(Path(service.archive_root, "my-comic.cbz")),
    ]
    assert command[command.index("--profile") + 1] == "standard"
    assert command[-1] == "--force"

    fine_command = service.preparation_command(issue_id, "fine-text")
    assert fine_command[fine_command.index("--profile") + 1] == "fine-text"
    try:
        service.preparation_command(issue_id, "oversized")
    except ComicSyncError as exc:
        assert "Unknown comic preparation profile" in str(exc)
    else:
        raise AssertionError("invalid preparation profile was accepted")


def test_comic_reading_direction_adds_manga_flag(config, tmp_dir):
    config.set("comics_library_path", str(Path(tmp_dir) / "comics"))
    service = RockboxComicService(config, REPO_ROOT)
    source = Path(tmp_dir) / "Some Manga.cbz"
    _make_cbz(source)
    issue_id = service.import_archive(source)

    service.set_reading_direction([issue_id], "rtl")
    command = service.preparation_command(issue_id)
    assert "--manga" in command

    try:
        service.set_reading_direction([issue_id], "sideways")
    except ComicSyncError as exc:
        assert "ltr" in str(exc)
    else:
        raise AssertionError("invalid reading direction was accepted")


def test_comic_sync_writes_catalog_and_remove(config, tmp_dir):
    config.set("comics_library_path", str(Path(tmp_dir) / "comics"))
    service = RockboxComicService(config, tmp_dir)
    service.ensure_library()
    _prepared_issue(service.prepared_root)
    mount = Path(tmp_dir) / "ipod"
    mount.mkdir()
    profile = {"device_mount_path": str(mount)}
    issues = service.list_issues(profile)

    assert issues[0]["prepared"]
    assert service.sync_issues(profile, issues) == 1
    target = mount / "Comics"
    assert (target / "sample" / "pages" / "0002.jpg").is_file()
    assert (target / "catalog.mgi").read_text(encoding="utf-8").endswith(
        "sample\n"
    )
    assert service.list_issues(profile)[0]["on_target"]

    assert service.remove_issues(profile, issues) == 1
    assert not (target / "sample").exists()
    assert (target / "catalog.mgi").read_text(encoding="utf-8") == (
        "# Rockbox Comics catalog v1\n"
    )


def test_comic_sync_widget_and_sidebar_entry():
    QApplication.instance() or QApplication([])
    widget = ComicSyncWidget()
    issue = {
        "id": "sample",
        "title": "Sample Comic",
        "creator": "",
        "page_count": 12,
        "reading_direction": "rtl",
        "archive_path": "/tmp/sample.cbz",
        "prepared_path": "/tmp/sample",
        "prepared": True,
        "on_target": False,
    }
    widget.set_issues([issue], ["sample"])

    assert widget.selected_issues()[0]["id"] == "sample"
    assert widget._prepare.isEnabled()
    assert widget._sync.isEnabled()
    assert not widget._remove.isEnabled()
    widget._profile.setCurrentIndex(1)
    assert widget.preparation_profile() == "fine-text"
    widget._direction.setCurrentIndex(1)
    assert widget.reading_direction() == "rtl"

    sidebar = Sidebar()
    matches = sidebar._tree.findItems(
        "Comic / Manga Sync", Qt.MatchExactly | Qt.MatchRecursive, 0
    )
    assert matches
    assert matches[0].data(0, Qt.UserRole) == Sidebar.ROCKBOX_COMICS


def test_comic_categories_and_locks_persist(config, tmp_dir):
    config.set("comics_library_path", str(Path(tmp_dir) / "comics"))
    service = RockboxComicService(config, tmp_dir)
    service.ensure_library()
    _prepared_issue(service.prepared_root)

    service.set_issue_category(["sample"], "Superheroes")
    service.set_issues_locked(["sample"], True)
    issue = service.list_issues()[0]
    manifest = (
        Path(service.prepared_root) / "sample" / "issue.mgi"
    ).read_text(encoding="utf-8")

    assert issue["category"] == "Superheroes"
    assert issue["locked"]
    assert "Superheroes" in service.categories()
    assert "category=Superheroes\n" in manifest
    assert "locked=1\n" in manifest

    service.rename_category("Superheroes", "Capes")
    assert service.list_issues()[0]["category"] == "Capes"
    service.delete_category("Capes")
    assert service.list_issues()[0]["category"] == "Uncategorized"


def test_locked_comic_sync_requires_and_writes_shared_pin(config, tmp_dir):
    config.set("comics_library_path", str(Path(tmp_dir) / "comics"))
    service = RockboxComicService(config, tmp_dir)
    service.ensure_library()
    _prepared_issue(service.prepared_root)
    service.set_issues_locked(["sample"], True)
    issues = service.list_issues()
    mount = Path(tmp_dir) / "ipod"
    mount.mkdir()
    profile = {"device_mount_path": str(mount)}

    try:
        service.sync_issues(profile, issues)
    except ComicSyncError as exc:
        assert "4-digit" in str(exc)
    else:
        raise AssertionError("locked sync accepted no PIN")

    assert service.sync_issues(profile, issues, locked_pin="2468") == 1
    assert (
        mount / ".rockbox" / "videolist" / "locked.pin"
    ).read_text(encoding="ascii") == "2468\n"


def test_parse_comic_download_progress_reports_completion():
    progress = parse_comic_download_progress(
        ["ROCKPOD_COMIC_PROGRESS=42%", "ROCKPOD_COMIC_OUTPUT=/tmp/example.cbz"]
    )
    assert progress["phase"] == "Completed"
    assert progress["progress"] == "100%"
    assert progress["detail"] == "example.cbz"

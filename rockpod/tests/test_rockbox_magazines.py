from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication

from services.rockbox_magazines import MagazineSyncError, RockboxMagazineService
from ui.magazine_sync import MagazineSyncWidget
from ui.sidebar import Sidebar


REPO_ROOT = Path(__file__).resolve().parents[2]


def _prepared_issue(root, issue_id="sample", pages=2):
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
                "title=Sample Magazine",
                f"page_count={pages}",
                "cover=cover.jpg",
                "page_pattern=pages/%04d.jpg",
                "",
            ]
        ),
        encoding="utf-8",
    )
    return issue


def test_magazine_import_and_prepare_command(config, tmp_dir):
    config.set("magazines_library_path", str(Path(tmp_dir) / "magazines"))
    service = RockboxMagazineService(config, REPO_ROOT)
    source = Path(tmp_dir) / "My Magazine.pdf"
    source.write_bytes(b"%PDF-1.4\nfixture")

    issue_id = service.import_pdf(source)
    duplicate_id = service.import_pdf(source)
    command = service.preparation_command(issue_id)

    assert issue_id == "my-magazine"
    assert duplicate_id == issue_id
    assert len(list(Path(service.pdf_root).glob("*.pdf"))) == 1
    assert Path(service.pdf_root, "my-magazine.pdf").is_file()
    assert command[2:4] == [
        "--pdf",
        str(Path(service.pdf_root, "my-magazine.pdf")),
    ]
    assert command[command.index("--profile") + 1] == "standard"
    assert command[-1] == "--force"

    fine_command = service.preparation_command(issue_id, "fine-text")
    assert fine_command[fine_command.index("--profile") + 1] == "fine-text"
    try:
        service.preparation_command(issue_id, "oversized")
    except MagazineSyncError as exc:
        assert "Unknown magazine preparation profile" in str(exc)
    else:
        raise AssertionError("invalid preparation profile was accepted")


def test_magazine_sync_writes_catalog_and_remove(config, tmp_dir):
    config.set("magazines_library_path", str(Path(tmp_dir) / "magazines"))
    service = RockboxMagazineService(config, tmp_dir)
    service.ensure_library()
    _prepared_issue(service.prepared_root)
    mount = Path(tmp_dir) / "ipod"
    mount.mkdir()
    profile = {"device_mount_path": str(mount)}
    issues = service.list_issues(profile)

    assert issues[0]["prepared"]
    assert service.sync_issues(profile, issues) == 1
    target = mount / "Magazines"
    assert (target / "sample" / "pages" / "0002.jpg").is_file()
    assert (target / "catalog.mgi").read_text(encoding="utf-8").endswith(
        "sample\n"
    )
    assert service.list_issues(profile)[0]["on_target"]

    assert service.remove_issues(profile, issues) == 1
    assert not (target / "sample").exists()
    assert (target / "catalog.mgi").read_text(encoding="utf-8") == (
        "# Rockbox Magazines catalog v1\n"
    )


def test_magazine_sync_widget_and_sidebar_entry():
    QApplication.instance() or QApplication([])
    widget = MagazineSyncWidget()
    issue = {
        "id": "sample",
        "title": "Sample Magazine",
        "creator": "",
        "page_count": 12,
        "pdf_path": "/tmp/sample.pdf",
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

    sidebar = Sidebar()
    matches = sidebar._tree.findItems(
        "Magazine Sync", Qt.MatchExactly | Qt.MatchRecursive, 0
    )
    assert matches
    assert matches[0].data(0, Qt.UserRole) == Sidebar.ROCKBOX_MAGAZINES


def test_magazine_categories_and_locks_persist(config, tmp_dir):
    config.set("magazines_library_path", str(Path(tmp_dir) / "magazines"))
    service = RockboxMagazineService(config, tmp_dir)
    service.ensure_library()
    _prepared_issue(service.prepared_root)

    service.set_issue_category(["sample"], "Sports")
    service.set_issues_locked(["sample"], True)
    issue = service.list_issues()[0]
    manifest = (
        Path(service.prepared_root) / "sample" / "issue.mgi"
    ).read_text(encoding="utf-8")

    assert issue["category"] == "Sports"
    assert issue["locked"]
    assert "Sports" in service.categories()
    assert "category=Sports\n" in manifest
    assert "locked=1\n" in manifest

    service.rename_category("Sports", "Collectibles")
    assert service.list_issues()[0]["category"] == "Collectibles"
    service.delete_category("Collectibles")
    assert service.list_issues()[0]["category"] == "Uncategorized"


def test_locked_magazine_sync_requires_and_writes_shared_pin(config, tmp_dir):
    config.set("magazines_library_path", str(Path(tmp_dir) / "magazines"))
    service = RockboxMagazineService(config, tmp_dir)
    service.ensure_library()
    _prepared_issue(service.prepared_root)
    service.set_issues_locked(["sample"], True)
    issues = service.list_issues()
    mount = Path(tmp_dir) / "ipod"
    mount.mkdir()
    profile = {"device_mount_path": str(mount)}

    try:
        service.sync_issues(profile, issues)
    except MagazineSyncError as exc:
        assert "4-digit" in str(exc)
    else:
        raise AssertionError("locked sync accepted no PIN")

    assert service.sync_issues(profile, issues, locked_pin="2468") == 1
    assert (
        mount / ".rockbox" / "videolist" / "locked.pin"
    ).read_text(encoding="ascii") == "2468\n"

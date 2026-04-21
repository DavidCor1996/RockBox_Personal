import os
import zipfile

from ui.web_browser import extract_downloaded_archive, should_block_browser_url


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

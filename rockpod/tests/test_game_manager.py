from PySide6.QtWidgets import QApplication

from ui.game_manager import GameManagerWidget


def _game(game_id, title, added, platform="Game Boy", on_device=False, cover=""):
    return {
        "id": game_id,
        "title": title,
        "filename": f"{game_id}.gb",
        "source_path": f"/roms/{game_id}.gb",
        "size": 1024,
        "added_time": added,
        "modified_time": added,
        "platform": platform,
        "on_device": on_device,
        "on_simulator": False,
        "device_save_exists": False,
        "simulator_save_exists": False,
        "cover_path": cover,
        "year": "",
        "genre": "",
        "publisher": "",
        "developer": "",
        "description": "",
        "missing_source": False,
        "performance": {"level": "ok", "notes": []},
    }


def test_game_manager_defaults_to_recent_and_filters_library():
    QApplication.instance() or QApplication([])
    widget = GameManagerWidget()
    games = [
        _game("older", "Alpha", 100, on_device=True),
        _game("newest", "Zelda", 300, platform="Game Boy Color"),
        _game("middle", "Mario", 200),
    ]

    widget.set_games(games)

    assert widget._game_list.item(0).data(256)["id"] == "newest"
    assert widget._result_count.text() == "3 of 3"
    assert "Game Boy Color" in widget._game_list.item(0).text()
    assert "Not synced" in widget._game_list.item(0).text()

    widget._search_edit.setText("mario")
    assert widget._game_list.count() == 1
    assert widget._game_list.item(0).data(256)["id"] == "middle"

    widget._search_edit.clear()
    widget._status_filter.setCurrentIndex(
        widget._status_filter.findData("on_target")
    )
    assert widget._game_list.count() == 1
    assert widget._game_list.item(0).data(256)["id"] == "older"

    widget.close()


def test_game_manager_disables_source_actions_for_missing_game():
    QApplication.instance() or QApplication([])
    widget = GameManagerWidget()
    missing = _game("missing", "Missing", 100)
    missing["missing_source"] = True
    widget.set_games([missing])
    widget._game_list.item(0).setSelected(True)
    widget.set_selection_details([missing])

    assert widget._sync_btn.isEnabled() is False
    assert widget._fetch_metadata_btn.isEnabled() is False
    assert widget._remove_btn.isEnabled() is True

    widget.close()


def test_game_manager_exposes_separate_genesis_library_source():
    QApplication.instance() or QApplication([])
    widget = GameManagerWidget()

    widget.set_library_state(
        "/roms/gameboy", "/device/gameboy", "1 game indexed",
        genesis_library_path="/roms/genesis",
    )

    assert widget._genesis_library_edit.text() == "/roms/genesis"
    assert widget._genesis_browse_btn.text() == "Choose Genesis Folder"

    widget.close()

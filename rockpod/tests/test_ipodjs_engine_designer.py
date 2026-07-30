from PySide6.QtWidgets import QApplication

from ui.ipodjs_engine_designer import IPodJSEngineDesignerWidget


def test_ipodjs_designer_defaults_to_stock_apple_holdscreen():
    app = QApplication.instance() or QApplication([])

    widget = IPodJSEngineDesignerWidget()

    settings = widget._settings()
    assert settings["rockbox_ui_engine"] == "ipodjs"
    assert settings["rockbox_ui_accent"] == "blue"
    assert settings["rockbox_ui_dark_mode"] is False
    assert settings["rockbox_ui_density"] == "comfortable"
    assert settings["rockbox_ui_font_scale"] == "normal"
    assert settings["rockbox_ui_surface"] == "solid"
    assert settings["rockbox_ui_hold_effect"] == "lockscreen"
    assert settings["rockbox_ui_extras_pane"] == "clock"
    assert widget._extras_pane.currentText() == "Clock"
    assert widget._hold_effect.currentText() == "Lockscreen"
    assert widget._preview._screen == "home"
    assert app is not None

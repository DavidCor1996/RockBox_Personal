"""iTunes 7-era (2006-2007) faithful stylesheet for PySide6.

Visual reference: iTunes 7 on Mac OS X Tiger / Leopard.
Key characteristics:
  - Brushed-metal / unified gray toolbar
  - Source list sidebar with blue rounded-rect selections
  - White content area with alternating light-blue rows
  - Glossy gradient column headers
  - Aqua-tinted scrollbars and controls
  - Lucida Grande-style typography (we use the closest available)
"""

# Font stack: prefer fonts that match the late-2000s Mac aesthetic.
# Lucida Grande was THE Mac system font of that era.
FONT_FAMILY = (
    "'Lucida Grande', 'Segoe UI', 'Helvetica Neue', 'Ubuntu', "
    "'Noto Sans', 'Liberation Sans', Arial, sans-serif"
)

# ── Color constants ──────────────────────────────────────────────────
# Toolbar / chrome
TOOLBAR_GRAD_TOP = "#eeeeee"
TOOLBAR_GRAD_BOT = "#a7a7a7"
TOOLBAR_BORDER = "#777777"

# Sidebar (source list)
SIDEBAR_BG = "#d8dde4"
SIDEBAR_BG_GRAD_TOP = "#e3e7ed"
SIDEBAR_BG_GRAD_BOT = "#c9cfd7"
SIDEBAR_SECTION_TEXT = "#6c7177"
SIDEBAR_ITEM_TEXT = "#1a1a1a"
SIDEBAR_SEL_GRAD_TOP = "#9db1cb"
SIDEBAR_SEL_GRAD_BOT = "#7089ab"
SIDEBAR_SEL_TEXT = "#ffffff"
SIDEBAR_HOVER_BG = "#cfd6de"
SIDEBAR_BORDER = "#a7adb6"

# Content area
CONTENT_BG = "#ffffff"
TABLE_ALT_ROW = "#f5f7fa"
TABLE_SEL_GRAD_TOP = "#a1b4ce"
TABLE_SEL_GRAD_BOT = "#758eaf"
TABLE_SEL_TEXT = "#ffffff"
TABLE_GRID_COLOR = "#d9dde2"
TABLE_TEXT = "#1a1a1a"
TABLE_TEXT_SECONDARY = "#666666"

# Column headers
HEADER_GRAD_TOP = "#f9f9f9"
HEADER_GRAD_BOT = "#dbdbdb"
HEADER_BORDER = "#b8bcc2"
HEADER_TEXT = "#404040"
HEADER_PRESSED_TOP = "#d6d6d6"
HEADER_PRESSED_BOT = "#b9b9b9"

# Status bar
STATUS_BG = "#e9e9e9"
STATUS_BORDER = "#b9bcc1"
STATUS_TEXT = "#555555"

# Storage bar
STORAGE_AUDIO_COLOR = "#4a90d9"
STORAGE_OTHER_COLOR = "#e8a838"
STORAGE_FREE_COLOR = "#e8e8e8"
STORAGE_BORDER = "#999999"

# Scrollbar (aqua-inspired)
SCROLL_BG = "#ececec"
SCROLL_HANDLE = "#a2abb5"
SCROLL_HANDLE_HOVER = "#8b97a4"
SCROLL_HANDLE_PRESSED = "#707d8c"

# General
FOCUS_RING = "#6aabe8"
SEPARATOR = "#c0c0c0"


def get_stylesheet():
    """Return the complete QSS stylesheet string."""
    return f"""
/* ═══════════════════════════════════════════════════════════════════
   RockPod — iTunes 7-era stylesheet
   ═══════════════════════════════════════════════════════════════════ */

/* ── Global defaults ─────────────────────────────────────────────── */
QWidget {{
    font-family: {FONT_FAMILY};
    font-size: 13px;
    color: {TABLE_TEXT};
    background: {CONTENT_BG};
}}

QPushButton {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff,
        stop:0.18 #f7f7f7,
        stop:0.49 #e4e4e4,
        stop:0.50 #d1d1d1,
        stop:1 #bfc4ca);
    border: 1px solid #7f8790;
    border-top-color: #9aa1a8;
    border-radius: 3px;
    color: #1f2933;
    font-size: 11px;
    font-weight: bold;
    padding: 2px 10px;
    min-height: 19px;
}}

QPushButton:hover {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.42 #f4f7fb, stop:0.43 #dbe4ef, stop:1 #b7c3d1);
}}

QPushButton:pressed {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #8997a7, stop:1 #c2ccd8);
    color: #ffffff;
}}

QPushButton:disabled {{
    color: #8b8f94;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f4f4f4, stop:1 #dadada);
    border-color: #b6bbc0;
}}

QLineEdit,
QComboBox {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.55 #fbfbfb, stop:1 #ededed);
    border: 1px solid #9aa2ab;
    border-radius: 3px;
    padding: 2px 6px;
    min-height: 18px;
    selection-background-color: {TABLE_SEL_GRAD_TOP};
    font-size: 11px;
}}

QLineEdit:focus,
QComboBox:focus {{
    border: 1px solid {FOCUS_RING};
}}

QComboBox::drop-down {{
    width: 18px;
    border: none;
}}

QListWidget,
QTreeWidget,
QTableView {{
    selection-background-color: {TABLE_SEL_GRAD_TOP};
    selection-color: {TABLE_SEL_TEXT};
}}

/* ── Main window ─────────────────────────────────────────────────── */
QMainWindow {{
    background: {SIDEBAR_BG};
}}

QMainWindow::separator {{
    background: {SIDEBAR_BORDER};
    width: 1px;
    height: 1px;
}}

/* ── Toolbar area ────────────────────────────────────────────────── */
QWidget#toolbar_container {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f9f9f9,
        stop:0.03 #efefef,
        stop:0.08 #d7d7d7,
        stop:0.22 #c7c7c7,
        stop:0.50 #b9b9b9,
        stop:0.51 #ababab,
        stop:0.76 #c6c6c6,
        stop:1 #9f9f9f);
    border-bottom: 1px solid #5f5f5f;
    border-top: 1px solid #fdfdfd;
    min-height: 44px;
    max-height: 44px;
}}

QWidget#toolbar_group_left,
QWidget#toolbar_group_center,
QWidget#toolbar_group_right {{
    background: transparent;
}}

QWidget#toolbar_center_shell {{
    background: transparent;
}}

QWidget#toolbar_group_center {{
    min-width: 0px;
    margin-left: 3px;
    margin-right: 3px;
}}

QFrame#toolbar_separator {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.18 #efefef, stop:0.48 #adadad, stop:0.82 #d5d5d5, stop:1 #fbfbfb);
    margin-top: 6px;
    margin-bottom: 6px;
}}

QWidget#transport_group {{
    background: transparent;
}}

QWidget#playback_cluster {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fbfff4,
        stop:0.08 #eff6df,
        stop:0.50 #dfeac6,
        stop:0.51 #cbd8af,
        stop:1 #eff5de);
    border: 1px solid #7f8674;
    border-top-color: #5f6656;
    border-left-color: #6e7563;
    border-radius: 9px;
    min-width: 0px;
    padding: 1px 3px 1px 2px;
}}

QWidget#playback_cluster[active="true"] {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fffff6,
        stop:0.12 #f3f9e5,
        stop:0.50 #dce9c3,
        stop:0.51 #c7d5ab,
        stop:1 #edf5da);
    border: 1px solid #686f5e;
}}

QWidget#toolbar_group_center {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 rgba(255,255,255,22), stop:1 rgba(255,255,255,0));
    border-radius: 5px;
}}

QWidget#now_playing_group {{
    background: transparent;
}}

QWidget#now_playing_text_row,
QWidget#playback_progress_group,
QWidget#now_playing_text {{
    background: transparent;
}}

QWidget#toolbar_container QLabel {{
    color: #1a1a1a;
    background: transparent;
}}

QWidget#toolbar_container QLabel#selected_art {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.5 #f6f6f6, stop:1 #d5d5d5);
    border: 1px solid #666666;
    margin-left: 0px;
    margin-right: 1px;
}}

QWidget#toolbar_container QLabel#selected_title {{
    font-size: 12px;
    font-weight: bold;
    color: #101010;
    min-height: 11px;
    max-width: 252px;
}}

QWidget#toolbar_container QLabel#selected_title[active="false"] {{
    color: #565656;
}}

QWidget#toolbar_container QLabel#selected_artist {{
    font-size: 10px;
    color: #747474;
    min-height: 8px;
    max-width: 252px;
}}

QWidget#toolbar_container QLabel#selected_artist[active="false"] {{
    color: #858585;
}}

QWidget#toolbar_container QLabel#selected_time {{
    font-size: 10px;
    color: #6b6b6b;
    min-height: 8px;
    min-width: 54px;
    max-width: 62px;
}}

QWidget#toolbar_container QLabel#selected_time[active="false"] {{
    color: #858585;
}}

QWidget#toolbar_container QLabel#toolbar_title {{
    color: #535353;
    padding-left: 4px;
    padding-right: 0px;
    font-size: 10px;
    font-weight: bold;
}}

QWidget#toolbar_container QPushButton {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff,
        stop:0.18 #f8f8f8,
        stop:0.48 #e7e7e7,
        stop:0.49 #d2d2d2,
        stop:0.86 #b8b8b8,
        stop:1 #dcdcdc);
    border: 1px solid #747474;
    border-top-color: #858585;
    border-radius: 3px;
    padding: 0px 4px;
    color: #1a1a1a;
    font-size: 10px;
    font-weight: bold;
    min-height: 17px;
}}

QWidget#toolbar_container QPushButton#new_playlist_button {{
    padding: 0px;
    min-width: 18px;
    max-width: 18px;
    font-size: 12px;
    font-weight: bold;
}}

QWidget#toolbar_container QPushButton#prefs_button {{
    padding-left: 7px;
    padding-right: 7px;
}}

QWidget#toolbar_container QPushButton#playback_button {{
    padding: 0px;
    min-width: 22px;
    max-width: 29px;
    min-height: 22px;
    max-height: 24px;
    font-size: 10px;
    border-radius: 11px;
    border: 1px solid #626262;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff,
        stop:0.10 #fdfdfd,
        stop:0.34 #efefef,
        stop:0.35 #dcdcdc,
        stop:0.70 #c6c6c6,
        stop:1 #9d9d9d);
}}

QWidget#toolbar_container QPushButton#playback_button:hover {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff,
        stop:0.18 #ffffff,
        stop:0.44 #f4f4f4,
        stop:0.45 #e2e2e2,
        stop:1 #b9b9b9);
}}

QWidget#toolbar_container QPushButton#playback_button:pressed {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #8d877f, stop:0.24 #7f7a72, stop:0.25 #78736c, stop:1 #696560);
    color: #ffffff;
}}

QWidget#toolbar_container QSlider#playback_progress,
QWidget#toolbar_container QSlider#volume_slider {{
    background: transparent;
    border: none;
    outline: none;
}}

QWidget#toolbar_container QSlider#playback_progress::groove:horizontal {{
    height: 3px;
    border: none;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #9aa18b, stop:0.5 #777d6d, stop:1 #d9e2c4);
    border-radius: 2px;
}}

QWidget#toolbar_container QSlider#playback_progress::sub-page:horizontal {{
    border: none;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #8da5c2, stop:0.5 #6888ad, stop:1 #557396);
    border-radius: 2px;
}}

QWidget#toolbar_container QSlider#playback_progress::add-page:horizontal {{
    border: none;
    background: transparent;
    border-radius: 2px;
}}

QWidget#toolbar_container QSlider#volume_slider::groove:horizontal {{
    height: 2px;
    border: none;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #8f8f8f, stop:1 #d8d8d8);
    border-radius: 2px;
}}

QWidget#toolbar_container QSlider#volume_slider::sub-page:horizontal {{
    border: none;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #9da9b8, stop:1 #7d8da1);
    border-radius: 2px;
}}

QWidget#toolbar_container QSlider#volume_slider::add-page:horizontal {{
    border: none;
    background: transparent;
    border-radius: 2px;
}}

QWidget#toolbar_container QSlider#playback_progress::handle:horizontal,
QWidget#toolbar_container QSlider#volume_slider::handle:horizontal {{
    width: 7px;
    margin: -4px 0;
    border: 1px solid #686868;
    border-radius: 3px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.45 #e5e5e5, stop:1 #afafaf);
}}

QWidget#toolbar_container QSlider#playback_progress {{
    min-height: 9px;
}}

QWidget#toolbar_container QSlider#volume_slider {{
    min-height: 8px;
}}

QWidget#toolbar_container QSlider#playback_progress:focus,
QWidget#toolbar_container QSlider#volume_slider:focus {{
    border: none;
    outline: none;
}}

QWidget#toolbar_container QPushButton:hover {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.4 #fbfbfb, stop:0.41 #e8e8e8, stop:1 #c8c8c8);
}}

QWidget#toolbar_container QPushButton:pressed {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #aaaaaa, stop:0.42 #999999, stop:0.43 #919191, stop:1 #848484);
    color: #ffffff;
}}

QWidget#toolbar_container QPushButton#sync_button {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #6aade8, stop:0.5 #4a90d9, stop:0.51 #3a7cc8, stop:1 #2a68b8);
    border: 1px solid #2060a0;
    color: #ffffff;
}}

QWidget#toolbar_container QPushButton#sync_button:hover {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #7abdf8, stop:0.5 #5aa0e9, stop:0.51 #4a8cd8, stop:1 #3a78c8);
}}

QWidget#toolbar_container QPushButton#sync_button:pressed {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #2a58a0, stop:0.5 #204890, stop:0.51 #1a3878, stop:1 #143068);
}}

/* ── Search field (capsule shape like iTunes) ────────────────────── */
QLineEdit#search_field {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.55 #f7f7f7, stop:1 #eeeeee);
    border: 1px solid #8f8f8f;
    border-radius: 9px;
    padding: 0px 6px 0px 7px;
    font-size: 10px;
    min-width: 144px;
    max-width: 152px;
    min-height: 13px;
    selection-background-color: {TABLE_SEL_GRAD_TOP};
    selection-color: #ffffff;
}}

QLineEdit#search_field:focus {{
    border: 1px solid {FOCUS_RING};
}}

/* ── Sidebar (Source List) ───────────────────────────────────────── */
QWidget#sidebar {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #eef2f7, stop:0.12 #e4e9f0, stop:0.62 #d8dee7, stop:1 #c6ceda);
    border-right: 1px solid #8d96a2;
}}

QTreeWidget#source_list {{
    background: transparent;
    border: none;
    outline: none;
    font-size: 11px;
    show-decoration-selected: 1;
}}

QTreeWidget#source_list::item {{
    padding: 0px 5px 0px 2px;
    min-height: 15px;
    border: none;
    color: {SIDEBAR_ITEM_TEXT};
}}

QTreeWidget#source_list::item:hover {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #dde3ea, stop:1 #cfd6de);
    border-top: 1px solid #edf2f6;
    border-bottom: 1px solid #c3c9d1;
}}

QTreeWidget#source_list::item:selected {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #a8b9cf, stop:0.5 #8ea2bf, stop:1 #7188a7);
    color: {SIDEBAR_SEL_TEXT};
    border-top: 1px solid #c5d0de;
    border-bottom: 1px solid #5f7593;
}}

QTreeWidget#source_list::branch {{
    background: transparent;
}}

/* Section headers in the source list */
QTreeWidget#source_list QLabel#section_header {{
    color: {SIDEBAR_SECTION_TEXT};
    font-size: 10px;
    font-weight: bold;
    text-transform: uppercase;
    padding: 5px 0px 0px 7px;
    background: transparent;
}}

/* ── Track table (main content) ──────────────────────────────────── */
QTableView#track_table {{
    background: {CONTENT_BG};
    alternate-background-color: {TABLE_ALT_ROW};
    gridline-color: {TABLE_GRID_COLOR};
    border: 1px solid #c8cdd4;
    border-top: none;
    selection-background-color: {TABLE_SEL_GRAD_TOP};
    selection-color: {TABLE_SEL_TEXT};
    font-size: 11px;
    outline: none;
}}

QTableView#track_table::item {{
    padding: 0px 2px;
    border: none;
}}

QTableView#track_table::item:selected {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #a7b9d1, stop:0.5 #8da3c0, stop:1 #7288a8);
    color: {TABLE_SEL_TEXT};
}}

/* ── Device summary ───────────────────────────────────────────────── */
QWidget#device_summary {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f8f8f8, stop:0.08 #eeeeee, stop:0.36 #dedede, stop:1 #cfcfcf);
    color: #1a1a1a;
}}

QFrame#device_summary_header {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff,
        stop:0.09 #f8f8f8,
        stop:0.48 #e7e7e7,
        stop:0.49 #d4d4d4,
        stop:1 #bdbdbd);
    border: 1px solid #858585;
    border-top-color: #a5a5a5;
    border-radius: 4px;
}}

QLabel#device_summary_name {{
    background: transparent;
    font-size: 15px;
    font-weight: bold;
    color: #111111;
}}

QLabel#device_summary_model {{
    background: transparent;
    font-size: 11px;
    color: #2d2d2d;
}}

QLabel#device_summary_detail {{
    background: transparent;
    font-size: 10px;
    color: #6e6e6e;
}}

QLabel#device_summary_label {{
    background: transparent;
    font-size: 10px;
    color: #6b6b6b;
}}

QLabel#device_summary_value {{
    background: transparent;
    font-weight: bold;
    color: #202020;
    font-size: 11px;
}}

QWidget#device_summary QCheckBox {{
    background: transparent;
}}

QWidget#device_summary QCheckBox::indicator {{
    width: 11px;
    height: 11px;
    border: 1px solid #9aa2ab;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f9f9f9, stop:1 #d9d9d9);
}}

QWidget#device_summary QCheckBox::indicator:checked {{
    border: 1px solid #6f7d8c;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #edf5dc, stop:1 #c7d5ab);
}}

QGroupBox#device_summary_group {{
    border: 1px solid #888888;
    border-top-color: #b6b6b6;
    border-radius: 3px;
    margin-top: 9px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.13 #f7f7f7, stop:0.52 #e9e9e9, stop:1 #dcdcdc);
    font-weight: bold;
}}

QGroupBox#device_summary_group::title {{
    subcontrol-origin: margin;
    left: 7px;
    padding: 1px 4px 0px 4px;
    color: #3f3f3f;
    font-size: 10px;
}}

QPushButton#device_summary_button {{
    min-height: 17px;
    padding: 0px 7px;
    border-radius: 3px;
    font-size: 10px;
}}

QTableView#track_table::item:focus {{
    outline: none;
    border: none;
}}

/* Column headers (glossy gradient) */
QHeaderView {{
    background: transparent;
    border: none;
}}

QHeaderView::section {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fbfbfb, stop:0.14 #f2f2f2, stop:0.58 #e1e1e1, stop:1 #cacaca);
    border: none;
    border-right: 1px solid #b8bcc2;
    border-bottom: 1px solid #aeb4bc;
    padding: 1px 5px;
    font-size: 10px;
    font-weight: bold;
    color: {HEADER_TEXT};
    min-height: 15px;
}}

QHeaderView::section:hover {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:1 #ebebeb);
}}

QHeaderView::section:pressed {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 {HEADER_PRESSED_TOP}, stop:1 {HEADER_PRESSED_BOT});
}}

QHeaderView::down-arrow {{
    image: none;
    width: 0;
    height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #666666;
    margin-right: 6px;
}}

QHeaderView::up-arrow {{
    image: none;
    width: 0;
    height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-bottom: 5px solid #666666;
    margin-right: 6px;
}}

/* ── Column browser panes ────────────────────────────────────────── */
QListWidget#browser_pane {{
    background: {CONTENT_BG};
    border: 1px solid #c8cdd4;
    border-top: none;
    font-size: 11px;
    outline: none;
}}

QListWidget#browser_pane::item {{
    padding: 0px 5px;
    min-height: 15px;
}}

QListWidget#browser_pane::item:selected {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 {TABLE_SEL_GRAD_TOP}, stop:1 {TABLE_SEL_GRAD_BOT});
    color: {TABLE_SEL_TEXT};
}}

QListWidget#browser_pane::item:hover {{
    background: {TABLE_ALT_ROW};
}}

QListWidget#library_group_list {{
    background: {CONTENT_BG};
    border: none;
    border-right: 1px solid #c8cdd4;
    font-size: 11px;
    outline: none;
}}

QListWidget#library_group_list::item {{
    padding: 0px 5px;
    min-height: 15px;
}}

QListWidget#library_group_list::item:selected {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 {TABLE_SEL_GRAD_TOP}, stop:1 {TABLE_SEL_GRAD_BOT});
    color: {TABLE_SEL_TEXT};
}}

QListWidget#library_group_list::item:hover {{
    background: {TABLE_ALT_ROW};
}}

QListWidget#album_grid {{
    background: {CONTENT_BG};
    border: 1px solid #c8cdd4;
    border-top: none;
    outline: none;
    font-size: 11px;
    padding: 5px;
}}

QListWidget#album_grid::item {{
    padding: 4px;
    color: {TABLE_TEXT};
}}

QListWidget#album_grid::item:hover {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f6f9fc, stop:1 #e8eef6);
    border: 1px solid #c3cfde;
    border-radius: 3px;
}}

QListWidget#album_grid::item:selected {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 {TABLE_SEL_GRAD_TOP}, stop:1 {TABLE_SEL_GRAD_BOT});
    color: {TABLE_SEL_TEXT};
    border: 1px solid #6a80a0;
    border-radius: 3px;
}}

/* Browser pane header */
QLabel#browser_header {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fbfbfb, stop:0.14 #f2f2f2, stop:0.58 #e1e1e1, stop:1 #cacaca);
    border: 1px solid #b8bcc2;
    border-top: none;
    padding: 1px 6px;
    font-size: 11px;
    font-weight: bold;
    color: {HEADER_TEXT};
}}

/* ── Status bar ──────────────────────────────────────────────────── */
QWidget#status_bar {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #bdbdbd, stop:0.08 #efefef, stop:0.42 #d6d6d6, stop:0.43 #b9b9b9, stop:1 #dedede);
    border-top: 1px solid #7e7e7e;
    min-height: 20px;
    max-height: 20px;
}}

QWidget#status_bar QLabel {{
    color: {STATUS_TEXT};
    font-size: 11px;
    background: transparent;
    padding: 0px 5px;
}}

QWidget#status_bar QLabel#status_left,
QWidget#status_bar QLabel#status_right {{
    color: #626262;
}}

QWidget#status_bar QLabel#status_center {{
    color: #4f4f4f;
}}

/* ── Storage bar ─────────────────────────────────────────────────── */
QWidget#storage_bar_container {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #eeeeee, stop:0.24 #d8d8d8, stop:0.25 #bdbdbd, stop:1 #dedede);
    border-top: 1px solid #858585;
    min-height: 42px;
    max-height: 42px;
}}

QWidget#storage_bar_container QLabel {{
    color: {STATUS_TEXT};
    font-size: 9px;
    background: transparent;
}}

/* ── Scrollbars (aqua-inspired) ──────────────────────────────────── */
QScrollBar:vertical {{
    background: {SCROLL_BG};
    width: 11px;
    margin: 0;
    border-left: 1px solid #d0d0d0;
}}

QScrollBar::handle:vertical {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #bcc4cc, stop:1 {SCROLL_HANDLE});
    min-height: 30px;
    border: 1px solid #8f98a2;
    border-radius: 4px;
    margin: 1px 1px;
}}

QScrollBar::handle:vertical:hover {{
    background: {SCROLL_HANDLE_HOVER};
}}

QScrollBar::handle:vertical:pressed {{
    background: {SCROLL_HANDLE_PRESSED};
}}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
    height: 0px;
}}

QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {{
    background: transparent;
}}

QScrollBar:horizontal {{
    background: {SCROLL_BG};
    height: 11px;
    margin: 0;
    border-top: 1px solid #d0d0d0;
}}

QScrollBar::handle:horizontal {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #bcc4cc, stop:1 {SCROLL_HANDLE});
    min-width: 30px;
    border: 1px solid #8f98a2;
    border-radius: 4px;
    margin: 1px 1px;
}}

QScrollBar::handle:horizontal:hover {{
    background: {SCROLL_HANDLE_HOVER};
}}

QScrollBar::handle:horizontal:pressed {{
    background: {SCROLL_HANDLE_PRESSED};
}}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
    width: 0px;
}}

QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {{
    background: transparent;
}}

/* ── Dialogs and secondary windows ───────────────────────────────── */
QDialog {{
    background: {SIDEBAR_BG};
}}

QDialog QLabel {{
    color: {TABLE_TEXT};
    background: transparent;
}}

QDialog QLineEdit {{
    background: #ffffff;
    border: 1px solid #aaaaaa;
    border-radius: 3px;
    padding: 3px 6px;
    font-size: 11px;
    selection-background-color: {TABLE_SEL_GRAD_TOP};
}}

QDialog QLineEdit:focus {{
    border: 1px solid {FOCUS_RING};
}}

QDialog QPushButton {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fafafa, stop:0.5 #e8e8e8, stop:0.51 #d8d8d8, stop:1 #c8c8c8);
    border: 1px solid #888888;
    border-radius: 4px;
    padding: 4px 16px;
    font-size: 11px;
    min-height: 20px;
    min-width: 60px;
}}

QDialog QPushButton:hover {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.5 #f0f0f0, stop:0.51 #e0e0e0, stop:1 #d0d0d0);
}}

QDialog QPushButton:pressed {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #b0b0b0, stop:1 #8a8a8a);
    color: #ffffff;
}}

QDialog QPushButton:default {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #6aade8, stop:0.5 #4a90d9, stop:0.51 #3a7cc8, stop:1 #2a68b8);
    border: 1px solid #2060a0;
    color: #ffffff;
    font-weight: bold;
}}

QDialog QComboBox {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fafafa, stop:1 #e0e0e0);
    border: 1px solid #999999;
    border-radius: 3px;
    padding: 2px 6px;
    font-size: 11px;
    min-height: 20px;
}}

QDialog QSpinBox {{
    background: #ffffff;
    border: 1px solid #aaaaaa;
    border-radius: 3px;
    padding: 2px 4px;
    font-size: 11px;
}}

QDialog QCheckBox {{
    spacing: 6px;
    font-size: 11px;
    background: transparent;
}}

QDialog QGroupBox {{
    background: transparent;
    border: 1px solid #c0c0c0;
    border-radius: 4px;
    margin-top: 12px;
    padding-top: 16px;
    font-weight: bold;
    font-size: 11px;
}}

QDialog QGroupBox::title {{
    subcontrol-origin: margin;
    padding: 0px 6px;
    color: #444444;
}}

QDialog#sync_dialog {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f6f6f6, stop:0.14 #e7e7e7, stop:0.55 #d7d7d7, stop:1 #c8c8c8);
}}

QFrame#sync_dialog_header {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff,
        stop:0.10 #f8f8f8,
        stop:0.48 #e7e7e7,
        stop:0.49 #d2d2d2,
        stop:1 #bdbdbd);
    border: 1px solid #858585;
    border-top-color: #aaaaaa;
    border-radius: 4px;
}}

QLabel#sync_dialog_icon {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.48 #f2f2f2, stop:0.49 #d9d9d9, stop:1 #bcbcbc);
    border: 1px solid #777777;
    border-radius: 4px;
    font-size: 16px;
    color: #4c6f9d;
}}

QLabel#sync_dialog_title {{
    font-size: 14px;
    font-weight: bold;
    color: #1c1c1c;
}}

QLabel#sync_dialog_subtitle {{
    font-size: 11px;
    color: #5e5e5e;
}}

QFrame#sync_summary_panel,
QFrame#sync_progress_panel,
QFrame#sync_warning_panel {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.11 #f8f8f8, stop:0.52 #e9e9e9, stop:1 #dcdcdc);
    border: 1px solid #8f8f8f;
    border-top-color: #bdbdbd;
    border-radius: 3px;
}}

QWidget#sync_animation_widget {{
    background: transparent;
}}

QWidget#sync_dialog_footer {{
    background: transparent;
}}

QLabel#sync_summary_label {{
    color: #5d5d5d;
    font-size: 11px;
}}

QLabel#sync_summary_value {{
    color: #1f1f1f;
    font-size: 12px;
    font-weight: bold;
}}

QLabel#sync_warning_title {{
    font-size: 11px;
    font-weight: bold;
    color: #5a4a28;
}}

QLabel#sync_warning_text {{
    color: #584c35;
    font-size: 11px;
}}

QLabel#sync_status_label {{
    color: #2a2a2a;
    font-size: 11px;
    font-weight: bold;
}}

QLabel#sync_item_label {{
    color: #5e5e5e;
    font-size: 11px;
}}

QLabel#sync_results_label {{
    color: #2f2f2f;
    font-size: 11px;
    font-weight: bold;
}}

QPushButton#sync_dialog_button,
QPushButton#sync_dialog_primary_button {{
    min-height: 20px;
    padding: 1px 12px;
    border-radius: 3px;
    font-size: 11px;
    font-weight: bold;
}}

QPushButton#sync_dialog_primary_button {{
    color: #ffffff;
    border: 1px solid #2060a0;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #6aade8, stop:0.5 #4a90d9, stop:0.51 #3a7cc8, stop:1 #2a68b8);
}}

QPushButton#sync_dialog_primary_button:hover {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #7abdf8, stop:0.5 #5aa0e9, stop:0.51 #4a8cd8, stop:1 #3a78c8);
}}

QPushButton#sync_dialog_primary_button:pressed {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #2a58a0, stop:0.5 #204890, stop:0.51 #1a3878, stop:1 #143068);
}}

/* ── Progress bar (sync) ─────────────────────────────────────────── */
QProgressBar {{
    background: #e0e0e0;
    border: 1px solid #a0a0a0;
    border-radius: 4px;
    text-align: center;
    font-size: 11px;
    min-height: 16px;
    max-height: 16px;
    color: #333333;
}}

QProgressBar::chunk {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #6aade8, stop:0.5 #4a90d9, stop:0.51 #3a7cc8, stop:1 #2a68b8);
    border-radius: 3px;
}}

/* ── Tooltips ────────────────────────────────────────────────────── */
QToolTip {{
    background: #ffffdd;
    border: 1px solid #999966;
    padding: 3px 6px;
    font-size: 11px;
    color: #333300;
}}

/* ── Splitter handles ────────────────────────────────────────────── */
QSplitter::handle {{
    background: {SIDEBAR_BORDER};
}}

QSplitter::handle:horizontal {{
    width: 1px;
}}

QSplitter::handle:vertical {{
    height: 1px;
}}

/* ── Menu bar (if used) ──────────────────────────────────────────── */
QMenuBar {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 {TOOLBAR_GRAD_TOP}, stop:1 {TOOLBAR_GRAD_BOT});
    border-bottom: 1px solid {TOOLBAR_BORDER};
    font-size: 11px;
    padding: 1px;
}}

QMenuBar::item {{
    background: transparent;
    padding: 4px 10px;
    color: #1a1a1a;
}}

QMenuBar::item:selected {{
    background: {SIDEBAR_SEL_GRAD_TOP};
    color: #ffffff;
    border-radius: 3px;
}}

QMenu {{
    background: #f8f8f8;
    border: 1px solid #aaaaaa;
    font-size: 11px;
    padding: 4px 0px;
}}

QMenu::item {{
    padding: 4px 28px 4px 20px;
    color: #1a1a1a;
}}

QMenu::item:selected {{
    background: {SIDEBAR_SEL_GRAD_TOP};
    color: #ffffff;
}}

QMenu::separator {{
    height: 1px;
    background: #d0d0d0;
    margin: 4px 8px;
}}

/* ── Tab widget (for info/metadata editor) ───────────────────────── */
QTabWidget::pane {{
    border: 1px solid #c0c0c0;
    background: {CONTENT_BG};
}}

QTabBar::tab {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f0f0f0, stop:1 #d8d8d8);
    border: 1px solid #b0b0b0;
    border-bottom: none;
    padding: 4px 12px;
    font-size: 11px;
    min-width: 60px;
    margin-right: 1px;
}}

QTabBar::tab:selected {{
    background: {CONTENT_BG};
    border-bottom-color: {CONTENT_BG};
}}

QTabBar::tab:hover {{
    background: #e8e8e8;
}}

QTabWidget#store_tabs::pane {{
    border-top: 1px solid #aeb6c1;
    background: #ffffff;
}}

QTabWidget#store_tabs QTabBar::tab {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f8f8f8, stop:0.48 #e6e6e6, stop:0.5 #d4d4d4, stop:1 #c2c7ce);
    border: 1px solid #9ba5b0;
    border-bottom-color: #8d97a3;
    min-width: 92px;
    padding-left: 12px;
    padding-right: 12px;
}}

QTabWidget#store_tabs QTabBar::tab:selected {{
    background: #ffffff;
    border-bottom-color: #ffffff;
}}

/* ── Rockbox Theme Hub ──────────────────────────────────────────── */
QWidget#theme_hub,
QWidget#photo_manager,
QWidget#game_manager,
QWidget#plugin_manager,
QWidget#boot_manager,
QWidget#simulator_panel {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f4f4f4, stop:0.22 #e7e7e7, stop:1 #d4d4d4);
}}

QFrame#theme_hub_header {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff,
        stop:0.12 #f7f7f7,
        stop:0.48 #e4e4e4,
        stop:0.50 #cfcfcf,
        stop:1 #bfc4ca);
    border: 1px solid #8b939d;
    border-top-color: #aab0b7;
    border-radius: 4px;
}}

QFrame#theme_hub_header QLabel {{
    background: transparent;
    color: #2e343b;
    font-size: 11px;
}}

QLabel#theme_hub_status,
QLabel#theme_hub_diff {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:1 #eeeeee);
    border: 1px solid #b8bec6;
    border-top-color: #d1d6dc;
    border-radius: 2px;
    padding: 5px 7px;
    color: #4a4a4a;
}}

QListWidget#game_list,
QListWidget#photo_list {{
    background: #ffffff;
    border: 1px solid #b8bcc2;
    font-size: 11px;
    outline: none;
}}

QListWidget#game_list::item,
QListWidget#photo_list::item {{
    padding: 2px 6px;
    min-height: 18px;
}}

QListWidget#game_list::item:selected,
QListWidget#photo_list::item:selected {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 {TABLE_SEL_GRAD_TOP}, stop:1 {TABLE_SEL_GRAD_BOT});
    color: {TABLE_SEL_TEXT};
}}

QLabel#game_cover_preview {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.52 #f3f3f3, stop:1 #dfdfdf);
    border: 1px solid #aeb5bd;
    border-top-color: #cfd4da;
    border-radius: 3px;
    padding: 4px;
    color: #666666;
}}

QLabel#game_warning_strip {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fff8da, stop:1 #f3e2a7);
    border: 1px solid #ccb56a;
    padding: 4px 6px;
    color: #5b4a12;
    font-weight: bold;
}}

QLabel#theme_preview {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:1 #eeeeee);
    border: 1px solid #aeb5bd;
    border-radius: 3px;
    color: #666666;
}}

/* ── Music Store / iTunes Store 2006-2008 shell ─────────────────── */
QWidget#browser_panel {{
    background: #d6dce5;
}}

QFrame#itunes_store_shell {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #edf1f6, stop:0.18 #dce4ee, stop:1 #c8d1dd);
    border-left: 1px solid #9da8b5;
}}

QFrame#itunes_store_nav {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fbfbfb, stop:0.08 #f2f2f2, stop:0.48 #d7d7d7, stop:0.5 #c5c5c5, stop:1 #aeb2b8);
    border: 1px solid #7d8792;
    border-radius: 4px;
}}

QFrame#itunes_store_tabs {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f4f6f9, stop:0.52 #d8e0ea, stop:0.54 #c4cedb, stop:1 #b1bdca);
    border: 1px solid #8a96a4;
    border-radius: 4px;
}}

QLabel#itunes_store_title {{
    color: #2a3541;
    font-size: 12px;
    font-weight: bold;
    padding-left: 8px;
    padding-right: 8px;
    background: transparent;
}}

QPushButton#store_nav_button,
QPushButton#store_buy_button {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.45 #e9edf2, stop:0.48 #d6dde6, stop:1 #aeb9c7);
    border: 1px solid #7b8795;
    border-radius: 3px;
    color: #1d2834;
    font-size: 11px;
    font-weight: bold;
    padding: 2px 8px;
    min-height: 18px;
}}

QPushButton#store_buy_button {{
    color: #ffffff;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #9ac2f0, stop:0.46 #5f93d2, stop:0.5 #3c77bd, stop:1 #255e9e);
    border: 1px solid #315f93;
}}

QPushButton#store_nav_button:pressed,
QPushButton#store_buy_button:pressed {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #8d99a8, stop:1 #c8d1dc);
}}

QPushButton#store_nav_button:checked,
QPushButton#store_nav_button[active="true"] {{
    color: #ffffff;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #7e8fa5, stop:0.47 #586c85, stop:0.5 #465a73, stop:1 #6f8198);
    border: 1px solid #46566a;
}}

QLineEdit#itunes_store_search,
QLineEdit#itunes_store_import_url {{
    background: #ffffff;
    border: 1px solid #7d8792;
    border-radius: 10px;
    padding: 2px 9px;
    selection-background-color: #7ca4d2;
    min-height: 18px;
}}

QFrame#itunes_store_import_bar {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f9fbfd, stop:1 #d9e2ec);
    border: 1px solid #a5b1bf;
    border-radius: 4px;
}}

QLabel#itunes_store_small_title,
QLabel#itunes_store_section_title {{
    background: transparent;
    color: #2f3b48;
    font-size: 11px;
    font-weight: bold;
}}

QScrollArea#itunes_store_scroll,
QWidget#itunes_store_page {{
    background: #ffffff;
}}

QFrame#itunes_store_hero {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #eef5ff, stop:0.45 #d8e8fb, stop:0.48 #c8d9ee, stop:1 #a7bad3);
    border: 1px solid #8b9eb7;
}}

QLabel#itunes_store_kicker {{
    background: transparent;
    color: #44698f;
    font-size: 11px;
    font-weight: bold;
}}

QLabel#itunes_store_headline {{
    background: transparent;
    color: #16293d;
    font-size: 20px;
    font-weight: bold;
}}

QLabel#itunes_store_subhead {{
    background: transparent;
    color: #35485a;
    font-size: 12px;
}}

QLabel#itunes_store_cover,
QLabel#itunes_store_album_art,
QLabel#itunes_store_movie_thumbnail {{
    color: #ffffff;
    font-size: 15px;
    font-weight: bold;
    border: 1px solid #6f7a86;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #9bb2c9, stop:0.48 #506d8e, stop:0.5 #354f6e, stop:1 #17283b);
}}

QLabel#itunes_store_movie_thumbnail {{
    font-size: 13px;
}}

QFrame#itunes_store_panel,
QFrame#itunes_store_sidebar_panel,
QFrame#itunes_store_web_frame {{
    background: #ffffff;
    border: 1px solid #aeb6c1;
}}

QFrame#itunes_store_album_tile {{
    background: #ffffff;
    border: none;
    min-width: 106px;
}}

QFrame#itunes_store_album_tile:hover {{
    background: #eef4fb;
}}

QFrame#itunes_store_album_detail {{
    background: #ffffff;
    border: 1px solid #b2bcc8;
}}

QFrame#itunes_store_detail_bar {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #f7f8fa, stop:0.48 #e1e5ea, stop:0.5 #cfd6df, stop:1 #b9c3cf);
    border-bottom: 1px solid #9da8b5;
}}

QLabel#itunes_store_detail_art {{
    color: #ffffff;
    font-size: 24px;
    font-weight: bold;
    border: 1px solid #6f7a86;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #9bb2c9, stop:0.48 #506d8e, stop:0.5 #354f6e, stop:1 #17283b);
}}

QLabel#itunes_store_detail_title {{
    background: transparent;
    color: #111111;
    font-size: 20px;
    font-weight: bold;
}}

QLabel#itunes_store_detail_artist {{
    background: transparent;
    color: #1d3d6f;
    font-size: 13px;
    font-weight: bold;
}}

QLabel#itunes_store_detail_meta {{
    background: transparent;
    color: #626a72;
    font-size: 11px;
}}

QFrame#itunes_store_detail_table {{
    background: #ffffff;
    border: 1px solid #ccd3db;
}}

QFrame#itunes_store_detail_row {{
    background: #ffffff;
    border-bottom: 1px solid #e5e9ee;
}}

QLabel#itunes_store_detail_key {{
    background: transparent;
    color: #626a72;
    font-size: 11px;
    font-weight: bold;
    min-width: 70px;
}}

QLabel#itunes_store_detail_value {{
    background: transparent;
    color: #202a36;
    font-size: 11px;
}}

QLabel#itunes_store_album_title {{
    background: transparent;
    color: #1d3d6f;
    font-size: 11px;
    font-weight: bold;
}}

QLabel#itunes_store_album_subtitle {{
    background: transparent;
    color: #626a72;
    font-size: 10px;
}}

QPushButton#itunes_store_chart_row {{
    text-align: left;
    background: #ffffff;
    border: none;
    border-bottom: 1px solid #e1e5ea;
    color: #1d3d6f;
    font-size: 11px;
    padding: 3px 4px;
}}

QPushButton#itunes_store_chart_row:hover {{
    background: #eef4fb;
}}

QTreeWidget#itunes_store_downloads {{
    background: #ffffff;
    alternate-background-color: #f6f8fb;
    border: 1px solid #9faab7;
    font-size: 11px;
}}

QTreeWidget {{
    background: #ffffff;
    alternate-background-color: #f6f8fb;
    border: 1px solid #b8bcc2;
}}
"""

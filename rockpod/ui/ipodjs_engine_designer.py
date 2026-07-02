"""Designer for RockPod's native iPod-style Rockbox engine."""

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor, QFont, QLinearGradient, QPainter, QPen
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSizePolicy,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)


ACCENTS = [
    ("blue", "Blue", QColor("#005cc0")),
    ("graphite", "Graphite", QColor("#545a64")),
    ("u2", "U2 Red", QColor("#b61823")),
    ("teal", "Teal", QColor("#008084")),
    ("green", "Green", QColor("#378e40")),
    ("gold", "Gold", QColor("#b88726")),
    ("orange", "Orange", QColor("#d06820")),
    ("purple", "Purple", QColor("#7152aa")),
    ("pink", "Pink", QColor("#c34880")),
]


def _accent_color(key):
    for value, _label, color in ACCENTS:
        if value == key:
            return color
    return ACCENTS[0][2]


def _blend(a, b, amount):
    return QColor(
        int(a.red() * (1.0 - amount) + b.red() * amount),
        int(a.green() * (1.0 - amount) + b.green() * amount),
        int(a.blue() * (1.0 - amount) + b.blue() * amount),
    )


class IPodJSPreview(QFrame):
    """Scalable preview of the native iPod engine surfaces."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self._options = {}
        self._screen = "home"
        self.setMinimumSize(430, 330)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setObjectName("ipodjs_preview")

    def set_options(self, options):
        self._options = dict(options or {})
        self.update()

    def set_screen(self, screen):
        self._screen = str(screen or "home")
        self.update()

    def _opt(self, key, default):
        return self._options.get(key, default)

    def _palette(self):
        dark = bool(self._opt("rockbox_ui_dark_mode", False))
        surface = self._opt("rockbox_ui_surface", "solid")
        accent = _accent_color(self._opt("rockbox_ui_accent", "blue"))

        if dark:
            panel = QColor("#181b20")
            row = QColor("#181b20")
            preview = QColor("#151922")
            text = QColor("#eff2f6")
            muted = QColor("#a6adb8")
            header_top = QColor("#303846")
            header_bottom = QColor("#10141a")
            split = QColor("#363c46")
        else:
            panel = {
                "solid": QColor("#ffffff"),
                "soft": QColor("#eceef1"),
                "transparent": QColor("#f8f9fa"),
            }.get(surface, QColor("#ffffff"))
            row = QColor("#ffffff")
            preview = QColor("#5e6674")
            text = QColor("#000000")
            muted = QColor("#64676b")
            header_top = QColor("#fcfdfd")
            header_bottom = QColor("#aeb2b7")
            split = QColor("#d2d2d2")

        return {
            "accent": accent,
            "panel": panel,
            "row": row,
            "preview": preview,
            "text": text,
            "muted": muted,
            "header_top": header_top,
            "header_bottom": header_bottom,
            "split": split,
            "dark": dark,
        }

    def _font(self, scale, bold=True):
        size = {
            "small": 9,
            "normal": 10,
            "large": 12,
        }.get(self._opt("rockbox_ui_font_scale", "normal"), 10)
        font = QFont("Helvetica", max(7, int(size * scale)))
        font.setBold(bold)
        return font

    def _rect(self):
        rect = self.rect().adjusted(18, 18, -18, -18)
        scale = min(rect.width() / 320.0, rect.height() / 240.0)
        width = int(320 * scale)
        height = int(240 * scale)
        x = rect.x() + (rect.width() - width) // 2
        y = rect.y() + (rect.height() - height) // 2
        return x, y, width, height, scale

    def _gradient_rect(self, p, x, y, w, h, top, bottom):
        grad = QLinearGradient(0, y, 0, y + h)
        grad.setColorAt(0.0, top)
        grad.setColorAt(1.0, bottom)
        p.fillRect(x, y, w, h, grad)

    def _draw_header(self, p, x, y, w, scale, pal, title="iPod"):
        h = int(20 * scale)
        self._gradient_rect(p, x, y, w, h, pal["header_top"], pal["header_bottom"])
        p.setPen(QPen(pal["split"], 1))
        p.drawLine(x, y + h, x + w, y + h)
        p.setFont(self._font(scale, True))
        p.setPen(pal["text"])
        p.drawText(x + int(7 * scale), y + int(14 * scale), title)
        p.fillRect(x + w - int(46 * scale), y + int(5 * scale), int(29 * scale), int(10 * scale), QColor("#54585d"))
        p.fillRect(x + w - int(44 * scale), y + int(7 * scale), int(20 * scale), int(6 * scale), QColor("#a5e07f"))
        p.fillRect(x + w - int(16 * scale), y + int(8 * scale), int(3 * scale), int(4 * scale), QColor("#c4c4c4"))

    def _draw_home(self, p, x, y, w, h, scale, pal):
        header_h = int(20 * scale)
        list_w = int(145 * scale)
        row_h = int((20 if self._opt("rockbox_ui_density", "comfortable") == "compact" else 24) * scale)
        labels = ["Music", "Videos", "Photos", "Extras", "Settings"]

        p.fillRect(x, y, w, h, pal["panel"])
        self._draw_header(p, x, y, list_w, scale, pal)
        p.fillRect(x, y + header_h + 1, list_w, h - header_h, pal["row"])
        p.setPen(QPen(pal["split"], 1))
        p.drawLine(x + list_w, y, x + list_w, y + h)

        p.setFont(self._font(scale, True))
        for index, label in enumerate(labels):
            row_y = y + header_h + index * row_h
            if index == 0:
                accent = pal["accent"]
                self._gradient_rect(p, x, row_y, list_w, row_h,
                                    _blend(accent, QColor("#ffffff"), 0.38),
                                    _blend(accent, QColor("#000000"), 0.18))
                p.setPen(QColor("#ffffff"))
            else:
                p.fillRect(x, row_y, list_w, row_h, pal["row"])
                p.setPen(pal["text"])
            p.drawText(x + int(8 * scale), row_y + int(16 * scale), label)

        right_x = x + list_w + 1
        preview_top = QColor("#b9c0ca") if not pal["dark"] else QColor("#333946")
        preview_bottom = pal["preview"]
        self._gradient_rect(p, right_x, y, w - list_w - 1, h, preview_top, preview_bottom)
        self._draw_cover_stack(p, right_x, y, w - list_w - 1, h, scale, pal)
        p.setPen(QColor("#ffffff"))
        p.setFont(self._font(scale, True))
        p.drawText(right_x + int(58 * scale), y + int(154 * scale), "Music")

    def _draw_cover_stack(self, p, x, y, w, h, scale, pal):
        accent = pal["accent"]
        cx = x + w // 2
        cy = y + int(86 * scale)
        for offset, alpha in [(-34, 120), (34, 120), (0, 255)]:
            color = QColor(accent)
            color.setAlpha(alpha)
            p.fillRect(cx - int(34 * scale) + int(offset * scale),
                       cy - int(34 * scale), int(68 * scale), int(68 * scale),
                       color)
            p.setPen(QPen(QColor("#ffffff"), 2))
            p.drawRect(cx - int(34 * scale) + int(offset * scale),
                       cy - int(34 * scale), int(68 * scale), int(68 * scale))

    def _draw_wps(self, p, x, y, w, h, scale, pal):
        p.fillRect(x, y, w, h, pal["panel"])
        self._draw_header(p, x, y, w, scale, pal, "Now Playing")
        art_x = x + int(38 * scale)
        art_y = y + int(55 * scale)
        art = int(96 * scale)
        self._gradient_rect(p, art_x, art_y, art, art,
                            QColor("#f2f2f2"), QColor("#cfd3d8"))
        p.setPen(QPen(QColor("#ffffff"), 3))
        p.drawRect(art_x, art_y, art, art)
        p.setPen(pal["muted"])
        p.setFont(self._font(scale, True))
        p.drawText(art_x + int(26 * scale), art_y + int(55 * scale), "♪")

        info_x = x + int(152 * scale)
        p.setPen(pal["text"])
        p.drawText(info_x, y + int(76 * scale), "Track Title")
        p.setPen(pal["muted"])
        p.drawText(info_x, y + int(98 * scale), "Artist")
        p.drawText(info_x, y + int(118 * scale), "Album")
        p.setPen(pal["accent"])
        p.drawText(info_x, y + int(150 * scale), "Playing")

        bar_x = x + int(58 * scale)
        bar_y = y + int(190 * scale)
        bar_w = int(204 * scale)
        p.fillRect(bar_x, bar_y, bar_w, int(13 * scale), QColor("#303845") if pal["dark"] else QColor("#dfe3e9"))
        p.fillRect(bar_x + 1, bar_y + 1, int(82 * scale), int(11 * scale), pal["accent"])
        p.setPen(pal["muted"])
        p.drawText(x + int(20 * scale), y + int(224 * scale), "0:42")
        p.drawText(x + int(260 * scale), y + int(224 * scale), "3:28")

    def _draw_lock(self, p, x, y, w, h, scale, pal):
        top = QColor("#f7f8f9") if not pal["dark"] else QColor("#353d49")
        bottom = QColor("#747e8c") if not pal["dark"] else QColor("#10151d")
        self._gradient_rect(p, x, y, w, h, top, bottom)
        self._draw_header(p, x, y, w, scale, pal, "Hold")
        p.setFont(QFont("Helvetica", int(34 * scale), QFont.Bold))
        p.setPen(QColor("#262b32") if not pal["dark"] else QColor("#f3f6f8"))
        p.drawText(x + int(98 * scale), y + int(92 * scale), "12:41")
        p.setFont(self._font(scale, True))
        p.drawText(x + int(96 * scale), y + int(122 * scale), "Thursday, July 2")
        p.setPen(pal["accent"])
        p.drawText(x + int(120 * scale), y + int(164 * scale), "Clear")
        p.setPen(QColor("#39404a") if not pal["dark"] else QColor("#d4d9e0"))
        p.drawText(x + int(124 * scale), y + int(186 * scale), "Battery 82%")

    def paintEvent(self, event):
        super().paintEvent(event)
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, False)
        x, y, w, h, scale = self._rect()
        pal = self._palette()

        if self._screen == "wps":
            self._draw_wps(p, x, y, w, h, scale, pal)
        elif self._screen == "lock":
            self._draw_lock(p, x, y, w, h, scale, pal)
        else:
            self._draw_home(p, x, y, w, h, scale, pal)
            if self._opt("rockbox_ui_hold_effect", "lockscreen") == "lockscreen":
                overlay = QColor(32, 37, 46, 205)
            else:
                overlay = QColor(54, 58, 66, 185)
            p.fillRect(x + int(18 * scale), y + int(82 * scale),
                       w - int(36 * scale), int(76 * scale), overlay)
            p.setPen(QColor("#ffffff"))
            p.setFont(self._font(scale, True))
            p.drawText(x + int(142 * scale), y + int(114 * scale), "Hold")
        p.end()


class IPodJSEngineDesignerWidget(QWidget):
    """Configuration workspace for the native iPod engine."""

    apply_device_requested = Signal(dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("ipodjs_engine_designer")

        root = QHBoxLayout(self)
        root.setContentsMargins(14, 14, 14, 14)
        root.setSpacing(14)

        left = QFrame()
        left.setObjectName("DesignerSection")
        left.setMaximumWidth(390)
        controls = QGridLayout(left)
        controls.setContentsMargins(14, 14, 14, 14)
        controls.setHorizontalSpacing(10)
        controls.setVerticalSpacing(10)

        title = QLabel("iPod Engine")
        title.setObjectName("SectionTitle")
        title.setStyleSheet("font-weight: 700; font-size: 16px;")
        controls.addWidget(title, 0, 0, 1, 2)

        subtitle = QLabel("Design a stock Apple-style iPodJS surface for menus, playback, and hold screens.")
        subtitle.setWordWrap(True)
        subtitle.setStyleSheet("color: palette(mid);")
        controls.addWidget(subtitle, 1, 0, 1, 2)

        self._accent = self._combo([label for _key, label, _color in ACCENTS])
        self._dark_mode = QCheckBox("Dark mode")
        self._density = self._combo(["Comfortable", "Compact"])
        self._font_scale = self._combo(["Small", "Normal", "Large"], 1)
        self._surface = self._combo(["Solid", "Soft", "Transparent"])
        self._hold_effect = self._combo(["Lockscreen", "Dim Overlay"])

        rows = [
            ("Accent", self._accent),
            ("Mode", self._dark_mode),
            ("Density", self._density),
            ("Font", self._font_scale),
            ("Surface", self._surface),
            ("Hold", self._hold_effect),
        ]
        for row, (label, widget) in enumerate(rows, start=2):
            controls.addWidget(QLabel(label), row, 0)
            controls.addWidget(widget, row, 1)

        swatch_row = len(rows) + 3
        controls.addWidget(QLabel("Palette"), swatch_row, 0)
        swatches = QWidget()
        swatch_layout = QHBoxLayout(swatches)
        swatch_layout.setContentsMargins(0, 0, 0, 0)
        swatch_layout.setSpacing(4)
        for index, (_key, label, color) in enumerate(ACCENTS):
            button = QPushButton()
            button.setToolTip(label)
            button.setFixedSize(22, 22)
            button.setStyleSheet(
                "QPushButton {"
                f"background: {color.name()};"
                "border: 1px solid palette(mid);"
                "}"
                "QPushButton:focus { border: 2px solid palette(text); }"
            )
            button.clicked.connect(lambda _checked=False, idx=index: self._accent.setCurrentIndex(idx))
            swatch_layout.addWidget(button)
        controls.addWidget(swatches, swatch_row, 1)

        self._tabs = QTabWidget()
        self._tabs.addTab(QWidget(), "Home")
        self._tabs.addTab(QWidget(), "WPS")
        self._tabs.addTab(QWidget(), "Lock")
        controls.addWidget(QLabel("Preview"), swatch_row + 1, 0)
        controls.addWidget(self._tabs, swatch_row + 1, 1)

        self._apply = QPushButton("Apply To Connected iPod")
        self._apply.clicked.connect(self._emit_apply)
        controls.addWidget(self._apply, swatch_row + 2, 0, 1, 2)

        self._preview = IPodJSPreview()
        root.addWidget(left)
        root.addWidget(self._preview, 1)

        for combo in (self._accent, self._density, self._font_scale, self._surface, self._hold_effect):
            combo.currentIndexChanged.connect(self._refresh_preview)
        self._dark_mode.toggled.connect(self._refresh_preview)
        self._tabs.currentChanged.connect(self._set_preview_tab)
        self._refresh_preview()

    @staticmethod
    def _combo(items, index=0):
        combo = QComboBox()
        combo.addItems(items)
        combo.setCurrentIndex(index)
        combo.setFocusPolicy(Qt.StrongFocus)
        return combo

    def _set_preview_tab(self, index):
        self._preview.set_screen({0: "home", 1: "wps", 2: "lock"}.get(index, "home"))

    def _settings(self):
        return {
            "rockbox_ui_engine": "ipodjs",
            "rockbox_ui_accent": ACCENTS[self._accent.currentIndex()][0],
            "rockbox_ui_dark_mode": bool(self._dark_mode.isChecked()),
            "rockbox_ui_density": {0: "comfortable", 1: "compact"}.get(self._density.currentIndex(), "comfortable"),
            "rockbox_ui_font_scale": {0: "small", 1: "normal", 2: "large"}.get(self._font_scale.currentIndex(), "normal"),
            "rockbox_ui_surface": {0: "solid", 1: "soft", 2: "transparent"}.get(self._surface.currentIndex(), "solid"),
            "rockbox_ui_hold_effect": {0: "lockscreen", 1: "dim"}.get(self._hold_effect.currentIndex(), "lockscreen"),
        }

    def _refresh_preview(self):
        self._preview.set_options(self._settings())

    def _emit_apply(self):
        self.apply_device_requested.emit(self._settings())

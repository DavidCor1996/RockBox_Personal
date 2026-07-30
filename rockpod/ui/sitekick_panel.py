"""YTV-styled Sitekick device screen and trade workspace."""

from __future__ import annotations

from datetime import datetime

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from services.sitekick import BACKGROUNDS, BODY_COLORS


SLOT_NAMES = (
    "Aura", "Shell / Legs", "Arms", "Face",
    "Eyes", "Hair / Helmet", "Antenna", "Accessory",
)


class SitekickPanel(QWidget):
    """Shows the authoritative Sitekick state read from a mounted iPod."""

    refresh_requested = Signal()
    trade_requested = Signal(list)
    code_requested = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("sitekick_panel")
        self.setStyleSheet(
            "#sitekick_panel { background: #f7f0f9; }"
            "#sitekickHeader { background: #4c0b64; border-bottom: 4px solid #ff700b; }"
            "#sitekickHeader QLabel { color: #ffd81f; }"
            "#sitekickStage { background: #b9dd42; border: 3px solid #4c0b64; "
            "border-radius: 8px; }"
            "#sitekickCard { background: rgba(255,255,255,218); "
            "border: 1px solid #a48aac; border-radius: 8px; }"
            "#sitekickSection { color: #4c0b64; font-size: 15px; font-weight: 700; }"
            "QPushButton { background: #ffb313; color: #2a0837; border: 1px solid #7d289d; "
            "border-radius: 5px; padding: 6px 12px; font-weight: 700; }"
            "QPushButton:hover { background: #ffd81f; }"
            "QLineEdit, QListWidget { background: rgba(255,255,255,230); "
            "border: 1px solid #a48aac; border-radius: 4px; padding: 4px; }"
        )

        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        header = QFrame()
        header.setObjectName("sitekickHeader")
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(18, 10, 18, 10)
        title = QLabel("YTV SITEKICK")
        title.setStyleSheet("font-size: 22px; font-weight: 900;")
        self._connection = QLabel("Plug in an iPod to sync")
        self._connection.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        header_layout.addWidget(title)
        header_layout.addStretch(1)
        header_layout.addWidget(self._connection)
        outer.addWidget(header)

        content = QHBoxLayout()
        content.setContentsMargins(16, 16, 16, 16)
        content.setSpacing(16)
        outer.addLayout(content, 1)

        stage = QFrame()
        stage.setObjectName("sitekickStage")
        stage_layout = QVBoxLayout(stage)
        stage_layout.setContentsMargins(10, 10, 10, 10)
        self._preview = QLabel("CURRENT SITEKICK")
        self._source_pixmap = QPixmap()
        self._preview.setAlignment(Qt.AlignCenter)
        self._preview.setMinimumSize(230, 318)
        self._preview.setStyleSheet("color: #4c0b64; font-weight: 800;")
        stage_layout.addWidget(self._preview, 1)
        self._stats = QLabel("XP 0  •  250 COINS")
        self._stats.setAlignment(Qt.AlignCenter)
        self._stats.setStyleSheet(
            "background: rgba(247,240,249,220); color: #2a0837; "
            "border-radius: 5px; padding: 7px; font-weight: 700;"
        )
        stage_layout.addWidget(self._stats)
        content.addWidget(stage, 0)

        right = QVBoxLayout()
        right.setSpacing(12)
        content.addLayout(right, 1)

        loadout = QFrame()
        loadout.setObjectName("sitekickCard")
        loadout_layout = QGridLayout(loadout)
        loadout_layout.setContentsMargins(12, 12, 12, 12)
        loadout_layout.addWidget(self._section("Current Loadout"), 0, 0, 1, 2)
        self._slot_values = []
        for index, name in enumerate(SLOT_NAMES, start=1):
            label = QLabel(name)
            label.setStyleSheet("color: #796280;")
            value = QLabel("—")
            value.setStyleSheet("color: #2a0837; font-weight: 700;")
            loadout_layout.addWidget(label, index, 0)
            loadout_layout.addWidget(value, index, 1)
            self._slot_values.append(value)
        right.addWidget(loadout)

        trade = QFrame()
        trade.setObjectName("sitekickCard")
        trade_layout = QVBoxLayout(trade)
        trade_layout.setContentsMargins(12, 12, 12, 12)
        trade_layout.addWidget(self._section("Trade Center"))
        hint = QLabel(
            "Offers staged on the iPod appear here. Enter incoming chip IDs "
            "to settle the trade; RockPod sends them through the Sitekick inbox."
        )
        hint.setWordWrap(True)
        hint.setStyleSheet("color: #796280;")
        trade_layout.addWidget(hint)
        self._offers = QListWidget()
        self._offers.setMaximumHeight(92)
        trade_layout.addWidget(self._offers)
        self._incoming = QLineEdit()
        self._incoming.setPlaceholderText("Incoming chip IDs, e.g. 12, 81, 204")
        trade_layout.addWidget(self._incoming)
        buttons = QHBoxLayout()
        self._refresh = QPushButton("Sync Current Sitekick")
        self._accept = QPushButton("Accept Trade")
        self._refresh.clicked.connect(self.refresh_requested)
        self._accept.clicked.connect(self._emit_trade)
        buttons.addWidget(self._refresh)
        buttons.addWidget(self._accept)
        buttons.addStretch(1)
        trade_layout.addLayout(buttons)
        right.addWidget(trade)

        codes = QFrame()
        codes.setObjectName("sitekickCard")
        codes_layout = QVBoxLayout(codes)
        codes_layout.setContentsMargins(12, 12, 12, 12)
        codes_layout.addWidget(self._section("Secret Codes"))
        code_hint = QLabel(
            "Enter a code to send its chips to the connected iPod. "
            "They unlock the next time Sitekick opens."
        )
        code_hint.setWordWrap(True)
        code_hint.setStyleSheet("color: #796280;")
        codes_layout.addWidget(code_hint)
        code_row = QHBoxLayout()
        self._code = QLineEdit()
        self._code.setPlaceholderText("Secret code")
        self._redeem = QPushButton("Unlock & Sync")
        self._code.returnPressed.connect(self._emit_code)
        self._redeem.clicked.connect(self._emit_code)
        code_row.addWidget(self._code, 1)
        code_row.addWidget(self._redeem)
        codes_layout.addLayout(code_row)
        right.addWidget(codes)
        right.addStretch(1)
        self.set_disconnected()

    @staticmethod
    def _section(text):
        label = QLabel(text)
        label.setObjectName("sitekickSection")
        return label

    def set_disconnected(self):
        self._connection.setText("Plug in an iPod to sync")
        self._preview.clear()
        self._source_pixmap = QPixmap()
        self._preview.setText("CURRENT SITEKICK\n\nWaiting for iPod…")
        self._stats.setText("XP —  •  — COINS")
        self._offers.clear()
        for value in self._slot_values:
            value.setText("—")
        self._refresh.setEnabled(False)
        self._accept.setEnabled(False)
        self._code.setEnabled(False)
        self._redeem.setEnabled(False)

    def set_error(self, message):
        self._connection.setText("Sitekick sync unavailable")
        self._preview.clear()
        self._source_pixmap = QPixmap()
        self._preview.setText(str(message or "No Sitekick data found"))
        self._refresh.setEnabled(True)
        self._accept.setEnabled(False)
        self._code.setEnabled(False)
        self._redeem.setEnabled(False)

    def set_snapshot(self, snapshot):
        state = snapshot.state
        self._connection.setText(
            f"Synced {datetime.now().strftime('%-I:%M %p')}  •  "
            f"{len(state.owned)} chips"
        )
        pixmap = QPixmap(str(snapshot.preview_path))
        if not pixmap.isNull():
            self._source_pixmap = pixmap
            self._update_preview_pixmap()
        color = BODY_COLORS[state.body_color][0]
        background = BACKGROUNDS[state.background][0]
        self._stats.setText(
            f"XP {state.xp:,}  •  {state.coins:,} COINS  •  "
            f"{color} / {background}"
        )
        for index, value in enumerate(self._slot_values):
            chip_id = state.equipped[index]
            chip = snapshot.chips.get(chip_id) if chip_id is not None else None
            value.setText(f"{chip.name}  #{chip.id:04d}" if chip else "—")
        self._offers.clear()
        if snapshot.offers:
            for offer in snapshot.offers:
                chip = snapshot.chips.get(offer.chip_id)
                name = offer.name or (chip.name if chip else "Unknown chip")
                self._offers.addItem(f"#{offer.chip_id:04d}  {name}")
        else:
            self._offers.addItem("No trade staged on this iPod")
        self._refresh.setEnabled(True)
        self._accept.setEnabled(bool(snapshot.offers))
        self._code.setEnabled(True)
        self._redeem.setEnabled(True)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._update_preview_pixmap()

    def _update_preview_pixmap(self):
        if not self._source_pixmap.isNull():
            self._preview.setPixmap(
                self._source_pixmap.scaled(
                    self._preview.size(), Qt.KeepAspectRatio,
                    Qt.SmoothTransformation,
                )
            )

    def _emit_trade(self):
        incoming = []
        for part in self._incoming.text().replace(";", ",").split(","):
            part = part.strip()
            if part.isdigit():
                incoming.append(int(part))
        self.trade_requested.emit(incoming)

    def _emit_code(self):
        code = self._code.text().strip()
        if not code:
            return
        self._code.clear()
        self.code_requested.emit(code)

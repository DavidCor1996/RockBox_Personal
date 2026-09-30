"""Wiggle-mode application arrangement with an explicit device save."""
import math

from PySide6.QtCore import Qt, QSize, QTimer, Signal, QMimeData
from PySide6.QtGui import QColor, QIcon, QPen, QPainterPath, QPixmap, QDrag
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QListWidget,
    QListWidgetItem, QAbstractItemView, QStyledItemDelegate, QStyle, QMessageBox,
)
from services.application_layout import APPLICATIONS, normalize_order


class ApplicationDelegate(QStyledItemDelegate):
    def sizeHint(self, option, index):
        return QSize(104, 116)

    def paint(self, painter, option, index):
        grid = self.parent()
        painter.save()
        if option.state & QStyle.State_Selected:
            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor('#dcecff'))
            painter.drawRoundedRect(option.rect.adjusted(3, 3, -3, -3), 8, 8)
        center = option.rect.center()
        painter.save()
        painter.translate(center.x(), option.rect.top() + 40)
        if grid.editing:
            painter.rotate(2.5 * math.sin(grid.phase + index.row() * 1.8))
        icon = index.data(Qt.DecorationRole)
        if icon:
            clip = QPainterPath()
            clip.addRoundedRect(-32, -32, 64, 64, 12, 12)
            painter.setClipPath(clip)
            icon.paint(painter, -32, -32, 64, 64)
        painter.restore()
        painter.setPen(QPen(QColor('#26364b')))
        painter.drawText(option.rect.adjusted(3, 76, -3, -4),
                         Qt.AlignHCenter | Qt.AlignTop | Qt.TextWordWrap,
                         index.data(Qt.DisplayRole))
        painter.restore()


class ApplicationGrid(QListWidget):
    order_changed = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.editing = False
        self.phase = 0
        self.dragged_id = None
        self.setViewMode(QListWidget.IconMode)
        self.setFlow(QListWidget.LeftToRight)
        self.setWrapping(True)
        self.setResizeMode(QListWidget.Adjust)
        self.setGridSize(QSize(104, 116))
        self.setFixedWidth(440)
        self.setMinimumHeight(370)
        self.setSelectionMode(QAbstractItemView.SingleSelection)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.setItemDelegate(ApplicationDelegate(self))
        self.timer = QTimer(self)
        self.timer.setInterval(60)
        self.timer.timeout.connect(self._wiggle)
        self.set_editing(False)

    def _wiggle(self):
        self.phase += .65
        self.viewport().update()

    def set_editing(self, editing):
        self.editing = editing
        self.setDragEnabled(editing)
        self.setAcceptDrops(editing)
        self.setDragDropMode(QAbstractItemView.InternalMove if editing
                            else QAbstractItemView.NoDragDrop)
        self.setDefaultDropAction(Qt.MoveAction)
        self.timer.start() if editing and self.isVisible() else self.timer.stop()
        self.viewport().update()

    def hideEvent(self, event):
        self.timer.stop()
        super().hideEvent(event)

    def showEvent(self, event):
        super().showEvent(event)
        if self.editing:
            self.timer.start()

    def move_item(self, source, target):
        if not self.editing or not 0 <= source < self.count():
            return
        target = max(0, min(target, self.count() - 1))
        if source == target:
            return
        item = self.takeItem(source)
        self.insertItem(target, item)
        self.setCurrentItem(item)
        self.order_changed.emit()

    def startDrag(self, supported_actions):
        if not self.editing or self.currentItem() is None:
            return
        # Own the move ourselves so Qt does not remove the source row again
        # after dropEvent has already inserted it in its new position.
        self.dragged_id = self.currentItem().data(Qt.UserRole)
        mime = QMimeData()
        mime.setData('application/x-rockpod-app-id', self.dragged_id.encode())
        drag = QDrag(self)
        drag.setMimeData(mime)
        drag.setPixmap(self.currentItem().icon().pixmap(64, 64))
        try:
            drag.exec(Qt.MoveAction)
        finally:
            self.dragged_id = None

    def dragEnterEvent(self, event):
        if event.source() is self and self.editing:
            event.setDropAction(Qt.MoveAction)
            event.accept()
        else:
            event.ignore()

    def dragMoveEvent(self, event):
        super().dragMoveEvent(event)  # retain edge auto-scrolling
        self.dragEnterEvent(event)

    def dropEvent(self, event):
        if event.source() is not self or not self.editing:
            event.ignore()
            return
        target = self.indexAt(event.position().toPoint()).row()
        source = (self.ids().index(self.dragged_id) if self.dragged_id
                  else self.currentRow())
        self.move_item(source, target if target >= 0 else self.count() - 1)
        event.setDropAction(Qt.MoveAction)
        event.accept()

    def keyPressEvent(self, event):
        if self.editing and event.modifiers() & Qt.ControlModifier:
            delta = {Qt.Key_Left: -1, Qt.Key_Right: 1, Qt.Key_Up: -4,
                     Qt.Key_Down: 4}.get(event.key())
            if delta is not None:
                self.move_item(self.currentRow(), self.currentRow() + delta)
                event.accept()
                return
        super().keyPressEvent(event)

    def ids(self):
        return [self.item(i).data(Qt.UserRole) for i in range(self.count())]


class ApplicationsPanel(QWidget):
    def __init__(self, service, device_provider, parent=None):
        super().__init__(parent)
        self.service = service
        self.device_provider = device_provider
        self.mount = None
        self.dirty = False
        layout = QVBoxLayout(self)
        title = QLabel('Applications')
        title.setStyleSheet('font-size:24px; font-weight:bold')
        layout.addWidget(title)
        note = QLabel('Arrange the apps on your iPod. Turn on wiggle mode, then drag '
                      'icons into order. Four icons per row; twelve per iPod page. '
                      'You can also use Ctrl + arrow keys to move the selected app.')
        note.setWordWrap(True)
        layout.addWidget(note)
        buttons = QHBoxLayout()
        self.arrange = QPushButton('Arrange icons')
        self.arrange.setCheckable(True)
        self.arrange.toggled.connect(self._set_editing)
        buttons.addWidget(self.arrange)
        self.reset_button = QPushButton('Default order')
        self.reset_button.clicked.connect(self.reset_order)
        buttons.addWidget(self.reset_button)
        self.save_button = QPushButton('Save to iPod')
        self.save_button.clicked.connect(self.save)
        buttons.addWidget(self.save_button)
        buttons.addStretch()
        layout.addLayout(buttons)
        self.grid = ApplicationGrid()
        self.grid.order_changed.connect(self._changed)
        layout.addWidget(self.grid, 1, Qt.AlignHCenter)
        self.status = QLabel('Connect an iPod Classic or Video to arrange its apps.')
        self.status.setWordWrap(True)
        layout.addWidget(self.status)
        self.refresh()

    def _set_editing(self, editing):
        self.grid.set_editing(editing)
        self.arrange.setText('Done arranging' if editing else 'Arrange icons')

    def _changed(self):
        self.dirty = True
        self.save_button.setEnabled(bool(self.mount))
        self.status.setText('Order changed. Save to iPod when you are finished.')

    def _populate(self, order):
        names = dict(APPLICATIONS)
        self.grid.clear()
        for key in order:
            pixmap = QPixmap(str(self.service.icon_path(key)))
            if not pixmap.isNull():
                pixmap = pixmap.copy(2, 2, pixmap.width() - 4,
                                     pixmap.height() - 4)
            item = QListWidgetItem(QIcon(pixmap), names[key])
            item.setData(Qt.UserRole, key)
            item.setToolTip(names[key] + (' — shown while docked' if key == 'desktop-mode' else ''))
            self.grid.addItem(item)

    def refresh(self):
        device = self.device_provider()
        mount = getattr(device, 'mount_path', None) if device else None
        # Preserve unsaved edits when the current page refreshes for the same
        # connected device, but never transfer a draft to a different iPod.
        if mount and str(mount) == self.mount and self.dirty:
            return
        try:
            order = self.service.load(mount)
            visible = self.service.visible_ids(mount)
        except (ValueError, OSError) as exc:
            self.mount = None
            self.grid.clear()
            self.arrange.setChecked(False)
            self.status.setText(str(exc))
        else:
            self.mount = str(mount)
            self._populate([key for key in order if key in visible])
            self.status.setText('Ready. Saved order applies to the Applications grid and list. '
                                'Desktop Mode appears on the iPod only while docked.')
        self.dirty = False
        self.arrange.setEnabled(bool(self.mount))
        self.reset_button.setEnabled(bool(self.mount))
        self.save_button.setEnabled(False)

    def reset_order(self):
        self._populate([key for key in normalize_order([]) if key in self.grid.ids()])
        self._changed()

    def save(self):
        device = self.device_provider()
        mount = getattr(device, 'mount_path', None) if device else None
        try:
            if not mount or str(mount) != self.mount:
                raise ValueError('Reconnect the same iPod before saving this layout.')
            self.service.save(mount, self.grid.ids())
        except (ValueError, OSError) as exc:
            QMessageBox.warning(self, 'Applications', str(exc))
            return
        self.dirty = False
        self.save_button.setEnabled(False)
        self.arrange.setChecked(False)
        self.status.setText('Saved to iPod. Safely eject, then reopen Applications on the iPod.')

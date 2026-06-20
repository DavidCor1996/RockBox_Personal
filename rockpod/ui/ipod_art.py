"""Shared iPod illustration helpers for RockPod UI."""

from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QColor, QLinearGradient, QPainter, QPainterPath, QPen, QPixmap


def plugged_ipod_pixmap(size=72, connected=True, asset_path=""):
    """Return a small iPod Video-style device illustration with a dock cable."""
    size = max(int(size or 72), 48)
    if asset_path:
        themed = QPixmap(str(asset_path))
        if not themed.isNull():
            return themed.scaled(size, size, Qt.KeepAspectRatio, Qt.SmoothTransformation)

    px = QPixmap(size, size)
    px.fill(Qt.transparent)
    painter = QPainter(px)
    painter.setRenderHint(QPainter.Antialiasing, True)
    draw_plugged_ipod(painter, QRectF(0, 0, size, size), connected=connected)
    painter.end()
    return px


def draw_plugged_ipod(painter, bounds, connected=True, asset_path=""):
    """Draw a classic white iPod with screen, click wheel, and dock cable."""
    bounds = QRectF(bounds)
    if asset_path:
        themed = QPixmap(str(asset_path))
        if not themed.isNull():
            scaled = themed.scaled(
                int(bounds.width()),
                int(bounds.height()),
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation,
            )
            target = QRectF(
                bounds.center().x() - scaled.width() / 2.0,
                bounds.center().y() - scaled.height() / 2.0,
                scaled.width(),
                scaled.height(),
            )
            painter.drawPixmap(target.toRect(), scaled)
            return

    scale = min(bounds.width() / 72.0, bounds.height() / 72.0)
    body_w = 32.0 * scale
    body_h = 56.0 * scale
    body_x = bounds.center().x() - body_w / 2.0
    body_y = bounds.top() + 4.0 * scale
    body = QRectF(body_x, body_y, body_w, body_h)

    shadow = body.translated(0, 2.2 * scale)
    painter.setPen(Qt.NoPen)
    painter.setBrush(QColor(0, 0, 0, 34))
    painter.drawRoundedRect(shadow, 5.5 * scale, 5.5 * scale)

    body_grad = QLinearGradient(body.topLeft(), body.bottomRight())
    body_grad.setColorAt(0.0, QColor("#ffffff"))
    body_grad.setColorAt(0.52, QColor("#f4f4f4"))
    body_grad.setColorAt(1.0, QColor("#d8d8d8"))
    painter.setPen(QPen(QColor("#8f8f8f"), max(1.0, 1.0 * scale)))
    painter.setBrush(body_grad)
    painter.drawRoundedRect(body, 5.5 * scale, 5.5 * scale)

    gloss = QRectF(body.left() + 3 * scale, body.top() + 3 * scale, body.width() - 6 * scale, 17 * scale)
    painter.setPen(Qt.NoPen)
    painter.setBrush(QColor(255, 255, 255, 125))
    painter.drawRoundedRect(gloss, 4 * scale, 4 * scale)

    screen = QRectF(body.left() + 5 * scale, body.top() + 7 * scale, body.width() - 10 * scale, 19 * scale)
    screen_grad = QLinearGradient(screen.topLeft(), screen.bottomRight())
    screen_grad.setColorAt(0.0, QColor("#253345"))
    screen_grad.setColorAt(0.55, QColor("#95b3ce"))
    screen_grad.setColorAt(1.0, QColor("#d7e4ed"))
    painter.setPen(QPen(QColor("#707070"), max(1.0, 0.8 * scale)))
    painter.setBrush(screen_grad)
    painter.drawRoundedRect(screen, 1.5 * scale, 1.5 * scale)

    screen_glare = QPainterPath()
    screen_glare.moveTo(screen.left() + 1.5 * scale, screen.top() + 1.5 * scale)
    screen_glare.lineTo(screen.right() - 2 * scale, screen.top() + 1.5 * scale)
    screen_glare.lineTo(screen.left() + 7 * scale, screen.bottom() - 2 * scale)
    screen_glare.closeSubpath()
    painter.setPen(Qt.NoPen)
    painter.setBrush(QColor(255, 255, 255, 72))
    painter.drawPath(screen_glare)

    wheel_size = 21.0 * scale
    wheel = QRectF(body.center().x() - wheel_size / 2.0, body.top() + 31.5 * scale, wheel_size, wheel_size)
    wheel_grad = QLinearGradient(wheel.topLeft(), wheel.bottomRight())
    wheel_grad.setColorAt(0.0, QColor("#f6f6f6"))
    wheel_grad.setColorAt(1.0, QColor("#cccccc"))
    painter.setPen(QPen(QColor("#b0b0b0"), max(1.0, 0.8 * scale)))
    painter.setBrush(wheel_grad)
    painter.drawEllipse(wheel)

    button_size = 8.0 * scale
    button = QRectF(wheel.center().x() - button_size / 2.0, wheel.center().y() - button_size / 2.0, button_size, button_size)
    painter.setPen(QPen(QColor("#a8a8a8"), max(1.0, 0.65 * scale)))
    painter.setBrush(QColor("#f7f7f7"))
    painter.drawEllipse(button)

    dock = QRectF(body.center().x() - 7.5 * scale, body.bottom() - 2 * scale, 15 * scale, 3.5 * scale)
    painter.setPen(QPen(QColor("#8a8a8a"), max(1.0, 0.7 * scale)))
    painter.setBrush(QColor("#d5d5d5"))
    painter.drawRoundedRect(dock, 1.2 * scale, 1.2 * scale)

    if connected:
        cable_pen = QPen(QColor("#9aa0a8"), max(2.0, 2.0 * scale))
        cable_pen.setCapStyle(Qt.RoundCap)
        painter.setPen(cable_pen)
        start_x = body.center().x()
        start_y = dock.bottom() + 1 * scale
        painter.drawLine(int(start_x), int(start_y), int(start_x), int(bounds.bottom() - 5 * scale))

        plug = QRectF(start_x - 6 * scale, body.bottom() + 2.8 * scale, 12 * scale, 5 * scale)
        painter.setPen(QPen(QColor("#858b92"), max(1.0, 0.7 * scale)))
        painter.setBrush(QColor("#c8ccd0"))
        painter.drawRoundedRect(plug, 1.4 * scale, 1.4 * scale)

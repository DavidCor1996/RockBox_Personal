"""Live physical-iPod framebuffer viewer for RockPod."""

from __future__ import annotations

import logging
import socket
import struct
import subprocess
import time
from pathlib import Path

from PySide6.QtCore import QProcess, QRectF, QSize, Qt, QTimer
from PySide6.QtGui import QColor, QImage, QPainter, QPen
from PySide6.QtMultimedia import QMediaDevices
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


IPOD_FRAME_SIZE = QSize(320, 240)
DCP750_PANEL_SIZE = QSize(480, 234)
IPOD_IP = "10.77.0.2"
HOST_IP = "10.77.0.1"
FRAMEBUFFER_PORT = 47704
FRAMEBUFFER_MAGIC = b"RPF1"
FRAMEBUFFER_HEADER = struct.Struct("!4sBBHHHIII")
FRAMEBUFFER_MAX_BYTES = 320 * 240 * 2
FRAMEBUFFER_SIZES = {(320, 240), (160, 120), (120, 90)}
LOGGER = logging.getLogger(__name__)


def is_rockbox_audio_input(device):
    """Identify the physical iPod's UAC capture node exposed by PipeWire."""
    try:
        identity = f"{device.description()} {bytes(device.id()).decode(errors='ignore')}"
    except (AttributeError, TypeError, ValueError):
        return False
    identity = identity.casefold()
    return (
        "rockbox" in identity
        or "rockpod" in identity
        or "ipod classic" in identity
        or ("05ac" in identity and "1261" in identity)
    )


def rockpod_sink_input_ids(pactl_output):
    """Return Pulse/PipeWire sink-input IDs belonging to RockPod."""
    matches = []
    for block in str(pactl_output).split("Sink Input #")[1:]:
        lines = block.splitlines()
        if not lines:
            continue
        stream_id = lines[0].strip()
        if stream_id.isdigit() and (
            'media.name = "RockPod"' in block
            or 'media.name = "RockPod-Dock-Audio output"' in block
        ):
            matches.append(stream_id)
    return matches


def video_destination_rect(panel_rect, mode="4:3", overscan=0.0):
    """Return the iPod image rectangle inside the DCP750-shaped viewport."""
    panel = QRectF(panel_rect)
    if mode == "stretch":
        return panel

    source_aspect = IPOD_FRAME_SIZE.width() / IPOD_FRAME_SIZE.height()
    width = panel.height() * source_aspect
    height = panel.height()
    if width > panel.width():
        width = panel.width()
        height = width / source_aspect

    scale = max(1.0, 1.0 + float(overscan))
    width *= scale
    height *= scale
    return QRectF(
        panel.center().x() - width / 2.0,
        panel.center().y() - height / 2.0,
        width,
        height,
    )


def discover_rockpod_interface(net_root="/sys/class/net"):
    """Return the CDC-Ethernet interface belonging to the physical iPod."""
    root = Path(net_root)
    if not root.is_dir():
        return ""
    for net_path in root.iterdir():
        try:
            device = net_path.resolve()
        except OSError:
            continue
        for parent in (device, *device.parents):
            try:
                vendor = (parent / "idVendor").read_text().strip().lower()
                product = (parent / "idProduct").read_text().strip().lower()
            except OSError:
                continue
            if vendor == "05ac" and product == "1261":
                return net_path.name
    return ""


def configure_rockpod_interface(interface):
    """Give the iPod CDC-Ethernet link its fixed host address."""
    connection = f"RockPod USB Internet ({interface})"
    try:
        exists = subprocess.run(
            ("nmcli", "-t", "-f", "NAME", "connection", "show", connection),
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        ).returncode == 0
        if exists:
            command = (
                "nmcli", "connection", "modify", connection,
                "connection.interface-name", interface,
                "ipv4.method", "manual", "ipv4.addresses", "10.77.0.1/24",
                "ipv4.never-default", "yes", "ipv6.method", "disabled",
            )
        else:
            command = (
                "nmcli", "connection", "add", "type", "ethernet",
                "ifname", interface, "con-name", connection,
                "ipv4.method", "manual", "ipv4.addresses", "10.77.0.1/24",
                "ipv4.never-default", "yes", "ipv6.method", "disabled",
            )
        subprocess.run(command, check=True, capture_output=True, text=True)
        subprocess.run(
            ("nmcli", "connection", "up", connection),
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError as error:
        raise RuntimeError("NetworkManager's nmcli command is not installed") from error
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or error.stdout or "NetworkManager rejected the link").strip()
        raise RuntimeError(detail) from error
    return connection


class DockPreview(QWidget):
    """Render only pixels received from the physical iPod."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(330)
        self._frame = QImage()
        self._fit_mode = "4:3"
        self._soft_video = False
        self._safe_area = False
        self._message = "Waiting for physical iPod framebuffer"

    def sizeHint(self):
        return QSize(720, 390)

    def set_frame(self, frame):
        # Completed frames already own their pixels. QImage is implicitly
        # shared, so another deep copy here only delays the next USB request.
        self._frame = frame if not frame.isNull() else QImage()
        self.update()

    def clear_frame(self, message="Waiting for physical iPod framebuffer"):
        self._frame = QImage()
        self._message = str(message)
        self.update()

    def set_fit_mode(self, mode):
        self._fit_mode = mode
        self.update()

    def set_soft_video(self, enabled):
        self._soft_video = bool(enabled)
        self.update()

    def set_safe_area(self, enabled):
        self._safe_area = bool(enabled)
        self.update()

    def _panel_rect(self):
        available_width = max(240.0, self.width() - 32.0)
        available_height = max(130.0, self.height() - 32.0)
        aspect = DCP750_PANEL_SIZE.width() / DCP750_PANEL_SIZE.height()
        width = min(available_width, available_height * aspect)
        height = width / aspect
        return QRectF(
            (self.width() - width) / 2.0,
            (self.height() - height) / 2.0,
            width,
            height,
        )

    def paintEvent(self, event):
        del event
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor("#121417"))
        panel = self._panel_rect()
        painter.fillRect(panel, QColor("#000000"))
        painter.setPen(QPen(QColor("#343a40"), 1))
        painter.drawRect(panel)

        if self._frame.isNull():
            painter.setPen(QColor("#aab2bc"))
            painter.drawText(panel, Qt.AlignCenter, self._message)
            painter.end()
            return

        overscan = 0.06 if self._fit_mode == "overscan" else 0.0
        fit = "stretch" if self._fit_mode == "stretch" else "4:3"
        destination = video_destination_rect(panel, fit, overscan)
        painter.save()
        painter.setClipRect(panel)
        painter.setRenderHint(QPainter.SmoothPixmapTransform, self._soft_video)
        painter.drawImage(destination, self._frame)
        if self._safe_area:
            safe = destination.adjusted(
                destination.width() * 0.05,
                destination.height() * 0.05,
                -destination.width() * 0.05,
                -destination.height() * 0.05,
            )
            painter.setPen(QPen(QColor(255, 255, 255, 150), 1, Qt.DashLine))
            painter.setBrush(Qt.NoBrush)
            painter.drawRect(safe)
        painter.restore()
        painter.end()


class VideoOutPanel(QWidget):
    """View and control the framebuffer of a physical iPod over USB."""

    _REMOTE_ACTIONS = {
        "menu": 1,
        "up": 2,
        "down": 3,
        "left": 4,
        "select": 5,
        "right": 6,
        "play": 7,
    }

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("video_out_panel")
        self._socket = None
        self._interface = ""
        self._request_id = 0
        self._awaiting_frame = False
        self._request_deadline = 0.0
        self._frame_buffer = None
        self._frame_offsets = set()
        self._frame_received = 0
        self._frame_width = 0
        self._frame_height = 0
        self._frame_total = 0
        self._frame_generation = 0
        self._frame_is_video = False
        self._frame_times = []
        # Native-rate accounting. lcd_external_generation ticks once per
        # decoded frame on the iPod, and every packet header carries it, so
        # the gap between two displayed frames is exactly how many frames the
        # device produced that we never showed.
        self._last_shown_generation = 0
        self._native_deltas = []
        self._request_sent = 0.0
        self._decode_seconds = 0.0
        self._transport_ms = 0.0
        self._decode_ms = 0.0
        self._paint_ms = 0.0
        self._last_transport_log = 0.0
        self._remote_events = []
        self._auto_connect_attempted = False
        self._audio_loopback = None
        self._media_devices = QMediaDevices(self)

        self._network_timer = QTimer(self)
        self._network_timer.setInterval(2)
        self._network_timer.timeout.connect(self._poll_network)
        self._audio_retry_timer = QTimer(self)
        self._audio_retry_timer.setInterval(500)
        self._audio_retry_timer.timeout.connect(self._start_audio_monitor)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        header = QFrame()
        header.setObjectName("theme_hub_header")
        header_layout = QVBoxLayout(header)
        title = QLabel("Video Out — Live Physical iPod")
        title.setStyleSheet("font-size: 18px; font-weight: bold;")
        self._warning_label = QLabel(
            "These are real pixels from the connected iPod over USB Internet. "
            "This validates Rockbox UI mirroring, not the dock's analog CVBS electronics. "
            "Dock Display also monitors the iPod USB Audio source through the laptop speakers."
        )
        self._warning_label.setWordWrap(True)
        self._warning_label.setStyleSheet("color: #75551c;")
        header_layout.addWidget(title)
        header_layout.addWidget(self._warning_label)
        layout.addWidget(header)

        controls = QGridLayout()
        self._connect_btn = QPushButton("Connect to iPod")
        self._disconnect_btn = QPushButton("Disconnect Viewer")
        self._fit_combo = QComboBox()
        self._fit_combo.addItem("Correct 4:3 (pillarbox)", "4:3")
        self._fit_combo.addItem("Fill DCP750 panel", "stretch")
        self._fit_combo.addItem("4:3 with 6% overscan", "overscan")
        self._soft_video = QCheckBox("CVBS-style smoothing")
        self._safe_area = QCheckBox("Show 90% safe area")
        self._status_label = QLabel(
            "On the iPod choose USB Connection: Internet, then reconnect the cable."
        )
        self._status_label.setWordWrap(True)
        self._audio_status_label = QLabel("Laptop audio: waiting for Dock Display connection")
        self._audio_status_label.setWordWrap(True)
        self._connect_btn.clicked.connect(self.start_live_view)
        self._disconnect_btn.clicked.connect(self.stop_live_view)
        self._fit_combo.currentIndexChanged.connect(
            lambda: self._preview.set_fit_mode(self._fit_combo.currentData() or "4:3")
        )
        self._soft_video.toggled.connect(self._preview_soft_video_changed)
        self._safe_area.toggled.connect(self._preview_safe_area_changed)
        controls.addWidget(self._connect_btn, 0, 0)
        controls.addWidget(self._disconnect_btn, 0, 1)
        controls.addWidget(QLabel("Display:"), 0, 2)
        controls.addWidget(self._fit_combo, 0, 3)
        controls.addWidget(self._soft_video, 1, 0, 1, 2)
        controls.addWidget(self._safe_area, 1, 2, 1, 2)
        controls.addWidget(self._status_label, 2, 0, 1, 4)
        controls.addWidget(self._audio_status_label, 3, 0, 1, 4)
        controls.setColumnStretch(3, 1)
        layout.addLayout(controls)

        self._preview = DockPreview()
        layout.addWidget(self._preview, 1)

        remote_row = QHBoxLayout()
        remote_row.addWidget(QLabel("Physical iPod controls:"))
        for label, action in (
            ("Menu", "menu"), ("Wheel Up", "up"), ("Wheel Down", "down"),
            ("Previous", "left"), ("Select", "select"),
            ("Next", "right"), ("Play / Pause", "play"),
        ):
            button = QPushButton(label)
            button.clicked.connect(
                lambda checked=False, value=action: self._remote_action(value)
            )
            remote_row.addWidget(button)
        layout.addLayout(remote_row)

    def _preview_soft_video_changed(self, enabled):
        self._preview.set_soft_video(enabled)

    def _preview_safe_area_changed(self, enabled):
        self._preview.set_safe_area(enabled)

    def showEvent(self, event):
        super().showEvent(event)
        if not self._auto_connect_attempted:
            self._auto_connect_attempted = True
            QTimer.singleShot(0, self.start_live_view)

    def hideEvent(self, event):
        self.stop_live_view(clear_frame=False)
        super().hideEvent(event)

    def closeEvent(self, event):
        self.stop_live_view(clear_frame=False)
        super().closeEvent(event)

    def start_live_view(self):
        self.stop_live_view(clear_frame=True)
        interface = discover_rockpod_interface()
        if not interface:
            self._status_label.setText(
                "Physical iPod USB network not found. On the iPod select "
                "USB Connection: Internet, unplug the cable, and reconnect it."
            )
            self._preview.clear_frame("Physical iPod not in USB Internet mode")
            return False
        try:
            configure_rockpod_interface(interface)
            stream_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            stream_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF,
                                     4 * 1024 * 1024)
            stream_socket.setblocking(False)
            stream_socket.bind((HOST_IP, FRAMEBUFFER_PORT))
        except (OSError, RuntimeError) as error:
            self._status_label.setText(f"Could not open the iPod link: {error}")
            self._preview.clear_frame("USB Internet link could not be configured")
            return False

        self._interface = interface
        self._socket = stream_socket
        self._frame_generation = 0
        self._network_timer.start()
        self._status_label.setText(
            f"Connected to physical iPod on {interface}; requesting framebuffer…"
        )
        self._preview.clear_frame("Waiting for real iPod pixels…")
        self._send_frame_request()
        self._audio_retry_timer.start()
        self._start_audio_monitor()
        return True

    def stop_live_view(self, clear_frame=True):
        self._network_timer.stop()
        self._audio_retry_timer.stop()
        self._stop_audio_monitor()
        if self._socket is not None:
            try:
                self._socket.sendto(FRAMEBUFFER_MAGIC + b"\x03",
                                    (IPOD_IP, FRAMEBUFFER_PORT))
            except OSError:
                pass
            self._socket.close()
        self._socket = None
        self._awaiting_frame = False
        self._frame_buffer = None
        if clear_frame:
            self._preview.clear_frame()

    def _start_audio_monitor(self):
        if self._audio_loopback is not None or self._socket is None:
            return
        audio_input = next(
            (device for device in QMediaDevices.audioInputs()
             if is_rockbox_audio_input(device)),
            None,
        )
        if audio_input is None:
            self._audio_status_label.setText(
                "Laptop audio: waiting for the iPod USB Audio source…"
            )
            return

        output = QMediaDevices.defaultAudioOutput()
        capture_id = bytes(audio_input.id()).decode(errors="ignore")
        playback_id = bytes(output.id()).decode(errors="ignore")
        if not capture_id or not playback_id:
            self._audio_status_label.setText(
                "Laptop audio: could not identify the iPod or default speakers"
            )
            return

        loopback = QProcess(self)
        loopback.setProcessChannelMode(QProcess.MergedChannels)
        loopback.setProgram("pw-loopback")
        loopback.setArguments([
            "--name", "RockPod-Dock-Audio",
            "--latency", "50",
            "--capture", capture_id,
            "--playback", playback_id,
        ])
        loopback.finished.connect(self._audio_loopback_finished)
        loopback.start()
        if not loopback.waitForStarted(1500):
            detail = loopback.errorString() or "pw-loopback did not start"
            loopback.deleteLater()
            self._audio_status_label.setText(f"Laptop audio: {detail}")
            return

        self._audio_loopback = loopback
        self._audio_retry_timer.stop()
        self._audio_status_label.setText(
            f"Laptop audio: LIVE from {audio_input.description()}"
        )
        LOGGER.info("Monitoring physical iPod USB Audio source: %s",
                    audio_input.description())
        QTimer.singleShot(250, self._ensure_audio_monitor_unmuted)

    def _ensure_audio_monitor_unmuted(self):
        """Clear a PipeWire/Pulse stream-restore mute on RockPod only."""
        if self._audio_loopback is None:
            return
        try:
            listing = subprocess.run(
                ("pactl", "list", "sink-inputs"),
                check=True,
                capture_output=True,
                text=True,
            ).stdout
            for stream_id in rockpod_sink_input_ids(listing):
                subprocess.run(
                    ("pactl", "set-sink-input-mute", stream_id, "0"),
                    check=True,
                    capture_output=True,
                    text=True,
                )
        except (FileNotFoundError, subprocess.CalledProcessError) as error:
            LOGGER.warning("Could not clear RockPod stream mute: %s", error)

    def _audio_loopback_finished(self, exit_code, exit_status):
        del exit_code, exit_status
        loopback = self.sender()
        if loopback is self._audio_loopback:
            self._audio_loopback = None
            loopback.deleteLater()
            self._audio_status_label.setText(
                "Laptop audio: native loopback stopped; retrying…"
            )
            if self._socket is not None:
                self._audio_retry_timer.start()

    def _stop_audio_monitor(self):
        loopback = self._audio_loopback
        self._audio_loopback = None
        if loopback is not None:
            loopback.finished.disconnect(self._audio_loopback_finished)
            loopback.terminate()
            if not loopback.waitForFinished(1000):
                loopback.kill()
                loopback.waitForFinished(1000)
            loopback.deleteLater()
        if hasattr(self, "_audio_status_label"):
            self._audio_status_label.setText(
                "Laptop audio: waiting for Dock Display connection"
            )

    def _send_frame_request(self):
        if self._socket is None:
            return
        self._request_id = (self._request_id + 1) & 0xFFFFFFFF
        if self._request_id == 0:
            self._request_id = 1
        packet = (
            FRAMEBUFFER_MAGIC + b"\x01" +
            struct.pack("!I", self._request_id) + b"\x01" +
            struct.pack("!H", self._frame_generation)
        )
        try:
            self._socket.sendto(packet, (IPOD_IP, FRAMEBUFFER_PORT))
        except OSError as error:
            self._status_label.setText(f"iPod framebuffer request failed: {error}")
            return
        self._awaiting_frame = True
        self._request_sent = time.monotonic()
        self._request_deadline = self._request_sent + 2.0
        self._frame_buffer = None
        self._frame_offsets = set()
        self._frame_received = 0
        self._frame_total = 0
        self._decode_seconds = 0.0

    def _poll_network(self):
        if self._socket is None:
            return
        # One native frame is normally 112 UDP datagrams. Drain several frames'
        # worth so a Qt paint or WebEngine wakeup cannot leave the final strip
        # waiting for another timer pass.
        for _ in range(512):
            try:
                packet, _address = self._socket.recvfrom(2048)
            except BlockingIOError:
                break
            except OSError as error:
                self._status_label.setText(f"Physical iPod link failed: {error}")
                self.stop_live_view(clear_frame=False)
                return
            self._accept_frame_packet(packet)
        if self._awaiting_frame and time.monotonic() >= self._request_deadline:
            self._status_label.setText(
                "The iPod link is present but no complete framebuffer arrived; retrying…"
            )
            self._send_frame_request()

    def _accept_frame_packet(self, packet):
        if len(packet) < FRAMEBUFFER_HEADER.size:
            return
        try:
            (magic, packet_type, pixel_format, width, height, generation,
             request_id, total, offset) = FRAMEBUFFER_HEADER.unpack_from(packet)
        except struct.error:
            return
        payload = packet[FRAMEBUFFER_HEADER.size:]
        if magic != FRAMEBUFFER_MAGIC or pixel_format != 1:
            return
        if request_id != self._request_id or (width, height) not in FRAMEBUFFER_SIZES:
            return
        if packet_type == 0x12:
            if payload or total != 0 or offset != 0:
                return
            self._frame_generation = generation
            self._awaiting_frame = False
            QTimer.singleShot(60, self._send_frame_request)
            return
        if packet_type not in (0x10, 0x11, 0x13, 0x14):
            return
        if total != width * height * 2 or total > FRAMEBUFFER_MAX_BYTES:
            return
        if packet_type in (0x11, 0x14):
            decode_started = time.monotonic()
            payload = self._decode_packbits_rgb565(payload, total - offset)
            self._decode_seconds += time.monotonic() - decode_started
            if payload is None:
                return
        if not payload or offset + len(payload) > total:
            return
        if self._frame_buffer is None:
            self._frame_buffer = bytearray(total)
            self._frame_width = width
            self._frame_height = height
            self._frame_total = total
            self._frame_generation = generation
            self._frame_is_video = packet_type in (0x13, 0x14)
        if generation != self._frame_generation:
            return
        if offset in self._frame_offsets:
            return
        self._frame_offsets.add(offset)
        self._frame_buffer[offset:offset + len(payload)] = payload
        self._frame_received += len(payload)
        if self._frame_received < self._frame_total:
            return

        complete = time.monotonic()
        frame = QImage(
            bytes(self._frame_buffer),
            self._frame_width,
            self._frame_height,
            self._frame_width * 2,
            QImage.Format_RGB16,
        ).copy()
        if frame.isNull():
            return
        self._preview.set_frame(frame)
        now = time.monotonic()
        self._awaiting_frame = False

        # Transport is request-sent to last-packet-in; paint is the QImage
        # copy and the widget update. Together with the frame interval they
        # account for the whole budget, so the slow half is identifiable
        # without guessing.
        self._transport_ms = ((complete - self._request_sent) * 1000.0
                              if self._request_sent else 0.0)
        self._decode_ms = self._decode_seconds * 1000.0
        self._paint_ms = (now - complete) * 1000.0

        # How many frames the iPod rendered since the one we last showed. A
        # steady 1 means we are at native rate; 2 means we are showing every
        # other frame.
        delta = (generation - self._last_shown_generation) & 0xFFFF
        self._last_shown_generation = generation
        if 1 <= delta <= 64:
            self._native_deltas.append(delta)
            del self._native_deltas[:-60]

        self._frame_times = [stamp for stamp in self._frame_times if now - stamp < 2.0]
        self._frame_times.append(now)
        fps = len(self._frame_times) / min(2.0, max(0.25, now - self._frame_times[0] + 0.25))
        if self._native_deltas:
            mean_delta = sum(self._native_deltas) / len(self._native_deltas)
        else:
            mean_delta = 1.0
        native_fps = fps * mean_delta
        dropped = (1.0 - 1.0 / mean_delta) * 100.0 if mean_delta > 0 else 0.0

        if now - self._last_transport_log >= 2.0:
            LOGGER.info(
                "Physical iPod video out: %.1f fps (native %.1f fps, %.0f%% of "
                "frames dropped) %dx%d mode=%s transport=%.1fms decode=%.1fms "
                "paint=%.1fms",
                fps, native_fps, dropped, self._frame_width,
                self._frame_height, "video" if self._frame_is_video else "ui",
                self._transport_ms, self._decode_ms, self._paint_ms,
            )
            self._last_transport_log = now
        self._status_label.setText(
            f"LIVE PHYSICAL IPOD · {self._interface} · {fps:.1f} of "
            f"{native_fps:.1f} native fps · {self._frame_width}×"
            f"{self._frame_height} low-latency RGB565 · "
            f"transport {self._transport_ms:.0f}ms · "
            f"paint {self._paint_ms:.0f}ms · audio: laptop USB monitor"
        )
        # A fixed floor here capped the UI path near 30fps no matter how fast
        # the link ran. The loop is self-limiting: one request per completed
        # frame, so the request rate can never exceed the frame rate.
        QTimer.singleShot(0, self._send_frame_request)

    @staticmethod
    def _decode_packbits_rgb565(payload, capacity):
        """Expand independently decodable PackBits-style RGB565 data."""
        output = bytearray()
        cursor = 0
        while cursor < len(payload):
            token = payload[cursor]
            cursor += 1
            if token & 0x80:
                count = (token & 0x7F) + 2
                if cursor + 2 > len(payload):
                    return None
                pixel = payload[cursor:cursor + 2]
                cursor += 2
                if len(output) + count * 2 > capacity:
                    return None
                output.extend(pixel * count)
            else:
                count = token + 1
                byte_count = count * 2
                if cursor + byte_count > len(payload) or \
                        len(output) + byte_count > capacity:
                    return None
                output.extend(payload[cursor:cursor + byte_count])
                cursor += byte_count
        return bytes(output)

    def _remote_action(self, action):
        code = self._REMOTE_ACTIONS.get(action)
        if code is None or self._socket is None:
            self._status_label.setText("Connect the physical iPod before sending controls.")
            return False
        packet = FRAMEBUFFER_MAGIC + b"\x02" + bytes((code,))
        try:
            self._socket.sendto(packet, (IPOD_IP, FRAMEBUFFER_PORT))
        except OSError as error:
            self._status_label.setText(f"Could not control the iPod: {error}")
            return False
        self._remote_events.append(action)
        return True

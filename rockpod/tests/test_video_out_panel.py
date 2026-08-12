"""Physical-iPod framebuffer viewer geometry and protocol tests."""

import struct

from PySide6.QtCore import QRectF, QTimer
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QApplication

from ui.sidebar import Sidebar
from ui.main_window import MainWindow
from ui.video_out_panel import (
    FRAMEBUFFER_HEADER,
    FRAMEBUFFER_MAGIC,
    FRAMEBUFFER_MAX_BYTES,
    DockPreview,
    VideoOutPanel,
    discover_rockpod_interface,
    is_rockbox_audio_input,
    rockpod_sink_input_ids,
    video_destination_rect,
)


def _application():
    return QApplication.instance() or QApplication([])


def test_four_by_three_frame_is_pillarboxed_on_dcp750_panel():
    destination = video_destination_rect(QRectF(0, 0, 480, 234), "4:3")

    assert round(destination.width()) == 312
    assert round(destination.height()) == 234
    assert round(destination.left()) == 84
    assert round(destination.top()) == 0


def test_stretch_mode_fills_the_dcp750_panel():
    destination = video_destination_rect(QRectF(10, 20, 480, 234), "stretch")

    assert destination == QRectF(10, 20, 480, 234)


def test_overscan_expands_frame_around_panel_center():
    panel = QRectF(0, 0, 480, 234)
    fitted = video_destination_rect(panel, "4:3")
    overscan = video_destination_rect(panel, "4:3", 0.06)

    assert overscan.center() == fitted.center()
    assert overscan.width() > fitted.width()
    assert overscan.height() > fitted.height()


def test_video_out_panel_is_explicitly_physical_ipod_only():
    _application()
    panel = VideoOutPanel()

    assert "real pixels from the connected iPod" in panel._warning_label.text()
    assert "iPod USB Audio source" in panel._warning_label.text()
    assert "USB Connection: Internet" in panel._status_label.text()
    assert panel._preview._frame.isNull()


def test_audio_input_match_accepts_only_physical_rockbox_capture_node():
    class Device:
        def __init__(self, description, device_id):
            self._description = description
            self._id = device_id

        def description(self):
            return self._description

        def id(self):
            return self._id

    assert is_rockbox_audio_input(
        Device("Rockbox media player", b"alsa_input.usb-Rockbox.org")
    )
    assert is_rockbox_audio_input(
        Device(
            "iPod Classic Analog Stereo",
            b"alsa_input.usb-05ac_1261-02.analog-stereo",
        )
    )
    assert not is_rockbox_audio_input(
        Device("Built-in Audio Analog Stereo", b"alsa_input.pci")
    )


def test_rockpod_sink_input_ids_selects_only_rockpod_streams():
    listing = '''
Sink Input #206
    Mute: yes
        media.name = "RockPod"
Sink Input #210
    Mute: no
        media.name = "Firefox"
Sink Input #279
    Mute: no
        media.name = "RockPod-Dock-Audio output"
'''

    assert rockpod_sink_input_ids(listing) == ["206", "279"]


def test_video_out_remote_sends_action_to_physical_ipod():
    _application()
    panel = VideoOutPanel()
    packets = []

    class FakeSocket:
        def sendto(self, packet, address):
            packets.append((packet, address))

    panel._socket = FakeSocket()

    assert panel._remote_action("play")
    assert panel._remote_action("right")

    assert panel._remote_events == ["play", "right"]
    assert packets == [
        (FRAMEBUFFER_MAGIC + b"\x02\x07", ("10.77.0.2", 47704)),
        (FRAMEBUFFER_MAGIC + b"\x02\x06", ("10.77.0.2", 47704)),
    ]


def test_frame_request_enables_physical_external_only_dock_display():
    _application()
    panel = VideoOutPanel()
    packets = []

    class FakeSocket:
        def sendto(self, packet, address):
            packets.append((packet, address))

    panel._socket = FakeSocket()
    panel._send_frame_request()

    assert packets == [(
        FRAMEBUFFER_MAGIC + b"\x01" + struct.pack("!I", 1) + b"\x01\x00\x00",
        ("10.77.0.2", 47704),
    )]


def test_preview_never_synthesizes_an_ipod_screen():
    _application()
    preview = DockPreview()

    assert preview._frame.isNull()
    assert "physical iPod" in preview._message


def test_physical_frame_packets_reassemble_into_real_qimage():
    _application()
    panel = VideoOutPanel()
    panel._request_id = 42
    panel._awaiting_frame = True
    pixels = b"\x00\xf8" * (320 * 240)
    split = 80000

    for offset, payload in ((0, pixels[:split]), (split, pixels[split:])):
        packet = FRAMEBUFFER_HEADER.pack(
            FRAMEBUFFER_MAGIC, 0x10, 1, 320, 240, 0,
            42, FRAMEBUFFER_MAX_BYTES, offset,
        ) + payload
        panel._accept_frame_packet(packet)

    assert not panel._preview._frame.isNull()
    assert panel._preview._frame.size() == QImage(320, 240, QImage.Format_RGB16).size()
    assert "LIVE PHYSICAL IPOD" in panel._status_label.text()


def test_low_latency_physical_frame_reassembles_and_upscales_in_preview():
    _application()
    panel = VideoOutPanel()
    panel._request_id = 7
    panel._awaiting_frame = True
    pixels = b"\xe0\x07" * (160 * 120)

    packet = FRAMEBUFFER_HEADER.pack(
        FRAMEBUFFER_MAGIC, 0x10, 1, 160, 120, 1,
        7, len(pixels), 0,
    ) + pixels
    panel._accept_frame_packet(packet)

    assert panel._preview._frame.size() == QImage(
        160, 120, QImage.Format_RGB16
    ).size()
    assert "160×120 low-latency" in panel._status_label.text()


def test_proportional_video_frame_reassembles_at_transport_resolution():
    _application()
    panel = VideoOutPanel()
    panel._request_id = 8
    panel._awaiting_frame = True
    pixels = b"\x1f\x00" * (120 * 90)

    packet = FRAMEBUFFER_HEADER.pack(
        FRAMEBUFFER_MAGIC, 0x10, 1, 120, 90, 2,
        8, len(pixels), 0,
    ) + pixels
    panel._accept_frame_packet(packet)

    assert panel._preview._frame.size() == QImage(
        120, 90, QImage.Format_RGB16
    ).size()
    assert "120×90 low-latency" in panel._status_label.text()


def test_native_video_packet_keeps_full_ipod_resolution():
    _application()
    panel = VideoOutPanel()
    panel._request_id = 10
    panel._awaiting_frame = True
    pixels = b"\xff\xff" * (320 * 240)

    packet = FRAMEBUFFER_HEADER.pack(
        FRAMEBUFFER_MAGIC, 0x13, 1, 320, 240, 19,
        10, len(pixels), 0,
    ) + pixels
    panel._accept_frame_packet(packet)

    assert panel._frame_is_video
    assert panel._preview._frame.size() == QImage(
        320, 240, QImage.Format_RGB16
    ).size()


def test_packbits_frame_packet_expands_repeated_and_literal_rgb565():
    _application()
    panel = VideoOutPanel()
    panel._request_id = 9
    panel._awaiting_frame = True
    # Repeat three red pixels, followed by one green literal pixel.
    encoded = b"\x81\x00\xf8\x00\xe0\x07"
    total = 8

    packet = FRAMEBUFFER_HEADER.pack(
        FRAMEBUFFER_MAGIC, 0x11, 1, 2, 2, 14, 9, total, 0,
    ) + encoded
    # The production protocol only permits target screen sizes, so exercise
    # the decoder directly for this compact unit vector.
    decoded = panel._decode_packbits_rgb565(encoded, total)

    assert decoded == b"\x00\xf8" * 3 + b"\xe0\x07"


def test_unchanged_generation_schedules_poll_without_replacing_real_frame(monkeypatch):
    _application()
    panel = VideoOutPanel()
    panel._request_id = 11
    panel._awaiting_frame = True
    panel._preview.set_frame(QImage(320, 240, QImage.Format_RGB16))
    scheduled = []
    monkeypatch.setattr(QTimer, "singleShot", lambda delay, callback: scheduled.append(delay))

    packet = FRAMEBUFFER_HEADER.pack(
        FRAMEBUFFER_MAGIC, 0x12, 1, 320, 240, 22, 11, 0, 0,
    )
    panel._accept_frame_packet(packet)

    assert panel._frame_generation == 22
    assert not panel._awaiting_frame
    assert scheduled == [60]
    assert not panel._preview._frame.isNull()


def test_discovers_only_apple_1261_network_interface(tmp_path):
    net_root = tmp_path / "net"
    device = tmp_path / "devices" / "ipod"
    net_root.mkdir()
    device.mkdir(parents=True)
    (device / "idVendor").write_text("05ac\n")
    (device / "idProduct").write_text("1261\n")
    (net_root / "enxrockpod").symlink_to(device, target_is_directory=True)

    assert discover_rockpod_interface(net_root) == "enxrockpod"


def test_sidebar_exposes_video_out_dock_lab():
    _application()
    sidebar = Sidebar()

    item = sidebar._find_item(Sidebar.ROCKBOX_VIDEO_OUT)

    assert item is not None
    assert item.text(0) == "Video Out / Dock Lab"


def test_main_window_routes_video_out_sidebar_view(config):
    app = _application()
    window = MainWindow(config)
    try:
        window._on_sidebar_selection("rockbox", Sidebar.ROCKBOX_VIDEO_OUT)

        assert window._current_view == Sidebar.ROCKBOX_VIDEO_OUT
        assert window._content_stack.currentWidget() is window._video_out_panel
        assert window._toolbar._title_label.text() == "Video Out / Dock Lab"
        assert window._tracks_for_current_view() == []
    finally:
        window.close()
        window.deleteLater()
        app.processEvents()

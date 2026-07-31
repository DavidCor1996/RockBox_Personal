#!/usr/bin/env python3
"""Static integration gate for the iPod 6G Kokkia/AirPods contract."""

from pathlib import Path
import struct
import sys


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(source: str, needle: str, label: str) -> None:
    if needle not in source:
        raise AssertionError(f"{label}: missing {needle!r}")


def main() -> int:
    core = read("apps/iap/iap-core.c")
    lingo2 = read("apps/iap/iap-lingo2.c")
    render = read("apps/gui/skin_engine/skin_render.c")
    wps = read("apps/gui/wps.c")
    ui = read("apps/gui/ipodjs_ui.c")
    root_menu = read("apps/root_menu.c")
    lingo0 = read("apps/iap/iap-lingo0.c")
    lingo3 = read("apps/iap/iap-lingo3.c")
    serial = read("firmware/target/arm/s5l8702/ipod6g/serial-6g.c")
    spec = read("docs/kokkia-airpods-integration-spec.md")

    require(core, "device.serial_activation_sent", "ready boundary")
    require(core, "iap_kokkia_link_ready = true;", "latched ready state")
    require(core, "iap_kokkia_link_ready = false;", "ready reset boundary")
    require(core, "bool iap_kokkia_present(void)", "dongle presence state")
    require(core, "iap_note_kokkia_peer_connection",
            "peer connection-edge event")
    require(lingo2, "buf[3] & (BIT_N(0) | BIT_N(1))",
            "Kokkia peer status-pulse detection")
    note_pos = lingo2.index("iap_note_kokkia_peer_connection();")
    suppress_pos = lingo2.index("if (iap_remote_input_suppressed())")
    if note_pos > suppress_pos:
        raise AssertionError(
            "peer status pulse must be observed before startup quarantine")
    require(core, "iap_kokkia_connection_pending = false;",
            "connection-event consumption")
    require(core, "device.auth.state > iap_watchdog_auth_state",
            "forward authentication progress")
    require(core, "IAP_RECONNECT_ACTIVATION_TIMEOUT", "failure reasons")
    require(core, "IAP_RECONNECT_LINK_ERRORS",
            "READY-state link-error recovery reason")
    require(core, "IAP_HEALTH_ERROR_THRESHOLD 4",
            "clustered link-error threshold")
    require(core, "IAP_HEALTH_ERROR_WINDOW (2 * HZ)",
            "clustered link-error window")
    require(core, "iap_diag_uart_errors +",
            "UART health included in recovery")
    require(core, "iap_diag_checksum_errors",
            "checksum health included in recovery")
    require(core, "if (device.volume != volume)",
            "change-only volume notification")
    require(core, "device.volume = volume;",
            "volume notification cache")
    require(lingo3, "device.volume = iap_volume_byte();",
            "initial normalized volume cache")
    require(core, "iap_diag_uart_errors, iap_diag_uart_overruns",
            "UART error breakdown in recovery trace")
    require(core, "battery_voltage(), battery_level()",
            "power context in recovery trace")
    require(root_menu, "info->uart_errors",
            "Kokkia status UART diagnostics")
    require(root_menu, "info->checksum_errors",
            "Kokkia status checksum diagnostics")
    require(core, "btn |= BUTTON_PLAY;", "headset Play/Pause translation")
    require(core, "if (audio_status() & AUDIO_STATUS_PLAY)",
            "stopped-state input guard")
    require(lingo0, "iap_note_kokkia_ready();", "activation completion")
    auth_pos = lingo0.index("device.auth.state = AUST_AUTH;",
                           lingo0.index("case 0x18:"))
    ready_pos = lingo0.index("iap_note_kokkia_ready();", auth_pos)
    if ready_pos < auth_pos:
        raise AssertionError(
            "authenticated Kokkia sessions must become ready before the "
            "activation watchdog can retry them")
    require(lingo0, "if (iap_kokkia_present())",
            "pre-authentication Kokkia readiness predicate")
    require(lingo0, "device.serial_activation_tid ==",
            "idempotent activation retransmission predicate")
    require(lingo0, "A new transaction ID still follows the restart",
            "activation restart boundary")
    disconnect = core.index("void iap_note_serial_disconnect(void)")
    disconnect_end = core.index("bool iap_remote_input_suppressed(void)",
                                 disconnect)
    require(core[disconnect:disconnect_end], "iap_kokkia_present()",
            "pause-on-unplug presence predicate")
    require(spec, "optional follow-up",
            "authenticated Kokkia readiness contract")
    require(serial, "(HZ + 3) / 4", "250 ms dock debounce")
    require(serial, "if (!iap_ready_for_serial())", "boot UART gate")
    require(serial, "iap_diag_uart_frame_errors",
            "UART framing diagnostics")
    require(serial, "iap_diag_contact_dropouts",
            "dock-contact dropout diagnostics")
    require(serial, "iap_diag_max_absent_ticks",
            "dock-contact duration diagnostics")
    require(serial, "iap_diag_rate = -detected_rate",
            "numeric auto-bitrate diagnostics")
    require(spec, "Bluetooth audio is\n  quiet",
            "documented silence-safe recovery boundary")
    require(wps, "iap_kokkia_present()", "WPS dongle-presence icon")
    require(render, "fixed_ipodjs_wps && display->screen_type == SCREEN_MAIN",
            "WPS Bluetooth compositor")
    require(render, "ipodjs_ui_draw_bluetooth_indicator(display, 254, 2);",
            "WPS icon composition")
    require(wps, "iap_take_kokkia_connection_event",
            "WPS stale-event consumer")
    require(spec, "while the Home dashboard is active",
            "documented Home-only animation contract")

    start = ui.index("void ipodjs_ui_airpods_connected_animation(void)")
    end = ui.index("void ipodjs_ui_draw_header_battery", start)
    animation = ui[start:end]
    for forbidden in ("audio_", "pcm_", "playlist_", "core_alloc", "open("):
        if forbidden in animation:
            raise AssertionError(
                f"animation lifecycle boundary: found {forbidden!r}")
    require(animation, "7 * HZ / 10", "700 ms dismissal duration")
    require(animation, "current_tick - started",
            "elapsed-time animation position")
    require(animation, "ipodjs_ui_connection_smoothstep",
            "smooth AirPods dismissal")
    require(animation,
            "memcpy(ipodjs_ui_animation_old, FBADDR(0, 0), "
            "FRAMEBUFFER_SIZE);",
            "Home snapshot behind AirPods sheet")
    require(animation,
            "memcpy(FBADDR(0, 0), ipodjs_ui_animation_new, "
            "FRAMEBUFFER_SIZE);",
            "dimmed Home restoration per frame")

    asset = (
        ROOT
        / "assets/ipodjs/apple/airpods-pro-connected.apple.124x109x24.bmp"
    )
    header = asset.read_bytes()[:54]
    if len(header) != 54 or header[:2] != b"BM":
        raise AssertionError("AirPods asset is not a Windows BMP")
    width, height = struct.unpack_from("<ii", header, 18)
    bits = struct.unpack_from("<H", header, 28)[0]
    if (width, abs(height), bits) != (124, 109, 24):
        raise AssertionError(
            f"AirPods asset is {width}x{height}x{bits}, expected 124x109x24")

    print("Kokkia integration gate: PASS")
    print("  ready/auth/activation/recovery invariants present")
    print("  change-only volume and clustered link-error recovery present")
    print("  UART/checksum/contact diagnostics present")
    print("  headset center click uses physical iPod Play/Pause semantics")
    print("  presence icon and Home-only connection animation paths present")
    print("  animation is display-only and asset is 124x109x24 BMP")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, ValueError) as error:
        print(f"Kokkia integration gate: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)

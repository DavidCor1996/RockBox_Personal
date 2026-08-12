#!/usr/bin/env python3
"""RockPod USB Ethernet companion for weather, web, and Club Penguin."""

from __future__ import annotations

import argparse
import html
from html.parser import HTMLParser
import json
import os
import select
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

from PIL import Image

IPOD_IP = "10.77.0.2"
HOST_CIDR = "10.77.0.1/24"
WEATHER_PORT = 47700
CLUB_PORT = 47701
BROWSER_PORT = 47702
MAGIC = b"RPI1"
BROWSER_MAGIC = b"RPB1"


def discover_interface() -> str | None:
    for net_path in Path("/sys/class/net").iterdir():
        try:
            device = net_path.resolve()
        except OSError:
            continue
        for parent in (device, *device.parents):
            vendor = parent / "idVendor"
            product = parent / "idProduct"
            try:
                if (vendor.read_text().strip().lower() == "05ac" and
                        product.read_text().strip().lower() == "1261"):
                    return net_path.name
            except OSError:
                pass
    return None


def configure_interface(interface: str) -> None:
    connection = f"RockPod USB Internet ({interface})"
    exists = subprocess.run(
        ("nmcli", "-t", "-f", "NAME", "connection", "show", connection),
        check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    ).returncode == 0
    if exists:
        commands = ((
            "nmcli", "connection", "modify", connection,
            "connection.interface-name", interface,
            "ipv4.method", "manual", "ipv4.addresses", HOST_CIDR,
            "ipv4.never-default", "yes", "ipv6.method", "disabled",
        ),)
    else:
        commands = ((
            "nmcli", "connection", "add", "type", "ethernet",
            "ifname", interface, "con-name", connection,
            "ipv4.method", "manual", "ipv4.addresses", HOST_CIDR,
            "ipv4.never-default", "yes", "ipv6.method", "disabled",
        ),)
    commands += (("nmcli", "connection", "up", connection),)
    for command in commands:
        try:
            subprocess.run(
                command, check=True, stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except FileNotFoundError as error:
            raise SystemExit("The Linux 'nmcli' command is required.") from error
        except subprocess.CalledProcessError as error:
            raise SystemExit(
                f"Could not configure the RockPod profile on {interface}."
            ) from error


def weather_name(code: int) -> tuple[str, str]:
    if code == 0:
        return "clear", "Clear"
    if code in (1, 2):
        return "partly_cloudy", "Partly Cloudy"
    if code == 3:
        return "cloudy", "Cloudy"
    if code in (45, 48):
        return "fog", "Fog"
    if code in (51, 53, 55, 56, 57):
        return "drizzle", "Drizzle"
    if code in (61, 63, 65, 66, 67, 80, 81, 82):
        return "rain", "Rain"
    if code in (71, 73, 75, 77, 85, 86):
        return "snow", "Snow"
    if code in (95, 96, 99):
        return "thunderstorm", "Thunderstorm"
    return "cloudy", "Cloudy"


def compass(degrees: float) -> str:
    names = ("N", "NE", "E", "SE", "S", "SW", "W", "NW")
    return names[int((degrees + 22.5) // 45) % 8]


def fetch_weather(args: argparse.Namespace) -> bytes:
    current = ",".join((
        "temperature_2m", "weather_code", "wind_speed_10m",
        "wind_direction_10m", "is_day", "precipitation",
    ))
    hourly = ",".join((
        "temperature_2m", "precipitation_probability", "weather_code",
        "wind_speed_10m", "wind_direction_10m", "is_day",
    ))
    daily = ",".join((
        "weather_code", "temperature_2m_max", "temperature_2m_min",
        "precipitation_probability_max", "wind_speed_10m_max",
        "wind_direction_10m_dominant", "sunrise", "sunset",
    ))
    query = urllib.parse.urlencode({
        "latitude": args.latitude,
        "longitude": args.longitude,
        "current": current,
        "hourly": hourly,
        "daily": daily,
        "timezone": "auto",
        "forecast_days": 7,
        "temperature_unit": "fahrenheit" if args.imperial else "celsius",
        "wind_speed_unit": "mph" if args.imperial else "kmh",
    })
    url = "https://api.open-meteo.com/v1/forecast?" + query
    request = urllib.request.Request(url, headers={"User-Agent": "RockPod/1"})
    with urllib.request.urlopen(request, timeout=20) as response:
        forecast = json.load(response)

    generated = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%MZ")
    lines = ["\t".join((
        "rockpod_weather_v1", args.location, str(args.latitude),
        str(args.longitude), forecast.get("timezone", "UTC"), generated,
        "Open-Meteo", "imperial" if args.imperial else "metric",
    ))]
    current_data = forecast.get("current", {})
    if all(key in current_data for key in (
            "time", "temperature_2m", "weather_code")):
        icon, description = weather_name(current_data["weather_code"])
        lines.append("\t".join((
            "current", current_data["time"], icon, description,
            f'{current_data["temperature_2m"]:.0f}',
            f'{current_data.get("precipitation", 0):.0f}',
            f'{current_data.get("wind_speed_10m", 0):.0f}',
            compass(current_data.get("wind_direction_10m", 0)),
            str(current_data.get("is_day", 1)), "Open-Meteo Live",
        )))
    hourly_data = forecast["hourly"]
    for index, stamp in enumerate(hourly_data["time"][:168]):
        icon, description = weather_name(hourly_data["weather_code"][index])
        lines.append("\t".join((
            "hourly", stamp, icon, description,
            f'{hourly_data["temperature_2m"][index]:.0f}',
            f'{hourly_data["precipitation_probability"][index]:.0f}',
            f'{hourly_data["wind_speed_10m"][index]:.0f}',
            compass(hourly_data["wind_direction_10m"][index]),
            str(hourly_data["is_day"][index]), "Open-Meteo",
        )))
    daily_data = forecast["daily"]
    for index, date in enumerate(daily_data["time"]):
        icon, description = weather_name(daily_data["weather_code"][index])
        lines.append("\t".join((
            date, icon, description,
            f'{daily_data["temperature_2m_min"][index]:.0f}',
            f'{daily_data["temperature_2m_max"][index]:.0f}',
            f'{daily_data["precipitation_probability_max"][index]:.0f}',
            f'{daily_data["wind_speed_10m_max"][index]:.0f}',
            compass(daily_data["wind_direction_10m_dominant"][index]),
            daily_data["sunrise"][index][-5:],
            daily_data["sunset"][index][-5:], "Open-Meteo",
        )))
    return ("\n".join(lines) + "\n").encode()


def send_confirmed(sock: socket.socket, packet: bytes, expected: int,
                   destination: tuple[str, int]) -> None:
    for _ in range(6):
        sock.sendto(packet, destination)
        deadline = time.monotonic() + 0.75
        while time.monotonic() < deadline:
            readable, _, _ = select.select(
                [sock], [], [], deadline - time.monotonic()
            )
            if not readable:
                break
            response, _ = sock.recvfrom(2048)
            if len(response) == 9 and response[:5] == MAGIC + b"\x05" and \
                    struct.unpack("!I", response[5:9])[0] == expected:
                return
    raise TimeoutError(f"iPod did not acknowledge weather offset {expected}")


def send_weather(sock: socket.socket, payload: bytes) -> None:
    destination = (IPOD_IP, WEATHER_PORT)
    send_confirmed(
        sock, MAGIC + b"\x01" + struct.pack("!II", len(payload), 0),
        0, destination
    )
    for offset in range(0, len(payload), 480):
        chunk = payload[offset:offset + 480]
        send_confirmed(
            sock, MAGIC + b"\x02" + struct.pack("!I", offset) + chunk,
            offset + len(chunk), destination
        )
    send_confirmed(sock, MAGIC + b"\x03", 0xffffffff, destination)


class CompactWebPage(HTMLParser):
    """Reduce arbitrary HTML to the semantic subset rendered by the iPod."""

    def __init__(self, base_url: str) -> None:
        super().__init__(convert_charrefs=True)
        self.base_url = base_url
        self.output: list[str] = []
        self.hidden = 0

    def handle_starttag(self, tag: str,
                        attrs: list[tuple[str, str | None]]) -> None:
        tag = tag.lower()
        if tag in ("script", "style", "svg", "noscript"):
            self.hidden += 1
            return
        if self.hidden:
            return
        values = dict(attrs)
        if tag == "a" and values.get("href"):
            target = urllib.parse.urljoin(self.base_url, values["href"])
            if target.startswith(("http://", "https://")):
                self.output.append(
                    f'<a href="{html.escape(target, quote=True)}">'
                )
        elif tag in ("h1", "h2", "h3", "p", "li", "br", "hr"):
            self.output.append(f"<{tag}>")

    def handle_endtag(self, tag: str) -> None:
        tag = tag.lower()
        if tag in ("script", "style", "svg", "noscript"):
            if self.hidden:
                self.hidden -= 1
            return
        if self.hidden:
            return
        if tag in ("a", "h1", "h2", "h3", "p", "li"):
            self.output.append(f"</{tag}>")

    def handle_data(self, data: str) -> None:
        if self.hidden:
            return
        cleaned = " ".join(data.split())
        if cleaned:
            self.output.append(html.escape(cleaned) + " ")

    def payload(self) -> bytes:
        document = "<html><body>" + "".join(self.output) + "</body></html>"
        return document.encode("utf-8")[:65535]


def fetch_web_page(url: str) -> bytes:
    parsed = urllib.parse.urlsplit(url)
    if parsed.scheme not in ("http", "https") or not parsed.netloc:
        raise ValueError("Only HTTP and HTTPS addresses are supported")
    screenshot = render_web_page(url)
    request = urllib.request.Request(url, headers={
        "User-Agent": (
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/124.0 Safari/537.36"
        ),
        "Accept": "text/html,application/xhtml+xml",
        "Accept-Language": "en-CA,en;q=0.9",
    })
    with urllib.request.urlopen(request, timeout=20) as response:
        content_type = response.headers.get_content_type()
        if content_type not in ("text/html", "application/xhtml+xml"):
            raise ValueError(f"Unsupported page type: {content_type}")
        body = response.read(1024 * 1024)
        charset = response.headers.get_content_charset() or "utf-8"
        final_url = response.geturl()
    parser = CompactWebPage(final_url)
    parser.feed(body.decode(charset, errors="replace"))
    semantic = parser.payload().decode("utf-8", errors="replace")
    image = (
        '<img src="/.rockbox/offlineweb/cache/live.bmp" width="310" '
        'height="170" data-static="1"><br>'
    )
    google_search = ""
    if parsed.hostname and parsed.hostname.endswith("google.com"):
        google_search = (
            '<p><a href="rockbox:google-search">Search Google</a></p>'
        )
    html_payload = (
        "<html><body>" + image + google_search +
        semantic.removeprefix("<html><body>").removesuffix("</body></html>") +
        "</body></html>"
    ).encode("utf-8")
    return (b"RPWB" + struct.pack("!II", len(html_payload), len(screenshot)) +
            html_payload + screenshot)


def render_web_page(url: str) -> bytes:
    renderer = Path(__file__).with_name("rockpod_web_render.qml")
    qmlscene = "qmlscene6"
    with tempfile.TemporaryDirectory(prefix="rockpod-web-") as directory:
        png = Path(directory) / "page.png"
        bmp = Path(directory) / "page.bmp"
        environment = os.environ.copy()
        environment.update({
            "QT_QPA_PLATFORM": "offscreen",
            "QTWEBENGINE_DISABLE_SANDBOX": "1",
            "QTWEBENGINE_CHROMIUM_FLAGS": "--disable-gpu --no-sandbox",
        })
        parsed = urllib.parse.urlsplit(url)
        query = ""
        render_url = url
        if parsed.hostname and parsed.hostname.endswith("google.com") and \
                parsed.path == "/search":
            query = urllib.parse.parse_qs(parsed.query).get("q", [""])[0]
            if query:
                render_url = "https://www.google.com/"
        command = [
            qmlscene,
            f"rockpod-url={render_url}",
            f"rockpod-output={png}",
        ]
        if query:
            command.append(f"rockpod-query={query}")
        command.append(str(renderer))
        for attempt in range(3):
            try:
                completed = subprocess.run(
                    command, env=environment, timeout=25, check=False,
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                )
            except FileNotFoundError as error:
                raise RuntimeError(
                    "qmlscene6 and Qt WebEngine are required"
                ) from error
            if completed.returncode == 0 and png.is_file():
                break
            png.unlink(missing_ok=True)
            time.sleep(0.4 * (attempt + 1))
        if not png.is_file():
            raise RuntimeError("Web renderer did not produce a page image")
        with Image.open(png) as image_file:
            # The iPod LCD is RGB565.  An 8-bit adaptive palette is visually
            # indistinguishable at 310x170, cuts the USB transfer by roughly
            # two thirds, and avoids stressing the native USB stack with a
            # 158 KiB 24-bit bitmap for every page.
            image_file.convert("RGB").quantize(
                colors=128, method=Image.Quantize.MEDIANCUT
            ).save(bmp, format="BMP")
        return bmp.read_bytes()


def send_browser_confirmed(sock: socket.socket, packet: bytes,
                           request_id: int, expected: int) -> None:
    destination = (IPOD_IP, BROWSER_PORT)
    for _ in range(6):
        sock.sendto(packet, destination)
        deadline = time.monotonic() + 0.75
        while time.monotonic() < deadline:
            readable, _, _ = select.select(
                [sock], [], [], deadline - time.monotonic()
            )
            if not readable:
                break
            response, _ = sock.recvfrom(2048)
            if len(response) == 13 and response[:5] == BROWSER_MAGIC + b"\x05":
                ack_id, ack_offset = struct.unpack("!II", response[5:13])
                if ack_id == request_id and ack_offset == expected:
                    return
    raise TimeoutError(f"iPod did not acknowledge browser offset {expected}")


def send_browser_page(sock: socket.socket, request_id: int,
                      payload: bytes) -> None:
    prefix = BROWSER_MAGIC
    send_browser_confirmed(
        sock, prefix + b"\x02" + struct.pack("!III", request_id,
                                              len(payload), 0),
        request_id, 0,
    )
    for offset in range(0, len(payload), 476):
        chunk = payload[offset:offset + 476]
        send_browser_confirmed(
            sock, prefix + b"\x03" + struct.pack("!II", request_id, offset) +
            chunk,
            request_id, offset + len(chunk),
        )
    send_browser_confirmed(
        sock, prefix + b"\x04" + struct.pack("!I", request_id),
        request_id, 0xffffffff,
    )


def parse_relay(value: str | None) -> tuple[str, int] | None:
    if not value:
        return None
    host, separator, port = value.rpartition(":")
    if not separator:
        raise SystemExit("--relay must be HOST:PORT")
    return host, int(port)


def serve_interface(args: argparse.Namespace, interface: str) -> None:
    if not args.no_configure:
        configure_interface(interface)

    weather_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    weather_sock.bind(("10.77.0.1", WEATHER_PORT))
    club_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    club_sock.bind(("10.77.0.1", CLUB_PORT))
    browser_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    browser_sock.bind(("10.77.0.1", BROWSER_PORT))

    relay_address = parse_relay(args.relay)
    relay_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    relay_sock.bind(("0.0.0.0", 0))
    room = args.room.encode()[:16].ljust(16, b"\0")
    print(f"RockPod Internet active on {interface}; iPod is {IPOD_IP}")
    if relay_address:
        print(f"Club Penguin pair room '{args.room}' via {relay_address}")

    last_probe = 0.0
    try:
        while True:
            now = time.monotonic()
            if now - last_probe >= 3:
                weather_sock.sendto(b"RPI1\x00", (IPOD_IP, WEATHER_PORT))
                browser_sock.sendto(b"RPB1\x00", (IPOD_IP, BROWSER_PORT))
                last_probe = now
            readable, _, _ = select.select(
                [weather_sock, club_sock, browser_sock, relay_sock], [], [], 1.0
            )
            for source in readable:
                packet, _ = source.recvfrom(2048)
                if source is weather_sock and packet == MAGIC + b"\x04":
                    try:
                        payload = fetch_weather(args)
                        send_weather(weather_sock, payload)
                        print(f"Weather synced ({len(payload)} bytes)")
                    except Exception as error:
                        print(f"Weather sync failed: {error}", file=sys.stderr)
                elif source is club_sock and relay_address and \
                        packet.startswith(b"CPM1"):
                    relay_sock.sendto(b"CPR1" + room + packet, relay_address)
                elif source is browser_sock and len(packet) > 9 and \
                        packet[:5] == BROWSER_MAGIC + b"\x01":
                    request_id = struct.unpack("!I", packet[5:9])[0]
                    url = packet[9:].decode("utf-8", errors="replace")
                    try:
                        payload = fetch_web_page(url)
                        send_browser_page(browser_sock, request_id, payload)
                        print(f"Web page sent ({len(payload)} bytes): {url}")
                    except Exception as error:
                        message = str(error).encode(
                            "utf-8", errors="replace"
                        )[:240]
                        browser_sock.sendto(
                            BROWSER_MAGIC + b"\x06" +
                            struct.pack("!I", request_id) + message,
                            (IPOD_IP, BROWSER_PORT),
                        )
                        print(f"Web fetch failed: {error}", file=sys.stderr)
                elif source is relay_sock and packet.startswith(b"CPR1") and \
                        packet[4:20] == room and \
                        packet[20:].startswith(b"CPM1"):
                    club_sock.sendto(packet[20:], (IPOD_IP, CLUB_PORT))
    finally:
        weather_sock.close()
        club_sock.close()
        browser_sock.close()
        relay_sock.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--interface", help="CDC-ECM network interface")
    parser.add_argument(
        "--wait", action="store_true",
        help="wait for and reconnect to a RockPod CDC-Ethernet interface",
    )
    parser.add_argument("--no-configure", action="store_true")
    parser.add_argument("--latitude", type=float, default=46.0878)
    parser.add_argument("--longitude", type=float, default=-64.7782)
    parser.add_argument("--location", default="Moncton")
    parser.add_argument("--imperial", action="store_true")
    parser.add_argument("--relay", help="two-player relay as HOST:PORT")
    parser.add_argument("--room", default="rockpod", help="private pair code")
    args = parser.parse_args()

    waiting_announced = False
    while True:
        interface = args.interface or discover_interface()
        if not interface:
            if not args.wait:
                raise SystemExit("RockPod CDC-Ethernet interface not found.")
            if not waiting_announced:
                print("Waiting for RockPod USB Internet mode...", flush=True)
                waiting_announced = True
            time.sleep(1)
            continue
        waiting_announced = False
        try:
            serve_interface(args, interface)
        except (OSError, TimeoutError) as error:
            if not args.wait:
                raise
            print(f"RockPod link lost ({error}); waiting to reconnect...",
                  file=sys.stderr, flush=True)
            time.sleep(1)


if __name__ == "__main__":
    raise SystemExit(main())

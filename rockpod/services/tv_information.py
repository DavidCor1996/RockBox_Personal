"""Fresh, sync-time television information channels for RockPod.

RockPod is the network-connected headend.  It downloads a bounded set of
current source material, renders the information graphics on the host, and
builds one ordinary MPEG-ready programme reel per channel.  The iPod never
performs network access or allocates decorative playback-time framebuffers.
"""

from __future__ import annotations

import hashlib
import html
import json
import logging
import os
import re
import shutil
import subprocess
import time
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from datetime import datetime, timezone
from html.parser import HTMLParser
from pathlib import Path
from zoneinfo import ZoneInfo

from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageOps

from services.file_safety import atomic_write_json, atomic_write_text


logger = logging.getLogger(__name__)

TV_INFORMATION_VERSION = 1
TV_INFORMATION_CATEGORY_PREFIX = "TVInfo:"
TV_INFORMATION_BLOCK_SECONDS = 1800
TV_INFORMATION_MAX_DOWNLOAD_BYTES = 256 * 1024 * 1024
TV_INFORMATION_USER_AGENT = "RockPod TV Information/1.0"

NASA_CATEGORY = TV_INFORMATION_CATEGORY_PREFIX + "NASA"
NEWS_CATEGORY = TV_INFORMATION_CATEGORY_PREFIX + "NewsNB"
ROAD_CATEGORY = TV_INFORMATION_CATEGORY_PREFIX + "RoadMaritimes"

NASA_VIDEO_FEED = "https://www.jpl.nasa.gov/feeds/podcasts/video/"
NASA_SEARCH_API = "https://images-api.nasa.gov/search"
NEWS_VIDEO_PAGE = "https://globalnews.ca/videos/new-brunswick/"
NEWS_HEADLINE_FEED = (
    "https://api.io.canada.ca/io-server/gc/news/en/v2?"
    "atomtitle=New+Brunswick&format=atom&location=nb13&orderBy=desc&"
    "pick=100&publishedDate%3E=2021-10-25&sort=publishedDate"
)
ROAD_CAMERA_PAGES = (
    ("Nova Scotia", "https://novascotia.ca/tran/cameras/all.asp"),
    ("New Brunswick", "https://511.gnb.ca/cctv?lang=en"),
    ("Prince Edward Island", "https://511.gov.pe.ca/en/cameras.html"),
)
ROAD_CAPTURE_CAMERAS = (
    (
        "MacKay Bridge — Halifax bound",
        "https://images.novascotiawebcams.com/"
        "mackay-halifax/og_image.jpg",
    ),
    (
        "MacKay Bridge — Dartmouth bound",
        "https://images.novascotiawebcams.com/"
        "mackay-dartmouth/og_image.jpg",
    ),
    (
        "Macdonald Bridge — Halifax bound",
        "https://images.novascotiawebcams.com/"
        "macdonald-halifax/og_image.jpg",
    ),
)


@dataclass(frozen=True)
class TvInformationChannel:
    key: str
    category: str
    callsign: str
    name: str
    title: str
    rating: str
    description: str
    colours: tuple[str, str, str]


CHANNELS = (
    TvInformationChannel(
        key="nasa",
        category=NASA_CATEGORY,
        callsign="NASA",
        name="NASA",
        title="NASA Mission Update",
        rating="TV-G",
        description="Current NASA video and mission updates.",
        colours=("#07152F", "#174E86", "#E33B35"),
    ),
    TvInformationChannel(
        key="news-nb",
        category=NEWS_CATEGORY,
        callsign="NBT",
        name="New Brunswick News",
        title="New Brunswick Now",
        rating="TV-PG",
        description="Current New Brunswick headlines and video.",
        colours=("#071B36", "#1768A5", "#D42F35"),
    ),
    TvInformationChannel(
        key="road-mar",
        category=ROAD_CATEGORY,
        callsign="ROAD",
        name="Maritime RoadWatch",
        title="Maritime RoadWatch",
        rating="TV-G",
        description="Current Maritime highway cameras and road information.",
        colours=("#061B2B", "#087A8A", "#F0A52B"),
    ),
)


@dataclass
class FeedItem:
    title: str
    url: str = ""
    published: str = ""
    summary: str = ""
    source: str = ""
    enclosure: str = ""
    identity: str = ""

    def stable_id(self) -> str:
        value = self.identity or self.enclosure or self.url or self.title
        return hashlib.sha256(value.encode("utf-8", "replace")).hexdigest()[:16]


@dataclass
class TvInformationMedia:
    channel: TvInformationChannel
    path: str
    generated_at: str
    source_items: list[FeedItem] = field(default_factory=list)


@dataclass
class TvInformationResult:
    media: list[TvInformationMedia] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    generated_at: str = ""
    output_root: str = ""
    logos: dict[str, str] = field(default_factory=dict)
    previews: dict[str, str] = field(default_factory=dict)


def _config_value(config, key, default=None):
    if hasattr(config, "get"):
        return config.get(key, default)
    return getattr(config, key, default)


def _clean_text(value, limit=500):
    text = html.unescape(str(value or ""))
    text = re.sub(r"<[^>]+>", " ", text)
    text = re.sub(r"[\x00-\x1f\x7f]+", " ", text)
    text = " ".join(text.split())
    return text[:limit].strip()


def _utc_now():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _atlantic_stamp(value):
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        if parsed.tzinfo is None:
            parsed = parsed.replace(tzinfo=timezone.utc)
        return parsed.astimezone(
            ZoneInfo("America/Halifax")).strftime("%H:%M ATL")
    except (TypeError, ValueError):
        return _clean_text(value, 16)


def _request_bytes(url, timeout=25, max_bytes=4 * 1024 * 1024):
    parsed = urllib.parse.urlsplit(str(url))
    url = urllib.parse.urlunsplit((
        parsed.scheme,
        parsed.netloc,
        urllib.parse.quote(
            urllib.parse.unquote(parsed.path), safe="/%:@"),
        parsed.query,
        parsed.fragment,
    ))
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": TV_INFORMATION_USER_AGENT,
            "Accept": "*/*",
        },
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        length = response.headers.get("Content-Length")
        if length and int(length) > max_bytes:
            raise ValueError(f"response is larger than {max_bytes} bytes")
        payload = response.read(max_bytes + 1)
    if len(payload) > max_bytes:
        raise ValueError(f"response is larger than {max_bytes} bytes")
    return payload


def _download_atomic(url, target, max_bytes=TV_INFORMATION_MAX_DOWNLOAD_BYTES):
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp = target.with_suffix(target.suffix + ".tmp")
    try:
        payload = _request_bytes(url, timeout=45, max_bytes=max_bytes)
        if not payload:
            raise ValueError("download was empty")
        tmp.write_bytes(payload)
        os.replace(tmp, target)
    finally:
        try:
            tmp.unlink()
        except FileNotFoundError:
            pass
    return str(target)


def _file_digest(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            block = handle.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def parse_feed(payload, source="") -> list[FeedItem]:
    """Parse a bounded RSS or Atom feed without a third-party dependency."""
    if isinstance(payload, str):
        payload = payload.encode("utf-8")
    root = ET.fromstring(payload)
    results = []

    def node_text(node, names):
        for child in list(node):
            local = child.tag.rsplit("}", 1)[-1].lower()
            if local in names and child.text:
                return _clean_text(child.text)
        return ""

    nodes = [
        node for node in root.iter()
        if node.tag.rsplit("}", 1)[-1].lower() in {"item", "entry"}
    ]
    for node in nodes[:40]:
        title = node_text(node, {"title"})
        summary = node_text(
            node, {"description", "summary", "content", "subtitle"})
        published = node_text(
            node, {"pubdate", "published", "updated", "date"})
        link = ""
        enclosure = ""
        identity = node_text(node, {"guid", "id"})
        for child in list(node):
            local = child.tag.rsplit("}", 1)[-1].lower()
            href = _clean_text(child.attrib.get("href") or "", 2000)
            url = _clean_text(child.attrib.get("url") or "", 2000)
            rel = (child.attrib.get("rel") or "").lower()
            medium = (
                child.attrib.get("type") or
                child.attrib.get("medium") or ""
            ).lower()
            if local == "enclosure" and url:
                enclosure = url
            elif local in {"content", "player"} and url and (
                    "video" in medium or url.lower().endswith(
                        (".mp4", ".m4v", ".mov", ".webm"))):
                enclosure = url
            elif local == "link" and href:
                if rel == "enclosure" or "video" in medium:
                    enclosure = href
                elif not link or rel in {"", "alternate"}:
                    link = href
            elif local == "link" and child.text and not link:
                link = _clean_text(child.text, 2000)
        if not title:
            continue
        results.append(FeedItem(
            title=title,
            url=link,
            published=published,
            summary=summary,
            source=source,
            enclosure=enclosure,
            identity=identity,
        ))
    return results


class _LinkParser(HTMLParser):
    def __init__(self, base_url):
        super().__init__(convert_charrefs=True)
        self.base_url = base_url
        self.links = []
        self.images = []
        self._anchor = None
        self._text = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag.lower() == "a" and attrs.get("href"):
            self._anchor = urllib.parse.urljoin(
                self.base_url, attrs["href"])
            self._text = []
        if tag.lower() == "img":
            source = (
                attrs.get("data-src") or attrs.get("data-lazy-src") or
                attrs.get("src") or "")
            if source:
                self.images.append((
                    urllib.parse.urljoin(self.base_url, source),
                    _clean_text(attrs.get("alt") or attrs.get("title") or ""),
                ))

    def handle_data(self, data):
        if self._anchor:
            self._text.append(data)

    def handle_endtag(self, tag):
        if tag.lower() == "a" and self._anchor:
            self.links.append((
                self._anchor, _clean_text(" ".join(self._text))))
            self._anchor = None
            self._text = []


def parse_news_video_page(payload, base_url=NEWS_VIDEO_PAGE) -> list[FeedItem]:
    parser = _LinkParser(base_url)
    parser.feed(payload.decode("utf-8", "replace")
                if isinstance(payload, bytes) else str(payload))
    results = []
    seen = set()
    for url, title in parser.links:
        path = urllib.parse.urlparse(url).path.lower()
        if "/video/" not in path or url in seen:
            continue
        if not title or title.lower() in {
                "video", "watch", "main", "previous video", "next video"}:
            continue
        seen.add(url)
        results.append(FeedItem(
            title=title,
            url=url,
            source="Global News New Brunswick",
            identity=url,
        ))
        if len(results) >= 12:
            break
    return results


def parse_camera_page(payload, base_url, region) -> list[FeedItem]:
    parser = _LinkParser(base_url)
    parser.feed(payload.decode("utf-8", "replace")
                if isinstance(payload, bytes) else str(payload))
    candidates = []
    seen = set()
    for url, label in parser.images:
        lower = url.lower()
        if not lower.startswith(("http://", "https://")):
            continue
        if url in seen:
            continue
        if not re.search(r"\.(?:jpe?g|png)(?:\?|$)", lower):
            continue
        if not any(token in lower for token in (
                "camera", "camimage", "cctv", "snapshot", "webcam",
                "rwis_cam")):
            continue
        if any(token in lower for token in (
                "logo", "icon", "sprite", "marker", "favicon", "banner")):
            continue
        seen.add(url)
        candidates.append(FeedItem(
            title=label or f"{region} highway camera",
            url=url,
            source=region,
            enclosure=url,
            identity=url,
        ))
    return candidates


def camera_api_items(payload, base_url, region) -> list[FeedItem]:
    """Parse the map-camera endpoint used by the NB and PEI 511 sites."""
    if isinstance(payload, bytes):
        payload = payload.decode("utf-8", "replace")
    parsed = json.loads(payload)
    results = []
    for row in parsed.get("item2") or []:
        item_id = _clean_text(row.get("itemId"), 80)
        if not item_id:
            continue
        image_url = urllib.parse.urljoin(
            base_url, f"/map/Cctv/{urllib.parse.quote(item_id)}")
        results.append(FeedItem(
            title=f"{region} highway camera {item_id}",
            url=urllib.parse.urljoin(base_url, "/cctv"),
            source=region,
            enclosure=image_url,
            identity=f"{region}:{item_id}",
        ))
    return results


def _font(size, bold=False):
    names = (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
        if bold else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf"
        if bold else
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
    )
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def _gradient(size, top, bottom):
    image = Image.new("RGB", size, top)
    draw = ImageDraw.Draw(image)
    height = max(1, size[1] - 1)
    top_rgb = tuple(int(top[index:index + 2], 16) for index in (1, 3, 5))
    bottom_rgb = tuple(
        int(bottom[index:index + 2], 16) for index in (1, 3, 5))
    for y in range(size[1]):
        amount = y / height
        colour = tuple(
            round(top_rgb[i] * (1 - amount) + bottom_rgb[i] * amount)
            for i in range(3)
        )
        draw.line((0, y, size[0], y), fill=colour)
    return image


def _fit_text(draw, text, box, start_size, minimum=18, bold=False,
              max_lines=3):
    text = _clean_text(text, 350)
    x0, y0, x1, y1 = box
    width = x1 - x0
    height = y1 - y0
    for size in range(start_size, minimum - 1, -2):
        font = _font(size, bold=bold)
        words = text.split()
        lines = []
        current = ""
        for word in words:
            probe = f"{current} {word}".strip()
            if draw.textbbox((0, 0), probe, font=font)[2] <= width:
                current = probe
            else:
                if current:
                    lines.append(current)
                current = word
        if current:
            lines.append(current)
        if len(lines) <= max_lines:
            line_height = size + 5
            if len(lines) * line_height <= height:
                return font, lines
    font = _font(minimum, bold=bold)
    return font, [text[:55]]


def _ellipsize(draw, text, font, width):
    text = _clean_text(text, 200)
    if draw.textbbox((0, 0), text, font=font)[2] <= width:
        return text
    suffix = "..."
    while text and draw.textbbox(
            (0, 0), text + suffix, font=font)[2] > width:
        text = text[:-1].rstrip()
    return text + suffix


def _draw_glass_panel(image, box, fill=(7, 25, 51, 205),
                     outline=(160, 215, 240, 180), radius=10):
    layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer, "RGBA")
    draw.rounded_rectangle(
        box, radius=radius, fill=fill, outline=outline, width=2)
    highlight = (box[0] + 2, box[1] + 2, box[2] - 2,
                 min(box[3], box[1] + 18))
    draw.rounded_rectangle(
        highlight, radius=max(2, radius - 2), fill=(255, 255, 255, 22))
    image.alpha_composite(layer)


def _render_base(channel, stamp, size=(640, 480)):
    top, middle, accent = channel.colours
    base = _gradient(size, top, middle).convert("RGBA")
    glow = Image.new("RGBA", size, (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow, "RGBA")
    glow_draw.ellipse((-170, -160, 410, 390),
                      fill=tuple(int(accent[i:i + 2], 16)
                                 for i in (1, 3, 5)) + (78,))
    glow = glow.filter(ImageFilter.GaussianBlur(75))
    base.alpha_composite(glow)

    draw = ImageDraw.Draw(base, "RGBA")
    for x in range(-160, 800, 48):
        draw.line((x, 0, x - 180, 480), fill=(190, 225, 245, 22), width=1)
    for y in range(82, 480, 42):
        draw.line((0, y, 640, y), fill=(190, 225, 245, 15), width=1)

    draw.rectangle((0, 0, 640, 58), fill=(4, 13, 28, 224))
    draw.rectangle((0, 55, 640, 59), fill=accent)
    draw.text((24, 13), channel.name.upper(), font=_font(30, bold=True),
              fill="white")
    stamp_text = _clean_text(stamp, 40)
    stamp_width = draw.textbbox(
        (0, 0), stamp_text, font=_font(18, bold=True))[2]
    draw.text((615 - stamp_width, 18), stamp_text,
              font=_font(18, bold=True), fill=(205, 226, 241))

    draw.rectangle((0, 438, 640, 480), fill=(3, 11, 25, 236))
    draw.rectangle((0, 438, 640, 442), fill=accent)
    return base


def _draw_nasa_mark(draw, center, radius):
    cx, cy = center
    draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius),
                 fill=(20, 93, 173), outline="white", width=3)
    draw.arc((cx - radius - 8, cy - radius // 2,
              cx + radius + 8, cy + radius // 2),
             198, 342, fill=(233, 58, 55), width=5)
    draw.ellipse((cx + radius // 3, cy - radius // 2,
                  cx + radius // 3 + 5, cy - radius // 2 + 5),
                 fill="white")


def render_bulletin_frame(channel, items, output_path, generated_at=None,
                          camera_path=""):
    """Render a polished 2000s-broadcast frame and return its path."""
    generated_at = generated_at or _utc_now()
    stamp = _atlantic_stamp(generated_at)
    image = _render_base(channel, stamp)
    draw = ImageDraw.Draw(image, "RGBA")
    accent = channel.colours[2]

    if channel.key == "nasa":
        _draw_nasa_mark(draw, (560, 128), 42)
        draw.arc((420, 70, 685, 285), 145, 325,
                 fill=(125, 210, 245, 120), width=2)
        section = "MISSION UPDATE"
    elif channel.key == "news-nb":
        draw.polygon(((500, 68), (640, 68), (640, 220), (586, 181)),
                     fill=(255, 255, 255, 28))
        draw.rectangle((512, 86, 620, 154),
                       fill=(212, 47, 53, 225))
        draw.text((532, 98), "NB", font=_font(41, bold=True), fill="white")
        section = "NEW BRUNSWICK NOW"
    else:
        draw.rounded_rectangle(
            (507, 80, 620, 177), radius=18,
            fill=(245, 247, 238), outline=(30, 65, 85), width=5)
        draw.text((527, 97), "MAR", font=_font(27, bold=True),
                  fill=(18, 62, 85))
        draw.text((531, 132), "511", font=_font(28, bold=True),
                  fill=(218, 135, 32))
        section = "MARITIME ROADWATCH"

    draw.text((28, 78), section, font=_font(20, bold=True),
              fill=accent)
    _draw_glass_panel(image, (20, 108, 492, 406))
    draw = ImageDraw.Draw(image, "RGBA")

    if camera_path and os.path.isfile(camera_path):
        try:
            camera = Image.open(camera_path).convert("RGB")
            camera = ImageOps.fit(
                camera, (448, 212), method=Image.Resampling.LANCZOS)
            camera = camera.filter(ImageFilter.UnsharpMask(
                radius=1.0, percent=115, threshold=3))
            image.paste(camera, (32, 122))
            draw = ImageDraw.Draw(image, "RGBA")
            draw.rectangle((32, 122, 480, 334),
                           outline=(220, 238, 250, 210), width=2)
            headline_y = 344
        except OSError:
            headline_y = 126
    else:
        headline_y = 126

    primary = items[0].title if items else "Information unavailable"
    if camera_path:
        font, lines = _fit_text(
            draw, primary, (36, headline_y, 476, 394),
            25, minimum=18, bold=True, max_lines=2)
    else:
        font, lines = _fit_text(
            draw, primary, (38, headline_y, 476, 252),
            38, minimum=24, bold=True, max_lines=3)
    y = headline_y
    for line in lines:
        draw.text((38, y), line, font=font, fill="white")
        y += font.size + 6

    if not camera_path:
        secondary = (
            items[0].summary if items and items[0].summary else
            items[1].title if len(items) > 1 else channel.description
        )
        subfont, sublines = _fit_text(
            draw, secondary, (40, 278, 472, 385),
            22, minimum=16, max_lines=4)
        sy = 278
        for line in sublines:
            draw.text((40, sy), line, font=subfont,
                      fill=(205, 224, 238))
            sy += subfont.size + 5

    ticker = "  •  ".join(item.title for item in items[1:5])
    if not ticker:
        ticker = "SYNC WITH ROCKPOD FOR THE LATEST UPDATE"
    ticker_font = _font(18, bold=True)
    ticker = _ellipsize(draw, ticker, ticker_font, 604)
    draw.text((18, 449), ticker, font=ticker_font, fill="white")

    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    image.convert("RGB").save(output, quality=94)
    return str(output)


def render_road_capture_frame(source_path, output_path, title,
                              captured_at=None):
    """Format one genuine webcam capture as a broadcast video frame."""
    captured_at = captured_at or _utc_now()
    source = Image.open(source_path).convert("RGB")
    image = ImageOps.fit(
        source, (640, 480), method=Image.Resampling.LANCZOS)
    draw = ImageDraw.Draw(image, "RGBA")
    draw.rectangle((0, 392, 640, 480), fill=(3, 15, 28, 228))
    draw.rectangle((0, 392, 640, 397), fill=(240, 165, 43, 255))
    draw.text((22, 406), _clean_text(title, 80),
              font=_font(24, bold=True), fill="white")
    stamp = _atlantic_stamp(captured_at)
    credit = "LIVE CAPTURE  •  NS WEBCAMS  •  CC BY-NC-SA"
    if stamp:
        credit += "  •  " + stamp
    draw.text((23, 444), credit, font=_font(15, bold=True),
              fill=(187, 218, 235))
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output, quality=94)
    return str(output)


def render_channel_identity(channel, output_dir):
    """Create a guide logo and transparent on-air bug for one channel."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    logo = _gradient((240, 108), channel.colours[0],
                     channel.colours[1]).convert("RGBA")
    draw = ImageDraw.Draw(logo, "RGBA")
    draw.rounded_rectangle(
        (4, 4, 235, 103), radius=15,
        outline=(225, 239, 249), width=5)
    if channel.key == "nasa":
        _draw_nasa_mark(draw, (48, 54), 30)
        x = 88
    elif channel.key == "news-nb":
        draw.rectangle((17, 23, 78, 84), fill=(212, 47, 53))
        draw.text((24, 29), "NB", font=_font(35, bold=True), fill="white")
        x = 91
    else:
        draw.rounded_rectangle((14, 16, 83, 91), radius=13,
                               fill=(245, 247, 238),
                               outline=(240, 165, 43), width=4)
        draw.text((25, 35), "511", font=_font(24, bold=True),
                  fill=(18, 62, 85))
        x = 94
    label = channel.callsign
    font = _font(38 if len(label) <= 4 else 30, bold=True)
    draw.text((x, 34), label, font=font, fill="white")

    logo_path = output_dir / f"{channel.key}-guide.png"
    logo.save(logo_path)

    bug = logo.resize((120, 54), Image.Resampling.LANCZOS)
    bug_path = output_dir / f"{channel.key}-bug.png"
    bug.save(bug_path)
    return str(logo_path), str(bug_path)


def _probe(ffprobe, path):
    command = [
        ffprobe, "-v", "error", "-show_entries",
        "format=duration:stream=codec_type", "-of", "json", path,
    ]
    result = subprocess.run(
        command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        return 0.0, False
    try:
        payload = json.loads(result.stdout or "{}")
        duration = float((payload.get("format") or {}).get("duration") or 0)
        has_audio = any(
            stream.get("codec_type") == "audio"
            for stream in payload.get("streams") or [])
        return duration, has_audio
    except (TypeError, ValueError, json.JSONDecodeError):
        return 0.0, False


def _run_ffmpeg(command, target):
    tmp = str(target) + ".tmp"
    command = [str(value).replace("{output}", tmp) for value in command]
    try:
        os.remove(tmp)
    except FileNotFoundError:
        pass
    result = subprocess.run(
        command, capture_output=True, text=True, check=False)
    if result.returncode != 0 or not os.path.isfile(tmp) or \
            os.path.getsize(tmp) <= 0:
        try:
            os.remove(tmp)
        except FileNotFoundError:
            pass
        detail = (result.stderr or "").strip()[-500:]
        raise RuntimeError(f"video render failed: {detail}")
    os.replace(tmp, target)
    return str(target)


def _normalize_video(ffmpeg, ffprobe, source, bug, target):
    _duration, has_audio = _probe(ffprobe, source)
    inputs = [ffmpeg, "-y", "-loglevel", "error", "-i", source,
              "-loop", "1", "-i", bug]
    if not has_audio:
        inputs.extend([
            "-f", "lavfi", "-i", "anullsrc=r=44100:cl=stereo"])
    audio_index = "0:a:0" if has_audio else "2:a:0"
    graph = (
        "[0:v]scale=iw*sar:ih,"
        "scale=640:480:force_original_aspect_ratio=increase,"
        "crop=640:480,setsar=1,fps=20[base];"
        "[1:v]scale=120:54[bug];"
        "[base][bug]overlay=W-w-12:10:format=auto:shortest=1,"
        "format=yuv420p[vout]"
    )
    command = inputs + [
        "-filter_complex", graph,
        "-map", "[vout]", "-map", audio_index,
        "-c:v", "libx264", "-preset", "veryfast", "-crf", "22",
        "-c:a", "aac", "-ar", "44100", "-ac", "2", "-b:a", "128k",
        "-shortest", "-f", "matroska", "{output}",
    ]
    return _run_ffmpeg(command, target)


def _normalize_still(ffmpeg, source, target, seconds=10):
    command = [
        ffmpeg, "-y", "-loglevel", "error",
        "-loop", "1", "-t", str(seconds), "-i", source,
        "-f", "lavfi", "-t", str(seconds), "-i",
        "anullsrc=r=44100:cl=stereo",
        "-filter_complex",
        "[0:v]scale=656:492,"
        "zoompan=z='min(zoom+0.00055,1.025)':"
        "x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':"
        "d=200:s=640x480:fps=20,format=yuv420p[vout]",
        "-map", "[vout]", "-map", "1:a",
        "-c:v", "libx264", "-preset", "veryfast", "-crf", "21",
        "-c:a", "aac", "-ar", "44100", "-ac", "2", "-b:a", "96k",
        "-shortest", "-f", "matroska", "{output}",
    ]
    return _run_ffmpeg(command, target)


def _build_camera_capture(ffmpeg, frames, target, seconds_per_frame=2.5):
    """Turn chronologically captured webcam frames into a real timelapse."""
    if len(frames) < 2:
        raise RuntimeError("camera capture needs at least two frames")
    listing = str(target) + ".frames"
    lines = []
    for frame in frames:
        escaped = str(frame).replace("'", "'\\''")
        lines.append(f"file '{escaped}'")
        lines.append(f"duration {seconds_per_frame:.3f}")
    escaped = str(frames[-1]).replace("'", "'\\''")
    lines.append(f"file '{escaped}'")
    atomic_write_text(listing, "\n".join(lines) + "\n")
    command = [
        ffmpeg, "-y", "-loglevel", "error",
        "-safe", "0", "-f", "concat", "-i", listing,
        "-vf", "fps=20,setsar=1,format=yuv420p",
        "-an", "-c:v", "libx264", "-preset", "veryfast", "-crf", "21",
        "-f", "matroska", "{output}",
    ]
    try:
        return _run_ffmpeg(command, target)
    finally:
        try:
            os.remove(listing)
        except FileNotFoundError:
            pass


def _build_reel(ffmpeg, ffprobe, segments, target, seconds):
    valid = []
    duration = 0.0
    for segment in segments:
        item_duration, _audio = _probe(ffprobe, segment)
        if item_duration > 0:
            valid.append((segment, item_duration))
            duration += item_duration
    if not valid:
        raise RuntimeError("no playable information segments were produced")

    listing = str(target) + ".concat"
    lines = []
    covered = 0.0
    index = 0
    while covered < seconds + max(item[1] for item in valid):
        path, item_duration = valid[index % len(valid)]
        escaped = path.replace("'", "'\\''")
        lines.append(f"file '{escaped}'")
        covered += item_duration
        index += 1
    atomic_write_text(listing, "\n".join(lines) + "\n")
    command = [
        ffmpeg, "-y", "-loglevel", "error",
        "-safe", "0", "-f", "concat", "-i", listing,
        "-t", str(seconds), "-c", "copy", "-f", "matroska", "{output}",
    ]
    return _run_ffmpeg(command, target)


def _download_with_ytdlp(binary, url, target):
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp_template = str(target.with_suffix("")) + ".download.%(ext)s"
    command = [
        binary,
        "--no-playlist",
        "--no-progress",
        "--no-warnings",
        "--max-filesize", "256M",
        "-f", "worst[ext=mp4]/worst",
        "-o", tmp_template,
        url,
    ]
    result = subprocess.run(
        command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise RuntimeError((result.stderr or result.stdout or "").strip()[-400:])
    stem = str(target.with_suffix("")) + ".download."
    candidates = [
        path for path in target.parent.iterdir()
        if str(path).startswith(stem) and path.is_file()
    ]
    if not candidates:
        raise RuntimeError("video downloader produced no file")
    source = max(candidates, key=lambda path: path.stat().st_size)
    os.replace(source, target)
    for candidate in candidates:
        try:
            candidate.unlink()
        except FileNotFoundError:
            pass
    return str(target)


class TvInformationService:
    """Fetch and render the three renewable information-channel reels."""

    def __init__(self, config):
        self.config = config
        cache_dir = Path(str(_config_value(
            config, "cache_dir", Path.home() / ".rockpod" / "cache")))
        self.root = cache_dir / "tv-information"
        self.root.mkdir(parents=True, exist_ok=True)
        self.ffmpeg = str(_config_value(config, "ffmpeg_binary", "ffmpeg"))
        self.ffprobe = str(_config_value(config, "ffprobe_binary", "ffprobe"))
        self.ytdlp = str(_config_value(
            config, "youtube_movie_binary", "yt-dlp"))

    def enabled_channels(self):
        if not bool(_config_value(
                self.config, "tv_information_enabled", True)):
            return []
        return [
            channel for channel in CHANNELS
            if bool(_config_value(
                self.config,
                f"tv_information_{channel.key.replace('-', '_')}_enabled",
                True,
            ))
        ]

    def refresh(self, progress=None, force=False) -> TvInformationResult:
        generated = _utc_now()
        result = TvInformationResult(
            generated_at=generated, output_root=str(self.root))
        channels = self.enabled_channels()
        total = max(1, len(channels))
        for index, channel in enumerate(channels):
            if progress:
                progress(index, total, f"Refreshing {channel.name}...")
            try:
                media, logo, preview, warnings = self._refresh_channel(
                    channel, generated, force=force)
                result.media.append(media)
                result.logos[channel.category] = logo
                result.previews[channel.category] = preview
                result.warnings.extend(warnings)
            except Exception as exc:
                logger.exception(
                    "Could not refresh TV information channel %s",
                    channel.key)
                result.warnings.append(f"{channel.name}: {exc}")
        if progress:
            progress(total, total, "TV information ready")
        self._write_manifest(result)
        return result

    def _refresh_channel(self, channel, generated, force=False):
        channel_dir = self.root / channel.key
        source_dir = channel_dir / "source"
        normalized_dir = channel_dir / "normalized"
        graphics_dir = channel_dir / "graphics"
        for directory in (source_dir, normalized_dir, graphics_dir):
            directory.mkdir(parents=True, exist_ok=True)

        logo, bug = render_channel_identity(channel, graphics_dir)
        warnings = []
        if channel.key == "nasa":
            items, sources, source_warnings = self._nasa_sources(source_dir)
            camera_paths = []
        elif channel.key == "news-nb":
            items, sources, source_warnings = self._news_sources(source_dir)
            camera_paths = []
        else:
            items, sources, camera_paths, source_warnings = \
                self._road_sources(source_dir)
        warnings.extend(source_warnings)

        signature = hashlib.sha256(json.dumps(
            {
                "items": [
                    (item.stable_id(), item.title, item.published)
                    for item in items],
                "sources": [
                    (str(path), os.path.getsize(path), _file_digest(path))
                    for path in sources if os.path.isfile(path)],
                "cameras": [
                    (str(path), os.path.getsize(path), _file_digest(path))
                    for path in camera_paths if os.path.isfile(path)],
                "visual_profile": 3,
                "seconds": self.block_seconds,
            },
            sort_keys=True,
        ).encode("utf-8")).hexdigest()
        signature_path = channel_dir / "signature.txt"
        target = channel_dir / f"{channel.key}-current.mkv"
        if (
            not force and target.is_file() and target.stat().st_size > 0 and
            signature_path.is_file() and
            signature_path.read_text(encoding="utf-8").strip() == signature
        ):
            preview = graphics_dir / "preview.jpg"
            return (
                TvInformationMedia(
                    channel=channel, path=str(target),
                    generated_at=generated, source_items=items),
                logo, str(preview), warnings,
            )

        frames = []
        if channel.key == "road-mar" and camera_paths:
            for idx, camera in enumerate(camera_paths[:6]):
                frame = graphics_dir / f"camera-{idx}.jpg"
                frame_item = items[idx:idx + 5] or items
                render_bulletin_frame(
                    channel, frame_item, frame,
                    generated_at=generated, camera_path=camera)
                frames.append(str(frame))
        if not frames:
            page_count = min(4, max(1, len(items)))
            for idx in range(page_count):
                rotated = items[idx:] + items[:idx]
                frame = graphics_dir / f"bulletin-{idx}.jpg"
                render_bulletin_frame(
                    channel, rotated, frame, generated_at=generated)
                frames.append(str(frame))
        preview = graphics_dir / "preview.jpg"
        shutil.copy2(frames[0], preview)

        slate_segments = []
        for idx, frame in enumerate(frames):
            target_segment = normalized_dir / f"slate-{idx}.mkv"
            _normalize_still(
                self.ffmpeg, frame, str(target_segment),
                seconds=10 if channel.key != "road-mar" else 12)
            slate_segments.append(str(target_segment))
        video_segments = []
        for idx, source in enumerate(sources[:self.video_limit]):
            target_segment = normalized_dir / f"video-{idx}.mkv"
            try:
                _normalize_video(
                    self.ffmpeg, self.ffprobe, source, bug,
                    str(target_segment))
                video_segments.append(str(target_segment))
            except Exception as exc:
                warnings.append(
                    f"{channel.name} video {idx + 1} skipped: {exc}")

        if channel.key == "road-mar" and video_segments:
            segments = []
            for idx in range(max(len(video_segments), len(slate_segments))):
                if idx < len(video_segments):
                    segments.append(video_segments[idx])
                if idx < len(slate_segments):
                    segments.append(slate_segments[idx])
        else:
            segments = slate_segments + video_segments

        _build_reel(
            self.ffmpeg, self.ffprobe, segments, str(target),
            self.block_seconds)
        atomic_write_text(signature_path, signature + "\n")
        self._prune_numbered_files(
            normalized_dir, {"slate-", "video-"},
            {Path(path).name for path in segments})
        return (
            TvInformationMedia(
                channel=channel, path=str(target),
                generated_at=generated, source_items=items),
            logo, str(preview), warnings,
        )

    @property
    def block_seconds(self):
        try:
            value = int(_config_value(
                self.config, "tv_information_block_seconds",
                TV_INFORMATION_BLOCK_SECONDS))
        except (TypeError, ValueError):
            value = TV_INFORMATION_BLOCK_SECONDS
        return max(300, min(3600, value))

    @property
    def video_limit(self):
        try:
            value = int(_config_value(
                self.config, "tv_information_video_limit", 3))
        except (TypeError, ValueError):
            value = 3
        return max(1, min(6, value))

    def _nasa_sources(self, source_dir):
        warnings = []
        items = []
        api_url = str(_config_value(
            self.config, "tv_information_nasa_search_api",
            NASA_SEARCH_API))
        try:
            query = urllib.parse.urlencode({
                "q": "mission",
                "media_type": "video",
                "page_size": max(12, self.video_limit * 4),
                "year_start": datetime.now(timezone.utc).year - 1,
            })
            payload = json.loads(_request_bytes(
                f"{api_url}?{query}").decode("utf-8"))
            rows = (
                (payload.get("collection") or {}).get("items") or [])
            rows = sorted(
                rows,
                key=lambda row: str(
                    ((row.get("data") or [{}])[0]).get(
                        "date_created") or ""),
                reverse=True,
            )
            for row in rows:
                data = (row.get("data") or [{}])[0]
                nasa_id = _clean_text(data.get("nasa_id"), 500)
                collection_url = str(row.get("href") or "")
                if not nasa_id or not collection_url:
                    continue
                parsed_url = urllib.parse.urlsplit(collection_url)
                collection_url = urllib.parse.urlunsplit((
                    parsed_url.scheme,
                    parsed_url.netloc,
                    urllib.parse.quote(
                        urllib.parse.unquote(parsed_url.path), safe="/%"),
                    parsed_url.query,
                    parsed_url.fragment,
                ))
                assets = json.loads(_request_bytes(
                    collection_url, max_bytes=512 * 1024).decode("utf-8"))
                mp4s = [
                    str(asset).replace("http://", "https://", 1)
                    for asset in assets
                    if str(asset).lower().endswith(".mp4")
                ]
                preferred = next((
                    asset for marker in ("~mobile.mp4", "~small.mp4",
                                         "~preview.mp4", "~medium.mp4")
                    for asset in mp4s if asset.lower().endswith(marker)
                ), mp4s[0] if mp4s else "")
                if not preferred:
                    continue
                items.append(FeedItem(
                    title=_clean_text(data.get("title")) or nasa_id,
                    url=f"https://images.nasa.gov/details/"
                        f"{urllib.parse.quote(nasa_id)}",
                    published=_clean_text(data.get("date_created")),
                    summary=_clean_text(
                        data.get("description_508") or
                        data.get("description")),
                    source="NASA Image and Video Library",
                    enclosure=preferred,
                    identity=nasa_id,
                ))
                if len(items) >= self.video_limit:
                    break
        except Exception as exc:
            warnings.append(f"NASA video library unavailable: {exc}")

        if not items:
            feed_url = str(_config_value(
                self.config, "tv_information_nasa_video_feed",
                NASA_VIDEO_FEED))
            try:
                items = parse_feed(
                    _request_bytes(feed_url), source="NASA/JPL")
            except Exception as exc:
                warnings.append(f"NASA fallback feed unavailable: {exc}")
                items = self._cached_items(source_dir)
        selected = [item for item in items if item.enclosure][:self.video_limit]
        sources = []
        for idx, item in enumerate(selected):
            extension = Path(urllib.parse.urlparse(
                item.enclosure).path).suffix.lower()
            if extension not in {".mp4", ".m4v", ".mov", ".webm"}:
                extension = ".mp4"
            target = source_dir / f"video-{idx}{extension}"
            identity_path = source_dir / f"video-{idx}.id"
            identity = item.stable_id()
            try:
                try:
                    current_id = identity_path.read_text(
                        encoding="utf-8").strip()
                except OSError:
                    current_id = ""
                if (
                    current_id != identity or not target.is_file() or
                    target.stat().st_size <= 0
                ):
                    _download_atomic(item.enclosure, target)
                    atomic_write_text(identity_path, identity + "\n")
                sources.append(str(target))
            except Exception as exc:
                warnings.append(f"NASA video download failed: {exc}")
        self._prune_numbered_files(
            source_dir, {"video-"},
            {
                Path(path).name for path in sources
            } | {
                f"video-{idx}.id" for idx in range(len(sources))
            },
        )
        self._write_items(source_dir, items)
        return items, sources, warnings

    def _news_sources(self, source_dir):
        warnings = []
        feed_url = str(_config_value(
            self.config, "tv_information_news_headline_feed",
            NEWS_HEADLINE_FEED))
        page_url = str(_config_value(
            self.config, "tv_information_news_video_page",
            NEWS_VIDEO_PAGE))
        try:
            headlines = parse_feed(
                _request_bytes(feed_url), source="Canada News")
        except Exception as exc:
            warnings.append(f"New Brunswick headline feed unavailable: {exc}")
            headlines = self._cached_items(source_dir)
        try:
            videos = parse_news_video_page(
                _request_bytes(page_url), base_url=page_url)
        except Exception as exc:
            warnings.append(f"New Brunswick video page unavailable: {exc}")
            videos = []

        sources = []
        if bool(_config_value(
                self.config, "tv_information_news_download_video", True)):
            for idx, item in enumerate(videos[:self.video_limit]):
                target = source_dir / f"video-{idx}.mp4"
                identity_path = source_dir / f"video-{idx}.id"
                identity = item.stable_id()
                try:
                    current_id = identity_path.read_text(
                        encoding="utf-8").strip()
                except OSError:
                    current_id = ""
                try:
                    if current_id != identity or not target.is_file():
                        _download_with_ytdlp(self.ytdlp, item.url, target)
                        atomic_write_text(identity_path, identity + "\n")
                    sources.append(str(target))
                except Exception as exc:
                    warnings.append(
                        f"New Brunswick video download failed: {exc}")
        self._prune_numbered_files(
            source_dir, {"video-"},
            {
                Path(path).name for path in sources
            } | {
                f"video-{idx}.id" for idx in range(len(sources))
            },
        )
        merged = headlines[:12] or videos[:12]
        self._write_items(source_dir, merged)
        return merged, sources, warnings

    def _road_sources(self, source_dir):
        warnings = []
        official_items = []
        camera_paths = []
        capture_items, capture_sources, capture_warnings = \
            self._road_capture_sources(source_dir)
        warnings.extend(capture_warnings)
        max_cameras = int(_config_value(
            self.config, "tv_information_road_camera_limit", 6) or 6)
        max_cameras = max(1, min(12, max_cameras))
        configured = _config_value(
            self.config, "tv_information_road_camera_pages", None)
        pages = ROAD_CAMERA_PAGES
        if isinstance(configured, (list, tuple)) and configured:
            pages = tuple(
                (str(row.get("region") or "Maritimes"),
                 str(row.get("url") or ""))
                for row in configured if isinstance(row, dict)
            )
        per_region = max(1, (max_cameras + max(1, len(pages)) - 1)
                         // max(1, len(pages)))
        for region, page_url in pages:
            if not page_url:
                continue
            try:
                candidates = parse_camera_page(
                    _request_bytes(page_url), page_url, region)
                if not candidates and "511." in page_url:
                    api_url = urllib.parse.urljoin(
                        page_url, "/map/mapIcons/Cameras")
                    candidates = camera_api_items(
                        _request_bytes(api_url), page_url, region)
            except Exception as exc:
                warnings.append(f"{region} camera page unavailable: {exc}")
                continue
            region_added = 0
            for candidate in candidates:
                if len(camera_paths) >= max_cameras:
                    break
                if region_added >= per_region:
                    break
                suffix = Path(urllib.parse.urlparse(
                    candidate.enclosure).path).suffix.lower()
                if suffix not in {".jpg", ".jpeg", ".png"}:
                    suffix = ".jpg"
                target = source_dir / (
                    f"camera-{len(camera_paths)}{suffix}")
                try:
                    _download_atomic(
                        candidate.enclosure, target,
                        max_bytes=12 * 1024 * 1024)
                    with Image.open(target) as image:
                        image.verify()
                    camera_paths.append(str(target))
                    official_items.append(candidate)
                    region_added += 1
                except Exception as exc:
                    warnings.append(
                        f"{region} camera image skipped: {exc}")
            if len(camera_paths) >= max_cameras:
                break
        items = official_items + capture_items
        if not items:
            items = self._cached_items(source_dir)
        self._prune_numbered_files(
            source_dir, {"camera-", "capture-"},
            {
                Path(path).name
                for path in list(camera_paths) + list(capture_sources)
            },
        )
        self._write_items(source_dir, items)
        return items, capture_sources, camera_paths, warnings

    def _road_capture_sources(self, source_dir):
        """Capture short, genuine traffic-camera timelapses."""
        if not bool(_config_value(
                self.config, "tv_information_road_capture_enabled", True)):
            return [], [], []

        frame_count = int(_config_value(
            self.config, "tv_information_road_capture_frames", 3) or 3)
        frame_count = max(2, min(6, frame_count))
        interval = float(_config_value(
            self.config, "tv_information_road_capture_interval", 20) or 20)
        interval = max(1.0, min(60.0, interval))
        configured = _config_value(
            self.config, "tv_information_road_capture_cameras", None)
        cameras = ROAD_CAPTURE_CAMERAS
        if isinstance(configured, (list, tuple)) and configured:
            cameras = tuple(
                (
                    str(row.get("title") or "Maritime traffic camera"),
                    str(row.get("url") or ""),
                )
                for row in configured if isinstance(row, dict)
            )
        cameras = tuple(row for row in cameras if row[1])
        groups = [[] for _camera in cameras]
        warnings = []

        for frame_index in range(frame_count):
            round_started = time.monotonic()
            captured_at = _utc_now()
            for camera_index, (title, url) in enumerate(cameras):
                raw = Path(source_dir) / (
                    f"capture-{camera_index}-{frame_index}-raw.jpg")
                frame = Path(source_dir) / (
                    f"capture-{camera_index}-{frame_index}.jpg")
                try:
                    separator = "&" if "?" in url else "?"
                    fresh_url = (
                        f"{url}{separator}rockpod={int(time.time())}")
                    _download_atomic(
                        fresh_url, raw, max_bytes=8 * 1024 * 1024)
                    with Image.open(raw) as image:
                        image.verify()
                    render_road_capture_frame(
                        raw, frame, title, captured_at=captured_at)
                    groups[camera_index].append(str(frame))
                except Exception as exc:
                    warnings.append(
                        f"{title} live capture {frame_index + 1} "
                        f"skipped: {exc}")
            if frame_index + 1 < frame_count:
                elapsed = time.monotonic() - round_started
                time.sleep(max(0.0, interval - elapsed))

        items = []
        sources = []
        for camera_index, ((title, url), frames) in enumerate(
                zip(cameras, groups)):
            if len(frames) < 2:
                warnings.append(
                    f"{title} did not return enough fresh frames for video")
                continue
            target = Path(source_dir) / f"capture-{camera_index}.mkv"
            try:
                _build_camera_capture(
                    self.ffmpeg, frames, target,
                    seconds_per_frame=2.5)
                sources.append(str(target))
                items.append(FeedItem(
                    title=title,
                    url=url,
                    source="Nova Scotia Webcams",
                    summary="Live Halifax bridge traffic camera.",
                    identity="road-capture:" + url,
                ))
            except Exception as exc:
                warnings.append(f"{title} video build skipped: {exc}")
        return items, sources, warnings

    @staticmethod
    def _write_items(source_dir, items):
        payload = [
            {
                "title": item.title,
                "url": item.url,
                "published": item.published,
                "summary": item.summary,
                "source": item.source,
                "enclosure": item.enclosure,
                "identity": item.identity,
            }
            for item in items[:40]
        ]
        atomic_write_json(Path(source_dir) / "items.json", payload)

    @staticmethod
    def _cached_items(source_dir):
        try:
            payload = json.loads(
                (Path(source_dir) / "items.json").read_text(
                    encoding="utf-8"))
        except (OSError, ValueError):
            return []
        return [
            FeedItem(**{
                key: row.get(key, "")
                for key in FeedItem.__dataclass_fields__
            })
            for row in payload if isinstance(row, dict)
        ]

    @staticmethod
    def _prune_numbered_files(directory, prefixes, keep):
        for path in Path(directory).iterdir():
            if path.name in keep:
                continue
            if any(path.name.startswith(prefix) for prefix in prefixes):
                try:
                    path.unlink()
                except OSError:
                    pass

    def _write_manifest(self, result):
        payload = {
            "version": TV_INFORMATION_VERSION,
            "generated_at": result.generated_at,
            "channels": [
                {
                    "key": media.channel.key,
                    "category": media.channel.category,
                    "callsign": media.channel.callsign,
                    "name": media.channel.name,
                    "programme": media.path,
                    "source_items": [
                        {
                            "title": item.title,
                            "url": item.url,
                            "published": item.published,
                            "source": item.source,
                        }
                        for item in media.source_items[:12]
                    ],
                }
                for media in result.media
            ],
            "warnings": result.warnings,
        }
        atomic_write_json(self.root / "manifest.json", payload)


def format_tv_information_report(result):
    lines = [
        f"Output: {result.output_root}",
        f"Channels: {len(result.media)}",
    ]
    for media in result.media:
        lines.append(
            f"{media.channel.callsign}: "
            f"{len(media.source_items)} current items")
    lines.extend(f"Warning: {warning}" for warning in result.warnings)
    return "\n".join(lines)

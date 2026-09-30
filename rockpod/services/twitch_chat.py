"""Download and pack Twitch VOD chat replay for the offline iPod player."""

from __future__ import annotations

import json
import os
import re
import struct
import unicodedata
from concurrent.futures import ThreadPoolExecutor, as_completed
from io import BytesIO
from pathlib import Path
from urllib.parse import quote
from urllib.request import Request, urlopen

from PIL import Image


TWITCH_GQL_URL = "https://gql.twitch.tv/gql"
# Twitch's public web client and persisted replay query. This is the same
# anonymous data used by twitch.tv's VOD page; it needs no user credentials.
TWITCH_WEB_CLIENT_ID = "kimne78kx3ncx6brgo4mv6wki5h1ko"
TWITCH_COMMENTS_QUERY = (
    "b70a3591ff0f4e0313d126c6a1502d79a1c02baebb288227c582044aa76adf6a"
)
TWITCH_EMOTE_URL = (
    "https://static-cdn.jtvnw.net/emoticons/v2/{}/default/dark/1.0"
)
TWEMOJI_URL = (
    "https://cdn.jsdelivr.net/gh/jdecked/twemoji@latest/assets/72x72/{}.png"
)
CHAT_MAGIC = "# rockpod-twitch-chat-v2"
EMOJI_MAGIC = b"TWE1"
EMOJI_SIZE = 14
MAX_MESSAGES = 50_000
MAX_PAGES = 2_000
MAX_SPRITES = 4_096
MAX_MESSAGE_CHARS = 220
_TOKEN_RE = re.compile(r"~E([0-9A-F]{4})~")


def _http_bytes(url, *, payload=None, headers=None, timeout=30):
    request = Request(
        url,
        data=payload,
        headers={
            "User-Agent": "RockPod/1.0 Twitch VOD chat sync",
            **(headers or {}),
        },
    )
    with urlopen(request, timeout=timeout) as response:
        return response.read()


def _fetch_comment_page(video_id, cursor=None, content_offset=0):
    variables = {"videoID": str(video_id)}
    if cursor:
        variables["cursor"] = str(cursor)
    else:
        variables["contentOffsetSeconds"] = max(0, int(content_offset))
    body = json.dumps({
        "operationName": "VideoCommentsByOffsetOrCursor",
        "variables": variables,
        "extensions": {
            "persistedQuery": {
                "version": 1,
                "sha256Hash": TWITCH_COMMENTS_QUERY,
            }
        },
    }, separators=(",", ":")).encode("utf-8")
    raw = _http_bytes(
        TWITCH_GQL_URL,
        payload=body,
        headers={
            "Client-ID": TWITCH_WEB_CLIENT_ID,
            "Content-Type": "application/json",
        },
    )
    return json.loads(raw.decode("utf-8"))


def _is_emoji_base(codepoint):
    return (
        0x1F000 <= codepoint <= 0x1FAFF
        or 0x2600 <= codepoint <= 0x27BF
        or 0x1F1E6 <= codepoint <= 0x1F1FF
        or codepoint in {0x00A9, 0x00AE, 0x203C, 0x2049, 0x2122, 0x2139,
                         0x3030, 0x303D, 0x3297, 0x3299}
    )


def _take_emoji_cluster(text, start):
    """Return the end of one emoji grapheme, or ``start`` for normal text."""
    length = len(text)
    codepoint = ord(text[start])
    if text[start] in "#*0123456789":
        end = start + 1
        if end < length and ord(text[end]) == 0xFE0F:
            end += 1
        return end + 1 if end < length and ord(text[end]) == 0x20E3 else start
    if not _is_emoji_base(codepoint):
        return start
    end = start + 1
    if 0x1F1E6 <= codepoint <= 0x1F1FF and end < length:
        if 0x1F1E6 <= ord(text[end]) <= 0x1F1FF:
            return end + 1
    while end < length and ord(text[end]) in range(0xFE00, 0xFE10):
        end += 1
    if end < length and 0x1F3FB <= ord(text[end]) <= 0x1F3FF:
        end += 1
    while end < length and ord(text[end]) == 0x200D:
        if end + 1 >= length or not _is_emoji_base(ord(text[end + 1])):
            break
        end += 2
        while end < length and ord(text[end]) in range(0xFE00, 0xFE10):
            end += 1
        if end < length and 0x1F3FB <= ord(text[end]) <= 0x1F3FF:
            end += 1
    while end < length and 0xE0020 <= ord(text[end]) <= 0xE007F:
        end += 1
    return end


def _twemoji_name(cluster):
    # Twemoji filenames omit the presentation selector. Keycaps and ZWJ
    # sequences keep their semantic code points intact.
    return "-".join(
        f"{ord(character):x}" for character in cluster
        if ord(character) != 0xFE0F
    )


def _plain_parts(text):
    parts = []
    plain = []

    def flush():
        if plain:
            parts.append(("text", "".join(plain)))
            plain.clear()

    index = 0
    while index < len(text):
        end = _take_emoji_cluster(text, index)
        if end == index:
            plain.append(text[index])
            index += 1
            continue
        flush()
        cluster = text[index:end]
        name = _twemoji_name(cluster)
        parts.append(("sprite", f"unicode:{name}", cluster,
                      TWEMOJI_URL.format(name)))
        index = end
    flush()
    return parts


def _ascii(value, limit):
    normalized = unicodedata.normalize("NFKD", str(value or ""))
    normalized = normalized.encode("ascii", "ignore").decode("ascii")
    normalized = " ".join(normalized.replace("\t", " ").split())
    return normalized[:limit]


def _ascii_fragment(value, limit):
    """Sanitize chat text without discarding spacing between fragments."""
    normalized = unicodedata.normalize("NFKD", str(value or ""))
    normalized = normalized.encode("ascii", "ignore").decode("ascii")
    normalized = re.sub(r"\s+", " ", normalized)
    return normalized[:limit]


def _message_parts(node):
    message = (node or {}).get("message") or {}
    parts = []
    for fragment in message.get("fragments") or []:
        text = str((fragment or {}).get("text") or "")
        emote = (fragment or {}).get("emote") or {}
        emote_id = str(emote.get("emoteID") or "")
        if emote_id:
            parts.append((
                "sprite", f"twitch:{emote_id}", f":{text}:",
                TWITCH_EMOTE_URL.format(quote(emote_id, safe="")),
            ))
        else:
            parts.extend(_plain_parts(text))
    return parts


def _connection(payload):
    video = ((payload or {}).get("data") or {}).get("video") or {}
    return video.get("comments") or {}


def fetch_chat_messages(video_id, *, page_fetcher=None,
                        max_pages=MAX_PAGES, max_messages=MAX_MESSAGES,
                        progress_callback=None):
    """Fetch all public replay comments, retaining timestamped message parts."""
    fetch = page_fetcher or _fetch_comment_page
    offset_paging = page_fetcher is None
    progress = progress_callback or (lambda _count: None)
    cursor = None
    content_offset = 0
    seen_cursors = set()
    seen_ids = set()
    messages = []
    for _page in range(max_pages):
        if offset_paging:
            connection = _connection(
                fetch(str(video_id), None, content_offset)
            )
        else:
            connection = _connection(fetch(str(video_id), cursor))
        edges = connection.get("edges") or []
        if not edges:
            break
        for edge in edges:
            node = (edge or {}).get("node") or {}
            message_id = str(node.get("id") or "")
            if message_id and message_id in seen_ids:
                continue
            if message_id:
                seen_ids.add(message_id)
            commenter = node.get("commenter") or {}
            message = node.get("message") or {}
            messages.append({
                "offset": max(0, int(float(node.get("contentOffsetSeconds") or 0))),
                "user": _ascii(
                    commenter.get("displayName") or commenter.get("login") or "viewer",
                    24,
                ),
                "color": str(message.get("userColor") or "#B8B8C0"),
                "parts": _message_parts(node),
            })
            if len(messages) >= max_messages:
                break
        progress(len(messages))
        if len(messages) >= max_messages or not (
            connection.get("pageInfo") or {}
        ).get("hasNextPage"):
            break
        if offset_paging:
            next_offset = max(
                int(float(((edge or {}).get("node") or {}).get(
                    "contentOffsetSeconds"
                ) or 0))
                for edge in edges
            ) + 1
            if next_offset <= content_offset:
                break
            content_offset = next_offset
            continue
        next_cursor = str((edges[-1] or {}).get("cursor") or "")
        if not next_cursor or next_cursor in seen_cursors:
            break
        seen_cursors.add(next_cursor)
        cursor = next_cursor
    messages.sort(key=lambda item: item["offset"])
    return messages


def _sprite_rgba(raw):
    with Image.open(BytesIO(raw)) as source:
        source.seek(0)
        image = source.convert("RGBA")
    image.thumbnail((EMOJI_SIZE, EMOJI_SIZE), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (EMOJI_SIZE, EMOJI_SIZE), (0, 0, 0, 0))
    canvas.alpha_composite(
        image,
        ((EMOJI_SIZE - image.width) // 2, (EMOJI_SIZE - image.height) // 2),
    )
    return canvas.tobytes()


def _download_sprites(requests, asset_fetcher=None):
    fetch = asset_fetcher or (lambda url: _http_bytes(url, timeout=20))
    completed = {}

    def load(item):
        key, url = item
        return key, _sprite_rgba(fetch(url))

    with ThreadPoolExecutor(max_workers=8) as executor:
        futures = [executor.submit(load, item) for item in requests.items()]
        for future in as_completed(futures):
            try:
                key, rgba = future.result()
            except Exception:
                continue
            completed[key] = rgba
    return completed


def _parse_color(value):
    text = str(value or "").strip().lstrip("#")
    return text.upper() if re.fullmatch(r"[0-9A-Fa-f]{6}", text) else "B8B8C0"


def _encoded_message(parts, sprite_indexes):
    output = []
    for kind, *values in parts:
        if kind == "text":
            output.append(_ascii_fragment(values[0], MAX_MESSAGE_CHARS))
            continue
        key, fallback, _url = values
        if key in sprite_indexes:
            output.append(f"~E{sprite_indexes[key]:04X}~")
        else:
            output.append(_ascii(fallback, 32) or "?")
    return _ascii("".join(output), MAX_MESSAGE_CHARS).replace("~ E", "~E")


def write_chat_pack(messages, chat_path, emoji_path, *, asset_fetcher=None):
    """Write the ordered TSV stream and fixed-record RGBA emoji pack."""
    requests = {}
    for message in messages:
        for kind, *values in message["parts"]:
            if kind == "sprite" and len(requests) < MAX_SPRITES:
                key, _fallback, url = values
                requests.setdefault(key, url)
    sprites = _download_sprites(requests, asset_fetcher=asset_fetcher)
    ordered_keys = [key for key in requests if key in sprites]
    indexes = {key: index for index, key in enumerate(ordered_keys)}

    chat_path = Path(chat_path)
    emoji_path = Path(emoji_path)
    chat_path.parent.mkdir(parents=True, exist_ok=True)
    emoji_path.parent.mkdir(parents=True, exist_ok=True)
    chat_tmp = Path(str(chat_path) + ".rockpod-tmp")
    emoji_tmp = Path(str(emoji_path) + ".rockpod-tmp")
    with chat_tmp.open("w", encoding="ascii", newline="\n") as handle:
        handle.write(CHAT_MAGIC + "\n")
        for message in messages:
            encoded = _encoded_message(message["parts"], indexes)
            if not encoded:
                continue
            handle.write(
                f"{message['offset']}\t{_parse_color(message['color'])}\t"
                f"{_ascii(message['user'], 24) or 'viewer'}\t{encoded}\n"
            )
    with emoji_tmp.open("wb") as handle:
        handle.write(EMOJI_MAGIC)
        handle.write(struct.pack("<HHH", len(ordered_keys), EMOJI_SIZE, EMOJI_SIZE))
        for key in ordered_keys:
            handle.write(sprites[key])
    os.replace(chat_tmp, chat_path)
    os.replace(emoji_tmp, emoji_path)
    return {
        "messages": len(messages),
        "sprites": len(ordered_keys),
        "chat_path": str(chat_path),
        "emoji_path": str(emoji_path),
    }


def download_chat_replay(video_id, chat_path, emoji_path, *,
                         page_fetcher=None, asset_fetcher=None,
                         progress_callback=None):
    messages = fetch_chat_messages(
        video_id,
        page_fetcher=page_fetcher,
        progress_callback=progress_callback,
    )
    return write_chat_pack(
        messages, chat_path, emoji_path, asset_fetcher=asset_fetcher
    )


def chat_pack_sprite_indexes(text):
    """Expose sprite references for tests and RockPod diagnostics."""
    return [int(match.group(1), 16) for match in _TOKEN_RE.finditer(str(text))]

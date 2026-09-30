"""Compile selected SRT text to the existing bounded Rockbox caption format."""
import html
from pathlib import Path
import re
import shutil
import struct
import subprocess
from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT = 288, 44


def caption_font():
    candidates = [Path('/usr/share/fonts/TTF/DejaVuSans-Bold.ttf'),
        Path('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'),
        Path('/System/Library/Fonts/Supplemental/Arial Bold.ttf')]
    if shutil.which('fc-match'):
        candidates.insert(0, Path(subprocess.check_output(
            ['fc-match', '-f', '%{file}', 'sans:style=Bold'], text=True)))
    for path in candidates:
        if path.is_file(): return ImageFont.truetype(str(path), 13)
    raise RuntimeError('A system TrueType font is required for selected captions.')


def milliseconds(text):
    h, m, s, ms = map(int, re.split('[:,.]', text))
    if m >= 60 or s >= 60 or ms >= 1000: raise ValueError('Invalid subtitle time')
    return ((h * 60 + m) * 60 + s) * 1000 + ms


def compile_srt(source, output, offset_ms=0):
    if Path(source).stat().st_size > 8 * 1024 * 1024:
        raise ValueError('Subtitle file exceeds the bounded input limit.')
    font = caption_font(); cues = []
    for block in re.split(r'\n\s*\n', Path(source).read_text(encoding='utf-8-sig').replace('\r', '').strip()):
        lines = block.splitlines()
        timing = next((i for i, line in enumerate(lines) if '-->' in line), None)
        if timing is None: raise ValueError('Malformed SRT cue')
        match = re.fullmatch(r'\s*(\d+:\d+:\d+[,.]\d{3})\s*-->\s*(\d+:\d+:\d+[,.]\d{3})\s*', lines[timing])
        if not match: raise ValueError('Unsupported SRT timing or position markup')
        start, end = (milliseconds(x) - offset_ms for x in match.groups())
        text = html.unescape(re.sub('<[^>]+>', '', ' '.join(lines[timing + 1:])))
        if end <= start or start < 0 or (cues and start < cues[-1][1]):
            raise ValueError('Overlapping or out-of-range subtitles require burn-in.')
        wrapped = ['']
        for word in text.split():
            candidate = (wrapped[-1] + ' ' + word).strip()
            if font.getlength(candidate) <= WIDTH - 8: wrapped[-1] = candidate
            elif len(wrapped) < 2 and font.getlength(word) <= WIDTH - 8: wrapped.append(word)
            else: raise ValueError('Subtitle exceeds two readable lines; use burn-in for this track.')
        cues.append((start, end, wrapped))
        if len(cues) > 20000: raise ValueError('Too many subtitle cues')
    if not cues: raise ValueError('The selected subtitle track is empty.')
    with open(output, 'wb') as destination:
        destination.write(struct.pack('<II', 0x3153464e, len(cues)))
        for start, end, lines in cues:
            mask = Image.new('L', (WIDTH, HEIGHT), 0); draw = ImageDraw.Draw(mask)
            top = 8 if len(lines) == 2 else 16
            for line in lines:
                draw.text(((WIDTH - font.getlength(line))/2, top), line, font=font,
                          fill=255, stroke_width=1, stroke_fill=32, anchor='lt')
                top += 17
            packed = bytearray(WIDTH * HEIGHT // 4)
            for i, value in enumerate(mask.tobytes()):
                shade = 0 if value == 0 else 1 if value < 80 else 2 if value < 200 else 3
                packed[i // 4] |= shade << (i % 4 * 2)
            destination.write(struct.pack('<II', start, end)); destination.write(packed)
    return len(cues)

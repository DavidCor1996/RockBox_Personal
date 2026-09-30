#!/usr/bin/env python3
"""Pack timed word JSON as two-line, TV-safe NFS1 masks and editable SRT.

Input JSON: {segments: [{words: [{start: seconds, end: seconds, text: str}]}]}.
Uses a supplied TrueType font; does not fetch subtitles or media.
"""
import argparse
import json
import pathlib
import struct
from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT = 288, 44

def lines_for(words, font):
    lines = ['']
    for word in words:
        word = word.strip()
        candidate = (lines[-1] + ' ' + word).strip()
        if font.getlength(candidate) <= WIDTH - 8:
            lines[-1] = candidate
        elif len(lines) < 2 and font.getlength(word) <= WIDTH - 8:
            lines.append(word)
        else:
            return None
    return lines

def timestamp(ms):
    return f'{ms//3600000:02}:{ms//60000%60:02}:{ms//1000%60:02},{ms%1000:03}'

def pack(source, output, font_path):
    data = json.loads(source.read_text())
    font = ImageFont.truetype(str(font_path), 13)
    cues = data.get('cues', [])
    for segment in data.get('segments', []):
        words = [w for w in segment['words'] if w['text'].strip()]
        while words:
            n = 1
            while n < len(words):
                candidate = words[:n+1]
                if candidate[-1]['end'] - candidate[0]['start'] > 6 or not lines_for([w['text'] for w in candidate], font):
                    break
                n += 1
            group, words = words[:n], words[n:]
            lines = lines_for([w['text'] for w in group], font)
            if not lines:
                raise ValueError('A word exceeds the caption width')
            start = max(0, round(group[0]['start'] * 1000))
            end = max(start + 100, round(group[-1]['end'] * 1000))
            cues.append([start, end, lines])
    cues.sort(key=lambda c: c[0])
    for i, cue in enumerate(cues[:-1]):
        cue[1] = min(cue[1], cues[i+1][0])
    cues = [c for c in cues if c[1] > c[0]]
    output.parent.mkdir(parents=True, exist_ok=True)
    srt = []
    with output.open('wb') as f:
        f.write(struct.pack('<II', 0x3153464e, len(cues)))
        for num, (start, end, lines) in enumerate(cues, 1):
            mask = Image.new('L', (WIDTH, HEIGHT), 0)
            draw = ImageDraw.Draw(mask)
            y = 8 if len(lines) == 2 else 16
            for line in lines:
                x = (WIDTH - font.getlength(line)) / 2
                # Palette: transparent, black outline, gray antialias, white.
                draw.text((x, y), line, font=font, fill=255, stroke_width=1,
                          stroke_fill=32, anchor='lt')
                y += 17
            packed = bytearray(WIDTH * HEIGHT // 4)
            for i, value in enumerate(mask.getdata()):
                shade = 0 if value == 0 else 1 if value < 80 else 2 if value < 200 else 3
                packed[i//4] |= shade << ((i%4)*2)
            f.write(struct.pack('<II', start, end))
            f.write(packed)
            srt.append(f'{num}\n{timestamp(start)} --> {timestamp(end)}\n'+ '\n'.join(lines)+'\n')
            if num == min(12, len(cues)):
                preview = Image.new('RGB', (320,240), '#444444')
                rgba = Image.new('RGBA', mask.size)
                rgba.putdata([(0,0,0,0) if v == 0 else (0,0,0,255) if v < 80 else (140,140,140,255) if v < 200 else (255,255,255,255) for v in mask.getdata()])
                preview.paste(rgba, (16,182), rgba)
                preview.save(output.with_suffix('.preview.png'))
    output.with_suffix('.srt').write_text('\n'.join(srt))
    metadata = dict(language='en',label='English',generated=True,
                    source='Local transcription of the matching source media',
                    format='NFS1',cues=len(cues),font='DejaVu Sans Bold',
                    font_pixels=13,maximum_lines=2,bottom_margin=14,
                    source_media=data.get('source',''))
    output.with_name(output.name + '.json').write_text(json.dumps(metadata,indent=2)+'\n')
    raw = output.read_bytes()
    assert len(raw) == 8 + len(cues) * (8 + WIDTH * HEIGHT // 4)
    assert all(0 <= a < b for a,b,_ in cues)
    print(output, len(cues), 'cues')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--font', type=pathlib.Path, default=pathlib.Path('/usr/share/fonts/TTF/DejaVuSans-Bold.ttf'))
    args = parser.parse_args()
    pack(args.source, args.output, args.font)

#!/usr/bin/env python3
"""Convert original MSN 7.5 resources; never synthesize replacement artwork."""
import hashlib
import json
import struct
import subprocess
from pathlib import Path
from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/ipodjs/sources/msn'
DEST = ROOT / 'assets/ipodjs/rockbox/msn'


def bitmap(image, path, size=None, background=(255, 0, 255)):
    image = image.convert('RGBA')
    if size:
        image = image.resize(size, Image.Resampling.LANCZOS)
    alpha = image.getchannel('A')
    canvas = Image.new('RGB', image.size, (255, 255, 255) if background == (255, 0, 255) else background)
    canvas.paste(image, mask=alpha)
    if background == (255, 0, 255):
        canvas.paste(background, mask=alpha.point(lambda value: 255 if value == 0 else 0))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def main():
    provenance = json.loads((SOURCE / 'PROVENANCE.json').read_text())
    for name, digest in provenance['files'].items():
        if hashlib.sha256((SOURCE / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f'Original resource checksum mismatch: {name}')
    DEST.mkdir(parents=True, exist_ok=True)
    icon = Image.open(SOURCE / 'ICON-33.ico')
    # Existing Applications geometry: 46px tile, 8px corners, inset real mark.
    # Both the mark and the blue glass surface are original MSN resources.
    for size in (46, 64, 80):
        surface = ImageOps.fit(Image.open(SOURCE / 'PNG-785.png').convert('RGBA'),
                               (size,size), Image.Resampling.LANCZOS)
        tile = Image.new('RGBA',(size,size),'white')
        tile.alpha_composite(surface)
        mark = icon.convert('RGBA')
        mark.thumbnail((round(size*.76),round(size*.76)),Image.Resampling.LANCZOS)
        tile.alpha_composite(mark,((size-mark.width)//2,(size-mark.height)//2))
        mask = Image.new('L',(size,size),0)
        ImageDraw.Draw(mask).rounded_rectangle((0,0,size-1,size-1),
                                               radius=round(size*8/46),fill=255)
        tile.putalpha(mask)
        if size == 64:
            tile.save(ROOT / 'rockpod/assets/icons/msn-official.png')
        else:
            folder = 'applications' if size == 46 else 'tv-applications'
            bitmap(tile, DEST.parent / f'{folder}/msn.{size}x{size}x24.bmp')
    bitmap(icon, DEST / 'notification.bmp', (22, 22))
    mapping = {'logo': (729, (46, 16)), 'online': (701, (20, 28)),
               'away': (719, (20, 28)), 'busy': (720, (20, 28)),
               'offline': (721, (20, 28)), 'emoticon': (1011, (19, 19)),
               'nudge': (2004, (19, 19)), 'mail': (1091, (19, 19)),
               'sound': (1018, (19, 19)), 'history': (212, (19, 19)),
               'pen': (1037, (19, 19)), 'alert': (214, (19, 19)),
               'default': (298, (48, 48))}
    for name, (resource, size) in mapping.items():
        bitmap(Image.open(SOURCE / f'PNG-{resource}.png'), DEST / f'{name}.bmp', size)
    for n in range(298, 309):
        bitmap(Image.open(SOURCE / f'PNG-{n}.png'), DEST / f'avatar-{n}.bmp', (48, 48))
    # Original contact-window background, scaled/cropped as a UI surface.
    bitmap(Image.open(SOURCE / 'PNG-785.png'), DEST / 'header.bmp', (320, 70), (238, 244, 253))
    bitmap(Image.open(SOURCE / 'PNG-786.png'), DEST / 'toolbar.bmp', (320, 28))
    frames = []
    im = Image.open(SOURCE / 'GIF-419.gif')
    for frame in range(im.n_frames):
        im.seek(frame)
        frames.append((im.copy().convert('RGBA'), max(20, im.info.get('duration', 100))))
    atlas = Image.new('RGBA', (75, 69 * len(frames)))
    for i, (frame, _) in enumerate(frames):
        atlas.paste(frame, (0, i * 69))
    bitmap(atlas, DEST / 'signin.bmp')
    (DEST / 'signin.tsv').write_text(''.join(f'{duration}\n' for _, duration in frames))
    # Preserve every pixel in original animated emoticon sprite strips.
    for n in range(900, 912):
        bitmap(Image.open(SOURCE / f'PNG-{n}.png'), DEST / f'emote-{n}.bmp')
    for rate in (44100, 48000):
        for name, source in [('message','newemailwav'),('nudge','nudgewav'),('online','onlinewav')]:
            subprocess.run(['ffmpeg','-v','error','-y','-i',str(SOURCE/source),
                            '-ar',str(rate),'-ac','2','-f','s16le',str(DEST/f'{name}-{rate}.pcm')],check=True)
    subprocess.run(["ffmpeg","-v","error","-y","-i",str(SOURCE/"newemailwav"),
                    "-ar","11025","-ac","1","-f","s16le",str(DEST/"notice-11025.pcm")],check=True)
    bitmap(ImageOps.contain(Image.open(SOURCE/"winks/select_wink.png").convert("RGBA"),(20,28)), DEST/"wink.bmp")
    bitmap(ImageOps.contain(Image.open(SOURCE/"winks/send_nudge.png").convert("RGBA"),(20,28)), DEST/"nudge.bmp")
    from prepare_msn_emoticons import main as prepare_emoticons
    prepare_emoticons()
    hashes={str(p.relative_to(DEST)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(DEST.rglob("*"))
            if p.is_file() and p.name != 'SHA256SUMS'}
    (DEST/'SHA256SUMS').write_text(''.join(f'{v}  {k}\n' for k,v in hashes.items()))


if __name__ == '__main__':
    main()

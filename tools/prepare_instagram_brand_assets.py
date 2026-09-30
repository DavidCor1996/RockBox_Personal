#!/usr/bin/env python3
"""Mechanical conversions of sourced Instagram artwork; no drawn glyphs."""
from io import BytesIO
from pathlib import Path
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/ipodjs/sources/instagram/official'
OUTPUT = ROOT / 'apps/plugins/bitmaps/native'


def main():
    source = Image.open(SOURCE / 'instagram-video-feed-2013.avif').convert('RGB')
    # Instagram 4.0 iPhone feed: preserve its original blue navigation pixels.
    nav = source.crop((1, 22, 3, 64)).resize((320, 28), Image.Resampling.LANCZOS)
    raster = subprocess.run(['rsvg-convert', '-w', '352', str(
        SOURCE / 'instagram-wordmark-2013.svg')], check=True,
        capture_output=True).stdout
    wordmark = Image.open(BytesIO(raster)).convert('RGBA')
    wordmark = wordmark.crop(wordmark.getchannel('A').getbbox())
    wordmark.thumbnail((90, 25), Image.Resampling.LANCZOS)
    nav.paste('white', ((320-wordmark.width)//2, (28-wordmark.height)//2,
                       (320+wordmark.width)//2, (28+wordmark.height)//2),
              wordmark.getchannel('A'))
    nav.save(OUTPUT / 'instagram_nav.320x28x24.bmp')
    # Home, activity-heart, profile icons from the actual 2013 application.
    tabs = source.crop((2, 445, 4, 488)).resize((320, 24), Image.Resampling.LANCZOS)
    for x, crop in zip((0, 106, 212), ((14,452,47,485),
                      (208,452,247,485), (273,452,311,485))):
        icon = source.crop(crop).convert('RGBA')
        # Key only the original button background, preserving source glyphs.
        for y in range(icon.height):
            background = min(max(icon.getpixel((i,y))[:3])
                             for i in range(icon.width))
            for i in range(icon.width):
                pixel = icon.getpixel((i,y))
                alpha = max(0, min(255, round(
                    (max(pixel[:3])-background)*255/max(1,255-background))))
                icon.putpixel((i,y), (255,255,255,alpha))
        icon.thumbnail((23,20), Image.Resampling.LANCZOS)
        tabs.paste(icon, (x+8, (24-icon.height)//2), icon)
    tabs.save(OUTPUT / 'instagram_tabs.320x24x24.bmp')


if __name__ == '__main__':
    main()

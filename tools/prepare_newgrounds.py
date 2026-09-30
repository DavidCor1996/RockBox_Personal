#!/usr/bin/env python3
"""Prepare original Newgrounds assets for the fixed 320x240 iPod screen.
Usage: prepare_newgrounds.py INPUT_DIRECTORY ROCKBOX_DIRECTORY
Inputs: skullkid.swf, skullkid-card.png, newgrounds-title.webp, tank.webp.
Requires Pillow, ImageMagick and ffmpeg. Does not fetch or replace any source artwork.
"""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zlib
from PIL import Image, ImageOps

src, dst = map(Path, sys.argv[1:])
swf = (src / 'skullkid.swf').read_bytes()
expected = '02b205ebcc9e74dbcdd8485d7567139e21d0773af1c9a60485f9698a01651dfa'
if hashlib.sha256(swf).hexdigest() != expected:
    raise SystemExit('Unexpected Skull Kid SWF; verify the original release first.')
art = dst / 'ipodjs/newgrounds'
game = dst / 'flash/skullkid'
art.mkdir(parents=True, exist_ok=True)
game.mkdir(parents=True, exist_ok=True)
shutil.copyfile(src/'skullkid.swf',game/'skullkid.swf')
cover = ImageOps.fit(Image.open(src/'skullkid-card.png').convert('RGB'),(146,82),method=Image.Resampling.LANCZOS)
cover.save(art/'skullkid.bmp')
tank = Image.open(src/'tank.webp').convert('RGB')
ImageOps.fit(tank,(146,82),method=Image.Resampling.LANCZOS).save(art/'tank.bmp')
ImageOps.fit(tank,(28,28),method=Image.Resampling.LANCZOS).save(art/'tank-icon.bmp')
wordmark = Image.new('RGB',(146,28),'black')
logo = Image.open(src/'newgrounds-title.webp').convert('RGBA')
logo.thumbnail((138,22),Image.Resampling.LANCZOS)
wordmark.paste(logo,((146-logo.width)//2,(28-logo.height)//2),logo)
wordmark.save(art/'wordmark.bmp')
loading = Image.new('RGB',(320,240),'black')
loading.paste(ImageOps.fit(Image.open(src/'skullkid-card.png').convert('RGB'),(240,135),method=Image.Resampling.LANCZOS),(40,35))
loading.paste(wordmark,(87,4))
loading.save(art/'loading.bmp')
apps=dst/'ipodjs/applications';apps.mkdir(parents=True,exist_ok=True)
# Use the exact rounded-square mask and magenta transparency key of the grid.
for size, folder in [(46, apps), (80, dst/'ipodjs/tv-applications')]:
    folder.mkdir(parents=True, exist_ok=True)
    radius=size*8//46
    subprocess.run(['magick',str(src/'tank.webp'),'-auto-orient','-filter','Lanczos',
        '-resize',f'{size}x{size}^','-gravity','center','-extent',f'{size}x{size}',
        '(', '-size',f'{size}x{size}','xc:none','-fill','white','-draw',
        f'roundrectangle 0,0 {size-1},{size-1} {radius},{radius}', ')',
        '-alpha','off','-compose','CopyOpacity','-composite','-background','magenta',
        '-alpha','remove','-alpha','off','-colorspace','sRGB','-depth','8','-type','TrueColor',
        'BMP3:'+str(folder/f'newgrounds.{size}x{size}x24.bmp')],check=True)
# Decode the actual event sounds once on the desktop; the ARM runtime mixes PCM.
raw=swf[:8]+zlib.decompress(swf[8:]) if swf[:3]==b'CWS' else swf
pos=8+(5+4*(raw[8]>>3)+7)//8+4
sounds=[]
while pos<len(raw):
    tag=struct.unpack_from('<H',raw,pos)[0];pos+=2
    kind,length=tag>>6,tag&63
    if length==63: length=struct.unpack_from('<I',raw,pos)[0];pos+=4
    body=raw[pos:pos+length];pos+=length
    if kind==14:
        ident,flags,count=struct.unpack_from('<HBI',body)
        if flags>>4 != 2: raise SystemExit('Unexpected sound format')
        # gameswf receives MP3 bytes after the signed seek field.
        mp3=body[9:]
        checksum=zlib.crc32(mp3)&0xffffffff
        pcm=game/f'{ident}.pcm'
        seek=struct.unpack_from('<h',body,7)[0]
        result=subprocess.run(['ffmpeg','-v','error','-f','mp3','-i','pipe:0','-af',f'atrim=start_sample={max(0,seek)}:end_sample={max(0,seek)+count}', '-f','s16le','-ar','44100','-ac','2','pipe:1'],input=mp3,capture_output=True,check=True)
        pcm.write_bytes(result.stdout)
        sounds.append({'id':ident,'crc32':f'{checksum:08x}','sample_count':count,'pcm_bytes':len(result.stdout)})
manifest={'title':'The Skull Kid','author':'korded / Chance Banning','date':'2002-09-14','portal':'https://www.newgrounds.com/portal/view/63747','swf':'https://uploads.ungrounded.net/63000/63747_skullkid.swf?1032198614','sha256':expected,'cover':'https://picon.ngfiles.com/63000/flash_63747_card.png?f1601088663','wordmark':'https://img.ngfiles.com/newgroundstitle.webp?cached=1707337416','tank':'https://img.ngfiles.com/defaults/icon-portal-xl.webp','note':'Original third-party artwork and game; not covered by the launcher source license. Unofficial personal adaptation.','sounds':sounds}
(game/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Prepared original game, eight sounds, and Newgrounds artwork.')

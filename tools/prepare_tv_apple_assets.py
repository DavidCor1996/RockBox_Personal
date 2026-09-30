#!/usr/bin/env python3
"""Convert user-supplied Apple TV 3.0.1 BackRow PNGs; never synthesize replacement art."""
import argparse, hashlib, json, struct
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('header',type=Path);p.add_argument('manifest',type=Path);p.add_argument('--status-font',type=Path,required=True);a=p.parse_args()
records=[];code=['/* Generated from original Apple BackRow resources. Graphics copyright Apple.',' * See docs/tv-apple-assets.json and tools/prepare_tv_apple_assets.py. */']
def read(name):
 path=a.source/(name+'.png');data=path.read_bytes();records.append({'file':path.name,'sha256':hashlib.sha256(data).hexdigest()});return Image.open(path).convert('RGBA')
def emit(name,im,size):
 im=im.resize(size,Image.Resampling.LANCZOS);data=[]
 for r,g,b,alpha in im.getdata():
  pixel=((r>>3)<<11)|((g>>2)<<5)|(b>>3);data.extend([pixel&255,pixel>>8,alpha])
 code.append('static const unsigned char '+name+'_pixels[] = {\n'+',\n'.join(','.join(map(str,data[i:i+24])) for i in range(0,len(data),24))+'\n};')
 return '{%d,%d,%s_pixels}'%(*size,name)
# Source pieces, including Apple's transparency/glow. Assemble only source
# slices, then resize together so the supplied rounded caps stay in proportion.
left=Image.new('RGBA',(45,96));middle=Image.new('RGBA',(10,96))
for dest,names in [(left,['BlueGlowSelection_TopCorner','BlueGlowSelection_Edge','BlueGlowSelection_BottomCorner']),(middle,['BlueGlowSelection_Top','BlueGlowSelection_Center','BlueGlowSelection_Bottom'])]:
 y=0
 for name in names:
  im=read(name);dest.paste(im,(0,y));y+=im.height
# Crop the original outer glow padding, retaining the actual border pixels.
left=left.crop((21,18,45,82));middle=middle.crop((0,18,10,82))
selection=[(left,(10,27)),(middle,(6,27)),(left.transpose(Image.Transpose.FLIP_LEFT_RIGHT),(10,27))]
# Avoid interleaving array declarations inside struct initializer.
def group(name,images):
 entries=[emit(name+str(i),im,size) for i,(im,size) in enumerate(images)]
 code.append('static const struct ipodjs_retailos_image '+name+'[] = {\n'+',\n'.join(entries)+'\n};')
group('tv_apple_selection',selection)
# Actual 2009 category glow, retaining its original pixels and transparency.
group('tv_apple_tab',[(read('MainMenuBarGlow').crop((0,110,470,242)),(94,26))])
group('tv_apple_tab_inactive',[(read('MainMenuBarGlowInactive').crop((0,110,470,242)),(94,26))])
# Lower dark portion of Apple's shelf gradient keeps small white TV text legible.
group('tv_apple_background',[(read('MainMenuTopGradient').crop((0,256,1280,324)),(160,17))])
group('tv_apple_music',[(read('shelf_Music'),(180,180))])
for kind,state in [('progress','OFF'),('fill','ON')]:
 group('tv_apple_'+kind,[(read('GrayProgress_'+part+'_'+state),(w,9)) for part,w in [('LeftCap',5),('Center',1),('RightCap',5)]])
for name,source in [('play','StatusPlay'),('pause','StatusPause'),('shuffle','NowPlayingShuffleIcon'),('repeat','NowPlayingRepeatIcon')]:
 group('tv_apple_'+name,[(read(source),(16,16))])
# Fixed video status alphabet copied from the already deployed Apple font.
# This avoids font-cache reads on the video thread.
data=a.status_font.read_bytes()
records.append({'file':a.status_font.name,'source':'Existing iPod RetailOS Apple Helvetica bitmap font','sha256':hashlib.sha256(data).hexdigest()})
mw,h,asc,depth,first,default,count,nb,no,nw=struct.unpack_from('<4H6I',data,4)
align=2 if nb<0xffdb else 4; offset=(36+nb+align-1)//align*align
offs=struct.unpack_from('<'+('H' if align==2 else 'I')*no,data,offset)
widths=data[offset+no*align:offset+no*align+nw]
chars=''.join(sorted(set('PlayingPaused0123456789: /')))
images=[]
for ch in chars:
 index=ord(ch)-first;w=widths[index];bits=data[36+offs[index]:]
 im=Image.new('RGBA',(w,h))
 for y in range(h):
  for x in range(w):
   pixel=y*w+x
   alpha=255-((bits[pixel//2]>>((pixel&1)*4))&15)*17 if depth else (255 if bits[(y//8)*w+x] & (1<<(y%8)) else 0)
   im.putpixel((x,y),(255,255,255,alpha))
 images.append((im,(w,h)))
group('tv_apple_status_font',images)
code.append('static const char tv_apple_status_chars[] = '+json.dumps(chars)+';')
a.header.write_text('\n'.join(code)+'\n')
a.manifest.write_text(json.dumps({'source':'Apple TV 3.0.1 (2009), 2Z694-6004-003.dmg, OSBoot/System/Library/PrivateFrameworks/BackRow.framework/Versions/A/Resources','firmware_sha256':'26d4cf4fe0abca2068672cc70fcf366eaa0737066c3710519b0a0bcbee82a1bf','download':'https://mesu.apple.com/data/OS/061-7491.20091107.TVA31/2Z694-6004-003.dmg','method':'Original RGBA bitmap pieces, source-piece assembly and Lanczos size conversion only. No generated replacement imagery.','assets':records},indent=2)+'\n')
print('Generated',a.header.stat().st_size,'bytes of source, from',len(records),'original Apple resource files')

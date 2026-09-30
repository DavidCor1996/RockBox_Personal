#!/usr/bin/env python3
"""Pack FFDec's unedited PNG shape exports for Skull Kid's native renderer.
Export first: java -jar ffdec.jar -format shape:png -export shape SHAPES skullkid.swf
Usage: prepare_skullkid_shapes.py skullkid.swf SHAPES OUTPUT.shapes
"""
from pathlib import Path
from PIL import Image
import struct, sys, zlib
swf, directory, out=map(Path,sys.argv[1:])
s=swf.read_bytes();raw=s[:8]+zlib.decompress(s[8:]) if s[:3]==b'CWS' else s
pos=8+(5+4*(raw[8]>>3)+7)//8+4
bounds={}
while pos<len(raw):
    tag=struct.unpack_from('<H',raw,pos)[0];pos+=2
    kind,length=tag>>6,tag&63
    if length==63: length=struct.unpack_from('<I',raw,pos)[0];pos+=4
    body=raw[pos:pos+length];pos+=length
    if kind in (2,22,32,83):
        ident=struct.unpack_from('<H',body)[0]
        bits=''.join(f'{v:08b}' for v in body[2:20]);n=int(bits[:5],2)
        vals=[]
        for i in range(4):
            v=int(bits[5+i*n:5+(i+1)*n],2);vals.append(v-(1<<n) if v>>(n-1) else v)
        bounds[ident]=(vals[0],vals[2])
records=[];pixels=bytearray()
for path in sorted(directory.rglob('*.png'),key=lambda p:int(p.stem)):
    ident=int(path.stem)
    im=Image.open(path).convert('RGBA')
    original_width,original_height=im.size
    # One cache pixel maps to one LCD pixel at the original 550px stage width.
    im=im.resize(((im.width*32+54)//55,(im.height*32+54)//55),Image.Resampling.LANCZOS)
    if ident not in bounds:raise ValueError(ident)
    records.append(struct.pack('<IHHiiIHH',ident,im.width,im.height,*bounds[ident],len(pixels),original_width,original_height))
    pixels.extend(im.tobytes())
out.write_bytes(b'NGS2'+struct.pack('<I',len(records))+b''.join(records)+pixels)
print(f'{len(records)} original shapes, {len(pixels)} RGBA bytes')

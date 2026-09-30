#!/usr/bin/env python3
"""Render archived Microsoft Flash timelines and their sounds, without redrawing.
Usage: prepare_msn_winks.py /path/to/ffdec.jar
Requires Java, JPEXS, ffmpeg, Pillow and numpy. Source archives are provenance-only.
"""
import hashlib,json,math,subprocess,sys,tempfile,struct,xml.etree.ElementTree as ET
from pathlib import Path
import numpy as np
from PIL import Image,ImageOps
ROOT=Path(__file__).resolve().parents[1];SRC=ROOT/'assets/ipodjs/sources/msn/winks';DST=ROOT/'assets/ipodjs/rockbox/msn/winks'
def main(jar):
    DST.mkdir(parents=True,exist_ok=True);catalog=[]
    provenance=json.loads((SRC/'PROVENANCE.json').read_text())
    with tempfile.TemporaryDirectory(prefix='msn-winks-') as work:
        work=Path(work)
        for swf in sorted(SRC.glob('*.swf')):
            assert hashlib.sha256(swf.read_bytes()).hexdigest()==provenance['files'][swf.name]
            out=work/swf.stem;out.mkdir();xml=out/'movie.xml'
            command=['java','-Djava.awt.headless=true',f'-Duser.home={work}','-jar',str(jar)]
            subprocess.run(command+['-swf2xml',str(swf),str(xml)],check=True,stdout=subprocess.DEVNULL)
            tree=ET.parse(xml).getroot();fps=float(tree.get('frameRate'));count=int(tree.get('frameCount'))
            render_source=swf
            if count==1:
                count=max(int(x.get('frameCount','1')) for x in tree.iter())
                tree.set('frameCount',str(count));tags=tree.find('tags')
                for _ in range(count-1):tags.insert(len(tags)-1,ET.Element('item',{'type':'ShowFrameTag','forceWriteAsLong':'false'}))
                ET.ElementTree(tree).write(xml,encoding='utf-8',xml_declaration=True)
                render_source=out/'expanded.swf'
                subprocess.run(command+['-xml2swf',str(xml),str(render_source)],check=True,stdout=subprocess.DEVNULL)
            subprocess.run(command+['-ignorebackground','-format','frame:png','-export','frame,sound',str(out),str(render_source)],check=True,stdout=subprocess.DEVNULL)
            frames=sorted((out/'frames').glob('*.png'),key=lambda p:int(p.stem));assert len(frames)==count
            # Resample the original timeline to <=12 fps; preserve total duration.
            output_fps=min(12,fps);indices=list(range(0,count,max(1,math.ceil(fps/output_fps))))
            with (DST/(swf.stem+'.mwa')).open('wb') as stream:
                stream.write(struct.pack('<4sHHH',b'MWA2',240,160,len(indices)))
                for n,i in enumerate(indices):
                    im=Image.open(frames[i]).convert('RGBA');im=ImageOps.contain(im,(240,160),Image.Resampling.LANCZOS)
                    canvas=Image.new('RGBA',(240,160));canvas.alpha_composite(im,((240-im.width)//2,(160-im.height)//2))
                    rgba=np.asarray(canvas);rgb=np.asarray(Image.alpha_composite(Image.new('RGBA',canvas.size,'white'),canvas).convert('RGB')).astype(np.uint16)
                    pixels=((rgb[:,:,0]>>3)<<11)|((rgb[:,:,1]>>2)<<5)|(rgb[:,:,2]>>3);pixels[rgba[:,:,3]<8]=0xf81f
                    end=indices[n+1] if n+1<len(indices) else count
                    flat=pixels.ravel();starts=np.r_[0,np.flatnonzero(flat[1:]!=flat[:-1])+1];lengths=np.diff(np.r_[starts,len(flat)])
                    runs=np.column_stack((lengths,flat[starts])).astype('<u2')
                    stream.write(struct.pack('<HH',round((end-i)*1000/fps),len(runs)));stream.write(runs.tobytes())
            # Root-timeline sound cues, mixed at their original frame positions.
            audio=np.zeros(round(count/fps*11025)+11025,dtype=np.int32);frame=0;cue_count=0
            sprites={x.get('spriteId'):x.find('subTags') for x in tree.iter('item') if x.get('type')=='DefineSpriteTag'}
            def cues(tags,offset=0,depth=0):
                if tags is None or depth>5:return
                frame=offset
                for tag in tags:
                    if tag.get('type')=='ShowFrameTag':frame+=1
                    if tag.get('type')=='StartSoundTag':yield frame,tag
                    if tag.get('type','').startswith('PlaceObject') and tag.get('placeFlagHasCharacter')=='true':
                        yield from cues(sprites.get(tag.get('characterId')),frame,depth+1)
            for frame,tag in cues(tree.find('tags')):
                sid=tag.get('soundId');matches=list((out/'sounds').glob(sid+'.*'))
                if not matches:continue
                decoded=subprocess.check_output(['ffmpeg','-v','error','-i',str(matches[0]),'-ar','11025','-ac','1','-f','s16le','-'])
                data=np.frombuffer(decoded,dtype='<i2').astype(np.int32);start=round(frame/fps*11025);end=min(len(audio),start+len(data));audio[start:end]+=data[:end-start];cue_count+=1
            # Streamed sounds are already decoded and timed by the SWF exporter.
            for stream in (out/'sounds').glob('-1.*'):
                decoded=subprocess.check_output(['ffmpeg','-v','error','-i',str(stream),'-ar','11025','-ac','1','-f','s16le','-'])
                data=np.frombuffer(decoded,dtype='<i2').astype(np.int32)
                if len(data) and np.any(data):
                    end=min(len(audio),len(data));audio[:end]+=data[:end];cue_count+=1
            pcm=np.clip(audio[:min(len(audio),200000)],-32768,32767).astype('<i2').tobytes()
            assert len(audio)*2<=400000,'Wink exceeds resident sound limit'
            (DST/(swf.stem+'.pcm')).write_bytes(pcm)
            # Preserve a representative original frame for picker previews.
            thumbnail=Image.open(frames[min(count-1,count//3)]).convert('RGB');ImageOps.pad(thumbnail,(48,36),color='white').save(DST/(swf.stem+'.bmp'))
            catalog.append({'id':swf.stem,'name':swf.stem.replace('_',' ').title(),'frames':len(indices),'duration_ms':round(count/fps*1000),'sound_cues':cue_count})
            print(swf.stem,count,'frames',cue_count,'sound cues',flush=True)
    (DST/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n')
    (DST/'catalog.tsv').write_text(''.join(f"{x['id']}\t{x['name']}\n" for x in catalog))
if __name__=='__main__':main(Path(sys.argv[1]))

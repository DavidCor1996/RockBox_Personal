#!/usr/bin/env python3
"""Run the ambient preview and restore its host in a headless real simulator."""
import argparse
from datetime import datetime
import os
from pathlib import Path
import subprocess
import time
from PIL import Image

p=argparse.ArgumentParser()
p.add_argument('--art',action='store_true')
p.add_argument('--hourly',action='store_true')
p.add_argument('--weather-file',type=Path)
p.add_argument('--build',type=Path,default=Path('build-sim-ipod6g'))
p.add_argument('--output',type=Path,default=Path('/tmp/ambient-clock-sim-gate'))
a=p.parse_args();a.output=a.output.resolve();a.build=a.build.resolve()
a.output.mkdir(parents=True,exist_ok=True)
# Each run has an isolated settings/database namespace.
import tempfile
root=Path(tempfile.mkdtemp(prefix='runtime-',dir=a.output))
rockbox=root/'.rockbox';rockbox.mkdir()
(rockbox/'config.cfg').write_text('ui engine: rockbox\nstart in screen: menu\nshow icons: off\nidle poweroff: 0\n')
weather=rockbox/'rockpod/weather';weather.mkdir(parents=True)
(weather/'forecast.tsv').write_text('rockpod_weather_v1\tHalifax (test data)\t44\t-63\tAmerica/Halifax\t2026-09-12T20:00Z\tTest\tmetric\ncurrent\t'+datetime.now().strftime('%Y-%m-%dT%H:%M')+'\tclear\tClear skies\t18\t0\t4\tNW\t1\tTest\n')
if a.art:
 from PIL import ImageOps
 covers=Path(__file__).resolve().parents[1]/'assets/ipodjs/sources/sitekick/ipod-exclusive/sourced'
 albumlist=rockbox/'albumlist';(albumlist/'slides').mkdir(parents=True)
 rows=['album_id\tthumb\tslide\tartist\talbum\ttracks\tdevice_dirs']
 for i,path in enumerate(sorted(covers.glob('*cover.jpg'))):
  ImageOps.fit(Image.open(path).convert('RGB'),(384,384),Image.Resampling.LANCZOS).save(albumlist/'slides'/f'{i}.bmp')
  rows.append(f'{i}\tslides/{i}.bmp\tslides/{i}.bmp\tTest artist\t{path.stem}\t1\tMusic')
 (albumlist/'index.tsv').write_text('\n'.join(rows)+'\n')
if a.hourly:
 cache=weather/'forecast.tsv'
 cache.write_text(cache.read_text().replace('\ncurrent\t','\nhourly\t'))
if a.weather_file:
 (weather/'forecast.tsv').write_bytes(a.weather_file.read_bytes())
keys={'select'   :'SELECT','menu':'MENU','down':'SCROLL_FWD'}
gates={k:root/(k+'.gate') for k in keys}
env=os.environ.copy();env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy',SDL_RENDER_DRIVER='software',ROCKPOD_SIM_PREVIEW_BMP=str(root/'frame.bmp'),ROCKPOD_SIM_PREVIEW_INTERVAL_MS='0')
for k,v in keys.items():env['ROCKPOD_SIM_'+v+'_GATE']=str(gates[k])
log=(a.output/'simulator.log').open('w')
proc=subprocess.Popen([str(a.build/'rockboxui'),'--zoom','1','--nobackground','--root',str(root)],env=env,stdout=log,stderr=log)
def tap(key,count=1):
 for _ in range(count):
  assert proc.poll() is None,'simulator exited'
  gates[key].touch();time.sleep(.12);gates[key].unlink();time.sleep(.5)
def snapshot():
 for _ in range(10):
  try:return Image.open(root/'frame.bmp').convert('RGB')
  except (OSError,ValueError):time.sleep(.1)
 raise AssertionError('no readable simulator frame')
def color_count():return len(snapshot().getcolors(1000000))
try:
 time.sleep(4)
 tap('down',2);tap('select') # General settings
 tap('down',4);tap('select') # Display
 tap('select');tap('select') # LCD, Ambient
 tap('down',6);tap('select') # Preview
 time.sleep(2)
 assert color_count()>100,'preview did not replace settings'
 frame=snapshot()
 weather_ink=sum(min(frame.getpixel((x,y)))>160 for y in range(171,189) for x in range(55,265))
 assert weather_ink>100,('temperature/condition line missing',weather_ink)
 group=[x for y in range(168,192) for x in range(24,296)
        if min(frame.getpixel((x,y))[:2])>160]
 assert group and abs((min(group)+max(group))/2-160)<=2,'weather group off center'
 # Visible ink, rather than nominal character advances, must be centered.
 for name,top,bottom,threshold in [('date',20,42,170),('time',45,130,170),
                                  ('period',134,156,155),('location',192,214,160)]:
  xs=[x for y in range(top,bottom) for x in range(24,296)
      if min(frame.getpixel((x,y)))>threshold]
  assert xs,(name,'missing')
  center=(min(xs)+max(xs))/2
  assert abs(center-160)<=2,(name,'not optically centered',center)

 snapshot().save(a.output/'ambient-clock-native.png')
 snapshot().resize((960,720)).save(a.output/'ambient-clock.png')
 print('PASS: real simulator preview rendered with temperature and conditions',flush=True)
 if a.art:
  # Capture a complete real-time hold/dissolve into a second distinct cover.
  time.sleep(25)
  snapshot().resize((960,720)).save(a.output/'album-transition.png')
  time.sleep(6)
  after=snapshot()
  after.resize((960,720)).save(a.output/'album-next.png')
  # Away from clock/weather, the artwork must have actually changed.
  from PIL import ImageChops, ImageStat
  delta=ImageStat.Stat(ImageChops.difference(frame.crop((0,40,24,220)),after.crop((0,40,24,220)))).mean
  assert max(delta)>5,('album did not change',delta)
  print('PASS: real album artwork changed after hold and crossfade',flush=True)
 counts=[]
 for _ in range(3):
  tap('select');time.sleep(.4)
  deadline=time.monotonic()+4
  while color_count()>=100 and time.monotonic()<deadline:time.sleep(.1)
  assert color_count()<100,'wake did not restore settings'
  counts.append(len(list(Path(f'/proc/{proc.pid}/fd').iterdir())))
  tap('select');time.sleep(1)
  assert color_count()>100,'preview failed on reentry'
 assert max(counts)-min(counts)<=1,('file descriptor growth',counts)
 print('PASS: three wake/reentry cycles; file descriptor counts',counts,flush=True)
finally:
 proc.terminate()
 try:proc.wait(timeout=5)
 except subprocess.TimeoutExpired:proc.kill();proc.wait()
 log.close()
print(a.output)

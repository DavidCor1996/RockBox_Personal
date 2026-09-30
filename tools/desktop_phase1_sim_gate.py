#!/usr/bin/env python3
"""Exercise the real Desktop Mode plugin with a generated music library."""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import time
from PIL import Image
from desktop_mode_sim_gate import ROOT, stage, PLUGIN_PATH, SESSION_TOKEN


def run(build, output, width, extra_assets=None, long_library=False):
    output.mkdir(parents=True, exist_ok=True)
    root = output / 'simdisk'
    if root.exists():
        raise ValueError(f'Use a fresh output directory: {root}')
    stage(build, root)
    if extra_assets:
        shutil.copytree(extra_assets / '640x480', root / '.rockbox/rocks/apps/desktop_mode_snow_leopard/640x480')
    rb = root / '.rockbox'
    (rb / 'config.cfg').write_text('tagcache_autoupdate: on\ntagcache_ram: off\nresume: off\n')
    (rb / 'rocks/apps/desktop_mode.cfg').write_text('snow last app: itunes\nsnow restore session: on\n')
    music = [('Artist A','Album A1','Song 1'), ('Artist A','Album A1','Song 2'),
             ('Artist A','Album A2','Song 3'), ('Artist B','Album B1','Song 4')]
    if long_library:
        music = [('Artist A', 'Long Album', f'Song {i:02}') for i in range(40)]
    for index, (artist, album, title) in enumerate(music):
        folder = root / 'Music' / artist / album
        folder.mkdir(parents=True, exist_ok=True)
        subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','sine=frequency=440:duration=120',
            '-metadata',f'artist={artist}','-metadata',f'album={album}',
            '-metadata',f'title={title}','-metadata',f'track={index+1}',
            '-c:a','libmp3lame','-b:a','64k',str(folder / (title+'.mp3'))],check=True)
        Image.new('RGB',(51,51),[(240,40,40),(240,40,40),(30,170,80),(40,70,240)][index % 4]).save(folder / 'cover.51x51.bmp')
    (root / 'Playlists').mkdir()
    (root / 'Playlists/Test.m3u8').write_text('#EXTM3U\n../Music/Artist A/Album A1/Song 1.mp3\n../Music/Artist B/Album B1/Song 4.mp3\n')
    (root / 'Photos/Locked').mkdir(parents=True)
    Image.new('RGB',(800,400),(20,130,210)).save(root / 'Photos/Blue.jpg')
    Image.new('RGB',(800,400),(220,30,80)).save(root / 'Photos/Locked/Secret.jpg')
    (root / 'Photos/.photo_previews/Locked').mkdir(parents=True)
    Image.new('RGB',(320,160),(220,30,80)).save(root / 'Photos/.photo_previews/Locked/Secret.jpg.bmp')
    (rb / 'rocks/apps/photos.locks').write_text('Locked|1234\nLocked/Secret.jpg|5678\n')
    (rb / 'codecs').mkdir(exist_ok=True)
    for codec in ('mpa.codec',):
        source = build / 'lib/rbcodec/codecs' / codec
        if source.exists(): shutil.copy2(source, rb / 'codecs' / codec)
    live = output / 'live.bmp'
    log = output / 'simulator.log'
    env = {**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy',
        'ROCKPOD_SIM_HIDDEN':'1','ROCKPOD_SIM_IPODJS_TRACE':'1',
        'ROCKPOD_SIM_SURFACE_RESOURCES':'1','ROCKPOD_SIM_PREVIEW_BMP':str(live),
        'ROCKPOD_SIM_PREVIEW_INTERVAL_MS':'40','ROCKBOX_SIM_PLUGIN':PLUGIN_PATH,
        'ROCKBOX_SIM_PLUGIN_PARAM':SESSION_TOKEN}
    # A fresh Rockbox database commits on the next boot; prepare it before
    # occupying plugin memory, following the normal database lifecycle.
    bootstrap_env = {k:v for k,v in env.items() if not k.startswith('ROCKBOX_SIM_PLUGIN')}
    with (output / 'bootstrap.log').open('wb') as bootstrap_log:
        bootstrap = subprocess.Popen([str(build / 'rockboxui'),'--nobackground',
            '--root',str(root)],cwd=build,env=bootstrap_env,
            stdout=bootstrap_log,stderr=subprocess.STDOUT)
        deadline=time.monotonic()+20
        while time.monotonic()<deadline and not (rb/'database_tmp.tcd').exists():
            time.sleep(.2)
        time.sleep(1)
        bootstrap.terminate()
        bootstrap.wait(timeout=5)
    handle = log.open('wb')
    process = subprocess.Popen([str(build / 'rockboxui'),'--nobackground','--root',str(root)],
        cwd=build, env=env,stdout=handle,stderr=subprocess.STDOUT)
    sequence = 0
    def logs(): return log.read_text(errors='replace')
    def wait(predicate, label, timeout=30):
        until = time.monotonic()+timeout
        while time.monotonic()<until:
            if process.poll() is not None: raise AssertionError(f'Exited: {label}, see {log}')
            if predicate(): return
            time.sleep(.1)
        raise AssertionError(f'Timed out: {label}, see {log}')
    def event(kind, value=0, x=0,y=0,down=1,mods=0):
        nonlocal sequence
        sequence += 1
        temporary=rb / 'desktop-event.new'
        temporary.write_text(f'{sequence} {kind} {x} {y} {value} {mods} {down}\n')
        temporary.replace(rb / 'desktop-event')
        time.sleep(.12)
    def activate(x,y):
        event(0,x=x,y=y)
        event(3,257) # Enter uses focused target, same activation as double click
        time.sleep(.2)
    def snapshot(name):
        time.sleep(.3)
        with Image.open(live) as im: im.save(output / f'{name}.png')
    def current_rows():
        text=logs().rsplit('desktop library:',1)[-1]
        return [line.split(' | ')[0].split(' ',3)[-1] for line in text.splitlines() if line.startswith('desktop row:')]
    try:
        if long_library:
            import csv
            wait(lambda: 'desktop row: 0 Song 00' in logs(), 'Long database')
            listx=119 if width==640 else 95
            def playback():
                with (rb/'ipodjs-trace.tsv').open() as trace:
                    frames=list(csv.DictReader(trace,delimiter='\t'))
                return frames[-1]
            def play_and_check(expected, amount):
                activate(listx+45,102)
                wait(lambda: int(playback()['playlist_count']) == amount and
                     playback()['path'].endswith(expected+'.mp3'), 'Complete queue')
                assert not (rb/'rocks/apps/desktop_queue.m3u8').exists()
            event(2,-1)
            expected=current_rows()[0]
            assert expected != 'Song 00'
            play_and_check(expected,40)
            activate(35,101)
            for ch in 'Song 3': event(4,ord(ch))
            wait(lambda:current_rows()[0]=='Song 30','Filtered later results')
            play_and_check('Song 30',10)
            playlist=['../Music/Artist A/Long Album/Song 00.mp3']*80
            playlist[35]='../Music/Artist A/Long Album/Song 39.mp3'
            (root/'Playlists/Test.m3u8').write_text('\n'.join(playlist)+'\n')
            activate(35,96+5*13+5)
            wait(lambda:current_rows()==['Test.m3u8'],'Long playlist catalog')
            activate(listx+45,102)
            wait(lambda:current_rows()[0]=='Song 00.mp3','Long playlist')
            event(2,-1)
            play_and_check('Song 00',80)
            assert int(playback()['playlist_index']) == 2, playback()
            (output/'report.json').write_text(json.dumps({
                'width':width,'full_song_queue':40,'search_queue':10,
                'playlist_queue':80,'later_page_start':expected},indent=2)+'\n')
            print((output/'report.json').read_text())
            return
        wait(lambda:'Song 4 | Artist B' in logs(), 'database initialized')
        snapshot('itunes-songs')
        body=96
        listx=119 if width==640 else 95
        activate(35,body+2*13+5)
        wait(lambda: current_rows()==['Artist A','Artist B'], 'Artists')
        snapshot('itunes-artists')
        activate(listx+45,body+6)
        wait(lambda:sorted(current_rows())==['Album A1','Album A2'], 'Artist A albums')
        snapshot('itunes-artist-albums')
        album_x=listx+45+(0 if current_rows()[0]=='Album A1' else (width-8-1-listx-15)//2)
        activate(album_x,body+6)
        wait(lambda:sorted(current_rows())==['Song 1','Song 2'], 'Album A1 songs')
        snapshot('itunes-album-songs')
        activate(listx+45,body+6)
        event(3,258) # Escape returns to artist albums
        wait(lambda:sorted(current_rows())==['Album A1','Album A2'], 'Back to artist albums')
        time.sleep(1)
        baseline_fds=len(list(Path(f'/proc/{process.pid}/fd').iterdir()))
        fd_counts=[]
        core_counts=[]
        def core_state():
            import csv
            with (rb/'ipodjs-trace.tsv').open() as trace:
                frames=list(csv.DictReader(trace,delimiter='\t'))
            return tuple(int(frames[-1][key]) for key in
                         ('core_available','core_allocatable'))
        core_baseline=core_state()
        for _ in range(10):
            activate(album_x,body+6)
            wait(lambda:sorted(current_rows())==['Song 1','Song 2'], 'Repeated album')
            event(3,258)
            wait(lambda:sorted(current_rows())==['Album A1','Album A2'], 'Repeated back')
            fd_counts.append(len(list(Path(f'/proc/{process.pid}/fd').iterdir())))
            core_counts.append(core_state())
        assert all(state == core_baseline for state in core_counts), core_counts
        for _ in range(20):
            activate(35,body+13+5)
            wait(lambda:len(current_rows())==3, 'Rapid Albums')
            activate(35,body+2*13+5)
            wait(lambda:current_rows()==['Artist A','Artist B'], 'Rapid Artists')
        assert 'Database is not ready' not in logs()
        activate(35,body+5)
        wait(lambda:len(current_rows())==4, 'All songs')
        for ch in 'Song 4': event(4,ord(ch))
        wait(lambda:current_rows()==['Song 4'], 'Library-wide search')
        snapshot('itunes-search')
        activate(35,body+5*13+5)
        wait(lambda:current_rows()==['Test.m3u8'], 'Playlist catalog')
        activate(listx+45,body+6)
        wait(lambda:current_rows()==['Song 1.mp3','Song 4.mp3'], 'Playlist songs')
        snapshot('itunes-playlist')
        event(3,258); event(3,258) # back then desktop
        time.sleep(.8)
        snapshot('desktop')
        # System Preferences is the last regular dock slot; select using
        # Tab cycle later if the dock arrangement changes.
        dock_left=(width-8*34)//2
        activate(dock_left+6*34+16, 208 if width==320 else 448)
        snapshot('preferences')
        assert Image.open(live).getpixel((100,80)) != Image.open(output/'desktop.png').getpixel((100,80))
        activate(200 if width==320 else 400,60)
        snapshot('desktop-settings')
        activate(100,101) # Choose Image row: body 77 + 16
        snapshot('image-picker')
        # Photos root: Locked folder and Blue.jpg, with hidden sidecars excluded.
        activate(listx+45,77+19+8)
        wait(lambda:'wallpaper path: /Photos/Blue.jpg' in
             (rb/'rocks/apps/desktop_mode.cfg').read_text(), 'Wallpaper persisted')
        snapshot('wallpaper-settings')
        event(3,258)
        time.sleep(.8)
        snapshot('wallpaper-blue')
        with Image.open(live) as im:
            r,g,b=im.convert('RGB').getpixel((80,150))
            assert abs(r-20)<15 and abs(g-130)<15 and abs(b-210)<15
            # Both outside corners of the Dock must show today's wallpaper.
            shelf_left=(width-288)//2
            for x in (shelf_left+1,shelf_left+286):
                r,g,b=im.convert('RGB').getpixel((x, (240 if width==320 else 480)-24))
                assert abs(r-20)<15 and abs(g-130)<15 and abs(b-210)<15

        # Locked folder and separately locked original both require a code.
        activate(dock_left+6*34+16,208 if width==320 else 448)
        activate(100,101)
        activate(listx+45,77+8)
        snapshot('locked-folder-pin')
        for ch in '0000': event(4,ord(ch))
        event(3,257)
        snapshot('wrong-photo-pin')
        assert 'wallpaper path: /Photos/Blue.jpg' in (rb/'rocks/apps/desktop_mode.cfg').read_text()
        event(3,258) # Cancel cannot enter the folder.
        snapshot('locked-folder-cancelled')
        activate(listx+45,77+8)
        for ch in '1234': event(4,ord(ch))
        event(3,257)
        snapshot('unlocked-folder')
        activate(listx+45,77+8)
        snapshot('locked-photo-pin')
        for ch in '5678': event(4,ord(ch))
        event(3,257)
        wait(lambda:'wallpaper path: /Photos/Locked/Secret.jpg' in
             (rb/'rocks/apps/desktop_mode.cfg').read_text(), 'Locked photo authorized')
        event(3,258)
        snapshot('wallpaper-authorized-secret')
        with Image.open(live) as im:
            r,g,b=im.convert('RGB').getpixel((80,150))
            assert abs(r-220)<15 and abs(g-30)<15 and abs(b-80)<15
        # Restart must not silently restore the protected wallpaper.
        process.terminate(); process.wait(timeout=5)
        before_restart=len(logs())
        handle.close(); handle=log.open('ab')
        process = subprocess.Popen([str(build/'rockboxui'),'--nobackground','--root',str(root)],
            cwd=build,env=env,stdout=handle,stderr=subprocess.STDOUT)
        wait(lambda:'desktop resources:' in logs()[before_restart:],
             'Plugin initialized after restart')
        time.sleep(.5)
        snapshot('locked-wallpaper-restart')
        with Image.open(live) as im:
            r,g,b=im.convert('RGB').getpixel((80,150))
            assert not (abs(r-220)<15 and abs(g-30)<15 and abs(b-80)<15)
        # Recent selection must recheck ancestor and original locks.
        activate(dock_left+6*34+16,208 if width==320 else 448)
        activate(200 if width==320 else 400,60)
        activate(100,133)
        activate((width-240)//2+40,56+26+8)
        snapshot('locked-recent-pin')
        for ch in '0000': event(4,ord(ch))
        event(3,257)
        snapshot('locked-recent-wrong-pin')
        event(3,258)
        # Restore the cached default.
        activate(100,83)
        wait(lambda:'wallpaper path: \n' in
             (rb/'rocks/apps/desktop_mode.cfg').read_text(), 'Restore default')
        # Ctrl+W closes Preferences; the existing Trash identity opens a
        # desktop sheet, dismissed by Enter through the common control path.
        event(3,ord('w'),mods=2)
        time.sleep(.4)
        snapshot('keyboard-close')
        before_notice=len(logs())
        activate(dock_left+7*34+16,208 if width==320 else 448)
        wait(lambda:'desktop notice: Trash is empty' in logs()[before_notice:],
             'Desktop notice')
        center=(width//2,80)
        wait(lambda:sum(Image.open(live).convert('RGB').getpixel(center))>650,
             'Notice sheet presented')
        snapshot('notice-sheet')
        assert sum(Image.open(output/'notice-sheet.png').convert('RGB').getpixel(center))>650
        event(3,257)
        snapshot('notice-dismissed')
        assert Image.open(output/'notice-sheet.png').getpixel(center) != Image.open(output/'notice-dismissed.png').getpixel(center)
        resources=[tuple(map(int,m)) for m in re.findall(
            r'desktop resources: audio=(\d+) elapsed=(\d+) arena=(\d+) playlist=(\d+)',logs())]
        active=[r for r in resources if r[0]&1]
        assert len(active)>=20 and all(r[3]==2 for r in active)
        assert all(b[1]>=a[1] for a,b in zip(active,active[1:]))
        assert len({r[2] for r in resources})==1
        assert max(fd_counts)<=baseline_fds+1 and fd_counts[-1]<=baseline_fds+1

        report={'width':width,'hierarchy_cycles':10,'rapid_source_cycles':20,'library_search':True,
                'playlist_navigation':True,'wallpaper':True,'photo_locks':True,'dock_wallpaper_edges':True,'playback_continuity':True,
                'core_baseline':core_baseline,'core_cycles':core_counts,
                'fd_baseline':baseline_fds,'fd_cycles':fd_counts,
                'arena_bytes':resources[0][2],'screenshots':str(output)}
        (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report))
    finally:
        process.terminate()
        try: process.wait(timeout=5)
        except subprocess.TimeoutExpired: process.kill(); process.wait()
        handle.close()


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--build',type=Path,default=ROOT/'build-sim-ipod6g')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--width',type=int,default=320)
    parser.add_argument('--extra-assets',type=Path)
    parser.add_argument('--long-library',action='store_true')
    a=parser.parse_args()
    run(a.build.resolve(),a.output.resolve(),a.width,a.extra_assets,a.long_library)

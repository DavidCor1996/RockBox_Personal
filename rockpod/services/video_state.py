"""Import device-owned playback state; never push a stale desktop snapshot.

The device is the only playback-state writer. Host import replaces its last
snapshot even when the position decreased (Start Over is intentional). This
avoids timestamps from unsynchronized iPod/host clocks resolving conflicts.
"""
from pathlib import Path
import struct
import itertools
import json
import os
import subprocess
from fractions import Fraction


def import_video_state(db, mount, device_key):
    directories = (Path(mount)/'.rockbox/rocks/apps', Path(mount)/'.rockbox/rocks.data')
    db.execute('''CREATE TABLE IF NOT EXISTS video_playback_state (
        device_id TEXT NOT NULL, identity INTEGER NOT NULL,
        position_ms INTEGER NOT NULL, duration_ms INTEGER NOT NULL,
        watched INTEGER NOT NULL, PRIMARY KEY(device_id, identity))''')
    count = 0
    for path in itertools.chain.from_iterable(d.glob('video-state-????????.dat') for d in directories):
        if count >= 4096:
            break
        try:
            data = path.read_bytes() if path.stat().st_size == 20 else b''
            magic, identity, position, duration, watched = struct.unpack('<5I', data)
            if (magic != 0x564c5332 or not 0 < duration < 0xffffffff or
                    position > duration or watched > 1 or
                    path.stem != f'video-state-{identity:08x}'):
                continue
            db.execute('''INSERT INTO video_playback_state VALUES(?,?,?,?,?)
                ON CONFLICT(device_id,identity) DO UPDATE SET
                position_ms=excluded.position_ms, duration_ms=excluded.duration_ms,
                watched=excluded.watched''',
                (device_key, identity, position, duration, watched))
            count += 1
        except (OSError, ValueError, struct.error):
            continue
    return count


def rockbox_crc(data):
    crc = 0xffffffff
    for value in data:
        crc ^= value << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04c11db7 if crc & 0x80000000 else 0)) & 0xffffffff
    return crc


def migrate_legacy_state(mount, row, ffprobe='ffprobe'):
    """One-time migration only when the old and replacement durations agree.

    A previous .media identity deliberately blocks migration: a changed source
    fingerprint means the timeline may have been edited. Existing logical state
    is never overwritten, including an explicit Start Over.
    """
    previous = row.get('sync_previous_device_path')
    sidecar = row.get('sync_video_sidecars', {}).get('.media')
    if not previous or not sidecar:
        return False
    mount = Path(mount).resolve()
    old = (mount / previous.lstrip('/')).resolve()
    old.relative_to(mount)
    if old.with_suffix('.media').exists() or not old.is_file():
        return False
    line = Path(sidecar).read_text(encoding='ascii').strip()
    if not line.startswith('id=') or len(line) != 35:
        return False
    identity = rockbox_crc(line[3:].encode())
    directory = mount / '.rockbox/rocks/apps'
    destination = directory / f'video-state-{identity:08x}.dat'
    if destination.exists():
        return False
    try:
        info = json.loads(subprocess.check_output([ffprobe,'-v','error','-show_streams',
                                                  '-show_format','-of','json',str(old)]))
        v = next(s for s in info['streams'] if s['codec_type']=='video')
        duration = round(float(v.get('duration') or info['format']['duration'])*1000)
        if abs(duration-round(float(row['duration'])*1000)) > 1000:
            return False
        name = '/' + previous.lstrip('/')
        crc = rockbox_crc(name.encode())
        position = None; watched = False
        old_shared = directory/f'video-state-{crc:08x}.dat'
        if old_shared.is_file():
            magic, stored, position, total, watched = struct.unpack('<5I',old_shared.read_bytes())
            if magic!=0x564c5332 or stored!=crc or abs(total-duration)>1000:
                return False
        else:
            resume = directory/f'openh264-resume-{crc:08x}.dat'
            if resume.is_file():
                magic, stored, frame, total = struct.unpack('<IIii',resume.read_bytes())
                if (magic!=0x52565031 or stored!=crc or total!=int(v.get('nb_frames') or 0)
                        or v.get('avg_frame_rate')!=v.get('r_frame_rate')):
                    return False
                position = round(Fraction(frame*1000)/Fraction(v['avg_frame_rate']))
            else:
                config = mount/'.rockbox/rocks/viewers/mpegplayer.cfg'
                if config.is_file() and config.stat().st_size < 8*1024*1024:
                    for setting in config.read_text(errors='replace').splitlines():
                        key, sep, value = setting.rpartition(':')
                        if sep and key==name:
                            position = int(value.strip())*1000//45000
            watched_file=mount/'.rockbox/videolist/netflix-watched.tsv'
            if watched_file.is_file() and watched_file.stat().st_size < 1024*1024:
                watched = name in watched_file.read_text(errors='replace').splitlines()
        if position is None and not watched:
            return False
        position = position or 0
        if not 0 <= position <= duration or watched not in (0,1,False,True):
            return False
        directory.mkdir(parents=True,exist_ok=True)
        temporary = destination.with_suffix('.tmp')
        with temporary.open('wb') as f:
            f.write(struct.pack('<5I',0x564c5332,identity,position,duration,int(watched)))
            f.flush();os.fsync(f.fileno())
        os.replace(temporary,destination)
        return True
    except (OSError,ValueError,KeyError,StopIteration,struct.error,subprocess.SubprocessError):
        return False

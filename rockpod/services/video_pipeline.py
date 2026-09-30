"""Probe, review and prepare one validated rendition for the installed player.

The UI and app sync share this backend through VideoRvpTranscoder. No source
file is modified. Encoder processes use pipes and bounded logs, not raw movies
in RAM or a temporary uncompressed movie on disk.
"""
from __future__ import annotations

import hashlib
import json
import math
import os
from pathlib import Path
from fractions import Fraction
import shutil
import subprocess
import tempfile

from services.video_capabilities import read_device_capabilities, contract_digest
from services.video_validation import validate_full_h264

VERSION = 'video-pipeline-2'
PREFIX = 'video2:'
RATES = tuple(Fraction(x) for x in ('24000/1001', '24', '25', '30000/1001', '30'))
QUALITIES = {'space': (320, 240, 550), 'balanced': (480, 360, 1000),
             'tv': (640, 480, 1500), 'custom': (640, 480, 1500)}


def options(profile=None):
    if isinstance(profile, dict):
        value = dict(profile)
    elif str(profile or '').startswith(PREFIX):
        value = json.loads(str(profile)[len(PREFIX):])
    else:
        value = {'format': 'h264' if 'h264' in str(profile) else
                 'mpeg' if profile == 'quality' else 'auto', 'quality': 'tv'}
    value = {'format': 'auto', 'quality': 'balanced', 'audio': 'default',
             'subtitle': None, 'subtitle_mode': 'soft', 'target_mib': 0,
             'bitrate': 0, 'deinterlace': 'auto', 'color': 'auto',
             'subtitle_file': '', **value}
    if value['format'] not in ('auto', 'h264', 'mpeg'):
        raise ValueError('Choose Automatic, H.264 or MPEG.')
    if value['quality'] not in QUALITIES:
        raise ValueError('Unknown video quality.')
    if value['subtitle_mode'] not in ('soft', 'burn'):
        raise ValueError('Unknown subtitle mode.')
    if value['deinterlace'] not in ('auto', 'on', 'off'):
        raise ValueError('Unknown deinterlace setting.')
    if value['color'] not in ('auto', 'bt709', 'smpte170m', 'bt470bg'):
        raise ValueError('Unknown source color override.')
    for name in ('target_mib', 'bitrate'):
        value[name] = max(0, int(value[name] or 0))
    return value


def profile_string(value):
    return PREFIX + json.dumps(options(value), sort_keys=True, separators=(',', ':'))


def digest_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def atomic_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(str(path) + '.part', 'w', encoding='utf-8') as output:
        json.dump(value, output, sort_keys=True, indent=2)
        output.flush()
        os.fsync(output.fileno())
    os.replace(str(path) + '.part', path)


def rate(value, default=Fraction(25)):
    try:
        result = Fraction(str(value).replace(':', '/'))
        return result if result > 0 else default
    except (ValueError, ZeroDivisionError):
        return default


def probe(path, ffprobe='ffprobe'):
    command = [ffprobe, '-v', 'error', '-show_streams', '-show_format',
               '-show_chapters', '-show_data', '-of', 'json', str(path)]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode:
        raise RuntimeError('Cannot inspect video: ' + result.stderr[-1500:])
    info = json.loads(result.stdout)
    videos = [s for s in info['streams'] if s['codec_type'] == 'video'
              and not s.get('disposition', {}).get('attached_pic')]
    if not videos:
        raise RuntimeError('No video stream was found.')
    info['video'] = videos[0]
    info['audio_tracks'] = [s for s in info['streams'] if s['codec_type'] == 'audio']
    info['subtitle_tracks'] = [s for s in info['streams'] if s['codec_type'] == 'subtitle']
    return info


def selected_track(tracks, selection):
    if selection is None or selection == 'none':
        return None
    if selection == 'default':
        return next((s for s in tracks if s.get('disposition', {}).get('default')),
                    tracks[0] if tracks else None)
    found = next((s for s in tracks if s['index'] == int(selection)), None)
    if found is None:
        raise ValueError('The selected media track no longer exists.')
    return found


def rotation(video):
    return int(round(float(next((s['rotation'] for s in video.get('side_data_list', [])
                                if 'rotation' in s), video.get('tags', {}).get('rotate', 0))))) % 360


def dimensions(video, maximum):
    w, h = int(video['width']), int(video['height'])
    sar = rate(video.get('sample_aspect_ratio'), Fraction(1))
    # Normalize anamorphic display geometry without increasing the number of
    # samples: shrink the other axis instead of upscaling the stretched axis.
    dw, dh = (w, h / float(sar)) if sar >= 1 else (w * float(sar), h)
    if rotation(video) in (90, 270):
        dw, dh = dh, dw
    scale = min(1, maximum[0] / dw, maximum[1] / dh)
    return max(2, int(dw * scale) & ~1), max(2, int(dh * scale) & ~1)


def run(command, cwd=None):
    with tempfile.TemporaryFile() as log:
        result = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=log, cwd=cwd)
        if result.returncode:
            log.seek(max(0, log.tell() - 4000))
            raise RuntimeError(log.read().decode('utf-8', 'replace'))


def validate(path, fmt, ffmpeg, ffprobe, *, decode=True):
    info = probe(path, ffprobe)
    v = info['video']; audio = info['audio_tracks']
    if len(audio) > 1 or info['subtitle_tracks']:
        raise RuntimeError('Only one audio stream and external selected captions are supported.')
    if rotation(v) or rate(v.get('sample_aspect_ratio'), Fraction(1)) != 1:
        raise RuntimeError('Rotation or anamorphic metadata needs normalization.')
    if v.get('pix_fmt') != 'yuv420p' or v.get('field_order', 'progressive') not in ('progressive', 'unknown'):
        raise RuntimeError('Video must be progressive 8-bit YUV420.')
    if v.get('color_transfer') in ('smpte2084', 'arib-std-b67') or v.get('color_range') == 'pc':
        raise RuntimeError('HDR/full-range video needs color conversion.')
    if v.get('color_space') not in (None, 'unknown', 'smpte170m', 'bt470bg'):
        raise RuntimeError('Source color matrix needs conversion to SD television color.')
    if audio:
        a = audio[0]
        if (a['codec_name'] not in (('aac',) if fmt == 'h264' else ('mp2', 'mp3'))
                or (fmt == 'h264' and a.get('profile') != 'LC')
                or int(a['channels']) not in (1, 2) or int(a['sample_rate']) != 44100):
            raise RuntimeError('Selected audio needs conversion.')
    if fmt == 'h264':
        if v['codec_name'] != 'h264':
            raise RuntimeError('Video is not H.264.')
        report = validate_full_h264(path, v, ffprobe, ffmpeg, decode=decode)
    else:
        if (info['format']['format_name'] != 'mpeg' or v['codec_name'] not in ('mpeg1video', 'mpeg2video')
                or int(v['width']) > 320 or int(v['height']) > 240
                or rate(v.get('r_frame_rate')) not in RATES):
            raise RuntimeError('MPEG exceeds the conservative software decoder profile.')
        # Stream all packet timing, without retaining a full-movie JSON tree.
        last = {}; count = 0; key_time = None
        with tempfile.TemporaryFile() as errors:
            p = subprocess.Popen([ffprobe, '-v', 'error', '-show_packets', '-show_entries',
                'packet=stream_index,dts_time,pts_time,flags', '-of', 'compact=p=0', str(path)],
                stdout=subprocess.PIPE, stderr=errors, text=True)
            try:
                for line in p.stdout:
                    pkt = dict(part.split('=', 1) for part in line.strip().split('|') if '=' in part)
                    if 'stream_index' not in pkt: continue
                    idx = int(pkt['stream_index']); dts = float(pkt['dts_time'])
                    if idx in last and dts <= last[idx]:
                        raise RuntimeError('MPEG decode timestamps are not increasing.')
                    last[idx] = dts
                    if idx == v['index']:
                        count += 1
                        if 'K' in pkt.get('flags', ''): key_time = dts
                        if key_time is None or dts - key_time > 4.1:
                            raise RuntimeError('MPEG needs a bounded random-access interval.')
                if p.wait(): raise RuntimeError('MPEG packet scan failed.')
                errors.seek(0)
                if errors.read(1): raise RuntimeError('MPEG contains container errors.')
            finally:
                if p.poll() is None: p.kill(); p.wait()
                p.stdout.close()
        if not count: raise RuntimeError('No MPEG pictures.')
        if os.path.getsize(path) > 2147483647: raise RuntimeError('Movie exceeds the conservative file-size limit.')
        if decode:
            run([ffmpeg, '-v', 'error', '-xerror', '-threads', '2', '-i', str(path),
                 '-map', '0:v:0', '-map', '0:a:0?', '-f', 'null', '-'])
        report = {'scope': 'full_stream', 'pictures': count, 'host_decode_checked': decode,
                  'hardware_qualified': False}
    return {'video': v, 'audio': audio[0] if audio else {}, 'full_stream': report,
            'sha256': digest_file(path), 'bytes': os.path.getsize(path)}


class VideoPipeline:
    def __init__(self, transcoder):
        self.backend = transcoder
        self.ffmpeg = transcoder.ffmpeg_bin()
        self.ffprobe = transcoder._ffprobe_bin(self.ffmpeg)
        self.cache = Path(transcoder._cache_root) / 'prepared-v2'
        self.cache.mkdir(parents=True, exist_ok=True)

    def status(self, phase, source):
        callback = getattr(self.backend, '_progress', None)
        if callback:
            callback(f'{phase}: {Path(source).name}')

    def plan(self, source, settings, mount=None, target=''):
        self.status('Probe', source)
        source = str(Path(source).resolve()); opt = options(settings)
        info = probe(source, self.ffprobe); v = info['video']
        installed = read_device_capabilities(mount)
        fmt = opt['format']
        if fmt == 'auto':
            h264 = installed['capabilities']['h264']
            fmt = 'h264' if installed['verified'] and h264.get('hardware_qualified') else 'mpeg'
        if fmt == 'h264' and mount is not None and not (
                installed['verified'] and installed['capabilities']['h264'].get('implemented')):
            raise RuntimeError('H.264 is unavailable for this installed firmware. Choose MPEG.')
        if fmt == 'h264' and target in ('ipodvideo', 'ipodvideo64mb'):
            raise RuntimeError('This 5G player has no implemented H.264 decoder. Choose MPEG.')
        width, height, kbps = QUALITIES[opt['quality']]
        if fmt == 'mpeg':
            width, height = 320, 240
            kbps = {'space': 500, 'balanced': 1000, 'tv': 1500, 'custom': 1500}[opt['quality']]
        width, height = dimensions(v, (width, height))
        source_rate = rate(v.get('avg_frame_rate') or v.get('r_frame_rate'))
        cadence = min(RATES, key=lambda x: abs(x - (source_rate / 2 if source_rate > 30 else source_rate)))
        audio = selected_track(info['audio_tracks'], opt['audio'])
        subtitle = selected_track(info['subtitle_tracks'], opt['subtitle'])
        if opt['subtitle_file']:
            external = Path(opt['subtitle_file']).resolve()
            if external.suffix.lower() != '.srt' or external.stat().st_size > 8*1024*1024:
                raise ValueError('External subtitles must be a UTF-8 SRT file under 8 MiB.')
            external.read_text(encoding='utf-8-sig')
            subtitle = {'index': 0, 'codec_name': 'subrip', 'file': str(external),
                        'tags': {'title': external.name, 'language': 'external'}}
        if subtitle and opt['subtitle_mode'] == 'soft' and subtitle['codec_name'] not in ('subrip', 'mov_text', 'text', 'webvtt'):
            raise ValueError('Selected complex/image subtitles require explicit burn-in.')
        if subtitle and subtitle['codec_name'] not in ('subrip', 'mov_text', 'text', 'webvtt', 'ass', 'ssa', 'dvd_subtitle', 'hdmv_pgs_subtitle', 'dvb_subtitle'):
            raise ValueError('The selected subtitle codec is unsupported; selection was not dropped.')
        duration = float(v.get('duration') or info['format'].get('duration') or 0)
        if duration <= 0: raise ValueError('A finite video duration is required.')
        abps = (128 if fmt == 'h264' else 112) if audio else 0
        if opt['bitrate']: kbps = opt['bitrate']
        if opt['target_mib']:
            kbps = math.floor(opt['target_mib'] * 1048576 * 8 / (duration * 1.02) / 1000 - abps)
        if not 150 <= kbps <= (4000 if fmt == 'h264' else 1600):
            raise ValueError('Size/bitrate is outside this profile. Choose a smaller resolution or a different budget.')
        if fmt == 'h264' and width * height <= 320 * 240:
            kbps = min(kbps, 750)
        estimate = math.ceil(duration * (kbps + abps) * 1000 / 8 * 1.02)
        if estimate > 2147483647:
            raise ValueError('Estimated movie exceeds 2 GiB. Choose a smaller bitrate or size budget.')
        warnings = []
        if not installed.get('capabilities', {}).get(fmt, {}).get('hardware_qualified'):
            warnings.append('Hardware qualification is pending for this build/profile.')
        if fmt == 'mpeg': warnings.append('Software decoding: limited to 320×240; TV quality does not add source detail.')
        if source_rate != cadence: warnings.append(f'Frame selection: {source_rate} → {cadence} fps; duration is preserved.')
        if v.get('color_space') in (None, 'unknown') and opt['color'] == 'auto':
            warnings.append('Unknown source color: assume BT.709 for HD, SD television color otherwise; override available.')
        if subtitle and opt['subtitle_mode'] == 'burn': warnings.append('Burned subtitles cannot be switched off.')
        plan = {'source': source, 'source_sha256': digest_file(source), 'options': opt,
                'info': info, 'format': fmt, 'width': width, 'height': height,
                'fps': str(cadence), 'video_kbps': kbps, 'audio_kbps': abps,
                'duration': duration, 'audio': audio, 'subtitle': subtitle,
                'estimated_bytes': estimate, 'warnings': warnings,
                'contract': contract_digest(), 'target': installed.get('target') or target,
                'operation': 'convert', 'reason': 'Video needs normalization or a different rendition.'}
        material = {k: plan[k] for k in ('source_sha256', 'options', 'format', 'width', 'height', 'fps', 'video_kbps', 'contract', 'target')}
        material['options'] = dict(opt)
        if not material['options'].get('subtitle_file'):
            material['options'].pop('subtitle_file', None)
        material['version'] = VERSION
        material['encoder_policy'] = 'bounded-gop-no-mbtree-mpeg-mux-timing-v3'
        if subtitle and subtitle.get('file'):
            material['subtitle_sha256'] = digest_file(subtitle['file'])
        material['ffmpeg'] = subprocess.check_output([self.ffmpeg, '-version'], text=True).splitlines()[0]
        if fmt == 'h264':
            encoder = self.backend._verified_apple_x264(str(self.cache))
            material['encoder_sha256'] = digest_file(encoder)
        plan['key'] = hashlib.sha256(json.dumps(material, sort_keys=True).encode()).hexdigest()
        plan_cache = self.cache / ('plan-' + plan['key'] + '.json')
        try:
            cached = json.loads(plan_cache.read_text())
            if cached['key'] == plan['key'] and cached['source'] == source:
                return cached
        except (OSError, ValueError, KeyError):
            pass
        transforms = (rotation(v) or rate(v.get('sample_aspect_ratio'), Fraction(1)) != 1
                      or int(v['width']) > width or int(v['height']) > height
                      or (subtitle and opt['subtitle_mode'] == 'burn')
                      or opt['deinterlace'] == 'on' or opt['color'] != 'auto'
                      or opt['target_mib'] or opt['bitrate'])
        if not transforms:
            self.status('Validate source', source)
            fd, temporary = tempfile.mkstemp(prefix='inspect-', suffix='.m4v' if fmt == 'h264' else '.mpg', dir=self.cache)
            os.close(fd)
            candidate = Path(temporary)
            try:
                direct = validate(source, fmt, self.ffmpeg, self.ffprobe)
                tracks_match = (not info['audio_tracks'] and audio is None) or (
                    len(info['audio_tracks']) == 1 and audio == info['audio_tracks'][0])
                if tracks_match:
                    plan.update(operation='copy', reason='Already compatible; preserve the original video and audio.', validation=direct)
            except (RuntimeError, ValueError, KeyError):
                pass
            if plan['operation'] != 'copy':
                # Only a compressed stream copy, never a video encode during review.
                try:
                    self.remux({**plan, 'audio': None}, candidate)
                    checked = validate(candidate, fmt, self.ffmpeg, self.ffprobe)
                    candidate.unlink(missing_ok=True)
                    plan.update(operation='audio' if audio and (not self.audio_compatible(audio, fmt) or
                                    abs(float(audio.get('start_time') or 0)-float(v.get('start_time') or 0)) > .001) else 'remux',
                                reason='Preserve compatible video; normalize selected audio/container.', validation=checked)
                except (RuntimeError, ValueError, KeyError):
                    candidate.unlink(missing_ok=True)
        if not transforms: candidate.unlink(missing_ok=True)
        if plan['operation'] == 'copy': plan['estimated_bytes'] = os.path.getsize(source)
        atomic_json(plan_cache, plan)
        return plan

    @staticmethod
    def audio_compatible(audio, fmt):
        return (audio['codec_name'] in (('aac',) if fmt == 'h264' else ('mp2', 'mp3'))
                and (fmt != 'h264' or audio.get('profile') == 'LC')
                and int(audio['channels']) <= 2 and int(audio['sample_rate']) == 44100)

    def audio_args(self, plan):
        if not plan['audio']: return ['-an']
        delta = float(plan['audio'].get('start_time') or 0) - float(plan['info']['video'].get('start_time') or 0)
        filters = ['asetpts=PTS-STARTPTS']
        if delta < 0: filters += [f'atrim=start={-delta}', 'asetpts=PTS-STARTPTS']
        if delta > 0: filters += [f'adelay={round(delta * 1000)}:all=1']
        # FFmpeg's normalized rematrix coefficients provide surround headroom;
        # preserve stereo gain and avoid applying loudness normalization.
        filters += ['aresample=44100:rematrix_maxval=1.0', 'apad']
        args = ['-c:a', 'aac' if plan['format'] == 'h264' else 'mp2',
                '-b:a', f"{plan['audio_kbps']}k", '-ar', '44100', '-ac', '2', '-af', ','.join(filters)]
        if plan['format'] == 'h264': args += ['-profile:a', 'aac_low']
        return args

    def mux_args(self, plan):
        if plan['format'] == 'h264':
            return ['-tag:v', 'avc1', '-movflags', '+faststart', '-f', 'mp4']
        # The default preload can omit timestamps across PES boundaries and
        # produce a repeated inferred DTS in low-bitrate portrait clips.
        return ['-muxpreload', '0', '-muxdelay', '0',
                '-packetsize', '2048', '-f', 'mpeg']

    def remux(self, plan, output, audio_convert=False):
        command = [self.ffmpeg, '-y', '-v', 'error', '-threads', '2', '-i', plan['source'],
                   '-map', f"0:{plan['info']['video']['index']}"]
        if plan['audio']: command += ['-map', f"0:{plan['audio']['index']}"]
        command += ['-map_metadata', '-1', '-map_chapters', '-1', '-sn', '-dn', '-c:v', 'copy']
        command += self.audio_args(plan) if audio_convert else (['-c:a', 'copy'] if plan['audio'] else ['-an'])
        command += ['-t', str(plan['duration'])] + self.mux_args(plan) + [str(output)]
        run(command)

    def filters(self, plan, preview=False):
        v = plan['info']['video']; opt = plan['options']; parts = []
        if opt['deinterlace'] == 'on' or (opt['deinterlace'] == 'auto' and v.get('field_order') in ('tt', 'bb', 'tb', 'bt')):
            parts.append('bwdif=mode=send_frame:parity=auto:deint=all')
        # FFmpeg autorotation precedes this filter. HDR is converted through
        # linear light, not retagged as SDR.
        if v.get('color_transfer') in ('smpte2084', 'arib-std-b67'):
            parts += ['zscale=t=linear:npl=100', 'format=gbrpf32le', 'zscale=p=bt709',
                      'tonemap=tonemap=mobius:desat=2', 'zscale=t=bt709:m=bt709:r=limited', 'format=yuv420p']
            incoming = 'bt709'
        else:
            incoming = opt['color'] if opt['color'] != 'auto' else v.get('color_space')
            if incoming in (None, 'unknown'):
                incoming = 'bt709' if int(v['height']) > 576 else 'smpte170m'
        parts += [f"scale={plan['width']}:{plan['height']}:flags=lanczos:in_color_matrix={incoming}:out_color_matrix=bt601:out_range=tv",
                  'setsar=1', 'format=yuv420p',
                  'setparams=range=limited:colorspace=smpte170m:color_trc=bt709:color_primaries=smpte170m',
                  'setpts=PTS-STARTPTS', f"fps={plan['fps']}:start_time=0"]
        if plan['subtitle'] and opt['subtitle_mode'] == 'burn' and plan['subtitle']['codec_name'] not in ('dvd_subtitle','hdmv_pgs_subtitle','dvb_subtitle'):
            # Prepared subtitle file lives at a simple relative path inside the
            # locked cache directory; source filenames never enter filter syntax.
            if preview:
                parts.append(f"setpts=PTS+{max(0, plan['duration']*.33)}/TB")
            parts.append('subtitles=selected.ass')
            if preview:
                parts.append('setpts=PTS-STARTPTS')
        return ','.join(parts)

    def encode(self, plan, output, directory, preview=False):
        duration = min(20, plan['duration']) if preview else plan['duration']
        source_args = ['-i', plan['source']]
        if preview:
            source_args = ['-ss', str(max(0, plan['duration'] * .33))] + source_args
        video_args = [self.ffmpeg, '-v', 'error', '-threads', '2', '-filter_threads', '1'] + source_args
        image_subtitle = plan['subtitle'] and plan['options']['subtitle_mode'] == 'burn' and plan['subtitle']['codec_name'] in ('dvd_subtitle','hdmv_pgs_subtitle','dvb_subtitle')
        if image_subtitle:
            video_args += ['-filter_complex_threads', '1', '-filter_complex',
                f"[0:{plan['info']['video']['index']}][0:{plan['subtitle']['index']}]overlay=eof_action=pass," + self.filters(plan, preview) + '[picture]',
                '-map', '[picture]', '-an', '-sn', '-dn', '-t', str(duration)]
        else:
            video_args += ['-map', f"0:{plan['info']['video']['index']}", '-an', '-sn', '-dn',
                           '-vf', self.filters(plan, preview), '-t', str(duration)]
        passes = (1, 2) if plan['options']['target_mib'] and not preview else (0,)
        if plan['format'] == 'h264':
            raw = directory / 'video.mp4'
            x264 = self.backend._verified_apple_x264(str(directory))
            old_config = self.backend._profile_config
            self.backend._profile_config = {'small_video_bitrate': plan['video_kbps'], 'video_bitrate': plan['video_kbps']}
            try:
                fps = rate(plan['fps'])
                command = self.backend._h264_encode_command(x264, '-', str(raw), plan['width'], plan['height'],
                    plan['fps'], fps.numerator, fps.denominator)
            finally:
                self.backend._profile_config = old_config
            command[command.index('lavf')] = 'y4m'
            i = command.index('--vf'); del command[i:i + 2]
            # Scene cuts can exhaust the fixed seek table on a full movie.
            # A regular four-second IDR interval bounds both seek and index cost.
            command += ['--fps', plan['fps'], '--scenecut', '0']
            for pass_no in passes:
                # The audited Apple encoder crashes reading macroblock-tree
                # statistics in pass two. Keep ordinary two-pass rate control,
                # disabling that optional allocation heuristic in both passes.
                encode_command = command + (['--no-mbtree', '--slow-firstpass', '--pass', str(pass_no), '--stats', str(directory / 'pass.log')] if pass_no else [])
                with tempfile.TemporaryFile() as decode_log, tempfile.TemporaryFile() as encode_log:
                    producer = subprocess.Popen(video_args + ['-f', 'yuv4mpegpipe', '-'], stdout=subprocess.PIPE, stderr=decode_log, cwd=directory)
                    try:
                        consumer = subprocess.Popen(encode_command, stdin=producer.stdout, stdout=subprocess.DEVNULL, stderr=encode_log, cwd=directory)
                        producer.stdout.close()
                        code = consumer.wait(); source_code = producer.wait()
                        if code or source_code:
                            errors = []
                            for log in (decode_log, encode_log):
                                log.seek(max(0, log.tell() - 2000)); errors.append(log.read().decode('utf-8', 'replace'))
                            raise RuntimeError(f'Video encoder exit {code}; source reader exit {source_code}.\n'+'\n'.join(errors))
                    finally:
                        if producer.poll() is None: producer.kill(); producer.wait()
            command = [self.ffmpeg, '-y', '-v', 'error'] + source_args + ['-i', str(raw), '-map', '1:v:0']
            if plan['audio']: command += ['-map', f"0:{plan['audio']['index']}"]
            command += ['-c:v', 'copy'] + self.audio_args(plan)
        else:
            command = video_args[:]
            # Remove -an only when an audio track was selected.
            if plan['audio']:
                command.remove('-an'); command += ['-map', f"0:{plan['audio']['index']}"]
            command += ['-c:v', 'mpeg2video', '-threads', '2', '-pix_fmt', 'yuv420p',
                        '-bf', '0', '-g', '12', '-b:v', f"{plan['video_kbps']}k",
                        '-maxrate', '1600k', '-bufsize', '800k'] + self.audio_args(plan)
            if len(passes) == 2:
                run(command + ['-pass', '1', '-passlogfile', str(directory / 'pass'), '-f', 'null', '-'], cwd=directory)
                command += ['-pass', '2', '-passlogfile', str(directory / 'pass')]
        command += ['-y', '-map_metadata', '-1', '-map_chapters', '-1', '-sn', '-dn',
                    '-t', str(duration), '-color_primaries', 'smpte170m', '-color_trc', 'bt709',
                    '-colorspace', 'smpte170m', '-color_range', 'tv'] + self.mux_args(plan) + [str(output)]
        # Filter paths for burned captions are relative to this rendition work dir.
        with tempfile.TemporaryFile() as log:
            result = subprocess.run(command, cwd=directory, stderr=log, stdout=subprocess.DEVNULL)
            if result.returncode:
                log.seek(max(0, log.tell() - 4000)); raise RuntimeError(log.read().decode('utf-8', 'replace'))

    def prepare(self, row, settings, mount=None, target='', preview=False):
        settings = options(settings)
        settings = settings.get('items', {}).get(str(row.get('id')), settings)
        plan = self.plan(row['file_path'], settings, mount, target)
        identity = str(row.get('logical_video_id') or
                       ('library-' + str(row['id']) if row.get('id') else 'source'))
        identity += ':' + plan['source_sha256']
        logical_id = hashlib.sha256(identity.encode()).hexdigest()[:32]
        rendition = hashlib.sha256((logical_id + ':' + plan['key']).encode()).hexdigest()
        key = plan['key'] + ('-preview' if preview else '')
        output = self.cache / (key + ('.m4v' if plan['format'] == 'h264' else '.mpg'))
        record = output.with_suffix('.json')
        device_path = ''
        with self.backend._bundle_lock(str(output)):
            self.status('Check prepared rendition', plan['source'])
            from services.video_sync_transaction import recorded_rendition
            existing = recorded_rendition(mount, rendition) if not preview else None
            if existing:
                output, validation, device_path = existing
            elif not preview and plan['operation'] == 'copy':
                output = Path(plan['source']); validation = plan['validation']
            elif output.exists() and record.exists() and json.loads(record.read_text()).get('sha256') == digest_file(output):
                validation = json.loads(record.read_text())['validation']
            else:
                required = (plan['estimated_bytes'] * 2 + 64 * 1024 * 1024) if not preview else 64 * 1024 * 1024
                if shutil.disk_usage(self.cache).free < required:
                    raise RuntimeError(f'Not enough cache space: about {required / 1048576:.0f} MiB needed. Choose another cache location.')
                with tempfile.TemporaryDirectory(prefix=key[:16], dir=self.cache) as temp:
                    directory = Path(temp); temporary = directory / output.name
                    if plan['subtitle'] and plan['options']['subtitle_mode'] == 'burn' and plan['subtitle']['codec_name'] not in ('dvd_subtitle','hdmv_pgs_subtitle','dvb_subtitle'):
                        run([self.ffmpeg, '-v', 'error', '-i', plan['subtitle'].get('file', plan['source']), '-map', f"0:{plan['subtitle']['index']}", '-c:s', 'ass', str(directory / 'selected.ass')])
                    self.status('Convert' if preview or plan['operation'] == 'convert' else 'Remux', plan['source'])
                    if preview or plan['operation'] == 'convert': self.encode(plan, temporary, directory, preview)
                    else: self.remux(plan, temporary, audio_convert=plan['operation'] == 'audio')
                    self.status('Validate', plan['source'])
                    validation = validate(temporary, plan['format'], self.ffmpeg, self.ffprobe)
                    if plan['options']['target_mib'] and not preview and temporary.stat().st_size > plan['options']['target_mib'] * 1048576:
                        raise RuntimeError('Output exceeded the selected size budget; it was not published.')
                    with open(temporary, 'rb') as complete: os.fsync(complete.fileno())
                    os.replace(temporary, output)
                    atomic_json(record, {'plan': plan, 'sha256': validation['sha256'], 'validation': validation})
            sidecars = {}
            if not preview:
                metadata = self.cache / (rendition + '.media')
                # A new source timeline invalidates old state conservatively;
                # changing only quality/container keeps the same identity.
                # Bounded, simple text for core/plugin readers; no path-based ID.
                metadata.write_text('id=' + logical_id + '\n', encoding='ascii')
                sidecars['.media'] = str(metadata)
                if plan['subtitle'] and plan['options']['subtitle_mode'] == 'soft':
                    caption = self.cache / (key + '.srt')
                    run([self.ffmpeg, '-y', '-v', 'error', '-i', plan['subtitle'].get('file', plan['source']), '-map', f"0:{plan['subtitle']['index']}", '-c:s', 'srt', str(caption)])
                    sidecars['.srt'] = str(caption)
                    from services.video_captions import compile_srt
                    packed = self.cache / (key + '.nfs')
                    compile_srt(caption, packed, 0 if plan['subtitle'].get('file') else
                                round(float(plan['info']['video'].get('start_time') or 0) * 1000))
                    sidecars['.nfs'] = str(packed)
                chapters = self.cache / (key + '.chapters')
                lines = ['# Rockpod chapters v1\n']
                previous = -1
                for chapter in plan['info'].get('chapters', [])[:256]:
                    title = str(chapter.get('tags', {}).get('title', 'Chapter')).replace('\n', ' ').replace('\t', ' ')[:96]
                    start = round((float(chapter['start_time']) - float(plan['info']['video'].get('start_time') or 0)) * 1000)
                    if start <= previous or not 0 <= start < plan['duration']*1000:
                        raise ValueError('Chapters have invalid ordering or timestamps.')
                    previous = start
                    lines.append(f"{start}\t{title}\n")
                chapters.write_text(''.join(lines), encoding='utf-8'); sidecars['.chapters'] = str(chapters)
        result = dict(row)
        result.update(sync_source_path=str(output), sync_output_ext=output.suffix,
            sync_transcoded=True, sync_video_profile=profile_string(plan['options']),
            sync_video_codec='H.264' if plan['format'] == 'h264' else 'MPEG-2',
            sync_video_width=int(validation['video']['width']), sync_video_height=int(validation['video']['height']),
            sync_video_fps=float(rate(plan['fps'])), sync_video_sidecars=sidecars,
            sync_video_validation=validation, sync_reuse_device_path=device_path,
            sync_video_cache_key=plan['key'],
            sync_video_rendition=rendition, file_hash=validation['sha256'], file_size=os.path.getsize(output),
            duration=plan['duration'], codec='H.264' if plan['format'] == 'h264' else 'MPEG-2')
        from models.track import compute_metadata_hash
        result['metadata_hash'] = compute_metadata_hash(
            result.get('title', ''), result.get('artist', ''), result.get('album', ''),
            result.get('album_artist', ''), result.get('track_number'), result.get('disc_number', 1),
            result.get('genre', ''), result.get('year'), result.get('composer', ''),
            result.get('duration', 0), result.get('bitrate', 0), result.get('codec', ''),
            result.get('media_type', ''), result.get('video_kind', ''), result.get('show_title', ''),
            result.get('season_number'), result.get('episode_number'))
        return result, {'converted': plan['operation'] == 'convert', 'reason': plan['reason'],
                        'plan': plan, 'validation': validation, 'cache_path': str(output)}

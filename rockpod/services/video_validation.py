"""Bounded full-stream checks against the shared installed-player contract."""
from __future__ import annotations
import json
import os
import subprocess
import tempfile
from pathlib import Path
from fractions import Fraction
from services.apple_video_exact import (
    AppleVideoContractError, _atom_extent, _read_moov, _sample_nals, _rbsp,
    decode_ffprobe_hex, parse_avcc, parse_sps, parse_pps, parse_slice, top_level_atoms,
)
from services.video_capabilities import contract_definition, contract_digest

Error = AppleVideoContractError

def _tables(path, limits):
    atoms = top_level_atoms(path)
    if any(a[0] in {'moof', 'mfra'} for a in atoms):
        raise Error('fragmented MP4 is not supported')
    if sum(a[0]=='mdat' for a in atoms)!=1:
        raise Error('need one finalized MP4 media-data atom')
    data = _read_moov(path)
    def visit(start, end, depth=0):
        if depth > 12: raise Error('excessively nested MP4 index')
        offset=start
        while offset<end:
            kind, size, header=_atom_extent(data,offset,end)
            p=offset+header; atom_end=offset+size
            if kind in {b'moov',b'trak',b'mdia',b'minf',b'stbl',b'edts'}:
                visit(p,atom_end,depth+1)
            elif kind == b'elst':
                if atom_end-p < 8 or data[p] not in (0,1):
                    raise Error('invalid edit list')
                count=int.from_bytes(data[p+4:p+8],'big')
                width=20 if data[p] else 12
                if count != 1 or atom_end-p < 8+width:
                    raise Error('multiple/empty timeline edits require normalization')
                entry=p+8
                media=entry+(8 if data[p] else 4)
                media_width=8 if data[p] else 4
                if int.from_bytes(data[media:media+media_width],'big',signed=True)<0:
                    raise Error('empty timeline edits require normalization')
                if data[media+media_width:media+media_width+4] != b'\0\1\0\0':
                    raise Error('non-unit edit rate is unsupported')
            elif kind in {b'stts',b'stss',b'stco',b'co64',b'stsc',b'stsz'}:
                if atom_end-p<8: raise Error('truncated sample table')
                if data[p]!=0: raise Error('unsupported sample-table version')
                count=int.from_bytes(data[p+4:p+8],'big'); entries=p+8
                widths={b'stts':8,b'stss':4,b'stco':4,b'co64':8,b'stsc':12,b'stsz':4}
                width=widths[kind]
                if kind==b'stsz':
                    if atom_end-p<12: raise Error('truncated sample sizes')
                    fixed=count; count=int.from_bytes(data[p+8:p+12],'big'); entries=p+12
                    if fixed: width=0
                    if fixed>limits['sample_buffer_bytes']: raise Error('sample exceeds read buffer')
                    if count>limits['audio_samples']: raise Error('too many samples')
                if width and count > (atom_end - entries) // width:
                    raise Error('sample table exceeds its atom')
                if kind==b'stts' and count>limits['timing_runs']: raise Error('too many timing runs')
                if kind==b'stss' and count>limits['sync_samples']: raise Error('too many random-access entries')
                if kind in {b'stco',b'co64'}:
                    for entry in range(entries,entries+count*width,width):
                        value=int.from_bytes(data[entry:entry+width],'big')
                        if value>=os.path.getsize(path): raise Error('chunk offset outside file')
            offset=atom_end
    visit(0,len(data))
    return atoms

class AvcStreamValidator:
    """One compressed access unit at a time; never retain decoded frames."""
    def __init__(self, extradata, limits=None):
        self.limits=limits or contract_definition()['limits']
        if len(extradata)>self.limits['codec_data_bytes']: raise Error('codec data exceeds firmware buffer')
        self.length_size,self.sps_units,self.pps_units=parse_avcc(extradata)
        if len(self.sps_units)!=1 or len(self.pps_units)!=1: raise Error('multiple parameter sets are unqualified')
        self.sps=parse_sps(self.sps_units[0]); self.pps=parse_pps(self.pps_units[0])
        s,p=self.sps,self.pps; l=self.limits
        if (s['profile_idc']!=66 or s['level_idc']>l['h264_level']
            or s['pic_order_cnt_type']!=l['h264_poc_type']
            or s['max_num_ref_frames']>l['h264_references']
            or not s['frame_mbs_only'] or s['gaps_allowed']
            or s['crop_left'] or s['crop_top']
            or not 0<s['width']<=s['coded_width']<=l['h264_width']
            or not 0<s['height']<=s['coded_height']<=l['h264_height']
            or not 0<=s['log2_max_frame_num_minus4']<=12
            or not 0<=s.get('log2_max_pic_order_cnt_lsb_minus4',-1)<=12):
            raise Error('SPS exceeds installed decoder syntax/dimensions')
        if (p['sps_id']!=s['sps_id'] or p['entropy_coding_mode'] or p['weighted_pred']
            or p['weighted_bipred_idc'] or p['num_ref_idx_l0_default_active_minus1']):
            raise Error('PPS exceeds installed decoder restrictions')
        self.pictures=0; self.max_sample=0; self.max_dma=0; self.idr=0

    def feed(self,sample):
        if not sample or len(sample)>self.limits['sample_buffer_bytes']: raise Error('oversized/empty access unit')
        slices=[]; dma=0
        for nal in _sample_nals(sample,self.length_size):
            if nal[0]&128: raise Error('forbidden H.264 NAL bit')
            kind=nal[0]&31
            if kind in {7,8}:
                expected=self.sps_units[0] if kind==7 else self.pps_units[0]
                if nal!=expected: raise Error('in-stream parameter-set change is unqualified')
            elif kind in {1,5}:
                part=parse_slice(nal,self.sps,self.pps)
                if part['slice_type'] not in {0,2} or part['active_refs_minus1'] or part['pps_id']!=self.pps['pps_id']:
                    raise Error('unsupported slice/reference configuration')
                slices.append(part); dma+=(len(_rbsp(nal))+31)&~31
            elif kind not in {6,9,10,11,12}:
                raise Error('unsupported H.264 NAL type')
        if not 1<=len(slices)<=self.limits['slices_per_picture']: raise Error('unsupported slice count')
        if dma>self.limits['slice_dma_bytes']: raise Error('picture exceeds slice DMA workspace')
        previous=-1; total=self.sps['coded_width']*self.sps['coded_height']//256
        for part in slices:
            mb=part['first_mb_in_slice']
            if mb<=previous or mb>=total: raise Error('invalid slice macroblock order')
            for key in ('frame_num','poc_lsb','idr_pic_id','slice_type','nal_type','nal_ref_idc'):
                if part.get(key)!=slices[0].get(key): raise Error('mixed pictures in one access unit')
            previous=mb
        if slices[0]['first_mb_in_slice']!=0: raise Error('incomplete picture')
        if not self.pictures and slices[0]['nal_type']!=5: raise Error('first picture is not random-access')
        self.pictures+=1; self.idr+=slices[0]['nal_type']==5
        self.max_sample=max(self.max_sample,len(sample)); self.max_dma=max(self.max_dma,dma)
        if self.pictures>self.limits['video_samples']: raise Error('too many video samples')


def validate_full_h264(path, video, ffprobe='ffprobe', ffmpeg='ffmpeg', *, decode=True):
    limits=contract_definition()['limits']; size=os.path.getsize(path)
    if size>limits['desktop_max_file_bytes']: raise Error('file exceeds conservative player offset limit')
    _tables(path,limits)
    validator=AvcStreamValidator(decode_ffprobe_hex(video.get('extradata')),limits)
    video_index=int(video['index']); last={}; counts={}; timing_runs=0; previous_duration=None
    try:
        time_base = Fraction(video['time_base'])
        if time_base <= 0: raise ValueError('nonpositive time base')
    except (KeyError, ValueError, ZeroDivisionError) as exc:
        raise Error('invalid video time base') from exc
    # Stream packet metadata so long movies cannot create a huge JSON object.
    command=[ffprobe,'-v','error','-show_packets','-show_entries',
             'packet=stream_index,pos,size,pts,dts,duration','-of','compact=p=0:nk=0',str(path)]
    with tempfile.TemporaryFile() as errors, open(path,'rb') as media:
        process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=errors,text=True)
        try:
            for line in process.stdout:
                packet=dict(part.split('=',1) for part in line.strip().split('|') if '=' in part)
                if 'stream_index' not in packet: continue
                try:
                    index=int(packet['stream_index']); pos=int(packet['pos']); length=int(packet['size'])
                    dts=int(packet['dts']); pts=int(packet['pts']); duration=int(packet['duration'])
                except (KeyError,ValueError) as exc: raise Error('missing packet timing/extent') from exc
                if pos<0 or length<=0 or pos>size-length or duration<=0: raise Error('invalid packet extent/timing')
                if index in last and dts<=last[index]: raise Error('non-monotonic decode timestamps')
                last[index]=dts; counts[index]=counts.get(index,0)+1
                if index==video_index:
                    if pts!=dts: raise Error('reordered video timestamps unsupported')
                    if duration * time_base < Fraction(1, 30):
                        raise Error('video packet cadence exceeds 30 fps')
                    if duration!=previous_duration: timing_runs+=1; previous_duration=duration
                    if timing_runs>limits['timing_runs']: raise Error('too many timing runs')
                    if length>limits['sample_buffer_bytes']: raise Error('oversized video sample')
                    media.seek(pos); validator.feed(media.read(length))
                elif counts[index]>limits['audio_samples']: raise Error('too many audio samples')
            if process.wait()!=0: raise Error('full-stream packet scan failed')
            errors.seek(0)
            if errors.read(1): raise Error('container errors during packet scan')
        finally:
            if process.poll() is None: process.kill(); process.wait()
            process.stdout.close()
    if not validator.pictures: raise Error('no video pictures')
    if video.get('nb_frames') not in (None,'N/A') and int(video['nb_frames'])!=validator.pictures:
        raise Error('packet count differs from declared sample count')
    if decode:
        with tempfile.TemporaryFile() as errors:
            result=subprocess.run([ffmpeg,'-v','error','-xerror','-threads','2','-i',str(path),
                                   '-map','0:v:0','-map','0:a:0?','-f','null','-'],
                                  stdout=subprocess.DEVNULL,stderr=errors,check=False)
            errors.seek(0)
            if result.returncode or errors.read(1): raise Error('host decode check failed')
    return {'scope':'full_stream','contract_sha256':contract_digest(),
            'pictures':validator.pictures,'random_access_pictures':validator.idr,
            'max_access_unit_bytes':validator.max_sample,'max_slice_dma_bytes':validator.max_dma,
            'timing_runs':timing_runs,'host_decode_checked':decode,'hardware_qualified':False}

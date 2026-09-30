"""Installed-player identity, separate from model and hardware qualification."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path

SOURCE = Path(__file__).with_name('data')/'video-capabilities-v1.json'

def contract_definition():
    return json.loads(SOURCE.read_text(encoding='utf-8'))

def contract_digest():
    data=json.dumps(contract_definition(),sort_keys=True,separators=(',',':')).encode()
    return hashlib.sha256(data).hexdigest()

def _hash(path):
    result=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''): result.update(chunk)
    return result.hexdigest()

def find_device_mount(path):
    current=Path(path).absolute()
    for parent in (current, *current.parents):
        if (parent/'.rockbox/rockbox-info.txt').is_file(): return str(parent)
    return ''

def read_device_capabilities(mount):
    """A stale package, unknown contract or mismatched image is not evidence."""
    fallback={'verified':False,'target':'','capabilities':{'h264':{'implemented':False},
              'mpeg':{'implemented':True,'decoder':'software','conversion_envelope':[320,240]}},
              'reason':'Installed video capabilities are unknown; use conservative MPEG.'}
    if not mount: return fallback
    root=Path(mount)
    try:
        info=dict(line.split(':',1) for line in (root/'.rockbox/rockbox-info.txt').read_text().splitlines() if ':' in line)
        target=info.get('Target','').strip(); fallback['target']=target
        record=json.loads((root/'.rockbox/video-capabilities.json').read_text())
        definition=contract_definition(); base=definition['aliases'].get(target,target)
        if (record.get('schema')!=1 or record.get('contract_sha256')!=contract_digest()
            or record.get('target')!=target or record.get('build')!=info.get('Version','').strip()
            or record.get('capabilities')!=definition['targets'].get(base)
            or record.get('limits')!=definition['limits']
            or record.get('qualification')!=definition['qualification']):
            return {**fallback,'reason':'Capability record does not match this player/build; use conservative MPEG.'}
        images=[root/'rockbox.ipod',root/'.rockbox/rockbox.ipod']
        if any(not p.is_file() or _hash(p)!=record.get('firmware_sha256') for p in images):
            return {**fallback,'reason':'Firmware image and capability record differ; use conservative MPEG.'}
        return {**record,'verified':True,'reason':'Installed player limits verified; hardware qualification is recorded separately.'}
    except (OSError,ValueError,KeyError,TypeError):
        return fallback

def require_h264(mount):
    record=read_device_capabilities(mount)
    if not record['verified'] or not record['capabilities']['h264'].get('implemented'):
        raise RuntimeError('H.264 sync is unavailable for this installed player. '+record['reason'])
    return record

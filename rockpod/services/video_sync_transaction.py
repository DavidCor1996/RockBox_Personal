"""Recoverable video publication: staged bytes, read-back hash, then index.

The journal resides on the removable volume, so moving it to another host
does not lose recovery evidence. A published rendition is immutable. Old
renditions are retired only after the caller has committed its library.
"""
from pathlib import Path
import hashlib
import json
import os
from services.video_pipeline import atomic_json, digest_file
from services.path_safety import resolve_under_root


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY)
    try: os.fsync(fd)
    finally: os.close(fd)


def verified_copy(source, destination, expected=None, cancelled=lambda: False, status=lambda phase: None):
    source, destination = Path(source), Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    staged = destination.with_name(destination.name + '.rockpod-part')
    digest = hashlib.sha256(); size = 0
    try:
        status('Copy')
        with source.open('rb') as incoming, staged.open('wb') as outgoing:
            for chunk in iter(lambda: incoming.read(1024 * 1024), b''):
                if cancelled(): raise InterruptedError('Video sync cancelled')
                digest.update(chunk); size += len(chunk); outgoing.write(chunk)
            outgoing.flush(); os.fsync(outgoing.fileno())
        sha = digest.hexdigest()
        if expected and sha != expected:
            raise RuntimeError('Prepared source changed before it could be synced.')
        status('Verify destination')
        if staged.stat().st_size != size or digest_file(staged) != sha:
            raise RuntimeError('Device read-back verification failed.')
        if cancelled(): raise InterruptedError('Video sync cancelled')
        status('Publish')
        os.replace(staged, destination)
        sync_directory(destination.parent)
        return {'sha256': sha, 'bytes': size, 'verification': 'full_destination_readback'}
    finally:
        staged.unlink(missing_ok=True)


class VideoSyncTransaction:
    def __init__(self, mount, rendition):
        self.mount = Path(mount).resolve()
        self.rendition = rendition
        if len(rendition) != 64 or any(c not in '0123456789abcdef' for c in rendition):
            raise ValueError('Invalid rendition identity')
        self.journal = self.mount / '.rockbox/rockpod/video-transactions' / (rendition + '.json')

    def publish(self, row, destination, cancelled=lambda: False, status=lambda phase: None):
        dest = Path(destination).resolve()
        rel = str(dest.relative_to(self.mount))
        # Path safety includes symlink escapes, not only ../ string checks.
        resolve_under_root(str(self.mount), rel)
        data = {'schema': 1, 'state': 'staging', 'rendition': self.rendition,
                'track_id': row.get('id'), 'destination': rel, 'files': [],
                'validation': row.get('sync_video_validation', {}),
                'cache_key': row.get('sync_video_cache_key', ''),
                'previous': row.get('sync_previous_device_path', '')}
        atomic_json(self.journal, data)
        pairs = [(Path(source), dest.with_suffix(suffix), None)
                 for suffix, source in row.get('sync_video_sidecars', {}).items()]
        pairs.append((Path(row['sync_source_path']), dest, row['file_hash']))
        for source, target, expected in pairs:
            # Recovery reuses only verified complete bytes; never trust size alone.
            sha = expected or digest_file(source)
            if target.is_file() and digest_file(target) == sha:
                evidence = {'sha256': sha, 'bytes': target.stat().st_size,
                            'verification': 'full_destination_readback'}
            else:
                evidence = verified_copy(source, target, sha, cancelled, status)
            data['files'].append({'path': str(target.relative_to(self.mount)), **evidence})
            atomic_json(self.journal, data)
        data['state'] = 'published'
        from services.video_state import migrate_legacy_state
        data['legacy_state_migrated'] = migrate_legacy_state(self.mount, row)
        atomic_json(self.journal, data)
        sync_directory(self.journal.parent)

    def indexed(self):
        data = json.loads(self.journal.read_text())
        if data['state'] != 'published': raise RuntimeError('Incomplete video publication')
        data['state'] = 'indexed'
        atomic_json(self.journal, data)
        sync_directory(self.journal.parent)


def recorded_rendition(mount, key):
    """Reuse a published movie after cache eviction only after full read-back."""
    if not mount:
        return None
    transaction = VideoSyncTransaction(mount, key)
    try:
        data = json.loads(transaction.journal.read_text())
        if data['state'] not in ('published', 'indexed') or data['rendition'] != key:
            return None
        validation = data['validation']
        destination = resolve_under_root(str(transaction.mount), data['destination'])
        Path(destination).resolve().relative_to(transaction.mount)
        if not validation.get('full_stream', {}).get('host_decode_checked'):
            return None
        if digest_file(destination) != validation['sha256']:
            return None
        return Path(destination), validation, data['destination']
    except (OSError, ValueError, KeyError):
        return None


def cleanup_published_cache(cache, mount, dry_run=False):
    """Evict only completed, indexed renditions; device journals permit reuse."""
    removed = []
    directory = Path(mount) / '.rockbox/rockpod/video-transactions'
    for journal in directory.glob('*.json'):
        try:
            data = json.loads(journal.read_text())
            key = data.get('cache_key', '')
            if (data['state'] != 'indexed' or len(key) != 64 or
                    any(c not in '0123456789abcdef' for c in key)):
                continue
            installed = Path(resolve_under_root(mount, data['destination'])).resolve()
            installed.relative_to(Path(mount).resolve())
            if installed.stat().st_size != data['validation']['bytes']:
                continue
            for suffix in ('.m4v', '.mpg'):
                movie = Path(cache) / 'prepared-v2' / (key + suffix)
                if movie.is_file() and digest_file(movie) == data['validation']['sha256']:
                    size = movie.stat().st_size
                    if not dry_run:
                        movie.unlink()
                    removed.append((str(movie), size))
        except (OSError, ValueError, KeyError):
            continue
    return removed

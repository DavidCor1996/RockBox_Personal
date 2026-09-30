"""MSN contacts and authored, deterministic offline conversations for RockPod."""
from __future__ import annotations
import calendar
import csv
import hashlib
import json
import os
import random
import re
import shutil
import tempfile
import uuid
from datetime import datetime, timedelta
from pathlib import Path
from PIL import Image, ImageOps, ImageSequence
from services.file_safety import atomic_write_text
from services.path_safety import validate_device_root

MAX_CONTACTS = 64
MAX_EVENTS = 1024
KINDS = ('message', 'photo', 'gif', 'video', 'nudge', 'status', 'wink')


def clean(value, length):
    text = ' '.join(str(value or '').replace('\x00', '').split())
    encoded = text.encode('utf-8')
    if len(encoded) > length:
        raise ValueError(f'Text is too long (maximum {length} UTF-8 bytes)')
    return text


def identifier(value):
    value = str(value)
    if not re.fullmatch(r'[a-zA-Z0-9_-]{1,32}', value):
        raise ValueError('Invalid MSN identifier')
    return value


def device_path(root, relative):
    root = Path(root).resolve()
    target = root / relative
    # Do not permit existing symlinks at any level, including leaves.
    cursor = target
    while cursor != root:
        if cursor.is_symlink():
            raise ValueError('MSN sync refuses symbolic-link destinations')
        if cursor.parent == cursor:
            raise ValueError('Unsafe MSN destination')
        cursor = cursor.parent
    if not target.resolve().is_relative_to(root):
        raise ValueError('Unsafe MSN destination')
    return target


def compile_schedule(plan, anchor=None):
    """RTC timestamps represent local wall time, matching Rockbox mktime."""
    rolling = plan.get('schedule_mode') == 'fresh_sync'
    anchor = anchor or datetime.now().replace(microsecond=0)
    month = anchor.replace(hour=0, minute=0, second=0) if rolling else datetime.strptime(plan['month'], '%Y-%m')
    if not 2020 <= month.year <= 2037:
        raise ValueError('Choose a month between 2020 and 2037 (iPod clock range)')
    days = 30 if rolling else calendar.monthrange(month.year, month.month)[1]
    contacts = plan.get('contacts', [])
    if len(contacts) > MAX_CONTACTS:
        raise ValueError('MSN supports up to 64 contacts')
    ids = [identifier(c['id']) for c in contacts]
    if len(set(ids)) != len(ids):
        raise ValueError('Duplicate contact identifier')
    events = []
    occupied = {}
    event_ids = set()
    for item in plan.get('messages', []):
        key = identifier(item['id'])
        if key in event_ids:
            raise ValueError('Duplicate conversation identifier')
        event_ids.add(key)
        contact = identifier(item['contact'])
        if contact not in ids:
            raise ValueError('A conversation references a missing contact')
        kind = item.get('kind', 'message')
        if kind not in KINDS:
            raise ValueError('Unknown message type')
        first, last = int(item.get('day', 1)), int(item.get('last_day', item.get('day', 1)))
        if not 1 <= first <= last <= days:
            raise ValueError(f'Choose days between 1 and {days}')
        start = datetime.strptime(item.get('after', '18:00'), '%H:%M')
        end = datetime.strptime(item.get('before', '21:00'), '%H:%M')
        lo, hi = start.hour * 3600 + start.minute * 60, end.hour * 3600 + end.minute * 60
        if hi <= lo:
            raise ValueError('Delivery windows must end after they start on the same day')
        campaign = str(plan.get('campaign_id') or plan['month'])
        seed = hashlib.sha256((campaign + key).encode()).digest()
        rng = random.Random(seed)
        day = rng.randint(first, last)
        options=item.get('reply_options',[])
        if not isinstance(options,list) or len(options)>5:
            raise ValueError('A conversation supports up to five reply choices')
        for option in options:
            if not clean(option.get('text',''),511) or not clean(option.get('response',''),511):
                raise ValueError('Each reply choice needs its text and a follow-up')
        parts = item.get('parts')
        if parts is None:
            lines = str(item.get('text', '')).splitlines() or ['']
            if kind != 'message':
                lines = [' '.join(lines)]
            parts = [dict(kind=kind, text=line, media=item.get('media','')) for line in lines]
        if not parts:
            raise ValueError('Write a conversation before adding it')
        reserve = len(parts) * 45
        if hi - lo <= reserve:
            raise ValueError('Delivery window is too short for this conversation')
        date = month + timedelta(days=day-1) if rolling else month.replace(day=day)
        when = date + timedelta(seconds=rng.randint(lo, hi-reserve))
        if rolling and day == 1:
            # A first sync starts the welcome conversation after a short pause.
            when = anchor + timedelta(seconds=90+rng.randint(0,180))
        previous = occupied.get((contact, day))
        if previous and when <= previous:
            when = previous + timedelta(minutes=3)
        for index, part in enumerate(parts):
            part_kind = part.get('kind', 'message')
            if part_kind not in KINDS:
                raise ValueError('Unknown message type')
            line = clean(part.get('text',''),511)
            if part_kind == 'message' and not line:
                raise ValueError('Write a message before adding it')
            if index:
                when += timedelta(seconds=rng.randint(8,25)+min(len(line)//8,20))
            if not (rolling and day == 1) and (when.date()!=date.date() or when.hour*3600+when.minute*60+when.second>=hi):
                raise ValueError('Too many conversations in this delivery window')
            if part_kind == 'status' and line not in ('Online','Away','Busy','Offline'):
                raise ValueError('Status must be Online, Away, Busy, or Offline')
            events.append({'id':hashlib.sha256(f"{campaign}:{key}:{index}".encode()).hexdigest()[:16],
                           'contact':contact,'at':calendar.timegm(when.timetuple()),
                           'kind':part_kind,'text':line,'media':str(part.get('media') or ''),
                           'typing':rng.randint(2,6),'reply_options':item.get('reply_options',[]),'reply_group':hashlib.sha256(f'{campaign}:{key}'.encode()).hexdigest()[:20]})
        occupied[(contact, day)] = when
    if len(events) > MAX_EVENTS:
        raise ValueError('A month can contain at most 1024 messages')
    return sorted(events, key=lambda row: (row['at'], row['id']))


class MsnAppService:
    def __init__(self, config, repo_root):
        self.config, self.repo_root = config, Path(repo_root)
        self.root = Path(config.get('cache_dir') or Path.home()/'.rockpod/cache')/'msn'

    def load(self):
        path = self.root/'plan.json'
        if not path.exists():
            return {'version': 1, 'month': datetime.now().strftime('%Y-%m'),
                    'contacts': [], 'messages': []}
        return json.loads(path.read_text(encoding='utf-8'))

    def save(self, plan):
        compile_schedule(plan)
        for c in plan['contacts']:
            clean(c['name'], 127)
            clean(c.get('email',''), 95)
            clean(c.get('status',''), 255)
        self.root.mkdir(parents=True, exist_ok=True)
        atomic_write_text(self.root/'plan.json', json.dumps(plan, ensure_ascii=False, indent=2)+'\n')

    def import_contacts(self, filename):
        """Import explicit user-selected CSV; profile pictures are local paths."""
        plan = self.load()
        with open(filename, encoding='utf-8-sig', newline='') as stream:
            rows = list(csv.DictReader(stream))
        for row in rows:
            name = row.get('name') or row.get('Name') or row.get('Display Name')
            if not name:
                raise ValueError('Contact CSV needs a name column; email, status and pfp are optional')
            email = row.get('email') or row.get('E-mail Address') or ''
            existing = next((c for c in plan['contacts'] if email and c.get('email') == email), None)
            contact = existing or {'id': uuid.uuid4().hex[:16]}
            picture = row.get('pfp', '')
            if picture and not Path(picture).is_absolute():
                picture = str(Path(filename).resolve().parent/picture)
            contact.update(name=name, email=email, status=row.get('status',''), pfp=picture)
            if not existing:
                plan['contacts'].append(contact)
        self.save(plan)
        return len(rows)

    def import_instagram(self, value, progress=None):
        """Reuse RockPod's selected profile cache, or its maintained importer."""
        import unicodedata
        from urllib.parse import urlparse
        username = urlparse(value).path.strip('/').split('/')[0] if '://' in value else value.strip('@ /')
        if not re.fullmatch(r'[A-Za-z0-9_.]{1,30}', username):
            raise ValueError('Enter an Instagram profile URL or username')
        configured = self.config.get('instagram_cache_dir')
        candidates = [Path(configured)] if configured else []
        candidates += [Path(self.config.get('cache_dir') or Path.home()/'.rockpod/cache')/'instagram']
        candidates += sorted((Path('/run/media')/os.environ.get('USER','')).glob('*/RockPod/instagram'))
        profile = None
        cache = None
        for candidate in candidates:
            try:
                payload=json.loads((candidate/'library.json').read_text())
                profile=next((c for c in payload.get('profiles',[]) if c.get('username')==username),None)
                if profile:
                    cache=candidate
                    break
            except (OSError,ValueError):
                continue
        if profile is None:
            from services.instagram_app import InstagramAppService
            service=InstagramAppService(self.config,self.repo_root)
            service.import_profile('https://www.instagram.com/'+username+'/', progress=progress)
            profile=next(c for c in service.list_profiles() if c['username']==username)
            cache=service.root
        def locate(path):
            p=Path(path)
            if p.is_file():return p
            for base in [cache/username,cache/username/'media']:
                if (base/p.name).is_file():return base/p.name
            return None
        self.root.mkdir(parents=True,exist_ok=True)
        personal=self.root/'imports'/username
        personal.mkdir(parents=True,exist_ok=True)
        def keep(path):
            source=locate(path)
            if not source:return ''
            target=personal/source.name
            if not target.exists():shutil.copyfile(source,target)
            return str(target)
        # Stored Instagram fallback artwork is identified as imported artwork.
        pfp=keep(profile.get('avatar_path','')) if profile.get('avatar_path') else ''
        media=[]
        for row in profile.get('media',[]):
            source=locate(row.get('source_path',''))
            if not source:continue
            media.append({'path':str(source),'caption':row.get('caption',row.get('title','')),
                          'kind':row.get('type','photo'),'source_url':row.get('post_url','')})
        name=unicodedata.normalize('NFKC',profile.get('display_name') or username)
        name=name.strip()
        bio=unicodedata.normalize('NFKC',profile.get('bio',''))
        bio=bio.encode('utf-8')[:255].decode('utf-8','ignore')
        plan=self.load()
        existing=next((c for c in plan['contacts'] if c.get('instagram')==username),None)
        contact=existing or {'id':uuid.uuid4().hex[:16]}
        contact.update(name=name,email='',status=bio,pfp=pfp,instagram=username,
                       source_url=profile.get('url'),source_name=profile.get('display_name'),
                       source_bio=profile.get('bio'),media_library=media)
        if not existing:plan['contacts'].append(contact)
        self.save(plan)
        return {'contact':name,'available_media':len(media)}

    def imported_media(self):
        """Offer existing local app downloads without logging in or downloading."""
        cache = Path(self.config.get('cache_dir') or Path.home()/'.rockpod/cache')
        items = []
        seen = set()
        for app, label in [('instagram', 'Instagram'), ('onlyfans', 'OnlyFans')]:
            root = cache/app
            if app == 'instagram' and self.config.get('instagram_cache_dir'):
                root = Path(self.config.get('instagram_cache_dir'))
            try:
                library = json.loads((root/'library.json').read_text(encoding='utf-8'))
            except (OSError, ValueError):
                continue
            for profile in library.get('profiles', []):
                for item in profile.get('media', []):
                    source = Path(item.get('source_path') or '')
                    if not source.is_file() or str(source) in seen:
                        continue
                    suffix = source.suffix.lower()
                    if suffix not in {'.jpg','.jpeg','.png','.webp','.bmp','.gif',
                                      '.mp4','.mov','.m4v','.mpg','.mpeg','.webm'}:
                        continue
                    seen.add(str(source))
                    kind = 'gif' if suffix == '.gif' else 'video' if suffix in {
                        '.mp4','.mov','.m4v','.mpg','.mpeg','.webm'} else 'photo'
                    items.append({'path':str(source), 'kind':kind,
                                  'label':f"{label} · @{profile.get('username','')} · "
                                          f"{item.get('title') or source.name}"})
        return items

    def sync(self, mount_path, progress=None):
        plan = self.load()
        if not plan['contacts']:
            raise ValueError('Add contacts before syncing Messenger')
        mount = Path(validate_device_root(mount_path))
        root = device_path(mount, '.rockbox/msn')
        root.mkdir(parents=True, exist_ok=True)
        campaign = identifier(plan.get('campaign_id') or plan['month'])
        anchors_path = device_path(mount,'.rockbox/msn/campaigns.json')
        anchors = json.loads(anchors_path.read_text()) if anchors_path.exists() else {}
        anchor = datetime.fromisoformat(anchors.get(campaign) or datetime.now().replace(microsecond=0).isoformat())
        events = compile_schedule(plan,anchor)
        archive_path = device_path(mount,'.rockbox/msn/archive.tsv')
        archived = {}
        if archive_path.exists():
            for row in csv.DictReader(archive_path.open(encoding='utf-8'),delimiter='\t'):
                archived[row['id']] = row
        else:
            pointer = root/'current.txt'
            if pointer.exists():
                old = pointer.read_text().strip()
                if not re.fullmatch('[a-f0-9]{16}',old):raise ValueError('Invalid previous Messenger bundle')
                old_manifest = root/'bundles'/old/'events.tsv'
                if old_manifest.exists():
                    for row in csv.DictReader(old_manifest.open(encoding='utf-8'),delimiter='\t'):
                        if row['media']:row['media'] = f"/.rockbox/msn/bundles/{old}/{row['media']}"
                        archived[row['id']] = row
        # Previously synced event identities and delivery times are immutable.
        for e in events:
            if e['id'] in archived:
                e['at'] = int(archived[e['id']]['at'])
        events.sort(key=lambda e:(e['at'],e['id']))
        device_path(mount, '.rockbox/msn/current.txt')
        bundles = device_path(mount, '.rockbox/msn/bundles')
        bundles.mkdir(exist_ok=True)
        stage = Path(tempfile.mkdtemp(prefix='.stage-', dir=bundles))
        try:
            (stage/'media').mkdir()
            (stage/'saved').mkdir()
            (stage/'avatars').mkdir()
            contacts = ['id\tname\temail\tstatus\tavatar']
            for c in plan['contacts']:
                cid = identifier(c['id'])
                source = Path(c['pfp']) if c.get('pfp') else self.repo_root/'assets/ipodjs/sources/msn/PNG-298.png'
                with Image.open(source) as im:
                    ImageOps.fit(im.convert('RGB'), (48,48)).save(stage/f'avatars/{cid}.bmp')
                contacts.append('\t'.join([cid,clean(c['name'],127),clean(c.get('email',''),95),
                                           clean(c.get('status',''),255),f'avatars/{cid}.bmp']))
            manifest = ['id\tat\tcontact\tkind\ttyping\ttext\tmedia']
            for i, event in enumerate(events):
                if progress:
                    progress(f'Preparing conversation {i+1} of {len(events)}')
                media = ''
                kind = event['kind']
                if kind == 'wink':
                    wink=identifier(event['media'])
                    if not (self.repo_root/f'assets/ipodjs/rockbox/msn/winks/{wink}.mwa').is_file():
                        raise ValueError('Choose an installed MSN Wink')
                    media=f'/.rockbox/ipodjs/msn/winks/{wink}.mwa'
                if kind in ('photo','gif','video'):
                    source = Path(event['media'])
                    if not source.is_file():
                        raise ValueError(f'Missing attachment: {source}')
                    media = f"media/{event['id']}"
                    if kind == 'photo':
                        media += '.bmp'
                        with Image.open(source) as im:
                            image = ImageOps.exif_transpose(im).convert('RGB')
                            ImageOps.pad(image,(300,160),color='white').save(stage/media)
                            image.save(stage/f"saved/{event['id']}.jpg", quality=95,
                                       subsampling=0, progressive=False)
                    elif kind == 'gif':
                        media += '.mga'
                        self._animation(source, stage/media)
                        with Image.open(source) as im:
                            if im.format != 'GIF':
                                raise ValueError('Choose a GIF file for animated attachments')
                        shutil.copyfile(source, stage/f"saved/{event['id']}.gif")
                    else:
                        from services.app_video_sync import stage_app_video
                        media += '.mpg'
                        stage_app_video(source, stage/media, config=self.config, profile='quality',
                                        cache_namespace='msn-video',device_key='msn',title=event['text'])
                    if kind in ('photo', 'gif'):
                        with Image.open(source) as im:
                            image = ImageOps.exif_transpose(im).convert('RGB')
                            for suffix, size in [('thumb', (64,48)), ('preview', (320,320))]:
                                thumb = image.copy()
                                thumb.thumbnail(size, Image.Resampling.LANCZOS)
                                thumb.save(stage/f"saved/{event['id']}.{suffix}.bmp")
                manifest.append('\t'.join(str(event[k]) for k in ('id','at','contact','kind','typing','text'))+'\t'+media)
            (stage/'contacts.tsv').write_text('\n'.join(contacts)+'\n',encoding='utf-8')
            (stage/'events.tsv').write_text('\n'.join(manifest)+'\n',encoding='utf-8')
            # Content-addressed immutable bundle. Publish only after ALL media succeeds.
            digest = hashlib.sha256()
            for file in sorted(stage.rglob('*')):
                if file.is_file():
                    digest.update(str(file.relative_to(stage)).encode())
                    with file.open('rb') as stream:
                        for block in iter(lambda:stream.read(1024*1024), b''):
                            digest.update(block)
            generation = digest.hexdigest()[:16]
            target = device_path(mount, f'.rockbox/msn/bundles/{generation}')
            if not target.exists():
                os.replace(stage, target)
            for event, row in zip(events, manifest[1:]):
                fields = row.split('\t')
                if fields[6] and not fields[6].startswith('/'):fields[6] = f"/.rockbox/msn/bundles/{generation}/{fields[6]}"
                archived.setdefault(event['id'],dict(zip(manifest[0].split('\t'),fields)))
            archive_rows = [manifest[0]] + ['\t'.join(str(e[k]) for k in manifest[0].split('\t'))
                for e in sorted(archived.values(),key=lambda e:(int(e['at']),e['id']))]
            def publish(relative, content):
                dst=device_path(mount,relative)
                tmp=device_path(mount,relative+'.msnpart')
                tmp.write_text(content,encoding='utf-8');os.replace(tmp,dst)
            publish('.rockbox/msn/archive.tsv','\n'.join(archive_rows)+'\n')
            anchors.setdefault(campaign,anchor.isoformat())
            publish('.rockbox/msn/campaigns.json',json.dumps(anchors,indent=2)+'\n')
            # Keep removed contacts available to historical conversations.
            contact_archive = device_path(mount,'.rockbox/msn/archive-contacts.tsv')
            old_contacts = {}
            if contact_archive.exists():
                for row in csv.DictReader(contact_archive.open(encoding='utf-8'),delimiter='\t'):
                    old_contacts[row['id']]=row
            for row in contacts[1:]:
                fields=row.split('\t');fields[4]=f"/.rockbox/msn/bundles/{generation}/{fields[4]}"
                old_contacts[fields[0]]=dict(zip(contacts[0].split('\t'),fields))
            publish('.rockbox/msn/archive-contacts.tsv',contacts[0]+'\n'+'\n'.join(
                '\t'.join(c[k] for k in contacts[0].split('\t')) for c in old_contacts.values())+'\n')
            choices=[]
            for event in events:
                for index,option in enumerate(event.get('reply_options',[])[:5]):
                    choices.append('\t'.join([event['id'],clean(option['text'],511),
                        'q-'+event['reply_group']+'-'+str(index),clean(option.get('response',''),511)]))
            publish('.rockbox/msn/choices.tsv','\n'.join(choices)+'\n')
            responses=['id\tcontact\tkeywords\ttext']
            for row in plan.get('responses',[]):
                if len(identifier(row['id']))>30:raise ValueError('Response identifiers must be at most 30 characters')
                responses.append('\t'.join([identifier(row['id']),identifier(row['contact']),
                                            clean(row.get('keywords',''),127),clean(row['text'],511)]))
            publish('.rockbox/msn/responses.tsv','\n'.join(responses)+'\n')
            # Original assets and icon use atomic replacement; state stays outside bundles.
            for relative, src in [
                *[(f'.rockbox/ipodjs/msn/{p.relative_to(self.repo_root / "assets/ipodjs/rockbox/msn")}',p) for p in (self.repo_root/'assets/ipodjs/rockbox/msn').rglob('*') if p.is_file()],
                ('.rockbox/ipodjs/applications/msn.46x46x24.bmp',self.repo_root/'assets/ipodjs/rockbox/applications/msn.46x46x24.bmp'),
                ('.rockbox/ipodjs/tv-applications/msn.80x80x24.bmp',self.repo_root/'assets/ipodjs/rockbox/tv-applications/msn.80x80x24.bmp')]:
                dst = device_path(mount, relative)
                dst.parent.mkdir(parents=True, exist_ok=True)
                device_path(mount, relative+'.msnpart')
                shutil.copyfile(src, str(dst)+'.msnpart')
                os.replace(str(dst)+'.msnpart',dst)
            device_path(mount, '.rockbox/msn/current.txt.tmp')
            # Explicit safe sibling instead of an implicit temp-file name.
            pointer = root/'current.txt.msnpart'
            device_path(mount, '.rockbox/msn/current.txt.msnpart')
            pointer.write_text(generation+'\n')
            os.replace(pointer,root/'current.txt')
            return {'contacts':len(plan['contacts']),'messages':len(events),'month':plan['month'],'generation':generation}
        finally:
            if stage.exists():
                shutil.rmtree(stage)

    @staticmethod
    def _animation(source, target):
        """Bounded streamed RGB565 animation, original frame timing retained."""
        import struct
        with Image.open(source) as im:
            if getattr(im,'n_frames',1) > 600:
                raise ValueError('Animated attachments are limited to 600 frames')
            with target.open('wb') as out:
                out.write(struct.pack('<4sHHH',b'MGA1',160,120,getattr(im,'n_frames',1)))
                for frame in ImageSequence.Iterator(im):
                    image = ImageOps.pad(frame.convert('RGB'),(160,120),color='white')
                    out.write(struct.pack('<H',min(60000,max(20,frame.info.get('duration',100)))))
                    data=bytearray()
                    for r,g,b in image.getdata():
                        data.extend(struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3)))
                    out.write(data)

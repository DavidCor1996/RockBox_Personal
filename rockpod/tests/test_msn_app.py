import calendar
import copy
from pathlib import Path
import pytest
from PIL import Image
from services.msn_app import MsnAppService,compile_schedule

ROOT=Path(__file__).resolve().parents[2]


def plan():
    return {'month':'2026-10','contacts':[{'id':'alice','name':'Alice','email':'','status':'brb','pfp':''}],
            'messages':[{'id':'first','contact':'alice','kind':'message','text':'hello\nhow are you?',
                         'day':1,'last_day':31,'after':'18:00','before':'21:00'}]}


def test_deterministic_burst_in_window():
    p=plan();a=compile_schedule(p);assert a==compile_schedule(copy.deepcopy(p))
    assert 8<=a[1]['at']-a[0]['at']<=45
    assert all(18<=__import__('datetime').datetime.utcfromtimestamp(e['at']).hour<21 for e in a)
    assert a[0]['id']!=a[1]['id']


@pytest.mark.parametrize('change',[{'day':0},{'last_day':32},{'after':'22:00','before':'07:00'},
                                  {'contact':'missing'},{'kind':'unknown'},{'text':'x'*512}])
def test_invalid_schedule(change):
    p=plan();p['messages'][0].update(change)
    with pytest.raises(ValueError):compile_schedule(p)


def test_duplicate_ids_and_leap_year():
    p=plan();p['messages'].append(copy.deepcopy(p['messages'][0]))
    with pytest.raises(ValueError):compile_schedule(p)
    p=plan();p['month']='2028-02';p['messages'][0].update(day=29,last_day=29)
    assert len(compile_schedule(p))==2
    p['month']='2027-02'
    with pytest.raises(ValueError):compile_schedule(p)


def test_transaction_preserves_state_and_idempotence(tmp_path):
    s=MsnAppService({'cache_dir':str(tmp_path/'cache')},ROOT);s.save(plan())
    mount=tmp_path/'device';mount.mkdir()
    a=s.sync(mount);root=mount/'.rockbox/msn';(root/'read.tsv').write_text('first\n')
    (root/'replies.tsv').write_text('my reply\n');(root/'delivered.txt').write_text('123\n')
    before={p.name:p.read_bytes() for p in root.iterdir() if p.is_file()}
    b=s.sync(mount);assert a==b
    assert all((root/name).read_bytes()==data for name,data in before.items())
    p=s.load();p['messages'].append({'id':'bad','contact':'alice','kind':'photo','text':'',
        'day':2,'last_day':2,'after':'18:00','before':'21:00','media':str(tmp_path/'missing.png')})
    s.save(p)
    with pytest.raises(ValueError):s.sync(mount)
    assert (root/'current.txt').read_bytes()==before['current.txt']
    assert not list((root/'bundles').glob('.stage-*'))


def test_symlink_refused(tmp_path):
    s=MsnAppService({'cache_dir':str(tmp_path/'cache')},ROOT);s.save(plan())
    mount=tmp_path/'device';mount.mkdir();outside=tmp_path/'outside';outside.mkdir()
    (mount/'.rockbox').symlink_to(outside,target_is_directory=True)
    with pytest.raises(ValueError,match='symbolic'):s.sync(mount)
    assert not list(outside.iterdir())


def test_photo_and_gif_bundle(tmp_path):
    s=MsnAppService({'cache_dir':str(tmp_path/'cache')},ROOT)
    p=plan();photo=tmp_path/'image.png';Image.new('RGB',(25,20),'red').save(photo)
    gif=tmp_path/'image.gif';Image.new('RGB',(10,10),'red').save(gif,save_all=True,
        append_images=[Image.new('RGB',(10,10),'blue')],duration=[100,200],loop=0)
    p['messages']=[dict(p['messages'][0],id='photo',kind='photo',media=str(photo)),
                   dict(p['messages'][0],id='gif',kind='gif',media=str(gif),day=31,last_day=31)]
    s.save(p);mount=tmp_path/'device';mount.mkdir();s.sync(mount)
    media=list((mount/'.rockbox/msn/bundles').glob('*/media/*'))
    assert {p.suffix for p in media}=={'.bmp','.mga'}
    assert next(p for p in media if p.suffix=='.mga').read_bytes()[:10]==b'MGA1\xa0\x00x\x00\x02\x00'
    saved = next((mount/'.rockbox/msn/bundles').glob('*/saved'))
    assert Image.open(next(saved.glob('*.jpg'))).size == (25,20)
    assert next(saved.glob('*.gif')).read_bytes() == gif.read_bytes()
    assert len(list(saved.glob('*.thumb.bmp'))) == 2
    # Photos owns the copied file, independent of any subsequent MSN bundle.
    album = mount/'Photos/MSN Messenger'
    album.mkdir(parents=True)
    original = next(saved.glob('*.jpg')).read_bytes()
    (album/'saved.jpg').write_bytes(original)
    p['messages'] = []
    s.save(p)
    s.sync(mount)
    assert (album/'saved.jpg').read_bytes() == original


def test_contacts_csv_update(tmp_path):
    s=MsnAppService({'cache_dir':str(tmp_path/'cache')},ROOT)
    f=tmp_path/'contacts.csv';f.write_text('name,email,status\nAlice,alice@example.test,hello\n')
    s.import_contacts(f);first=s.load()['contacts'][0]['id']
    f.write_text('name,email,status\nAlice,alice@example.test,away\n');s.import_contacts(f)
    assert len(s.load()['contacts'])==1
    assert s.load()['contacts'][0]['id']==first
    assert s.load()['contacts'][0]['status']=='away'


def test_first_sync_anchor_and_sequence(tmp_path):
    import json
    from datetime import datetime,timedelta
    p=plan();p.update(schedule_mode='fresh_sync',campaign_id='welcome')
    p['contacts'][0]['name']='🌷Jasmine🌷'
    p['messages'][0].update(day=1,last_day=1,parts=[{'kind':'message','text':'hello 🌷'}, {'kind':'message','text':'a different thought :)'}])
    anchor=datetime(2026,9,30,23,59)
    events=compile_schedule(p,anchor)
    assert 90<=(datetime.utcfromtimestamp(events[0]['at'])-anchor).total_seconds()<=270
    assert events[1]['at']>events[0]['at']
    assert [e['id'] for e in events]==[e['id'] for e in compile_schedule(p,anchor+timedelta(days=12))]
    s=MsnAppService({'cache_dir':str(tmp_path/'cache')},ROOT);s.save(p)
    mount=tmp_path/'device';mount.mkdir();s.sync(mount);root=mount/'.rockbox/msn'
    archive=(root/'archive.tsv').read_bytes();anchors=(root/'campaigns.json').read_bytes()
    (root/'local.tsv').write_text('persistent replies')
    s.sync(mount)
    assert (root/'archive.tsv').read_bytes()==archive
    assert (root/'campaigns.json').read_bytes()==anchors
    assert (root/'local.tsv').read_text()=='persistent replies'
    assert '🌷Jasmine🌷' in (root/'archive-contacts.tsv').read_text()


def test_old_campaign_and_attachment_history_survive(tmp_path):
    import csv
    p=plan();p['messages'][0]['text']='old conversation'
    s=MsnAppService({'cache_dir':str(tmp_path/'cache')},ROOT);s.save(p)
    mount=tmp_path/'device';mount.mkdir();s.sync(mount);root=mount/'.rockbox/msn'
    old=(root/'archive.tsv').read_text().splitlines()[1]
    p['month']='2026-11';p['messages'][0].update(last_day=30,text='new conversation')
    s.save(p);s.sync(mount);s.sync(mount)
    rows=list(csv.DictReader((root/'archive.tsv').open(),delimiter='\t'))
    assert len(rows)==2 and len({r['id'] for r in rows})==2
    assert old in (root/'archive.tsv').read_text()


def test_context_choices_are_once_per_conversation_and_winks_sync(tmp_path):
    import csv
    p=plan();p['messages'][0].update(parts=[{'kind':'message','text':'A little hello'},
        {'kind':'wink','text':'Kiss','media':'kiss'}],
        reply_options=[{'text':'That was sweet','response':'A tiny kiss for your window :)'}])
    s=MsnAppService({'cache_dir':str(tmp_path/'cache')},ROOT);s.save(p)
    mount=tmp_path/'device';mount.mkdir();result=s.sync(mount);root=mount/'.rockbox/msn'
    choices=[line.split('\t') for line in (root/'choices.tsv').read_text().splitlines()]
    assert len(choices)==2 and choices[0][2]==choices[1][2]
    assert choices[0][0]!=choices[1][0]
    rows=list(csv.DictReader((root/'bundles'/result['generation']/'events.tsv').open(),delimiter='\t'))
    assert rows[1]['kind']=='wink' and rows[1]['media']=='/.rockbox/ipodjs/msn/winks/kiss.mwa'
    (root/'winks-played.tsv').write_text(rows[1]['id']+'\n')
    s.sync(mount)
    assert (root/'winks-played.tsv').read_text()==rows[1]['id']+'\n'


def test_original_wink_streams_are_bounded_and_have_sound():
    import struct
    import json
    folder=ROOT/'assets/ipodjs/rockbox/msn/winks'
    catalog=json.loads((folder/'catalog.json').read_text())
    assert len(catalog)==20
    for wink in catalog:
        data=(folder/(wink['id']+'.mwa')).read_bytes()
        magic,w,h,frames=struct.unpack_from('<4sHHH',data)
        assert (magic,w,h)==(b'MWA2',240,160) and 1<frames<=600
        pos=10;duration=0
        for _ in range(frames):
            delay,runs=struct.unpack_from('<HH',data,pos);pos+=4;duration+=delay
            assert 0<runs<=w*h
            assert sum(struct.unpack_from('<H',data,pos+i*4)[0] for i in range(runs))==w*h
            pos+=runs*4
        assert pos==len(data) and abs(duration-wink['duration_ms'])<100
        sound=(folder/(wink['id']+'.pcm')).read_bytes()
        assert 0<len(sound)<=400000 and any(sound) and len(sound)%2==0

#!/usr/bin/env bash
set -euo pipefail

# One-track fixture; reuse the established isolated runtime and window helpers.
source "$(dirname "${BASH_SOURCE[0]}")/ipodjs_navigation_sim_regression.sh"
if [ "${IPODJS_WPS_HEADLESS:-0}" = 1 ]; then
    source "$(dirname "${BASH_SOURCE[0]}")/ipodjs_sim_headless.sh"
fi

wps_page()
{
    awk -F '\t' '$4 == "WPS Select" { value=$10 } END { print value+0 }' \
        "${runtime_root}/.rockbox/ipodjs-trace.tsv"
}

wps_next_page()
{
    local expected="$1" before
    before="$(trace_last_sequence)"
    tap_key KP_5 0.45
    wait_for_trace_after "WPS Select" "${before}"
    [ "$(wps_page)" = "${expected}" ] || {
        printf 'expected WPS page %s, found %s\n' "${expected}" "$(wps_page)" >&2
        exit 1
    }
}

wps_retail_run()
{
    local before source_music
    mkdir -p "${out_dir}"
    trap cleanup EXIT INT TERM HUP
    prepare_root
    # Own the test sidecars: never write through the source Music symlink.
    if [ -L "${runtime_root}/Music" ]; then
        source_music="$(readlink -f "${runtime_root}/Music")"
        rm "${runtime_root}/Music"
        cp -a "${source_music}" "${runtime_root}/Music"
    fi
    [ -f "${runtime_root}/Music/test.mp3" ]
    printf '[00:01.00]First lyric line\n[00:02.00]Second lyric line\n' \
        >"${runtime_root}/Music/test.lrc"
    printf '\nnotifications: off\nbacklight timeout: on\n' >>"${runtime_root}/.rockbox/config.cfg"
    cp "${repo_root}/wps/ipodjs-classic.wps" \
        "${runtime_root}/.rockbox/wps/ipodjs-classic.wps"
    launch_sim
    wait_for_home
    tap_key KP_2
    tap_key KP_5
    tap_key KP_2
    tap_key KP_5
    tap_key KP_5
    before="$(trace_last_sequence)"
    tap_key KP_5
    wait_for_audio_after 1 "${before}"
    capture "01-music-note"
    wps_next_page 1
    capture "02-rating"
    wps_next_page 2
    capture "04-scrubber"
    before="$(trace_last_sequence)"
    tap_key space
    wait_for_audio_after 3 "${before}"
    capture "05-scrubber-paused"
    tap_key KP_2
    capture "06-scrubber-forward"
    tap_key KP_8
    capture "07-scrubber-back"
    before="$(trace_last_sequence)"
    tap_key space
    wait_for_audio_after 1 "${before}"
    wps_next_page 3
    tap_key KP_2
    capture "08-shuffle-songs"
    tap_key KP_8
    capture "09-shuffle-off"
    wps_next_page 4
    capture "10-native-lyrics"
    tap_key KP_8
    tap_key KP_2
    wps_next_page 0
    capture "11-artwork-return"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal
    wait_for_trace_kind_after list "${before}"
    capture "12-source-list"
    cp "${runtime_root}/.rockbox/ipodjs-trace.tsv" "${out_dir}/ipodjs-trace.tsv"
    cleanup_sim
    "${repo_root}/rockpod/.venv/bin/python" - "${out_dir}" "${repo_root}" <<'PY'
import csv, struct, sys
from pathlib import Path
from PIL import Image
out, root = map(Path, sys.argv[1:])
rows=list(csv.DictReader((out/'ipodjs-trace.tsv').open(), delimiter='\t'))
playing=[r for r in rows if int(r['audio']) & 1]
assert playing and all(r['path']=='/Music/test.mp3' for r in playing)
assert all(r['playlist_count']=='1' for r in playing)
assert [int(r['selected']) for r in rows if r['name']=='WPS Select']==[1,2,3,4,0]
assert not any(r['name'] in ('Playlist','Loading Music') for r in rows)
paused=[int(r['elapsed_ms']) for r in rows if r['kind']=='wps' and r['audio']=='3']
assert any(b-a >= 900 for a,b in zip(paused,paused[1:])),paused
assert any(a-b >= 900 for a,b in zip(paused,paused[1:])),paused
assert Image.open(out/'08-shuffle-songs.png').tobytes() != Image.open(out/'09-shuffle-off.png').tobytes()
# Exact extracted note pixels, projected through the existing artwork geometry.
data=(root/'assets/ipodjs/apple/retailos-2.0.4/resources/006.rga').read_bytes()[8:]
for filename in ('01-music-note.png','11-artwork-return.png'):
    image=Image.open(out/filename).convert('RGB')
    differences=0
    for dx in range(136):
        top=dx*10//135
        height=136-top
        sx=dx*128//136
        for dy in range(height):
            sy=dy*128//height
            pixel=struct.unpack_from('<H',data,(sy*128+sx)*3)[0]
            r,g,b=image.getpixel((10+dx,34+top+dy))
            actual=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
            differences += actual != pixel
    assert differences==0,(filename,differences)
print('WPS pages, playback identity, source return, and exact note pixels passed')
PY
    printf 'WPS RetailOS simulator captures: %s\n' "${out_dir}"
}

wps_retail_run

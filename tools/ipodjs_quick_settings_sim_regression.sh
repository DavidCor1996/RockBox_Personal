#!/usr/bin/env bash
set -euo pipefail

# Reuse the isolated fixture, exact LCD capture and input helpers of the
# navigation gate. No writes to a mounted player or the source simdisk.
source "$(dirname "${BASH_SOURCE[0]}")/ipodjs_navigation_sim_regression.sh"

qs_selected()
{
    awk -F '\t' '$4 == "Quick Settings" { n = $10 } END { print n }' \
        "${runtime_root}/.rockbox/ipodjs-trace.tsv"
}

qs_seek()
{
    local target="$1" attempt
    for attempt in $(seq 1 24); do
        [ "$(qs_selected)" = "${target}" ] && return
        tap_key KP_2 0.12
    done
    printf 'Quick Settings did not reach row %s\n' "${target}" >&2
    exit 1
}

qs_run()
{
    local before count parent_top restored_top
    mkdir -p "${out_dir}"
    trap cleanup EXIT INT TERM HUP
    prepare_root
    printf '\nnotifications: off\n' >>"${runtime_root}/.rockbox/config.cfg"
    launch_sim
    wait_for_home
    if [ "${IPODJS_QS_PLAYBACK:-0}" = 1 ]; then
        # The focused playback fixture has one tagged artist/album/track.
        tap_key KP_2
        tap_key KP_5
        tap_key KP_2
        tap_key KP_5
        tap_key KP_5
        tap_key KP_5
        before="$(trace_last_sequence)"
        tap_key KP_5
        wait_for_audio_after 1 "${before}"
        for level in 1 2 3 4 5; do tap_key w; done
    fi
    before="$(trace_last_sequence)"
    hold_key w 1.3
    wait_for_trace_after "Quick Settings" "${before}"
    capture "01-list"
    count="$(awk -F '\t' '$4 == "Quick Settings" { n = $12 }
        END { print n }' "${runtime_root}/.rockbox/ipodjs-trace.tsv")"
    [ "$(qs_selected)" = 0 ]

    # Both editors must push once, update in place and pop to the same row.
    for row in 0 1; do
        qs_seek "${row}"
        before="$(trace_last_sequence)"
        tap_key KP_5
        wait_for_trace_after "$([ "$row" = 0 ] && echo Volume || echo Brightness)" "${before}"
        capture "02-editor-${row}"
        tap_key KP_8
        capture "03-adjusted-${row}"
        tap_key KP_2
        tap_key w
        [ "$(qs_selected)" = "${row}" ]
    done

    # Endpoint wrap and reverse scroll retain an independent viewport.
    qs_seek 0
    tap_key KP_8
    [ "$(qs_selected)" = "$((count - 1))" ]
    capture "04-bottom"
    tap_key KP_2
    [ "$(qs_selected)" = 0 ]
    qs_seek "$((count - 2))"
    parent_top="$(awk -F '\t' '$4 == "Quick Settings" { n = $11 }
        END { print n }' "${runtime_root}/.rockbox/ipodjs-trace.tsv")"
    tap_key KP_5
    capture "05-maintenance"
    tap_key KP_5 1.5
    tap_key KP_5 1.5
    capture "05-maintenance-refreshed"
    tap_key KP_2
    tap_key KP_5
    capture "06-restart-confirmation"
    tap_key w
    tap_key w
    [ "$(qs_selected)" = "$((count - 2))" ]
    restored_top="$(awk -F '\t' '$4 == "Quick Settings" { n = $11 }
        END { print n }' "${runtime_root}/.rockbox/ipodjs-trace.tsv")"
    [ "${parent_top}" = "${restored_top}" ]

    # Theme settings must cause complete redraws and preserve access to all
    # rows. Font cycles cover normal, large and small; density toggles twice.
    qs_seek 2
    tap_key KP_5
    capture "07-dark-list"
    qs_seek 8
    for variant in 1 2 3; do
        tap_key KP_5
        capture "08-font-${variant}"
    done
    qs_seek 7
    tap_key KP_5
    capture "09-density"
    tap_key KP_5
    qs_seek 2
    tap_key KP_5
    capture "10-restored-list"
    touch "${runtime_root}/hold.gate"
    sleep 0.5
    capture "11-hold"
    rm "${runtime_root}/hold.gate"
    sleep 0.5
    tap_key w
    capture "12-return"
    for cycle in $(seq 1 10); do
        hold_key w 1.3
        qs_seek 1
        tap_key KP_5
        tap_key KP_8
        tap_key KP_2
        tap_key w
        before="$(trace_last_sequence)"
        tap_key w
        wait_for_trace_after "Home" "${before}"
    done
    cleanup_sim
    cp "${runtime_root}/.rockbox/ipodjs-trace.tsv" "${out_dir}/ipodjs-trace.tsv"
    python3 - "${out_dir}/ipodjs-trace.tsv" <<'PY'
import csv, os, sys
rows = list(csv.DictReader(open(sys.argv[1]), delimiter='\t'))
qs = [r for r in rows if r['name'] == 'Quick Settings']
assert any(r['update'] == 'row-delta' for r in qs)
assert any(r['update'] == 'list-delta' for r in qs)
assert any(r['update'] == 'slider-delta' for r in rows)
groups = []
for r in rows:
    if r['name'] != 'Transition':
        continue
    if r['first'] == '0':
        groups.append([])
    assert groups
    groups[-1].append(r)
for group in groups:
    assert [int(r['first']) for r in group] == list(range(8))
    positions = [int(r['selected']) for r in group]
    assert positions == sorted(positions)
    assert 24 <= int(group[-1]['tick']) - int(group[0]['tick']) <= 28
assert len(groups) >= 8
if os.environ.get('IPODJS_QS_PLAYBACK') == '1':
    assert all(int(r['audio']) & 1 for r in qs)
    assert len({(r['path'], r['playlist_index'], r['playlist_count'])
                for r in qs}) == 1
    elapsed = [int(r['elapsed_ms']) for r in qs]
    assert elapsed == sorted(elapsed)
    assert len({r['core_available'] for r in qs}) == 1
    assert len({r['rockbox_open_files'] for r in qs}) == 1
print(f'Quick Settings gate passed: {len(qs)} list frames, {len(groups)} transitions')
PY
}

qs_run "$@"

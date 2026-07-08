#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${OUT_DIR:-$repo_root/test-videos/openh264-ipod6g}"
ipod_mount="${IPOD_MOUNT:-/run/media/david/OWNER_S IPO}"
ipod_video_dir="${IPOD_VIDEO_DIR:-$ipod_mount/Videos/OpenH264}"
ipod_log_dir="${IPOD_LOG_DIR:-$ipod_mount/.rockbox/openh264}"
ipod_rock_dir="${IPOD_ROCK_DIR:-$ipod_mount/.rockbox/rocks/viewers}"
build_dir="${BUILD_DIR:-$repo_root/build-hw-ipod6g}"

quick_src="${QUICK_SRC:-$repo_root/docs/ipone-original-full-art-slideshow/slideshow-glide.mp4}"
phone_src="${PHONE_SRC:-/home/david/Downloads/snaptik_7240088802050641198_v3.mp4}"
long_src="${LONG_SRC:-/home/david/Videos/TV Shows/6teen_2004_complete_series_202508/6teen S02E27 Girlie Boys.mp4}"

mkdir -p "$out_dir"
run_log="$out_dir/install.log"

log() {
    printf '%s %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" "$*" | tee -a "$run_log"
}

require_file() {
    local path="$1"
    if [[ ! -f "$path" ]]; then
        echo "missing source: $path" >&2
        exit 1
    fi
}

convert_h264() {
    local src="$1"
    local out="$2"
    local fps="$3"
    local bitrate="$4"
    local maxrate="$5"
    local bufsize="$6"
    local keyint=$((fps * 2))

    ffmpeg -y -loglevel error -i "$src" \
        -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=$fps" \
        -an \
        -c:v libx264 -profile:v baseline -level:v 1.3 -pix_fmt yuv420p \
        -preset veryslow \
        -x264-params "ref=1:bframes=0:cabac=0:weightp=0:8x8dct=0:subme=6:me=hex:keyint=$keyint:min-keyint=$keyint:scenecut=0" \
        -b:v "$bitrate" -maxrate "$maxrate" -bufsize "$bufsize" \
        -f h264 "$out"
}

convert_raw_rvp() {
    local src="$1"
    local base="$2"
    local seconds="$3"

    ffmpeg -y -loglevel error -t "$seconds" -i "$src" \
        -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=20" \
        -an -pix_fmt yuv420p -f rawvideo "$out_dir/$base.yuv"

    if [[ -n "$(ffprobe -v error -select_streams a:0 -show_entries stream=index -of csv=p=0 "$src")" ]]; then
        ffmpeg -y -loglevel error -t "$seconds" -i "$src" \
            -vn -ac 2 -ar 44100 -f s16le "$out_dir/$base.pcm"
    else
        ffmpeg -y -loglevel error -f lavfi -t "$seconds" \
            -i "anullsrc=channel_layout=stereo:sample_rate=44100" \
            -f s16le "$out_dir/$base.pcm"
    fi

    {
        echo "ROCKPOD_RAW_VIDEO_V1"
        echo "width=320"
        echo "height=240"
        echo "fps=20"
        echo "sample_rate=44100"
        echo "channels=2"
        echo "video=$base.yuv"
        echo "audio=$base.pcm"
    } > "$out_dir/$base.rvp"
}

probe_file() {
    local file="$1"
    ffprobe -v error \
        -show_entries format=duration,bit_rate \
        -show_entries stream=index,codec_type,codec_name,width,height,r_frame_rate,avg_frame_rate,pix_fmt,profile,level,has_b_frames \
        -of default=nw=1 "$file"
}

require_file "$quick_src"
require_file "$phone_src"
if [[ "${INCLUDE_LONG:-0}" == "1" ]]; then
    require_file "$long_src"
fi

: > "$run_log"

rm -f "$out_dir"/*.h264 "$out_dir"/*.rvp "$out_dir"/*.yuv "$out_dir"/*.pcm

if [[ "${INCLUDE_H264:-0}" == "1" ]]; then
    log "converting quick_20.h264 from $quick_src"
    convert_h264 "$quick_src" "$out_dir/quick_20.h264" 20 350k 450k 900k
    log "converting quick_24.h264 from $quick_src"
    convert_h264 "$quick_src" "$out_dir/quick_24.h264" 24 450k 600k 1200k
    log "converting phone_20.h264 from $phone_src"
    convert_h264 "$phone_src" "$out_dir/phone_20.h264" 20 350k 450k 900k
    log "converting phone_24.h264 from $phone_src"
    convert_h264 "$phone_src" "$out_dir/phone_24.h264" 24 450k 600k 1200k
fi

if [[ "${INCLUDE_QUICK_RAW:-0}" == "1" ]]; then
    log "converting quick_raw.rvp from $quick_src"
    convert_raw_rvp "$quick_src" "quick_raw" 6
fi

log "converting phone_raw.rvp from $phone_src"
convert_raw_rvp "$phone_src" "phone_raw" 14

if [[ "${INCLUDE_LONG:-0}" == "1" ]]; then
    log "converting long_20.h264 from $long_src"
    convert_h264 "$long_src" "$out_dir/long_20.h264" 20 350k 450k 900k
    log "converting long_24.h264 from $long_src"
    convert_h264 "$long_src" "$out_dir/long_24.h264" 24 450k 600k 1200k
fi

{
    date -u
    shopt -s nullglob
    for file in "$out_dir"/*.h264; do
        echo "== $file =="
        probe_file "$file"
    done
    for file in "$out_dir"/*.rvp; do
        base="${file%.rvp}"
        echo "== $file =="
        cat "$file"
        wc -c "$base.yuv" "$base.pcm"
    done
} > "$out_dir/manifest.txt"

if [[ ! -d "$ipod_mount" ]]; then
    echo "iPod mount not found: $ipod_mount" >&2
    exit 1
fi

mkdir -p "$ipod_video_dir"
mkdir -p "$ipod_log_dir"
rm -f "$ipod_video_dir"/*.h264 "$ipod_video_dir"/quick_raw.*
cp -a "$out_dir"/*.rvp "$out_dir"/*.yuv "$out_dir"/*.pcm "$ipod_video_dir/"
if compgen -G "$out_dir/*.h264" > /dev/null; then
    cp -a "$out_dir"/*.h264 "$ipod_video_dir/"
fi
cp -a "$out_dir/manifest.txt" "$ipod_video_dir/"
cp -a "$out_dir/manifest.txt" "$ipod_log_dir/openh264_sample_manifest.txt"
cp -a "$run_log" "$ipod_log_dir/openh264_install.log"

if [[ -f "$build_dir/apps/plugins/openh264_player.rock" ]]; then
    mkdir -p "$ipod_rock_dir"
    cp -a "$build_dir/apps/plugins/openh264_player.rock" "$ipod_rock_dir/"
fi

log "installed OpenH264 samples to $ipod_video_dir"
log "installed logs to $ipod_log_dir"
if [[ ! -f "$build_dir/apps/plugins/openh264_player.rock" ]]; then
    echo "openh264_player.rock was not found in $build_dir; build Rockbox before device testing." >&2
fi

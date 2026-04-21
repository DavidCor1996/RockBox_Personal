#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
sim_root="$repo_root/build-sim-video-5g/simdisk"
video_dir="$sim_root/Videos/iPodTikTok"
apps_dir="$sim_root/.rockbox/rocks/apps"
feed_path="$apps_dir/.ipodtiktok_feed.tsv"

sources=(
  "/home/david/Downloads/ipodtiktok1.mp4:ipodtiktok1"
  "/home/david/Downloads/ipodtiktok2.mp4:ipodtiktok2"
  "/home/david/Downloads/tiktok3.mp4:ipodtiktok3"
  "/home/david/Downloads/tiktok4.mp4:ipodtiktok4"
  "/home/david/Downloads/tiktok5.mp4:ipodtiktok5"
)

mkdir -p "$video_dir" "$apps_dir"

for item in "${sources[@]}"; do
  src="${item%%:*}"
  if [[ ! -f "$src" ]]; then
    echo "missing source: $src" >&2
    exit 1
  fi
done

convert_clip() {
  local src="$1"
  local out_base="$2"

  ffmpeg -y -loglevel error -i "$src" \
    -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=24" \
    -c:v mpeg2video -pix_fmt yuv420p -q:v 6 -maxrate 1500k -bufsize 1835k \
    -c:a mp2 -ar 44100 -ac 2 -b:a 128k \
    "$video_dir/$out_base.mpg"
}

for item in "${sources[@]}"; do
  src="${item%%:*}"
  out_base="${item##*:}"
  convert_clip "$src" "$out_base"
done

rm -rf "$apps_dir/ipodtiktok"

cat > "$feed_path" <<'EOF'
id	title	path
ipodtiktok1	Clip 1	/Videos/iPodTikTok/ipodtiktok1.mpg
ipodtiktok2	Clip 2	/Videos/iPodTikTok/ipodtiktok2.mpg
ipodtiktok3	Clip 3	/Videos/iPodTikTok/ipodtiktok3.mpg
ipodtiktok4	Clip 4	/Videos/iPodTikTok/ipodtiktok4.mpg
ipodtiktok5	Clip 5	/Videos/iPodTikTok/ipodtiktok5.mpg
EOF

echo "Imported iPodTikTok clips into $video_dir"

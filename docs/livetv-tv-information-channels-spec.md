# RockPod TV Information Channels

## Outcome

`Device > Sync TV Information` refreshes three renewable video channels and
then writes the complete Live TV line-up to the connected iPod:

| Channel | Guide ID | Current material | Video |
|---|---|---|---|
| NASA | `NASA` | NASA Image and Video Library items from the last two years | Official NASA MP4 renditions |
| New Brunswick News | `NBT` | Government of Canada New Brunswick Atom headlines | Current Global News New Brunswick clips when `yt-dlp` can resolve them |
| Maritime RoadWatch | `ROAD` | Nova Scotia, New Brunswick, and PEI highway cameras | Genuine Halifax bridge-camera timelapses plus current official 511 frames |

The stable source names (`nasa-current.mkv`, `news-nb-current.mkv`, and
`road-mar-current.mkv`) are replaced atomically when source identities or
content hashes change. A failed provider retains usable cached source
metadata where possible and is reported as a sync warning.

## Playback architecture

RockPod is the network-connected headend. It downloads a bounded set of
current material, renders all graphics on the host, and produces one
30-minute programme reel per channel. The normal Live TV MPEG conversion,
copy, schedule, and playback paths handle those reels.

The iPod does not fetch data, render network content, allocate a decorative
framebuffer, or take ownership of PCM/shared playback buffers. Information
channels also opt out of the generic commercial pool. With 30-minute blocks,
the three channels consume 288 two-day guide slots, keeping ample room under
the device's 2,048-slot ceiling.

## Refresh and replacement policy

1. Fetch official/current provider indexes.
2. Download at most three bounded video items per video-backed channel.
3. Capture three chronologically distinct frames from each licensed Halifax
   bridge traffic webcam and turn them into genuine short timelapses.
4. Download up to six current 511 road-camera frames, distributed across all
   three Maritime provinces.
5. Compare item identities and SHA-256 content digests with the last build.
6. Rebuild only changed programme reels; prune superseded numbered sources.
7. Add or update the three category-stable channels without overwriting user
   channel numbers or names.
8. Convert guide logos to the existing 40x18 Rockbox BMP format, transcode the
   reels through the existing 320x240 MPEG profile, and write the full guide.

## Graphics

Each channel has a unique 640x480 geometric broadcast package, guide icon,
and transparent on-air bug. The look uses crisp gradients, glass panels,
ticker bars, grid geometry, and bold sans-serif typography associated with
early/mid-2000s digital cable. There are no hand-drawn assets.

The marks are deliberately descriptive in-house channel identities:

- NASA: blue orbital disc and red trajectory
- New Brunswick News: red `NB` tile
- Maritime RoadWatch: cream/orange `MAR 511` road tile

## Configuration

The feature and each provider are enabled by default:

- `tv_information_enabled`
- `tv_information_nasa_enabled`
- `tv_information_news_nb_enabled`
- `tv_information_road_mar_enabled`
- `tv_information_block_seconds` (300–3600; default 1800)
- `tv_information_video_limit` (1–6; default 3)
- `tv_information_road_camera_limit` (1–12; default 6)
- `tv_information_road_capture_enabled`
- `tv_information_road_capture_frames` (2–6; default 3)
- `tv_information_road_capture_interval` (1–60 seconds; default 20)
- `tv_information_news_download_video`

Provider URLs can be overridden by advanced configuration keys in
`services/tv_information.py`. Downloaded news video remains subject to the
provider's terms and the user's right to make a personal offline copy.

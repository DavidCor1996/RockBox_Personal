# Native TikTok app and RockPod sync

TikTok is a standalone Rockbox application (`ipodtiktok.rock`), not part of
the Internet application. RockPod manages two feed sources:

- Manual videos added from a local file or imported from a TikTok URL remain
  until the user removes them.
- Each followed account contributes its newest 10 videos. A refresh downloads
  the current ten before removing older account-managed files. It never prunes
  manual imports.

RockPod converts non-MPEG sources with aspect-preserving scale and black pad,
so portrait TikToks remain portrait on the iPod's 320x240 screen. Thumbnails
use the same contain-with-letterbox rule. The generated feed is
`.rockbox/rocks/apps/.ipodtiktok_feed.tsv`; the home-screen thumbnail index is
`.rockbox/tiktok/library.tsv`.

Every URL import also records the creator profile separately from the video:
profile URL, username, display name, bio, avatar reference, follower/following
counts, profile likes, and post count. Followed-account refreshes update these
fields. TikTok does not expose every count for every account, so unavailable
values remain empty/zero rather than being fabricated. Profiles are exported
to `.rockbox/tiktok/profiles.tsv` and shown in RockPod's Profiles tab.

## iPod controls

- Clickwheel clockwise/counter-clockwise: next/previous video in the active
  section.
- Right/left: For You/Following.
- Select twice quickly: like or unlike the current video.
- Play/Pause: pause or resume.
- Menu: back/exit.
- Wheel volume actions continue to use Rockbox's real playback volume and show
  the native MPEGPlayer volume overlay.

The player stores the current feed item and likes beside the feed manifest, so
playback returns to the last TikTok by default.

# RockPod Reddit application

Reddit is a standalone offline application under RockPod's **Applications** source group and Rockbox **Extras → Applications**. It does not extend the Internet application.

RockPod accepts canonical subreddit URLs, beginning with `https://www.reddit.com/r/ipod/`. The maintained gallery-dl Reddit extractor reads the signed-in Firefox session, preserves Reddit listing order and metadata, and incrementally archives media. Updates retain cached posts that have left the current listing and skip media already present in the download archive.

The iPod interface adapts Reddit's 2010 link-list design: the official Snoo asset, pale-blue masthead, blue link titles, orange vote treatment, compact gray metadata, square thumbnails, and thin bordered rows. It deliberately avoids modern Reddit cards. The first screen lists subscribed communities. A community opens a two-row post list so wheel scrolling is immediately apparent. Each row shows title, author, vote score, comment count, flair/pinned state, and a real media thumbnail where available. Select opens text/details, a fitted image viewer, or mpegplayer for video. Menu goes back and exits from the community list.

Synced data lives in `.rockbox/reddit/`; `library.tsv`, `subreddits.tsv`, and `previews.tsv` are rewritten only when content changes. Converted images and videos use source signatures so later syncs do not redo completed work.

OnlyFans is treated as hidden content in RockPod. Its Applications entry is invisible until **View → View Hidden Content** succeeds with the existing PBKDF2-protected hidden-content password. Turning hidden content off removes access immediately.

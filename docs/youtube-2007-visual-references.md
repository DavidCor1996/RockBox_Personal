# Standalone YouTube app: 2007 visual references

The standalone app targets YouTube's interface during 2007, with the June
watch-page/player treatment as the centre of the visual system. It is an
offline personal library and does not represent itself as a network client.

## Period sources reviewed

- [Official YouTube Blog, January 2007](https://youtube.googleblog.com/2007/01/)
  documents channel module visibility, a selected featured video, expanded
  featured-video browsing, and the era's `Channel Design` terminology.
- [Official YouTube Australia Blog, 2007](https://youtube-au.googleblog.com/2007/)
  documents profile-picture uploads, the Videos tab, Most Active and
  Previously Popular views, and contemporary channel search terminology.
- [Google Operating System, 26 May 2007](https://googlesystem.blogspot.com/2007/05/screenshots-of-youtubes-new-player.html)
  preserves screenshots of the 2007 player, its seek control, related-video
  shelf and Menu interaction.
- [Wikimedia's sourced YouTube player history](https://commons.wikimedia.org/wiki/File:YouTube_video_player_history.png)
  preserves the actual 2006–2008 embedded-player control strip used as the
  playback overlay bitmap. The device asset is a proportional rasterization,
  not a reconstruction.
- [Peter Forret, 14 June 2007](https://blog.forret.com/2007/06/14/new-beta-youtube-layout/)
  preserves the mid-2007 watch-page arrangement and notes the move of video
  information below the player.
- [Version Museum's YouTube history](https://www.versionmuseum.com/history-of/youtube-website)
  provides side-by-side 2006, 2007 and 2008 home/watch-page references used to
  reject later visual language.
- [V&A reconstruction of an early YouTube watch page](https://www.vam.ac.uk/articles/acquiring-an-early-youtube-watch-page-and-its-first-ever-video)
  documents preservation of original front-end code and the period Flash
  player from the December 2006 page, immediately adjacent to the target year.

## Device adaptation decisions

- White page surfaces, `#0033cc` links, pale-blue module headers, gray rules,
  red active accents and the authentic outlined five-star strip form the
  common visual language.
- The header keeps the authentic 2006–2011 wordmark and uses `Home`, `Videos`
  and `Profile`; no modern Home feed, Likes, handles, Shorts, bells or Material
  icons appear.
- Home is a finite, curated `Featured Videos` carousel. Video pages use
  `From:`, `Added:`, aggregate five-star ratings and view counts.
- Profiles use user names, `About Me`, `Channel Views`, `Video Views`,
  `Subscribers`, `Friends`, Videos and per-profile colors. The iPod renderer
  consumes data and colors rather than a screenshot of a channel page. The
  wheel scrolls from the channel overview through period member fields and
  into the channel's two-row thumbnail video list.
- Aggregate ratings combine the authentic archived empty and filled YouTube
  star sprites. The filled strip is clipped to the live 0.00-5.00 rating, so
  partial stars and unrated videos remain visually distinct.
- Video detail pages expose a period `Related Videos` shelf, ranking uploads
  from the same user before other videos in the same category.
- Playback uses the existing MPEG decoder and its YouTube-specific handoff.
  Its visible control strip is the authentic archived 2006–2008 YouTube player
  bitmap at 320x28, composited pixel-for-pixel over decoded video. The moving
  seek and volume knobs are transparent crops of those exact captured control
  pixels, rather than geometric redraws, and are positioned from the live
  decoder timestamp and Rockbox volume.
  The app never takes the shared audio buffer and never mutates the music
  playlist.
- The Videos tab exposes period-appropriate `Recently Added`, `Most Viewed`,
  `Top Rated` and `Favorites` views by holding the iPod centre button. PLAY
  consistently starts the selected video, while LEFT/RIGHT moves between
  adjacent video detail pages. These are local-library views; they do not
  imply a network service.

## Application boundary

The new catalog and profile files live under `/.rockbox/youtube`, the packaged
assets under `/.rockbox/ipodjs/youtube`, and the launcher is
`/.rockbox/rocks/apps/youtube.rock`. Nothing in this design adds a destination
to, or stores library state inside, the Internet/offline-web application.

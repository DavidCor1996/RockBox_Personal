# Personal Weather Channel video sources

These source files are used only from the user's uncommitted
`~/Documents/Weather Channel` library. Public visibility does not itself grant
permission to redistribute a video; do not ship these files in a Rockbox or
RockPod package.

## Selected inserts

- **Carissa Codel MMJ / Anchor Reel** — Carissa Codel TV:
  https://www.youtube.com/watch?v=seUEyK-kuk8
  - Personal cuts: 00:12–00:28, 00:40–00:56, 00:56–01:12, and
    01:12–01:28
  - Use: distinct weather, studio-anchor, and local-news fallback inserts.
- **Carissa Codel March Anchor Read** — Carissa Codel TV:
  https://www.youtube.com/watch?v=dSl0ZbjBTu4
  - Personal cuts: 01:26–01:42 and 05:02–05:18
  - Use: local-news brief and weather/seven-day-outlook inserts.
- **Carissa Codel December Anchor Read** — Carissa Codel TV:
  https://www.youtube.com/watch?v=avardwd2whA
  - Personal cut: 00:10–00:26
  - Use: general studio weather handoff.
- **Carissa Codel February Snow Coverage** — Carissa Codel TV:
  https://www.youtube.com/watch?v=tHMQZMymESE
  - Personal cut: 00:12–00:28
  - Destination: `interstitials/snow/carissa-february-snow-coverage.mp4`
  - Use: only when the current hourly condition is snow, sleet, ice, or
    freezing precipitation.
- **October Anchor Read** — Carissa Codel TV:
  https://www.youtube.com/watch?v=-ZhejOhtDag
  - Personal cut: 00:03–00:19
  - Destination: `interstitials/general/carissa-weather-lab.mp4`
  - Use: general studio/weather-lab interstitial.
- **August 2025 MMJ Reel** — Carissa Codel TV:
  https://www.youtube.com/watch?v=heUPFqqM_Qo
  - Personal cut: 00:00–00:16
  - Destination: `interstitials/general/weather-mmj-opening.mp4`
  - Use: alternate general weather-story opening.

- **Carissa Codel June 11 & 13 Anchoring** — Carissa Codel TV:
  https://www.youtube.com/watch?v=xfqIg8yQYs4
  - Personal cut: uninterrupted 00:05.68–00:24.48 anchor-to-weather handoff.
  - Destination: `interstitials/summer/carissa-june-anchor-weather-read.mp4`
  - Use: summer-only anchor read, June through August, unless snow/ice
    routing takes priority. At 18.85 s it is below the long-report floor, so
    it does not replace the live forecast rotation.
- **Carissa Codel Anchor and MMJ Reel July 2025** — Carissa Codel TV:
  https://www.youtube.com/watch?v=nLzJYz8D9II
  - Personal cut: 00:54–01:10
  - Destination: `interstitials/summer/carissa-july-weekend-rain-anchor.mp4`
  - Use: summer-only "Chance for Weekend Rain" anchor toss and temperature
    map.
- **News Anchor Reads Mean Comments About Her Weight** — Inside Edition
  (presenter: Carissa Codel):
  https://www.youtube.com/watch?v=vlqcfi8_67c
  - Personal cut: uninterrupted 00:58.239–01:47.70 interview sequence,
    beginning with Carissa already on camera and ending after her complete
    "nothing can hurt you" answer. No cross-source splice or replacement
    audio is used.
  - Destination: `interstitials/comments/carissa-reads-viewer-comments.mp4`
  - Use: one `VIEWER COMMENTS` break near the middle of every hour,
    independent of the forecast.

## Newly identified Carissa Codel sources (not yet cut)

Enumerated from the channel itself with
`yt-dlp --flat-playlist "https://www.youtube.com/@CarissaCodelTV/videos"`,
so every id, title and runtime below is the real value reported by YouTube.
Per-video pages could not be opened (`HTTP 429` / bot check on every player
client), so **no cut points are recorded — they have to be chosen by
watching.** Nothing below is a guess dressed up as a timecode.

### Why long cuts are the thing worth taking

Every Carissa clip currently in the library is 16.02 s long (the June
anchoring cut is 18.85 s), measured with `ffprobe`.

In `rockpod/services/livetv.py`, `select_weather_interstitials()` returns one
combined pool — the season-appropriate `general`/`summer`/`snow` Carissa
clips plus any condition-matched `conditional` clips. The caller then applies
the floor to *that whole pool*: anything below
`LIVETV_WEATHER_MIN_NATURAL_REPORT_SECONDS` (45) goes into `short`, is logged
as "Weather rotation skipped N unnaturally short clip(s)", and is dropped by
`inserts = natural`. Anything above `LIVETV_WEATHER_MAX_REPORT_SECONDS` (112)
is dropped the same way.

The report clocks are not empty — 13 of the 18 `conditional/` clips clear the
floor and do air, gated by their condition rules. But **every clip in
Carissa's own `general`/`summer`/`snow` pools is 16.02 s**, so all 13 of them
are filtered out before air. The nominal house presenter was the one
presenter who never reached a forecast-report clock; the other women carried
every one of them.

So the useful cut from a Carissa source is a single continuous **45–112 s**
segment that ends on its own sentence boundary. More 16-second snippets
change nothing.

- **Carissa Codel MMJ / Reporter Reel January 2025** — Carissa Codel TV:
  https://www.youtube.com/watch?v=rcJ13gnnEAU (4:39)
  - Personal cut: **done** — see "Cut and added to the
    rotation" above (`news/carissa-trash-pileup-winter-delay.mp4`). No
    45–112 s weather report in this reel; its winter content is a standup
    of about ten seconds.
- **Carissa Codel June Anchor Read** — Carissa Codel TV:
  https://www.youtube.com/watch?v=HX4TJ4uwuCg (4:09)
  - Personal cut: **done** — see "Cut and added to the
    rotation" above (`news/carissa-highway-fire-and-crash.mp4`). The weather
    content here is a ~10 s handoff to meteorologist Natalie Nunn.
- **Carissa Codel Reporter/Anchor Reel 2024** — Carissa Codel TV:
  https://www.youtube.com/watch?v=py9ImltFEKQ (11:08)
  - Personal cut: not yet selected — scanned, no weather-dense passage
    over 45 s (its only snow block is a 25 s Rockies-fan kicker).
  - Proposed destination: `interstitials/general/`
- **2023 MMJ REEL** — Carissa Codel TV:
  https://www.youtube.com/watch?v=3PA_-ax4hIk (10:07)
  - Personal cut: not yet selected — scanned, no weather-dense passage.
  - Proposed destination: `interstitials/general/`
- **Carissa Codel MMJ/Anchor Reel** — Carissa Codel TV:
  https://www.youtube.com/watch?v=wSE_7CJPPTE (8:40)
  - Personal cut: not yet selected — scanned, no weather-dense passage.
  - Proposed destination: `interstitials/general/`
- **Carissa Codel October 2024 Reel** — Carissa Codel TV:
  https://www.youtube.com/watch?v=YqwLxXRNyTI (3:52)
  - Personal cut: **done** — see "Cut and added to the
    rotation" above (`conditional/carissa-tornado-shelter-lockout.mp4`).
- **Carissa Codel A Block** — Carissa Codel TV:
  https://www.youtube.com/watch?v=J3t5iz-Glsc (4:13)
  - Personal cut: not yet selected — scanned, no weather-dense passage.
  - Proposed destination: `interstitials/general/`
  - Use: a full A-block open is the most "top of the hour" material on the
    channel.
- **Carissa Codel Mock Anchor Read** — Carissa Codel TV:
  https://www.youtube.com/watch?v=c8l_zdFpk6M (3:28)
  - Personal cut: **done** — see "Cut and added to the
    rotation" above (`news/carissa-labor-day-parade.mp4`).

### One existing pair is byte-identical

`interstitials/general/weather-mmj-opening.mp4` and
`interstitials/summer/carissa-august-rain-report.mp4` are the same file
(md5 `85fc8d5d…`), both cut 00:00–00:16 from `heUPFqqM_Qo`. The library
record describes the summer copy as giving all three carrier variants
distinct inserts, but `_weather_unique_media()` in
`rockpod/services/livetv.py` fingerprints the general+summer pool with
sha256 and drops exact duplicates, so in June–August the pool is one clip
thinner than the file count suggests. A second, different 45–112 s cut from
that same 5:41 source would fix it.

### Cut and added to the rotation

Downloaded with `yt-dlp` and cut against the auto-caption timeline, so each
cut starts and ends on a spoken sentence boundary. All four are encoded to
the library's existing format: 640x360 H.264 High / yuv420p / 30000:1001,
AAC-LC 44.1 kHz stereo.

- **Ozark storm-shelter lockout** — from *Carissa Codel October 2024 Reel*,
  https://www.youtube.com/watch?v=YqwLxXRNyTI
  - Personal cut: 00:47.5–02:32.0 (104.50 s), opening on Carissa's live
    standup at the tornado-shelter sign and ending on the closing soundbite
    from the 85-year-old resident.
  - Destination: `interstitials/conditional/carissa-tornado-shelter-lockout.mp4`
  - Rule: `thunder` condition, `min_precipitation: 50`,
    `weather_overlay: true`, `overlay_style: sidebar`, `priority: 140`.
    This follows the spec's own instruction that weather-news such as
    tornado coverage lives in the conditional manifest rather than the
    general news folder, and matches the existing Margaret Orr and Mary Mays
    tornado entries.
  - **This is the first Carissa clip that clears the 45 s report floor**, so
    it is the first time the house presenter reaches a forecast-report
    clock. Verified to activate on a thunderstorm forecast and to stay out
    of clear and snow rotations.
- **Nixa trash pile-up after the winter storm** — from *MMJ / Reporter Reel
  January 2025*, https://www.youtube.com/watch?v=rcJ13gnnEAU
  - Personal cut: 00:43.2–02:10.0 (86.82 s), a complete package from the
    viewer-tip open to the "we want to keep it that way" soundbite.
  - Destination: `interstitials/news/carissa-trash-pileup-winter-delay.mp4`
    (`overlay_style: sidebar`)
- **Trailer fire and I-44 crash** — from *Carissa Codel June Anchor Read*,
  https://www.youtube.com/watch?v=HX4TJ4uwuCg
  - Personal cut: 02:27.8–03:09.5 (41.71 s), the "new at midday" block
    through "the driver was not injured in the crash". No injuries in
    either story.
  - Destination: `interstitials/news/carissa-highway-fire-and-crash.mp4`
    (`overlay_style: lower-third`)
- **Labor Day parade and Branson holiday weekend** — from *Carissa Codel
  Mock Anchor Read*, https://www.youtube.com/watch?v=c8l_zdFpk6M
  - Personal cut: 01:25.3–02:00.3 (35.03 s), the parade and Table Rock
    tourism stories.
  - Destination: `interstitials/news/carissa-labor-day-parade.mp4`
    (`overlay_style: sidebar`)

### Why the general/summer/snow pools got nothing

About 46 minutes of the unused reels were pulled and read against their
caption timelines. Carissa is an MMJ and anchor, not the meteorologist: her
weather material is consistently a ~10-second standup ("this weekend it's
going to be brutally cold ... I just checked my color 10 weather app, it
says it is 16 degrees") or an anchor-to-meteorologist handoff ("and now for
your first look at weather ... meteorologist Natalie Nunn joins us now").
That is exactly why every existing clip in those folders is 16 seconds — it
is not a shortcut, it is the length of the source material.

Her 45–112 s continuous blocks are news packages, so that is where they were
put. Only one had genuine weather-emergency content, and it went to
`conditional/` under the thunder gate. Nothing was forced into a
forecast-report folder to pad the count: a trash-collection package filed as
a snow forecast would be worse than an empty pool.

Remaining long reels (`py9ImltFEKQ`, `3PA_-ax4hIk`, `wSE_7CJPPTE`,
`J3t5iz-Glsc`) were scanned for weather-dense passages and contain none
beyond short kickers — for example the only "snow" block in the 11-minute
2024 reel is a 25-second story about a Rockies fan catching snowflakes.

### Carissa-fronted news-break candidates

These are complete standalone packages rather than reels, and all fall
inside the 24–300 s news window
(`LIVETV_WEATHER_MIN_NEWS_BREAK_SECONDS` / `..._MAX_NEWS_BREAK_SECONDS`),
so several may need no recut at all. These four match the consumer and
human-interest tone already in `interstitials/news`.

- **Comparing Springfield women's monthly salaries to childbirth medical
  bills** — https://www.youtube.com/watch?v=sLftw8wjYQA (3:13)
- **Medical marijuana comes to Springfield** —
  https://www.youtube.com/watch?v=nIdInU-JjUI (1:41)
- **Inflation Impacting Local Businesses** —
  https://www.youtube.com/watch?v=CtFkwSPcwHY (1:22)
- **Substance abuse increasing during 2020 pandemic** —
  https://www.youtube.com/watch?v=R5Fu362xCbQ (1:41)
  - Proposed destination for all four: `interstitials/news/`

### Left for the owner to decide

- `https://www.youtube.com/watch?v=U5KWPUngcS0` (6:06, civilian "vigilante"
  group) and `https://www.youtube.com/watch?v=1iQySM6Iwk8` (5:08, *My
  Quarantine Life*) — both exceed the 300 s news ceiling whole and would
  need a recut.
- Both Gypsy Rose Blanchard packages —
  `https://www.youtube.com/watch?v=COwswlbI-ZI` (2:55, a Springfield woman
  who was incarcerated with her) and
  `https://www.youtube.com/watch?v=5RXOSeCVdDs` (2:44, her early release).
  In range and squarely Carissa's reporting, but it is matricide-case
  coverage, not the human-interest register of the current news pool.
- `https://www.youtube.com/watch?v=-0u8PgL6n98` (elderly couple attacked
  during a home invasion), `https://www.youtube.com/watch?v=cD4AyFe99Gk`
  (mother protests after son killed by DEA), and
  `https://www.youtube.com/watch?v=SBHG73jRUhw` (backlog of sexual assault
  kits) — all in range and all genuinely Carissa's reporting, but far
  heavier than the stolen-dog-reunion / charity-dance / airplane-food tone
  of the current news pool. Not added on my own judgement.

## Reviewed but excluded

- `https://www.youtube.com/watch?v=93P32CObu9U` — excluded because the
  uploader's description expressly says not to use the content without owner
  permission.
- `https://www.youtube.com/watch?v=n4ejXq35r1E` — mostly unrelated reporting.
- `https://www.youtube.com/watch?v=e_xlmA0XAlE` — blooper reel did not fit the
  continuous weather-channel tone.
- `https://www.youtube.com/watch?v=0rFq6VRIq5A` — "Ducks Community", from
  the same set as the already-excluded "Ducks Weather"
  (`93P32CObu9U`), whose uploader description forbids reuse without
  permission. Excluded by association unless that is checked and cleared.
- `https://www.youtube.com/watch?v=Ow6jLJM18Sw` — "Ducks BRoll prproj":
  raw b-roll, not a broadcast segment.
- `https://www.youtube.com/watch?v=GmUmHUEA820` — "pug montage": not a
  broadcast segment.
- `https://www.youtube.com/watch?v=DMd815uAzc0` — "BSS State Champs": not a
  broadcast segment.

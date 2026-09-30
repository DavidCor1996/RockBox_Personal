# RockPod MSN Messenger

MSN Messenger is an offline native RockPod application with original Microsoft
MSN Messenger 7.5 artwork, sounds, sign-in animation, emoticons and Winks. Its
320x240 layout follows iPod navigation rather than emulating a desktop mouse.
The Applications icon uses the same 46-pixel rounded tile geometry as other
apps. Its mark and glass surface are original MSN resources, with white-matted
alpha edges to prevent purple fringes.

## RockPod editor

Applications → MSN Messenger opens the dedicated companion screen. Contacts
supports names, personal messages, profile pictures, optional email, CSV import
and Instagram import. Monthly conversations supports Add conversation, Add
photo, ordered message/media rows, earliest/latest day, delivery hours, and
natural reply choices with a separate follow-up for each choice. Double-click
edits the selected contact or conversation; blue selection covers the whole row.

The media chooser can use existing local Instagram and OnlyFans library
entries. It reads their existing library metadata; it does not start a login,
fetch another profile, or replace either sync tool. Video attachments call the
same `stage_app_video` conversion service used by OnlyFans. Original Flash
Winks need a separate asset-preparation step because they are vector timelines.

The prepared private plan contains three contacts, 66 conversations, 234
scheduled events, and 174 contextual reply choices across 30 days. Its dialogue
and girlfriend relationship are fictional, romantic and non-explicit. Public
profile media is not evidence of a real private conversation. Personal data,
media and dialogue stay in the user's RockPod cache, outside committed source.
Nothing is transmitted to the source accounts.

## Delivery and history

The default prepared campaign starts on the first sync to each device, rather
than a fixed calendar date. `campaigns.json` retains that device's anchor.
Calendar-month mode remains available. Deterministic campaign/conversation IDs
retain delivery times and prevent resync from replaying messages. Ordered rows
arrive with typing pauses. Contextual reply follow-ups have stable once-only IDs
shared by every row of their conversation. Free-text replies use finite authored
keyword/fallback banks; exhausted responses are not repeated.

Delivery uses the iPod's local clock, with supported years 2020–2037. The player
must be awake for notifications. Reopening after sleep reveals due messages
and emits at most one catch-up notification. Core checks run in normal service
context, never in an interrupt or paint callback.

An active schedule supports 64 contacts and 1024 events. The resident working
set is bounded; adding replies evicts only already-delivered resident entries
when full. Disk history remains intact. `archive.tsv` retains scheduled messages;
`local.tsv` retains outgoing messages and authored responses; previous reply
journals remain readable. History pages load 64 entries at a time. Read receipts,
used responses, selected contact and played-Wink IDs survive resync.

## iPod controls

- Wheel: select contacts/messages, tools or reply choices.
- Select: open the highlighted contact/message/tool. In a photo or GIF viewer,
  Select saves to Photos.
- Hold Select in a conversation: choose a contextual reply or Write your own.
- Play: move focus between conversation and toolbar.
- Left/Right in a conversation: focus and move through toolbar tools.
- Menu: leave the current pane, return to Contacts, then exit.

The toolbar contains Reply, Emoticons, Nudge, Winks, Animated emoticons, History,
and Sounds. Its selected tool has a double blue outline and an explicit Select
label. Contact and reply selections use blue with white text. Message selection
has a blue outline. Original MSN icons remain visible inside selection frames.
Right in the emoticon picker switches to Unicode emoji; History uses Left for
older messages and Right for the latest page.

## Photos, GIFs and video

Sync prepares small display images, 128x80 inline chat previews and separate
full-resolution, EXIF-oriented baseline JPEGs. GIF saves retain the original
animation file. Saved copies go under `/Photos/MSN Messenger`, with the existing
Photos app's thumbnail/preview sidecars. Repeated saves are idempotent and later
Messenger syncs do not remove saved copies. Copies use a fixed 4096-byte transfer
buffer, temporary sibling file and rename. Full videos use the existing player
and return to the same Messenger conversation through its plugin handoff.

## Original resources and visual reference

The Microsoft 7.5 installer was extracted without execution. Source checksums
and its archive URL are in `assets/ipodjs/sources/msn/PROVENANCE.json`.
`tools/prepare_msn_assets.py` derives bounded bitmaps from those resources.
The runtime tree has recursive `SHA256SUMS`. Microsoft retains its artwork
rights; provenance is not a redistribution license.

The layout and toolbar were compared with first-hand 2005 MSN 7.5 screenshots:

- [Original contact window](https://www.shahabjafri.com/Images/MainScreen.jpg)
- [Original conversation window](https://www.shahabjafri.com/Images/PmBox.jpg)
- [Screenshot author's page](https://www.shahabjafri.com/Pages/More-screenshots-MSN.html)

The reference comparison corrected the Nudge icon to the original vibrating
face. Speaker and clock resources replace unrelated status/mail symbols for
Sounds and History. The blue chrome, inline "says" transcript, display picture,
emoticon toolbar and typing line retain MSN's visual vocabulary. The iPod uses
its own wheel and button focus instead of desktop title-bar controls.

The [archived emoticon collection](https://github.com/bernzrdo/msn-emoticons)
provides 81 original entries, including 11 animations and five unused assets.
The resident atlas is 304x171 (103,968 pixel bytes), animated at at most ten
updates per second. Unicode uses the existing 4,009-glyph firmware emoji atlas,
including flower emoji, variation selectors and joined sequences.

Twenty original Flash Winks are preserved from the
[default-Wink archive](https://wink.messengergeek.com/t/how-to-extract-the-default-msn-winks/14337)
and the archive linked in `sources/msn/winks/PROVENANCE.json`. JPEXS 26.3.0
renders the original timelines, including nested animation and original sound
cues. No substitute artwork is drawn. `prepare_msn_winks.py` produces transparent
240x160 RGB565 run-length streams capped at 12 frames per second and mono PCM.
Incoming Winks autoplay once when their chat is opened; a journal preserves that
state. Select can replay them, and Menu cancels. This is the recovered stock
collection, not every third-party Wink or custom GIF ever published.

## Storage, memory and audio

`/.rockbox/msn/current.txt` selects a content-addressed immutable bundle. Sync
publishes that pointer after preparing contacts, media and assets. Old bundles
remain available to history. Device-root and destination symlinks are rejected.

The native plugin uses 1,888,188 bytes fixed BSS and 35,888 bytes code within the
iPod plugin arena. The sign-in atlas, photo display and streamed animation share
one union. One 128x80 image cache, including BMP scaler scratch, uses 29,200 bytes.
Only one missing preview is loaded after input settles for 125 ms; draws only paint cached pixels.
No UI code loads a font cache, takes playback's buffer, changes a playlist,
stops playback or requests core allocation. Native core memory and plugin
sizes are recorded separately because the working tree contains other changes.

Short original effects use the BEEP mixer channel. Wink sound storage is capped
at 400,000 bytes with a 2048-byte resampling buffer. Callbacks read only resident
PCM; owned callbacks/channels stop before plugin exit or video handoff. Full
video uses the existing playback player. These paths still need the physical
playback/volume transition matrix from plugin audio lifecycle guidance.

## Build and validation status, 2026-09-26

Before the user's request to perform final testing, 16 companion tests passed,
covering scheduling, resync/history, Unicode, choices, asset streams and sync
safety. Focused simulator checks exercised original Wink autoplay once, photo
context replies, outgoing Winks, saved photos/GIFs, video return, and repeated
app entry/exit with seven process descriptors each time. Earlier navigation
checks passed ten hierarchy cycles and twenty rapid switches with stable
playback and resources.

Later broad navigation attempts did not fully pass: one failed to enter Music;
another failed the transition trace gate for an incomplete back transition.
Those results are unresolved, not a successful final regression. No additional
runtime tests were started after the user took over testing. Latest selection,
third-contact and editor changes received build/syntax checks only. The latest
6G firmware/simulator and 5G plugin builds compile; device validation is pending.
No firmware, app runtime or personal data was deployed to a physical iPod.

## Inline image update

Photo and GIF messages show a larger image card directly in the conversation.
The image uses a bounded 128x80 area beside its caption, with Select Enlarge
(or Play / Save for GIFs). The wheel moves through image and text messages;
one image card fits the transcript pane. Idle loading reads the existing
preview sidecars, with a first-frame fallback for sent animated emoticons.
Photo enlargement now fits the image proportionally into 304x184, without the
old padded display image reducing portrait size. Select in the enlarged viewer
saves the original photo/GIF to Photos. This update needs no conversation resync.

The 6G, 5G and 6G simulator plugins compile. The updated plugin was installed
on the previously deployed 6G, with its firmware, database and Messenger state
checked unchanged. Runtime testing remains with the user.

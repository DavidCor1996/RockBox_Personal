# Playback, App Controls, Live TV, And Weather Notifications

## Scope

This revision extends the fixed-memory iPodJS notification manager without
taking playback memory or adding a full-screen framebuffer. Notifications
remain local to the iPod and continue to render over menus and plugins through
the LCD overlay-row hook.

From the iPodJS Home screen, one short Left press opens Notification Center.
Another short Left press closes it. The panel slides down from the top using
the stock transition compositor's eight-sample smoothstep cadence; dismissing
it slides the panel back upward. Left-repeat actions do not launch the panel,
preventing a hold from producing an open-then-close flash. A short Menu press
on Home never launches Notification Center.
After dismissal, Home consumes the release and rearms only after observing a
new physical Left press, so one click cannot close and immediately reopen it.

While Notification Center is open, another short Menu press returns to Home
with the upward slide. Holding Select presents the `Clear all notifications?`
confirmation; short Select continues to open the highlighted notification.
Because opening uses Left-hold, Menu dismissal needs no launch-input guard and
never requires a simultaneous Select press.

## Settings

`Settings > Theme Settings > Notification Settings` contains master switches
for Notifications, Banners, and Sound. Its new `Apps` screen independently
controls Achievements, Music & Playback, Sitekick, Live TV, Weather, Battery,
and Storage.

`Test Notification` immediately presents a non-persistent preview banner even
when banners are disabled. The banner reuses the fixed 320x42 overlay
workspace: a captured background strip, inset rounded card, translucent edge,
and soft shadow provide the floating treatment without another framebuffer or
playback-memory allocation.

Disabling an app prevents new history entries, banners, and schedules from
that source. Sitekick remains opt-in by default; the other requested sources
default on.

## Music And Playback

The normal UI service loop observes `audio_status()` and the current cached
track metadata. It posts:

- Now Playing when the current track changes, including artist and album when
  available;
- Playback Paused and Playback Resumed on state transitions;
- Queue Finished when the final track reaches its end;
- Playback Stopped for other transitions from playing to stopped.

No playback callback performs file I/O, allocates memory, draws, or changes
audio state. The observer uses a fixed `MAX_PATH` cache and reads metadata
already owned by playback.

## Sitekick

The existing Sitekick producer remains the source of truth. It schedules and
posts independent events for Dump readiness, Shop restocks, newly acquired
chips, and inbox rewards. All are controlled by the Sitekick app toggle.

## Live TV Reminders

Pressing Select on a future programme opens a DIRECTV-styled `Upcoming
Program` sheet over the guide. It shows the programme, start time, channel,
and these actions:

- Set Reminder;
- Cancel Reminder;
- Back to Guide.

A reminder is keyed by channel, weekday, programme start, and title. Setting
the same reminder again updates its scheduler entry. It fires five minutes
before airtime, or immediately when set inside that window. The notification
links back to DIRECTV. The existing fixed scheduler limit remains eight.

## Weather Freshness

After a short boot grace period, then at most once every 30 minutes, the
manager checks `/.rockbox/rockpod/weather/forecast.tsv` metadata:

- missing data asks the user to sync Weather with RockPod;
- data at least 72 hours old is marked potentially outdated;
- data older than six days is marked expired.

Warnings are edge-triggered by freshness level and forecast modification
time, so ordinary navigation does not repeatedly post the same warning.

## Battery And Storage Warnings

Battery checks begin 20 seconds after boot and run at most once per minute.
While unplugged, Low Battery posts at 20 percent and Critical Battery posts at
10 percent. The warning state resets only after charging begins or the level
rises above 25 percent, preventing threshold jitter from generating banners.

Storage checks begin 30 seconds after boot and run at most once every ten
minutes. Storage Running Low posts below 2 GiB or three percent free;
Storage Almost Full posts below 512 MiB or one percent free. Each alert shows
the remaining free space. Healthy-space recovery resets the episode, while a
daily stable identifier prevents repeated alerts from ordinary navigation or
reboots. Checks use cached filesystem volume accounting from the idle
notification service and never run in menu, center, or banner draw code.

## Memory And Lifecycle Budget

The change adds no core allocation, audio-buffer claim, decoder restart,
playlist mutation, image decode in a draw callback, or new framebuffer. New
persistent data uses the existing notification history and eight-entry
scheduler. Runtime state is limited to one track path and small scalar fields.
Battery and storage monitoring add no buffers or scheduler records.

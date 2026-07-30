# Kokkia / AirPods Integration Specification

## Scope

This feature set applies to iPod 6G serial iAP accessories that exhibit the
Kokkia StartIDPS activation sequence.  It must not change USB iAP, ordinary
serial remotes, PCM/mixer ownership, the active playlist, or playback-buffer
memory.

## Observable Connection States

Rockbox cannot read Kokkia's private Bluetooth pairing database or a separate
AirPods radio-link bit.  The strongest state available over the dock protocol
is therefore:

1. dock contact present and UART open;
2. valid Kokkia StartIDPS signature observed;
3. MFi authentication complete; and
4. post-authentication Kokkia activation replies sent.

The Bluetooth status icon represents Kokkia dongle presence, not headphone
connection.  It appears once the open serial dock has emitted the
Kokkia-style StartIDPS signature and remains visible through authentication,
retry, and peer discovery.  Generic dock presence alone is insufficient, so
ordinary non-Kokkia accessories do not receive the icon.

Only state 4 is `READY` and may translate Kokkia headset controls.  Kokkia
does not expose a documented Bluetooth radio-link bit over iAP.  The best
observable peer-success edge is its first transient Simple Remote play-state
status pulse after dock detection—the same non-user pulse that previously
opened Cover Flow.  Rockbox records that edge directly in the lingo handler
before startup quarantine discards it as input, then uses it to trigger the
connection animation.  Command-button bytes remain ordinary headset input and
do not manufacture connection notifications.  The icon can already be visible
for an unpaired or searching Kokkia.

At boot, the dock UART remains closed until the iAP queue and fixed framing
buffers are ready.  This prevents an already-inserted Kokkia from exhausting
its bounded identification attempts during the earlier audio/settings startup
window.  Once authenticated activation completes, `READY` is latched until a
real dock removal, accessory restart, manual restart, or watchdog recovery;
transient internal authentication bookkeeping cannot flicker the icon.

The public state machine is:

- `DISCONNECTED`: dock UART closed;
- `DETECTING`: UART open, waiting for a Kokkia signature;
- `AUTHENTICATING`: a Kokkia signature was seen and authentication is moving;
- `RETRYING`: recovery restarted a stalled Kokkia session;
- `READY`: authenticated and activated.

## Recovery

- Physical removal remains a hard iAP session boundary.
- Dock absence must remain continuous for 250 ms before it is accepted.
- A fully activated session is never reset merely because Bluetooth audio is
  quiet.  READY-state recovery requires at least four explicit UART or iAP
  checksum failures clustered inside a two-second window.
- Isolated UART/checksum errors age out without disturbing playback.  A
  clustered-error recovery records `link errors` as its reason and uses the
  same bounded retry path as startup recovery.
- Authentication progress clears the retry backoff and grants a fresh
  15-second stage deadline.
- Recovery uses bounded 15, 30, then 60-second backoff and continues at the
  60-second ceiling.
- Failure reasons distinguish no serial data, autobaud failure,
  authentication timeout, activation timeout, accessory restart, and manual
  restart, plus clustered READY-state link errors.
- Manual restart is exposed from Quick Settings and never reboots Rockbox.

## Serial And Volume Stability

- Auto-bitrate diagnostics retain the detected numeric rate instead of only
  reporting that a lock exists.
- UART overrun, parity, framing, and break errors are counted at the target
  driver.  Invalid iAP checksums are counted at the framing layer.
- The dock-contact debounce remains 250 ms.  Every observed absence edge and
  the longest continuous absence are recorded so hardware evidence can guide
  any future threshold change.
- Display Remote volume notifications are sent only when the normalized iAP
  volume byte changes.  The current volume is cached when notifications are
  enabled and after direct volume replies, preventing an unchanged packet
  from being transmitted every 100 ms.
- Recovery traces include UART error breakdowns, checksum failures, contact
  dropout telemetry, and battery voltage/level at the time of capture.

## Remote Controls

- Startup quarantine is enforced at the final remote-button delivery
  boundary, covering every iAP lingo.
- During a Kokkia session, the headset center-button bit that arrives as
  Simple Remote `Select` maps to the iPod's physical `BUTTON_PLAY` semantics.
  This is intentional: the generic iPod remote keymap treats a raw
  `BUTTON_RC_PLAY` press as Standard Select before its release.
- The remap is delivered only while playback exists (playing or paused), so a
  headset click cannot select a Home item or launch Cover Flow while stopped.
- Physical click-wheel Select remains unchanged.

## User Interface

Quick Settings contains a Kokkia row.  Its detail screen shows state, retry
count and reason, detected serial bitrate, received byte count, autobaud
relaunches, UART errors, and checksum failures.
It provides:

- `Reconnect Now`;
- `Pairing Help`;
- `Pause on Unplug`, disabled by default; and
- `Back`.

All iPodJS screens that use the shared status renderer invalidate themselves
when Kokkia presence changes.  The skin-owned stock Music WPS composes the
same authentic Bluetooth glyph beside its battery area immediately before the
skin's single framebuffer flush.  Presence changes request a full skin update,
so insertion and removal are reflected without a post-flush overlay racing
periodic header redraws.  Draw paths read cached state and pixels only.

## Connection Animation

The first Kokkia peer-success pulse in each dock session raises a connection
edge.  If that edge occurs while the Home dashboard is active, it immediately
plays the AirPods animation.  WPS and other screens consume the edge silently,
and the event expires after two seconds, so an animation cannot appear late
merely because the user later navigated Home.  A later genuine
disconnect/restart followed by another successful connection may animate
again when Home is active.

The full-screen sequence follows the modern iPhone setup-sheet presentation:
a dimmed live Home snapshot, a white sheet rising from the bottom, the AirPods
product render, product name, connection progress, and a completed Connected
state.  Each frame restores the cached Home pixels before drawing the moving
sheet, so its entrance and dismissal reveal Home without black fill or trails.
The original Home snapshot is restored at the normal end of the animation.
It is a display-only notification and has no Connect button because Kokkia
has already completed its observable activation before the sequence begins.
The dismissal uses a 700 ms elapsed-time smoothstep curve at the bounded
20-frame-per-second cadence.  Position derives from `current_tick`, so a late
frame advances motion instead of stretching or restarting the animation.

The AirPods product render is derived from Apple's public support image:

`https://cdsassets.apple.com/live/7WUAS350/images/airpods/iphone-16-pro-ios-26-airpods-pro-connect.png`

The packaged derivative is a fixed 124x109 RGB bitmap.  It is loaded at the
existing iPodJS screen-entry service point into a fixed native-color buffer
(approximately 27 KiB).  Animation frames draw that cached bitmap and simple
geometry; they perform no file I/O, allocation, tagcache work, PCM work, or
new full-screen allocation.  Home snapshot and dimming reuse the existing
target-scoped transition pair whose lifetime cannot overlap this synchronous
notification, so the feature adds zero framebuffer bytes.

## Diagnostics And Storage

The 64-entry packet trace stays in RAM during normal operation.  It is written
only when recovery records a failure or when the Debug IAP screen explicitly
requests a dump.  Ordinary remote traffic must not rewrite the trace file.

## Verification

- Native iPod 6G and simulator builds pass.
- The focused Kokkia integration gate covers the ready predicate,
  first-ready event consumption, retry-progress reset, failure-reason
  coverage, change-only volume reporting, clustered link-error recovery,
  serial/contact diagnostics, pre-quarantine peer-pulse capture, final WPS
  composition, and the packaged bitmap contract.
- Input invariants prove Kokkia Select becomes physical iPod Play/Pause and
  is ignored while stopped, while non-Kokkia Select is unchanged.
- iPodJS navigation regression runs with playback active.
- `rockbox.elf` text/data/BSS sizes are recorded before and after the fixed
  AirPods cache.
- Physical deploy preserves the tagcache database and installs identical
  firmware at both required boot paths.

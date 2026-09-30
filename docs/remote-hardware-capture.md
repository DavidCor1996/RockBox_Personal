# Dock remote hardware capture

The revision-4 firmware was deployed with matching firmware/runtime files,
but the user reports that dock Up/Down still changes volume instead of moving
the selection. Host tests do not resolve this hardware report.

Revision 5 adds observation only; it does not claim to fix that report.
The existing opt-in RAM ring now includes lingo-3 SetiPodStateInfo and
lingo-4 PlayControl commands, which can bypass button mapping, plus the
navigation/TV/Kokkia/quarantine policy and volume at each recorded event.
Exports identify the running build. Short and transaction-bearing packets
are bounded. No per-packet storage writes or allocations are introduced.
The two existing 64-entry rings grow by 1,024 bytes on 32-bit ARM.

## Capture on the actual dock

1. Keep the Classic in the TV dock. Using the iPod controls, open Settings >
   Display > Composite Video > IAP Remote Debug.
2. Press the iPod's Right button to clear the trace. Press remote Up twice,
   Down twice, Left, Right, Select, Menu and Play, leaving a short pause
   between each. Note whether the iPod volume display changes or only the
   external audio level changes.
3. Press the iPod's Menu button to save and leave. This first capture shows
   what the dock forwards while the local diagnostic screen owns input.
4. For a contextual capture, reopen the diagnostic, clear it, leave, return
   to Home and reproduce one failed Up/Down press. Reopen the diagnostic
   using the click wheel and press the iPod's Select button to export.
5. Connect the Classic by USB. Read `/.rockbox/iap-remote-trace.txt` before
   performing another capture. The trace remains enabled in RAM after exit;
   reboot disables it. Left in the diagnostic also toggles recording.

The diagnostic itself consumes raw buttons; it is not a scrolling test.
The separate Home capture includes the actual UI action mapping.

`C` means a direct protocol command, `P/R/H` a parsed button press/release/
hold, `A` a UI action, and `W/F/B` the button filtering path. Policy bits:
1 navigation, 2 TV navigation, 4 Kokkia candidate present, 8 startup quarantine.
Volume is the current Rockbox volume, not the dock's analog output level.

A missing remote event must not be converted into an assumed software key.
Likewise a volume synchronization command is not automatically an arrow.
Apple's Universal Dock guide documents playback/volume controls and says
playlist selection uses the iPod controls. This is not proof that a particular
dock never forwards usable packets; the capture is the deciding evidence.
Source: https://manuals.plus/m/2d1122d3a1d783f78762b9046be9c18762fb35db09a27090c253ddf5546de128.pdf

The broader StartIDPS-to-Kokkia candidate classification is an audit finding,
not a confirmed cause of this user's remote symptom. Do not change accessory
handshake or absolute-volume semantics without the actual exchange.

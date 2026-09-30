# Watched badges, guide Back gesture, and bounded guide text

The TV Netflix shelf now receives the watched flags for all three visible entries and draws the existing cached 16px Netflix checkmark inside the actual fitted poster corner. State persistence and handheld badges remain unchanged; no new bitmap storage or drawing-time I/O is added. The core-only shelf signature changes, not the plugin API.

The Live TV guide treated held remote Left as a Menu press, but the remote adapter correctly consumed its eventual release. The guide consequently waited forever for a Menu release that could not arrive. Its main loop now recognizes that complete held-Left gesture and returns LIVETV_GUIDE_EXIT, which the player maps to VIDEO_STOP. Local Menu press/release, remote Menu, short Left, Select, parental unlock, and modal Back behavior remain intact.

TV guide metadata no longer inherits overlapping handheld-font bounds: clock, programme time window, and rating have disjoint cells. The time-header columns and bottom -12h/+12h hints are bounded too. Metadata/hint strips use a 10px rasterization of the same resident Apple font; programme rows remain 12px. Text truncates inside its assigned cell and retains the overscan inset. No guide model, schedule, or tuning changes.

Validation covers the production raw-button adapter plus full guide loop, all eight watched masks/removal in all 24 shelf layouts, text-pixel confinement across both aspects and all overscan values, persistent guide-picture ownership, native/simulator builds and required playback-navigation stress. Leave the Classic mounted after deployment.

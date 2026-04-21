# Personal iTunes Theme

This folder is for your local-only iTunes 2007 assets.

- Put your real user-supplied assets here.
- Create `theme.json` by copying `theme.example.json`.
- The contents of this folder are ignored by git, except this README, the example
  manifest, and `.gitkeep` placeholders.

RockPod looks here when **Personal iTunes Theme** is selected in Preferences.
If `theme.json` is missing or invalid, RockPod falls back to `assets/theme_default/`.
If an individual asset is missing, RockPod falls back to the default theme asset.

Suggested structure:

```text
assets/theme_itunes_personal/
  theme.json
  chrome/
  toolbar/
  sidebar/
  playback/
  table/
  statusbar/
  device/
  icons/
  branding/
  artwork/
```

Recommended first files:

- `branding/title.png`
- `toolbar/sync.png`
- `toolbar/refresh.png`
- `toolbar/new_playlist.png`
- `playback/previous.png`
- `playback/play.png`
- `playback/pause.png`
- `playback/next.png`
- `sidebar/music.png`
- `sidebar/playlist.png`
- `sidebar/device.png`

Keep file names stable so future asset drops do not require code changes.

Local helper commands:

- `rockpod find-itunes-assets`
- `rockpod extract-itunes-assets /path/to/iTunesSetup.exe`
- `rockpod review-itunes-assets /path/to/extracted/assets`
- `rockpod import-itunes-assets /path/to/extracted/assets`
- `rockpod validate-theme`

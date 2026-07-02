# iPone Designer Lockscreen Weather Spec

## Goal

Show a compact weather badge on iPone Designer lockscreens when no music is
playing. The badge appears between the live time and date, and only when
RockPod has synced weather data available from `forecast.tsv`.

## Scope

Applies to:

- `iPone` and iPone-derived designer exports for 320x240 layouts.
- The no-music lockscreen branch only.
- RockPod-generated SBS/WPS assets.

Does not apply to:

- Charge screens.
- AOD.
- Playback screens.
- Menu/list viewports.

## Data Source

RockPod reads the first row from:

```text
/.rockbox/rockpod/weather/forecast.tsv
```

If the file is missing or does not contain at least one daily forecast row, the
weather badge is omitted.

The badge uses:

- `condition_text`
- `condition_code`
- `temp_min`
- `temp_max`
- `units`

## Layout

The weather badge is generated as a small themed bitmap and inserted between
the lockscreen time and date lines.

Order:

1. Time
2. Weather badge
3. Date

The badge is aligned to the current lockscreen clock box, so left/center/right
clock layouts keep the weather block visually aligned with the rest of the
lockscreen.

## Art Direction

The generated weather card should match the iPone lockscreen language:

- dark glass-like panel treatment;
- muted border and highlight;
- small condition icon;
- compact text for condition plus temperature;
- no new standalone white card.

The asset is generated per export so it inherits the selected theme colors.

## Acceptance Criteria

- A synced weather bundle causes the lockscreen to show a weather badge.
- The badge appears only on the no-music lockscreen branch.
- No weather bundle means no badge and no empty placeholder space.
- The generated lockscreen skin remains valid Rockbox skin text.


# Lyric Plugin Modernization Plan

## Goal
Completely modernize the lrcplayer plugin design with iPhone/iOS-inspired aesthetics.

---

## Current Design Analysis

### Screen Layout (320x240 LCD)
- Title scroll at top
- Multi-line lyrics with wipe effect
- Progress bar + time at bottom

---

## New iOS-Inspired Design

### Layout (320x240)
```
┌────────────────────────┐
│ ♪ Now Playing  [FLAC]│
│ Song Title          │
│ Artist Name        │
├─────────────────────┤
│ Prev line (dimmed)  │
├─────────────────────┤
│ ACTIVE LINE (bright)│
├─────────────────────┤
│ Next line (dimmed)  │
├─────────────────────┤
│ ●●●●○──── 1:23     │
└──────────────────────
```

### Color Scheme (iOS Dark Mode)
| Element | Color |
|---------|-------|
| Background | #1C1C1E |
| Active text | #FFFFFF |
| Inactive text | #8E8E93 |
| Accent/Progress | #30D158 (green) |
| Info text | #0A84FF (blue) |

### Additional Info
- Bitrate (320k)
- Format (MP3/FLAC/OGG)
- Track # if in playlist

---

## User Experience Improvements
1. 3-line layout - focused, easier to follow
2. Progress dots - iOS aesthetic
3. No wipe effect - clearer to read
4. Larger active text
5. Album art thumbnail (if available)

---

## Implementation Notes

### Files to Modify
- `apps/plugins/lrcplayer.c`

### New Settings
- Color scheme options
- Show file info
- Progress style
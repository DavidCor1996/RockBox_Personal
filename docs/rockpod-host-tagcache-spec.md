# RockPod Host Tagcache Generation Spec

## Goal

After RockPod syncs music to an iPod, the Rockbox database should already be current before the device is safely ejected. The user should be able to boot Rockbox and open Music without waiting for on-device initialization or update scans.

## Flow

1. RockPod sync copies, updates, or removes audio files under the device music roots.
2. RockPod updates its cached device inventory.
3. RockPod generates Rockbox tagcache files on the host:
   - `.rockbox/database_idx.tcd`
   - `.rockbox/database_0.tcd`
   - `.rockbox/database_1.tcd`
   - `.rockbox/database_2.tcd`
   - `.rockbox/database_3.tcd`
   - `.rockbox/database_4.tcd`
   - `.rockbox/database_5.tcd`
   - `.rockbox/database_6.tcd`
   - `.rockbox/database_7.tcd`
   - `.rockbox/database_8.tcd`
   - `.rockbox/database_12.tcd`
4. RockPod validates the generated files with its Rockbox tagcache reader.
5. RockPod replaces only Rockbox database/tagcache files under `.rockbox`.
6. If host generation fails, RockPod falls back to enabling Rockbox auto-update.

## Safety

- RockPod must never delete or rewrite user music files during tagcache generation.
- RockPod may remove stale Rockbox database/tagcache files only under `.rockbox`.
- Generation happens in a temporary directory first; files are published only after validation.
- Existing Rockbox on-device auto-update remains a fallback.

## Expected Result

After safe eject and boot, Rockbox should see a valid ready database and Music should open immediately with newly synced albums visible.

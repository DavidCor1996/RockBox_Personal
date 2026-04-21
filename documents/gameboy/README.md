# Dialtone Drop

Files in this folder:

- `dialtone.gb` — built ROM
- `dialtone-cover.png` — main cover art
- `dialtone-cover.bmp` — smaller bitmap cover derived from the main cover

Current slice highlights:

- `Luma Lane` district plus apartment, record shop, used tech shop, ramen stand, rooftop booth
- Gemini-generated startup cards and room backgrounds converted into real GB tilemaps
- Five neighbors with short errand-driven dialogue
- Pack / Tunes / Decor / Pause / Help flows tuned for Rockboy button mapping
- Session-based progression for now; persistent save is intentionally deferred while the Rockboy boot path stays conservative

Project source lives in:

- [gb/dialtone/README.md](/home/david/Documents/RockBox_Personal-master/gb/dialtone/README.md:1)
- [gb/dialtone/src/main.c](/home/david/Documents/RockBox_Personal-master/gb/dialtone/src/main.c:1)

Recommended Rockboy clickwheel mental model:

- `WHEEL` move
- `SELECT` talk / confirm
- `LEFT` pack / previous tab
- `RIGHT` tunes / next tab
- `MENU` pause / back

Built with the local GBDK bundle extracted to `/tmp/gbdk-4.5.0/gbdk`.

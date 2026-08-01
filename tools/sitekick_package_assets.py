#!/usr/bin/env python3
"""Package Sitekick Remastered art for the Rockbox iPod plugin.

The runtime intentionally loads prepared BMP and TSV files.  This host-side
tool converts real source art from a clone of the Sitekick Remastered "Art"
repository (https://github.com/SitekickRemastered/Art, GPL-3.0); it does not
generate replacement sprites.

Anchors and draw order are recovered from the Unity prefabs rather
than guessed: every chip ships a `Chip_NNNN_Effect.prefab` whose transforms
carry the same local coordinates and pivots the original game used to hang
the sprite off the robot body.  Footprint categories are collection metadata
only; the runtime's eight equip positions accept any wearable chip.

Usage:
    sitekick_package_assets.py --art-root <clone> [--out <dir>]
"""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

try:
    from PIL import Image, ImageChops, ImageDraw, ImageOps
except ImportError:  # pragma: no cover - dependency is declared in rockpod
    sys.exit("Pillow is required; run with rockpod/.venv/bin/python")


# ---------------------------------------------------------------------------
# Stage geometry.  Keep this in step with the fixed 320x240 plugin viewport;
# it is also emitted in stage.v1.tsv for pack inspection and host tooling.
# ---------------------------------------------------------------------------

STAGE_W = 240
STAGE_H = 192
BODY_TARGET_H = 52
BODY_ORIGIN_X = 120
BODY_ORIGIN_Y = 70
ICON_PX = 32
ICONS_PER_ROW = 5
ICON_ROWS = 10
ICONS_PER_PAGE = ICONS_PER_ROW * ICON_ROWS

# Unity sprites in this project are authored at 400 pixels per unit with
# centred pivots; the loader verifies rather than assumes.
DEFAULT_PPU = 400.0

BODY_SPRITE = "sitekick_body"
YTV_LOGO = (Path(__file__).resolve().parent.parent /
            "assets/ipodjs/sources/sitekick/YTV_Logo_2003.png")
BACKGROUND_SOURCE_DIR = (Path(__file__).resolve().parent.parent /
                         "assets/ipodjs/sources/sitekick/backgrounds")
IPOD_EXCLUSIVE_SOURCE_DIR = (
    Path(__file__).resolve().parent.parent /
    "assets/ipodjs/sources/sitekick/ipod-exclusive"
)
SITEKICK_BACKGROUNDS = (
    ("sitekick-splash", "sitekick-splash.png"),
    ("butterfly", "butterfly.png"),
    ("purple-gear", "purple-gear.png"),
    ("blue-gear", "blue-gear.png"),
    ("aqua-leaf", "aqua-leaf.png"),
    ("ooze-grid", "ooze-grid.png"),
    ("oliver-scrapyard", "oliver-scrapyard.png"),
    ("emma-neon-box", "emma-neon-box.png"),
    ("oliver-alone-crowd", "oliver-alone-crowd.png"),
    ("beatles-crosswalk", "beatles-crosswalk.png"),
    ("beatles-pepperland", "beatles-pepperland.png"),
    ("beatles-rooftop", "beatles-rooftop.png"),
    ("hasan-news-studio", "hasan-news-studio.png"),
    ("qtc-spotlight-stage", "qtc-spotlight-stage.png"),
    ("maya-wildlife-perch", "maya-wildlife-perch.png"),
    ("habs-home-ice", "habs-home-ice.png"),
    ("polaroid-darkroom", "polaroid-darkroom.png"),
    ("ki-arena-lightning", "ki-arena-lightning.png"),
    ("cyberpunk-night-city", "cyberpunk-night-city-bg.png"),
)
BASE_PARTS = (
    ("body", "sitekick_body"),
    ("eye", "sitekick_eye"),
    ("antenna_base", "sitekick_antenna_base"),
    ("antenna_stick", "sitekick_antenna_stick"),
    ("antenna_knob", "sitekick_antenna_knob"),
)
BODY_TINTS = (
    None,
    (105, 196, 52),
    (151, 72, 181),
    (255, 137, 29),
    (60, 181, 218),
    (235, 75, 156),
    (255, 216, 31),
)

# Reference anchors lifted from EditorSitekick.prefab, in Unity units.
BODY_ANCHORS = {
    "eye_l": (-0.177, 0.191),
    "eye_r": (0.313, 0.218),
    "antenna": (-0.295, 0.417),
    "knob": (-0.134, 0.818),
    "shell": (-0.002, 0.035),
}

SLOT_ORDER = ("aura", "hair", "shell", "eyes", "face", "antenna", "arms",
              "accessory")

# UI sound bank.  The effects are the original game's own button sounds from
# the Ooze install files; they are mono 22050 Hz and get resampled to
# whichever rate the Rockbox mixer is running at.  Durations are capped so
# the whole bank stays inside the device budget.
SOUND_BUDGET = 128 * 1024
UI_SOUNDS = (
    ("navigate", "buttonover1.wav", 0.090),
    ("select", "buttonclicked2.wav", 0.200),
    ("back", "buttonover2.wav", 0.150),
    ("reward", "gotBBdisc.wav", 0.220),
)
OOZE_SOUND_DIR = ("Source/Sitekick(TM) They Came for the Ooze (YTV Edition)"
                  "/Install Files/sound")
RARITIES = ("common", "common", "common", "rare", "rare", "legendary")

IPOD_EXCLUSIVE_CHIPS = (
    # id, slot, z, anchor x/y, rarity, name, composition, primary source
    (900, "arms", 2, -52, -38, "common", "iPod Earbuds",
     "earbuds", "generated/earbuds.png"),
    (901, "hair", 3, -35, -44, "common", "Beatles Mop Top",
     "moptop", "generated/beatles-moptop.png"),
    (902, "accessory", 3, 26, -13, "common", "Rockbox Badge",
     "rockbox-badge", "generated/rockbox-badge.png"),
    (903, "shell", 2, -34, -4, "rare", "Click Wheel Shell",
     "clickwheel-shell", "generated/clickwheel-shell.png"),
    (904, "accessory", 3, 28, -12, "rare", "Beatles Drum",
     "beatles-drum", "generated/beatles-moptop.png"),
    (905, "accessory", 3, 21, -18, "rare", "Emma Stage Mic",
     "emma-mic", "generated/emma-mic.png"),
    (906, "accessory", 3, -61, -18, "rare", "Rockbox Clef",
     "rockbox-clef", "generated/rockbox-badge.png"),
    (907, "aura", 4, -54, -45, "legendary", "iPod Listener",
     "ipod-listener", "generated/ipod-prop.png"),
    (908, "shell", -1, -37, -36, "legendary", "Abbey Road Disc",
     "abbey-disc", "generated/abbey-road-disc.png"),
    (909, "shell", -1, -42, -4, "legendary", "Emma Stage Jacket",
     "emma-jacket", "generated/emma-jacket.png"),
    (910, "aura", -3, -55, -52, "legendary", "Rockbox Halo",
     "rockbox-halo", "generated/rockbox-badge.png"),
    (911, "accessory", 3, 23, -18, "legendary", "RockPod Classic",
     "rockpod-classic", "generated/ipod-prop.png"),
    (912, "hair", 4, -38, -46, "common", "Oliver Bowl Cut",
     "oliver-hair", "generated/oliver-hair.png"),
    (913, "eyes", 5, -22, -18, "rare", "Oliver Shades",
     "oliver-glasses", "generated/oliver-glasses.png"),
    (914, "shell", -1, -42, -4, "legendary", "Oliver LP3 Jacket",
     "oliver-jacket", "generated/oliver-jacket.png"),
    (915, "hair", 4, -36, -52, "common", "Fox Ears",
     "fox-ears", "generated/fox-ears.png"),
    (916, "hair", 4, -36, -42, "common", "Hound Ears",
     "hound-ears", "generated/hound-ears.png"),
    (917, "aura", -2, -72, -20, "rare", "Fox Tail",
     "fox-tail", "generated/fox-tail.png"),
    (918, "shell", -1, -44, -3, "legendary", "Hound Friendship Collar",
     "hound-collar", "generated/hound-collar.png"),
    (919, "hair", 4, -37, -43, "common", "Villains Bob",
     "emma-villains-hair", "generated/emma-villains-hair.png"),
    (920, "shell", -1, -43, -3, "rare", "Villains Ringer Tee",
     "emma-villains-tee", "generated/emma-villains-tee.png"),
    (921, "hair", 4, -50, -44, "rare", "Box Pigtails",
     "emma-box-pigtails", "generated/emma-box-pigtails.png"),
    (922, "shell", -1, -46, -3, "legendary", "Box Stripe Sweater",
     "emma-box-sweater", "generated/emma-box-sweater.png"),
    (923, "hair", 4, -44, -43, "common", "Madly Long Locks",
     "oliver-madly-hair", "generated/oliver-madly-hair.png"),
    (924, "shell", -1, -47, -4, "rare", "Madly Junkyard Fit",
     "oliver-madly-outfit", "generated/oliver-madly-outfit.png"),
    (925, "aura", -3, -73, -48, "legendary", "Madly Scrap Heart",
     "oliver-madly-heart", "generated/oliver-madly-heart.png"),
    (926, "hair", 4, -39, -45, "common", "Camden Tousle",
     "earl-tousled-hair", "generated/earl-tousled-hair.png"),
    (927, "face", 5, -16, 4, "common", "Lucky Moustache",
     "earl-mustache", "generated/earl-mustache.png"),
    (928, "hair", 4, -50, -48, "rare", "Joy Ponytail",
     "joy-ponytail", "generated/joy-ponytail.png"),
    (929, "shell", -1, -47, -4, "rare", "Camden Plaid Fit",
     "earl-plaid-outfit", "generated/earl-plaid-outfit.png"),
    (930, "accessory", 3, 22, -17, "legendary", "The Karma List",
     "karma-list", "generated/karma-list.png"),
    (931, "aura", -3, -73, -48, "legendary", "Good Karma Halo",
     "good-karma-aura", "generated/good-karma-aura.png"),
    (932, "arms", 2, -55, -12, "common", "Peace Sign Pair",
     "hand-peace-pair", "generated/hand-peace-pair.png"),
    (933, "arms", 2, -55, -8, "rare", "Double Thumbs Up",
     "hand-thumbs-pair", "generated/hand-thumbs-pair.png"),
    (934, "arms", 2, -55, -12, "legendary", "Rock Horns Pair",
     "hand-rock-pair", "generated/hand-rock-pair.png"),
    (935, "hair", 4, -38, -45, "common", "Turbo Bowl Cut",
     "oliver-turbo-hair", "generated/oliver-turbo-hair.png"),
    (936, "eyes", 5, -22, -18, "rare", "Turbo Red Shades",
     "oliver-turbo-shades", "generated/oliver-turbo-shades.png"),
    (937, "shell", -1, -48, -4, "legendary", "Turbo Windbreaker Fit",
     "oliver-turbo-outfit", "generated/oliver-turbo-outfit.png"),
    (938, "aura", -3, -75, -48, "legendary", "Wall Brick Halo",
     "wall-brick-halo", "generated/wall-brick-halo.png"),
    (939, "eyes", 5, -23, -18, "rare", "Prism Beam Shades",
     "prism-beam-shades", "generated/prism-beam-shades.png"),
    (940, "arms", 2, -65, -39, "legendary", "Hybrid Mech Wings",
     "hybrid-mech-wings", "generated/hybrid-mech-wings.png"),
    (941, "shell", -1, -48, -4, "rare", "Meteora Spray Hoodie",
     "meteora-spray-hoodie", "generated/meteora-spray-hoodie.png"),
    (942, "hair", 4, -41, -45, "common", "Riot Scribble Bob",
     "riot-scribble-bob", "generated/riot-scribble-bob.png"),
    (943, "shell", -1, -48, -4, "rare", "Riot Orange Jacket",
     "riot-orange-jacket", "generated/riot-orange-jacket.png"),
    (944, "eyes", 5, -23, -18, "common", "Sunnyvale Shades",
     "sunnyvale-shades", "generated/sunnyvale-shades.png"),
    (945, "shell", -1, -48, -4, "rare", "Trailer Park Work Shirt",
     "trailer-park-work-shirt", "generated/trailer-park-work-shirt.png"),
    (946, "hair", 4, -43, -45, "common", "Playground Cap",
     "playground-cap", "generated/playground-cap.png"),
    (947, "arms", 2, -60, -35, "rare", "Recess Backpack",
     "recess-backpack", "generated/recess-backpack.png"),
    (948, "arms", 2, -63, -43, "common", "Mall Headphones",
     "mall-headphones", "generated/mall-headphones.png"),
    (949, "shell", -1, -48, -4, "legendary", "Food Court Hoodie",
     "food-court-hoodie", "generated/food-court-hoodie.png"),
    (950, "hair", 4, -41, -46, "common", "Rooftop Shag",
     "beatles-rooftop-shag", "generated/beatles-rooftop-shag.png"),
    (951, "eyes", 5, -23, -18, "common", "Round Beat Glasses",
     "beatles-round-glasses", "generated/beatles-round-glasses.png"),
    (952, "face", 5, -17, 3, "rare", "Pepper Moustache",
     "beatles-pepper-moustache", "generated/beatles-pepper-moustache.png"),
    (953, "shell", -1, -48, -4, "legendary", "Pepper Band Jacket",
     "beatles-pepper-jacket", "generated/beatles-pepper-jacket.png"),
    (954, "shell", -1, -48, -4, "rare", "Rooftop Shearling",
     "beatles-rooftop-coat", "generated/beatles-rooftop-coat.png"),
    (955, "accessory", 3, 15, -25, "legendary", "Violin Beat Bass",
     "beatles-violin-bass", "generated/beatles-violin-bass.png"),
    (956, "hair", 4, -43, -47, "rare", "Submarine Captain",
     "beatles-submarine-cap", "generated/beatles-submarine-cap.png"),
    (957, "arms", 2, -54, 8, "common", "Beat Drumsticks",
     "beatles-drumsticks", "generated/beatles-drumsticks.png"),
    (958, "hair", 4, -43, -45, "common", "Trainer League Cap",
     "pokemon-trainer-cap", "generated/pokemon-trainer-cap.png"),
    (959, "accessory", 3, 20, -14, "common", "Poke Ball Toss",
     "pokemon-poke-ball", "generated/pokemon-poke-ball.png"),
    (960, "shell", -1, -48, -4, "rare", "GO Field Jacket",
     "pokemongo-field-jacket", "generated/pokemongo-field-jacket.png"),
    (961, "shell", -1, -48, -4, "rare", "GO Research Coat",
     "pokemongo-research-coat", "generated/pokemongo-research-coat.png"),
    (962, "accessory", 3, 22, -18, "rare", "GO Raid Pass",
     "pokemongo-raid-pass", "generated/pokemongo-raid-pass.png"),
    (963, "accessory", 3, 20, -18, "legendary", "Pocket Dex",
     "pokemon-pocket-dex", "generated/pokemon-pocket-dex.png"),
    (964, "aura", -3, -75, -48, "legendary", "GO Lure Halo",
     "pokemongo-lure-halo", "generated/pokemongo-lure-halo.png"),
    (965, "eyes", 5, -24, -18, "common", "GO AR Visor",
     "pokemongo-ar-visor", "generated/pokemongo-ar-visor.png"),
    (966, "shell", -1, -48, -4, "legendary", "Rocket Grunt Fit",
     "pokemon-rocket-uniform", "generated/pokemon-rocket-uniform.png"),
    (967, "accessory", 3, 20, -14, "rare", "Great Ball Toss",
     "pokemon-great-ball", "generated/pokemon-great-ball.png"),
    (968, "aura", -3, -75, -42, "legendary", "GO Map Explorer",
     "pokemongo-map-aura", "generated/pokemongo-map-aura.png"),
    # RockPod media wave 2. The dive/fast-food set is a purely thematic
    # cartoon interpretation (mask, fins, wetsuit, bubbles) with no name or
    # likeness attached. The doll set follows the "Dollhouse" video's
    # porcelain-doll aesthetic (space buns, asymmetric doll eyes, pinafore,
    # cracked porcelain) rather than any performer's name.
    (969, "eyes", 5, -24, -19, "common", "Dive Mask Visor",
     "dive-mask-visor", "generated/dive-mask-visor.png"),
    (970, "arms", 2, -56, 10, "rare", "Fry Basket Fins",
     "fry-basket-fins", "generated/fry-basket-fins.png"),
    (971, "shell", -1, -48, -4, "rare", "Drive-Thru Wetsuit",
     "drive-thru-wetsuit", "generated/drive-thru-wetsuit.png"),
    (972, "aura", -3, -72, -46, "legendary", "Bubble Trail Aura",
     "bubble-trail-aura", "generated/bubble-trail-aura.png"),
    (973, "hair", 4, -50, -46, "common", "Doll Space Buns",
     "doll-space-buns", "generated/doll-space-buns.png"),
    (974, "eyes", 5, -24, -19, "rare", "Cracked Doll Eyes",
     "cracked-doll-eyes", "generated/cracked-doll-eyes.png"),
    (975, "shell", -1, -46, -3, "rare", "Dollhouse Pinafore",
     "dollhouse-pinafore", "generated/dollhouse-pinafore.png"),
    (976, "aura", -3, -70, -46, "legendary", "Cracked Porcelain",
     "porcelain-crack-halo", "generated/porcelain-crack-halo.png"),
    (977, "aura", -3, -80, -56, "legendary", "Polaroid Frame",
     "polaroid-frame-backdrop", "generated/polaroid-frame-backdrop.png"),
    (978, "eyes", 5, -23, -18, "common", "Flash Pop Shades",
     "flash-pop-shades", "generated/flash-pop-shades.png"),
    (979, "accessory", 3, 20, -18, "rare", "Instant Print Fan",
     "instant-print-fan", "generated/instant-print-fan.png"),
    # RockPod media wave 3: a "hero of time" homage built from generic
    # fantasy-adventurer shapes (pointed cap, tunic, shield/sword,
    # ocarina, heart, triforce, fairy) -- an original cartoon
    # interpretation, no copied logos or ripped game assets.
    (980, "hair", 4, -43, -45, "common", "Hero Cap",
     "hero-cap", "generated/hero-cap.png"),
    (981, "shell", -1, -48, -4, "rare", "Hero Tunic",
     "hero-tunic", "generated/hero-tunic.png"),
    (982, "accessory", 3, 21, -18, "legendary", "Hylian Shield",
     "hylian-shield", "generated/hylian-shield.png"),
    (983, "eyes", 5, -23, -19, "common", "Forest Eye Mask",
     "forest-eye-mask", "generated/forest-eye-mask.png"),
    (984, "accessory", 3, 22, -16, "rare", "Ocarina Charm",
     "ocarina-charm", "generated/ocarina-charm.png"),
    (985, "accessory", 3, 20, -17, "legendary", "Heart Container",
     "heart-container", "generated/heart-container.png"),
    (986, "aura", -3, -70, -46, "legendary", "Triforce Halo",
     "triforce-halo", "generated/triforce-halo.png"),
    (987, "aura", -3, -70, -30, "rare", "Fairy Companion",
     "fairy-companion", "generated/fairy-companion.png"),
    # RockPod media wave 4: two streamer homages built from generic props
    # (backwards cap, headset, hoodie, news-ticker glow for one; ponytail,
    # cat-eye visor, awards trophy, spotlight glow for the other) -- no
    # portraits, no likenesses, same restraint as every prior wave.
    (988, "hair", 4, -43, -46, "common", "Hasan Backward Cap",
     "hasan-cap", "generated/hasan-cap.png"),
    (989, "accessory", 3, 22, -18, "rare", "Hasan Headset Mic",
     "hasan-headset", "generated/hasan-headset.png"),
    (990, "shell", -1, -48, -4, "rare", "Hasan News Hoodie",
     "hasan-hoodie", "generated/hasan-hoodie.png"),
    (991, "aura", -3, -75, -45, "legendary", "Breaking News Aura",
     "hasan-news-aura", "generated/hasan-news-aura.png"),
    (992, "hair", 4, -50, -48, "common", "QTC Ponytail",
     "qtc-ponytail", "generated/qtc-ponytail.png"),
    (993, "eyes", 5, -23, -18, "rare", "QTC Cat-Eye Visor",
     "qtc-visor", "generated/qtc-visor.png"),
    (994, "accessory", 3, 22, -17, "legendary", "QTC Awards Trophy",
     "qtc-trophy", "generated/qtc-trophy.png"),
    (995, "aura", -3, -70, -50, "legendary", "QTC Stage Glow",
     "qtc-spotlight-aura", "generated/qtc-spotlight-aura.png"),
    # RockPod media wave 5: Maya Higa (generic ranger/falconry props, no
    # likeness), Montreal Canadiens (colors + rink motifs only, no crest),
    # and five original pet-companion critters that sit beside the body
    # rather than on it -- same eight generic equip positions, just an
    # anchor placed off to the side (see chip 987 Fairy Companion for the
    # existing precedent of a beside-the-body chip).
    (996, "hair", 4, -44, -44, "common", "Maya Ranger Hat",
     "maya-ranger-hat", "generated/maya-ranger-hat.png"),
    (997, "arms", 2, -55, -8, "rare", "Maya Falcon Glove",
     "maya-falcon-glove", "generated/maya-falcon-glove.png"),
    (998, "shell", -1, -48, -4, "rare", "Maya Field Vest",
     "maya-field-vest", "generated/maya-field-vest.png"),
    (999, "aura", -3, -75, -48, "legendary", "Maya Forest Aura",
     "maya-forest-aura", "generated/maya-forest-aura.png"),
    (1000, "shell", -1, -48, -4, "rare", "Habs Home Jersey",
     "habs-home-jersey", "generated/habs-home-jersey.png"),
    (1001, "arms", 2, -50, -6, "rare", "Habs Hockey Stick",
     "habs-hockey-stick", "generated/habs-hockey-stick.png"),
    (1002, "hair", 4, -43, -46, "common", "Habs Winter Toque",
     "habs-winter-toque", "generated/habs-winter-toque.png"),
    (1003, "aura", -3, -75, -46, "legendary", "Habs Rink Aura",
     "habs-rink-aura", "generated/habs-rink-aura.png"),
    (1004, "accessory", 3, 34, -20, "legendary", "Spyro Companion",
     "spyro-companion", "generated/spyro-companion.png"),
    (1005, "accessory", 3, 36, -8, "common", "Cat Companion",
     "cat-companion", "generated/cat-companion.png"),
    (1006, "accessory", 3, -92, -8, "common", "Dog Companion",
     "dog-companion", "generated/dog-companion.png"),
    (1007, "accessory", 3, -88, 6, "rare", "Turtle Companion",
     "turtle-companion", "generated/turtle-companion.png"),
    (1008, "aura", -3, 30, -46, "legendary", "Navi Companion",
     "navi-companion", "generated/navi-companion.png"),
    # RockPod media wave 6: Pink Floyd (generic floating pig, prism/
    # spectrum beam, crossed hammers -- no album art traced), Portal 2
    # (generic ray-gun, heart-marked cube, chunky boots, companion
    # turret -- no game textures traced), and Old School RuneScape armor/
    # weapon silhouettes in the games' iconic color schemes rather than
    # copied game art.
    (1009, "aura", -3, -70, -55, "legendary", "Flying Pig Aura",
     "pink-floyd-pig", "generated/pink-floyd-pig.png"),
    (1010, "aura", -3, -75, -48, "legendary", "Prism Spectrum",
     "pink-floyd-prism", "generated/pink-floyd-prism.png"),
    (1011, "accessory", 3, 22, -18, "rare", "Marching Hammers",
     "pink-floyd-hammers", "generated/pink-floyd-hammers.png"),
    (1012, "accessory", 3, 20, -14, "legendary", "Portal Gun",
     "portal-gun", "generated/portal-gun.png"),
    (1013, "accessory", 3, 36, -6, "rare", "Companion Cube",
     "companion-cube", "generated/companion-cube.png"),
    (1014, "arms", 2, -58, 10, "common", "Long Fall Boots",
     "long-fall-boots", "generated/long-fall-boots.png"),
    (1015, "accessory", 3, -90, -14, "legendary", "Aperture Turret",
     "aperture-turret", "generated/aperture-turret.png"),
    (1016, "shell", -1, -48, -4, "rare", "Rune Platebody",
     "osrs-rune-platebody", "generated/osrs-rune-platebody.png"),
    (1017, "shell", -1, -48, -4, "legendary", "Dragon Platebody",
     "osrs-dragon-platebody", "generated/osrs-dragon-platebody.png"),
    (1018, "arms", 2, -50, -40, "legendary", "Ornate Godsword",
     "osrs-godsword", "generated/osrs-godsword.png"),
    (1019, "accessory", 3, 24, -20, "legendary", "Abyssal Whip",
     "osrs-abyssal-whip", "generated/osrs-abyssal-whip.png"),
    (1020, "arms", 2, -55, -10, "legendary", "Dragon Claws",
     "osrs-dragon-claws", "generated/osrs-dragon-claws.png"),
    # RockPod media wave 7: more popular OSRS items for the "Runes" set --
    # original silhouettes in each item's iconic color scheme, no traced
    # game icons.
    (1021, "hair", 4, -35, -50, "legendary", "OSRS Party Hat",
     "osrs-party-hat", "generated/osrs-party-hat.png"),
    (1022, "hair", 4, -38, -50, "legendary", "OSRS Santa Hat",
     "osrs-santa-hat", "generated/osrs-santa-hat.png"),
    (1023, "shell", -1, -48, -4, "legendary", "OSRS Fire Cape",
     "osrs-fire-cape", "generated/osrs-fire-cape.png"),
    (1024, "shell", -1, -48, -4, "legendary", "OSRS Max Cape",
     "osrs-max-cape", "generated/osrs-max-cape.png"),
    (1025, "accessory", 3, 22, -30, "legendary", "Twisted Bow",
     "osrs-twisted-bow", "generated/osrs-twisted-bow.png"),
    (1026, "arms", 2, -45, -35, "legendary", "Dragon Scimitar",
     "osrs-dragon-scimitar", "generated/osrs-dragon-scimitar.png"),
    (1027, "accessory", 3, -20, -22, "legendary", "Amulet of Fury",
     "osrs-amulet-of-fury", "generated/osrs-amulet-of-fury.png"),
    (1028, "arms", 2, -55, -8, "legendary", "Barrows Gloves",
     "osrs-barrows-gloves", "generated/osrs-barrows-gloves.png"),
    # RockPod media wave 8: more Polaroid pieces, Killer Instinct (generic
    # lightning "combo" burst, glowing ninja visor, energy blades -- no
    # character likeness), and Cyberpunk 2077 (generic neon visor, chrome
    # arm, retractable arm blades, neon skyline -- no game textures/logos).
    (1029, "accessory", 3, 22, -18, "rare", "Polaroid Camera",
     "polaroid-camera", "generated/polaroid-camera.png"),
    (1030, "accessory", 3, -20, -20, "common", "Photo Strip",
     "photo-strip", "generated/photo-strip.png"),
    (1031, "aura", -3, -75, -48, "legendary", "Retro Filmstrip",
     "retro-filmstrip", "generated/retro-filmstrip.png"),
    (1032, "aura", -3, -75, -46, "legendary", "Ultra Combo Aura",
     "ki-ultra-combo", "generated/ki-ultra-combo.png"),
    (1033, "eyes", 5, -23, -18, "rare", "Cyber Ninja Visor",
     "ki-ninja-visor", "generated/ki-ninja-visor.png"),
    (1034, "arms", 2, -55, -10, "legendary", "Energy Blades",
     "ki-energy-blades", "generated/ki-energy-blades.png"),
    (1035, "eyes", 5, -23, -18, "rare", "Neon Visor",
     "cyberpunk-neon-visor", "generated/cyberpunk-neon-visor.png"),
    (1036, "arms", 2, -30, -8, "legendary", "Chrome Cyberarm",
     "cyberpunk-cyberarm", "generated/cyberpunk-cyberarm.png"),
    (1037, "accessory", 3, 20, -10, "legendary", "Mantis Blades",
     "cyberpunk-mantis-blades", "generated/cyberpunk-mantis-blades.png"),
    (1038, "aura", -3, -75, -45, "legendary", "Night City Neon",
     "cyberpunk-night-city", "generated/cyberpunk-night-city.png"),
    # RockPod media wave 9: more Cyberpunk 2077 (Johnny Silverhand, Judy
    # Alvarez) and more Killer Instinct (Spinal) -- generic rockerboy/
    # mechanic/pirate-skeleton props evoking each character's silhouette
    # and color scheme, no portraits or traced game art.
    (1039, "shell", -1, -48, -4, "legendary", "Silverhand Jacket",
     "silverhand-jacket", "generated/silverhand-jacket.png"),
    (1040, "arms", 2, -30, -8, "legendary", "Chrome Rock Arm",
     "chrome-rock-arm", "generated/chrome-rock-arm.png"),
    (1041, "eyes", 5, -23, -18, "rare", "Aviator Shades",
     "aviator-shades", "generated/aviator-shades.png"),
    (1042, "accessory", 3, 18, -30, "legendary", "Rockerboy Guitar",
     "rockerboy-guitar", "generated/rockerboy-guitar.png"),
    (1043, "hair", 4, -50, -46, "common", "Judy Twin Braids",
     "judy-twin-braids", "generated/judy-twin-braids.png"),
    (1044, "eyes", 5, -23, -19, "rare", "Welding Goggles",
     "welding-goggles", "generated/welding-goggles.png"),
    (1045, "shell", -1, -48, -4, "rare", "Mechanic Overalls",
     "mechanic-overalls", "generated/mechanic-overalls.png"),
    (1046, "face", 5, -20, -14, "legendary", "Spinal Skull Mask",
     "spinal-skull-mask", "generated/spinal-skull-mask.png"),
    (1047, "hair", 4, -42, -46, "common", "Pirate Bandana",
     "pirate-bandana", "generated/pirate-bandana.png"),
    (1048, "arms", 2, -55, -10, "legendary", "Twin Cutlasses",
     "twin-cutlasses", "generated/twin-cutlasses.png"),
    (1049, "aura", -3, -75, -46, "legendary", "Ghost Flame Aura",
     "ghost-flame-aura", "generated/ghost-flame-aura.png"),
)

SECRET_CODE_GRANTS = (
    # code, chip id, public label
    ("OLIVER", 912, "Oliver Bowl Cut"),
    ("OLIVER", 913, "Oliver Shades"),
    ("OLIVER", 914, "Oliver LP3 Jacket"),
    ("OLIVER", 923, "Madly Long Locks"),
    ("OLIVER", 924, "Madly Junkyard Fit"),
    ("OLIVER", 925, "Madly Scrap Heart"),
    ("OLIVER", 935, "Turbo Bowl Cut"),
    ("OLIVER", 936, "Turbo Red Shades"),
    ("OLIVER", 937, "Turbo Windbreaker Fit"),
    ("BLACKERY", 905, "Emma Stage Mic"),
    ("BLACKERY", 909, "Emma Stage Jacket"),
    ("BLACKERY", 919, "Villains Bob"),
    ("BLACKERY", 920, "Villains Ringer Tee"),
    ("BLACKERY", 921, "Box Pigtails"),
    ("BLACKERY", 922, "Box Stripe Sweater"),
    ("RUNES", 1016, "Rune Platebody"),
    ("RUNES", 1017, "Dragon Platebody"),
    ("RUNES", 1018, "Ornate Godsword"),
    ("RUNES", 1019, "Abyssal Whip"),
    ("RUNES", 1020, "Dragon Claws"),
    ("RUNES", 1021, "OSRS Party Hat"),
    ("RUNES", 1022, "OSRS Santa Hat"),
    ("RUNES", 1023, "OSRS Fire Cape"),
    ("RUNES", 1024, "OSRS Max Cape"),
    ("RUNES", 1025, "Twisted Bow"),
    ("RUNES", 1026, "Dragon Scimitar"),
    ("RUNES", 1027, "Amulet of Fury"),
    ("RUNES", 1028, "Barrows Gloves"),
)


# ---------------------------------------------------------------------------
# Unity asset indexing
# ---------------------------------------------------------------------------

RE_GUID = re.compile(r"^guid:\s*([0-9a-f]{32})", re.M)
RE_PPU = re.compile(r"spritePixelsToUnits:\s*([0-9.]+)")
RE_PIVOT = re.compile(r"spritePivot:\s*\{x:\s*([-0-9.]+),\s*y:\s*([-0-9.]+)\}")


@dataclass
class SpriteMeta:
    path: Path
    guid: str
    ppu: float = DEFAULT_PPU
    pivot: tuple[float, float] = (0.5, 0.5)


@dataclass
class PrefabPart:
    name: str
    pos: tuple[float, float]
    scale: tuple[float, float]
    order: int
    guid: str
    flip: tuple[bool, bool] = (False, False)


@dataclass
class ChipRecord:
    cid: int
    parts: list[PrefabPart] = field(default_factory=list)
    icon: Path | None = None
    slot: str = "accessory"
    z: int = 0
    anchor: tuple[float, float] = (0.0, 0.0)


def index_sprites(art_root: Path) -> dict[str, SpriteMeta]:
    """Map every sprite guid to its PNG, pixels-per-unit and pivot."""
    index: dict[str, SpriteMeta] = {}
    for meta in art_root.rglob("*.png.meta"):
        png = meta.with_suffix("")           # strip .meta
        if not png.exists():
            continue
        try:
            text = meta.read_text(errors="replace")
        except OSError:
            continue
        m = RE_GUID.search(text)
        if not m:
            continue
        ppu = RE_PPU.search(text)
        pivot = RE_PIVOT.search(text)
        index[m.group(1)] = SpriteMeta(
            path=png,
            guid=m.group(1),
            ppu=float(ppu.group(1)) if ppu else DEFAULT_PPU,
            pivot=(float(pivot.group(1)), float(pivot.group(2)))
            if pivot else (0.5, 0.5),
        )
    return index


RE_DOC = re.compile(r"^--- !u!(\d+) &(\d+)", re.M)


def parse_unity_docs(text: str) -> list[tuple[int, str, str]]:
    """Split a Unity YAML file into (class id, anchor id, body) documents."""
    marks = list(RE_DOC.finditer(text))
    docs = []
    for i, m in enumerate(marks):
        end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        docs.append((int(m.group(1)), m.group(2), text[m.end():end]))
    return docs


def _vec(body: str, key: str, default=(0.0, 0.0)) -> tuple[float, float]:
    m = re.search(key + r":\s*\{x:\s*([-0-9.eE]+),\s*y:\s*([-0-9.eE]+)", body)
    if not m:
        return default
    return (float(m.group(1)), float(m.group(2)))


def _fileid(body: str, key: str) -> str | None:
    m = re.search(key + r":\s*\{fileID:\s*(-?\d+)", body)
    return m.group(1) if m else None


def _sprite_guid(body: str) -> str | None:
    m = re.search(r"m_Sprite:\s*\{fileID:\s*-?\d+,\s*guid:\s*([0-9a-f]{32})",
                  body)
    return m.group(1) if m else None


def parse_prefab(path: Path) -> list[PrefabPart]:
    """Extract every sprite-bearing node with its world position and order.

    Unity stores positions relative to each node's parent, so the transform
    chain is walked to the prefab root before the anchor is recorded.
    """
    try:
        text = path.read_text(errors="replace")
    except OSError:
        return []

    transforms: dict[str, dict] = {}
    renderers: dict[str, dict] = {}
    names: dict[str, str] = {}

    for cls, anchor, body in parse_unity_docs(text):
        if cls == 1:                                  # GameObject
            m = re.search(r"m_Name:\s*(.*)", body)
            names[anchor] = m.group(1).strip() if m else ""
        elif cls == 4:                                # Transform
            go = _fileid(body, "m_GameObject")
            transforms[anchor] = {
                "go": go,
                "pos": _vec(body, "m_LocalPosition"),
                "scale": _vec(body, "m_LocalScale", (1.0, 1.0)),
                "father": _fileid(body, "m_Father"),
            }
        elif cls == 212:                              # SpriteRenderer
            go = _fileid(body, "m_GameObject")
            order = re.search(r"m_SortingOrder:\s*(-?\d+)", body)
            renderers[go] = {
                "guid": _sprite_guid(body),
                "order": int(order.group(1)) if order else 0,
                "flip": (
                    bool(re.search(r"m_FlipX:\s*1", body)),
                    bool(re.search(r"m_FlipY:\s*1", body)),
                ),
            }

    by_go = {t["go"]: (anchor, t) for anchor, t in transforms.items() if t["go"]}

    def world(anchor: str) -> tuple[tuple[float, float], tuple[float, float]]:
        x = y = 0.0
        sx = sy = 1.0
        seen = set()
        cur = anchor
        while cur and cur in transforms and cur not in seen:
            seen.add(cur)
            t = transforms[cur]
            x += t["pos"][0] * sx
            y += t["pos"][1] * sy
            sx *= t["scale"][0]
            sy *= t["scale"][1]
            cur = t["father"]
            if cur in ("0", None):
                break
        return (x, y), (sx, sy)

    parts: list[PrefabPart] = []
    for go, rend in renderers.items():
        if not rend["guid"] or go not in by_go:
            continue
        anchor, _ = by_go[go]
        pos, scale = world(anchor)
        parts.append(PrefabPart(
            name=names.get(go, ""),
            pos=pos,
            scale=scale,
            order=rend["order"],
            guid=rend["guid"],
            flip=rend["flip"],
        ))
    return parts


# ---------------------------------------------------------------------------
# Slot classification
# ---------------------------------------------------------------------------

def classify(ax: int, ay: int, w: int, h: int, body_w: int, body_h: int) -> str:
    """Bucket a chip into a paperdoll slot from its composed stage footprint.

    The stage box is a far better signal than the Unity anchor mean: a mean
    anchor puts a wig and an eyepatch in the same place, whereas their
    footprints relative to the body differ sharply.  Coordinates are relative
    to the body centre, +y down.
    """
    if w <= 0 or h <= 0:
        return "none"

    half_w, half_h = body_w / 2.0, body_h / 2.0
    # Normalise everything against the body half-extents.
    cx = (ax + w / 2.0) / half_w
    cy = (ay + h / 2.0) / half_h
    top = ay / half_h
    bottom = (ay + h) / half_h
    coverage = (w * h) / float(body_w * body_h)

    # Much larger than the robot: backdrops, scenery, full-frame effects.
    if coverage >= 2.5:
        return "aura"
    # Entirely above the shell: masts and floating props.
    if bottom <= -0.85:
        return "antenna"
    # Rides on the crown without draping over the body: wigs, hats, helmets.
    if top <= -0.90 and bottom <= 0.40:
        return "hair"
    # Hangs off the bottom: limbs, feet, boards, vehicles.
    if cy >= 0.80:
        return "arms"
    # Compact and at eye height.
    if coverage <= 0.22 and -0.55 <= cy <= 0.15:
        return "eyes"
    # Compact and centred: masks, mouths, noses.
    if coverage <= 0.60 and abs(cx) <= 0.60 and abs(cy) <= 0.60:
        return "face"
    # Small but off to one side.
    if coverage <= 0.30:
        return "accessory"
    return "shell"


def assert_stage_fit(cid: int, name: str, ax: int, ay: int,
                     w: int, h: int) -> None:
    """Fail loudly instead of shipping pixels sitekick.c silently clips.

    The runtime draws every chip relative to the body origin and never
    scrolls the stage, so anything outside this box is truncated at blit
    time rather than at build time (see the preserved-catalogue crop a few
    lines up).  The iPod-exclusive series bypasses that crop entirely, so it
    must be checked explicitly here.
    """
    vis_l, vis_t = -BODY_ORIGIN_X, -BODY_ORIGIN_Y
    vis_r, vis_b = vis_l + STAGE_W, vis_t + STAGE_H
    if not (ax >= vis_l and ay >= vis_t and ax + w <= vis_r and ay + h <= vis_b):
        raise ValueError(
            f"chip {cid} ({name!r}) does not fit the stage: "
            f"anchor=({ax},{ay}) size=({w},{h}) stage="
            f"[{vis_l},{vis_t}..{vis_r},{vis_b}]"
        )


def rarity_for(cid: int) -> str:
    digest = hashlib.sha256(f"sitekick-chip-{cid}".encode()).digest()
    return RARITIES[digest[0] % len(RARITIES)]


# ---------------------------------------------------------------------------
# Image helpers
# ---------------------------------------------------------------------------

def trim(img: Image.Image) -> tuple[Image.Image, int, int]:
    """Crop transparent margins, returning the crop offset."""
    box = img.getbbox()
    if not box:
        return img, 0, 0
    return img.crop(box), box[0], box[1]


def tint_body(img: Image.Image, color: tuple[int, int, int]) -> Image.Image:
    """Colorize the real body sprite while preserving its authored shading."""
    rgba = img.convert("RGBA")
    alpha = rgba.getchannel("A")
    luminance = ImageOps.grayscale(rgba)
    tinted = ImageOps.colorize(luminance, black=(0, 0, 0), white=color)
    tinted.putalpha(alpha)
    return tinted


def cover_image(img: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Aspect-fill an authentic background and crop it to the target."""
    target_w, target_h = size
    scale = max(target_w / img.width, target_h / img.height)
    resized = img.resize(
        (max(1, round(img.width * scale)),
         max(1, round(img.height * scale))),
        Image.LANCZOS,
    )
    left = (resized.width - target_w) // 2
    top = (resized.height - target_h) // 2
    return resized.crop((left, top, left + target_w, top + target_h))


def contain_alpha(img: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Trim and contain an RGBA source in an exact transparent canvas."""
    source, _, _ = trim(img.convert("RGBA"))
    source.thumbnail(size, Image.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.alpha_composite(
        source,
        ((size[0] - source.width) // 2, (size[1] - source.height) // 2),
    )
    return canvas


def clear_alpha_ellipse(img: Image.Image,
                        box: tuple[int, int, int, int]) -> Image.Image:
    """Cut a true transparent opening without disturbing its drawn rim."""
    result = img.copy()
    alpha = result.getchannel("A")
    ImageDraw.Draw(alpha).ellipse(box, fill=0)
    result.putalpha(alpha)
    return result


def snap_alpha(img: Image.Image, threshold: int = 4) -> Image.Image:
    """Zero out near-invisible LANCZOS ringing so corners stay true alpha.

    contain_alpha's thumbnail resize can leave a 1-3/255 alpha residue at
    otherwise-transparent corners of large flat shapes (e.g. a near-full-
    canvas rounded rectangle). It is invisible but fails a strict "no
    corner matte" check and doesn't match the rest of the catalogue, which
    has exact zero there.
    """
    alpha = img.getchannel("A").point(lambda a: 0 if a < threshold else a)
    result = img.copy()
    result.putalpha(alpha)
    return result


def cel_shade(img: Image.Image, light: int = 130, shadow: int = 140,
              shadow_color: tuple[int, int, int] = (12, 6, 18)
              ) -> Image.Image:
    """Overlay a soft diagonal light/shadow sheen inside the image's own
    alpha silhouette.

    Flat single-tone polygon fills read as amateur clip art next to the
    hand-authored/AI-rendered chips (900-987): those carry real light and
    shadow. A diagonal highlight (upper-left) fading into a diagonal
    shadow (lower-right), masked to the shape's own alpha so it never
    bleeds outside the silhouette, gives any flat-filled shape a sense of
    volume for near-zero extra drawing work per chip.
    """
    img = img.convert("RGBA")
    w, h = img.size
    if w == 0 or h == 0:
        return img
    alpha = img.getchannel("A")
    grad = Image.linear_gradient("L").resize((w, h))  # 0 at TL -> 255 at BR
    inv_grad = grad.point(lambda v: 255 - v)
    highlight_alpha = ImageChops.multiply(inv_grad, alpha).point(
        lambda v: v * light // 255)
    shadow_alpha = ImageChops.multiply(grad, alpha).point(
        lambda v: v * shadow // 255)
    shadow_layer = Image.new("RGBA", img.size, (*shadow_color, 255))
    shadow_layer.putalpha(shadow_alpha)
    highlight_layer = Image.new("RGBA", img.size, (255, 255, 255, 255))
    highlight_layer.putalpha(highlight_alpha)
    out = Image.alpha_composite(img, shadow_layer)
    out = Image.alpha_composite(out, highlight_layer)
    return out


def finish_chip(img: Image.Image, threshold: int = 4) -> Image.Image:
    """The standard finish for every RockPod-original (wave 4+) chip:
    snap_alpha's corner-matte cleanup plus a cel-shading sheen, so flat
    procedural fills read as dimensional game assets instead of flat clip
    art. Use in place of a bare snap_alpha() call for any new chip kind.
    """
    return cel_shade(snap_alpha(img, threshold))


def read_bmp32_alpha(path: Path) -> Image.Image:
    """Read the packager's BI_RGB BGRA while retaining its alpha byte."""
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        return Image.open(path).convert("RGBA")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, raw_height = struct.unpack_from("<ii", data, 18)
    depth = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if width <= 0 or raw_height == 0 or depth != 32 or compression != 0:
        return Image.open(path).convert("RGBA")
    height = abs(raw_height)
    raw = data[offset:offset + width * height * 4]
    return Image.frombytes(
        "RGBA", (width, height), raw, "raw", "BGRA", width * 4,
        -1 if raw_height > 0 else 1,
    )


def build_ipod_exclusive_image(kind: str, out_root: Path) -> Image.Image:
    """Compose one iPod-only chip from transparent YTV-styled source art."""
    generated = IPOD_EXCLUSIVE_SOURCE_DIR / "generated"

    if kind == "earbuds":
        return contain_alpha(Image.open(generated / "earbuds.png"),
                             (104, 108))
    if kind == "moptop":
        return contain_alpha(Image.open(generated / "beatles-moptop.png"),
                             (70, 46))
    if kind == "rockbox-badge":
        return contain_alpha(Image.open(generated / "rockbox-badge.png"),
                             (34, 38))
    if kind == "clickwheel-shell":
        shell = contain_alpha(Image.open(generated / "clickwheel-shell.png"),
                              (68, 68))
        return clear_alpha_ellipse(shell, (21, -7, 47, 14))
    if kind == "beatles-drum":
        canvas = Image.new("RGBA", (92, 52), (0, 0, 0, 0))
        draw = ImageDraw.Draw(canvas)
        draw.ellipse((1, 1, 90, 50), fill=(247, 240, 249, 255),
                     outline=(76, 11, 100, 255), width=4)
        draw.ellipse((6, 5, 85, 46), outline=(255, 112, 11, 255), width=2)
        for x, color in zip(
                (18, 35, 52, 69),
                ((189, 238, 0, 255), (0, 174, 239, 255),
                 (255, 216, 31, 255), (154, 45, 192, 255))):
            draw.ellipse((x - 6, 19, x + 6, 31), fill=color,
                         outline=(42, 8, 55, 255), width=2)
        draw.line((15, 13, 76, 38), fill=(42, 8, 55, 255), width=2)
        draw.line((15, 38, 76, 13), fill=(42, 8, 55, 255), width=2)
        return contain_alpha(canvas, (58, 34))
    if kind == "emma-mic":
        canvas = Image.new("RGBA", (60, 72), (0, 0, 0, 0))
        microphone = contain_alpha(Image.open(generated / "emma-mic.png"),
                                   (54, 66))
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(microphone, (6, 0))
        canvas.alpha_composite(left_hand, (0, 38))
        return canvas
    if kind == "rockbox-clef":
        return contain_alpha(Image.open(generated / "rockbox-badge.png"),
                             (48, 48))
    if kind == "ipod-listener":
        canvas = Image.new("RGBA", (108, 118), (0, 0, 0, 0))
        earbuds = contain_alpha(Image.open(generated / "earbuds.png"),
                                (108, 108))
        ipod = contain_alpha(Image.open(generated / "ipod-prop.png"),
                            (42, 66))
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        midpoint = hands.width // 2
        left_hand = hands.crop((0, 0, midpoint, hands.height))
        right_hand = hands.crop((midpoint, 0, hands.width, hands.height))
        canvas.alpha_composite(earbuds, (0, 0))
        canvas.alpha_composite(ipod, (33, 50))
        canvas.alpha_composite(left_hand, (16, 74))
        canvas.alpha_composite(right_hand, (66, 74))
        return canvas
    if kind == "abbey-disc":
        return contain_alpha(Image.open(generated / "abbey-road-disc.png"),
                             (74, 74))
    if kind == "emma-jacket":
        jacket = contain_alpha(Image.open(generated / "emma-jacket.png"),
                               (84, 72))
        return clear_alpha_ellipse(jacket, (30, 8, 54, 18))
    if kind == "rockbox-halo":
        logo = Image.open(generated / "rockbox-badge.png").convert("RGBA")
        logo.putalpha(logo.getchannel("A").point(
            lambda alpha: alpha * 155 // 255
        ))
        return contain_alpha(logo, (110, 104))
    if kind == "rockpod-classic":
        canvas = Image.new("RGBA", (48, 72), (0, 0, 0, 0))
        ipod = contain_alpha(Image.open(generated / "ipod-prop.png"),
                             (42, 66))
        logo = contain_alpha(Image.open(generated / "rockbox-badge.png"),
                             (18, 18))
        ipod.alpha_composite(logo, (12, 5))
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(ipod, (6, 0))
        canvas.alpha_composite(left_hand, (0, 38))
        return canvas
    if kind == "oliver-hair":
        return contain_alpha(Image.open(generated / "oliver-hair.png"),
                             (76, 56))
    if kind == "oliver-glasses":
        return contain_alpha(Image.open(generated / "oliver-glasses.png"),
                             (44, 18))
    if kind == "oliver-jacket":
        jacket = contain_alpha(Image.open(generated / "oliver-jacket.png"),
                               (84, 72))
        return clear_alpha_ellipse(jacket, (31, 4, 53, 16))
    if kind == "fox-ears":
        return contain_alpha(Image.open(generated / "fox-ears.png"),
                             (72, 50))
    if kind == "hound-ears":
        return contain_alpha(Image.open(generated / "hound-ears.png"),
                             (72, 66))
    if kind == "fox-tail":
        return contain_alpha(Image.open(generated / "fox-tail.png"),
                             (88, 86))
    if kind == "hound-collar":
        collar = contain_alpha(Image.open(generated / "hound-collar.png"),
                               (88, 66))
        return clear_alpha_ellipse(collar, (32, 3, 56, 17))
    if kind == "emma-villains-hair":
        return contain_alpha(
            Image.open(generated / "emma-villains-hair.png"), (74, 58)
        )
    if kind == "emma-villains-tee":
        shirt = contain_alpha(
            Image.open(generated / "emma-villains-tee.png"), (86, 72)
        )
        return clear_alpha_ellipse(shirt, (31, 3, 55, 17))
    if kind == "emma-box-pigtails":
        pigtails = contain_alpha(
            Image.open(generated / "emma-box-pigtails.png"), (100, 72)
        )
        return clear_alpha_ellipse(pigtails, (29, 18, 72, 78))
    if kind == "emma-box-sweater":
        sweater = contain_alpha(
            Image.open(generated / "emma-box-sweater.png"), (92, 76)
        )
        return clear_alpha_ellipse(sweater, (34, 3, 58, 17))
    if kind == "oliver-madly-hair":
        hair = contain_alpha(
            Image.open(generated / "oliver-madly-hair.png"), (88, 114)
        )
        return clear_alpha_ellipse(hair, (27, 22, 61, 78))
    if kind == "oliver-madly-outfit":
        outfit = contain_alpha(
            Image.open(generated / "oliver-madly-outfit.png"), (94, 126)
        )
        return clear_alpha_ellipse(outfit, (34, 0, 60, 22))
    if kind == "oliver-madly-heart":
        return contain_alpha(
            Image.open(generated / "oliver-madly-heart.png"), (146, 132)
        )
    if kind == "earl-tousled-hair":
        hair = contain_alpha(
            Image.open(generated / "earl-tousled-hair.png"), (78, 62)
        )
        return clear_alpha_ellipse(hair, (22, 18, 56, 63))
    if kind == "earl-mustache":
        return contain_alpha(
            Image.open(generated / "earl-mustache.png"), (32, 10)
        )
    if kind == "joy-ponytail":
        hair = contain_alpha(
            Image.open(generated / "joy-ponytail.png"), (100, 94)
        )
        return clear_alpha_ellipse(hair, (29, 31, 71, 95))
    if kind == "earl-plaid-outfit":
        outfit = contain_alpha(
            Image.open(generated / "earl-plaid-outfit.png"), (94, 126)
        )
        return clear_alpha_ellipse(outfit, (33, 0, 61, 23))
    if kind == "karma-list":
        canvas = Image.new("RGBA", (60, 76), (0, 0, 0, 0))
        karma_list = contain_alpha(
            Image.open(generated / "karma-list.png"), (50, 70)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(karma_list, (10, 0))
        canvas.alpha_composite(left_hand, (0, 42))
        return canvas
    if kind == "good-karma-aura":
        return contain_alpha(
            Image.open(generated / "good-karma-aura.png"), (146, 132)
        )
    if kind == "hand-peace-pair":
        return contain_alpha(
            Image.open(generated / "hand-peace-pair.png"), (110, 62)
        )
    if kind == "hand-thumbs-pair":
        return contain_alpha(
            Image.open(generated / "hand-thumbs-pair.png"), (110, 58)
        )
    if kind == "hand-rock-pair":
        return contain_alpha(
            Image.open(generated / "hand-rock-pair.png"), (110, 62)
        )
    if kind == "oliver-turbo-hair":
        hair = contain_alpha(
            Image.open(generated / "oliver-turbo-hair.png"), (76, 58)
        )
        return clear_alpha_ellipse(hair, (22, 20, 54, 59))
    if kind == "oliver-turbo-shades":
        return contain_alpha(
            Image.open(generated / "oliver-turbo-shades.png"), (44, 18)
        )
    if kind == "oliver-turbo-outfit":
        outfit = contain_alpha(
            Image.open(generated / "oliver-turbo-outfit.png"), (96, 126)
        )
        return clear_alpha_ellipse(outfit, (34, 0, 62, 23))
    if kind == "wall-brick-halo":
        return contain_alpha(
            Image.open(generated / "wall-brick-halo.png"), (150, 100)
        )
    if kind == "prism-beam-shades":
        return contain_alpha(
            Image.open(generated / "prism-beam-shades.png"), (46, 18)
        )
    if kind == "hybrid-mech-wings":
        return contain_alpha(
            Image.open(generated / "hybrid-mech-wings.png"), (130, 94)
        )
    if kind == "meteora-spray-hoodie":
        hoodie = contain_alpha(
            Image.open(generated / "meteora-spray-hoodie.png"), (96, 80)
        )
        return clear_alpha_ellipse(hoodie, (34, 0, 62, 24))
    if kind == "riot-scribble-bob":
        hair = contain_alpha(
            Image.open(generated / "riot-scribble-bob.png"), (82, 60)
        )
        return clear_alpha_ellipse(hair, (23, 20, 59, 62))
    if kind == "riot-orange-jacket":
        jacket = contain_alpha(
            Image.open(generated / "riot-orange-jacket.png"), (96, 80)
        )
        return clear_alpha_ellipse(jacket, (34, 0, 62, 24))
    if kind == "sunnyvale-shades":
        return contain_alpha(
            Image.open(generated / "sunnyvale-shades.png"), (46, 20)
        )
    if kind == "trailer-park-work-shirt":
        shirt = contain_alpha(
            Image.open(generated / "trailer-park-work-shirt.png"), (96, 80)
        )
        return clear_alpha_ellipse(shirt, (33, 0, 63, 25))
    if kind == "playground-cap":
        cap = contain_alpha(
            Image.open(generated / "playground-cap.png"), (86, 58)
        )
        return clear_alpha_ellipse(cap, (22, 24, 64, 62))
    if kind == "recess-backpack":
        return contain_alpha(
            Image.open(generated / "recess-backpack.png"), (120, 100)
        )
    if kind == "mall-headphones":
        return contain_alpha(
            Image.open(generated / "mall-headphones.png"), (126, 90)
        )
    if kind == "food-court-hoodie":
        hoodie = contain_alpha(
            Image.open(generated / "food-court-hoodie.png"), (96, 80)
        )
        return clear_alpha_ellipse(hoodie, (34, 0, 62, 24))
    if kind == "beatles-rooftop-shag":
        hair = contain_alpha(
            Image.open(generated / "beatles-rooftop-shag.png"), (82, 60)
        )
        return clear_alpha_ellipse(hair, (23, 21, 59, 62))
    if kind == "beatles-round-glasses":
        return contain_alpha(
            Image.open(generated / "beatles-round-glasses.png"), (46, 18)
        )
    if kind == "beatles-pepper-moustache":
        return contain_alpha(
            Image.open(generated / "beatles-pepper-moustache.png"), (34, 12)
        )
    if kind == "beatles-pepper-jacket":
        jacket = contain_alpha(
            Image.open(generated / "beatles-pepper-jacket.png"), (96, 80)
        )
        return clear_alpha_ellipse(jacket, (34, 0, 62, 24))
    if kind == "beatles-rooftop-coat":
        coat = contain_alpha(
            Image.open(generated / "beatles-rooftop-coat.png"), (96, 80)
        )
        return clear_alpha_ellipse(coat, (34, 0, 62, 24))
    if kind == "beatles-violin-bass":
        canvas = Image.new("RGBA", (78, 84), (0, 0, 0, 0))
        bass = contain_alpha(
            Image.open(generated / "beatles-violin-bass.png"), (68, 78)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        midpoint = hands.width // 2
        left_hand = hands.crop((0, 0, midpoint, hands.height))
        right_hand = hands.crop((midpoint, 0, hands.width, hands.height))
        canvas.alpha_composite(bass, (5, 0))
        canvas.alpha_composite(left_hand, (0, 45))
        canvas.alpha_composite(right_hand, (39, 24))
        return canvas
    if kind == "beatles-submarine-cap":
        cap = contain_alpha(
            Image.open(generated / "beatles-submarine-cap.png"), (86, 58)
        )
        return clear_alpha_ellipse(cap, (22, 24, 64, 62))
    if kind == "beatles-drumsticks":
        canvas = Image.new("RGBA", (108, 68), (0, 0, 0, 0))
        sticks = contain_alpha(
            Image.open(generated / "beatles-drumsticks.png"), (66, 54)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        midpoint = hands.width // 2
        left_hand = hands.crop((0, 0, midpoint, hands.height))
        right_hand = hands.crop((midpoint, 0, hands.width, hands.height))
        canvas.alpha_composite(sticks, (21, 0))
        canvas.alpha_composite(left_hand, (16, 37))
        canvas.alpha_composite(right_hand, (62, 37))
        return canvas
    if kind == "pokemon-trainer-cap":
        cap = contain_alpha(
            Image.open(generated / "pokemon-trainer-cap.png"), (86, 58)
        )
        return clear_alpha_ellipse(cap, (22, 24, 64, 62))
    if kind in ("pokemon-poke-ball", "pokemon-great-ball"):
        canvas = Image.new("RGBA", (52, 62), (0, 0, 0, 0))
        filename = "pokemon-poke-ball.png" \
            if kind == "pokemon-poke-ball" else "pokemon-great-ball.png"
        ball = contain_alpha(Image.open(generated / filename), (36, 36))
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(ball, (16, 0))
        canvas.alpha_composite(left_hand, (0, 34))
        return canvas
    if kind == "pokemongo-field-jacket":
        jacket = contain_alpha(
            Image.open(generated / "pokemongo-field-jacket.png"), (96, 80)
        )
        return clear_alpha_ellipse(jacket, (34, 0, 62, 24))
    if kind == "pokemongo-research-coat":
        coat = contain_alpha(
            Image.open(generated / "pokemongo-research-coat.png"), (96, 80)
        )
        return clear_alpha_ellipse(coat, (34, 0, 62, 24))
    if kind in ("pokemongo-raid-pass", "pokemon-pocket-dex"):
        canvas = Image.new("RGBA", (62, 76), (0, 0, 0, 0))
        filename = "pokemongo-raid-pass.png" \
            if kind == "pokemongo-raid-pass" else "pokemon-pocket-dex.png"
        prop = contain_alpha(Image.open(generated / filename), (52, 70))
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(prop, (10, 0))
        canvas.alpha_composite(left_hand, (0, 42))
        return canvas
    if kind == "pokemongo-lure-halo":
        return contain_alpha(
            Image.open(generated / "pokemongo-lure-halo.png"), (150, 132)
        )
    if kind == "pokemongo-ar-visor":
        return contain_alpha(
            Image.open(generated / "pokemongo-ar-visor.png"), (48, 18)
        )
    if kind == "pokemon-rocket-uniform":
        uniform = contain_alpha(
            Image.open(generated / "pokemon-rocket-uniform.png"), (96, 80)
        )
        return clear_alpha_ellipse(uniform, (34, 0, 62, 24))
    if kind == "pokemongo-map-aura":
        return contain_alpha(
            Image.open(generated / "pokemongo-map-aura.png"), (150, 120)
        )
    if kind == "dive-mask-visor":
        return snap_alpha(contain_alpha(
            Image.open(generated / "dive-mask-visor.png"), (48, 22)
        ))
    if kind == "fry-basket-fins":
        return snap_alpha(contain_alpha(
            Image.open(generated / "fry-basket-fins.png"), (112, 52)
        ))
    if kind == "drive-thru-wetsuit":
        wetsuit = contain_alpha(
            Image.open(generated / "drive-thru-wetsuit.png"), (96, 80)
        )
        return snap_alpha(clear_alpha_ellipse(wetsuit, (34, 0, 62, 24)))
    if kind == "bubble-trail-aura":
        return snap_alpha(contain_alpha(
            Image.open(generated / "bubble-trail-aura.png"), (144, 112)
        ))
    if kind == "doll-space-buns":
        buns = contain_alpha(
            Image.open(generated / "doll-space-buns.png"), (100, 74)
        )
        return snap_alpha(clear_alpha_ellipse(buns, (30, 18, 70, 76)))
    if kind == "cracked-doll-eyes":
        return snap_alpha(contain_alpha(
            Image.open(generated / "cracked-doll-eyes.png"), (48, 20)
        ))
    if kind == "dollhouse-pinafore":
        pinafore = contain_alpha(
            Image.open(generated / "dollhouse-pinafore.png"), (92, 76)
        )
        return snap_alpha(clear_alpha_ellipse(pinafore, (32, 3, 60, 17)))
    if kind == "porcelain-crack-halo":
        return snap_alpha(contain_alpha(
            Image.open(generated / "porcelain-crack-halo.png"), (140, 120)
        ))
    if kind == "polaroid-frame-backdrop":
        return snap_alpha(contain_alpha(
            Image.open(generated / "polaroid-frame-backdrop.png"), (160, 140)
        ))
    if kind == "flash-pop-shades":
        return snap_alpha(contain_alpha(
            Image.open(generated / "flash-pop-shades.png"), (46, 18)
        ))
    if kind == "instant-print-fan":
        canvas = Image.new("RGBA", (60, 74), (0, 0, 0, 0))
        fan = contain_alpha(
            Image.open(generated / "instant-print-fan.png"), (50, 70)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(fan, (10, 0))
        canvas.alpha_composite(left_hand, (0, 44))
        return snap_alpha(canvas)
    if kind == "hero-cap":
        cap = contain_alpha(Image.open(generated / "hero-cap.png"), (84, 56))
        return snap_alpha(clear_alpha_ellipse(cap, (22, 24, 62, 60)))
    if kind == "hero-tunic":
        tunic = contain_alpha(
            Image.open(generated / "hero-tunic.png"), (96, 80)
        )
        return snap_alpha(clear_alpha_ellipse(tunic, (34, 0, 62, 24)))
    if kind == "hylian-shield":
        canvas = Image.new("RGBA", (62, 76), (0, 0, 0, 0))
        shield = contain_alpha(
            Image.open(generated / "hylian-shield.png"), (52, 70)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(shield, (8, 0))
        canvas.alpha_composite(left_hand, (0, 44))
        return snap_alpha(canvas)
    if kind == "forest-eye-mask":
        return snap_alpha(contain_alpha(
            Image.open(generated / "forest-eye-mask.png"), (46, 20)
        ))
    if kind == "ocarina-charm":
        canvas = Image.new("RGBA", (58, 66), (0, 0, 0, 0))
        ocarina = contain_alpha(
            Image.open(generated / "ocarina-charm.png"), (46, 54)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(ocarina, (10, 0))
        canvas.alpha_composite(left_hand, (0, 36))
        return snap_alpha(canvas)
    if kind == "heart-container":
        canvas = Image.new("RGBA", (54, 78), (0, 0, 0, 0))
        heart = contain_alpha(
            Image.open(generated / "heart-container.png"), (44, 58)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(heart, (8, 0))
        canvas.alpha_composite(left_hand, (0, 40))
        return snap_alpha(canvas)
    if kind == "triforce-halo":
        return snap_alpha(contain_alpha(
            Image.open(generated / "triforce-halo.png"), (140, 96)
        ))
    if kind == "fairy-companion":
        return snap_alpha(contain_alpha(
            Image.open(generated / "fairy-companion.png"), (100, 86)
        ))
    if kind == "hasan-cap":
        return finish_chip(contain_alpha(
            Image.open(generated / "hasan-cap.png"), (84, 58)
        ))
    if kind == "hasan-headset":
        canvas = Image.new("RGBA", (60, 74), (0, 0, 0, 0))
        headset = contain_alpha(
            Image.open(generated / "hasan-headset.png"), (54, 66)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(headset, (4, 0))
        canvas.alpha_composite(left_hand, (0, 40))
        return finish_chip(canvas)
    if kind == "hasan-hoodie":
        hoodie = contain_alpha(
            Image.open(generated / "hasan-hoodie.png"), (96, 80)
        )
        return finish_chip(clear_alpha_ellipse(hoodie, (34, 0, 62, 24)))
    if kind == "hasan-news-aura":
        return finish_chip(contain_alpha(
            Image.open(generated / "hasan-news-aura.png"), (150, 90)
        ))
    if kind == "qtc-ponytail":
        hair = contain_alpha(
            Image.open(generated / "qtc-ponytail.png"), (100, 94)
        )
        return finish_chip(clear_alpha_ellipse(hair, (29, 31, 71, 95)))
    if kind == "qtc-visor":
        return finish_chip(contain_alpha(
            Image.open(generated / "qtc-visor.png"), (46, 20)
        ))
    if kind == "qtc-trophy":
        canvas = Image.new("RGBA", (58, 76), (0, 0, 0, 0))
        trophy = contain_alpha(
            Image.open(generated / "qtc-trophy.png"), (46, 58)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(trophy, (6, 0))
        canvas.alpha_composite(left_hand, (0, 42))
        return finish_chip(canvas)
    if kind == "qtc-spotlight-aura":
        return finish_chip(contain_alpha(
            Image.open(generated / "qtc-spotlight-aura.png"), (140, 110)
        ))
    if kind == "maya-ranger-hat":
        return finish_chip(contain_alpha(
            Image.open(generated / "maya-ranger-hat.png"), (88, 52)
        ))
    if kind == "maya-falcon-glove":
        return finish_chip(contain_alpha(
            Image.open(generated / "maya-falcon-glove.png"), (110, 64)
        ))
    if kind == "maya-field-vest":
        vest = contain_alpha(
            Image.open(generated / "maya-field-vest.png"), (96, 80)
        )
        return finish_chip(clear_alpha_ellipse(vest, (34, 0, 62, 24)))
    if kind == "maya-forest-aura":
        return finish_chip(contain_alpha(
            Image.open(generated / "maya-forest-aura.png"), (150, 100)
        ))
    if kind == "habs-home-jersey":
        jersey = contain_alpha(
            Image.open(generated / "habs-home-jersey.png"), (96, 80)
        )
        return finish_chip(clear_alpha_ellipse(jersey, (34, 0, 62, 24)))
    if kind == "habs-hockey-stick":
        return finish_chip(contain_alpha(
            Image.open(generated / "habs-hockey-stick.png"), (100, 90)
        ))
    if kind == "habs-winter-toque":
        return finish_chip(contain_alpha(
            Image.open(generated / "habs-winter-toque.png"), (84, 58)
        ))
    if kind == "habs-rink-aura":
        return finish_chip(contain_alpha(
            Image.open(generated / "habs-rink-aura.png"), (150, 100)
        ))
    if kind == "spyro-companion":
        return finish_chip(contain_alpha(
            Image.open(generated / "spyro-companion.png"), (60, 48)
        ))
    if kind == "cat-companion":
        return finish_chip(contain_alpha(
            Image.open(generated / "cat-companion.png"), (50, 40)
        ))
    if kind == "dog-companion":
        return finish_chip(contain_alpha(
            Image.open(generated / "dog-companion.png"), (54, 42)
        ))
    if kind == "turtle-companion":
        return finish_chip(contain_alpha(
            Image.open(generated / "turtle-companion.png"), (52, 34)
        ))
    if kind == "navi-companion":
        return finish_chip(contain_alpha(
            Image.open(generated / "navi-companion.png"), (40, 40)
        ))
    if kind == "pink-floyd-pig":
        return finish_chip(contain_alpha(
            Image.open(generated / "pink-floyd-pig.png"), (140, 90)
        ))
    if kind == "pink-floyd-prism":
        return finish_chip(contain_alpha(
            Image.open(generated / "pink-floyd-prism.png"), (150, 100)
        ))
    if kind == "pink-floyd-hammers":
        return finish_chip(contain_alpha(
            Image.open(generated / "pink-floyd-hammers.png"), (64, 64)
        ))
    if kind == "portal-gun":
        canvas = Image.new("RGBA", (70, 60), (0, 0, 0, 0))
        gun = contain_alpha(Image.open(generated / "portal-gun.png"),
                            (60, 50))
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(gun, (6, 0))
        canvas.alpha_composite(left_hand, (2, 32))
        return finish_chip(canvas)
    if kind == "companion-cube":
        return finish_chip(contain_alpha(
            Image.open(generated / "companion-cube.png"), (56, 56)
        ))
    if kind == "long-fall-boots":
        return finish_chip(contain_alpha(
            Image.open(generated / "long-fall-boots.png"), (110, 60)
        ))
    if kind == "aperture-turret":
        return finish_chip(contain_alpha(
            Image.open(generated / "aperture-turret.png"), (54, 54)
        ))
    if kind == "osrs-rune-platebody":
        armor = contain_alpha(
            Image.open(generated / "osrs-rune-platebody.png"), (96, 80)
        )
        return finish_chip(clear_alpha_ellipse(armor, (34, 0, 62, 24)))
    if kind == "osrs-dragon-platebody":
        armor = contain_alpha(
            Image.open(generated / "osrs-dragon-platebody.png"), (96, 80)
        )
        return finish_chip(clear_alpha_ellipse(armor, (34, 0, 62, 24)))
    if kind == "osrs-godsword":
        return finish_chip(contain_alpha(
            Image.open(generated / "osrs-godsword.png"), (100, 90)
        ))
    if kind == "osrs-abyssal-whip":
        return finish_chip(contain_alpha(
            Image.open(generated / "osrs-abyssal-whip.png"), (60, 80)
        ))
    if kind == "osrs-dragon-claws":
        return finish_chip(contain_alpha(
            Image.open(generated / "osrs-dragon-claws.png"), (110, 64)
        ))
    if kind == "osrs-party-hat":
        return finish_chip(contain_alpha(
            Image.open(generated / "osrs-party-hat.png"), (70, 60)
        ))
    if kind == "osrs-santa-hat":
        return finish_chip(contain_alpha(
            Image.open(generated / "osrs-santa-hat.png"), (76, 66)
        ))
    if kind == "osrs-fire-cape":
        cape = contain_alpha(
            Image.open(generated / "osrs-fire-cape.png"), (96, 80)
        )
        return finish_chip(clear_alpha_ellipse(cape, (34, 0, 62, 24)))
    if kind == "osrs-max-cape":
        cape = contain_alpha(
            Image.open(generated / "osrs-max-cape.png"), (96, 80)
        )
        return finish_chip(clear_alpha_ellipse(cape, (34, 0, 62, 24)))
    if kind == "osrs-twisted-bow":
        return finish_chip(contain_alpha(
            Image.open(generated / "osrs-twisted-bow.png"), (60, 90)
        ))
    if kind == "osrs-dragon-scimitar":
        return finish_chip(contain_alpha(
            Image.open(generated / "osrs-dragon-scimitar.png"), (90, 80)
        ))
    if kind == "osrs-amulet-of-fury":
        return finish_chip(contain_alpha(
            Image.open(generated / "osrs-amulet-of-fury.png"), (40, 45)
        ))
    if kind == "osrs-barrows-gloves":
        return finish_chip(contain_alpha(
            Image.open(generated / "osrs-barrows-gloves.png"), (110, 64)
        ))
    if kind == "polaroid-camera":
        canvas = Image.new("RGBA", (60, 74), (0, 0, 0, 0))
        camera = contain_alpha(
            Image.open(generated / "polaroid-camera.png"), (54, 60)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(camera, (4, 0))
        canvas.alpha_composite(left_hand, (0, 42))
        return finish_chip(canvas)
    if kind == "photo-strip":
        return finish_chip(contain_alpha(
            Image.open(generated / "photo-strip.png"), (40, 70)
        ))
    if kind == "retro-filmstrip":
        return finish_chip(contain_alpha(
            Image.open(generated / "retro-filmstrip.png"), (150, 100)
        ))
    if kind == "ki-ultra-combo":
        return finish_chip(contain_alpha(
            Image.open(generated / "ki-ultra-combo.png"), (150, 100)
        ))
    if kind == "ki-ninja-visor":
        return finish_chip(contain_alpha(
            Image.open(generated / "ki-ninja-visor.png"), (46, 20)
        ))
    if kind == "ki-energy-blades":
        return finish_chip(contain_alpha(
            Image.open(generated / "ki-energy-blades.png"), (110, 70)
        ))
    if kind == "cyberpunk-neon-visor":
        return finish_chip(contain_alpha(
            Image.open(generated / "cyberpunk-neon-visor.png"), (46, 20)
        ))
    if kind == "cyberpunk-cyberarm":
        return finish_chip(contain_alpha(
            Image.open(generated / "cyberpunk-cyberarm.png"), (60, 90)
        ))
    if kind == "cyberpunk-mantis-blades":
        return finish_chip(contain_alpha(
            Image.open(generated / "cyberpunk-mantis-blades.png"), (70, 60)
        ))
    if kind == "cyberpunk-night-city":
        return finish_chip(contain_alpha(
            Image.open(generated / "cyberpunk-night-city.png"), (150, 100)
        ))
    if kind == "silverhand-jacket":
        jacket = contain_alpha(
            Image.open(generated / "silverhand-jacket.png"), (96, 80)
        )
        return finish_chip(clear_alpha_ellipse(jacket, (34, 0, 62, 24)))
    if kind == "chrome-rock-arm":
        return finish_chip(contain_alpha(
            Image.open(generated / "chrome-rock-arm.png"), (60, 90)
        ))
    if kind == "aviator-shades":
        return finish_chip(contain_alpha(
            Image.open(generated / "aviator-shades.png"), (46, 20)
        ))
    if kind == "rockerboy-guitar":
        canvas = Image.new("RGBA", (70, 90), (0, 0, 0, 0))
        guitar = contain_alpha(
            Image.open(generated / "rockerboy-guitar.png"), (60, 84)
        )
        hands = read_bmp32_alpha(out_root / "chips" / "0406.bmp")
        hands, _, _ = trim(hands)
        left_hand = hands.crop((0, 0, hands.width // 2, hands.height))
        canvas.alpha_composite(guitar, (4, 0))
        canvas.alpha_composite(left_hand, (0, 56))
        return finish_chip(canvas)
    if kind == "judy-twin-braids":
        hair = contain_alpha(
            Image.open(generated / "judy-twin-braids.png"), (100, 90)
        )
        return finish_chip(clear_alpha_ellipse(hair, (29, 4, 71, 50)))
    if kind == "welding-goggles":
        return finish_chip(contain_alpha(
            Image.open(generated / "welding-goggles.png"), (46, 22)
        ))
    if kind == "mechanic-overalls":
        overalls = contain_alpha(
            Image.open(generated / "mechanic-overalls.png"), (96, 80)
        )
        return finish_chip(clear_alpha_ellipse(overalls, (34, 0, 62, 24)))
    if kind == "spinal-skull-mask":
        return finish_chip(contain_alpha(
            Image.open(generated / "spinal-skull-mask.png"), (40, 28)
        ))
    if kind == "pirate-bandana":
        return finish_chip(contain_alpha(
            Image.open(generated / "pirate-bandana.png"), (84, 58)
        ))
    if kind == "twin-cutlasses":
        return finish_chip(contain_alpha(
            Image.open(generated / "twin-cutlasses.png"), (110, 70)
        ))
    if kind == "ghost-flame-aura":
        return finish_chip(contain_alpha(
            Image.open(generated / "ghost-flame-aura.png"), (150, 100)
        ))
    raise ValueError(f"unknown iPod-exclusive composition: {kind}")


def place_sprite(part: PrefabPart, meta: SpriteMeta, scale: float,
                 body_ppu: float) -> tuple[Image.Image, float, float]:
    """Return a prefab sprite plus its stage-space top-left position.

    Unity pivots are measured from the sprite's bottom-left.  Treating every
    sprite as centre-pivoted clipped the Sitekick's curved antenna and also
    displaced a number of arm/leg chips.
    """
    img = Image.open(meta.path).convert("RGBA")
    if part.flip[0]:
        img = img.transpose(Image.FLIP_LEFT_RIGHT)
    if part.flip[1]:
        img = img.transpose(Image.FLIP_TOP_BOTTOM)
    sw = max(1, round(img.width * scale * abs(part.scale[0])
                      * (body_ppu / meta.ppu)))
    sh = max(1, round(img.height * scale * abs(part.scale[1])
                      * (body_ppu / meta.ppu)))
    img = img.resize((sw, sh), Image.LANCZOS)

    world_x = part.pos[0] * body_ppu * scale
    world_y = -part.pos[1] * body_ppu * scale
    pivot = (
        1.0 - meta.pivot[0] if part.flip[0] else meta.pivot[0],
        1.0 - meta.pivot[1] if part.flip[1] else meta.pivot[1],
    )
    pivot_x = pivot[0] * sw
    pivot_y = (1.0 - pivot[1]) * sh
    return img, world_x - pivot_x, world_y - pivot_y


def write_bmp32(path: Path, img: Image.Image) -> None:
    """Write a bottom-up 32bpp BI_RGB BGRA BMP.

    Rockbox's reader (apps/recorder/bmp.c) takes BI_RGB 32bpp in BGRA order
    and derives a 4bpp alpha plane from it, which is what FORMAT_TRANSPARENT
    blits need.
    """
    img = img.convert("RGBA")
    w, h = img.size
    rows = []
    px = img.load()
    for y in range(h - 1, -1, -1):
        row = bytearray()
        for x in range(w):
            r, g, b, a = px[x, y]
            row += bytes((b, g, r, a))
        rows.append(bytes(row))
    pixels = b"".join(rows)
    header = struct.pack("<2sIHHI", b"BM", 14 + 40 + len(pixels), 0, 0, 14 + 40)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 32, 0, len(pixels),
                       2835, 2835, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + info + pixels)


def write_rga(path: Path, img: Image.Image) -> None:
    """Write Desktop Mode's RGB565 plus 8-bit coverage icon format."""
    rgba = img.convert("RGBA")
    payload = bytearray(struct.pack("<4sHH", b"RGA1", *rgba.size))
    for red, green, blue, alpha in rgba.getdata():
        value = (
            ((red & 0xF8) << 8)
            | ((green & 0xFC) << 3)
            | (blue >> 3)
        )
        payload.extend(struct.pack("<HB", value, alpha))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)


def desktop_icon(base: Image.Image, logo: Image.Image, size: int) -> Image.Image:
    """Compose the real Sitekick robot into a Dock-readable YTV app tile."""
    scale = 4
    work_size = size * scale
    icon = Image.new("RGBA", (work_size, work_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(icon)
    radius = max(4, work_size // 5)
    border = max(2, work_size // 24)
    draw.rounded_rectangle(
        (border, border, work_size - border - 1, work_size - border - 1),
        radius=radius,
        fill=(132, 218, 18, 255),
        outline=(65, 6, 82, 255),
        width=border,
    )
    grid = max(8, work_size // 8)
    for position in range(grid, work_size, grid):
        draw.line(
            (border, position, work_size - border, position),
            fill=(91, 183, 17, 110),
            width=max(1, scale),
        )
        draw.line(
            (position, border, position, work_size - border),
            fill=(91, 183, 17, 110),
            width=max(1, scale),
        )

    bbox = base.getbbox()
    if bbox:
        robot = base.crop(bbox)
        robot.thumbnail(
            (work_size * 7 // 10, work_size * 7 // 10),
            Image.Resampling.LANCZOS,
        )
        icon.alpha_composite(
            robot,
            ((work_size - robot.width) // 2,
             work_size - robot.height - work_size // 12),
        )

    badge = logo.copy()
    badge.thumbnail(
        (work_size * 3 // 10, work_size * 3 // 10),
        Image.Resampling.LANCZOS,
    )
    icon.alpha_composite(badge, (work_size // 14, work_size // 14))
    return icon.resize((size, size), Image.Resampling.LANCZOS)


def load_wav_mono(path: Path) -> tuple["object", int]:
    """Read a mono/stereo 16-bit WAV as float samples in [-1, 1]."""
    import wave

    import numpy as np

    with wave.open(str(path)) as wav:
        if wav.getsampwidth() != 2:
            raise ValueError(f"{path.name}: expected 16-bit PCM")
        rate = wav.getframerate()
        channels = wav.getnchannels()
        raw = wav.readframes(wav.getnframes())
    data = np.frombuffer(raw, dtype="<i2").astype("float32") / 32768.0
    if channels > 1:
        data = data.reshape(-1, channels).mean(axis=1)
    return data, rate


def resample_to_stereo(samples, src_rate: int, dst_rate: int,
                       max_seconds: float) -> bytes:
    """Resample, trim and fade a mono effect into interleaved stereo PCM."""
    import numpy as np

    limit = int(src_rate * max_seconds)
    if len(samples) > limit:
        samples = samples[:limit]
    if len(samples) == 0:
        return b""

    if src_rate != dst_rate:
        count = max(1, int(round(len(samples) * dst_rate / float(src_rate))))
        src_idx = np.linspace(0.0, len(samples) - 1, count, dtype="float64")
        samples = np.interp(src_idx, np.arange(len(samples)), samples)

    # Truncation leaves a discontinuity; a short fade keeps it from clicking.
    fade = min(len(samples), max(1, int(dst_rate * 0.008)))
    if fade > 1:
        samples = samples.copy()
        samples[-fade:] *= np.linspace(1.0, 0.0, fade)

    peak = float(np.max(np.abs(samples))) if len(samples) else 0.0
    if peak > 0.0:
        samples = samples * (0.89 / peak)

    pcm = np.clip(samples * 32767.0, -32768, 32767).astype("<i2")
    return np.repeat(pcm, 2).tobytes()          # mono -> L/R


def encode_ui_bank(rate: int, effects: dict[str, bytes]) -> bytes:
    """Build a UIB1 bank; byte-compatible with the achievements loader."""
    names = tuple(name for name, _, _ in UI_SOUNDS)
    header_size = 12 + len(names) * 12
    payload = bytearray()
    entries = []
    for name in names:
        pcm = bytes(effects.get(name, b""))
        if len(pcm) % 4:
            raise ValueError(f"{name} PCM is not stereo-frame aligned")
        entries.append((header_size + len(payload), len(pcm)))
        payload += pcm
    out = bytearray(struct.pack("<4sIHH", b"UIB1", rate, len(names), 0))
    for index, (offset, size) in enumerate(entries):
        out += struct.pack("<III", index, offset, size)
    out += payload
    if len(out) > SOUND_BUDGET:
        raise ValueError(f"UI bank for {rate} Hz is {len(out)} bytes, over "
                         f"the {SOUND_BUDGET} byte budget")
    return bytes(out)


def build_sound_banks(art_root: Path, out_root: Path,
                      manifest: list[tuple[str, str]]) -> list[str]:
    """Emit ui-44100.uib and ui-48000.uib from the original button sounds."""
    src_dir = art_root / OOZE_SOUND_DIR
    if not src_dir.is_dir():
        return [f"sound source directory missing: {src_dir}"]

    loaded = {}
    notes = []
    for name, filename, seconds in UI_SOUNDS:
        path = src_dir / filename
        if not path.exists():
            notes.append(f"missing UI sound {filename}")
            continue
        try:
            loaded[name] = (*load_wav_mono(path), seconds)
        except Exception as exc:                      # noqa: BLE001
            notes.append(f"{filename}: {exc}")
            continue
        manifest.append((f"sounds/{name}", sha256_of(path)))

    sound_dir = out_root / "sounds"
    sound_dir.mkdir(parents=True, exist_ok=True)
    for rate in (44100, 48000):
        effects = {}
        for name, (samples, src_rate, seconds) in loaded.items():
            effects[name] = resample_to_stereo(samples, src_rate, rate,
                                               seconds)
        bank = encode_ui_bank(rate, effects)
        (sound_dir / f"ui-{rate}.uib").write_bytes(bank)
        notes.append(f"ui-{rate}.uib: {len(bank)} bytes")
    return notes


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for block in iter(lambda: fh.read(65536), b""):
            h.update(block)
    return h.hexdigest()


# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------

def collect_chips(art_root: Path, sprites: dict[str, SpriteMeta]) -> dict[int, ChipRecord]:
    chips: dict[int, ChipRecord] = {}
    for asset in art_root.rglob("Chip_[0-9]*.asset"):
        m = re.match(r"Chip_(\d+)\.asset$", asset.name)
        if not m:
            continue
        cid = int(m.group(1))
        rec = chips.setdefault(cid, ChipRecord(cid=cid))
        chip_dir = asset.parent

        icon = chip_dir / "Sprites" / f"chip_{cid:04d}.png"
        if icon.exists():
            rec.icon = icon

        prefab = chip_dir / f"Chip_{cid:04d}_Effect.prefab"
        if prefab.exists():
            parts = parse_prefab(prefab)
            # Prefer the front/right facing when a chip ships several.
            keep = [p for p in parts if not p.name.endswith("_left")]
            rec.parts = keep or parts

        if not rec.parts:
            # Fall back to any base sprite present on disk.
            for cand in sorted((chip_dir / "Sprites").glob(f"chip_{cid:04d}_base*.png")
                               if (chip_dir / "Sprites").exists() else []):
                guid = next((g for g, s in sprites.items() if s.path == cand), None)
                if guid:
                    rec.parts = [PrefabPart("", (0.0, 0.035), (1.0, 1.0), 0, guid)]
                    break
    return chips


def build(art_root: Path, out_root: Path) -> int:
    sprites = index_sprites(art_root)
    if not sprites:
        sys.exit(f"no Unity sprite metadata under {art_root}")

    by_name = {s.path.stem: s for s in sprites.values()}
    manifest: list[tuple[str, str]] = []

    body_meta = by_name.get(BODY_SPRITE)
    if not body_meta:
        sys.exit("sitekick_body.png not found; is --art-root a clone of Art?")
    body_img = Image.open(body_meta.path).convert("RGBA")
    scale = BODY_TARGET_H / body_img.height
    ppu = body_meta.ppu

    # --- complete base Sitekick -----------------------------------------
    base_dir = out_root / "base"
    base_rows = []
    for slot, name in BASE_PARTS:
        meta = by_name.get(name)
        if not meta:
            continue
        img = Image.open(meta.path).convert("RGBA")
        trimmed, ox, oy = trim(img)
        tw = max(1, round(trimmed.width * scale))
        th = max(1, round(trimmed.height * scale))
        small = trimmed.resize((tw, th), Image.LANCZOS)
        write_bmp32(base_dir / f"{slot}.bmp", small)
        manifest.append((f"base/{slot}.bmp", sha256_of(meta.path)))
        # Offset of the trimmed sprite's centre from the full sprite's pivot,
        # expressed in stage pixels.
        cx = (ox + trimmed.width / 2) - img.width / 2
        cy = (oy + trimmed.height / 2) - img.height / 2
        base_rows.append((slot, tw, th, round(cx * scale), round(cy * scale)))

    base_prefab = art_root / "Unity/Assets/Sitekick/EditorSitekick.prefab"
    base_canvas = Image.new("RGBA", (STAGE_W, STAGE_H), (0, 0, 0, 0))
    base_parts = parse_prefab(base_prefab)
    if not base_parts:
        sys.exit(f"could not read base Sitekick prefab: {base_prefab}")

    # The antenna lives in a SortingGroup at -1 in the preserved prefab.
    # parse_prefab intentionally deals in SpriteRenderers, so restore that
    # one group ordering by its authored child names.
    def base_order(part: PrefabPart) -> int:
        return -1 if part.name.lower() in ("stick", "knob", "base") \
            else part.order

    def compose_base(body_tint=None):
        canvas = Image.new("RGBA", (STAGE_W, STAGE_H), (0, 0, 0, 0))
        for part in sorted(base_parts, key=base_order):
            meta = sprites.get(part.guid)
            if not meta or not meta.path.exists():
                continue
            img, left, top = place_sprite(part, meta, scale, ppu)
            if body_tint is not None and meta.path.stem == BODY_SPRITE:
                img = tint_body(img, body_tint)
            canvas.alpha_composite(
                img, (round(BODY_ORIGIN_X + left),
                      round(BODY_ORIGIN_Y + top)))
        return canvas

    base_canvas = compose_base()
    write_bmp32(base_dir / "sitekick.bmp", base_canvas)
    manifest.append(("base/sitekick.bmp", sha256_of(base_prefab)))
    for index, color in enumerate(BODY_TINTS[1:], start=1):
        write_bmp32(
            base_dir / f"sitekick-color-{index}.bmp",
            compose_base(color),
        )
        manifest.append(
            (f"base/sitekick-color-{index}.bmp", sha256_of(base_prefab))
        )

    if YTV_LOGO.is_file():
        full_logo = Image.open(YTV_LOGO).convert("RGBA")
        logo = full_logo.copy()
        logo.thumbnail((28, 28), Image.LANCZOS)
        write_bmp32(base_dir / "ytv-logo.bmp", logo)
        manifest.append(("base/ytv-logo.bmp", sha256_of(YTV_LOGO)))

        desktop_dir = out_root / "desktop"
        for size, name in (
            (32, "icon.32x32.rga"),
            (34, "icon-dock-34.34x34.rga"),
            (38, "icon-dock-38.38x38.rga"),
            (64, "icon.64x64.rga"),
            (66, "icon-dock-66.66x66.rga"),
            (70, "icon-dock-70.70x70.rga"),
        ):
            write_rga(
                desktop_dir / name,
                desktop_icon(base_canvas, full_logo, size),
            )
            manifest.append(
                (f"desktop/{name}", sha256_of(base_prefab))
            )

    # Authentic Sitekick Remastered backgrounds, combined with the preserved
    # 2003 YTV wordmark. Templates are shared by the plugin and RockPod.
    background_dir = out_root / "backgrounds"
    pane_templates = []
    for index, (name, filename) in enumerate(SITEKICK_BACKGROUNDS):
        source = BACKGROUND_SOURCE_DIR / filename
        if not source.is_file():
            sys.exit(f"missing preserved Sitekick background: {source}")
        background = Image.open(source).convert("RGBA")
        stage_background = cover_image(background, (STAGE_W, STAGE_H))
        pane_background = cover_image(background, (174, 240))
        pane_draw = ImageDraw.Draw(pane_background)
        pane_draw.rectangle((0, 0, 173, 31), fill=(76, 11, 100, 255))
        pane_draw.rectangle((0, 31, 173, 35), fill=(255, 112, 11, 255))
        pane_draw.rounded_rectangle(
            (8, 190, 165, 230), radius=6,
            fill=(247, 240, 249, 235), outline=(76, 11, 100, 255),
            width=2,
        )
        if YTV_LOGO.is_file():
            preview_logo = Image.open(YTV_LOGO).convert("RGBA")
            preview_logo.thumbnail((42, 25), Image.LANCZOS)
            pane_background.alpha_composite(preview_logo, (7, 3))
        write_bmp32(background_dir / f"stage-{index}.bmp",
                    stage_background)
        write_bmp32(background_dir / f"pane-{index}.bmp",
                    pane_background)
        manifest.append(
            (f"backgrounds/stage-{index}.bmp", sha256_of(source))
        )
        manifest.append(
            (f"backgrounds/pane-{index}.bmp", sha256_of(source))
        )
        pane_templates.append(pane_background)

    # Default Applications/right-pane card. RockPod or the plugin replaces
    # this with the player's current loadout whenever state changes.
    pane = pane_templates[0].copy()
    preview_draw = ImageDraw.Draw(pane)
    bounds = base_canvas.getbbox()
    character = base_canvas.crop(bounds) if bounds else base_canvas
    character.thumbnail((148, 129), Image.LANCZOS)
    floating = Image.new("RGBA", (154, 139), (255, 0, 255, 0))
    floating.alpha_composite(
        character,
        ((154 - character.width) // 2, (139 - character.height) // 2),
    )
    preview_draw.text((16, 197), "XP 0", fill=(42, 8, 55, 255))
    preview_draw.text((16, 213), "COINS 250", fill=(42, 8, 55, 255))
    preview = pane.copy()
    preview.alpha_composite(floating, (10, 40))
    write_bmp32(out_root / "preview" / "pane-background.bmp", pane)
    write_bmp32(out_root / "preview" / "current-float.bmp", floating)
    write_bmp32(out_root / "preview" / "current.bmp", preview)

    body_w, body_h = next(((w, h) for slot, w, h, _, _ in base_rows
                           if slot == "body"), (121, BODY_TARGET_H))

    # --- chips -----------------------------------------------------------
    chips = collect_chips(art_root, sprites)
    usable: list[ChipRecord] = []
    for cid, rec in sorted(chips.items()):
        if not rec.parts and rec.icon is None:
            continue
        if rec.parts:
            xs = [p.pos[0] for p in rec.parts]
            ys = [p.pos[1] for p in rec.parts]
            rec.anchor = (sum(xs) / len(xs), sum(ys) / len(ys))
            rec.z = max(p.order for p in rec.parts)
        rec.slot = "none"          # resolved once the composite exists
        usable.append(rec)

    chip_dir = out_root / "chips"
    icon_tiles: list[tuple[int, Image.Image]] = []
    rows = []
    for rec in usable:
        composed = None
        if rec.parts:
            # Compose the chip's own sprites into one stage-space bitmap.
            placed = []
            for p in sorted(rec.parts, key=lambda p: p.order):
                meta = sprites.get(p.guid)
                if not meta or not meta.path.exists():
                    continue
                try:
                    img, left, top = place_sprite(p, meta, scale, ppu)
                except OSError:
                    continue
                placed.append((img, left, top))
            if placed:
                left = min(px for im, px, py in placed)
                top = min(py for im, px, py in placed)
                right = max(px + im.width for im, px, py in placed)
                bottom = max(py + im.height for im, px, py in placed)
                cw = max(1, min(STAGE_W * 2, round(right - left)))
                ch = max(1, min(STAGE_H * 2, round(bottom - top)))
                composed = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
                for im, px, py in placed:
                    composed.alpha_composite(
                        im,
                        (round(px - left), round(py - top)))
                # Anchor = offset of the composite's top-left from body centre.
                ax, ay = round(left), round(top)
                # Classify from the *uncropped* footprint: cropping to the
                # stage would erase exactly the size signal that separates a
                # full-frame backdrop from a body-sized overlay.
                rec.slot = classify(ax, ay, cw, ch, body_w, body_h)
                # Anything outside the stage is clipped at render time, so
                # crop it here rather than shipping pixels the iPod will
                # never blit.
                vis_l, vis_t = -BODY_ORIGIN_X, -BODY_ORIGIN_Y
                vis_r, vis_b = vis_l + STAGE_W, vis_t + STAGE_H
                cl = max(0, vis_l - ax)
                ct = max(0, vis_t - ay)
                cr = min(cw, vis_r - ax)
                cb = min(ch, vis_b - ay)
                if cr <= cl or cb <= ct:
                    composed = None
                elif (cl, ct, cr, cb) != (0, 0, cw, ch):
                    composed = composed.crop((cl, ct, cr, cb))
                    ax += cl
                    ay += ct
            else:
                composed = None

        if composed is not None:
            write_bmp32(chip_dir / f"{rec.cid:04d}.bmp", composed)
            cw, ch = composed.size
        else:
            ax = ay = 0
            cw = ch = 0
            rec.slot = "none"

        # Icon: prefer the authored 128x128 token, else derive from the sprite.
        if rec.icon is not None:
            icon_src = Image.open(rec.icon).convert("RGBA")
        elif composed is not None:
            icon_src = composed
        else:
            continue
        tile, _, _ = trim(icon_src)
        tile.thumbnail((ICON_PX, ICON_PX), Image.LANCZOS)
        canvas = Image.new("RGBA", (ICON_PX, ICON_PX), (0, 0, 0, 0))
        canvas.alpha_composite(tile, ((ICON_PX - tile.width) // 2,
                                      (ICON_PX - tile.height) // 2))
        idx = len(icon_tiles)
        icon_tiles.append((rec.cid, canvas))
        rows.append((rec.cid, rec.slot, rec.z, ax, ay, cw, ch,
                     idx // ICONS_PER_PAGE, idx % ICONS_PER_PAGE,
                     rarity_for(rec.cid), 1 if composed is not None else 0))

    # iPod-only series. These IDs live above the preserved catalogue and are
    # composed from generated YTV-style pieces plus authentic Sitekick hands.
    # Real-world photos and product art are reference-only source material.
    for (cid, slot, z, ax, ay, rarity, name, kind,
         primary_source) in IPOD_EXCLUSIVE_CHIPS:
        composed = build_ipod_exclusive_image(kind, out_root)
        assert_stage_fit(cid, name, ax, ay, composed.width, composed.height)
        write_bmp32(chip_dir / f"{cid:04d}.bmp", composed)
        tile, _, _ = trim(composed)
        tile.thumbnail((ICON_PX, ICON_PX), Image.LANCZOS)
        canvas = Image.new("RGBA", (ICON_PX, ICON_PX), (0, 0, 0, 0))
        canvas.alpha_composite(
            tile,
            ((ICON_PX - tile.width) // 2, (ICON_PX - tile.height) // 2),
        )
        idx = len(icon_tiles)
        icon_tiles.append((cid, canvas))
        rows.append((
            cid, slot, z, ax, ay, composed.width, composed.height,
            idx // ICONS_PER_PAGE, idx % ICONS_PER_PAGE,
            rarity, 1, name,
        ))
        manifest.append((
            f"chips/{cid:04d}.bmp",
            sha256_of(IPOD_EXCLUSIVE_SOURCE_DIR / primary_source),
        ))

    # --- icon page strips ------------------------------------------------
    icon_dir = out_root / "icons"
    pages = (len(icon_tiles) + ICONS_PER_PAGE - 1) // ICONS_PER_PAGE
    for page in range(pages):
        sheet = Image.new("RGBA",
                          (ICON_PX * ICONS_PER_ROW, ICON_PX * ICON_ROWS),
                          (0, 0, 0, 0))
        for slot, (_, tile) in enumerate(
                icon_tiles[page * ICONS_PER_PAGE:(page + 1) * ICONS_PER_PAGE]):
            sheet.alpha_composite(tile, ((slot % ICONS_PER_ROW) * ICON_PX,
                                         (slot // ICONS_PER_ROW) * ICON_PX))
        write_bmp32(icon_dir / f"page{page}.bmp", sheet)

    # --- data files ------------------------------------------------------
    data_dir = out_root / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    stage = ["# key\tvalue",
             f"stage_w\t{STAGE_W}",
             f"stage_h\t{STAGE_H}",
             f"body_origin_x\t{BODY_ORIGIN_X}",
             f"body_origin_y\t{BODY_ORIGIN_Y}",
             f"body_h\t{BODY_TARGET_H}",
             f"icon_px\t{ICON_PX}",
             f"icons_per_row\t{ICONS_PER_ROW}",
             f"icon_rows\t{ICON_ROWS}",
             f"icon_pages\t{pages}",
             f"chip_count\t{len(rows)}"]
    for slot, w, h, cx, cy in base_rows:
        stage.append(f"base\t{slot}\t{w}\t{h}\t{cx}\t{cy}")
    (data_dir / "stage.v1.tsv").write_text("\n".join(stage) + "\n")

    header = ("# id\tslot\tz\tax\tay\tw\th\tpage\tindex\trarity\tworn\tname")
    lines = [header]
    for row in rows:
        cid, slot, z, ax, ay, w, h, page, index, rarity, worn = row[:11]
        name = row[11] if len(row) > 11 else f"Chip {cid:04d}"
        lines.append(f"{cid}\t{slot}\t{z}\t{ax}\t{ay}\t{w}\t{h}\t{page}\t"
                     f"{index}\t{rarity}\t{worn}\t{name}")
    (data_dir / "chips.v1.tsv").write_text("\n".join(lines) + "\n")

    counts: dict[str, int] = defaultdict(int)
    for r in rows:
        counts[r[1]] += 1
    series = ["# id\tname\tmembers"]
    for slot in SLOT_ORDER + ("none",):
        members = [str(r[0]) for r in rows if r[1] == slot]
        if members:
            series.append(f"{slot}\t{slot.title()} Collection\t"
                          + ",".join(members))
    series.append(
        "ipod-exclusive\tiPod Exclusives\t" +
        ",".join(str(spec[0]) for spec in IPOD_EXCLUSIVE_CHIPS)
    )
    series.append(
        "my-name-is-earl\tMy Name Is Earl\t" +
        ",".join(str(spec[0]) for spec in IPOD_EXCLUSIVE_CHIPS
                 if 926 <= spec[0] <= 931)
    )
    series.append("hand-gestures\tHand Gestures\t932,933,934")
    series.append("oliver-turbo\tOliver Tree Turbo\t935,936,937")
    series.append("the-wall\tThe Wall\t938,939")
    series.append("hybrid-meteora\tHybrid Theory + Meteora\t940,941")
    series.append("riot\tRiot!\t942,943")
    series.append("sunnyvale\tSunnyvale\t944,945")
    series.append("recess\tRecess\t946,947")
    series.append("6teen\t6teen\t948,949")
    series.append(
        "beatles-eras\tBeatles Eras\t"
        "901,904,908,950,951,952,953,954,955,956,957"
    )
    series.append(
        "pokemon-core\tPokemon\t958,959,963,966,967"
    )
    series.append(
        "pokemon-go\tPokemon GO\t960,961,962,964,965,968"
    )
    series.append(
        "fast-food-dive\tFast Food House Dive\t969,970,971,972"
    )
    series.append(
        "dollhouse\tDollhouse\t973,974,975,976"
    )
    series.append(
        "polaroid\tPolaroid\t977,978,979"
    )
    (data_dir / "series.v1.tsv").write_text("\n".join(series) + "\n")

    codes = ["# code\tkind\tvalue\tlabel"]
    for code, chip_id, label in SECRET_CODE_GRANTS:
        codes.append(f"{code}\tgrant\t{chip_id}\t{label}")
    (data_dir / "codes.v1.tsv").write_text("\n".join(codes) + "\n")

    sound_notes = build_sound_banks(art_root, out_root, manifest)

    manifest_lines = [f"{name}\t{digest}" for name, digest in manifest]
    (out_root / "source.manifest").write_text("\n".join(manifest_lines) + "\n")

    print(f"chips packaged : {len(rows)}")
    print(f"  with worn art: {sum(1 for r in rows if r[10])}")
    print(f"  icon pages   : {pages}")
    for slot in SLOT_ORDER + ("none",):
        if counts[slot]:
            print(f"  {slot:<10}: {counts[slot]}")
    for note in sound_notes:
        print(f"  {note}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--art-root", required=True, type=Path,
                    help="clone of github.com/SitekickRemastered/Art")
    ap.add_argument("--out", type=Path,
                    default=Path(__file__).resolve().parent.parent
                    / "assets/ipodjs/rockbox/sitekick")
    args = ap.parse_args()
    if not args.art_root.exists():
        sys.exit(f"art root not found: {args.art_root}")
    args.out.mkdir(parents=True, exist_ok=True)
    return build(args.art_root, args.out)


if __name__ == "__main__":
    raise SystemExit(main())

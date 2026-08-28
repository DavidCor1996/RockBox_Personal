#!/usr/bin/env python3
"""Package preserved Club Penguin assets for the Rockbox iPod plugin.

The runtime intentionally loads prepared BMP and TSV files. This host-side
tool converts/copies real source art into that package; it does not generate
replacement room art.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
from pathlib import Path


WORLD_ROWS = [
    ("My Place", "Enter your offline igloo.", 1763, 1309, 110, "player_home",
     160, 170),
    ("Town", "Enter Town.", 1034, 1102, 135, "town", 160, 170),
    ("Plaza", "Enter the Plaza.", 1942, 1044, 150, "plaza", 160, 170),
    ("Dock", "Enter the Dock.", 322, 1276, 140, "dock", 160, 170),
    ("Ski Village", "Enter Ski Village.", 848, 505, 120, "ski_village",
     160, 170),
    ("Dojo", "Enter the Dojo.", 1492, 133, 110, "dojo", 160, 170),
    ("Cove", "Enter the Cove.", 2170, 472, 125, "cove", 160, 170),
    ("Beach", "Enter the Beach.", 271, 679, 130, "beach", 160, 170),
    ("Snow Forts", "Enter the Snow Forts.", 1475, 1061, 120, "snow_forts",
     160, 170),
    ("Memorial Park", "Enter Night City's central park.",
     1950, 688, 120, "forest", 160, 170),
    ("Mine", "Enter the Mine Shack.", 1755, 389, 110, "mine_shack",
     160, 170),
    ("Iceberg", "Enter the Iceberg.", 2416, 290, 110, "iceberg", 160, 170),
    ("Puffle Hotel", "Enter the Puffle Hotel.", 2475, 994, 85,
     "puffle_hotel_lobby", 160, 170),
    ("Mall", "Enter the Mall.", 2391, 1201, 85, "mall", 160, 170),
]

WORLD_SOURCE_WIDTH = 2713
WORLD_SOURCE_HEIGHT = 1823
WORLD_OUTPUT_WIDTH = 320
WORLD_OUTPUT_HEIGHT = 220


ROOM_ROWS = [
    ("player_home", "My Place", "rooms/player_home.bmp",
     160, 170, 45, 120, 275, 198),
    ("backyard", "Backyard", "backyard/1.bmp",
     74, 160, 45, 78, 285, 198),
    ("town", "Town", "rooms/town.bmp", 160, 170, 25, 120, 295, 198),
    ("plaza", "Night City Plaza", "rooms/plaza.bmp",
     160, 170, 20, 112, 300, 198),
    ("dock", "Laguna Bend", "rooms/dock.bmp",
     160, 170, 25, 105, 295, 198),
    ("ski_village", "Ski Village", "rooms/ski_village.bmp",
     160, 170, 25, 115, 295, 198),
    ("dojo", "Dojo", "rooms/dojo.bmp", 160, 170, 35, 120, 285, 198),
    ("cove", "Cove", "rooms/cove.bmp", 160, 170, 25, 115, 295, 198),
    ("beach", "Laguna Bend Shore", "rooms/beach.bmp",
     160, 170, 25, 105, 295, 198),
    ("snow_forts", "Snow Forts", "rooms/snow_forts.bmp",
     160, 170, 20, 105, 300, 198),
    ("forest", "Memorial Park", "rooms/forest.bmp",
     160, 170, 20, 84, 300, 198),
    ("mine", "Mine", "rooms/mine.bmp", 160, 170, 30, 120, 290, 198),
    ("iceberg", "Iceberg", "rooms/iceberg.bmp",
     160, 170, 35, 105, 285, 198),
    ("gift_shop", "Gift Shop", "rooms/gift_shop.bmp",
     160, 170, 25, 80, 295, 198),
    ("coffee_shop", "Tom's Diner", "rooms/coffee_shop.bmp",
     160, 170, 25, 90, 295, 198),
    ("pet_shop", "Pet Shop", "rooms/pet_shop.bmp",
     160, 170, 25, 80, 295, 198),
    ("pizza_parlor", "Buck-A-Slice", "rooms/pizza_parlor.bmp",
     160, 170, 25, 90, 295, 198),
    ("night_club", "Afterlife", "rooms/night_club.bmp",
     160, 170, 25, 85, 295, 198),
    ("lounge", "Afterlife Lounge", "rooms/lounge.bmp",
     160, 170, 25, 85, 295, 198),
    ("book_room", "Book Room", "rooms/book_room.bmp",
     160, 170, 25, 85, 295, 198),
    ("boiler_room", "Boiler Room", "rooms/boiler_room.bmp",
     160, 170, 25, 85, 295, 198),
    ("ski_lodge", "Ski Lodge", "rooms/ski_lodge.bmp",
     160, 170, 25, 85, 295, 198),
    ("lodge_attic", "Lodge Attic", "rooms/lodge_attic.bmp",
     160, 170, 25, 85, 295, 198),
    ("sport_shop", "Sport Shop", "rooms/sport_shop.bmp",
     160, 170, 25, 85, 295, 198),
    ("ski_hill", "Ski Hill", "rooms/ski_hill.bmp",
     160, 170, 25, 85, 295, 198),
    ("lighthouse", "Laguna Bend Cottage", "rooms/lighthouse.bmp",
     160, 170, 25, 90, 295, 198),
    ("beacon", "Laguna Bend Rooftop", "rooms/beacon.bmp",
     160, 170, 25, 90, 295, 198),
    ("stadium", "Stadium", "rooms/stadium.bmp",
     160, 170, 25, 85, 295, 198),
    ("underground_pool", "Underground Pool",
     "rooms/underground_pool.bmp", 160, 170, 25, 85, 295, 198),
    ("cave_mine", "Cave Mine", "rooms/cave_mine.bmp",
     160, 170, 25, 85, 295, 198),
    ("dojo_courtyard", "Dojo Courtyard", "rooms/dojo_courtyard.bmp",
     160, 170, 25, 85, 295, 198),
    ("stage", "Oliver Tree Arena", "concert/frames/0.bmp",
     160, 180, 25, 80, 295, 198),
    ("emma_sewer", "Emma Blackery: Sewer Sessions",
     "rooms/emma_sewer.bmp", 160, 180, 24, 78, 296, 198),
    ("mine_shack", "Mine Shack", "rooms/mine_shack.bmp",
     160, 170, 25, 95, 295, 198),
    ("hidden_lake", "Hidden Lake", "rooms/hidden_lake.bmp",
     160, 170, 25, 68, 295, 198),
    ("recycling_plant", "Recycling Plant", "rooms/recycling_plant.bmp",
     160, 170, 25, 85, 295, 198),
    ("ninja_hideout", "Ninja Hideout", "rooms/ninja_hideout.bmp",
     160, 170, 25, 85, 295, 198),
    ("fire_dojo", "Fire Dojo", "rooms/fire_dojo.bmp",
     160, 170, 25, 85, 295, 198),
    ("water_dojo", "Water Dojo", "rooms/water_dojo.bmp",
     160, 170, 25, 85, 295, 198),
    ("snow_dojo", "Snow Dojo", "rooms/snow_dojo.bmp",
     160, 170, 25, 85, 295, 198),
    ("epf_lobby", "Everyday Phoning Facility", "rooms/epf_lobby.bmp",
     160, 170, 25, 85, 295, 198),
    ("epf_command", "EPF Command Room", "rooms/epf_command.bmp",
     160, 170, 25, 85, 295, 198),
    ("epf_vr", "VR Room", "rooms/epf_vr.bmp",
     160, 170, 25, 85, 295, 198),
    ("migrator", "The Migrator", "rooms/migrator.bmp",
     160, 170, 25, 85, 295, 198),
    ("shipnest", "Crow's Nest", "rooms/shipnest.bmp",
     166, 124, 25, 70, 295, 198),
    ("ship_hold", "Ship Hold", "rooms/ship_hold.bmp",
     160, 170, 25, 85, 295, 198),
    ("captain_quarters", "Captain's Quarters",
     "rooms/captain_quarters.bmp", 160, 170, 25, 85, 295, 198),
    ("cloud_forest", "Cloud Forest", "rooms/cloud_forest.bmp",
     160, 170, 25, 85, 295, 198),
    ("puffle_wild", "Puffle Wild", "rooms/puffle_wild.bmp",
     160, 170, 25, 85, 295, 198),
    ("puffle_hotel_lobby", "Puffle Hotel Lobby",
     "rooms/puffle_hotel_lobby.bmp", 160, 170, 25, 85, 295, 198),
    ("puffle_hotel_spa", "Puffle Hotel Spa",
     "rooms/puffle_hotel_spa.bmp", 160, 170, 25, 85, 295, 198),
    ("puffle_hotel_roof", "Puffle Hotel Roof",
     "rooms/puffle_hotel_roof.bmp", 160, 170, 25, 85, 295, 198),
    ("mall", "Mall", "rooms/mall.bmp", 160, 170, 25, 85, 295, 198),
    ("school", "School", "rooms/school.bmp", 160, 170, 25, 85, 295, 198),
    ("park", "Puffle Park", "rooms/park.bmp",
     160, 170, 25, 85, 295, 198),
    ("skatepark", "Skatepark", "rooms/skatepark.bmp",
     160, 170, 25, 85, 295, 198),
    ("underwater", "Underwater", "rooms/underwater.bmp",
     40, 119, 25, 75, 295, 198),
    ("box_dimension", "Box Dimension", "rooms/box_dimension.bmp",
     242, 66, 25, 45, 295, 198),
    ("welcome", "Welcome Solo", "rooms/welcome.bmp",
     139, 126, 25, 70, 295, 198),
]


INTERACTION_ROWS = [
    ("mine", "cart_surfer", 246, 127, 28, "minigame", "cart_surfer",
     "Play Cart Surfer", 0, 0),
    ("night_club", "sound_studio", 30, 165, 22, "minigame",
     "sound_studio", "Use the Afterlife DJ Booth", 0, 0),
    ("town", "to_dock", 8, 170, 24, "room", "dock",
     "Go to the Dock", 272, 170),
    ("town", "to_snow_forts", 295, 170, 24, "room", "snow_forts",
     "Go to the Snow Forts", 45, 170),
    ("dock", "to_ski_village", 8, 130, 18, "room", "ski_village",
     "Go to Ski Village", 275, 170),
    ("dock", "to_town", 312, 130, 18, "room", "town",
     "Go to Town", 45, 170),
    ("dock", "to_lighthouse", 165, 72, 18, "room", "lighthouse",
     "Enter the Laguna Bend Cottage", 160, 170),
    ("dock", "judy", 210, 142, 22, "message", "judy",
     "Judy: The lake is quiet tonight. Want to go diving?", 0, 0),
    ("ski_village", "to_beach", 25, 155, 25, "room", "beach",
     "Go to the Beach", 255, 165),
    ("ski_village", "to_dock", 295, 155, 25, "room", "dock",
     "Go to the Dock", 85, 155),
    ("beach", "to_ski_village", 294, 130, 18, "room", "ski_village",
     "Go to Ski Village", 45, 160),
    ("snow_forts", "to_town", 52, 125, 25, "room", "town",
     "Go to Town", 275, 170),
    ("snow_forts", "to_plaza", 266, 125, 25, "room", "plaza",
     "Go to the Plaza", 45, 170),
    ("plaza", "to_snow_forts", 8, 128, 18, "room", "snow_forts",
     "Go to the Snow Forts", 275, 165),
    ("plaza", "to_forest", 312, 128, 18, "room", "forest",
     "Go to Memorial Park", 55, 150),
    ("forest", "to_plaza", 35, 120, 26, "room", "plaza",
     "Take the Corpo Plaza path", 275, 165),
    ("forest", "to_cove", 285, 115, 26, "room", "cove",
     "Take the eastern park path", 55, 135),
    ("cove", "to_forest", 35, 120, 26, "room", "forest",
     "Go to Memorial Park", 265, 135),
    ("town", "to_gift_shop", 247, 104, 12, "room", "gift_shop",
     "Enter the Night City Gift Shop", 225, 125),
    ("gift_shop", "to_town", 230, 105, 28, "room", "town",
     "Return to Town", 255, 155),
    ("gift_shop", "penguin_style", 285, 170, 30, "shop",
     "penguin_style", "Browse Penguin Style", 0, 0),
    ("town", "to_coffee_shop", 88, 104, 12, "room", "coffee_shop",
     "Enter the Night City Coffee Shop", 150, 125),
    ("coffee_shop", "to_town", 153, 73, 18, "room", "town",
     "Return to Night City", 88, 155),
    ("plaza", "to_pet_shop", 98, 91, 16, "room", "pet_shop",
     "Enter the Pet Shop", 155, 125),
    ("pet_shop", "to_plaza", 155, 90, 30, "room", "plaza",
     "Return to the Plaza", 45, 155),
    ("plaza", "to_pizza_parlor", 242, 90, 16, "room", "pizza_parlor",
     "Enter Buck-A-Slice", 160, 125),
    ("pizza_parlor", "to_plaza", 160, 71, 18, "room", "plaza",
     "Return to Night City Plaza", 275, 155),
    ("town", "to_night_club", 160, 93, 12, "room", "night_club",
     "Enter Afterlife", 160, 170),
    ("night_club", "to_town", 160, 84, 12, "room", "town",
     "Exit Afterlife to Night City", 160, 150),
    ("night_club", "to_lounge", 286, 108, 12, "room", "lounge",
     "Go up to the Afterlife Lounge", 160, 170),
    ("lounge", "to_night_club", 160, 64, 12, "room", "night_club",
     "Return to Afterlife", 270, 150),
    ("coffee_shop", "to_book_room", 285, 105, 16, "room", "book_room",
     "Go upstairs to the Book Room", 160, 170),
    ("book_room", "to_coffee_shop", 160, 92, 30, "room", "coffee_shop",
     "Return to the Coffee Shop", 270, 150),
    ("plaza", "to_stage", 160, 90, 16, "room", "stage",
     "Enter the Oliver Tree Arena", 160, 180),
    ("stage", "to_plaza", 24, 99, 18, "room", "plaza",
     "Return to Night City Plaza", 160, 160),
    ("plaza", "to_emma_sewer", 116, 150, 12, "room", "emma_sewer",
     "Climb down to Sewer Sessions", 160, 180),
    ("emma_sewer", "to_plaza", 24, 99, 18, "room", "plaza",
     "Return to Night City Plaza", 160, 170),
    ("ski_village", "to_ski_lodge", 88, 125, 30, "room", "ski_lodge",
     "Enter the Ski Lodge", 160, 170),
    ("ski_lodge", "to_ski_village", 160, 92, 30, "room", "ski_village",
     "Return to Ski Village", 88, 160),
    ("ski_lodge", "to_lodge_attic", 280, 112, 30, "room", "lodge_attic",
     "Go upstairs to the Lodge Attic", 160, 170),
    ("lodge_attic", "to_ski_lodge", 160, 92, 30, "room", "ski_lodge",
     "Return to the Ski Lodge", 270, 150),
    ("ski_village", "to_sport_shop", 225, 125, 30, "room", "sport_shop",
     "Enter the Sport Shop", 160, 170),
    ("sport_shop", "to_ski_village", 160, 92, 30, "room", "ski_village",
     "Return to Ski Village", 225, 160),
    ("sport_shop", "snow_and_sports", 292, 190, 24, "shop", "sport_shop",
     "Browse Snow and Sports", 0, 0),
    ("ski_village", "to_ski_hill", 158, 100, 28, "room", "ski_hill",
     "Climb to Ski Hill", 160, 170),
    ("ski_hill", "to_ski_village", 160, 92, 30, "room", "ski_village",
     "Return to Ski Village", 158, 150),
    ("beach", "to_lighthouse", 176, 73, 18, "room", "lighthouse",
     "Enter the Laguna Bend Cottage", 160, 170),
    ("lighthouse", "to_dock", 160, 74, 18, "room", "dock",
     "Return to Laguna Bend Dock", 165, 135),
    ("lighthouse", "to_beacon", 286, 103, 18, "room", "beacon",
     "Climb to the Cottage Rooftop", 160, 170),
    ("beacon", "to_lighthouse", 166, 125, 18, "room", "lighthouse",
     "Climb down to the Cottage", 270, 150),
    ("beacon", "to_lighthouse_door", 174, 76, 16, "room", "lighthouse",
     "Enter the Cottage", 270, 150),
    ("snow_forts", "to_stadium", 160, 105, 18, "room", "stadium",
     "Enter the Stadium", 160, 170),
    ("stadium", "to_snow_forts", 160, 92, 30, "room", "snow_forts",
     "Return to the Snow Forts", 160, 155),
    ("mine", "to_cave_mine", 160, 112, 32, "room", "cave_mine",
     "Enter the Cave Mine", 160, 170),
    ("cave_mine", "to_mine", 35, 150, 30, "room", "mine",
     "Return to the Mine", 160, 160),
    ("cave_mine", "to_underground_pool", 285, 150, 30, "room",
     "underground_pool", "Enter the Underground Pool", 160, 170),
    ("underground_pool", "to_cave_mine", 285, 150, 30, "room",
     "cave_mine", "Return to the Cave Mine", 275, 150),
    ("underground_pool", "to_boiler_room", 35, 150, 30, "room",
     "boiler_room", "Enter the Boiler Room", 160, 170),
    ("boiler_room", "to_underground_pool", 160, 92, 30, "room",
     "underground_pool", "Return to the Underground Pool", 45, 150),
    ("dojo", "to_dojo_courtyard", 160, 92, 30, "room",
     "dojo_courtyard", "Step into the Dojo Courtyard", 160, 170),
    ("dojo_courtyard", "to_dojo", 160, 115, 32, "room", "dojo",
     "Enter the Dojo", 160, 170),
    ("mine_shack", "to_mine", 232, 108, 28, "room", "mine",
     "Enter the Mine", 80, 160),
    ("mine", "to_mine_shack", 54, 116, 25, "room", "mine_shack",
     "Return to the Mine Shack", 235, 165),
    ("mine_shack", "to_recycling_plant", 116, 108, 28, "room",
     "recycling_plant", "Enter the Recycling Plant", 160, 165),
    ("recycling_plant", "to_mine_shack", 285, 145, 28, "room",
     "mine_shack", "Return to the Mine Shack", 115, 165),
    ("forest", "to_hidden_lake", 224, 104, 24, "room", "hidden_lake",
     "Enter the NCART underpass", 45, 160),
    ("hidden_lake", "to_forest", 91, 40, 28, "room", "forest",
     "Return to Memorial Park", 245, 150),
    ("hidden_lake", "to_cave_mine", 19, 99, 28, "room", "cave_mine",
     "Enter the Cave Mine", 95, 155),
    ("hidden_lake", "to_underwater", 295, 139, 28, "room", "underwater",
     "Unlock the Moss-Key Door", 114, 109),
    ("cave_mine", "to_hidden_lake", 95, 125, 26, "room", "hidden_lake",
     "Enter the Hidden Lake", 275, 155),
    ("dojo_courtyard", "to_ninja_hideout", 270, 145, 28, "room",
     "ninja_hideout", "Enter the Ninja Hideout", 160, 175),
    ("ninja_hideout", "to_dojo_courtyard", 160, 185, 28, "room",
     "dojo_courtyard", "Return to the Dojo Courtyard", 265, 165),
    ("ninja_hideout", "to_fire_dojo", 55, 105, 28, "room", "fire_dojo",
     "Enter the Fire Dojo", 160, 170),
    ("fire_dojo", "to_ninja_hideout", 160, 95, 30, "room",
     "ninja_hideout", "Return to the Ninja Hideout", 55, 160),
    ("ninja_hideout", "to_water_dojo", 160, 105, 28, "room",
     "water_dojo", "Enter the Water Dojo", 160, 170),
    ("water_dojo", "to_ninja_hideout", 160, 185, 30, "room",
     "ninja_hideout", "Return to the Ninja Hideout", 160, 160),
    ("ninja_hideout", "to_snow_dojo", 265, 105, 28, "room", "snow_dojo",
     "Enter the Snow Dojo", 160, 170),
    ("ninja_hideout", "martial_artworks", 303, 181, 20, "shop",
     "ninja_catalog", "Open Martial Artworks", 0, 0),
    ("snow_dojo", "to_ninja_hideout", 290, 180, 28, "room",
     "ninja_hideout", "Return to the Ninja Hideout", 265, 160),
    ("ski_village", "to_epf_lobby", 270, 125, 28, "room", "epf_lobby",
     "Enter the Everyday Phoning Facility", 160, 170),
    ("epf_lobby", "to_ski_village", 35, 150, 28, "room", "ski_village",
     "Return to Ski Village", 270, 160),
    ("epf_lobby", "to_epf_command", 160, 110, 30, "room", "epf_command",
     "Enter the EPF Command Room", 160, 170),
    ("epf_command", "to_epf_lobby", 160, 190, 28, "room", "epf_lobby",
     "Return to the Everyday Phoning Facility", 160, 160),
    ("epf_command", "to_epf_vr", 285, 120, 28, "room", "epf_vr",
     "Enter the VR Room", 160, 170),
    ("epf_vr", "to_epf_command", 285, 120, 28, "room", "epf_command",
     "Return to the EPF Command Room", 275, 160),
    ("beach", "to_migrator", 25, 125, 18, "room", "migrator",
     "Board the Migrator", 45, 165),
    ("migrator", "to_beach", 25, 165, 28, "room", "beach",
     "Return to the Beach", 45, 155),
    ("migrator", "to_ship_hold", 290, 125, 28, "room", "ship_hold",
     "Enter the Ship Hold", 275, 165),
    ("migrator", "to_shipnest", 161, 55, 32, "room", "shipnest",
     "Climb to the Crow's Nest", 166, 124),
    ("shipnest", "to_migrator", 216, 162, 30, "room", "migrator",
     "Climb down to the Migrator Deck", 175, 114),
    ("ship_hold", "to_migrator", 285, 165, 28, "room", "migrator",
     "Return to the Migrator Deck", 275, 155),
    ("ship_hold", "to_captain_quarters", 273, 91, 26, "room",
     "captain_quarters", "Enter the Captain's Quarters", 275, 160),
    ("captain_quarters", "to_ship_hold", 288, 145, 28, "room",
     "ship_hold", "Return to the Ship Hold", 265, 150),
    ("underwater", "to_hidden_lake", 31, 100, 30, "room", "hidden_lake",
     "Return to the Hidden Lake", 232, 147),
    ("underwater", "moss_key", 187, 170, 22, "message", "item_7016",
     "Moss Key (item 7016) is active for this offline profile", 0, 0),
    ("forest", "to_puffle_wild", 112, 104, 24, "room", "puffle_wild",
     "Enter the park conservatory", 55, 165),
    ("puffle_wild", "to_forest", 55, 165, 28, "room", "forest",
     "Return to Memorial Park", 160, 155),
    ("puffle_wild", "to_cloud_forest", 285, 145, 28, "room",
     "cloud_forest", "Follow the rainbow to Cloud Forest", 160, 170),
    ("cloud_forest", "to_puffle_wild", 160, 175, 30, "room",
     "puffle_wild", "Return to Puffle Wild", 275, 155),
    ("puffle_hotel_lobby", "to_plaza", 35, 160, 28, "room", "plaza",
     "Return to the Plaza", 100, 160),
    ("puffle_hotel_lobby", "to_puffle_hotel_spa", 285, 125, 28, "room",
     "puffle_hotel_spa", "Go to the Puffle Hotel Spa", 55, 165),
    ("puffle_hotel_spa", "to_puffle_hotel_lobby", 35, 150, 28, "room",
     "puffle_hotel_lobby", "Return to the Puffle Hotel Lobby", 275, 160),
    ("puffle_hotel_spa", "to_puffle_hotel_roof", 285, 125, 28, "room",
     "puffle_hotel_roof", "Go to the Puffle Hotel Roof", 55, 165),
    ("puffle_hotel_roof", "to_puffle_hotel_spa", 35, 145, 28, "room",
     "puffle_hotel_spa", "Return to the Puffle Hotel Spa", 275, 160),
    ("mall", "to_plaza", 160, 185, 30, "room", "plaza",
     "Return to the Plaza", 173, 160),
    ("mall", "costume_trunk", 278, 174, 24, "shop",
     "costume_trunk", "Open the Costume Trunk", 0, 0),
    ("mine_shack", "to_school", 280, 145, 24, "room", "school",
     "Enter the School", 160, 170),
    ("school", "to_mine_shack", 160, 185, 30, "room", "mine_shack",
     "Return to the Mine Shack", 275, 165),
    ("pet_shop", "to_park", 280, 105, 24, "room", "park",
     "Enter Puffle Park", 55, 165),
    ("park", "to_pet_shop", 35, 165, 28, "room", "pet_shop",
     "Return to the Pet Shop", 275, 155),
    ("mine_shack", "to_skatepark", 35, 150, 26, "room", "skatepark",
     "Enter the Skatepark", 55, 165),
    ("skatepark", "to_mine_shack", 35, 165, 28, "room", "mine_shack",
     "Return to the Mine Shack", 45, 165),
    ("puffle_hotel_roof", "rainbow_puffle_quest", 160, 105, 30,
     "puffle", "rainbow", "Complete the Rainbow Puffle Quest", 0, 0),
    ("cave_mine", "gold_puffle_quest", 220, 105, 28, "puffle", "gold",
     "Complete the Gold Puffle Quest", 0, 0),
    ("pet_shop", "adopt_puffle", 60, 50, 28, "puffle", "adopt",
     "Adopt or choose a Puffle", 0, 0),
    ("pet_shop", "pet_furniture", 298, 178, 20, "shop",
     "pet_furniture", "Open Pet Furniture", 0, 0),
    ("player_home", "puffle_care", 85, 155, 30, "puffle", "care",
     "Care for your Puffle", 0, 0),
    ("player_home", "edit_igloo", 255, 155, 30, "igloo", "edit",
     "Edit your Igloo", 0, 0),
    ("player_home", "upgrade_igloo", 280, 105, 22, "igloo", "upgrade",
     "Open Igloo Upgrades", 0, 0),
    ("player_home", "wardrobe", 160, 155, 30, "shop", "wardrobe",
     "Customize your Penguin", 0, 0),
    ("player_home", "to_backyard", 112, 120, 34, "room", "backyard",
     "Go to your Backyard", 74, 160),
    ("backyard", "to_player_home", 61, 172, 26, "room", "player_home",
     "Return to your Igloo", 55, 165),
    ("backyard", "to_player_home_icon", 302, 177, 18, "room",
     "player_home", "Return to your Igloo", 55, 165),
    ("backyard", "backyard_info", 302, 202, 16, "message", "info",
     "Your puffles can play safely in your backyard.", 0, 0),
]


VANILLA_ROOMS = "media/default/svanilla/media/play/v2/content/global/rooms"
LEGACY_ROOMS = "media/default/slegacy/media/play/v2/content/global/rooms"

ROOM_SWFS = {
    "player_home": "media/default/fix/Igloo1.swf",
    "town": f"{VANILLA_ROOMS}/town.swf",
    "plaza": f"{VANILLA_ROOMS}/plaza.swf",
    "dock": f"{VANILLA_ROOMS}/dock.swf",
    "ski_village": f"{VANILLA_ROOMS}/village.swf",
    "dojo": f"{VANILLA_ROOMS}/dojo.swf",
    "cove": f"{VANILLA_ROOMS}/cove.swf",
    "beach": f"{VANILLA_ROOMS}/beach.swf",
    "snow_forts": f"{VANILLA_ROOMS}/forts.swf",
    "forest": f"{VANILLA_ROOMS}/forest.swf",
    "mine": f"{VANILLA_ROOMS}/mine.swf",
    "iceberg": f"{LEGACY_ROOMS}/berg.swf",
    "gift_shop": f"{VANILLA_ROOMS}/shop.swf",
    "coffee_shop": f"{VANILLA_ROOMS}/coffee.swf",
    "pet_shop": f"{VANILLA_ROOMS}/pet.swf",
    "pizza_parlor": f"{VANILLA_ROOMS}/pizza.swf",
    "night_club": f"{VANILLA_ROOMS}/dance.swf",
    "lounge": f"{VANILLA_ROOMS}/lounge.swf",
    "book_room": f"{VANILLA_ROOMS}/book.swf",
    "boiler_room": f"{VANILLA_ROOMS}/boiler.swf",
    "ski_lodge": f"{VANILLA_ROOMS}/lodge.swf",
    "lodge_attic": f"{VANILLA_ROOMS}/attic.swf",
    "sport_shop": f"{LEGACY_ROOMS}/sport.swf",
    "ski_hill": f"{VANILLA_ROOMS}/mtn.swf",
    "lighthouse": f"{VANILLA_ROOMS}/light.swf",
    "beacon": f"{VANILLA_ROOMS}/beacon.swf",
    "stadium": f"{VANILLA_ROOMS}/rink.swf",
    "underground_pool": f"{VANILLA_ROOMS}/cave.swf",
    "cave_mine": f"{VANILLA_ROOMS}/cavemine.swf",
    "dojo_courtyard": f"{VANILLA_ROOMS}/dojoextsolo.swf",
    "stage": "media/default/archives/04252012Stage.swf",
    "mine_shack": f"{VANILLA_ROOMS}/shack.swf",
    "hidden_lake": f"{VANILLA_ROOMS}/lake.swf",
    "recycling_plant": f"{LEGACY_ROOMS}/eco.swf",
    "ninja_hideout": f"{LEGACY_ROOMS}/dojohide.swf",
    "fire_dojo": f"{VANILLA_ROOMS}/dojofire.swf",
    "water_dojo": f"{VANILLA_ROOMS}/dojowater.swf",
    "snow_dojo": f"{VANILLA_ROOMS}/dojosnow.swf",
    "epf_lobby": f"{VANILLA_ROOMS}/agentlobbymulti.swf",
    "epf_command": f"{VANILLA_ROOMS}/agentcom.swf",
    "epf_vr": f"{LEGACY_ROOMS}/agentvr.swf",
    "migrator": f"{LEGACY_ROOMS}/ship.swf",
    "ship_hold": f"{LEGACY_ROOMS}/shiphold.swf",
    "captain_quarters": f"{LEGACY_ROOMS}/shipquarters.swf",
    "cloud_forest": f"{VANILLA_ROOMS}/cloudforest.swf",
    "puffle_wild": f"{VANILLA_ROOMS}/pufflewild.swf",
    "puffle_hotel_lobby": f"{VANILLA_ROOMS}/hotellobby.swf",
    "puffle_hotel_spa": f"{VANILLA_ROOMS}/hotelspa.swf",
    "puffle_hotel_roof": f"{VANILLA_ROOMS}/hotelroof.swf",
    "mall": f"{VANILLA_ROOMS}/mall.swf",
    "school": f"{VANILLA_ROOMS}/school.swf",
    "park": f"{VANILLA_ROOMS}/park.swf",
    "skatepark": f"{VANILLA_ROOMS}/skatepark.swf",
}

DIRECT_ROOM_FRAMES = set(ROOM_SWFS) - {
    "player_home", "town", "plaza", "dock", "ski_village", "dojo",
    "cove", "beach", "snow_forts", "forest", "mine", "iceberg",
    "gift_shop", "coffee_shop", "pet_shop", "pizza_parlor",
}

PUFFLE_COLORS = [
    ("blue", "Blue", 137),
    ("red", "Red", 182),
    ("pink", "Pink", 135),
    ("black", "Black", 218),
    ("green", "Green", 156),
    ("purple", "Purple", 160),
    ("yellow", "Yellow", 185),
    ("white", "White", 221),
    ("orange", "Orange", 288),
    ("brown", "Brown", 576),
    ("rainbow", "Rainbow", 503),
    ("gold", "Gold", 339),
]
PUFFLE_FRAMES = [1, 8, 16, 24, 72, 88, 120, 136]

PUFFLE_FOOD_ASSETS = (
    "apple", "chocolatecoin", "fishburger", "hummusandpita",
    "icecreamsandwich", "popcorn", "pretzel", "puffleos",
    "rainbowlollipop", "sock", "stinkycheese", "tacos", "watermelon",
    "yogurtparfait",
)

PUFFLE_WALK_ROOT_FRAMES = (9, 11, 13, 15)
PUFFLE_WALK_SUBFRAMES = (1, 7)
PUFFLE_ROOM_VIEWBOX = (3, -39, 54, 54)

PUFFLE_DIG_SPRITES = {
    "black": 161,
    "blue": 164,
    "brown": 189,
    "gold": 205,
    "green": 187,
    "orange": 249,
    "pink": 182,
    "purple": 192,
    "rainbow": 208,
    "red": 175,
    "white": 186,
    "yellow": 197,
}

PUFFLE_EAT_SPRITES = {
    "black": 20,
    "blue": 19,
    "brown": 19,
    "green": 19,
    "orange": 21,
    "pink": 19,
    "purple": 19,
    "rainbow": 18,
    "red": 19,
    "white": 19,
    "yellow": 19,
}

PUFFLE_PLAY_SPRITES = {
    "black": (120, 173),
    "blue": (76, 95),
    "brown": (282, 469),
    "gold": (191, 304),
    "green": (93, 119),
    "orange": (143, 255),
    "pink": (89, 113),
    "purple": (89, 137),
    "rainbow": (163, 465),
    "red": (77, 142),
    "white": (106, 190),
    "yellow": (119, 156),
}

# The reaction arrays in the preserved item table use the original care
# client order, which differs from the port's display order.
PUFFLE_REACTION_INDEX = {
    "blue": 0,
    "pink": 1,
    "black": 2,
    "green": 3,
    "purple": 4,
    "red": 5,
    "yellow": 6,
    "white": 7,
    "orange": 8,
    "brown": 9,
    "rainbow": 10,
    "gold": 11,
}

PUFFLE_CARE_ICONS = (
    ("food", 116),
    ("play", 112),
    ("sleep", 110),
    ("care", 114),
    ("walk", 39),
    ("dig", 108),
)

PUFFLE_TRICKS = (
    "jumpForward", "jumpSpin", "nuzzle", "roll", "speak",
    "standOnHead",
)

PUFFLE_HAT_MISSING_WEARABLES = {
    "bigbang", "candycanecap", "heavymetal", "jollyroger",
    "snowflakehelmet",
}


SHOP_PAGE_FILES = [
    "2c167cd6-6a3a-476b-8a67-62e36e966fce_left_bg_617.png",
    "7cf6a8ad-a563-4145-85a3-8d31df4a1c46_left_bg_617.png",
    "7e45742a-5eaa-4974-9bcf-695e45763ea2_left_bg_617.png",
    "a8a8affe-1998-44f6-86d6-e9a439c8813d_left_bg_617.png",
]

AVATAR_ITEMS = {
    0: [
        (403, "Hard Hat", 50), (405, "Green Cap", 200),
        (406, "Pink Cap", 200), (415, "Pilgrim Hat", 200),
        (423, "Top Hat", 350), (424, "Chef Hat", 50),
        (426, "Jester Hat", 250), (483, "Blue Earmuffs", 250),
        (435, "Red Cap", 200), (436, "Blue Cap", 200),
        (441, "Admiral's Hat", 400),
        (1124, "The Part", 0),
    ],
    1: [
        (239, "Wetsuit", 450), (251, "Jean Jacket", 450),
        (256, "Ballerina", 450), (291, "Leprechaun Tuxedo", 560),
        (296, "Red Letterman Jacket", 550),
        (297, "Lifeguard Shirt", 180),
        (299, "Firefighter Jacket", 380), (770, "Safety Vest", 200),
        (785, "Ski Patrol Jacket", 600),
        (4786, "No Fuss Denim Jacket", 400),
        (4769, "Green Tuff Jacket", 400),
        (4284, "Pink Sled Coat", 0),
    ],
    2: [
        (351, "Brown Shoes", 400), (352, "Black Sneakers", 250),
        (353, "Ballet Shoes", 180), (357, "Blue Sneakers", 250),
        (360, "Running Shoes", 300), (363, "Yellow Sandals", 150),
        (365, "Winter Boots", 450), (366, "Bunny Slippers", 200),
        (368, "Cowboy Boots", 300), (370, "Elf Shoes", 170),
        (6169, "Cotton Sandals", 150),
        (6039, "Pink Canvas Shoes", 0),
    ],
    4: [
        (2125, "Oliver Tree Full Look", 0),
        (118, "White Diva Sunglasses", 225),
        (125, "Aviator Sunglasses", 50),
        (2032, "Pink Diva Shades", 125),
        (2044, "Blue Aviator Shades", 200),
        (2046, "Indigo Sunglasses", 50),
        (2057, "Golden Shades", 100),
        (2069, "Giant White Sunglasses", 200),
        (2149, "Pixel Shades", 1500),
        (2059, "Pink Starglasses", 100),
        (2027, "Blue Face Paint", 50),
        (2019, "Yellow Face Paint", 50),
    ],
}

COLOR_ITEMS = [
    ("Green", "#009900"), ("Pink", "#ff3399"),
    ("Black", "#333333"), ("Peach", "#ff6666"),
    ("Dark Green", "#006600"), ("Light Blue", "#0099cc"),
    ("Lime Green", "#8ae302"), ("Aqua", "#02a797"),
    ("Gray", "#93a0a4"), ("Red", "#cc0000"),
    ("Orange", "#ff6600"), ("Yellow", "#ffcc00"),
    ("Dark Purple", "#660099"), ("Brown", "#996600"),
    ("Blue", "#003366"),
]

# Canonical purchase calls extracted from the preserved June 2008
# Snow and Sports catalog. Types match the native wardrobe departments;
# 5 is hand, 6 is player-card background, and 7 is furniture.
SPORT_ITEMS = [
    (2, 719, 5, "Basketball", 300),
    (2, 726, 5, "Football", 300),
    (2, 727, 5, "Soccer Ball", 300),
    (2, 836, 1, "Red Basketball Jersey", 600),
    (2, 837, 1, "Blue Basketball Jersey", 600),
    (3, 792, 1, "Blue Baseball Uniform", 600),
    (3, 791, 1, "Red Baseball Uniform", 600),
    (3, 435, 0, "Red Ball Cap", 200),
    (3, 436, 0, "Blue Ball Cap", 200),
    (3, 717, 5, "Baseball Glove", 300),
    (4, 349, 5, "Tennis Racket", 250),
    (4, 775, 1, "Red Soccer Jersey", 600),
    (4, 254, 1, "Red Cheerleader Uniform", 350),
    (4, 255, 1, "Blue Cheerleader Uniform", 350),
    (4, 778, 1, "Blue Soccer Jersey", 600),
    (4, 385, 2, "Cleats", 250),
    (5, 321, 5, "Flashing Lure Fishing Rod", 200),
    (6, 701, 5, "Flame Surfboard", 400),
    (6, 702, 5, "Daisy Surfboard", 400),
    (6, 703, 5, "Silver Surfboard", 800),
    (7, 712, 5, "Pink Striped Wakeboard", 300),
    (7, 711, 5, "Yellow Arrow Wakeboard", 300),
    (7, 993, 6, "Sports Background", 60),
    (7, 918, 6, "Soccer Background", 60),
    (7, 965, 6, "Baseball Background", 60),
    (8, 303, 7, "Baseball Base", 100),
    (8, 290, 7, "Basketball Net", 320),
    (8, 291, 7, "Scoreboard", 520),
    (8, 302, 7, "Home Plate", 150),
    (8, 289, 7, "Cricket Wickets", 225),
]

SPORT_WEARABLE_IDS = {
    836, 837, 792, 791, 435, 436, 717, 775, 254, 255, 778, 385,
}

# Canonical purchase calls from the April 26, 2012 Ruby and the Ruby
# Costume Trunk.  This catalog is the exact companion to 04252012Stage.swf.
# Types match the native paper-doll slots; 3 is neck, 5 is hand, and 6 is
# player-card background.
COSTUME_ITEMS = [
    (3, 1010, 0, "The Movie Star", 600),
    (3, 842, 1, "Dazzle Dress", 600),
    (3, 181, 3, "Pearl Necklace", 550),
    (3, 1009, 0, "Gray Fedora", 250),
    (3, 4012, 1, "Detective's Coat", 650),
    (3, 5003, 5, "Magnifying Glass", 150),
    (4, 1011, 0, "Blue Felt Hat", 300),
    (4, 4013, 1, "Blue Zoot Suit", 700),
    (4, 3005, 3, "Skinny Blue Tie", 150),
    (4, 1012, 0, "Doorman's Cap", 150),
    (4, 4014, 1, "Doorman's Jacket", 400),
    (4, 6008, 2, "Black Zoot Shoes", 450),
    (5, 9002, 6, "Detective Background", 60),
    (5, 489, 0, "Director's Hat", 250),
    (5, 4015, 1, "Dark Detective's Coat", 650),
]

COSTUME_WEARABLE_IDS = {row[1] for row in COSTUME_ITEMS if row[2] != 6}

SHOP_ITEMS = [
    (page, name, cost)
    for page in (0, 1, 2)
    for _, name, cost in AVATAR_ITEMS[page]
] + [
    (3, name, 20) for name, _ in COLOR_ITEMS
] + [
    (4, name, cost) for _, name, cost in AVATAR_ITEMS[4]
]

# The authentic 193-frame paper-doll timeline begins with the complete
# directional move.  Keep the runtime strip in S/W/N/E order: the south
# walk poses, west turn, north/back walk, and east turn.  The previous
# 46/82/138 samples are emote poses and made every non-south direction look
# front-facing.
AVATAR_FRAMES = (1, 5, 9, 19, 20, 21, 22, 29, 36, 39, 40, 41)
AVATAR_FRAME_W = 52
AVATAR_FRAME_H = 56
PENGUIN_STYLE_PAGES = 16
PENGUIN_STYLE_ITEMS = 186
PENGUIN_STYLE_WEARABLES = 110
NINJA_CATALOG_PAGES = 15
NINJA_CATALOG_ITEMS = 20
NINJA_CATALOG_WEARABLES = 15
PUFFLE_ADOPT_PAGES = 11
PUFFLE_ADOPT_ITEMS = (
    (3, 0, "Blue Puffle", 800),
    (3, 5, "Red Puffle", 800),
    (4, 1, "Pink Puffle", 800),
    (4, 2, "Black Puffle", 800),
    (5, 3, "Green Puffle", 800),
    (5, 6, "Yellow Puffle", 800),
    (6, 4, "Purple Puffle", 800),
    (6, 7, "White Puffle", 800),
    (7, 8, "Orange Puffle", 800),
    (7, 9, "Brown Puffle", 800),
)
PET_FURNITURE_PAGES = 5
PET_FURNITURE_ITEMS = (
    (2, 0, 604, "Puffle Ball", 150, 1, 1, 99),
    (2, 0, 603, "Puffle Washer", 450, 1, 1, 99),
    (2, 0, 224, "Scratch Tower", 350, 1, 1, 99),
    (2, 0, 210, "Small Scratching Post", 150, 1, 1, 99),
    (2, 0, 617, "Salon Chair", 400, 1, 1, 99),
    (2, 1, 220, "Puffle Condo", 280, 1, 1, 99),
    (3, 0, 618, "Orange Puffle House", 500, 1, 1, 99),
    (3, 0, 208, "Green Puffle House", 500, 1, 1, 99),
    (3, 0, 209, "Purple Puffle House", 500, 1, 1, 99),
    (3, 0, 228, "Yellow Puffle House", 500, 1, 1, 99),
    (3, 0, 233, "White Puffle House", 500, 1, 1, 99),
    (3, 0, 207, "Blue Puffle House", 500, 1, 1, 99),
    (3, 0, 206, "Pink Puffle House", 500, 1, 1, 99),
    (3, 1, 223, "Red Puffle House", 500, 1, 1, 99),
    (3, 1, 204, "Gray Puffle House", 500, 1, 1, 99),
    (4, 0, 232, "White Puffle Bed", 250, 1, 1, 99),
    (4, 0, 227, "Yellow Puffle Bed", 250, 1, 1, 99),
    (4, 0, 222, "Red Puffle Bed", 250, 1, 1, 99),
    (4, 0, 200, "Blue Puffle Bed", 250, 1, 1, 99),
    (4, 0, 221, "Purple Puffle Bed", 250, 1, 1, 99),
    (4, 0, 201, "Pink Puffle Bed", 250, 1, 1, 99),
    (4, 0, 619, "Orange Puffle Bed", 250, 1, 1, 99),
    (4, 0, 203, "Green Puffle Bed", 250, 1, 1, 99),
    (4, 0, 212, "Water Dish", 50, 1, 1, 99),
    (4, 0, 225, "Double Dish", 80, 1, 1, 99),
    (4, 0, 214, "Water Bottle", 50, 1, 1, 99),
    (4, 1, 202, "Gray Puffle Bed", 250, 1, 1, 99),
)
AVATAR_CANVAS = 64
AVATAR_ORIGIN_X = 32
AVATAR_ORIGIN_Y = 34
WADDLE_SOURCE_COMMIT = "bcf7e9d4d4f7619710492448d532f4e7eb1e5caa"

# These SWFs attach their furniture from ActionScript, so their root frame is
# transparent. The preserved wrapper/composite sprite below is the real item
# render; this is deliberately explicit so packaging is reproducible and can
# never silently replace missing art with a blank or invented image.
FURNITURE_SPRITE_FRAMES = {
    170: "DefineSprite_9_f170/1.png",
    171: "DefineSprite_9_f171/1.png",
    172: "DefineSprite_9_f172/1.png",
    173: "DefineSprite_9_f173/1.png",
    174: "DefineSprite_9_f174/1.png",
    283: "DefineSprite_8/1.png",
    284: "DefineSprite_8/2.png",
    287: "DefineSprite_9/3.png",
    485: "DefineSprite_5/6.png",
    611: "DefineSprite_6/1.png",
    612: "DefineSprite_10/6.png",
    635: "DefineSprite_26/2.png",
    640: "DefineSprite_21/2.png",
    642: "DefineSprite_8/2.png",
    685: "DefineSprite_23/3.png",
    716: "DefineSprite_19/4.png",
    719: "DefineSprite_34/4.png",
    768: "DefineSprite_9/3.png",
    831: "DefineSprite_22/7.png",
    847: "DefineSprite_7/2.png",
    848: "DefineSprite_7/2.png",
    936: "DefineSprite_5/2.png",
    942: "DefineSprite_7/3.png",
    946: "DefineSprite_7/3.png",
    949: "DefineSprite_7/2.png",
    957: "DefineSprite_12/3.png",
    967: "DefineSprite_6/1.png",
    2106: "DefineSprite_5/2.png",
    2154: "DefineSprite_7/2.png",
    2257: "DefineSprite_16/1.png",
    2260: "DefineSprite_7/2.png",
    2261: "DefineSprite_22/2.png",
    2262: "DefineSprite_7/2.png",
    2331: "DefineSprite_10/3.png",
}


def validate_interactions() -> None:
    rooms = {row[0]: row for row in ROOM_ROWS}
    seen: set[tuple[str, str]] = set()
    room_links: set[tuple[str, str]] = set()
    dynamic_room_links = {("player_home", "box_dimension")}
    tutorial_rooms = {"welcome"}

    world_targets = {row[5] for row in WORLD_ROWS}
    unknown_map_targets = world_targets - set(rooms)
    if unknown_map_targets:
        raise SystemExit(
            "map has unknown rooms: " + ", ".join(sorted(unknown_map_targets))
        )
    for room, target in dynamic_room_links:
        if room not in rooms or target not in rooms:
            raise SystemExit(f"dynamic room link is invalid: {room}/{target}")

    for row in INTERACTION_ROWS:
        room, interaction_id, x, y, radius, action, target, _, to_x, to_y = row
        key = (room, interaction_id)

        if room not in rooms:
            raise SystemExit(f"interaction has unknown room: {room}")
        if key in seen:
            raise SystemExit(
                f"duplicate interaction id in room: {room}/{interaction_id}"
            )
        seen.add(key)
        if not (0 <= x < WORLD_OUTPUT_WIDTH and
                0 <= y < WORLD_OUTPUT_HEIGHT and radius > 0):
            raise SystemExit(
                f"interaction is outside room bounds: {room}/{interaction_id}"
            )
        if action == "room":
            if target not in rooms:
                raise SystemExit(
                    f"interaction has unknown target: {room}/{target}"
                )
            target_room = rooms[target]
            _, _, _, _, _, left, top, right, bottom = target_room
            if (to_x, to_y) != (0, 0) and not (
                    left <= to_x <= right and top <= to_y <= bottom):
                raise SystemExit(
                    f"interaction spawn is not walkable: "
                    f"{room}/{interaction_id}"
                )
            room_links.add((room, target))
        elif action == "minigame":
            if target not in {"cart_surfer", "sound_studio"}:
                raise SystemExit(f"unknown minigame target: {target}")
        elif action == "shop":
            if target not in {
                    "penguin_style", "wardrobe", "sport_shop",
                    "costume_trunk", "ninja_catalog", "pet_furniture"}:
                raise SystemExit(f"unknown shop target: {target}")
        elif action == "puffle":
            if target not in {"adopt", "care", "rainbow", "gold"}:
                raise SystemExit(f"unknown puffle target: {target}")
        elif action == "igloo":
            if target not in {"edit", "upgrade"}:
                raise SystemExit(f"unknown igloo target: {target}")
        elif action not in {"map", "message"}:
            raise SystemExit(f"unknown interaction action: {action}")

    for room, target in room_links:
        if (target, room) not in room_links and room not in world_targets:
            raise SystemExit(f"room link has no return path: {room} -> {target}")

    reachable = set(world_targets)
    while True:
        expanded = reachable | {
            target for room, target in room_links if room in reachable
        } | {
            target for room, target in dynamic_room_links if room in reachable
        }
        if expanded == reachable:
            break
        reachable = expanded
    unreachable = set(rooms) - reachable - tutorial_rooms
    if unreachable:
        raise SystemExit(
            "rooms missing entry paths: " + ", ".join(sorted(unreachable))
        )


def validate_shop() -> None:
    expected_counts = [12, 12, 12, 15, 12]
    counts = [0, 0, 0, 0, 0]
    names: set[str] = set()

    if len(SHOP_ITEMS) > 64:
        raise SystemExit("shop inventory exceeds the two-mask save format")
    for page, name, cost in SHOP_ITEMS:
        if page < 0 or page >= len(expected_counts) or cost < 0:
            raise SystemExit(f"invalid shop item: {name}")
        if name in names:
            raise SystemExit(f"duplicate shop item name: {name}")
        names.add(name)
        counts[page] += 1
    if counts != expected_counts:
        raise SystemExit(f"unexpected shop page counts: {counts}")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_ffmpeg(args: list[str]) -> None:
    subprocess.run(["ffmpeg", "-y", "-hide_banner", "-loglevel", "error",
                    *args], check=True)


def run_magick(args: list[str]) -> None:
    subprocess.run(["magick", *args], check=True)


def render_registered_puffle_frame(source: Path, output: Path) -> None:
    """Render an official 20px root frame without clipping registration.

    The walk and room-hat SWFs are external-loader assets. Their root stage is
    only 20x20, while the registered puffle and hat art intentionally extends
    beyond it. JPEXS preserves those transforms in SVG; replacing only the
    viewport retains the original scale, position, animation, and z layer.
    """
    svg = source.read_text(encoding="utf-8")
    root_start = svg.find("<svg")
    root_end = svg.find(">", root_start)
    if root_start < 0 or root_end < 0:
        raise SystemExit(f"invalid registered puffle SVG: {source}")
    root = re.sub(
        r'\s(?:height|width|viewBox)="[^"]*"', "",
        svg[root_start:root_end],
    )
    x, y, width, height = PUFFLE_ROOM_VIEWBOX
    svg = (
        svg[:root_start] + root +
        f' width="40px" height="40px" '
        f'viewBox="{x} {y} {width} {height}">' +
        svg[root_end + 1:]
    )
    svg = svg.replace(
        '    <rect fill="#ffffff" height="20.0px" width="20.0px"/>\n',
        "", 1,
    )
    temporary = output.with_suffix(".svg")
    temporary.write_text(svg, encoding="utf-8")
    run_magick([
        "-background", "none", str(temporary), "-background", "magenta",
        "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
    ])
    temporary.unlink()


def write_alpha_mask(source: Path, output: Path) -> None:
    """Write the official Flash floor mask as a compact 1-bit stage mask."""
    alpha = subprocess.check_output([
        "magick", str(source), "-resize", "320x220!", "-alpha", "extract",
        "-threshold", "0", "-depth", "8", "gray:-",
    ])
    width = 320
    height = 220
    row_bytes = (width + 7) // 8
    if len(alpha) != width * height:
        raise SystemExit(
            f"unexpected igloo mask size for {source}: {len(alpha)}"
        )
    packed = bytearray(b"CPMASK1\n")
    for y in range(height):
        row = alpha[y * width:(y + 1) * width]
        for byte_x in range(row_bytes):
            value = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < width and row[x] != 0:
                    value |= 1 << (7 - bit)
            packed.append(value)
    output.write_bytes(packed)


def make_player_frame(source: Path, frame: tuple[str, int, int, int, int],
                      out: Path) -> None:
    sheet, x, y, w, h = frame
    run_ffmpeg([
        "-i", str(source / "images" / sheet),
        "-f", "lavfi",
        "-i", f"color=c=magenta:s={AVATAR_FRAME_W}x{AVATAR_FRAME_H}",
        "-filter_complex",
        f"[0:v]crop={w}:{h}:{x}:{y},"
        "scale=50:54:force_original_aspect_ratio=decrease[fg];"
        "[1:v][fg]overlay=(main_w-overlay_w)/2:"
        "(main_h-overlay_h)/2:format=auto,format=bgr24",
        "-frames:v", "1",
        str(out),
    ])


def git_commit(path: Path) -> str:
    if not (path / ".git").exists():
        return "unknown"
    try:
        return subprocess.check_output(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except subprocess.SubprocessError:
        return "unknown"


def write_tsvs(out: Path) -> None:
    validate_interactions()
    validate_shop()
    data = out / "data"
    data.mkdir(parents=True, exist_ok=True)
    with (data / "world.tsv").open("w", encoding="utf-8") as f:
        f.write("# name\tdetail\tx\ty\tradius\ttarget\tto_x\tto_y\n")
        for name, detail, x, y, radius, target, to_x, to_y in WORLD_ROWS:
            screen_x = round(x * WORLD_OUTPUT_WIDTH / WORLD_SOURCE_WIDTH)
            screen_y = round(y * WORLD_OUTPUT_HEIGHT / WORLD_SOURCE_HEIGHT)
            screen_radius = max(
                8,
                round(radius * min(
                    WORLD_OUTPUT_WIDTH / WORLD_SOURCE_WIDTH,
                    WORLD_OUTPUT_HEIGHT / WORLD_SOURCE_HEIGHT,
                )),
            )
            row = (name, detail, screen_x, screen_y, screen_radius, target,
                   to_x, to_y)
            f.write("\t".join(str(value) for value in row) + "\n")

    with (data / "rooms.tsv").open("w", encoding="utf-8") as f:
        f.write("# id\ttitle\tbmp\tstart_x\tstart_y\t"
                "walk_left\twalk_top\twalk_right\twalk_bottom\n")
        for row in ROOM_ROWS:
            f.write("\t".join(str(value) for value in row) + "\n")

    with (data / "interactions.tsv").open("w", encoding="utf-8") as f:
        f.write("# room\tid\tx\ty\tradius\taction\ttarget\tlabel\t"
                "to_x\tto_y\n")
        for row in INTERACTION_ROWS:
            f.write("\t".join(str(value) for value in row) + "\n")


def write_manifest(out: Path, source: Path, generated: list[Path],
                   waddle_source: Path | None = None) -> None:
    commit = git_commit(source)
    with (out / "source.manifest").open("w", encoding="utf-8") as f:
        f.write("CLUBPENGUIN_ASSET_MANIFEST_V1\n")
        f.write("source_repo=despedite/clubpenguinfreeroam\n")
        f.write(f"source_commit={commit}\n")
        f.write("source_path=images/sprite-sheet0.png\n")
        f.write("source_path=images/sprite2-sheet0.png\n")
        f.write("source_path=images/sprite2-sheet1.png\n")
        if waddle_source is not None:
            f.write("room_source_repo=nhaar/Waddle-Forever\n")
            f.write(f"room_source_commit={WADDLE_SOURCE_COMMIT}\n")
            cover_logo = (waddle_source / "media/default/websites/modern/"
                          "assets/sites/default/themes/snowball/img/"
                          "club-penguin-logo.png")
            f.write("cover_source=media/default/websites/modern/assets/"
                    "sites/default/themes/snowball/img/club-penguin-logo.png"
                    f"\t{sha256(cover_logo)}\n")
            shop_source = (waddle_source / "media/default/svanilla/play/"
                           "mobile/cp-mobile-ui/clubpenguin_v1_6/en_US/"
                           "deploy/metaplace/devicepng/assets/catalog/"
                           "penstyle")
            for filename in SHOP_PAGE_FILES:
                f.write(f"shop_source={filename}\t"
                        f"{sha256(shop_source / filename)}\n")
            for room_id, rel in ROOM_SWFS.items():
                swf = waddle_source / rel
                f.write(f"room_source={room_id}\t{rel}\t{sha256(swf)}\n")
        for path in generated:
            f.write(f"generated={path.relative_to(out)}\n")
            f.write(f"sha256={sha256(path)}\n")
        for rel in ("data/world.tsv", "data/rooms.tsv",
                    "data/interactions.tsv"):
            path = out / rel
            f.write(f"generated={rel}\n")
            f.write(f"sha256={sha256(path)}\n")
        f.write("world_scale=320x220\n")
        f.write("world_coordinate_source=2713x1823\n")
        f.write("player_strip=13x52x56 animation=4x3 selector=1\n")
        f.write("data=data/world.tsv\n")
        f.write("data=data/rooms.tsv\n")
        f.write("data=data/interactions.tsv\n")


def refresh_existing_manifest(out: Path) -> None:
    """Refresh hashes after running an individual packaging stage.

    Full package runs replace this file through write_manifest(). Keeping this
    partial-stage path makes provenance checks equally strict during iterative
    archive imports, without fabricating unavailable source metadata.
    """
    manifest = out / "source.manifest"
    if not manifest.exists():
        return
    old_lines = manifest.read_text(encoding="utf-8").splitlines()
    generated_index = next(
        (i for i, line in enumerate(old_lines)
         if line.startswith("generated=")), len(old_lines)
    )
    suffix_index = next(
        (i for i, line in enumerate(old_lines)
         if line.startswith("world_scale=")), len(old_lines)
    )
    lines = old_lines[:generated_index]
    ignored = {manifest, out / "save.dat", out / "save.tmp"}
    package_files = sorted(
        (path for path in out.rglob("*")
         if path.is_file() and path not in ignored and
         not any(part.startswith(".") for part in path.relative_to(out).parts)),
        key=lambda path: str(path.relative_to(out)),
    )
    for path in package_files:
        lines.extend([
            f"generated={path.relative_to(out)}",
            f"sha256={sha256(path)}",
        ])
    lines.extend(old_lines[suffix_index:])
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def refresh_generated_hashes(manifest: Path, root: Path) -> None:
    """Refresh generated= hashes without changing source provenance."""
    lines: list[str] = []

    for line in manifest.read_text(encoding="utf-8").splitlines():
        if line.startswith("generated="):
            relative = line.split("\t", 1)[0].split("=", 1)[1]
            output = root / relative
            if output.exists():
                line = f"generated={relative}\t{sha256(output)}"
        lines.append(line)
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def package_room_frames(room_frames: Path, waddle_source: Path, out: Path,
                        generated: list[Path]) -> None:
    rooms_out = out / "rooms"
    rooms_out.mkdir(parents=True, exist_ok=True)

    for room_id, rel in ROOM_SWFS.items():
        frame = room_frames / f"{room_id}.png"
        swf = waddle_source / rel
        room_out = rooms_out / f"{room_id}.bmp"

        if not frame.exists():
            raise SystemExit(f"missing rendered room frame: {frame}")
        if not swf.exists():
            raise SystemExit(f"missing preserved room SWF: {swf}")

        if room_id in DIRECT_ROOM_FRAMES or room_id in {
                "gift_shop", "coffee_shop", "pet_shop", "pizza_parlor"}:
            video_filter = "scale=320:220"
        else:
            # The preserved captures are 968x777 Ruffle window screenshots.
            # Crop the displayed 760x480 SWF canvas (scaled to 808x510).
            video_filter = "crop=808:510:80:118,scale=320:220"
        run_ffmpeg([
            "-i", str(frame), "-vf", video_filter,
            "-pix_fmt", "bgr24", str(room_out),
        ])
        generated.append(room_out)


def package_backyard(backyard_export: Path, out: Path,
                     generated: list[Path]) -> None:
    """Package the eight official location-specific backyard scenes.

    The input frames must be JPEXS root-frame exports made after removing the
    root `pet_area` placement. The original client hides that collision clip
    immediately after loading; keeping its green debug fill would not be a
    faithful visible frame. The two interface icons come from the preserved
    `backyard.swf` loader and are placed at its original ActionScript
    coordinates after scaling the 760x480 stage to 320x220.
    """
    backyard_out = out / "backyard"
    source_dir = backyard_export / "source"
    frame_dir = backyard_export / "frames"
    icon_dir = backyard_export / "icons"
    loader = source_dir / "backyard.swf"
    igloo_icon = icon_dir / "igloo.png"
    info_icon = icon_dir / "info.png"
    manifest = backyard_out / "source.manifest"
    lines = [
        "CLUBPENGUIN_BACKYARD_MANIFEST_V1",
        "source_mirror=icerink.solero.me/media1.clubpenguin.com",
        "source_path=play/v2/client/backyard.swf",
        "background_path=play/v2/content/global/backyard",
        "extractor=JPEXS_FFDec_26.2.1",
        "stage=760x480 output=320x220 BMP3",
        "visibility=official BackyardBackgroundView hides pet_area",
        "player_start=175,350 scaled=74,160",
        "gate_move_point=145,375 scaled=61,172",
    ]

    for path in (loader, igloo_icon, info_icon):
        if not path.exists():
            raise SystemExit(f"missing preserved backyard source: {path}")
    lines.append(f"source=backyard.swf\t{sha256(loader)}")

    backyard_out.mkdir(parents=True, exist_ok=True)
    for location in range(1, 9):
        source_swf = source_dir / f"{location}_backyard.swf"
        frame = frame_dir / f"{location}.png"
        output = backyard_out / f"{location}.bmp"

        if not source_swf.exists() or not frame.exists():
            raise SystemExit(
                f"missing preserved backyard location {location}"
            )
        run_magick([
            str(frame), "-resize", "320x220!",
            "(", str(igloo_icon), "-resize", "21x22!", ")",
            "-geometry", "+291+166", "-composite",
            "(", str(info_icon), "-resize", "21x23!", ")",
            "-geometry", "+291+192", "-composite",
            "-background", "magenta", "-alpha", "remove", "-alpha", "off",
            f"BMP3:{output}",
        ])
        lines.extend([
            f"source={source_swf.name}\t{sha256(source_swf)}",
            f"frame={frame.relative_to(backyard_export)}\t{sha256(frame)}",
            f"generated={output.name}\t{sha256(output)}",
        ])
        generated.append(output)

    lines.extend([
        f"icon=igloo.png\t{sha256(igloo_icon)}",
        f"icon=info.png\t{sha256(info_icon)}",
    ])
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.append(manifest)


def package_archive_rooms(archive_room_export: Path, out: Path,
                          generated: list[Path]) -> None:
    """Package permanent rooms recovered from the official content mirror.

    Frames are JPEXS root-frame exports after removing only collision/control
    clips hidden by each room's own ActionScript. No visible room artwork is
    repainted or substituted.
    """
    rooms_out = out / "rooms"
    source_dir = archive_room_export / "source"
    frame_dir = archive_room_export / "frames"
    manifest = rooms_out / "preserved.manifest"
    room_sources = (
        ("shipnest", "shipnest.swf", "shipnest.png",
         "triggers_mc root tag 291; block_mc root tag 294"),
        ("underwater", "underwater.swf", "underwater.png",
         "block_mc root tag 142; triggers_mc root tag 148"),
        ("box_dimension", "boxdimension.swf", "box_dimension.png",
         "block_mc root tag 29; triggers_mc root tag 232"),
    )
    lines = [
        "CLUBPENGUIN_PRESERVED_ROOM_MANIFEST_V1",
        "source_mirror=icerink.solero.me/media1.clubpenguin.com",
        "source_path=play/v2/content/global/rooms",
        "extractor=JPEXS_FFDec_26.2.1",
        "stage=760x480 output=320x220 BMP3",
        "visibility=only ActionScript-hidden control placements removed",
        "shipnest_start=393,270 scaled=166,124",
        "shipnest_exit=ship,415,248 scaled=175,114",
        "underwater_start=95,260 scaled=40,119",
        "underwater_exit=lake,550,320 scaled=232,147",
        "lake_underwater_entry=270,237 scaled=114,109 item=7016",
        "box_dimension_start=575,145 scaled=242,66",
        "box_dimension_exit=map portal=600,125 scaled=253,57",
        "box_dimension_entry=furniture_item_529_portal_box",
        "welcome_start=330,275 scaled=139,126",
        "welcome_entry=first_login_only room_id=112",
        "welcome_exit=guideComplete then island_map",
    ]

    rooms_out.mkdir(parents=True, exist_ok=True)
    for room_id, swf_name, frame_name, hidden in room_sources:
        source_swf = source_dir / swf_name
        frame = frame_dir / frame_name
        output = rooms_out / f"{room_id}.bmp"

        if not source_swf.exists() or not frame.exists():
            raise SystemExit(f"missing preserved room export: {room_id}")
        run_magick([
            str(frame), "-resize", "320x220!", "-background", "magenta",
            "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
        ])
        lines.extend([
            f"source={swf_name}\t{sha256(source_swf)}",
            f"frame={frame_name}\t{sha256(frame)}",
            f"hidden={room_id}\t{hidden}",
            f"generated={output.name}\t{sha256(output)}",
        ])
        generated.append(output)

    welcome_source = source_dir / "welcomesolo.swf"
    guide_source = source_dir / "newplayerexperience.swf"
    welcome_frame = frame_dir / "welcome.png"
    guide_frames = (
        ("welcome_guide_0.png", 3, 79),
        ("welcome_guide_1.png", 5, 169),
        ("welcome_guide_2.png", 7, 126),
    )
    welcome_output = rooms_out / "welcome.bmp"
    tutorial_out = out / "tutorial"

    if (not welcome_source.exists() or not guide_source.exists() or
            not welcome_frame.exists()):
        raise SystemExit("missing preserved Welcome Solo export")
    for filename, _, _ in guide_frames:
        if not (frame_dir / filename).exists():
            raise SystemExit(f"missing Welcome Solo guide frame: {filename}")

    run_magick([
        str(welcome_frame), "-resize", "320x220!", "-background", "magenta",
        "-alpha", "remove", "-alpha", "off", f"BMP3:{welcome_output}",
    ])
    generated.append(welcome_output)
    tutorial_out.mkdir(parents=True, exist_ok=True)
    lines.extend([
        f"source=welcomesolo.swf\t{sha256(welcome_source)}",
        f"source=newplayerexperience.swf\t{sha256(guide_source)}",
        f"frame=welcome.png\t{sha256(welcome_frame)}",
        "hidden=welcome\tblock_mc character 221; "
        "snowballBlock character 223",
        f"generated={welcome_output.name}\t{sha256(welcome_output)}",
    ])
    for page, (filename, root_frame, subframe) in enumerate(guide_frames):
        guide_frame = frame_dir / filename
        output = tutorial_out / f"welcome_{page}.bmp"

        run_magick([
            str(welcome_frame), "(", str(guide_frame), "-crop",
            "760x480+0+0", "+repage", ")", "-composite", "-resize",
            "320x220!", "-background", "magenta", "-alpha", "remove",
            "-alpha", "off", f"BMP3:{output}",
        ])
        lines.extend([
            f"guide_frame={filename}\troot={root_frame} "
            f"subframe={subframe}\t{sha256(guide_frame)}",
            f"generated=tutorial/{output.name}\t{sha256(output)}",
        ])
        generated.append(output)

    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.append(manifest)
    refresh_existing_manifest(out)


def package_sound_studio(sound_export: Path, out: Path,
                         generated: list[Path]) -> None:
    """Package the official Sound Studio client art and album audio.

    The four available album SWFs contain 40 named MP3 sounds each. They are
    decoded losslessly to signed 16-bit mono PCM for the native Rockbox mixer;
    no sound is synthesized, substituted, or rearranged. The Spooky selector
    remains part of the official board art, but no album is emitted because
    album_spooky.swf is absent from the verified archive.
    """
    sound_out = out / "soundstudio"
    source_dir = sound_export / "source"
    frame_dir = sound_export / "frames"
    audio_dir = sound_export / "audio"
    title = frame_dir / "title.png"
    board = frame_dir / "board.png"
    saved_tracks = frame_dir / "saved_populated.png"
    saved_empty = frame_dir / "saved_empty.png"
    saved_list_clean = frame_dir / "saved_list_clean.png"
    save_prompt = frame_dir / "save_prompt.png"
    instructions_dir = sound_export / "instructions"
    title_out = sound_out / "title.bmp"
    board_out = sound_out / "board.bmp"
    saved_tracks_out = sound_out / "saved_tracks.bmp"
    saved_empty_out = sound_out / "saved_empty.bmp"
    save_prompt_out = sound_out / "save_prompt.bmp"
    manifest = sound_out / "source.manifest"
    client_files = (
        "soundroom.swf", "Music.swf", "music_game.swf",
        "music_mysonglist.swf", "music_widget.swf",
    )
    genres = ("pop", "rock", "dance", "dubstep")
    sample_rate = 22050
    clip_count = 40
    header_size = 16
    entry_size = 8
    data_offset = header_size + clip_count * entry_size
    lines = [
        "CLUBPENGUIN_SOUND_STUDIO_MANIFEST_V1",
        "source_mirror=icerink.solero.me/media1.clubpenguin.com",
        "room_path=play/v2/content/global/rooms/soundroom.swf",
        "client_path=play/v2/client/music",
        "album_path=play/v2/content/global/music/albums",
        "extractor=JPEXS_FFDec_26.2.1",
        "stage=760x480 output=320x220 BMP3",
        "board_visibility=official board with prompts/startScreen/"
        "instructions overlays hidden",
        "instructions=character_495 frames_2..6 cropped to stage and "
        "composited over official board",
        "saved_populated=all_saved_music_1 frame_1 subframe_2; dynamic "
        "text/likes/sharing placements removed before export",
        "saved_empty=official music_mysonglist root frame_2",
        "save_prompt=character_392 frame_2 subframe_7 cropped to stage",
        "trigger=night_club mixmaster_mc 347.5,175 scaled=146,80",
        "layout=40 buttons column-major; loops=0..24; oneshots=25..39",
        "format=CPSA_V1 s16le mono 22050Hz indexed clips",
        "album_spooky=unavailable in verified indexed archive; no substitute",
    ]

    for filename in client_files:
        path = source_dir / filename
        if not path.exists():
            raise SystemExit(f"missing preserved Sound Studio source: {path}")
        lines.append(f"source={filename}\t{sha256(path)}")
    clean_saved_swf = source_dir / "music_mysonglist_clean.swf"
    if not clean_saved_swf.exists():
        raise SystemExit(
            f"missing cleaned Saved Tracks source: {clean_saved_swf}"
        )
    lines.extend([
        f"derived={clean_saved_swf.name}\t{sha256(clean_saved_swf)}",
        "derived_visibility=removed only text_name, text_likes, "
        "button_share, and label placements from saved-track rows",
    ])
    for path in (title, board, saved_tracks, saved_empty,
                 saved_list_clean, save_prompt):
        if not path.exists():
            raise SystemExit(f"missing preserved Sound Studio frame: {path}")

    sound_out.mkdir(parents=True, exist_ok=True)
    for frame, output in ((title, title_out), (board, board_out)):
        run_magick([
            str(frame), "-resize", "320x220!", "-background", "black",
            "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
        ])
        lines.append(
            f"frame={frame.relative_to(sound_export)}\t{sha256(frame)}"
        )
        lines.append(f"generated={output.name}\t{sha256(output)}")
        generated.append(output)

    for frame, output in (
        (saved_tracks, saved_tracks_out),
        (saved_empty, saved_empty_out),
        (save_prompt, save_prompt_out),
    ):
        run_magick([
            str(frame), "-resize", "320x220!", "-background", "black",
            "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
        ])
        lines.extend([
            f"frame={frame.relative_to(sound_export)}\t{sha256(frame)}",
            f"generated={output.name}\t{sha256(output)}",
        ])
        generated.append(output)
    lines.append(
        f"frame={saved_list_clean.relative_to(sound_export)}\t"
        f"{sha256(saved_list_clean)}"
    )

    for page, swf_frame in enumerate(range(2, 7), 1):
        overlay = instructions_dir / f"{swf_frame}.png"
        cropped = sound_out / f".instruction_{page}.png"
        output = sound_out / f"instruction_{page}.bmp"

        if not overlay.exists():
            raise SystemExit(
                f"missing preserved Sound Studio instruction: {overlay}"
            )
        run_magick([
            str(overlay), "-crop", "760x480+243+24", "+repage",
            str(cropped),
        ])
        run_magick([
            str(board), str(cropped), "-composite", "-resize", "320x220!",
            "-background", "black", "-alpha", "remove", "-alpha", "off",
            f"BMP3:{output}",
        ])
        cropped.unlink()
        lines.extend([
            f"instruction={overlay.relative_to(sound_export)}\t"
            f"{sha256(overlay)}",
            f"generated={output.name}\t{sha256(output)}",
        ])
        generated.append(output)

    for genre in genres:
        album = source_dir / f"album_{genre}.swf"
        output = sound_out / f"{genre}.cpsa"
        clips: list[bytes] = []
        entries: list[tuple[int, int]] = []
        offset = data_offset

        if not album.exists():
            raise SystemExit(f"missing preserved Sound Studio album: {album}")
        lines.append(f"source={album.name}\t{sha256(album)}")
        for clip_id in range(clip_count):
            source = audio_dir / genre / f"{clip_id}.mp3"
            if not source.exists():
                raise SystemExit(
                    f"missing {genre} Sound Studio clip {clip_id}: {source}"
                )
            decoded = subprocess.run(
                ["ffmpeg", "-hide_banner", "-loglevel", "error",
                 "-i", str(source), "-f", "s16le", "-acodec",
                 "pcm_s16le", "-ac", "1", "-ar", str(sample_rate), "-"],
                check=True, stdout=subprocess.PIPE,
            ).stdout
            if not decoded or len(decoded) % 2 != 0:
                raise SystemExit(f"invalid decoded Sound Studio clip: {source}")
            clips.append(decoded)
            entries.append((offset, len(decoded) // 2))
            offset += len(decoded)
            lines.append(
                f"clip={genre}/{clip_id}.mp3\t{sha256(source)}\t"
                f"samples={len(decoded) // 2}"
            )
        loop_lengths = {samples for _, samples in entries[:25]}
        if len(loop_lengths) != 1:
            raise SystemExit(f"{genre} loop clips are not synchronized")
        with output.open("wb") as f:
            f.write(struct.pack(
                "<4sHHHHI", b"CPSA", 1, sample_rate, clip_count, 0,
                data_offset,
            ))
            for clip_offset, samples in entries:
                f.write(struct.pack("<II", clip_offset, samples))
            for decoded in clips:
                f.write(decoded)
        lines.append(
            f"generated={output.name}\t{sha256(output)}\t"
            f"bytes={output.stat().st_size}"
        )
        generated.append(output)

    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.append(manifest)
    refresh_existing_manifest(out)


def package_shop(waddle_source: Path, out: Path,
                 generated: list[Path]) -> None:
    source_dir = (waddle_source / "media/default/svanilla/play/mobile/"
                  "cp-mobile-ui/clubpenguin_v1_6/en_US/deploy/metaplace/"
                  "devicepng/assets/catalog/penstyle")
    shop_out = out / "shop"
    shop_out.mkdir(parents=True, exist_ok=True)
    (out / "data").mkdir(parents=True, exist_ok=True)

    for index, filename in enumerate(SHOP_PAGE_FILES):
        source = source_dir / filename
        output = shop_out / f"page{index}.bmp"
        if not source.exists():
            raise SystemExit(f"missing preserved catalog page: {source}")
        run_magick([
            str(source), "-resize", "177x220!", "-gravity", "west",
            "-background", "#0875b9", "-extent", "320x220",
            f"BMP3:{output}",
        ])
        generated.append(output)

    # Face items use the same preserved Penguin Style catalog chrome. The
    # item art and the live paper-doll preview identify the department.
    face_page = shop_out / "page4.bmp"
    shutil.copyfile(shop_out / "page0.bmp", face_page)
    generated.append(face_page)

    write_shop_data(out, generated)


def write_shop_data(out: Path, generated: list[Path]) -> None:
    (out / "data").mkdir(parents=True, exist_ok=True)
    shop_data = out / "data" / "shop.tsv"
    with shop_data.open("w", encoding="utf-8") as f:
        f.write("# page\tslot\tname\tcost\n")
        slots = [0, 0, 0, 0, 0]
        for page, name, cost in SHOP_ITEMS:
            f.write(f"{page}\t{slots[page]}\t{name}\t{cost}\n")
            slots[page] += 1
    generated.append(shop_data)


def avatar_item_root(export: Path, kind: str, item_id: str) -> Path:
    """Accept FFDec's single-file and multi-file export directory names."""
    plain = export / kind / item_id
    suffixed = export / kind / f"{item_id}.swf"
    if plain.is_dir():
        return plain
    if suffixed.is_dir():
        return suffixed
    raise SystemExit(f"missing {kind} avatar export for {item_id}")


def avatar_master(export: Path, item_id: str) -> tuple[Path, Path]:
    """Return matching PNG/SVG directories for a 193-frame paper doll."""
    png_root = avatar_item_root(export, "png", item_id)
    svg_root = avatar_item_root(export, "svg", item_id)
    matches = [
        directory for directory in png_root.glob("DefineSprite_*")
        if len(list(directory.glob("*.png"))) == 193
    ]
    if len(matches) != 1:
        raise SystemExit(
            f"expected one 193-frame avatar sprite for {item_id}: {matches}"
        )
    svg_dir = svg_root / matches[0].name
    if not svg_dir.is_dir():
        raise SystemExit(f"missing matching SVG avatar export: {svg_dir}")
    return matches[0], svg_dir


def avatar_origin(svg: Path) -> tuple[float, float]:
    text = svg.read_text(encoding="utf-8")
    match = re.search(
        r'<g transform="matrix\([^,]+, [^,]+, [^,]+, [^,]+, '
        r'([^,]+), ([^)]+)\)',
        text,
    )
    if match is None:
        raise SystemExit(f"missing FFDec frame origin: {svg}")
    return float(match.group(1)), float(match.group(2))


def avatar_place_args(png: Path, svg: Path) -> list[str]:
    tx, ty = avatar_origin(svg)
    x = round(AVATAR_ORIGIN_X - tx)
    y = round(AVATAR_ORIGIN_Y - ty)
    return [str(png), "-geometry", f"+{x}+{y}", "-composite"]


def avatar_frame(export: Path, sources: list[tuple[str, int]],
                 frame: int, output: Path,
                 tint: str | None = None) -> None:
    args = ["-size", f"{AVATAR_CANVAS}x{AVATAR_CANVAS}", "canvas:none"]
    for item_id, sprite_id in sources:
        png_dir = (avatar_item_root(export, "png", item_id) /
                   f"DefineSprite_{sprite_id}")
        svg_dir = (avatar_item_root(export, "svg", item_id) /
                   f"DefineSprite_{sprite_id}")
        png = png_dir / f"{frame}.png"
        svg = svg_dir / f"{frame}.svg"
        if not png.exists() or not svg.exists():
            raise SystemExit(f"missing avatar frame: {png}")
        if tint is not None and item_id == "penguin" and sprite_id == 229:
            args.extend([
                "(", str(png), "-channel", "RGB", "-colorspace", "gray",
                "+level-colors", f"#101010,{tint}", "+channel", ")",
            ])
            tx, ty = avatar_origin(svg)
            args.extend([
                "-geometry",
                f"+{round(AVATAR_ORIGIN_X - tx)}+"
                f"{round(AVATAR_ORIGIN_Y - ty)}",
                "-composite",
            ])
        else:
            args.extend(avatar_place_args(png, svg))
    args.extend([
        "-resize", "52x52!", "-channel", "A", "-threshold", "40%",
        "+channel", "-background", "magenta", "-alpha", "remove",
        "-alpha", "off", "-gravity", "center", "-extent", "52x56",
        f"BMP3:{output}",
    ])
    run_magick(args)


def avatar_legacy_frame(export: Path, item_id: str, sprite_id: int,
                        frame: int, translate_x: int, translate_y: int,
                        output: Path) -> None:
    """Render an authentic pre-193-frame directional clothing timeline."""
    png_dir = (avatar_item_root(export, "png", item_id) /
               f"DefineSprite_{sprite_id}")
    svg_dir = (avatar_item_root(export, "svg", item_id) /
               f"DefineSprite_{sprite_id}")
    png = png_dir / f"{frame}.png"
    svg = svg_dir / f"{frame}.svg"
    if not png.exists() or not svg.exists():
        raise SystemExit(f"missing legacy avatar frame: {png}")

    tx, ty = avatar_origin(svg)
    scale = 0.099990845
    x = round(AVATAR_ORIGIN_X + translate_x / 20.0 - tx * scale)
    y = round(AVATAR_ORIGIN_Y + translate_y / 20.0 - ty * scale)
    run_magick([
        "-size", f"{AVATAR_CANVAS}x{AVATAR_CANVAS}", "canvas:none",
        "(", str(png), "-resize", f"{scale * 100}%", ")",
        "-geometry", f"+{x}+{y}", "-composite",
        "-resize", "52x52!", "-channel", "A", "-threshold", "40%",
        "+channel", "-background", "magenta", "-alpha", "remove",
        "-alpha", "off", "-gravity", "center", "-extent", "52x56",
        f"BMP3:{output}",
    ])


def avatar_strip(frame_paths: list[Path], output: Path) -> None:
    run_magick([
        *[str(path) for path in frame_paths], "+append", f"BMP3:{output}"
    ])


def package_avatar(avatar_export: Path, waddle_source: Path, out: Path,
                   generated: list[Path]) -> None:
    """Build Rockbox strips from preserved Club Penguin move timelines."""
    avatar_out = out / "avatar"
    avatar_out.mkdir(parents=True, exist_ok=True)
    temporary: list[Path] = []
    avatar_generated: list[Path] = []

    base_png = avatar_export / "png" / "penguin"
    base_svg = avatar_export / "svg" / "penguin"
    for sprite_id in (229, 273):
        if not (base_png / f"DefineSprite_{sprite_id}").is_dir() or not (
                base_svg / f"DefineSprite_{sprite_id}").is_dir():
            raise SystemExit(f"missing official penguin sprite {sprite_id}")

    for slot, (_, tint) in enumerate(COLOR_ITEMS):
        frames: list[Path] = []
        for index, source_frame in enumerate(AVATAR_FRAMES):
            frame = avatar_out / f".color{slot}_{index}.bmp"
            avatar_frame(
                avatar_export,
                [("penguin", 229), ("penguin", 273)],
                source_frame,
                frame,
                tint,
            )
            frames.append(frame)
            temporary.append(frame)

        # The final frame is the map selector, made from authentic penguin
        # art at the same reduced scale used by the original port.
        selector = avatar_out / f".color{slot}_selector.bmp"
        run_magick([
            str(frames[0]), "-transparent", "magenta", "-trim", "+repage",
            "-resize", "20x22", "-channel", "A", "-threshold", "50%",
            "+channel", "-gravity", "south", "-background", "magenta",
            "-alpha", "remove", "-alpha", "off", "-extent", "52x56",
            f"BMP3:{selector}",
        ])
        temporary.append(selector)
        output = avatar_out / f"color_{slot}.bmp"
        avatar_strip([*frames, selector], output)
        generated.append(output)
        avatar_generated.append(output)

    for page in (0, 1, 2, 4):
        for slot, (item_id, _, _) in enumerate(AVATAR_ITEMS[page]):
            png_dir, _ = avatar_master(avatar_export, str(item_id))
            sprite_id = int(png_dir.name.rsplit("_", 1)[1])
            frames = []
            for index, source_frame in enumerate(AVATAR_FRAMES):
                frame = avatar_out / f".p{page}s{slot}_{index}.bmp"
                avatar_frame(
                    avatar_export,
                    [(str(item_id), sprite_id)],
                    source_frame,
                    frame,
                )
                frames.append(frame)
                temporary.append(frame)
            blank = avatar_out / f".p{page}s{slot}_selector.bmp"
            run_magick([
                "-size", "52x56", "canvas:magenta", f"BMP3:{blank}"
            ])
            temporary.append(blank)
            output = avatar_out / f"page{page}_{slot}.bmp"
            avatar_strip([*frames, blank], output)
            generated.append(output)
            avatar_generated.append(output)

    for path in temporary:
        path.unlink()

    source_root = (waddle_source / "media/clothing/slegacy/media/play/v2/"
                   "content/global/clothing/sprites")
    manifest = avatar_out / "source.manifest"
    source_commit = git_commit(waddle_source)
    if source_commit == "unknown":
        source_commit = WADDLE_SOURCE_COMMIT
    lines = [
        "CLUBPENGUIN_AVATAR_ASSET_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={source_commit}",
        "penguin_source=media/default/slegacy/media/play/v2/content/"
        "global/penguin/penguin.swf",
        "penguin_sprites=229:color,273:features",
        "move_frames=1,5,9,19,20,21,22,29,36,39,40,41",
        "move_directions=down,left,up,right",
        "extractor=JPEXS_FFDec_26.2.1",
    ]
    penguin = (waddle_source / "media/default/slegacy/media/play/v2/"
               "content/global/penguin/penguin.swf")
    lines.append(f"penguin_sha256={sha256(penguin)}")
    for page in (0, 1, 2, 4):
        for slot, (item_id, name, _) in enumerate(AVATAR_ITEMS[page]):
            swf = source_root / f"{item_id}.swf"
            lines.append(
                f"item={page}:{slot}:{item_id}:{name}\t{sha256(swf)}"
            )
    lines.extend([
        "output=15 color strips plus 48 authentic item strips",
        "strip=13x52x56 transparent=magenta",
    ])
    for path in avatar_generated:
        lines.append(f"generated={path.name}\t{sha256(path)}")
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.append(manifest)


def package_sport_catalog(sport_export: Path, avatar_export: Path,
                          out: Path, generated: list[Path]) -> None:
    """Package the preserved Snow and Sports catalog and paper dolls."""
    page_out = out / "shop" / "sport"
    avatar_out = out / "avatar"
    data_out = out / "data" / "sport_shop.tsv"
    manifest = page_out / "source.manifest"
    catalog_swf = sport_export / "sport_cpip.swf"
    temporary: list[Path] = []
    outputs: list[Path] = []

    if not catalog_swf.exists():
        raise SystemExit(f"missing preserved sport catalog: {catalog_swf}")
    page_out.mkdir(parents=True, exist_ok=True)
    avatar_out.mkdir(parents=True, exist_ok=True)
    data_out.parent.mkdir(parents=True, exist_ok=True)

    for page in range(1, 10):
        source = sport_export / "frames" / f"{page}.png"
        output = page_out / f"page{page}.bmp"
        if not source.exists():
            raise SystemExit(f"missing sport catalog page: {source}")
        run_magick([
            str(source), "-resize", "320x220!", "-background", "magenta",
            "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
        ])
        outputs.append(output)

    with data_out.open("w", encoding="utf-8") as catalog:
        catalog.write("# page\tid\ttype\tname\tcost\n")
        for row in SPORT_ITEMS:
            catalog.write("\t".join(str(value) for value in row) + "\n")
    outputs.append(data_out)

    for item_id in sorted(SPORT_WEARABLE_IDS):
        png_dir, _ = avatar_master(avatar_export, str(item_id))
        sprite_id = int(png_dir.name.rsplit("_", 1)[1])
        frames: list[Path] = []
        for index, source_frame in enumerate(AVATAR_FRAMES):
            frame = avatar_out / f".sport{item_id}_{index}.bmp"
            avatar_frame(
                avatar_export,
                [(str(item_id), sprite_id)],
                source_frame,
                frame,
            )
            frames.append(frame)
            temporary.append(frame)
        blank = avatar_out / f".sport{item_id}_selector.bmp"
        run_magick([
            "-size", "52x56", "canvas:magenta", f"BMP3:{blank}",
        ])
        temporary.append(blank)
        output = avatar_out / f"sport_{item_id}.bmp"
        avatar_strip([*frames, blank], output)
        outputs.append(output)

    for path in temporary:
        path.unlink()

    lines = [
        "CLUBPENGUIN_SPORT_CATALOG_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "catalog_source=media/default/recreation/catalog/sport_cpip.swf"
        f"\t{sha256(catalog_swf)}",
        "extractor=JPEXS_FFDec_26.2.1",
        "catalog_pages=9",
        f"catalog_items={len(SPORT_ITEMS)}",
    ]
    for item_id in sorted(SPORT_WEARABLE_IDS):
        source = avatar_export / "source" / f"{item_id}.swf"
        if not source.exists():
            raise SystemExit(f"missing sport paper-doll SWF: {source}")
        prefix = "media/default" if item_id in (321, 703) else "media/clothing"
        lines.append(
            f"paper_source={prefix}/slegacy/media/play/v2/content/global/"
            f"clothing/sprites/{item_id}.swf\t{sha256(source)}"
        )
    for path in outputs:
        lines.append(
            f"generated={path.relative_to(out)}\t{sha256(path)}"
        )
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_costume_catalog(costume_export: Path, avatar_export: Path,
                            out: Path, generated: list[Path]) -> None:
    """Package the matching Ruby and the Ruby Costume Trunk."""
    page_out = out / "shop" / "costume"
    avatar_out = out / "avatar"
    data_out = out / "data" / "costume_shop.tsv"
    manifest = page_out / "source.manifest"
    catalog_swf = costume_export / "Apr2012Costume.swf"
    temporary: list[Path] = []
    outputs: list[Path] = []

    if not catalog_swf.exists():
        raise SystemExit(f"missing preserved costume catalog: {catalog_swf}")
    page_out.mkdir(parents=True, exist_ok=True)
    avatar_out.mkdir(parents=True, exist_ok=True)
    data_out.parent.mkdir(parents=True, exist_ok=True)

    for page in range(1, 7):
        source = costume_export / "frames" / f"{page}.png"
        output = page_out / f"page{page}.bmp"
        if not source.exists():
            raise SystemExit(f"missing costume catalog page: {source}")
        run_magick([
            str(source), "-resize", "320x220!", "-background", "magenta",
            "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
        ])
        outputs.append(output)

    with data_out.open("w", encoding="utf-8") as catalog:
        catalog.write("# page\tid\ttype\tname\tcost\n")
        for row in COSTUME_ITEMS:
            catalog.write("\t".join(str(value) for value in row) + "\n")
    outputs.append(data_out)

    for item_id in sorted(COSTUME_WEARABLE_IDS):
        png_dir, _ = avatar_master(avatar_export, str(item_id))
        sprite_id = int(png_dir.name.rsplit("_", 1)[1])
        frames: list[Path] = []
        for index, source_frame in enumerate(AVATAR_FRAMES):
            frame = avatar_out / f".costume{item_id}_{index}.bmp"
            avatar_frame(
                avatar_export,
                [(str(item_id), sprite_id)],
                source_frame,
                frame,
            )
            frames.append(frame)
            temporary.append(frame)
        blank = avatar_out / f".costume{item_id}_selector.bmp"
        run_magick([
            "-size", "52x56", "canvas:magenta", f"BMP3:{blank}",
        ])
        temporary.append(blank)
        # The runtime shares one authentic catalog-layer namespace across
        # Snow and Sports and the Costume Trunk, keyed by original item ID.
        output = avatar_out / f"sport_{item_id}.bmp"
        avatar_strip([*frames, blank], output)
        outputs.append(output)

    for path in temporary:
        path.unlink()

    lines = [
        "CLUBPENGUIN_COSTUME_CATALOG_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "catalog_source=media/default/archives/Apr2012Costume.swf"
        f"\t{sha256(catalog_swf)}",
        "matching_room=media/default/archives/04252012Stage.swf",
        "play=Ruby and the Ruby",
        "release=2012-04-26",
        "extractor=JPEXS_FFDec_26.2.1",
        "catalog_pages=6",
        f"catalog_items={len(COSTUME_ITEMS)}",
    ]
    for item_id in sorted(COSTUME_WEARABLE_IDS):
        source = avatar_export / "source" / f"{item_id}.swf"
        if not source.exists():
            raise SystemExit(f"missing costume paper-doll SWF: {source}")
        lines.append(
            "paper_source=media/clothing/slegacy/media/play/v2/content/"
            f"global/clothing/sprites/{item_id}.swf\t{sha256(source)}"
        )
    for path in outputs:
        lines.append(
            f"generated={path.relative_to(out)}\t{sha256(path)}"
        )
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_penguin_style_catalog(catalog_export: Path,
                                  avatar_export: Path, out: Path,
                                  generated: list[Path]) -> None:
    """Package the preserved April 2012 Penguin Style catalog."""
    page_out = out / "shop" / "penguin_style"
    avatar_out = out / "avatar"
    data_out = out / "data" / "penguin_style_shop.tsv"
    manifest = page_out / "source.manifest"
    catalog_swf = catalog_export / "PenguinStyleApr2012.swf"
    catalog_tsv = catalog_export / "penguin_style_shop.tsv"
    paper_items = catalog_export / "paper_items.json"
    player_colors = catalog_export / "player_colors.json"
    temporary: list[Path] = []
    outputs: list[Path] = []
    wearable_ids: set[int] = set()
    item_count = 0

    for required in (catalog_swf, catalog_tsv, paper_items, player_colors):
        if not required.exists():
            raise SystemExit(f"missing Penguin Style source: {required}")
    page_out.mkdir(parents=True, exist_ok=True)
    avatar_out.mkdir(parents=True, exist_ok=True)
    data_out.parent.mkdir(parents=True, exist_ok=True)

    for page in range(1, PENGUIN_STYLE_PAGES + 1):
        source = catalog_export / "frames" / f"{page}.png"
        output = page_out / f"page{page}.bmp"
        if not source.exists():
            raise SystemExit(f"missing Penguin Style page: {source}")
        run_magick([
            str(source), "-resize", "320x220!", "-background", "magenta",
            "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
        ])
        outputs.append(output)

    for raw in catalog_tsv.read_text(encoding="utf-8").splitlines():
        if not raw or raw.startswith("#"):
            continue
        fields = raw.split("\t")
        if len(fields) != 5:
            raise SystemExit(f"invalid Penguin Style row: {raw}")
        page, item_id, item_type, _, cost = fields
        values = tuple(int(value) for value in
                       (page, item_id, item_type, cost))
        if (values[0] < 1 or values[0] > PENGUIN_STYLE_PAGES or
                values[1] <= 0 or values[2] < 0 or values[2] > 9 or
                values[3] < 0):
            raise SystemExit(f"invalid Penguin Style item: {raw}")
        item_count += 1
        if values[2] in {0, 1, 2, 3, 4, 5}:
            wearable_ids.add(values[1])
    if item_count != PENGUIN_STYLE_ITEMS:
        raise SystemExit(
            f"expected {PENGUIN_STYLE_ITEMS} Penguin Style items, "
            f"found {item_count}"
        )
    if len(wearable_ids) != PENGUIN_STYLE_WEARABLES:
        raise SystemExit(
            f"expected {PENGUIN_STYLE_WEARABLES} Penguin Style wearables, "
            f"found {len(wearable_ids)}"
        )
    shutil.copyfile(catalog_tsv, data_out)
    outputs.append(data_out)

    for item_id in sorted(wearable_ids):
        frames: list[Path] = []
        if item_id == 5133:
            # Blue Water Bottle predates the 193-frame master. Its original
            # root timeline maps the official S/W/N/E eight-frame walk clips
            # below at the recorded Flash placement matrices.
            legacy_directions = (
                (25, -5, -302), (43, 56, -260),
                (61, -2, -198), (79, -58, -256),
            )
            for direction, (sprite_id, tx, ty) in enumerate(
                    legacy_directions):
                for phase, source_frame in enumerate((1, 4, 8)):
                    frame = avatar_out / (
                        f".style{item_id}_{direction}_{phase}.bmp"
                    )
                    avatar_legacy_frame(
                        avatar_export, str(item_id), sprite_id,
                        source_frame, tx, ty, frame,
                    )
                    frames.append(frame)
                    temporary.append(frame)
        else:
            if item_id == 4535:
                # Winter Threads is two official layers. The root SWF places
                # sprite 197 behind sprite 235, so retain that depth order.
                sources = [(str(item_id), 197), (str(item_id), 235)]
            else:
                png_dir, _ = avatar_master(avatar_export, str(item_id))
                sprite_id = int(png_dir.name.rsplit("_", 1)[1])
                sources = [(str(item_id), sprite_id)]
            for index, source_frame in enumerate(AVATAR_FRAMES):
                frame = avatar_out / f".style{item_id}_{index}.bmp"
                avatar_frame(
                    avatar_export, sources, source_frame, frame,
                )
                frames.append(frame)
                temporary.append(frame)

        blank = avatar_out / f".style{item_id}_selector.bmp"
        run_magick([
            "-size", "52x56", "canvas:magenta", f"BMP3:{blank}",
        ])
        temporary.append(blank)
        output = avatar_out / f"sport_{item_id}.bmp"
        avatar_strip([*frames, blank], output)
        outputs.append(output)

    for path in temporary:
        path.unlink()

    lines = [
        "CLUBPENGUIN_PENGUIN_STYLE_CATALOG_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "catalog_source=media/default/archives/PenguinStyleApr2012.swf"
        f"\t{sha256(catalog_swf)}",
        f"paper_items_metadata={sha256(paper_items)}",
        f"player_colors_metadata={sha256(player_colors)}",
        "release=2012-04",
        "extractor=JPEXS_FFDec_26.2.1",
        f"catalog_pages={PENGUIN_STYLE_PAGES}",
        f"catalog_items={item_count}",
        f"wearable_items={len(wearable_ids)}",
        "legacy_directional_item=5133:25,43,61,79:frames_1,4,8",
        "composite_item=4535:197,235:official_depth_order",
    ]
    for item_id in sorted(wearable_ids):
        source = avatar_export / "source" / f"{item_id}.swf"
        if not source.exists():
            raise SystemExit(f"missing Penguin Style paper-doll SWF: {source}")
        lines.append(
            "paper_source=media/clothing/slegacy/media/play/v2/content/"
            f"global/clothing/sprites/{item_id}.swf\t{sha256(source)}"
        )
    for path in outputs:
        lines.append(
            f"generated={path.relative_to(out)}\t{sha256(path)}"
        )
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_ninja_catalog(catalog_export: Path, avatar_export: Path,
                          out: Path, generated: list[Path]) -> None:
    """Package the preserved December 2011 Martial Artworks catalog."""
    page_out = out / "shop" / "ninja"
    avatar_out = out / "avatar"
    data_out = out / "data" / "ninja_catalog.tsv"
    manifest = page_out / "source.manifest"
    catalog_swf = catalog_export / "ENCataloguesNinjaDecember2011.swf"
    catalog_tsv = catalog_export / "ninja_catalog.tsv"
    frames = catalog_export / "catalog_frames" / "DefineSprite_1231"
    outputs: list[Path] = []
    temporary: list[Path] = []
    wearable_ids: set[int] = set()
    runtime_rows: list[tuple[int, int, int, str, int, int]] = []

    for required in (catalog_swf, catalog_tsv):
        if not required.exists():
            raise SystemExit(f"missing Martial Artworks source: {required}")
    page_out.mkdir(parents=True, exist_ok=True)
    avatar_out.mkdir(parents=True, exist_ok=True)
    data_out.parent.mkdir(parents=True, exist_ok=True)

    for page in range(1, NINJA_CATALOG_PAGES + 1):
        source = frames / f"{page}.png"
        output = page_out / f"page{page}.bmp"
        if not source.exists():
            raise SystemExit(f"missing Martial Artworks page: {source}")
        run_magick([
            str(source), "-resize", "320x220!", "-background", "magenta",
            "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
        ])
        outputs.append(output)

    paper_type = {2: 0, 5: 1, 6: 5}
    for raw in catalog_tsv.read_text(encoding="utf-8").splitlines():
        if not raw or raw.startswith("#"):
            continue
        fields = raw.split("\t")
        if len(fields) != 8:
            raise SystemExit(f"invalid Martial Artworks row: {raw}")
        page = int(fields[0])
        kind = fields[1]
        item_id = int(fields[2])
        name = fields[3]
        cost = int(fields[4])
        source_type = int(fields[5])
        maximum = int(fields[7])
        if (page < 1 or page > NINJA_CATALOG_PAGES or item_id <= 0 or
                cost < 0 or maximum < 1):
            raise SystemExit(f"invalid Martial Artworks item: {raw}")
        if kind == "clothing":
            if source_type not in paper_type:
                raise SystemExit(
                    f"unknown Martial Artworks clothing type: {raw}"
                )
            item_type = paper_type[source_type]
            wearable_ids.add(item_id)
        elif kind == "furniture":
            item_type = 7
        elif kind == "building":
            item_type = 10
        else:
            raise SystemExit(f"unknown Martial Artworks item kind: {raw}")
        runtime_rows.append(
            (page, item_id, item_type, name, cost, maximum)
        )

    if len(runtime_rows) != NINJA_CATALOG_ITEMS:
        raise SystemExit(
            f"expected {NINJA_CATALOG_ITEMS} Martial Artworks items, "
            f"found {len(runtime_rows)}"
        )
    if len(wearable_ids) != NINJA_CATALOG_WEARABLES:
        raise SystemExit(
            f"expected {NINJA_CATALOG_WEARABLES} Martial Artworks "
            f"wearables, found {len(wearable_ids)}"
        )
    with data_out.open("w", encoding="utf-8") as catalog:
        catalog.write("# page\tid\ttype\tname\tcost\tmax_quantity\n")
        for row in runtime_rows:
            catalog.write("\t".join(str(value) for value in row) + "\n")
    outputs.append(data_out)

    for item_id in sorted(wearable_ids):
        item_frames: list[Path] = []
        if item_id == 5012:
            # Hand Gong predates the 193-frame paper-doll master. Its root
            # timeline places these official S/W/N/E eight-frame sprites at
            # the transforms below. Keep three walk phases per direction.
            legacy_directions = (
                (25, -5, -302), (43, 56, -260),
                (61, -2, -198), (79, -58, -240),
            )
            for direction, (sprite_id, tx, ty) in enumerate(
                    legacy_directions):
                for phase, source_frame in enumerate((1, 4, 8)):
                    frame = avatar_out / (
                        f".ninja{item_id}_{direction}_{phase}.bmp"
                    )
                    avatar_legacy_frame(
                        avatar_export, str(item_id), sprite_id,
                        source_frame, tx, ty, frame,
                    )
                    item_frames.append(frame)
                    temporary.append(frame)
        else:
            if item_id == 4034:
                # Ninja Outfit is two simultaneous official root layers:
                # sprite 194 at depth 1 and sprite 221 at depth 4.
                sources = [(str(item_id), 194), (str(item_id), 221)]
            else:
                png_dir, _ = avatar_master(avatar_export, str(item_id))
                sprite_id = int(png_dir.name.rsplit("_", 1)[1])
                sources = [(str(item_id), sprite_id)]
            for index, source_frame in enumerate(AVATAR_FRAMES):
                frame = avatar_out / f".ninja{item_id}_{index}.bmp"
                avatar_frame(
                    avatar_export, sources, source_frame, frame,
                )
                item_frames.append(frame)
                temporary.append(frame)
        blank = avatar_out / f".ninja{item_id}_selector.bmp"
        run_magick([
            "-size", "52x56", "canvas:magenta", f"BMP3:{blank}",
        ])
        temporary.append(blank)
        output = avatar_out / f"sport_{item_id}.bmp"
        avatar_strip([*item_frames, blank], output)
        outputs.append(output)

    for path in temporary:
        path.unlink()

    lines = [
        "CLUBPENGUIN_MARTIAL_ARTWORKS_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "catalog_source=media/default/archives/"
        "ENCataloguesNinjaDecember2011.swf"
        f"\t{sha256(catalog_swf)}",
        f"catalog_calls={sha256(catalog_tsv)}",
        "release=2011-12",
        "extractor=JPEXS_FFDec_26.2.1",
        f"catalog_pages={NINJA_CATALOG_PAGES}",
        f"catalog_items={len(runtime_rows)}",
        f"wearable_items={len(wearable_ids)}",
        "legacy_directional_item=5012:25,43,61,79:frames_1,4,8",
        "composite_item=4034:194,221:official_depth_order",
    ]
    for item_id in sorted(wearable_ids):
        source = avatar_export / "source" / f"{item_id}.swf"
        if not source.exists():
            raise SystemExit(
                f"missing Martial Artworks paper-doll SWF: {source}"
            )
        lines.append(
            "paper_source=media/clothing/slegacy/media/play/v2/content/"
            f"global/clothing/sprites/{item_id}.swf\t{sha256(source)}"
        )
    for path in outputs:
        lines.append(
            f"generated={path.relative_to(out)}\t{sha256(path)}"
        )
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_puffle_adoption(catalog_export: Path, out: Path,
                            generated: list[Path]) -> None:
    """Package the exact February 2011 ten-color adoption catalog."""
    page_out = out / "puffles" / "adopt"
    data_out = out / "data" / "puffle_adopt.tsv"
    manifest = page_out / "source.manifest"
    catalog_swf = catalog_export / "Feb2011Adopt.swf"
    frames = catalog_export / "frames"
    outputs: list[Path] = []

    if not catalog_swf.exists():
        raise SystemExit(
            f"missing February 2011 adoption catalog: {catalog_swf}"
        )
    page_out.mkdir(parents=True, exist_ok=True)
    data_out.parent.mkdir(parents=True, exist_ok=True)

    for page in range(1, PUFFLE_ADOPT_PAGES + 1):
        source = frames / f"{page}.png"
        output = page_out / f"page{page}.bmp"
        if not source.exists():
            raise SystemExit(f"missing adoption catalog page: {source}")
        run_magick([
            str(source), "-resize", "320x220!", "-background", "magenta",
            "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
        ])
        outputs.append(output)

    with data_out.open("w", encoding="utf-8") as catalog:
        catalog.write("# page\ttype\tname\tcost\n")
        for row in PUFFLE_ADOPT_ITEMS:
            catalog.write("\t".join(str(value) for value in row) + "\n")
    outputs.append(data_out)

    lines = [
        "CLUBPENGUIN_PUFFLE_ADOPTION_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "catalog_source=media/default/archives/Feb2011Adopt.swf"
        f"\t{sha256(catalog_swf)}",
        "release=2011-02",
        "extractor=JPEXS_FFDec_26.2.1",
        f"catalog_pages={PUFFLE_ADOPT_PAGES}",
        f"adopt_calls={len(PUFFLE_ADOPT_ITEMS)}",
        "adopt_types=0,5,1,2,3,6,4,7,8,9",
    ]
    for path in outputs:
        lines.append(
            f"generated={path.relative_to(out)}\t{sha256(path)}"
        )
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_pet_furniture(catalog_export: Path, out: Path,
                          generated: list[Path]) -> None:
    """Package the exact March 2010 Pet Furniture catalog and secrets."""
    page_out = out / "puffles" / "furniture"
    data_out = out / "data" / "pet_furniture.tsv"
    manifest = page_out / "source.manifest"
    catalog_swf = catalog_export / "Mar2010Pets.swf"
    frames = catalog_export / "frames"
    secrets = catalog_export / "secret_sprites"
    secret_pages = {
        2: (secrets / "DefineSprite_179" / "2.png", 380, 207),
        3: (secrets / "DefineSprite_216" / "2.png", 380, 195),
        4: (secrets / "DefineSprite_268" / "2.png", 380, 195),
    }
    outputs: list[Path] = []

    if not catalog_swf.exists():
        raise SystemExit(f"missing March 2010 Pet Furniture: {catalog_swf}")
    page_out.mkdir(parents=True, exist_ok=True)
    data_out.parent.mkdir(parents=True, exist_ok=True)

    for page in range(1, PET_FURNITURE_PAGES + 1):
        source = frames / f"{page}.png"
        output = page_out / f"page{page}.bmp"

        if not source.exists():
            raise SystemExit(f"missing Pet Furniture page: {source}")
        run_magick([
            str(source), "-resize", "320x220!", "-background", "magenta",
            "-alpha", "remove", "-alpha", "off", f"BMP3:{output}",
        ])
        outputs.append(output)
        if page in secret_pages:
            overlay, x, y = secret_pages[page]
            prepared = page_out / f".page{page}_secret.png"
            secret_output = page_out / f"page{page}_secret.bmp"

            if not overlay.exists():
                raise SystemExit(
                    f"missing official Pet Furniture secret: {overlay}"
                )
            run_magick([
                str(source), str(overlay), "-geometry", f"+{x}+{y}",
                "-composite", str(prepared),
            ])
            run_magick([
                str(prepared), "-resize", "320x220!", "-background",
                "magenta", "-alpha", "remove", "-alpha", "off",
                f"BMP3:{secret_output}",
            ])
            prepared.unlink()
            outputs.append(secret_output)

    with data_out.open("w", encoding="utf-8") as catalog:
        catalog.write(
            "# page\tsecret\tid\tname\tcost\ttype\tmember\tmax_quantity\n"
        )
        for row in PET_FURNITURE_ITEMS:
            catalog.write("\t".join(str(value) for value in row) + "\n")
    outputs.append(data_out)

    lines = [
        "CLUBPENGUIN_PET_FURNITURE_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "catalog_source=media/default/archives/Mar2010Pets.swf"
        f"\t{sha256(catalog_swf)}",
        "release=2010-03",
        "extractor=JPEXS_FFDec_26.2.1",
        f"catalog_pages={PET_FURNITURE_PAGES}",
        f"purchase_calls={len(PET_FURNITURE_ITEMS)}",
        "secret_pages=2:220,3:223+204,4:202",
    ]
    for page, (overlay, x, y) in secret_pages.items():
        lines.append(
            f"secret_source=page{page}:{overlay.name}@{x},{y}"
            f"\t{sha256(overlay)}"
        )
    for path in outputs:
        lines.append(f"generated={path.relative_to(out)}\t{sha256(path)}")
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_igloo_catalogs(catalog_export: Path, out: Path,
                           generated: list[Path]) -> None:
    """Package the matching 2012 Furniture and Igloo catalog pages."""
    catalog_out = out / "igloo" / "catalog"
    furniture_out = catalog_out / "furniture"
    upgrades_out = catalog_out / "upgrades"
    furniture_swf = catalog_export / "Apr2012Furniture.swf"
    upgrades_swf = catalog_export / "February2012Igloo.swf"
    furniture_tsv = catalog_export / "apr2012_furniture.tsv"
    upgrades_tsv = catalog_export / "feb2012_igloo.tsv"
    furniture_sprites = catalog_export / "furniture_sprite_frames"
    manifest = catalog_out / "source.manifest"
    outputs: list[Path] = []

    for required in (furniture_swf, upgrades_swf, furniture_tsv,
                     upgrades_tsv):
        if not required.exists():
            raise SystemExit(f"missing preserved igloo catalog: {required}")
    furniture_out.mkdir(parents=True, exist_ok=True)
    upgrades_out.mkdir(parents=True, exist_ok=True)
    (out / "data").mkdir(parents=True, exist_ok=True)

    catalogs = (
        ("furniture", 14, catalog_export / "furniture_clean_frames",
         furniture_out),
        ("upgrades", 11, catalog_export / "igloo_frames", upgrades_out),
    )
    for name, pages, frames, destination in catalogs:
        for page in range(1, pages + 1):
            source = frames / f"{page}.png"
            output = destination / f"page{page}.bmp"
            if not source.exists():
                raise SystemExit(f"missing {name} catalog page: {source}")
            prepared_source = source
            temporary: list[Path] = []
            if name == "furniture" and page == 13:
                # FFDec's main-timeline exporter omits DefineSprite 853 on
                # this frame, although the official Flash player displays it
                # at root depth 34. Reapply that exact embedded sprite at its
                # SWF transform, then restore the official higher-depth title,
                # price, and close-button pixels from the same source frame.
                # This is a depth-correct reconstruction from archive art;
                # no catalog pixels are drawn or synthesized.
                sprite_paths = {
                    sprite_id: furniture_sprites /
                    f"DefineSprite_{sprite_id}" / "1.png"
                    for sprite_id in (821, 823, 853)
                }
                for sprite_path in sprite_paths.values():
                    if not sprite_path.exists():
                        raise SystemExit(
                            "missing April 2012 furniture page 13 sprite: "
                            f"{sprite_path}"
                        )
                layered = furniture_out / ".page13_layered.png"
                run_magick([
                    str(source), str(sprite_paths[853]),
                    "-geometry", "+80+34", "-composite",
                    str(sprite_paths[821]), "-geometry", "+70+21",
                    "-composite", str(sprite_paths[823]),
                    "-geometry", "+380+21", "-composite",
                    str(layered),
                ])
                temporary.append(layered)
                foreground = (
                    (250, 100, 82, 30), (421, 163, 82, 30),
                    (564, 163, 82, 30), (107, 312, 82, 30),
                    (252, 310, 82, 30), (421, 326, 82, 30),
                    (567, 326, 82, 30), (658, 27, 32, 32),
                )
                composite_args = [str(layered)]
                for index, (x, y, width, height) in enumerate(foreground):
                    crop = furniture_out / f".page13_foreground_{index}.png"
                    run_magick([
                        str(source), "-crop",
                        f"{width}x{height}+{x}+{y}", "+repage", str(crop),
                    ])
                    composite_args.extend([
                        str(crop), "-geometry", f"+{x}+{y}",
                        "-composite",
                    ])
                    temporary.append(crop)
                recovered = furniture_out / ".page13_recovered.png"
                run_magick([*composite_args, str(recovered)])
                temporary.append(recovered)
                prepared_source = recovered
            run_magick([
                str(prepared_source), "-resize", "320x220!", "-background",
                "magenta", "-alpha", "remove", "-alpha", "off",
                f"BMP3:{output}",
            ])
            for path in temporary:
                path.unlink()
            outputs.append(output)

    data_outputs = (
        (furniture_tsv, out / "data" / "apr2012_furniture.tsv"),
        (upgrades_tsv, out / "data" / "feb2012_igloo.tsv"),
    )
    for source, output in data_outputs:
        shutil.copyfile(source, output)
        outputs.append(output)

    lines = [
        "CLUBPENGUIN_IGLOO_CATALOG_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "furniture_source=media/default/archives/Apr2012Furniture.swf"
        f"\t{sha256(furniture_swf)}",
        "upgrades_source=media/default/archives/February2012Igloo.swf"
        f"\t{sha256(upgrades_swf)}",
        "furniture_release=2012-04",
        "upgrades_release=2012-02",
        "furniture_pages=14",
        "furniture_items=120",
        "upgrade_pages=11",
        "upgrade_items=28",
        "furniture_visibility=official root secret clip hidden by ActionScript",
        "furniture_page13=official embedded sprites 853,821,823 at root "
        "depth order and SWF transforms",
        "extractor=JPEXS_FFDec_26.2.1",
    ]
    for path in outputs:
        lines.append(
            f"generated={path.relative_to(out)}\t{sha256(path)}"
        )
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_cart_surfer(cart_export: Path, waddle_source: Path, out: Path,
                        generated: list[Path]) -> None:
    cart_out = out / "minigames" / "cart_surfer"
    data_out = out / "data"
    source_swf = waddle_source / "media/default/fix/CartSurfer2006.swf"
    title_source = cart_export / "frames" / "1.png"
    tunnel_source = (cart_export / "sprites" / "DefineSprite_201" /
                     "1.png")
    cart_source = cart_export / "sprites" / "DefineSprite_173"
    cart_frames = [1, 9, 5, 10, 15, 22]
    track_frames = [1, 2, 3, 4]

    required = [source_swf, title_source, tunnel_source]
    required.extend(cart_source / f"{frame}.png" for frame in cart_frames)
    required.extend(
        cart_export / "sprites" / "DefineSprite_201" / f"{frame}.png"
        for frame in track_frames
    )
    for path in required:
        if not path.exists():
            raise SystemExit(f"missing Cart Surfer extraction: {path}")
    if shutil.which("magick") is None:
        raise SystemExit("ImageMagick is required to package Cart Surfer")

    cart_out.mkdir(parents=True, exist_ok=True)
    title_out = cart_out / "title.bmp"
    tunnel_out = cart_out / "tunnel.bmp"
    strip_out = cart_out / "cart.bmp"

    run_ffmpeg(["-i", str(title_source), "-vf", "scale=320:220",
                "-pix_fmt", "bgr24", str(title_out)])
    run_ffmpeg(["-i", str(tunnel_source), "-vf", "scale=320:220",
                "-pix_fmt", "bgr24", str(tunnel_out)])

    prepared: list[Path] = []
    for index, frame in enumerate(cart_frames):
        prepared_frame = cart_out / f".cart_frame_{index}.bmp"
        run_magick([
            str(cart_source / f"{frame}.png"), "-trim", "+repage",
            "-resize", "38x38", "-gravity", "center", "-background",
            "magenta", "-alpha", "remove", "-alpha", "off", "-extent",
            "40x40", f"BMP3:{prepared_frame}",
        ])
        prepared.append(prepared_frame)

    hstack_args: list[str] = []
    for frame in prepared:
        hstack_args.extend(["-i", str(frame)])
    cart_row = cart_out / ".cart_row.bmp"
    run_ffmpeg([
        *hstack_args,
        "-filter_complex", f"hstack=inputs={len(prepared)},"
        "pad=320:40:0:0:color=magenta,format=bgr24",
        str(cart_row),
    ])
    for frame in prepared:
        frame.unlink()

    track_prepared: list[Path] = []
    tunnel_frames = cart_export / "sprites" / "DefineSprite_201"
    for index, frame in enumerate(track_frames):
        prepared_frame = cart_out / f".track_frame_{index}.bmp"
        run_ffmpeg([
            "-i", str(tunnel_frames / f"{frame}.png"),
            "-vf", "scale=320:220,crop=80:24:120:125",
            "-pix_fmt", "bgr24", str(prepared_frame),
        ])
        track_prepared.append(prepared_frame)

    hstack_args = []
    for frame in track_prepared:
        hstack_args.extend(["-i", str(frame)])
    track_row = cart_out / ".track_row.bmp"
    run_ffmpeg([
        *hstack_args,
        "-filter_complex", f"hstack=inputs={len(track_prepared)},"
        "format=bgr24", str(track_row),
    ])
    run_ffmpeg([
        "-i", str(cart_row), "-i", str(track_row),
        "-filter_complex", "vstack=inputs=2,format=bgr24", str(strip_out),
    ])
    cart_row.unlink()
    track_row.unlink()
    for frame in track_prepared:
        frame.unlink()

    cart_data = data_out / "cart_surfer.tsv"
    cart_data.write_text(
        "# key\tvalue\n"
        "lives\t4\n"
        "score_ollie\t20\n"
        "score_backflip\t100\n"
        "score_spin\t80\n"
        "score_flap\t50\n"
        "score_grind\t80\n"
        "score_slide\t40\n"
        "score_lean\t10\n"
        "repeat_divisor\t2\n"
        "max_grind_ticks\t28\n"
        "max_slide_ticks\t32\n"
        "max_lean_ticks\t38\n"
        "coin_divisor\t10\n"
        "segments\t1,1,4,2,1,5,3,1,4,2,5,3,1,1,1,4,2,4,2,1,1,5,3,1,1,6\n",
        encoding="utf-8",
    )

    source_manifest = cart_out / "source.manifest"
    source_manifest.write_text(
        "CLUBPENGUIN_CART_SURFER_MANIFEST_V1\n"
        "source_repo=nhaar/Waddle-Forever\n"
        f"source_commit={git_commit(waddle_source)}\n"
        "source_path=media/default/fix/CartSurfer2006.swf\n"
        f"source_sha256={sha256(source_swf)}\n"
        "extractor=JPEXS_FFDec_26.2.1\n"
        "title_frame=main:1\n"
        "tunnel_frame=DefineSprite_201:1\n"
        "cart_frames=DefineSprite_173:1,9,5,10,15,22\n"
        "track_frames=DefineSprite_201:1,2,3,4\n"
        "atlas=320x64 cart=6x40x40 track=4x80x24\n",
        encoding="utf-8",
    )

    generated.extend([title_out, tunnel_out, strip_out, cart_data,
                      source_manifest])


def package_interface(ui_export: Path, waddle_source: Path, out: Path,
                      generated: list[Path]) -> None:
    ui_out = out / "ui"
    source_swf = (waddle_source /
                  "media/default/recreation/interfaces/2010_july.swf")
    source_frame = ui_export / "frames" / "1.png"
    toolbar_out = ui_out / "toolbar.bmp"

    for path in (source_swf, source_frame):
        if not path.exists():
            raise SystemExit(f"missing Club Penguin interface source: {path}")
    if shutil.which("magick") is None:
        raise SystemExit("ImageMagick is required to package the interface")

    ui_out.mkdir(parents=True, exist_ok=True)
    run_magick([
        str(source_frame), "-crop", "563x40+99+440", "+repage",
        "-resize", "320x20!", "-background", "#0b9bd0",
        "-alpha", "remove", "-alpha", "off", f"BMP3:{toolbar_out}",
    ])

    ui_manifest = ui_out / "source.manifest"
    ui_manifest.write_text(
        "CLUBPENGUIN_INTERFACE_MANIFEST_V1\n"
        "source_repo=nhaar/Waddle-Forever\n"
        f"source_commit={git_commit(waddle_source)}\n"
        "source_path=media/default/recreation/interfaces/2010_july.swf\n"
        f"source_sha256={sha256(source_swf)}\n"
        "extractor=JPEXS_FFDec_26.2.1\n"
        "toolbar_frame=main:1 crop=563x40+99+440\n",
        encoding="utf-8",
    )
    generated.extend([toolbar_out, ui_manifest])


def package_puffle(puffle_export: Path, waddle_source: Path, out: Path,
                   generated: list[Path]) -> None:
    puffle_out = out / "puffles"
    puffle_out.mkdir(parents=True, exist_ok=True)
    source_root = ("media/default/svanilla/media/play/v2/content/global/"
                   "puffle/sprites/igloo")
    outputs: list[Path] = []
    manifest_lines = [
        "CLUBPENGUIN_PUFFLE_MANIFEST_V2",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "extractor=JPEXS_FFDec_26.2.1",
        "frames=" + ",".join(str(frame) for frame in PUFFLE_FRAMES),
        "atlas=8x40x40 transparent=magenta",
    ]

    for color, name, sprite_id in PUFFLE_COLORS:
        source_rel = f"{source_root}/puffle_{color}_igloo.swf"
        source_swf = waddle_source / source_rel
        sprite_dir = (puffle_export / color /
                      f"DefineSprite_{sprite_id}")
        sources = [sprite_dir / f"{frame}.png"
                   for frame in PUFFLE_FRAMES]
        output = puffle_out / f"{color}.bmp"

        for path in [source_swf, *sources]:
            if not path.exists():
                raise SystemExit(f"missing {name} Puffle source: {path}")
        run_magick([
            *[str(path) for path in sources], "-trim", "+repage",
            "-resize", "38x30", "-gravity", "center", "-background",
            "magenta", "-alpha", "remove", "-alpha", "off", "-extent",
            "40x40", "+append", f"BMP3:{output}",
        ])
        manifest_lines.extend([
            f"source={color}\t{source_rel}\t{sha256(source_swf)}",
            f"sprite={color}\tDefineSprite_{sprite_id}",
            f"generated={color}.bmp",
            f"sha256={sha256(output)}",
        ])
        outputs.append(output)

    manifest = puffle_out / "source.manifest"
    manifest.write_text("\n".join(manifest_lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])


def package_puffle_food(food_export: Path, out: Path,
                        generated: list[Path]) -> None:
    """Package preserved care icons and their original effect metadata."""
    icon_out = out / "puffles" / "food"
    data_out = out / "data" / "puffle_food.tsv"
    metadata_source = food_export / "puffle_items.json"
    manifest = icon_out / "source.manifest"
    outputs: list[Path] = []

    if not metadata_source.exists():
        raise SystemExit(f"missing puffle item metadata: {metadata_source}")
    metadata = json.loads(metadata_source.read_text(encoding="utf-8"))
    records = {
        Path(str(record.get("asset", ""))).stem: record
        for record in metadata
        if record.get("type") == "food" and record.get("effect")
    }
    icon_out.mkdir(parents=True, exist_ok=True)
    data_out.parent.mkdir(parents=True, exist_ok=True)

    with data_out.open("w", encoding="utf-8") as catalog:
        catalog.write(
            "# id\tname\tasset\tcost\tfood\trest\thappy\tclean\n"
        )
        for asset in PUFFLE_FOOD_ASSETS:
            record = records.get(asset)
            source_swf = food_export / "source" / f"{asset}.swf"
            source_frame = (food_export / "frames" / f"{asset}.swf" /
                            "1.png")
            output = icon_out / f"{asset}.bmp"
            if record is None:
                raise SystemExit(f"missing puffle food metadata: {asset}")
            for path in (source_swf, source_frame):
                if not path.exists():
                    raise SystemExit(f"missing puffle food source: {path}")
            effect = record["effect"]
            run_magick([
                str(source_frame), "-trim", "+repage", "-resize", "34x34",
                "-gravity", "center", "-background", "magenta", "-alpha",
                "remove", "-alpha", "off", "-extent", "40x40",
                f"BMP3:{output}",
            ])
            values = (
                int(record["puffle_item_id"]), record["label"], asset,
                int(record["cost"]), int(effect["food"]),
                int(effect["rest"]), int(effect["play"]),
                int(effect["clean"]),
            )
            catalog.write("\t".join(str(value) for value in values) + "\n")
            outputs.append(output)
    outputs.append(data_out)

    lines = [
        "CLUBPENGUIN_PUFFLE_FOOD_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "metadata_source=media/default/approximation/game_configs/"
        f"puffle_items.json\t{sha256(metadata_source)}",
        "source_root=media/default/svanilla/media/play/v2/content/global/"
        "puffle/care_icons/food",
        "extractor=JPEXS_FFDec_26.2.1",
        f"items={len(PUFFLE_FOOD_ASSETS)}",
        "icon=40x40 transparent=magenta",
    ]
    for asset in PUFFLE_FOOD_ASSETS:
        source_swf = food_export / "source" / f"{asset}.swf"
        lines.append(f"source={asset}.swf\t{sha256(source_swf)}")
    for path in outputs:
        lines.append(f"generated={path.relative_to(out)}\t{sha256(path)}")
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_puffle_motion(motion_export: Path, out: Path,
                          generated: list[Path]) -> None:
    """Package compact, authentic walk, dig, and eat animations."""
    walk_out = out / "puffles" / "walk"
    dig_out = out / "puffles" / "dig"
    eat_out = out / "puffles" / "eat"
    manifest = out / "puffles" / "motion.manifest"
    outputs: list[Path] = []
    source_root = ("media/default/svanilla/media/play/v2/content/global/"
                   "puffle/sprites")

    walk_out.mkdir(parents=True, exist_ok=True)
    dig_out.mkdir(parents=True, exist_ok=True)
    eat_out.mkdir(parents=True, exist_ok=True)
    lines = [
        "CLUBPENGUIN_PUFFLE_MOTION_MANIFEST_V3",
        "source_mirror=icerink.solero.me/media1.clubpenguin.com",
        "extractor=JPEXS_FFDec_26.2.1",
        "walk=official root frames 9,11,13,15 x subframes 1,7",
        "walk_directions=down,left,up,right",
        "walk_registration=viewBox 3 -39 54 54 rendered to 40x40",
        "dig=8 preserved frames sampled across original full timeline",
        "eat=8 preserved frames sampled across original 60-frame timeline",
        "atlas=8x40x40 transparent=magenta",
    ]

    for color, _, _ in PUFFLE_COLORS:
        walk_swf = (motion_export / "source" / "walk" /
                    f"puffle_{color}_walk.swf")
        dig_swf = (motion_export / "source" / "dig" /
                   f"puffle_{color}_dig.swf")
        walk_root = (motion_export / "root_svg" / "walk" /
                     f"puffle_{color}_walk.swf")
        dig_root = (motion_export / "sprites" / f"puffle_{color}_dig" /
                    f"DefineSprite_{PUFFLE_DIG_SPRITES[color]}")
        for path in (walk_swf, dig_swf, dig_root):
            if not path.exists():
                raise SystemExit(f"missing puffle motion source: {path}")

        walk_frames: list[Path] = []
        temporary: list[Path] = []
        for direction_index, root_frame in enumerate(
                PUFFLE_WALK_ROOT_FRAMES):
            for frame_number in PUFFLE_WALK_SUBFRAMES:
                source = walk_root / str(root_frame) / f"{frame_number}.svg"
                prepared = (walk_out /
                            f".{color}_{direction_index}_{frame_number}.bmp")
                if not source.exists():
                    raise SystemExit(f"missing puffle walk frame: {source}")
                render_registered_puffle_frame(source, prepared)
                walk_frames.append(prepared)
                temporary.append(prepared)
        walk_output = walk_out / f"{color}.bmp"
        run_magick([
            *[str(path) for path in walk_frames], "+append",
            f"BMP3:{walk_output}",
        ])

        dig_sources = sorted(
            dig_root.glob("*.png"),
            key=lambda path: int(path.stem),
        )
        if len(dig_sources) < 8:
            raise SystemExit(f"incomplete puffle dig timeline: {dig_root}")
        dig_indices = [
            round(index * (len(dig_sources) - 1) / 7) for index in range(8)
        ]
        dig_frames: list[Path] = []
        for index, source_index in enumerate(dig_indices):
            prepared = dig_out / f".{color}_{index}.bmp"
            run_magick([
                str(dig_sources[source_index]), "-trim", "+repage",
                "-resize", "38x30", "-gravity", "center", "-background",
                "magenta", "-alpha", "remove", "-alpha", "off", "-extent",
                "40x40", f"BMP3:{prepared}",
            ])
            dig_frames.append(prepared)
            temporary.append(prepared)
        dig_output = dig_out / f"{color}.bmp"
        run_magick([
            *[str(path) for path in dig_frames], "+append",
            f"BMP3:{dig_output}",
        ])
        for path in temporary:
            path.unlink()

        lines.extend([
            f"source=walk/{walk_swf.name}\t{sha256(walk_swf)}",
            f"source=dig/{dig_swf.name}\t{sha256(dig_swf)}",
            f"walk_root_frames={color}\t"
            f"{','.join(str(value) for value in PUFFLE_WALK_ROOT_FRAMES)}",
            f"dig_sprite={color}\t{PUFFLE_DIG_SPRITES[color]}",
            f"generated=walk/{color}.bmp\t{sha256(walk_output)}",
            f"generated=dig/{color}.bmp\t{sha256(dig_output)}",
        ])
        outputs.extend([walk_output, dig_output])

        if color in PUFFLE_EAT_SPRITES:
            eat_swf = (motion_export / "source" / "eat" /
                       f"puffle_{color}_eat.swf")
            eat_root = (motion_export / "sprites" /
                        f"puffle_{color}_eat" /
                        f"DefineSprite_{PUFFLE_EAT_SPRITES[color]}")
            if not eat_swf.exists() or not eat_root.exists():
                raise SystemExit(f"missing puffle eat source: {eat_root}")
            eat_sources = sorted(
                eat_root.glob("*.png"),
                key=lambda path: int(path.stem),
            )
            if len(eat_sources) < 8:
                raise SystemExit(f"incomplete puffle eat timeline: {eat_root}")
            eat_indices = [
                round(index * (len(eat_sources) - 1) / 7)
                for index in range(8)
            ]
            eat_frames: list[Path] = []
            for index, source_index in enumerate(eat_indices):
                prepared = eat_out / f".{color}_{index}.bmp"
                run_magick([
                    str(eat_sources[source_index]), "-trim", "+repage",
                    "-resize", "38x30", "-gravity", "center",
                    "-background", "magenta", "-alpha", "remove",
                    "-alpha", "off", "-extent", "40x40",
                    f"BMP3:{prepared}",
                ])
                eat_frames.append(prepared)
            eat_output = eat_out / f"{color}.bmp"
            run_magick([
                *[str(path) for path in eat_frames], "+append",
                f"BMP3:{eat_output}",
            ])
            for path in eat_frames:
                path.unlink()
            lines.extend([
                f"source=eat/{eat_swf.name}\t{sha256(eat_swf)}",
                f"eat_sprite={color}\t{PUFFLE_EAT_SPRITES[color]}",
                f"generated=eat/{color}.bmp\t{sha256(eat_output)}",
            ])
            outputs.append(eat_output)
        else:
            lines.append("eat_source=gold\tnot present in pinned archive")

    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_puffle_care_ui(care_export: Path, out: Path,
                           generated: list[Path]) -> None:
    """Package the preserved puffle-care background and menu icons."""
    care_out = out / "puffles" / "care"
    source_swf = care_export / "source" / "puffle_care.swf"
    sprites = care_export / "sprites"
    background = next(sprites.glob("DefineSprite_18_*"), None)
    if not source_swf.exists() or background is None:
        raise SystemExit("missing preserved puffle care UI source")

    care_out.mkdir(parents=True, exist_ok=True)
    background_out = care_out / "background.bmp"
    run_magick([
        str(background / "1.png"), "-resize", "320x220!",
        "-background", "#e3e4ec", "-alpha", "remove", "-alpha", "off",
        f"BMP3:{background_out}",
    ])

    prepared: list[Path] = []
    for index, (name, sprite_id) in enumerate(PUFFLE_CARE_ICONS):
        sprite = next(sprites.glob(f"DefineSprite_{sprite_id}_*"), None)
        if sprite is None or not (sprite / "1.png").exists():
            raise SystemExit(f"missing puffle care icon: {name}")
        icon = care_out / f".{index}_{name}.bmp"
        run_magick([
            str(sprite / "1.png"), "-trim", "+repage", "-resize",
            "16x14", "-gravity", "center", "-background", "magenta",
            "-alpha", "remove", "-alpha", "off", "-extent", "16x16",
            f"BMP3:{icon}",
        ])
        prepared.append(icon)

    icons_out = care_out / "icons.bmp"
    run_magick([
        *[str(path) for path in prepared], "+append", f"BMP3:{icons_out}",
    ])
    for path in prepared:
        path.unlink()

    manifest = care_out / "source.manifest"
    lines = [
        "CLUBPENGUIN_PUFFLE_CARE_UI_MANIFEST_V1",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "source_path=media/default/svanilla/media/play/v2/client/"
        f"puffle_care.swf\t{sha256(source_swf)}",
        "extractor=JPEXS_FFDec_26.2.1",
        "background_sprite=18",
        "icons=" + ",".join(
            f"{name}:{sprite_id}" for name, sprite_id in PUFFLE_CARE_ICONS
        ),
        f"generated=background.bmp\t{sha256(background_out)}",
        f"generated=icons.bmp\t{sha256(icons_out)}",
        "background=320x220 BMP3",
        "icons=6x16x16 BMP3 transparent=magenta",
    ]
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([background_out, icons_out, manifest])
    refresh_existing_manifest(out)


def package_puffle_tricks(trick_export: Path, out: Path,
                          generated: list[Path]) -> None:
    """Package every preserved color-specific puffle trick timeline."""
    trick_out = out / "puffles" / "tricks"
    manifest = trick_out / "source.manifest"
    outputs: list[Path] = []
    lines = [
        "CLUBPENGUIN_PUFFLE_TRICKS_MANIFEST_V1",
        "source_mirror=icerink.solero.me/media1.clubpenguin.com",
        "source_path=play/v2/content/global/puffle/sprites/tricks",
        "extractor=JPEXS_FFDec_26.2.1",
        "tricks=" + ",".join(PUFFLE_TRICKS),
        "colors=" + ",".join(color for color, _, _ in PUFFLE_COLORS),
        "timeline=8 preserved frames sampled across each original SWF",
        "atlas=8x50x50 BMP3 transparent=magenta",
    ]

    trick_out.mkdir(parents=True, exist_ok=True)
    for color, _, _ in PUFFLE_COLORS:
        for trick in PUFFLE_TRICKS:
            source_swf = trick_export / "source" / color / f"{trick}.swf"
            sprite_parent = trick_export / "sprites" / color / trick
            sprite_roots = sorted(
                sprite_parent.glob("DefineSprite_*"),
                key=lambda path: int(path.name.split("_")[1]),
            )
            if not source_swf.exists() or not sprite_roots:
                raise SystemExit(
                    f"missing puffle trick source: {color}/{trick}"
                )

            # These standalone SWFs declare their playable, locally bounded
            # timeline last. Earlier sprites are nested effects or the large
            # Flash-stage wrapper.
            sprite_root = sprite_roots[-1]
            source_frames = sorted(
                sprite_root.glob("*.png"),
                key=lambda path: int(path.stem),
            )
            if len(source_frames) < 8:
                raise SystemExit(
                    f"incomplete puffle trick timeline: {sprite_root}"
                )
            sample_indices = [
                round(index * (len(source_frames) - 1) / 7)
                for index in range(8)
            ]
            prepared: list[Path] = []
            for index, source_index in enumerate(sample_indices):
                frame = trick_out / f".{color}_{trick}_{index}.bmp"
                run_magick([
                    str(source_frames[source_index]), "-resize", "50x50>",
                    "-gravity", "center", "-background", "magenta",
                    "-alpha", "remove", "-alpha", "off", "-extent",
                    "50x50", f"BMP3:{frame}",
                ])
                prepared.append(frame)

            output = trick_out / f"{color}_{trick}.bmp"
            run_magick([
                *[str(path) for path in prepared], "+append",
                f"BMP3:{output}",
            ])
            for path in prepared:
                path.unlink()
            lines.extend([
                f"source={color}/{trick}.swf\t{sha256(source_swf)}",
                f"sprite={color}/{trick}\t"
                f"{sprite_root.name.split('_')[1]}:{len(source_frames)}",
                f"generated={output.name}\t{sha256(output)}",
            ])
            outputs.append(output)

    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_puffle_toys(play_export: Path, out: Path,
                        generated: list[Path]) -> None:
    """Package the official normal and super toy for every puffle."""
    metadata_source = play_export / "puffle_items.json"
    metadata = json.loads(metadata_source.read_text(encoding="utf-8"))
    records = [
        record for record in metadata
        if record.get("type") == "play" and record.get("effect")
        and record.get("play_external") in ("play", "superplay")
    ]
    toy_out = out / "puffles" / "toys"
    icon_out = toy_out / "icons"
    data_out = out / "data" / "puffle_toys.tsv"
    manifest = toy_out / "source.manifest"
    outputs: list[Path] = []
    lines = [
        "CLUBPENGUIN_PUFFLE_TOYS_MANIFEST_V1",
        "source_mirror=icerink.solero.me/media1.clubpenguin.com",
        "source_path=play/v2/content/global/puffle/care_icons/play",
        "animation_source=official igloo root frames 27,28",
        "metadata_source=media/default/approximation/game_configs/"
        f"puffle_items.json\t{sha256(metadata_source)}",
        "extractor=JPEXS_FFDec_26.2.1",
        "colors=" + ",".join(color for color, _, _ in PUFFLE_COLORS),
        "toys_per_color=normal,super",
        "reaction=2 (likes) from the preserved care item table",
        "timeline=8 preserved frames sampled across each original sprite",
        "animation_atlas=8x50x50 BMP3 transparent=magenta",
        "icon=40x40 BMP3 transparent=magenta",
    ]

    toy_out.mkdir(parents=True, exist_ok=True)
    icon_out.mkdir(parents=True, exist_ok=True)
    data_out.parent.mkdir(parents=True, exist_ok=True)
    with data_out.open("w", encoding="utf-8") as catalog:
        catalog.write(
            "# color\tkind\tid\tname\tasset\tcost\tfood\trest\t"
            "happy\tclean\treaction\n"
        )
        for color_index, (color, _, _) in enumerate(PUFFLE_COLORS):
            reaction_index = PUFFLE_REACTION_INDEX[color]
            compatible = {
                "normal" if record["play_external"] == "play" else
                "super": record
                for record in records
                if record["reaction"][reaction_index] == "2"
            }
            if set(compatible) != {"normal", "super"}:
                raise SystemExit(
                    f"incomplete compatible puffle toys for {color}"
                )

            source_swf = (play_export / "source" / "igloo" /
                          f"puffle_{color}_igloo.swf")
            if not source_swf.exists():
                raise SystemExit(f"missing puffle toy source: {source_swf}")
            for kind_index, kind in enumerate(("normal", "super")):
                record = compatible[kind]
                asset = Path(record["asset"]).stem
                icon_swf = play_export / "source" / "icons" / f"{asset}.swf"
                icon_frame = (play_export / "icon_frames" /
                              f"{asset}.swf" / "1.png")
                sprite_id = PUFFLE_PLAY_SPRITES[color][kind_index]
                sprite_root = (play_export / "sprites" / color /
                               f"DefineSprite_{sprite_id}")
                source_frames = sorted(
                    sprite_root.glob("*.png"),
                    key=lambda path: int(path.stem),
                )
                if (not icon_swf.exists() or not icon_frame.exists() or
                        len(source_frames) < 8):
                    raise SystemExit(
                        f"incomplete puffle toy export: {color}/{kind}"
                    )

                sample_indices = [
                    round(index * (len(source_frames) - 1) / 7)
                    for index in range(8)
                ]
                prepared: list[Path] = []
                for frame_index, source_index in enumerate(sample_indices):
                    frame = toy_out / f".{color}_{kind}_{frame_index}.bmp"
                    run_magick([
                        str(source_frames[source_index]), "-trim", "+repage",
                        "-resize", "48x48", "-gravity", "center",
                        "-background", "magenta", "-alpha", "remove",
                        "-alpha", "off", "-extent", "50x50",
                        f"BMP3:{frame}",
                    ])
                    prepared.append(frame)
                animation = toy_out / f"{color}_{kind}.bmp"
                run_magick([
                    *[str(path) for path in prepared], "+append",
                    f"BMP3:{animation}",
                ])
                for path in prepared:
                    path.unlink()

                icon = icon_out / f"{asset}.bmp"
                run_magick([
                    str(icon_frame), "-trim", "+repage", "-resize",
                    "34x34", "-gravity", "center", "-background",
                    "magenta", "-alpha", "remove", "-alpha", "off",
                    "-extent", "40x40", f"BMP3:{icon}",
                ])
                effect = record["effect"]
                values = (
                    color_index, kind, int(record["puffle_item_id"]),
                    record["label"], asset, int(record["cost"]),
                    int(effect["food"]), int(effect["rest"]),
                    int(effect["play"]), int(effect["clean"]), 2,
                )
                catalog.write(
                    "\t".join(str(value) for value in values) + "\n"
                )
                lines.extend([
                    f"source=igloo/{source_swf.name}\t{sha256(source_swf)}",
                    f"source=icons/{icon_swf.name}\t{sha256(icon_swf)}",
                    f"sprite={color}/{kind}\t{sprite_id}:"
                    f"{len(source_frames)}",
                    f"generated={animation.relative_to(toy_out)}\t"
                    f"{sha256(animation)}",
                    f"generated={icon.relative_to(toy_out)}\t{sha256(icon)}",
                ])
                outputs.extend([animation, icon])
    outputs.append(data_out)
    lines.append(f"generated={data_out.relative_to(out)}\t{sha256(data_out)}")
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_puffle_hats(hat_export: Path, out: Path,
                        generated: list[Path]) -> None:
    """Package official puffle hat metadata and care/room layers."""
    metadata_source = hat_export / "puffle_items.json"
    metadata = json.loads(metadata_source.read_text(encoding="utf-8"))
    records = [record for record in metadata if record["type"] == "head"]
    hat_out = out / "puffles" / "hats"
    room_out = hat_out / "room"
    data_out = out / "data" / "puffle_hats.tsv"
    manifest = hat_out / "source.manifest"
    outputs: list[Path] = []
    lines = [
        "CLUBPENGUIN_PUFFLE_HATS_MANIFEST_V2",
        "source_mirror=icerink.solero.me/media1.clubpenguin.com",
        "source_icon_path=play/v2/content/global/puffle/"
        "care_catalog_icons/head",
        "source_wearable_path=play/v2/content/global/puffle/"
        "care_hats/head",
        "source_room_path=play/v2/content/global/puffle/hats/room",
        f"metadata=puffle_items.json\t{sha256(metadata_source)}",
        "extractor=JPEXS_FFDec_26.2.1",
        f"items={len(records)}",
        "wearable_items=63",
        "atlas=icon+care-front overlay, 2x40x40 BMP3, "
        "transparent=magenta",
        "room_layers=63 hats x 4 directions x front/back",
        "room_animation=subframes 1,7 in official walk timelines",
        "room_registration=viewBox 3 -39 54 54 rendered to 40x40",
        "missing_wearable_swfs=" +
        ",".join(sorted(PUFFLE_HAT_MISSING_WEARABLES)),
    ]
    data_lines = ["# id\tname\tasset\tcost\tavailable"]

    if len(records) != 68:
        raise SystemExit(f"expected 68 puffle hats, found {len(records)}")
    hat_out.mkdir(parents=True, exist_ok=True)
    room_out.mkdir(parents=True, exist_ok=True)
    data_out.parent.mkdir(parents=True, exist_ok=True)

    for record in records:
        item_id = int(record["puffle_item_id"])
        name = record["label"]
        cost = int(record["cost"])
        asset = Path(record["asset"]).stem
        icon_swf = hat_export / "source" / "catalog" / f"{asset}.swf"
        icon_frame = (hat_export / "svgframes" / "catalog" /
                      f"{asset}.swf" / "1.svg")
        care_swf = hat_export / "source" / "care" / f"{asset}.swf"
        care_fronts = sorted(
            (hat_export / "sprites" / "care" / asset).glob(
                "DefineSprite_*HatFront*/1.png"
            )
        )
        available = (care_swf.exists() and len(care_fronts) == 1 and
                     asset not in PUFFLE_HAT_MISSING_WEARABLES)
        if available:
            hat_front = care_fronts[0]
        else:
            hat_front = None
        icon_source_tmp = None
        if icon_frame.exists() and icon_swf.exists():
            # Catalog icons are library SWFs registered around (0, 0) for an
            # external loader. A normal stage render clips their negative
            # coordinates. Preserve the complete official vector frame by
            # rendering a centered viewport around that registration point.
            icon_svg = icon_frame.read_text(encoding="utf-8")
            root_start = icon_svg.find("<svg")
            root_end = icon_svg.find(">", root_start)
            if root_start < 0 or root_end < 0:
                raise SystemExit(f"invalid puffle hat SVG: {icon_frame}")
            root = re.sub(
                r'\s(?:height|width|viewBox)="[^"]*"', "",
                icon_svg[root_start:root_end],
            )
            icon_svg = (
                icon_svg[:root_start] + root +
                ' width="400px" height="400px" '
                'viewBox="-200 -200 400 400">' +
                icon_svg[root_end + 1:]
            )
            icon_source_tmp = hat_out / f".{asset}_source.svg"
            icon_source_tmp.write_text(icon_svg, encoding="utf-8")
            icon_source = icon_source_tmp
            lines.append(f"source_icon={asset}.swf\t{sha256(icon_swf)}")
        elif asset == "polkapufflehat" and hat_front is not None:
            # The archive has the original wearable but no separate catalog
            # icon SWF. Reuse that preserved front symbol for its thumbnail.
            icon_source = hat_front
            lines.append("source_icon=polkapufflehat\twearable-front-symbol")
        else:
            raise SystemExit(f"missing puffle hat icon source: {asset}")

        icon = hat_out / f".{asset}_icon.bmp"
        overlay = hat_out / f".{asset}_overlay.bmp"
        run_magick([
            str(icon_source), "-trim", "+repage", "-resize", "34x30>",
            "-gravity", "center", "-background", "magenta", "-alpha",
            "remove", "-alpha", "off", "-extent", "40x40",
            f"BMP3:{icon}",
        ])
        if hat_front is not None:
            run_magick([
                str(hat_front), "-trim", "+repage", "-resize", "32x24>",
                "-gravity", "north", "-background", "magenta", "-alpha",
                "remove", "-alpha", "off", "-extent", "40x40",
                f"BMP3:{overlay}",
            ])
            lines.extend([
                f"source_wearable={asset}.swf\t{sha256(care_swf)}",
                f"wearable_sprite={asset}\t"
                f"{hat_front.parent.name.split('_')[1]}",
            ])
        else:
            run_magick([
                "-size", "40x40", "xc:magenta", f"BMP3:{overlay}",
            ])
            lines.append(f"source_wearable={asset}\tmissing-in-archive")

        output = hat_out / f"{asset}.bmp"
        run_magick([str(icon), str(overlay), "+append", f"BMP3:{output}"])
        icon.unlink()
        overlay.unlink()
        if icon_source_tmp is not None:
            icon_source_tmp.unlink()
        lines.append(
            f"generated={output.relative_to(hat_out)}\t{sha256(output)}"
        )
        data_lines.append(
            f"{item_id}\t{name}\t{asset}\t{cost}\t{int(available)}"
        )
        outputs.append(output)

        if available:
            asset_room_out = room_out / asset
            asset_room_out.mkdir(parents=True, exist_ok=True)
            for layer in ("back", "front"):
                room_swf = (hat_export / "source" / "room" /
                            f"{asset}_hat_{layer}.swf")
                room_root = (hat_export / "root_svg" / "room" /
                             f"{asset}_hat_{layer}.swf")
                if not room_swf.exists() or not room_root.exists():
                    raise SystemExit(
                        f"missing puffle room hat source: {asset} {layer}"
                    )
                lines.append(
                    f"source_room_{layer}={room_swf.name}\t"
                    f"{sha256(room_swf)}"
                )
                for direction, root_frame in enumerate(
                        PUFFLE_WALK_ROOT_FRAMES):
                    prepared: list[Path] = []
                    for subframe in PUFFLE_WALK_SUBFRAMES:
                        source = (room_root / str(root_frame) /
                                  f"{subframe}.svg")
                        frame = (asset_room_out /
                                 f".{direction}_{layer}_{subframe}.bmp")
                        if not source.exists():
                            raise SystemExit(
                                f"missing puffle room hat frame: {source}"
                            )
                        render_registered_puffle_frame(source, frame)
                        prepared.append(frame)
                    room_layer = asset_room_out / f"{direction}_{layer}.bmp"
                    run_magick([
                        *[str(path) for path in prepared], "+append",
                        f"BMP3:{room_layer}",
                    ])
                    for path in prepared:
                        path.unlink()
                    lines.append(
                        f"generated={room_layer.relative_to(hat_out)}\t"
                        f"{sha256(room_layer)}"
                    )
                    outputs.append(room_layer)

    data_out.write_text("\n".join(data_lines) + "\n", encoding="utf-8")
    lines.append(f"catalog=data/puffle_hats.tsv\t{sha256(data_out)}")
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, data_out, manifest])
    refresh_existing_manifest(out)


def package_furniture(furniture_export: Path, waddle_source: Path, out: Path,
                      generated: list[Path]) -> None:
    source_root = ("media/default/slegacy/media/play/v2/content/global/"
                   "furniture/sprites")
    igloo_out = out / "igloo"
    item_out = igloo_out / "items"
    metadata_source = furniture_export / "furniture_items.json"
    catalog_out = out / "data" / "furniture.tsv"
    metadata = json.loads(metadata_source.read_text(encoding="utf-8"))
    records: list[tuple[int, str, int, int, int, int]] = []
    outputs: list[Path] = []

    item_out.mkdir(parents=True, exist_ok=True)
    catalog_out.parent.mkdir(parents=True, exist_ok=True)
    for record in metadata:
        item_id = int(record["furniture_item_id"])
        export_dir = furniture_export / f"{item_id}.swf"
        if item_id in FURNITURE_SPRITE_FRAMES:
            source = export_dir / FURNITURE_SPRITE_FRAMES[item_id]
        else:
            sprites = sorted(
                (path for path in export_dir.glob("DefineSprite_*")
                 if (path / "1.png").exists()),
                key=lambda path: int(
                    re.match(r"DefineSprite_(\d+)", path.name).group(1)
                ),
            )
            source = (sprites[-1] / "1.png") if sprites else (
                export_dir / "missing.png"
            )
        swf = waddle_source / source_root / f"{item_id}.swf"
        output = item_out / f"{item_id}.bmp"
        if not swf.exists():
            continue
        if not source.exists():
            raise SystemExit(f"missing furniture source: {source}")
        run_magick([
            str(source), "-trim", "+repage", "-resize", "56x72",
            "-gravity", "south",
            "-background", "magenta", "-alpha", "remove", "-alpha", "off",
            "-extent", "64x80", f"BMP3:{output}",
        ])
        label = str(record.get("label") or record.get("prompt") or
                    f"Furniture {item_id}").replace("\t", " ")
        records.append((item_id, label, int(record.get("cost", 0)),
                        int(record.get("type", 0)),
                        int(record.get("is_member_only", 0)),
                        int(record.get("max_quantity", 99))))
        outputs.append(output)

    if not records:
        raise SystemExit("no matching furniture SWF/frame pairs were found")

    records.sort(key=lambda row: row[0])
    with catalog_out.open("w", encoding="utf-8") as catalog:
        catalog.write("# id\tname\tcost\ttype\tmember\tmax_quantity\n")
        for record in records:
            catalog.write("\t".join(str(value) for value in record) + "\n")

    manifest = igloo_out / "source.manifest"
    lines = [
        "CLUBPENGUIN_IGLOO_FURNITURE_MANIFEST_V2",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        f"metadata_source=furniture_items.json\t{sha256(metadata_source)}",
        f"items={len(records)}",
    ]
    for item_id, *_ in records:
        swf = waddle_source / source_root / f"{item_id}.swf"
        output = item_out / f"{item_id}.bmp"
        lines.extend([
            f"source_path={source_root}/{item_id}.swf\t{sha256(swf)}",
            f"render={item_id}\t{source.relative_to(furniture_export)}",
            f"generated=items/{item_id}.bmp\t{sha256(output)}",
        ])
    lines.extend([
        "extractor=JPEXS_FFDec_26.2.1",
        f"catalog=data/furniture.tsv\t{sha256(catalog_out)}",
        "format=individual 64x80 BMP3 transparent=magenta",
    ])
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, catalog_out, manifest])
    refresh_existing_manifest(out)


def package_igloo_layers(igloo_export: Path, waddle_source: Path, out: Path,
                         generated: list[Path]) -> None:
    categories = {
        "buildings": {
            "source": ("media/default/svanilla/media/play/v2/content/"
                       "global/igloo/buildings/sprites"),
            "metadata": "igloos.json",
            "tsv": "igloo_buildings.tsv",
            "id": "igloo_id",
            "name": "name",
        },
        "flooring": {
            "source": ("media/default/svanilla/media/play/v2/content/"
                       "global/igloo/flooring/sprites"),
            "metadata": "igloo_floors.json",
            "tsv": "igloo_flooring.tsv",
            "id": "igloo_floor_id",
            "name": "label",
        },
        "locations": {
            "source": ("media/default/svanilla/media/play/v2/content/"
                       "global/igloo/locations/sprites"),
            "metadata": "igloo_locations.json",
            "tsv": "igloo_locations.tsv",
            "id": "igloo_location_id",
            "name": "label",
        },
    }
    igloo_out = out / "igloo"
    data_out = out / "data"
    manifest = igloo_out / "layers.manifest"
    manifest_lines = [
        "CLUBPENGUIN_IGLOO_LAYERS_MANIFEST_V2",
        "source_repo=nhaar/Waddle-Forever",
        f"source_commit={WADDLE_SOURCE_COMMIT}",
        "extractor=JPEXS_FFDec_26.2.1",
        "format=320x220 BMP3 transparent=magenta; 320x220 1-bit CPMASK1",
    ]
    outputs: list[Path] = []

    igloo_out.mkdir(parents=True, exist_ok=True)
    data_out.mkdir(parents=True, exist_ok=True)
    for category, config in categories.items():
        metadata_path = igloo_export / "config" / config["metadata"]
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        if isinstance(metadata, dict):
            records = list(metadata.values())
        else:
            records = metadata
        records.sort(key=lambda record: int(record[config["id"]]))
        category_out = igloo_out / category
        tsv_out = data_out / config["tsv"]
        category_out.mkdir(parents=True, exist_ok=True)
        for old_output in category_out.glob("*.bmp"):
            old_output.unlink()
        mask_out = igloo_out / "masks"
        if category == "buildings":
            mask_out.mkdir(parents=True, exist_ok=True)
            for old_output in mask_out.glob("*.msk"):
                old_output.unlink()
        manifest_lines.append(
            f"metadata={config['metadata']}\t{sha256(metadata_path)}"
        )
        with tsv_out.open("w", encoding="utf-8") as tsv:
            if category == "buildings":
                tsv.write("# id\tname\tcost\tfloor_frame\n")
            elif category == "flooring":
                tsv.write("# id\tname\tcost\tframes\n")
            else:
                tsv.write("# id\tname\tcost\n")
            for record in records:
                item_id = int(record[config["id"]])
                name = str(record.get(config["name"], item_id)).replace(
                    "\t", " "
                )
                cost = int(record.get("cost", 0))
                swf = waddle_source / config["source"] / f"{item_id}.swf"
                # Flooring ID 0 is the official remove-floor command and has
                # no sprite in this archive. Building ID 0 is the official
                # remove-building command; ID 81 is the intentionally
                # invisible Merry Walrus Iceberg. These remain real catalog
                # entries but deliberately have no visible art.
                no_art_command = (
                    (category == "flooring" and item_id == 0) or
                    (category == "buildings" and item_id in (0, 81))
                )
                if category == "buildings":
                    floor_frame_path = (igloo_export / category /
                                        f"{item_id}.swf" / "floor_frame.txt")
                    floor_frame = 0
                    if not no_art_command:
                        if not floor_frame_path.exists():
                            raise SystemExit(
                                "missing igloo floor frame mapping: "
                                f"{floor_frame_path}"
                            )
                        floor_frame = int(
                            floor_frame_path.read_text(encoding="utf-8").strip()
                        )
                    tsv.write(
                        f"{item_id}\t{name}\t{cost}\t{floor_frame}\n"
                    )
                elif category == "flooring":
                    frame_sources = sorted(
                        (igloo_export / category / f"{item_id}.swf").glob(
                            "[0-9]*.png"
                        ),
                        key=lambda path: int(path.stem),
                    )
                    frame_count = len(frame_sources)
                    tsv.write(
                        f"{item_id}\t{name}\t{cost}\t{frame_count}\n"
                    )
                else:
                    tsv.write(f"{item_id}\t{name}\t{cost}\n")
                if no_art_command:
                    continue
                if not swf.exists():
                    raise SystemExit(f"missing igloo layer source: {swf}")
                manifest_lines.append(
                    f"source={category}/{item_id}\t"
                    f"{config['source']}/{item_id}.swf\t{sha256(swf)}"
                )
                if category == "buildings":
                    source = (igloo_export / category / f"{item_id}.swf" /
                              "visible.png")
                    mask_source = (igloo_export / category /
                                   f"{item_id}.swf" / "mask.png")
                    output = category_out / f"{item_id}.bmp"
                    mask_output = mask_out / f"{item_id}.msk"
                    for path in (source, mask_source):
                        if not path.exists():
                            raise SystemExit(
                                f"missing igloo building render: {path}"
                            )
                    run_magick([
                        str(source), "-resize", "320x220!", "-background",
                        "magenta", "-alpha", "remove", "-alpha", "off",
                        f"BMP3:{output}",
                    ])
                    write_alpha_mask(mask_source, mask_output)
                    manifest_lines.extend([
                        f"render=buildings/{item_id}\tvisible.png",
                        f"mask=buildings/{item_id}\tmask.png",
                        f"generated=buildings/{item_id}.bmp\t{sha256(output)}",
                        f"generated=masks/{item_id}.msk\t"
                        f"{sha256(mask_output)}",
                    ])
                    outputs.extend([output, mask_output])
                elif category == "flooring":
                    if not frame_sources:
                        raise SystemExit(
                            f"missing igloo flooring frames for {item_id}"
                        )
                    for frame, source in enumerate(frame_sources, 1):
                        output = category_out / f"{item_id}_{frame}.bmp"
                        run_magick([
                            str(source), "-resize", "320x220!", "-background",
                            "magenta", "-alpha", "remove", "-alpha", "off",
                            f"BMP3:{output}",
                        ])
                        manifest_lines.append(
                            f"generated=flooring/{item_id}_{frame}.bmp\t"
                            f"{sha256(output)}"
                        )
                        outputs.append(output)
                else:
                    source = (igloo_export / category / f"{item_id}.swf" /
                              "1.png")
                    output = category_out / f"{item_id}.bmp"
                    if not source.exists():
                        raise SystemExit(
                            f"missing igloo location render: {source}"
                        )
                    run_magick([
                        str(source), "-resize", "320x220!", "-background",
                        "magenta", "-alpha", "remove", "-alpha", "off",
                        f"BMP3:{output}",
                    ])
                    manifest_lines.append(
                        f"generated=locations/{item_id}.bmp\t{sha256(output)}"
                    )
                    outputs.append(output)
        manifest_lines.append(
            f"catalog=data/{config['tsv']}\t{sha256(tsv_out)}"
        )
        outputs.append(tsv_out)

    manifest.write_text("\n".join(manifest_lines) + "\n", encoding="utf-8")
    generated.extend([*outputs, manifest])
    refresh_existing_manifest(out)


def package_freeroam(source: Path, out: Path,
                     room_frames: Path | None = None,
                     waddle_source: Path | None = None,
                     backyard_export: Path | None = None,
                     archive_room_export: Path | None = None,
                     sound_studio_export: Path | None = None,
                     cart_export: Path | None = None,
                     ui_export: Path | None = None,
                     puffle_export: Path | None = None,
                     puffle_food_export: Path | None = None,
                     puffle_motion_export: Path | None = None,
                     puffle_care_export: Path | None = None,
                     puffle_trick_export: Path | None = None,
                     puffle_play_export: Path | None = None,
                     puffle_hat_export: Path | None = None,
                     furniture_export: Path | None = None,
                     igloo_export: Path | None = None,
                     avatar_export: Path | None = None,
                     sport_export: Path | None = None,
                     sport_avatar_export: Path | None = None,
                     costume_export: Path | None = None,
                     costume_avatar_export: Path | None = None,
                     penguin_style_export: Path | None = None,
                     penguin_style_avatar_export: Path | None = None,
                     igloo_catalog_export: Path | None = None,
                     ninja_export: Path | None = None,
                     ninja_avatar_export: Path | None = None,
                     puffle_adopt_export: Path | None = None,
                     pet_furniture_export: Path | None = None) -> None:
    world_src = source / "images" / "sprite-sheet0.png"
    player_src = source / "images" / "sprite2-sheet0.png"
    player_src2 = source / "images" / "sprite2-sheet1.png"
    cover_dir = out / "covers"
    generated: list[Path] = []

    if not world_src.exists():
        raise SystemExit(f"missing real source asset: {world_src}")
    if not player_src.exists():
        raise SystemExit(f"missing real source asset: {player_src}")
    if not player_src2.exists():
        raise SystemExit(f"missing real source asset: {player_src2}")
    if shutil.which("ffmpeg") is None:
        raise SystemExit("ffmpeg is required to convert the preserved PNGs")

    out.mkdir(parents=True, exist_ok=True)
    cover_dir.mkdir(parents=True, exist_ok=True)

    world_out = out / "world.bmp"
    player_out = out / "player.bmp"
    cover_out = cover_dir / "ClubPenguin.bmp"
    cover_space_out = cover_dir / "Club Penguin.bmp"
    cover_lower_out = cover_dir / "clubpenguin.bmp"
    cover_pane_out = cover_dir / "Club Penguin.pane.bmp"
    cover_lower_pane_out = cover_dir / "clubpenguin.pane.bmp"

    run_ffmpeg([
        "-i", str(world_src),
        "-vf", "scale=320:220",
        "-pix_fmt", "bgr24",
        str(world_out),
    ])
    generated.append(world_out)

    frames = [
        ("sprite2-sheet0.png", 1, 1, 36, 35),
        ("sprite2-sheet0.png", 39, 1, 35, 35),
        ("sprite2-sheet0.png", 76, 36, 34, 35),
        ("sprite2-sheet0.png", 1, 77, 22, 41),
        ("sprite2-sheet0.png", 88, 73, 24, 41),
        ("sprite2-sheet1.png", 1, 1, 22, 41),
        ("sprite2-sheet0.png", 76, 1, 37, 33),
        ("sprite2-sheet0.png", 31, 38, 28, 36),
        ("sprite2-sheet0.png", 1, 38, 28, 37),
        ("sprite2-sheet0.png", 31, 76, 23, 42),
        ("sprite2-sheet0.png", 61, 73, 25, 40),
        ("sprite2-sheet1.png", 25, 1, 22, 41),
    ]
    frame_paths = []
    for i, frame in enumerate(frames):
        frame_path = out / f".player_frame_{i}.bmp"
        make_player_frame(source, frame, frame_path)
        frame_paths.append(frame_path)

    selector_path = out / ".player_selector.bmp"
    run_magick([
        str(frame_paths[0]), "-transparent", "magenta", "-trim", "+repage",
        "-resize", "20x22", "-channel", "A", "-threshold", "50%",
        "+channel", "-gravity", "south", "-background", "magenta",
        "-alpha", "remove", "-alpha", "off", "-extent", "52x56",
        f"BMP3:{selector_path}",
    ])
    frame_paths.append(selector_path)

    hstack_args = []
    for frame_path in frame_paths:
        hstack_args.extend(["-i", str(frame_path)])
    run_ffmpeg([
        *hstack_args,
        "-filter_complex", f"hstack=inputs={len(frame_paths)},format=bgr24",
        str(player_out),
    ])
    for frame_path in frame_paths:
        frame_path.unlink()
    generated.append(player_out)

    cover_logo = None
    if waddle_source is not None:
        cover_logo = (waddle_source / "media/default/websites/modern/"
                      "assets/sites/default/themes/snowball/img/"
                      "club-penguin-logo.png")
    if cover_logo is not None and cover_logo.exists():
        run_magick([
            str(world_src), "-crop", "2600x1660+55+55", "+repage",
            "-resize", "120x140^", "-gravity", "west",
            "-extent", "120x140", "(", str(cover_logo), "-resize",
            "110x53", ")", "-gravity", "north", "-geometry", "+0+8",
            "-composite",
            f"BMP3:{cover_out}",
        ])
    else:
        run_ffmpeg([
            "-i", str(world_src),
            "-vf", "scale=120:140:force_original_aspect_ratio=increase,"
                   "crop=120:140",
            "-pix_fmt", "bgr24", str(cover_out),
        ])
    generated.append(cover_out)
    shutil.copyfile(cover_out, cover_space_out)
    shutil.copyfile(cover_out, cover_lower_out)
    shutil.copyfile(cover_out, cover_pane_out)
    shutil.copyfile(cover_out, cover_lower_pane_out)
    generated.extend([
        cover_space_out, cover_lower_out, cover_pane_out, cover_lower_pane_out
    ])

    if room_frames is not None and waddle_source is not None:
        package_room_frames(room_frames, waddle_source, out, generated)

    if backyard_export is not None:
        package_backyard(backyard_export, out, generated)

    if archive_room_export is not None:
        package_archive_rooms(archive_room_export, out, generated)

    if sound_studio_export is not None:
        package_sound_studio(sound_studio_export, out, generated)

    if cart_export is not None and waddle_source is not None:
        package_cart_surfer(cart_export, waddle_source, out, generated)

    if ui_export is not None and waddle_source is not None:
        package_interface(ui_export, waddle_source, out, generated)

    if puffle_export is not None and waddle_source is not None:
        package_puffle(puffle_export, waddle_source, out, generated)

    if puffle_food_export is not None:
        package_puffle_food(puffle_food_export, out, generated)

    if puffle_motion_export is not None:
        package_puffle_motion(puffle_motion_export, out, generated)

    if puffle_care_export is not None:
        package_puffle_care_ui(puffle_care_export, out, generated)

    if puffle_trick_export is not None:
        package_puffle_tricks(puffle_trick_export, out, generated)

    if puffle_play_export is not None:
        package_puffle_toys(puffle_play_export, out, generated)

    if puffle_hat_export is not None:
        package_puffle_hats(puffle_hat_export, out, generated)

    if furniture_export is not None and waddle_source is not None:
        package_furniture(furniture_export, waddle_source, out, generated)

    if igloo_export is not None and waddle_source is not None:
        package_igloo_layers(igloo_export, waddle_source, out, generated)

    if avatar_export is not None and waddle_source is not None:
        package_avatar(avatar_export, waddle_source, out, generated)

    if sport_export is not None and sport_avatar_export is not None:
        package_sport_catalog(sport_export, sport_avatar_export, out,
                              generated)

    if costume_export is not None and costume_avatar_export is not None:
        package_costume_catalog(costume_export, costume_avatar_export, out,
                                generated)

    if (penguin_style_export is not None and
            penguin_style_avatar_export is not None):
        package_penguin_style_catalog(
            penguin_style_export, penguin_style_avatar_export, out,
            generated,
        )

    if igloo_catalog_export is not None:
        package_igloo_catalogs(igloo_catalog_export, out, generated)

    if ninja_export is not None and ninja_avatar_export is not None:
        package_ninja_catalog(ninja_export, ninja_avatar_export, out,
                              generated)

    if puffle_adopt_export is not None:
        package_puffle_adoption(puffle_adopt_export, out, generated)

    if pet_furniture_export is not None:
        package_pet_furniture(pet_furniture_export, out, generated)

    if waddle_source is not None:
        package_shop(waddle_source, out, generated)

    write_tsvs(out)
    write_manifest(out, source, generated, waddle_source)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path,
                        help="path to a checked-out preserved asset repo")
    parser.add_argument("--out", type=Path,
                        default=Path("assets/ipodjs/rockbox/clubpenguin"))
    parser.add_argument("--room-frames", type=Path,
                        help="directory of rendered <room_id>.png frames")
    parser.add_argument("--waddle-source", type=Path,
                        help="path to a pinned nhaar/Waddle-Forever checkout")
    parser.add_argument("--backyard-export", type=Path,
                        help="official backyard SWFs, clean frames, and icons")
    parser.add_argument("--archive-room-export", type=Path,
                        help="official permanent-room SWFs and clean frames")
    parser.add_argument("--sound-studio-export", type=Path,
                        help="official Sound Studio SWFs, frames, and MP3s")
    parser.add_argument("--cart-export", type=Path,
                        help="JPEXS export of the pinned CartSurfer2006 SWF")
    parser.add_argument("--ui-export", type=Path,
                        help="JPEXS export of the pinned 2010 interface SWF")
    parser.add_argument("--puffle-export", type=Path,
                        help="JPEXS per-color official puffle sprite exports")
    parser.add_argument("--puffle-food-export", type=Path,
                        help="JPEXS care icons plus puffle_items.json")
    parser.add_argument("--puffle-motion-export", type=Path,
                        help="JPEXS official walk and dig sprite exports")
    parser.add_argument("--puffle-care-export", type=Path,
                        help="JPEXS export of the preserved puffle care UI")
    parser.add_argument("--puffle-trick-export", type=Path,
                        help="JPEXS exports of the official puffle tricks")
    parser.add_argument("--puffle-play-export", type=Path,
                        help="official puffle toy icons, sprites, and metadata")
    parser.add_argument("--puffle-hat-export", type=Path,
                        help="official puffle hat icons, layers, and metadata")
    parser.add_argument("--furniture-export", type=Path,
                        help="JPEXS first-frame exports plus furniture "
                             "metadata")
    parser.add_argument("--igloo-export", type=Path,
                        help="JPEXS building/floor/location frame exports")
    parser.add_argument("--avatar-export", type=Path,
                        help="JPEXS PNG/SVG exports of official paper dolls")
    parser.add_argument("--sport-export", type=Path,
                        help="JPEXS frames from the preserved sport catalog")
    parser.add_argument("--sport-avatar-export", type=Path,
                        help="JPEXS PNG/SVG exports of sport paper dolls")
    parser.add_argument("--costume-export", type=Path,
                        help="JPEXS frames from the Ruby costume catalog")
    parser.add_argument("--costume-avatar-export", type=Path,
                        help="JPEXS PNG/SVG exports of costume paper dolls")
    parser.add_argument("--penguin-style-export", type=Path,
                        help="JPEXS frames and metadata for Penguin Style")
    parser.add_argument("--penguin-style-avatar-export", type=Path,
                        help="JPEXS PNG/SVG exports of Style paper dolls")
    parser.add_argument("--igloo-catalog-export", type=Path,
                        help="preserved Furniture and Igloo catalog exports")
    parser.add_argument("--ninja-export", type=Path,
                        help="frames and calls from Martial Artworks")
    parser.add_argument("--ninja-avatar-export", type=Path,
                        help="JPEXS PNG/SVG exports of ninja paper dolls")
    parser.add_argument("--puffle-adopt-export", type=Path,
                        help="frames from the February 2011 adoption book")
    parser.add_argument("--pet-furniture-export", type=Path,
                        help="March 2010 Pet Furniture pages and secrets")
    args = parser.parse_args()

    if (args.room_frames is None) != (args.waddle_source is None):
        parser.error("--room-frames and --waddle-source must be used together")
    if args.cart_export is not None and args.waddle_source is None:
        parser.error("--cart-export requires --waddle-source")
    if args.ui_export is not None and args.waddle_source is None:
        parser.error("--ui-export requires --waddle-source")
    if args.puffle_export is not None and args.waddle_source is None:
        parser.error("--puffle-export requires --waddle-source")
    if args.furniture_export is not None and args.waddle_source is None:
        parser.error("--furniture-export requires --waddle-source")
    if args.igloo_export is not None and args.waddle_source is None:
        parser.error("--igloo-export requires --waddle-source")
    if args.avatar_export is not None and args.waddle_source is None:
        parser.error("--avatar-export requires --waddle-source")
    if (args.sport_export is None) != (args.sport_avatar_export is None):
        parser.error("--sport-export and --sport-avatar-export are paired")
    if ((args.costume_export is None) !=
            (args.costume_avatar_export is None)):
        parser.error(
            "--costume-export and --costume-avatar-export are paired"
        )
    if ((args.penguin_style_export is None) !=
            (args.penguin_style_avatar_export is None)):
        parser.error(
            "--penguin-style-export and "
            "--penguin-style-avatar-export are paired"
        )
    if (args.ninja_export is None) != (args.ninja_avatar_export is None):
        parser.error("--ninja-export and --ninja-avatar-export are paired")

    package_freeroam(
        args.source.resolve(),
        args.out,
        args.room_frames.resolve() if args.room_frames else None,
        args.waddle_source.resolve() if args.waddle_source else None,
        args.backyard_export.resolve() if args.backyard_export else None,
        args.archive_room_export.resolve()
        if args.archive_room_export else None,
        args.sound_studio_export.resolve()
        if args.sound_studio_export else None,
        args.cart_export.resolve() if args.cart_export else None,
        args.ui_export.resolve() if args.ui_export else None,
        args.puffle_export.resolve() if args.puffle_export else None,
        args.puffle_food_export.resolve()
        if args.puffle_food_export else None,
        args.puffle_motion_export.resolve()
        if args.puffle_motion_export else None,
        args.puffle_care_export.resolve()
        if args.puffle_care_export else None,
        args.puffle_trick_export.resolve()
        if args.puffle_trick_export else None,
        args.puffle_play_export.resolve()
        if args.puffle_play_export else None,
        args.puffle_hat_export.resolve()
        if args.puffle_hat_export else None,
        args.furniture_export.resolve() if args.furniture_export else None,
        args.igloo_export.resolve() if args.igloo_export else None,
        args.avatar_export.resolve() if args.avatar_export else None,
        args.sport_export.resolve() if args.sport_export else None,
        args.sport_avatar_export.resolve()
        if args.sport_avatar_export else None,
        args.costume_export.resolve() if args.costume_export else None,
        args.costume_avatar_export.resolve()
        if args.costume_avatar_export else None,
        args.penguin_style_export.resolve()
        if args.penguin_style_export else None,
        args.penguin_style_avatar_export.resolve()
        if args.penguin_style_avatar_export else None,
        args.igloo_catalog_export.resolve()
        if args.igloo_catalog_export else None,
        args.ninja_export.resolve() if args.ninja_export else None,
        args.ninja_avatar_export.resolve()
        if args.ninja_avatar_export else None,
        args.puffle_adopt_export.resolve()
        if args.puffle_adopt_export else None,
        args.pet_furniture_export.resolve()
        if args.pet_furniture_export else None,
    )


if __name__ == "__main__":
    main()

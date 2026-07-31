Offline Maps assets

world_satellite.r16 is a 320x160 RGB565 frame derived from NASA's Blue Marble
global satellite mosaic (Terra/MODIS).  maritime_satellite.r16 is a 320x160
RGB565 crop covering eastern Canada and the Maritimes.  Both are packaged to
/.rockbox/maps/ for the Maps plugin and are loaded only when the user enters
Satellite mode; the render loop never performs I/O.

Attribution: NASA/Goddard Space Flight Center Scientific Visualization Studio;
Blue Marble data courtesy of Reto Stöckli (NASA/GSFC) and NASA Earth
Observatory.

world_tiles/ contains the complete global NASA Blue Marble tile pyramid for
zoom levels 1 through 3 (84 exact 320x160 RGB565 frames). It uses the same
NASA/Goddard and Blue Marble attribution above. RockPod can extend the atlas
with deeper licensed z/x/y satellite source tiles.

Street imagery

The plugin never substitutes a generated scene for missing Street View
coverage. A location either displays the cited real source below or reports
that an additional real image pack must be synced.

times_square_kartaview.jpg is a real 360-degree frame from KartaView photo
2481577465 (Times Square, New York; 2024-10-10).  The four
times_square_*.rgb files are 320x184 RGB565 directional views derived from
that source for offline use. KartaView imagery is CC BY-SA 4.0; attribute
KartaView / Grab and the contributor when redistributing this pack.

london_kartaview.jpg is a real KartaView dashcam frame from photo 1304128585
near Westminster, London (2021-05-17). london_kartaview.rgb is its 320x184
RGB565 offline frame. The same KartaView / Grab CC BY-SA 4.0 attribution
applies.

berlin_kartaview.jpg is a real KartaView dashcam frame from photo 1351162929
in central Berlin (2021-07-30). berlin_kartaview.rgb is its 320x184 RGB565
offline frame; KartaView / Grab CC BY-SA 4.0 attribution applies.

paris_panoramax.jpg is a real Panoramax image near the Eiffel Tower, picture
8ba2102a-9745-4e60-81e2-9c95a9199a7f (2025-03-30), contributed by tykayn.
paris_panoramax.rgb is its 320x184 RGB565 offline frame. The source is
CC-BY-SA-4.0 and must retain that attribution when redistributed.

toronto_panoramax_0.jpg through toronto_panoramax_2.jpg are a real three-frame
downtown Toronto street-camera sequence from Panoramax pictures
6f148420-16ea-4693-a03f-79f40a8a783d, d7cce051-d889-427c-89c5-e7777fd8441c,
and 95c5d728-01fa-4f86-a9be-04fb9cd83103 (2025-08-18), contributed by
tallcoleman. The matching RGB565 frames advance only on an explicit Left or
Right action. The sources are CC-BY-SA-4.0.

toronto_360_north.rgb through toronto_360_west.rgb are four perspective views
from the real 360-degree Panoramax source picture
d7cce051-d889-427c-89c5-e7777fd8441c. Its metadata identifies a GoPro Max
with a 360-degree field of view. Views are projected offline from the original
equirectangular image; Left and Right switch only between these real views.

tokyo_panoramax.jpg is a real street-camera frame beside Tokyo Tower from
Panoramax picture cee972d7-3df0-442e-9eed-459b0d9196f1 (2026-07-07),
contributed by pizzaiolo. tokyo_panoramax.rgb is its 320x184 RGB565 offline
frame. The source is CC-BY-SA-4.0.

fredericton_panoramax_0.jpg and fredericton_panoramax_1.jpg are real adjacent
Fredericton street-camera frames from Panoramax pictures
54dd904f-b96f-4db8-918f-79636d9b26f3 and
71d5bb34-c39b-42a0-a4a8-0161d54e05a5 (2026-04-20), contributed by breau.
Their matching RGB565 frames switch only on an explicit Left or Right action.
The sources are CC-BY-SA-4.0.

Legacy New Brunswick Maps assets

nb_imagery_overview.jpg was exported from:
https://geonb.snb.ca/arcgis/rest/services/GeoNB_Basemap_Imagery/MapServer

full_nb_tiles/ contains generated raw RGB565 Rockbox frames:

- street_z3_XX_YY.r16 / imagery_z3_XX_YY.r16: 10x10 grid
- street_z4_XX_YY.r16 / imagery_z4_XX_YY.r16: 20x20 grid
- street_z5_XX_YY.r16 / imagery_z5_XX_YY.r16: 40x40 grid
- street_XX_YY.r16 / imagery_XX_YY.r16: legacy 10x10 z3 names

The tile grids use the New Brunswick Stereographic projection (EPSG:2036) and
the plugin extent:

2306915,7277892,2710943,7674855

Each tile is 320x184 pixels and is loaded directly by the plugin from
/.rockbox/maps/new_brunswick/. Detailed views are generated from the official
GeoNB_Basemap_Grey and GeoNB_Basemap_Imagery services. The vector overview in
the plugin is a navigation aid; the tiled street and imagery views are the
accuracy source.

Attribution: contains information from GeoNB / Service New Brunswick.

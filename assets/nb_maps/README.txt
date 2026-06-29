New Brunswick Maps assets

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

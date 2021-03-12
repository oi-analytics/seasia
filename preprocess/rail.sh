#!/usr/bin/env bash
#
# Extract railways and stations from OSM
#

countries=(
    "cambodia"
    "indonesia"
    "laos"
    "myanmar"
    "philippines"
    "thailand"
    "vietnam"
)
for country in "${countries[@]}"; do
    osmium tags-filter \
        incoming_data/OpenStreetMap/$country-latest.osm.pbf \
        wnr/railway \
        -o incoming_data/OpenStreetMap/railways/$country-rail.osm.pbf

    OSM_CONFIG_FILE=preprocess/osmconf_rail.ini ogr2ogr -f GPKG \
        incoming_data/OpenStreetMap/railways/$country-rail.gpkg \
        incoming_data/OpenStreetMap/railways/$country-rail.osm.pbf \
        points lines multipolygons
done

# Run notebook
# jupyter nbconvert --to notebook --execute process_rail.ipynb

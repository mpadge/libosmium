#!/bin/sh
#
#  download_data.sh
#

cd $DATA_DIR
curl --location --output 1_liechtenstein.osm.pbf http://download.geofabrik.de/europe/liechtenstein-latest.osm.pbf   # about   2 MB
curl --location --output 2_bremen.osm.pbf        http://download.geofabrik.de/europe/germany/bremen-latest.osm.pbf  # about  16 MB
curl --location --output 3_sachsen.osm.pbf       http://download.geofabrik.de/europe/germany/sachsen-latest.osm.pbf # about 160 MB
curl --location --output 4_germany.osm.pbf       http://download.geofabrik.de/europe/germany-latest.osm.pbf         # about   3 GB
curl --location --output 5_planet.osm.pbf        http://planet.osm.org/pbf/planet-latest.osm.pbf                    # about  35 GB


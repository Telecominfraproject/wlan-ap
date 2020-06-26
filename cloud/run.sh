#!/bin/sh

#sudo docker login --username tip-read https://tip-tip-wlan-cloud-docker-repo.jfrog.io
#sudo docker pull tip-tip-wlan-cloud-docker-repo.jfrog.io/opensync-gateway-and-mqtt:0.0.1-SNAPSHOT

mkdir -p mosquitto/data
mkdir -p mosquitto/log
mkdir -p app/log
docker run --rm -it -p 1883:1883 -p 6640:6640 -p 6643:6643 -p 4043:4043 \
	-v /${PWD}/mosquitto/data:/mosquitto/data \
	-v /${PWD}/mosquitto/log:/mosquitto/log \
	-v ${PWD}/../keys:/opt/tip-wlan/certs \
	-v /${PWD}/app/log:/app/logs \
	-v /${PWD}/app/config:/app/config \
	-e OVSDB_IF_DEFAULT_BRIDGE='lan'   \
	-e OVSDB_EQUIPMENT_CONFIG_FILE='/app/config/EquipmentExample.json' \
	-e OVSDB_AP_PROFILE_CONFIG_FILE='/app/config/ProfileAPExample.json' \
	-e OVSDB_SSIDPROFILE_CONFIG_FILE='/app/config/ProfileSsid.json' \
	-e OVSDB_RADIUSPROFILE_CONFIG_FILE='/app/config/ProfileRadius.json' \
	-e OVSDB_LOCATION_CONFIG_FILE='/app/config/LocationBuildingExample.json' \
	tip-tip-wlan-cloud-docker-repo.jfrog.io/opensync-gateway-and-mqtt:0.0.1-SNAPSHOT

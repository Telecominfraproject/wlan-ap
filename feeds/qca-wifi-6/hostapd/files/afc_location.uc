#!/usr/bin/env ucode
'use strict';
let fs = require("fs");
let ubus = require('ubus').connect();

let gps_info = ubus.call('gps', 'info');
let latitude = gps_info.latitude || 0;
let longitude = gps_info.longitude || 0;

// afc-location.json file content
let afc_location = {};
afc_location.location_type = "ellipse";
afc_location.location = longitude + ":" + latitude ;
afc_location.height = gps_info.elevation || 0;
afc_location.height_type = "AMSL";
afc_location.major_axis = int(gps_info.major_axis) || 1;
afc_location.minor_axis = int(gps_info.minor_axis) || 1;
afc_location.orientation = gps_info.major_orientation || 0;
afc_location.vertical_tolerance = int(gps_info.vdop) || 1;

let afc_location_json = fs.open("/etc/ucentral/afc-location.json", "w");
afc_location_json.write(afc_location);
afc_location_json.close();

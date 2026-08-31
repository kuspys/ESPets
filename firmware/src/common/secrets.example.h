// Template for src/common/secrets.h (which is gitignored — never commit real credentials).
// Copy this file to secrets.h and fill in your own WiFi network before building
// any environment that needs WiFi (e.g. ota-test).
//
//   cp src/common/secrets.example.h src/common/secrets.h

#pragma once

#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// Optional: pin the OTA hostname/password used by ArduinoOTA. Leave the
// password empty to disable auth (fine for a closed dev network only).
#define OTA_HOSTNAME  "espets-ota-test"
#define OTA_PASSWORD  ""

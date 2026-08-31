// Shared OTA (Over-The-Air) update helper.
//
// Once a cube is sealed in its enclosure, USB is gone — this is the only way
// to push new firmware. Drop it into any sketch with two calls:
//
//   #include "../common/ota.h"
//   void setup() { ...; OtaSupport::begin(); ...; }
//   void loop()  { OtaSupport::handle(); ...; }
//
// Then flash once over USB, and every update after that goes over WiFi:
//   pio run -e <env> -t upload --upload-port <board-ip-from-serial-log>
//
// Requires src/common/secrets.h (copy secrets.example.h) for WIFI_SSID /
// WIFI_PASSWORD / OTA_HOSTNAME / OTA_PASSWORD.
//
// begin() is deliberately non-blocking on failure: a board that boots out of
// WiFi range should keep running its normal logic, not hang forever waiting
// for a network that isn't there. Check the return value if the caller needs
// to know whether OTA actually came up.

#ifndef ESPETS_OTA_H
#define ESPETS_OTA_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"

namespace OtaSupport {

static bool otaReady = false;

inline bool begin(const char *hostname = OTA_HOSTNAME, unsigned long timeoutMs = 10000) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.printf("[OTA] Connecting to WiFi '%s'...\n", WIFI_SSID);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) {
            Serial.println("[OTA] WiFi connect timed out — continuing without OTA.");
            return false;
        }
        delay(200);
    }
    Serial.printf("[OTA] WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());

    ArduinoOTA.setHostname(hostname);
    if (strlen(OTA_PASSWORD) > 0) {
        ArduinoOTA.setPassword(OTA_PASSWORD);
    }

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.printf("[OTA] Start: updating %s\n", type.c_str());
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("[OTA] Update complete. Rebooting...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", (progress * 100) / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error [%u]\n", error);
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready.");
    otaReady = true;
    return true;
}

// Call every loop() iteration. Cheap no-op if OTA never came up.
inline void handle() {
    if (otaReady) {
        ArduinoOTA.handle();
    }
}

inline bool isReady() {
    return otaReady;
}

} // namespace OtaSupport

#endif

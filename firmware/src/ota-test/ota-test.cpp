// OTA (Over-The-Air) update proof-of-concept for ESPets cubes.
//
// First flash MUST be over USB (upload_protocol=esptool, the default).
// Once the board prints its IP, subsequent uploads can go over WiFi:
//   pio run -e ota-test -t upload --upload-port <printed-ip>
// or by setting upload_port/upload_protocol=espota in platformio.ini.
//
// Requires src/common/secrets.h — copy src/common/secrets.example.h and
// fill in your WiFi credentials before building this environment.

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "../common/secrets.h"

static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
static const unsigned long HEARTBEAT_INTERVAL_MS = 5000;

static unsigned long lastHeartbeat = 0;

static bool connectToWiFi() {
    Serial.printf("Connecting to WiFi SSID '%s'...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("WiFi connect timed out.");
            return false;
        }
        delay(250);
        Serial.print(".");
    }

    Serial.println();
    Serial.printf("WiFi connected. IP address: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

static void setupOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    if (strlen(OTA_PASSWORD) > 0) {
        ArduinoOTA.setPassword(OTA_PASSWORD);
    }

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.printf("OTA start: updating %s\n", type.c_str());
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA update complete. Rebooting...");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA progress: %u%%\r", (progress * 100) / total);
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA error [%u]: ", error);
        switch (error) {
            case OTA_AUTH_ERROR:    Serial.println("Auth failed"); break;
            case OTA_BEGIN_ERROR:   Serial.println("Begin failed"); break;
            case OTA_CONNECT_ERROR: Serial.println("Connect failed"); break;
            case OTA_RECEIVE_ERROR: Serial.println("Receive failed"); break;
            case OTA_END_ERROR:     Serial.println("End failed"); break;
            default:                Serial.println("Unknown error"); break;
        }
    });

    ArduinoOTA.begin();
    Serial.println("ArduinoOTA ready.");
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- ESPets OTA Test ---");

    if (!connectToWiFi()) {
        Serial.println("Halting: no WiFi, OTA unavailable. Reset the board to retry.");
        while (true) {
            delay(1000);
        }
    }

    setupOTA();
}

void loop() {
    ArduinoOTA.handle();

    unsigned long now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;
        Serial.printf("Alive. IP=%s  RSSI=%ddBm  Uptime=%lus\n",
                       WiFi.localIP().toString().c_str(), WiFi.RSSI(), now / 1000);
    }
}

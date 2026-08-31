// OTA (Over-The-Air) update proof-of-concept for ESPets cubes.
// Thin wrapper around the shared src/common/ota.h module — see that file
// for how to drop OTA into any other sketch.
//
// First flash MUST be over USB (upload_protocol=esptool, the default).
// Once the board prints its IP, subsequent uploads can go over WiFi:
//   pio run -e ota-test -t upload --upload-port <printed-ip>
//
// Requires src/common/secrets.h — copy src/common/secrets.example.h and
// fill in your WiFi credentials before building this environment.

#include <Arduino.h>
#include "../common/ota.h"

static const unsigned long HEARTBEAT_INTERVAL_MS = 5000;
static unsigned long lastHeartbeat = 0;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- ESPets OTA Test ---");

    if (!OtaSupport::begin()) {
        Serial.println("Halting: no WiFi, OTA unavailable. Reset the board to retry.");
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    OtaSupport::handle();

    unsigned long now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;
        Serial.printf("Alive. IP=%s  RSSI=%ddBm  Uptime=%lus\n",
                       WiFi.localIP().toString().c_str(), WiFi.RSSI(), now / 1000);
    }
}

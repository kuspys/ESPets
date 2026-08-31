// ESPets "pet:3" paired demo: two boards share one pet over ESP-NOW.
//
// Whichever board holds the pet cycles through the normal mood animation
// (idle/happy/sleeping/walking, pulled from Animated Cat Face.dc.html).
// The other board shows the "pet:3" logo (gently breathing) and waits.
// Once a full mood cycle finishes, IF the two boards can currently hear
// each other (a periodic ESP-NOW heartbeat), the pet vanishes with a
// wipe/sparkle effect, a transfer packet goes to the peer, and it
// reappears there with the reverse effect. If only one board is powered
// (no heartbeat from a peer), a board never sits stuck on the logo forever
// — it just spawns its own pet locally and loops the regular demo solo.
//
// Flash one board with -D PET_OWNER=1 (env:pet-demo-a) so it starts
// holding the pet; flash the other with env:pet-demo-b so it starts
// showing the logo. Requires OTA channel-matching to be attempted first;
// see setupWiFiAndEspNow(), copied from the proven pattern in ball-demo.cpp.
//
// Sprites use true-black (#000000) ink and standard RGB565 packing, and
// every frame is composited into an off-screen TFT_eSprite buffer and
// pushed atomically — see screen-test.cpp / the tearing-fix commit for why.

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "../common/ota.h"
#include "frame_idle_open.h"
#include "frame_idle_blink.h"
#include "frame_happy.h"
#include "frame_sleep.h"
#include "frame_logo.h"

TFT_eSPI tft = TFT_eSPI(240, 240);
TFT_eSprite frameBuf = TFT_eSprite(&tft);

#define LCD_BL 20
static const int SCREEN_SIZE = 240;

// #FF2E93 (brand pink) and #FFD700 (brand gold), standard RGB565 packing.
static const uint16_t HEART_PINK = 0xF972;
static const uint16_t SPARKLE_GOLD = 0xFEA0;

// ---------------------------------------------------------------------
// ESP-NOW: peer presence heartbeat + one-shot pet transfer packet.
// ---------------------------------------------------------------------

enum PacketType : uint8_t { PKT_HEARTBEAT = 0, PKT_TRANSFER = 1 };

typedef struct __attribute__((packed)) {
    uint8_t senderMac[6]; // trust this over the driver's recv-callback mac_addr,
                          // which has been observed misreporting the sender
    uint8_t type;
} PetPacket;

static uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t ownMac[6];
static const uint8_t ESPNOW_CHANNEL = 1;

static const unsigned long HEARTBEAT_INTERVAL_MS = 600;
static const unsigned long PEER_TIMEOUT_MS = 2500;   // ~4 missed heartbeats
static const unsigned long BOOT_GRACE_MS = 4000;     // give a peer time to join at boot

static unsigned long bootMs = 0;
static unsigned long lastHeartbeatSentAt = 0;
static unsigned long lastPeerSeenAt = 0;
static bool everHeardPeer = false;
static bool paired = false;

static volatile bool incomingPending = false;
static PetPacket incomingPacket;

static void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(PetPacket)) {
        return;
    }
    PetPacket packet;
    memcpy(&packet, data, sizeof(packet));
    // ESP-NOW broadcasts can loop back into the sender's own recv callback,
    // and the driver's mac_addr argument has been observed misreporting the
    // sender in that case — trust the MAC embedded in the payload instead.
    if (memcmp(packet.senderMac, ownMac, sizeof(ownMac)) == 0) {
        return;
    }
    memcpy((void *)&incomingPacket, &packet, sizeof(packet));
    incomingPending = true;
}

static void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    // Quiet by design — heartbeats fire every 600ms and would flood serial.
}

static void setupWiFiAndEspNow() {
    // Try OTA first — if it joins a real AP, both boards land on that AP's
    // channel and stay consistent with each other automatically. Only fall
    // back to forcing a fixed channel if there's no network in range.
    bool otaConnected = OtaSupport::begin();
    WiFi.macAddress(ownMac);
    Serial.print("Local MAC Address: ");
    Serial.println(WiFi.macAddress());

    uint8_t peerChannel = 0; // 0 = use whatever channel WiFi ended up on
    if (!otaConnected) {
        esp_wifi_set_promiscuous(true);
        esp_err_t chResult = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
        Serial.printf("No WiFi — forced ESP-NOW channel %d (%s)\n",
                       ESPNOW_CHANNEL, esp_err_to_name(chResult));
        peerChannel = ESPNOW_CHANNEL;
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = peerChannel;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add ESP-NOW peer");
    }
}

static void sendPacket(PacketType type) {
    PetPacket packet;
    memcpy(packet.senderMac, ownMac, sizeof(ownMac));
    packet.type = (uint8_t)type;
    esp_now_send(broadcastAddress, (uint8_t *)&packet, sizeof(packet));
}

// ---------------------------------------------------------------------
// Pet state machine.
// ---------------------------------------------------------------------

enum Mode { MODE_IDLE, MODE_HAPPY, MODE_SLEEP, MODE_WALK };
static const unsigned long MODE_DURATION_MS[] = {6000, 5000, 6000, 5000};
static Mode currentMode = MODE_IDLE;
static unsigned long modeStartMs = 0;

static unsigned long lastBlinkAt = 0;
static unsigned long blinkUntil = 0;
static const unsigned long BLINK_INTERVAL_MS = 3200;
static const unsigned long BLINK_DURATION_MS = 180;

static const int NUM_OVERLAY = 2;

enum Effect { EFFECT_NONE, EFFECT_DISAPPEARING, EFFECT_APPEARING };
static Effect effect = EFFECT_NONE;
static unsigned long effectStartMs = 0;
static const unsigned long EFFECT_DURATION_MS = 700;
static const int SPARKLE_COUNT = 4;

static bool ownsPet = false;

static void drawHeart(int cx, int cy, int size) {
    int r = size / 2;
    frameBuf.fillCircle(cx - r / 2, cy, r / 2, HEART_PINK);
    frameBuf.fillCircle(cx + r / 2, cy, r / 2, HEART_PINK);
    frameBuf.fillTriangle(cx - r, cy, cx + r, cy, cx, cy + r, HEART_PINK);
}

static void drawOverlays(unsigned long now) {
    if (currentMode == MODE_HAPPY) {
        for (int i = 0; i < NUM_OVERLAY; i++) {
            float phase = fmod((now / 1400.0) + i * 0.5, 1.0);
            int y = 20 - (int)(phase * 18);
            int x = 90 + i * 60;
            drawHeart(x, y, 8);
        }
    } else if (currentMode == MODE_SLEEP) {
        frameBuf.setTextColor(TFT_WHITE, TFT_BLACK);
        frameBuf.setTextSize(1);
        for (int i = 0; i < NUM_OVERLAY; i++) {
            float phase = fmod((now / 1600.0) + i * 0.5, 1.0);
            int y = 20 - (int)(phase * 16);
            int x = 170 + i * 12;
            frameBuf.setCursor(x, y);
            frameBuf.print("Z");
        }
    }
}

// Advances the mood cycle. On wrapping WALK -> IDLE (one full lap), attempts
// a handoff if a peer is currently reachable; otherwise just keeps looping.
static void tickModeCycle(unsigned long now) {
    if (now - modeStartMs < MODE_DURATION_MS[currentMode]) {
        return;
    }
    if (currentMode == MODE_WALK) {
        if (paired) {
            effect = EFFECT_DISAPPEARING;
            effectStartMs = now;
            Serial.println("Full lap complete, peer present -> vanishing.");
            return; // freeze at idle pose; wipe effect takes over rendering
        }
        currentMode = MODE_IDLE;
        modeStartMs = now;
        return;
    }
    currentMode = (Mode)(currentMode + 1);
    modeStartMs = now;
}

static void drawFace(unsigned long now) {
    const uint16_t *sprite;
    int w, h;
    int offsetX = 0, offsetY = 0;

    switch (currentMode) {
        case MODE_IDLE: {
            bool blinking = now < blinkUntil;
            if (!blinking && now - lastBlinkAt > BLINK_INTERVAL_MS) {
                lastBlinkAt = now;
                blinkUntil = now + BLINK_DURATION_MS;
                blinking = true;
            }
            sprite = blinking ? IDLE_BLINK_SPRITE : IDLE_OPEN_SPRITE;
            w = IDLE_OPEN_WIDTH;
            h = IDLE_OPEN_HEIGHT;
            offsetY = (int)(-10 * sin(now / 1600.0 * 2 * PI));
            break;
        }
        case MODE_HAPPY:
            sprite = HAPPY_SPRITE;
            w = HAPPY_WIDTH;
            h = HAPPY_HEIGHT;
            offsetY = (int)(-4 * sin(now / 900.0 * 2 * PI));
            break;
        case MODE_SLEEP:
            sprite = SLEEP_SPRITE;
            w = SLEEP_WIDTH;
            h = SLEEP_HEIGHT;
            offsetY = (int)(-4 * sin(now / 1800.0 * 2 * PI));
            break;
        case MODE_WALK: {
            bool blinking = now < blinkUntil;
            if (!blinking && now - lastBlinkAt > BLINK_INTERVAL_MS) {
                lastBlinkAt = now;
                blinkUntil = now + BLINK_DURATION_MS;
                blinking = true;
            }
            sprite = blinking ? IDLE_BLINK_SPRITE : IDLE_OPEN_SPRITE;
            w = IDLE_OPEN_WIDTH;
            h = IDLE_OPEN_HEIGHT;
            offsetX = (int)(25 * sin(now / 3000.0 * 2 * PI));
            offsetY = (int)(-8 * sin(now / 1600.0 * 2 * PI));
            break;
        }
    }

    int x = (SCREEN_SIZE - w) / 2 + offsetX;
    int y = (SCREEN_SIZE - h) / 2 + offsetY;
    frameBuf.pushImage(x, y, w, h, sprite);
}

// Renders the idle pose with a "blinds closing toward the center" wipe:
// coverFraction 0 = fully visible, 1 = fully covered. Used for both the
// disappear (0->1) and appear (1->0) transitions, with a few gold sparkles
// riding the boundary line for flourish.
static void drawIdleWithWipe(float coverFraction) {
    int w = IDLE_OPEN_WIDTH, h = IDLE_OPEN_HEIGHT;
    int x = (SCREEN_SIZE - w) / 2;
    int y = (SCREEN_SIZE - h) / 2;
    frameBuf.pushImage(x, y, w, h, IDLE_OPEN_SPRITE);

    int halfCover = (int)(coverFraction * (h / 2));
    if (halfCover <= 0) {
        return;
    }
    int cy = y + h / 2;
    frameBuf.fillRect(x, cy - halfCover, w, halfCover * 2, TFT_BLACK);

    unsigned long now = millis();
    for (int i = 0; i < SPARKLE_COUNT; i++) {
        int sx = x + (i + 1) * w / (SPARKLE_COUNT + 1);
        int jitter = (int)(3 * sin(now / 80.0 + i));
        frameBuf.fillCircle(sx, cy - halfCover + jitter, 2, SPARKLE_GOLD);
        frameBuf.fillCircle(sx, cy + halfCover - jitter, 2, SPARKLE_GOLD);
    }
}

static void drawWaitingLogo(unsigned long now) {
    int offsetY = (int)(-4 * sin(now / 2000.0 * 2 * PI));
    int x = (SCREEN_SIZE - LOGO_WIDTH) / 2;
    int y = (SCREEN_SIZE - LOGO_HEIGHT) / 2 + offsetY;
    frameBuf.pushImage(x, y, LOGO_WIDTH, LOGO_HEIGHT, LOGO_SPRITE);
}

static void maybeSendHeartbeat(unsigned long now) {
    if (now - lastHeartbeatSentAt < HEARTBEAT_INTERVAL_MS) {
        return;
    }
    lastHeartbeatSentAt = now;
    sendPacket(PKT_HEARTBEAT);
}

static void updatePairingState(unsigned long now) {
    bool wasPaired = paired;
    paired = everHeardPeer && (now - lastPeerSeenAt < PEER_TIMEOUT_MS);
    if (paired != wasPaired) {
        Serial.println(paired ? "Paired with peer." : "Lost peer.");
    }
}

static void drainIncomingPackets(unsigned long now) {
    if (!incomingPending) {
        return;
    }
    PetPacket packet;
    memcpy(&packet, (const void *)&incomingPacket, sizeof(packet));
    incomingPending = false;

    lastPeerSeenAt = now;
    if (!everHeardPeer) {
        everHeardPeer = true;
        Serial.println("Peer detected.");
    }

    if (packet.type == PKT_TRANSFER && !ownsPet && effect == EFFECT_NONE) {
        Serial.println("Pet arriving from peer!");
        ownsPet = true;
        effect = EFFECT_APPEARING;
        effectStartMs = now;
    }
}

// A board with no reachable peer should never sit stuck waiting forever —
// spawn a pet locally and loop the regular demo solo.
static bool shouldFallbackSolo(unsigned long now) {
    if (paired) {
        return false;
    }
    if (everHeardPeer) {
        return true; // was paired, lost the peer mid-wait
    }
    return (now - bootMs) > BOOT_GRACE_MS; // never heard a peer at all
}

void showBootSplash() {
    tft.fillScreen(TFT_BLACK);
    int x = (SCREEN_SIZE - LOGO_WIDTH) / 2;
    int y = (SCREEN_SIZE - LOGO_HEIGHT) / 2;
    tft.pushImage(x, y, LOGO_WIDTH, LOGO_HEIGHT, LOGO_SPRITE);
    delay(1200);
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n--- pet:3 Demo (ESP-NOW paired) ---");

    setupWiFiAndEspNow();

    // Keep the backlight off until the panel is initialized and blanked —
    // GRAM holds uninitialized/random data right after power-up, which shows
    // through as RGB noise if the backlight is already on during tft.begin().
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, LOW);

    tft.begin();
    tft.invertDisplay(1);
    tft.setRotation(0);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    digitalWrite(LCD_BL, HIGH);

    showBootSplash();

    frameBuf.setColorDepth(16);
    frameBuf.setSwapBytes(true);
    if (frameBuf.createSprite(SCREEN_SIZE, SCREEN_SIZE) == nullptr) {
        Serial.println("Failed to allocate frame buffer sprite — halting.");
        while (true) {
            delay(1000);
        }
    }

#if defined(PET_OWNER) && PET_OWNER
    ownsPet = true;
    Serial.println("Starting as pet owner.");
#else
    ownsPet = false;
    Serial.println("Starting without pet — showing logo, waiting for a peer.");
#endif
    currentMode = MODE_IDLE;
    modeStartMs = millis();

    // Start the boot-grace clock only now, right before the main loop takes
    // over — WiFi connect (up to OTA's own ~10s timeout) and the splash
    // delay above can easily eat the whole grace window on their own,
    // which previously caused a fallback-solo pet to spawn before the board
    // had a real chance to listen for a peer's heartbeat.
    bootMs = millis();
}

void loop() {
    OtaSupport::handle();

    unsigned long now = millis();
    drainIncomingPackets(now);
    maybeSendHeartbeat(now);
    updatePairingState(now);

    frameBuf.fillSprite(TFT_BLACK);

    if (effect == EFFECT_DISAPPEARING) {
        float p = min(1.0f, (now - effectStartMs) / (float)EFFECT_DURATION_MS);
        drawIdleWithWipe(p);
        if (p >= 1.0f) {
            sendPacket(PKT_TRANSFER);
            ownsPet = false;
            effect = EFFECT_NONE;
            Serial.println("Sent pet to peer.");
        }
    } else if (effect == EFFECT_APPEARING) {
        float p = min(1.0f, (now - effectStartMs) / (float)EFFECT_DURATION_MS);
        drawIdleWithWipe(1.0f - p);
        if (p >= 1.0f) {
            effect = EFFECT_NONE;
            currentMode = MODE_IDLE;
            modeStartMs = now;
        }
    } else if (ownsPet) {
        tickModeCycle(now);
        if (effect == EFFECT_NONE) {
            drawOverlays(now);
            drawFace(now);
        } else {
            drawIdleWithWipe(0.0f); // first frame of the just-triggered vanish
        }
    } else if (paired) {
        drawWaitingLogo(now);
    } else if (shouldFallbackSolo(now)) {
        Serial.println("No peer reachable — spawning pet locally.");
        ownsPet = true;
        currentMode = MODE_IDLE;
        modeStartMs = now;
        drawFace(now);
    } else {
        drawWaitingLogo(now); // still within boot grace window
    }

    frameBuf.pushSprite(0, 0);
    delay(66); // ~15 FPS
}

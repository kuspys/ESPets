// ESPets ESP-NOW demo: a single ball bounces around one board's screen and,
// when it reaches the left/right edge, hands off to the other board over
// ESP-NOW so it re-enters from the matching edge there — the two screens
// read as one continuous world the ball travels between.
//
// Flash one board with -D BALL_OWNER=1 (env:ball-demo-a) so it starts holding
// the ball; flash the other with env:ball-demo-b so it starts empty and waits.

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "../common/ota.h"

#define LCD_BL 20
static const int SCREEN_SIZE = 240;
static const int BALL_RADIUS = 12;
static const float BALL_SPEED = 140.0f; // px/sec

TFT_eSPI tft = TFT_eSPI(SCREEN_SIZE, SCREEN_SIZE);

// Wire format for handing the ball off to the other screen.
typedef struct __attribute__((packed)) {
    uint8_t senderMac[6]; // trust this over the driver's recv-callback mac_addr,
                          // which has been observed misreporting the sender
    float x, y;           // entry position in the receiving screen's coordinate space
    float vx, vy;          // px/sec
    uint16_t color;
} BallPacket;

static uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t ownMac[6];

// ESP32 remembers the last WiFi channel it associated with (NVS) even across
// reboots into firmware that never calls WiFi.begin() — one board having
// previously run the OTA firmware (which joined a real AP) is enough to
// desync it from a board that never associated with anything. Force both
// onto the same fixed channel via promiscuous mode, which (unlike a plain
// esp_wifi_set_channel call) works even before the STA interface is fully up.
static const uint8_t ESPNOW_CHANNEL = 1;

// Local ball state.
static bool ballActive = false;
static bool hasPrevPos = false;
static float ballX, ballY, ballVX, ballVY;
static uint16_t ballColor;
static float prevX, prevY;
static unsigned long lastFrameMs = 0;

// Set from the ESP-NOW receive callback; drained once per loop().
static volatile bool incomingPending = false;
static BallPacket incomingPacket;

static void spawnBall(float x, float y, float vx, float vy, uint16_t color) {
    ballX = x;
    ballY = y;
    ballVX = vx;
    ballVY = vy;
    ballColor = color;
    ballActive = true;
    hasPrevPos = false;
    tft.fillScreen(TFT_BLACK);
}

static void sendBallToPeer(float x, float y, float vx, float vy, uint16_t color) {
    BallPacket packet;
    memcpy(packet.senderMac, ownMac, sizeof(ownMac));
    packet.x = x;
    packet.y = y;
    packet.vx = vx;
    packet.vy = vy;
    packet.color = color;
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&packet, sizeof(packet));
    Serial.printf("[send] esp_now_send() returned %d (%s)\n", result, esp_err_to_name(result));
}

static void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(BallPacket)) {
        return;
    }
    BallPacket packet;
    memcpy(&packet, data, sizeof(BallPacket));

    Serial.printf("[recv] driver says from %02X:%02X:%02X:%02X:%02X:%02X, packet says from %02X:%02X:%02X:%02X:%02X:%02X\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                   packet.senderMac[0], packet.senderMac[1], packet.senderMac[2],
                   packet.senderMac[3], packet.senderMac[4], packet.senderMac[5]);

    // ESP-NOW broadcasts can loop back into the sender's own recv callback,
    // and the driver's mac_addr argument has been observed misreporting the
    // sender in that case — trust the MAC embedded in the payload instead.
    if (memcmp(packet.senderMac, ownMac, sizeof(ownMac)) == 0) {
        Serial.println("[recv] ignoring self-loop");
        return;
    }
    memcpy((void *)&incomingPacket, &packet, sizeof(BallPacket));
    incomingPending = true;
}

static void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    Serial.printf("[send] status=%s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
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
        // ESP32 remembers the last WiFi channel it associated with (NVS)
        // even across reboots into firmware that never calls WiFi.begin() —
        // one board having previously joined an AP is enough to desync it
        // from a board that never associated with anything. With no AP in
        // range for either board right now, force both onto the same fixed
        // channel via promiscuous mode, which (unlike a plain
        // esp_wifi_set_channel call) works even before the STA interface is
        // fully initialized.
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

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- ESPets ESP-NOW Ball Demo ---");

    // Keep the backlight off until the panel is initialized and blanked —
    // GRAM holds uninitialized/random data right after power-up, which shows
    // through as RGB noise if the backlight is already on during tft.begin().
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, LOW);

    tft.begin();
    tft.invertDisplay(1);
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    digitalWrite(LCD_BL, HIGH);

    setupWiFiAndEspNow();

#if defined(BALL_OWNER) && BALL_OWNER
    Serial.println("Starting as ball owner.");
    spawnBall(SCREEN_SIZE / 2.0f, SCREEN_SIZE / 2.0f, BALL_SPEED, BALL_SPEED * 0.6f, TFT_CYAN);
#else
    Serial.println("Waiting for the ball to arrive from the other screen...");
#endif

    lastFrameMs = millis();
}

static void handleIncomingBall() {
    if (!incomingPending) {
        return;
    }
    BallPacket packet;
    memcpy(&packet, (const void *)&incomingPacket, sizeof(packet));
    incomingPending = false;

    Serial.printf("Ball arrived: entry=(%.0f,%.0f) v=(%.0f,%.0f)\n",
                   packet.x, packet.y, packet.vx, packet.vy);
    spawnBall(packet.x, packet.y, packet.vx, packet.vy, packet.color);
}

static void stepBall(float dt) {
    if (!ballActive) {
        return;
    }

    prevX = ballX;
    prevY = ballY;
    hasPrevPos = true;

    ballX += ballVX * dt;
    ballY += ballVY * dt;

    // Bounce off top/bottom in place — only left/right edges hand off.
    if (ballY - BALL_RADIUS < 0) {
        ballY = BALL_RADIUS;
        ballVY = -ballVY;
    } else if (ballY + BALL_RADIUS > SCREEN_SIZE) {
        ballY = SCREEN_SIZE - BALL_RADIUS;
        ballVY = -ballVY;
    }

    bool exitingRight = ballVX > 0 && ballX + BALL_RADIUS >= SCREEN_SIZE;
    bool exitingLeft = ballVX < 0 && ballX - BALL_RADIUS <= 0;

    if (exitingRight || exitingLeft) {
        // Enter the other screen from the matching edge, same y/velocity.
        float entryX = exitingRight ? BALL_RADIUS : (SCREEN_SIZE - BALL_RADIUS);
        Serial.printf("Ball leaving this screen (%s edge) -> handing off\n",
                       exitingRight ? "right" : "left");
        sendBallToPeer(entryX, ballY, ballVX, ballVY, ballColor);
        tft.fillScreen(TFT_BLACK);
        ballActive = false;
        hasPrevPos = false;
        return;
    }
}

static void renderBall() {
    if (!ballActive) {
        return;
    }
    if (hasPrevPos) {
        tft.fillCircle((int)prevX, (int)prevY, BALL_RADIUS, TFT_BLACK);
    }
    tft.fillCircle((int)ballX, (int)ballY, BALL_RADIUS, ballColor);
}

void loop() {
    OtaSupport::handle();
    handleIncomingBall();

    unsigned long now = millis();
    float dt = (now - lastFrameMs) / 1000.0f;
    lastFrameMs = now;
    if (dt > 0.1f) {
        dt = 0.1f; // clamp after any stall (e.g. serial/monitor pause)
    }

    stepBall(dt);
    renderBall();

    delay(16); // ~60 FPS cap
}

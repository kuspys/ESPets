// ESPets "pet:3" demo: a boot-time logo splash, then the cat mascot cycling
// through four moods (idle/happy/sleeping/walking) with blinking, a gentle
// bob/sway, and floating heart/"Z" overlays — pulled from the project's
// Claude Design canvas ("Animated Cat Face.dc.html") and translated from a
// live CSS/React animation into a small set of pre-rasterized sprite frames
// plus cheap runtime motion (position offsets, not per-frame re-rendering).
//
// Sprites use true-black (#000000) ink and standard RGB565 packing — see
// src/screen-test/screen-test.cpp for why (tft.setSwapBytes(true) is
// required; a near-black gray like #1A1A1A tints pink/violet on this panel).

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "../common/ota.h"
#include "frame_idle_open.h"
#include "frame_idle_blink.h"
#include "frame_happy.h"
#include "frame_sleep.h"
#include "frame_logo.h"

TFT_eSPI tft = TFT_eSPI(240, 240);

#define LCD_BL 20
static const int SCREEN_SIZE = 240;

// #FF2E93 (brand pink), standard RGB565 packing.
static const uint16_t HEART_PINK = 0xF972;

enum Mode { MODE_IDLE, MODE_HAPPY, MODE_SLEEP, MODE_WALK };
static const unsigned long MODE_DURATION_MS[] = {6000, 5000, 6000, 5000};
static Mode currentMode = MODE_IDLE;
static unsigned long modeStartMs = 0;

static unsigned long lastBlinkAt = 0;
static unsigned long blinkUntil = 0;
static const unsigned long BLINK_INTERVAL_MS = 3200;
static const unsigned long BLINK_DURATION_MS = 180;

static int prevDrawX = -1000, prevDrawY = -1000, prevDrawW = 0, prevDrawH = 0;

// Small overlay animation state (hearts for HAPPY, "Z"s for SLEEP), drawn in
// the narrow black margin above the sprite's bob range.
static const int NUM_OVERLAY = 2;

static void showLogo() {
  tft.fillScreen(TFT_BLACK);
  int x = (SCREEN_SIZE - LOGO_WIDTH) / 2;
  int y = (SCREEN_SIZE - LOGO_HEIGHT) / 2;
  tft.pushImage(x, y, LOGO_WIDTH, LOGO_HEIGHT, LOGO_SPRITE);
}

static void clearOverlayArea() {
  // Top margin band, above the sprite's max bob range.
  tft.fillRect(0, 0, SCREEN_SIZE, 24, TFT_BLACK);
}

static void drawHeart(int cx, int cy, int size) {
  int r = size / 2;
  tft.fillCircle(cx - r / 2, cy, r / 2, HEART_PINK);
  tft.fillCircle(cx + r / 2, cy, r / 2, HEART_PINK);
  tft.fillTriangle(cx - r, cy, cx + r, cy, cx, cy + r, HEART_PINK);
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
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    for (int i = 0; i < NUM_OVERLAY; i++) {
      float phase = fmod((now / 1600.0) + i * 0.5, 1.0);
      int y = 20 - (int)(phase * 16);
      int x = 170 + i * 12;
      tft.setCursor(x, y);
      tft.print("Z");
    }
  }
}

static void advanceMode(unsigned long now) {
  if (now - modeStartMs >= MODE_DURATION_MS[currentMode]) {
    currentMode = (Mode)((currentMode + 1) % 4);
    modeStartMs = now;
    tft.fillScreen(TFT_BLACK);
    prevDrawW = 0; // force a full redraw of the new mode's sprite
  }
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

  int baseX = (SCREEN_SIZE - w) / 2;
  int baseY = (SCREEN_SIZE - h) / 2;
  int x = baseX + offsetX;
  int y = baseY + offsetY;

  if (prevDrawW > 0 && (prevDrawX != x || prevDrawY != y)) {
    tft.fillRect(prevDrawX, prevDrawY, prevDrawW, prevDrawH, TFT_BLACK);
  }
  tft.pushImage(x, y, w, h, sprite);
  prevDrawX = x;
  prevDrawY = y;
  prevDrawW = w;
  prevDrawH = h;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- pet:3 Demo ---");

  OtaSupport::begin();

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  tft.begin();
  tft.invertDisplay(1);
  tft.setRotation(0);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  Serial.println("Showing logo...");
  showLogo();
  delay(2500);

  tft.fillScreen(TFT_BLACK);
  modeStartMs = millis();
  Serial.println("Starting cat demo (idle -> happy -> sleep -> walk -> loop)");
}

void loop() {
  OtaSupport::handle();

  unsigned long now = millis();
  advanceMode(now);
  clearOverlayArea();
  drawOverlays(now);
  drawFace(now);

  delay(66); // ~15 FPS
}

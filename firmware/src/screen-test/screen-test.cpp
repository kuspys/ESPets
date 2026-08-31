#include "HardwareSerial.h"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "../common/ota.h"
#include "pet_sprite.h"

TFT_eSPI tft = TFT_eSPI(240, 240);

#define LCD_BL 20

// Center the sprite on the 240x240 panel.
static const int PET_SPRITE_X = (240 - PET_SPRITE_WIDTH) / 2;
static const int PET_SPRITE_Y = (240 - PET_SPRITE_HEIGHT) / 2;

void setup() {
  Serial.begin(115200);
  Serial.println("Screen test starting...");

  OtaSupport::begin();

  // Backlight on
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // Init TFT (TFT_eSPI handles SPI.begin internally)
  tft.begin();

  tft.invertDisplay(1);
  tft.setRotation(0);
  // pushImage() sends our raw uint16_t array's bytes in native (little-endian)
  // order by default, but the panel wants MSB-first per pixel — without this,
  // custom sprites come out with scrambled/wrong colors even though
  // fillScreen()'s built-in named colors (TFT_RED etc.) look fine, since those
  // go through a different internal path. Confirmed via on-device calibration.
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  Serial.println("TFT initialized.");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Screen Test OK!");
  delay(1000);
}

void loop() {
  OtaSupport::handle();
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextSize(3);
  tft.setCursor(20, 100);
  tft.println("RED");
  Serial.println("RED");
  delay(2000);

  OtaSupport::handle();
  tft.fillScreen(TFT_GREEN);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.setCursor(20, 100);
  tft.println("GREEN");
  Serial.println("GREEN");
  delay(2000);

  OtaSupport::handle();
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setCursor(20, 100);
  tft.println("BLUE");
  Serial.println("BLUE");
  delay(2000);

  OtaSupport::handle();
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(PET_SPRITE_X, PET_SPRITE_Y, PET_SPRITE_WIDTH, PET_SPRITE_HEIGHT, PET_SPRITE);
  Serial.println("PET");
  delay(3000);
}

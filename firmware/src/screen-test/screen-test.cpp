#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI(240, 240);

#define LCD_BL 20

void setup() {
  Serial.begin(115200);
  Serial.println("Screen test starting...");

  // Backlight on
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // Init TFT (TFT_eSPI handles SPI.begin internally)
  tft.begin();
  tft.invertDisplay(1);
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  Serial.println("TFT initialized.");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Screen Test OK!");
  delay(1000);
}

void loop() {
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextSize(3);
  tft.setCursor(20, 100);
  tft.println("RED");
  delay(2000);

  tft.fillScreen(TFT_GREEN);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.setCursor(20, 100);
  tft.println("GREEN");
  delay(2000);

  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setCursor(20, 100);
  tft.println("BLUE");
  delay(2000);
}

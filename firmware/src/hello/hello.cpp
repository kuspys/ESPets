#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C LCD = LiquidCrystal_I2C(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  Serial.println("Hello Module: Starting...");
  
  pinMode(2, OUTPUT);
  
  LCD.init();
  LCD.backlight();
  Serial.println("Hello Module: LCD Initialized");
}

void loop() {
  Serial.println("Hello Module: Blinking...");
  
  // Internal LED Pulse
  digitalWrite(2, HIGH);
  
  // LCD ON
  LCD.setCursor(0, 0);
  LCD.print("  Hello World!  ");
  LCD.setCursor(0, 1);
  LCD.print("****************");
  delay(1000);
  
  digitalWrite(2, LOW);
  
  // LCD OFF
  LCD.clear();
  delay(1000);
}

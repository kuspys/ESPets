#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  Serial.println("Sensor: Starting Blink Test...");
}

void loop() {
  Serial.println("Sensor: Blinking LED...");
  
  // Simple Blink
  digitalWrite(2, HIGH); 
  delay(500);
  digitalWrite(2, LOW);
  delay(500);
}

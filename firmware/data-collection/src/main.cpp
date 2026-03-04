#include <Arduino.h>
#include <methods.h>

void setup() {
  Serial.begin(115200); // Initialize serial port for debug mode
  initSensors();
}

void loop() {
  static unsigned long lastSaveMs = 0;
  const unsigned long saveIntervalMs = 5000;

  outputData();

  delay(500);
}

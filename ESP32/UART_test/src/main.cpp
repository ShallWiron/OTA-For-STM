#include <Arduino.h>

void setup() {
  // Khởi tạo UART0
  Serial.begin(115200);
}

void loop() {
  Serial.println("Du lieu gui tu ESP32 qua CP2102");
  delay(1000);
}
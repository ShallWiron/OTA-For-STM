#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "FS.h"
#include "Config.h"
#include "SD_Manager.h"
#include "Server_Manager.h"

void setup() {
  Serial.begin(115200);
  
  if (!initSDCard()) {
    Serial.println("Hệ thống dừng hoạt động do lỗi phần cứng.");
    while(true) delay(100); // Treo luôn hệ thống ở đây
  }
  //-----------------------------------------------

  initNetworkAndServer();
  //-----------------------------------------------
}

void loop() {
  handleServerClient();
}
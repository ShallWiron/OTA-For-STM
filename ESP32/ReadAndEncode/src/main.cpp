#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "OTA.h"

#define SD_CS_PIN 5
#define RX_PIN 16
#define TX_PIN 17
#define CHUNK_SIZE 256

STM32_OTA stm32Updater(Serial2);

void setup() {
    Serial.begin(115200); 
    Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); 
    delay(1000);
    
    // 1. Khởi tạo SD Card
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Loi: Khong nhan the SD.");
        return;
    }
    
    File firmwareFile = SD.open("/firmware.bin", FILE_READ);
    if (!firmwareFile) {
        Serial.println("Loi: Khong the mo file firmware.bin!");
        return;
    }

    // 2. Bắt đầu phiên OTA
    if (!stm32Updater.begin(5000)) {
        Serial.println("Loi: STM32 tu choi lenh CMD_START!");
        firmwareFile.close();
        return;
    }
    
    Serial.println("Bat dau truyen Data...");
    uint8_t buffer[CHUNK_SIZE];
    int totalBytesRead = 0;
    bool success = true;

    // 3. Đọc từ SD và nhồi vào OTA module
    while (firmwareFile.available()) {
        int bytesRead = firmwareFile.read(buffer, CHUNK_SIZE);
        
        if (!stm32Updater.sendChunk(buffer, bytesRead, 3)) {
            Serial.println("\n[NGUY HIEM] OTA THAT BAI: STM32 mat ket noi hoac tu choi lien tuc!");
            success = false;
            break;
        }

        totalBytesRead += bytesRead;
        Serial.printf("Tien do: Da gui %d bytes\n", totalBytesRead);
    }

    firmwareFile.close();

    // 4. Kết thúc phiên OTA
    if (success) {
        if (stm32Updater.end(1000)) {
            Serial.println("Thanh cong: STM32 da nhan file va reset.");
        } else {
            Serial.println("Loi: Khong nhan duoc ACK cho lenh ket thuc.");
        }
    }
}

void loop() {
    // Để trống
}
#include "OTA.h"

STM32_OTA::STM32_OTA(HardwareSerial& serialPort) {
    _serial = &serialPort;
}

uint16_t STM32_OTA::calculateCRC(const uint8_t *buf, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

void STM32_OTA::sendPacket(uint8_t command, const uint8_t *payload, uint16_t payload_len) {
    uint16_t total_packet_size = 8 + payload_len;
    uint8_t packet_buffer[total_packet_size];
    uint16_t index = 0;

    packet_buffer[index++] = OTA_START_BYTE_1;
    packet_buffer[index++] = OTA_START_BYTE_2;
    packet_buffer[index++] = command;
    packet_buffer[index++] = (payload_len >> 8) & 0xFF; 
    packet_buffer[index++] = payload_len & 0xFF;        

    for (uint16_t i = 0; i < payload_len; i++) {
        packet_buffer[index++] = payload[i];
    }

    uint16_t crc_value = calculateCRC(packet_buffer, index);

    packet_buffer[index++] = (crc_value >> 8) & 0xFF; 
    packet_buffer[index++] = crc_value & 0xFF;        
    packet_buffer[index++] = OTA_END_BYTE;

    _serial->write(packet_buffer, total_packet_size);
    _serial->flush(); 
}

uint8_t STM32_OTA::waitForResponse(uint32_t timeout_ms) {
    uint32_t start_time = millis();
    AckState_t state = ACK_WAIT_START1;
    uint8_t rx_buffer[6];
    
    while (millis() - start_time < timeout_ms) {
        while (_serial->available() > 0) {
            uint8_t c = _serial->read();
            
            switch (state) {
                case ACK_WAIT_START1:
                    if (c == 0xAA) { rx_buffer[0] = c; state = ACK_WAIT_START2; }
                    break;
                case ACK_WAIT_START2:
                    if (c == 0xBB) { rx_buffer[1] = c; state = ACK_WAIT_STATUS; } 
                    else state = ACK_WAIT_START1;
                    break;
                case ACK_WAIT_STATUS:
                    rx_buffer[2] = c; state = ACK_WAIT_LEN; break;
                case ACK_WAIT_LEN:
                    if (c == 0x06) { rx_buffer[3] = c; state = ACK_WAIT_CRC_H; } 
                    else state = ACK_WAIT_START1;
                    break;
                case ACK_WAIT_CRC_H:
                    rx_buffer[4] = c; state = ACK_WAIT_CRC_L; break;
                case ACK_WAIT_CRC_L:
                    rx_buffer[5] = c;
                    uint16_t calc_crc = calculateCRC(rx_buffer, 4);
                    uint16_t recv_crc = (rx_buffer[4] << 8) | rx_buffer[5];
                    
                    if (calc_crc != recv_crc) return OTA_NACK;
                    return rx_buffer[2] == OTA_ACK ? OTA_ACK : OTA_NACK;
            }
        }
    }
    return OTA_TIMEOUT;
}

bool STM32_OTA::begin(uint32_t timeoutMs) {
    Serial.println("Bat dau tien trinh OTA. Dang xoa Flash STM32...");
    sendPacket(CMD_START, nullptr, 0);
    return waitForResponse(timeoutMs) == OTA_ACK;
}

bool STM32_OTA::sendChunk(const uint8_t* data, uint16_t length, uint8_t maxRetries) {
    for (uint8_t retry = 0; retry < maxRetries; retry++) {
        if (retry > 0) Serial.printf("Thu lai lan %d...\n", retry);
        
        sendPacket(CMD_DATA, data, length);
        uint8_t response = waitForResponse(1000);

        if (response == OTA_ACK) return true;
        
        Serial.printf("Canh bao: Loi NACK hoac Timeout! (Lan %d)\n", retry + 1);
        delay(50);
    }
    return false;
}

bool STM32_OTA::end(uint32_t timeoutMs) {
    Serial.println("Gui lenh ket thuc (CMD_END)...");
    sendPacket(CMD_END, nullptr, 0);
    return waitForResponse(timeoutMs) == OTA_ACK;
}
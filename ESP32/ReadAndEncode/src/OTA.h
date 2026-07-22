#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

// Định nghĩa giao thức
#define OTA_START_BYTE_1 0xAA
#define OTA_START_BYTE_2 0xBB
#define OTA_END_BYTE     0xED

// Mã trạng thái
#define OTA_ACK          0x00
#define OTA_NACK         0x01
#define OTA_TIMEOUT      0xFF

// Mã lệnh
#define CMD_START        0x01
#define CMD_DATA         0x02
#define CMD_END          0x03

class STM32_OTA {
public:
    STM32_OTA(HardwareSerial& serialPort);
    
    // Khởi tạo luồng OTA (gửi lệnh xóa Flash)
    bool begin(uint32_t timeoutMs = 5000);
    
    // Gửi từng phần dữ liệu, độc lập hoàn toàn với cách đọc file
    bool sendChunk(const uint8_t* data, uint16_t length, uint8_t maxRetries = 3);
    
    // Kết thúc OTA (gửi lệnh reset)
    bool end(uint32_t timeoutMs = 1000);

private:
    HardwareSerial* _serial;

    typedef enum {
        ACK_WAIT_START1,
        ACK_WAIT_START2,
        ACK_WAIT_STATUS,
        ACK_WAIT_LEN,
        ACK_WAIT_CRC_H,
        ACK_WAIT_CRC_L
    } AckState_t;

    uint16_t calculateCRC(const uint8_t *buf, uint16_t len);
    uint8_t waitForResponse(uint32_t timeout_ms);
    void sendPacket(uint8_t command, const uint8_t *payload, uint16_t payload_len);
};

#endif
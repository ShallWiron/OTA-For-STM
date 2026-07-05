#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <stdint.h>

#define CS_PIN 5
#define CHUNK_SIZE 256

#define START_BYTE_1 0xAA
#define START_BYTE_2 0xBB
#define END_BYTE     0xED

#define OTA_ACK     0x00
#define OTA_NACK    0x01
#define OTA_TIMEOUT 0xFF // Mã 255 tự định nghĩa để báo lỗi quá giờ
#define OTA_RESPONSE_TIMEOUT_MS 1000 // Thời gian chờ phản hồi từ STM32 (1 giây)

#define RESP_HEADER_1 0xAA
#define RESP_HEADER_2 0xBB
#define RESP_LENGTH   0x06

typedef enum {
    ACK_WAIT_START1,
    ACK_WAIT_START2,
    ACK_WAIT_STATUS,
    ACK_WAIT_LEN,
    ACK_WAIT_CRC_H,
    ACK_WAIT_CRC_L
} AckState_t;

//Ham CRC check
uint16_t CRC16_Modbus(uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t pos = 0; pos < len; pos++)
    {
        crc ^= (uint16_t)buf[pos];

        for (uint8_t i = 0; i < 8; i++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

uint8_t wait_for_response(uint32_t timeout_ms) {
    uint32_t start_time = millis();
    AckState_t state = ACK_WAIT_START1;
    uint8_t rx_buffer[6];
    
    uint8_t debug_rx[300];
    uint16_t debug_rx_cnt = 0;
    uint8_t result = OTA_TIMEOUT;
    
    while (millis() - start_time < timeout_ms) {
        while (Serial2.available() > 0) {
            uint8_t c = Serial2.read();
            if (debug_rx_cnt < 300) {
                debug_rx[debug_rx_cnt++] = c;
            }
            
            switch (state) {
                case ACK_WAIT_START1:
                    if (c == RESP_HEADER_1) {
                        rx_buffer[0] = c;
                        state = ACK_WAIT_START2;
                    }
                    break;
                    
                case ACK_WAIT_START2:
                    if (c == RESP_HEADER_2) {
                        rx_buffer[1] = c;
                        state = ACK_WAIT_STATUS;
                    } else {
                        state = ACK_WAIT_START1; // Rác, reset
                    }
                    break;
                    
                case ACK_WAIT_STATUS:
                    rx_buffer[2] = c; // Byte 2 là Status (0x00 hoặc 0x01)
                    state = ACK_WAIT_LEN;
                    break;
                    
                case ACK_WAIT_LEN:
                    if (c == RESP_LENGTH) { // Byte 3 là Length (0x06)
                        rx_buffer[3] = c;
                        state = ACK_WAIT_CRC_H;
                    } else {
                        state = ACK_WAIT_START1; // Độ dài sai, reset frame
                    }
                    break;
                    
                case ACK_WAIT_CRC_H:
                    rx_buffer[4] = c;
                    state = ACK_WAIT_CRC_L;
                    break;
                    
                case ACK_WAIT_CRC_L:
                    rx_buffer[5] = c;
                    
                    // Đã nhận đủ 6 byte, tiến hành kiểm tra CRC trên 4 byte đầu
                    uint16_t calc_crc = CRC16_Modbus(rx_buffer, 4);
                    uint16_t recv_crc = (rx_buffer[4] << 8) | rx_buffer[5];
                    
                    if (calc_crc != recv_crc) {
                        Serial.printf("Loi: CRC phan hoi sai! Calc: %04X, Recv: %04X\n", calc_crc, recv_crc);
                        result = OTA_NACK;
                        goto exit_wait;
                    }
                    
                    // CRC đúng, trả về Status thực tế
                    uint8_t status = rx_buffer[2];
                    if (status == OTA_ACK || status == OTA_NACK) {
                        result = status;
                        goto exit_wait;
                    }
                    result = OTA_NACK; // Trạng thái lạ
                    goto exit_wait;
            }
        }
    }
    
exit_wait:
    Serial.printf("STM32 -> ESP32 (%d bytes): ", debug_rx_cnt);
    for (uint8_t i = 0; i < debug_rx_cnt; i++) {
        Serial.printf("%02X ", debug_rx[i]);
    }
    Serial.println();
    return result;
}

void send_ota_packet(uint8_t command, const uint8_t *payload, uint16_t payload_len) {
    uint16_t total_packet_size = 8 + payload_len;
    uint8_t packet_buffer[total_packet_size];
    uint16_t index = 0;

    // --- ĐÓNG GÓI HEADER ---
    packet_buffer[index++] = START_BYTE_1;
    packet_buffer[index++] = START_BYTE_2;
    
    // Command (1 byte)
    packet_buffer[index++] = command;
    
    // Length (2 bytes)
    packet_buffer[index++] = (payload_len >> 8) & 0xFF; 
    packet_buffer[index++] = payload_len & 0xFF;        

    // --- ĐÓNG GÓI PAYLOAD ---
    for (uint16_t i = 0; i < payload_len; i++) {
        packet_buffer[index++] = payload[i];
    }

    // --- TÍNH TOÁN CRC ---
    // TÍNH TỪ ĐẦU MẢNG (Bao gồm cả 0xAA và 0xBB), biến index lúc này đang là tổng số byte cần tính
    uint16_t crc_value = CRC16_Modbus(packet_buffer, index);

    // --- ĐÓNG GÓI CRC VÀ END BYTE ---
    packet_buffer[index++] = (crc_value >> 8) & 0xFF; 
    packet_buffer[index++] = crc_value & 0xFF;        
    packet_buffer[index++] = END_BYTE;

    // --- PRINT DEBUG ---
    Serial.printf("ESP32 -> STM32 (%d bytes): ", total_packet_size);
    for (uint16_t i = 0; i < total_packet_size; i++) {
        Serial.printf("%02X ", packet_buffer[i]);
    }
    Serial.println();

    // --- TRUYỀN RA UART ---
    Serial2.write(packet_buffer, total_packet_size);
    Serial2.flush(); 
}

void setup() {
  // Tốc độ này để in ra màn hình CP2102 xem debug
  Serial.begin(115200); 
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17
  delay(1000);
  
  Serial.println("Khoi tao the SD...");

  // Khởi tạo SPI và kiểm tra thẻ SD
  if (!SD.begin(CS_PIN)) {
    Serial.println("Loi: Khong nhan the SD. Kiem tra lai day SPI hoac the chua cam chat!");
    return;
  }
  Serial.println("The SD san sang.");

  // Mở file dưới dạng ĐỌC
  File firmwareFile = SD.open("/firmware.bin", FILE_READ);
  if (!firmwareFile) {
    Serial.println("Loi: Khong the mo file firmware.bin!");
    return;
  }
  Serial.println("Da mo file. Bat dau doc va truyen theo khoi...");

  //START
  send_ota_packet(0x01, NULL, 0); // Gửi lệnh bắt đầu OTA
  if (wait_for_response(5000) != OTA_ACK) {
      Serial.println("Loi: STM32 khong phan hoi lenh xoa Flash!");
      firmwareFile.close();
      return;
  }
  Serial.println("Xoa Flash thanh cong. Bat dau truyen Data...");

  // Tạo bộ đệm chứa dữ liệu
  uint8_t buffer[CHUNK_SIZE];
  int totalBytesRead = 0;

  // Vòng lặp đọc cho đến khi hết file
  while (firmwareFile.available()) {
    int bytesRead = firmwareFile.read(buffer, CHUNK_SIZE);
    
    bool packet_success = false;
    const uint8_t MAX_RETRIES = 3;

    // Vòng lặp kiểm soát việc gửi lại tối đa 3 lần nếu có lỗi
    for (uint8_t retry = 0; retry < MAX_RETRIES; retry++) {
      if (retry > 0) {
        Serial.printf("Trang thai: Thong tin loi, dang thu lai lan %d...\n", retry);
      }

      // Gửi gói tin dữ liệu
      send_ota_packet(0x02, buffer, bytesRead);
      
      // Chờ phản hồi từ STM32, thời gian timeout đặt là 1000ms
      uint8_t response = wait_for_response(1000);

      if (response == OTA_ACK) {
        packet_success = true;
        break; // Gửi thành công, thoát khỏi vòng lặp retry để sang khối tiếp theo
      } 
      else if (response == OTA_NACK) {
        Serial.printf("Canh bao: STM32 bao loi NACK o khoi du lieu! (Lan thu %d)\n", retry + 1);
      } 
      else if (response == OTA_TIMEOUT) {
        Serial.printf("Canh bao: Qua thoi gian cho phan hoi tu STM32! (Lan thu %d)\n", retry + 1);
      }

      // Delay một khoảng nhỏ trước khi gửi lại để STM32 có thời gian reset bộ đệm UART
      delay(50); 
    }

    // Nếu đã thử lại 3 lần liên tiếp mà vẫn thất bại thì dừng toàn bộ quá trình OTA
    if (!packet_success) {
      Serial.println("\n[NGUY HIEM] QUÁ TRÌNH OTA THẤT BẠI: Mat ket noi hoac STM32 loi lien tuc quá 3 lan!");
      firmwareFile.close();
      return; // Thoát hàm setup, không chạy tiếp nữa
    }

    totalBytesRead += bytesRead;
    Serial.printf("Thanh cong: Da xac nhan %d bytes. Tong: %d bytes\n", bytesRead, totalBytesRead);
  }

  Serial.println("Truyen file xong. Gui lenh ket thuc (0x03)...");
  
  send_ota_packet(0x03, NULL, 0);
  if (wait_for_response(1000) == OTA_ACK) {
      Serial.println("\nChúc mung! Da truyen thanh cong va STM32 da reset.");
  } else {
      Serial.println("\nLoi: STM32 khong phan hoi lenh ket thuc.");
  }

  firmwareFile.close();
}

void loop() {
  // Hệ thống nhúng thì loop để trống hoặc cho delay
}
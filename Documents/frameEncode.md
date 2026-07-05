# Cấu trúc Frame truyền
- Một gói tin hoàn chỉnh nên bao gồm 6 thành phần, đi theo đúng thứ tự này:

## 1. START (2 bytes - Ký tự đồng bộ): 0xAABB

- Dùng để báo hiệu "Chuẩn bị nhận dữ liệu!". Nên dùng 2 byte cố định không dễ bị nhầm lẫn với dữ liệu thường, ví dụ: 0xAA và 0x55.

- Lý do: Khi STM32 đang chờ, nó cứ liên tục đọc UART. Thấy đúng chuỗi 0xAA 0x55 thì nó mới bắt đầu mở luồng lưu các byte tiếp theo vào buffer.

## 2. COMMAND (1 byte - Lệnh điều khiển):

| Mã   | Ý nghĩa                                   |
| ---- | ----------------------------------------- |
| 0x01 | Xóa FLASH, bắt đầu truyền DATA            |
| 0x02 | Chứa DATA firmware, đọc rồi ghi vào FLASH |
| 0x03 | Hoàn thành truyền DATA, nhảy qua APP mới  |

- Bên nhận cần biết gói tin này chứa cái gì để còn xử lý.

- Ví dụ: 0x01 (Bắt đầu OTA, chuẩn bị xóa Flash), 0x02 (Gói tin chứa dữ liệu firmware), 0x03 (Kết thúc truyền, yêu cầu reset chip).

## 3. LENGTH (2 bytes - Độ dài Payload):

- Độ dài của khúc dữ liệu .bin thực tế đính kèm trong gói này. Nếu cậu đọc từ thẻ SD mỗi lần 256 bytes, thì Length ở đây là 256 (0x0100).

## 4. PAYLOAD (N bytes - Dữ liệu thực):

- Chính là cái mảng buffer[CHUNK_SIZE] mà cậu vừa đọc ra từ thẻ SD đấy.

## 5. CRC (2 bytes - Kiểm tra toàn vẹn):

- Mã kiểm tra lỗi cho toàn bộ từ phần COMMAND đến hết PAYLOAD. Thường dùng thuật toán CRC-16 (nhẹ, tính nhanh, cực kỳ phù hợp cho vi điều khiển như STM32).

## 6. END (1 byte - Ký tự kết thúc): 0xED

- Chốt hạ gói tin, ví dụ dùng 0xED (End Data). Giúp STM32 xác nhận lại một lần nữa là gói tin không bị đứt gánh giữa chừng.

# Struct
```c
// Tổng kích thước frame = 2 (Start) + 1 (Cmd) + 2 (Len) + 256 (Payload) + 2 (CRC) + 1 (End) = 264 bytes
struct OtaPacket {
    uint8_t start_1 = 0xAA;
    uint8_t start_2 = 0xBB;
    uint8_t command;
    uint16_t length;
    uint8_t payload[256];
    uint16_t crc16;
    uint8_t end_byte = 0xED;
};
```
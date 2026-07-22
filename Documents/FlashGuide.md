# Hướng Dẫn Thực Hành Đọc, Xóa và Ghi Flash Trên STM32 (Hệ Điều Hành Linux)

Tài liệu này hướng dẫn chi tiết các bước từ cơ bản đến nâng cao để thao tác với bộ nhớ FLASH của vi điều khiển STM32F103 trên môi trường Linux.

---

## 1. Các Nguyên Tắc Vàng Khi Làm Việc Với FLASH
1. **Unlock & Lock:** Bộ nhớ FLASH luôn được khóa mặc định để bảo vệ code. Trước khi ghi/xóa phải mở khóa (`FLASH_Unlock()`) và sau khi xong phải khóa lại (`FLASH_Lock()`).
2. **Xóa trước khi ghi:** FLASH chỉ có thể chuyển bit từ `1` thành `0`. Muốn đảo ngược lại để ghi đè dữ liệu mới, bạn **bắt buộc phải xóa trang chứa ô nhớ đó** để đưa toàn bộ bit về `1` (tương ứng giá trị `0xFFFFFFFF`).
3. **Kích thước trang xóa:** Đối với chip STM32F103C8T6 (và bản 128KB), kích thước mỗi trang là **1 KB** (`1024 bytes` hoặc `0x400`). Khi ra lệnh xóa, toàn bộ 1 KB tại trang đó sẽ bị làm sạch.
4. **Đọc trực tiếp:** Bộ nhớ FLASH được ánh xạ trực tiếp vào bản đồ bộ nhớ bắt đầu từ `0x08000000`, bạn có thể dùng con trỏ C đọc thẳng mà không cần unlock.

---

## 2. Cách 1: Quan Sát Kết Quả Ghi Flash Nhanh (Không Cần Debug Dòng Lệnh)

Đây là cách dễ nhất dành cho người mới bắt đầu. Chúng ta sẽ nạp code cho chip tự chạy, sau đó dùng công cụ `st-flash` để xuất ngược bộ nhớ ra một file trên máy tính và mở xem nội dung mã Hex.

### Các bước thực hiện:

#### Bước 1: Di chuyển vào thư mục dự án và Biên dịch
Mở Terminal của Linux và gõ:
```bash
cd STM32/Boot\ copy/
make clean
make
```

#### Bước 2: Nạp code xuống chip
Kết nối ST-Link với máy tính và STM32, sau đó chạy lệnh nạp:
```bash
st-flash write build/template_make.bin 0x08000000
```
*(Ngay sau khi nạp xong, chip sẽ tự khởi động và chạy hàm `Practice_Flash_Memory()`, thực hiện xóa trang và ghi giá trị `0x55AA55AA` vào địa chỉ `0x0801F000`).*

#### Bước 3: Xuất dữ liệu bộ nhớ FLASH ra file
Chạy lệnh đọc bộ nhớ từ địa chỉ ghi (`0x0801F000`) với dung lượng 1 KB (`0x400`) lưu vào file `ketqua.bin`:
```bash
st-flash read ketqua.bin 0x0801F000 0x400
```

#### Bước 4: Xem kết quả dữ liệu mã Hex
Chạy lệnh sau trên Linux để kiểm tra nội dung file nhị phân vừa đọc được:
```bash
xxd ketqua.bin | head -n 5
```
*Kết quả hiển thị mong đợi:*
```text
00000000: aa55 aa55 ffff ffff ffff ffff ffff ffff  .U.U............
00000010: ffff ffff ffff ffff ffff ffff ffff ffff  ................
```
* **`aa55 aa55`** chính là số `0x55AA55AA` (được hiển thị ngược byte do kiến trúc Little Endian).
* **`ffff ffff`** là phần còn lại của trang 1 KB đã được xóa sạch.

---

## 3. Cách 2: Debug Từng Dòng Code Bằng GDB (Theo Dõi Thời Gian Thực)

Cách này cho phép bạn làm "đóng băng" thời gian của chip, chạy từng dòng lệnh xóa/ghi và xem trực tiếp sự thay đổi trong GDB.

### Các bước thực hiện:

#### Bước 1: Khởi động OpenOCD (Terminal số 1)
Mở Terminal 1 và gõ lệnh để tạo cầu nối debug:
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg
```
*(Giữ nguyên cửa sổ này chạy ngầm).*

#### Bước 2: Khởi động GDB và kết nối (Terminal số 2)
Mở Terminal 2, di chuyển vào thư mục dự án và chạy:
```bash
cd STM32/Boot\ copy/
gdb-multiarch build/template_make.elf
```
Tại dấu nhắc `(gdb)`, gõ các lệnh sau để kết nối và nạp code mới:
```gdb
(gdb) target remote :3333
(gdb) monitor halt
(gdb) load
```

#### Bước 3: Đặt các điểm giám sát bộ nhớ
Chúng ta sẽ đặt điểm dừng ngay tại hàm ghi FLASH và bắt GDB theo dõi 3 biến trạng thái:
```gdb
(gdb) break Practice_Flash_Memory
(gdb) display before_erase_val
(gdb) display after_erase_val
(gdb) display after_write_val
```

#### Bước 4: Chạy từng dòng lệnh (Trace)
1. **Chạy tới hàm test:**
   ```gdb
   (gdb) continue
   ```
   *Chương trình sẽ dừng ngay đầu hàm `Practice_Flash_Memory()`.*
   
2. **Chạy từng dòng bằng lệnh `next` (hoặc phím `n`):**
   * Nhấn `n` lần 1: Chạy qua dòng đọc trước khi thao tác $\rightarrow$ Biến `before_erase_val` sẽ cập nhật giá trị đang có trên chip.
   * Nhấn `n` tiếp tục cho tới khi chạy qua hàm xóa trang `FLASH_ErasePage()` và gán giá trị sau xóa $\rightarrow$ Biến `after_erase_val` sẽ nhận giá trị `4294967295` (tương đương `0xFFFFFFFF` dạng hex).
   * Nhấn `n` tiếp tục cho tới khi chạy qua hàm ghi `FLASH_ProgramWord()` và gán giá trị sau ghi $\rightarrow$ Biến `after_write_val` sẽ nhận đúng giá trị `1437226410` (tương đương `0x55AA55AA` dạng hex).

3. **Xem trực tiếp giá trị của ô nhớ trên phần cứng:**
   ```gdb
   (gdb) x/1xw 0x0801F000
   ```
   *Kết quả trả về:* `0x0801f000: 0x55aa55aa`

---

## 4. Các Hàm Ghi/Đọc Dùng Trong Mã Nguồn C (Thư Viện SPL)

Khi phát triển Bootloader thực tế, bạn có thể tham khảo các đoạn mã mẫu sau để viết ứng dụng nạp OTA:

### 1. Đọc dữ liệu từ FLASH bằng con trỏ:
```c
uint32_t data32 = *(volatile uint32_t*)0x0801F000;
```

### 2. Ghi một mảng Byte (ví dụ 256 bytes firmware nhận qua UART):
```c
uint8_t Flash_Write_Buffer(uint32_t start_address, uint8_t *buffer, uint32_t length_in_bytes)
{
    FLASH_Status status = FLASH_COMPLETE;
    
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    // Ghi FLASH theo từng Half-Word (16-bit / 2 bytes)
    for (uint32_t i = 0; i < length_in_bytes; i += 2)
    {
        // Ghép 2 byte uint8_t thành 1 số uint16_t (Little Endian)
        uint16_t data16 = buffer[i] | (buffer[i + 1] << 8);
        
        status = FLASH_ProgramHalfWord(start_address + i, data16);
        
        if (status != FLASH_COMPLETE) {
            break; // Gặp lỗi dừng ghi ngay
        }
    }
    
    FLASH_Lock();
    return (status == FLASH_COMPLETE) ? 1 : 0;
}
```
*(Lưu ý: Trước khi gọi hàm ghi mảng này, địa chỉ cần ghi phải được xóa bằng hàm `FLASH_ErasePage(addr)` từ trước).*

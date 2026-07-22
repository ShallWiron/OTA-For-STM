# Hướng Dẫn Phân Vùng Flash và Cấu Hình Dự Án OTA (STM32 - 128KB)

Tài liệu này hướng dẫn chi tiết các bước thiết kế phân vùng bộ nhớ Flash (Memory Partitioning), cấu hình mã nguồn cho dự án đa ứng dụng gồm 3 phần vùng (Bootloader, App1, App2), và cơ chế quản lý khởi động/khôi phục tương ứng.

---

## 1. Thiết Kế Sơ Đồ Phân Vùng Flash (3 Phân Vùng)

Tổng dung lượng bộ nhớ Flash vật lý là **128 KB** (128 Pages, mỗi Page 1 KB, địa chỉ từ `0x0800 0000` đến `0x0801 FFFF`). Bộ nhớ được chia thành 3 phần vùng chính như sau:

| Phân vùng | Địa chỉ bắt đầu (Origin) | Địa chỉ kết thúc | Kích thước | Số Pages (1KB) |
| :--- | :--- | :--- | :---: | :---: |
| **Bootloader** | `0x0800 0000` | `0x0800 3FFF` | 16 KB | 16 |
| **App1 (Active)** | `0x0800 4000` | `0x0801 1FFF` | 56 KB | 56 |
| **App2 (Update)** | `0x0801 2000` | `0x0801 FFFF` | 56 KB | 56 |

---

## 2. Hướng Dẫn Cấu Hình Dự Án

### Bước 1: Cấu hình Linker Script (`.ld`) của các Project

Bạn cần cập nhật cấu hình vùng nhớ trong file Linker Script (`.ld`) của từng dự án tương ứng để trình biên dịch định vị mã nguồn chính xác.

#### 1. Cấu hình file Linker Script của Bootloader:
```ld
MEMORY
{
  RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 20K
  FLASH (rx)     : ORIGIN = 0x08000000, LENGTH = 16K
}
```

#### 2. Cấu hình file Linker Script của App1:
```ld
MEMORY
{
  RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 20K
  FLASH (rx)     : ORIGIN = 0x08004000, LENGTH = 56K
}
```

#### 3. Cấu hình file Linker Script của App2:
```ld
MEMORY
{
  RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 20K
  FLASH (rx)     : ORIGIN = 0x08012000, LENGTH = 56K
}
```

---

### Bước 2: Dịch chuyển Vector Table (VTOR) trong App

Mỗi ứng dụng (App1 và App2) sau khi khởi động cần phải khai báo lại vùng Vector Table ngắt tương ứng với địa chỉ bắt đầu của nó.

Mở file `system_stm32f10x.c` trong từng dự án App và chỉnh sửa giá trị `#define VECT_TAB_OFFSET` như sau:

#### 1. Dành cho dự án App1:
```c
#define VECT_TAB_OFFSET  0x4000 /* Dịch chuyển 16 KB (0x4000) từ đầu FLASH_BASE */
```

#### 2. Dành cho dự án App2:
```c
#define VECT_TAB_OFFSET  0x12000 /* Dịch chuyển 72 KB (0x12000) từ đầu FLASH_BASE */
```

> [!NOTE]
> Dự án **Bootloader** giữ nguyên giá trị `#define VECT_TAB_OFFSET 0x0` để sử dụng Vector Table tại địa chỉ mặc định `0x08000000`.

---

### Bước 3: Thay đổi vị trí nạp code trong Makefile

#### 1. Boot
```
flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $< 0x08000000
```

#### 2. App1
```
flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $< 0x08004000
```

#### 3. App2
```
flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $< 0x08012000
```

---

### Bước 4: Lập trình hàm chuyển vùng chạy (Jump) trong Bootloader

Tại mã nguồn Bootloader, khi muốn nhảy sang App1 hoặc App2, ta sử dụng hàm sau:

```c
typedef void (*pFunction)(void);

void Jump_To_App(uint32_t app_address)
{
    // 1. Kiểm tra tính hợp lệ của Stack Pointer (MSP) nằm trong vùng RAM
    if (((*(__IO uint32_t*)app_address) & 0x2FFE0000 ) == 0x20000000)
    {
        // 2. Vô hiệu hóa ngắt toàn cục
        __disable_irq();
        
        // 3. Tắt và reset SysTick Timer
        SysTick->CTRL = 0;
        SysTick->VAL = 0;
        
        // 4. Tắt toàn bộ ngắt ngoại vi trong NVIC
        for (uint8_t i = 0; i < 8; i++) {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }
        
        // 5. Cài đặt lại Main Stack Pointer (MSP)
        __set_MSP(*(__IO uint32_t*)app_address);
        
        // 6. Lấy địa chỉ của Reset Handler và thực hiện cú nhảy (Jump)
        uint32_t JumpAddress = *(__IO uint32_t*) (app_address + 4);
        pFunction Jump = (pFunction) JumpAddress;
        Jump();
    }
}
```

---

## 3. Cơ Chế Kiểm Tra và Tự Động Rollback với 3 Phân Vùng

Do không có phân vùng cấu hình riêng (phân vùng thứ 4), bạn có thể lựa chọn một trong hai giải pháp sau để lưu trữ trạng thái chạy/kiểm tra rollback:

### Giải pháp A: Lưu trạng thái trên Option Bytes của STM32 (Khuyên dùng)
STM32F103 có vùng nhớ không bay màu đặc biệt gọi là **Option Bytes**, trong đó có hai thanh ghi dữ liệu người dùng (`Data0` và `Data1`) hoạt động độc lập với bộ nhớ chương trình Flash chính và không bị mất đi khi Reset.

* **Cách dùng:**
  * Sử dụng byte dữ liệu `Data0` để ghi nhận App nào đang chạy kích hoạt (`0x01` đại diện App1, `0x02` đại diện App2).
  * Sử dụng byte dữ liệu `Data1` để đánh dấu trạng thái hoạt động (`0x00` = Hoạt động tốt/Stable, `0x01` = Đang chạy thử nghiệm/Testing).

#### Logic xử lý Rollback tại Bootloader (Giải pháp A):
```c
#include "stm32f10x_flash.h"

#define OB_DATA0_ADDR   0x1FFFF804 // Địa chỉ User Data 0 của Option Bytes
#define OB_DATA1_ADDR   0x1FFFF806 // Địa chỉ User Data 1 của Option Bytes

void Check_And_Boot(void)
{
    // Đọc trạng thái từ Option Bytes
    uint8_t active_app = OB->USER & 0xFF; // Hoặc đọc trực tiếp từ địa chỉ thanh ghi OB_DATA0
    uint8_t app_status = (OB->USER >> 8) & 0xFF; // Đọc OB_DATA1
    
    // Khởi tạo mặc định nếu là lần đầu chạy (chưa ghi đè)
    if (active_app != 1 && active_app != 2) {
        FLASH_Unlock();
        FLASH_OB_Unlock();
        FLASH_ProgramOptionByteData(OB_DATA0_ADDR, 1); // Set mặc định chạy App1
        FLASH_ProgramOptionByteData(OB_DATA1_ADDR, 0); // Set mặc định trạng thái OK
        FLASH_OB_Launch(); // Thực thi cập nhật Option Bytes
        FLASH_Lock();
        active_app = 1;
        app_status = 0;
    }
    
    // --- KHÔI PHỤC (ROLLBACK) ---
    if (app_status == 1) { // Ứng dụng mới khởi động thất bại hoặc bị crash reset
        active_app = (active_app == 1) ? 2 : 1; // Đảo ngược sang App cũ ổn định
        app_status = 0; // Đặt lại trạng thái chạy ổn định
        
        FLASH_Unlock();
        FLASH_OB_Unlock();
        FLASH_ProgramOptionByteData(OB_DATA0_ADDR, active_app);
        FLASH_ProgramOptionByteData(OB_DATA1_ADDR, app_status);
        FLASH_OB_Launch();
        FLASH_Lock();
        
        printf("Phat hien crash khi boot! Rollback thanh cong ve App %d\n", active_app);
    }
    
    // Thực hiện nhảy sang vùng App tương ứng
    uint32_t run_address = (active_app == 1) ? 0x08004000 : 0x08012000;
    Jump_To_App(run_address);
}
```

#### Xác nhận App chạy thành công (Gọi trong Main của App):
```c
// Hàm này được gọi trong App sau khi khởi tạo thành công và chạy ổn định khoảng 5-10 giây
void Verify_App_Success(uint8_t this_app_id)
{
    uint8_t active_app = OB->USER & 0xFF;
    uint8_t app_status = (OB->USER >> 8) & 0xFF;
    
    if (active_app == this_app_id && app_status == 1) // Trạng thái đang TESTING
    {
        FLASH_Unlock();
        FLASH_OB_Unlock();
        FLASH_ProgramOptionByteData(OB_DATA1_ADDR, 0); // Đổi trạng thái thành OK (0)
        FLASH_OB_Launch();
        FLASH_Lock();
        printf("App %d da khoi dong va bat tay thanh cong!\n", this_app_id);
    }
}
```

---

### Giải pháp B: Điều phối trạng thái thông qua ESP32
Vì ESP32 có bộ nhớ lớn và có thẻ SD, bạn có thể lưu trạng thái cập nhật trên ESP32:
1. Khi ESP32 cập nhật xong firmware vào phân vùng mới của STM32, nó lưu trạng thái `active_app` của STM32 vào bộ nhớ NVS/EEPROM của ESP32.
2. Khi STM32 khởi động, Bootloader sẽ gửi một tin nhắn chào hỏi qua UART hỏi ESP32 nên boot vùng nào.
3. ESP32 sẽ phản hồi và STM32 nhảy vào vùng tương ứng.
4. Nếu STM32 bị crash khi boot hoặc Watchdog Reset xảy ra $\rightarrow$ Bootloader của STM32 khởi động lại và hỏi lại. ESP32 nhận thấy STM32 bị reset bất thường trong thời gian ngắn $\rightarrow$ ESP32 sẽ tự cập nhật bộ nhớ của mình và ra lệnh cho STM32 nhảy về phân vùng cũ.

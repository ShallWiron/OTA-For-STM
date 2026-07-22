#include "stm32f10x.h"
#include "stm32f10x_flash.h"

#define APP1_ADDR 0x08004000  // Example address in flash memory
#define TEST_DATA 0x12345678  // Example data to write
// Biến toàn cục lưu địa chỉ ghi tiếp theo
uint32_t current_write_address = APP1_ADDR; 
uint8_t dataFromESP[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}; // Example data received from ESP

// Ví dụ: Xóa toàn bộ phân vùng App1 bắt đầu từ 0x08004000 (kích thước 56 KB)
void Erase_Application_Partition(uint32_t start_address, uint32_t size_in_kb)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    for (uint32_t i = 0; i < size_in_kb; i++)
    {
        // Mỗi trang cách nhau 1 KB (1024 bytes hay 0x400)
        uint32_t page_addr = start_address + (i * 1024);
        FLASH_ErasePage(page_addr);
    }

    FLASH_Lock();
}

void Process_Received_Chunk(uint8_t *chunk_buffer, uint32_t chunk_len)
{
    // Chỉ ghi trực tiếp, hoàn toàn không có lệnh Erase ở đây!
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    for (uint32_t i = 0; i < chunk_len; i += 2)
    {
        uint16_t data16 = chunk_buffer[i] | (chunk_buffer[i + 1] << 8);
        FLASH_ProgramHalfWord(current_write_address + i, data16);
    }
    
    FLASH_Lock();

    // Tăng địa chỉ ghi tiếp theo lên tương ứng với kích thước khối vừa nhận
    current_write_address += chunk_len; 
}

int main(void)
{
  /* Main program code */

  Erase_Application_Partition(0x08004000, 56); // Xóa phân vùng App1 (56 KB)
  Process_Received_Chunk(dataFromESP, sizeof(dataFromESP)); // Ghi dữ liệu nhận được từ ESP vào flash

  while (1)
  {
	// Your application code here
  }
}
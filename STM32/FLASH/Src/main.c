#include "stm32f10x.h"
#include "stm32f10x_flash.h"

#define APP1_ADDR 0x08004000

uint32_t current_write_address = APP1_ADDR; 
uint8_t buffer[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}; // Example data received from ESP

/*

Quy trình update App:
1. Xóa phân vùng App cần ghi.
2. Nhận firmware từ ESP32 theo từng khối và ghi vào FLASH tại current_write_address sau đó dịch current_write_address lên tương ứng với kích thước khối vừa nhận.
3. Tiếp tục cho đến hết.

*/

void Erase_Application_Partition(uint32_t start_address, uint32_t size_in_kb)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    for (uint32_t i = 0; i < size_in_kb; i++)
    {
        uint32_t page_addr = start_address + (i * 1024);
        FLASH_ErasePage(page_addr);
    }
    FLASH_Lock();    
}

void Process_Received_Chunk(uint8_t *chunk_buffer, uint32_t chunk_len)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    for (uint32_t i = 0; i < chunk_len; i += 2)
    {
        uint16_t data16 = chunk_buffer[i] | (chunk_buffer[i + 1] << 8);
        FLASH_ProgramHalfWord(current_write_address + i, data16);
    }
    FLASH_Lock();
    current_write_address += chunk_len; 
}

int main(void)
{
  Erase_Application_Partition(0x08004000, 56);
  Process_Received_Chunk(buffer, sizeof(buffer));

  while (1)
  {
    // Your application code here
  }
}
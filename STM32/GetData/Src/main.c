#include "stm32f10x.h"                  // Device header
#include "UART.h"
#include "delay.h"
#include "stm32f10x_flash.h"
#include <stdio.h>

#define APP1_ADDRESS 0x08004000
#define APP2_ADDRESS 0x08012000
#define APP1_SIZE 56 // KB
#define APP2_SIZE 56 // KB
#define MAX_LEN 300
#define MAX_PAYLOAD_SIZE 256

typedef enum {
    STATE_WAIT_START1,
    STATE_WAIT_START2,
    STATE_WAIT_CMD,
    STATE_WAIT_LEN_H,
    STATE_WAIT_LEN_L,
    STATE_WAIT_PAYLOAD,
    STATE_WAIT_CRC_H,
    STATE_WAIT_CRC_L,
    STATE_WAIT_END
} RxState_t;

RxState_t rx_state = STATE_WAIT_START1;
uint16_t payload_len = 0;
uint16_t payload_cnt = 0;

uint8_t buffer_input[MAX_LEN];
uint8_t buffer_output[6];
uint32_t COUNT = 0;
uint8_t FINISH = 0;
uint8_t temp_byte;
uint32_t current_write_address = APP1_ADDRESS; 

/*Cau truc buffer_input:
[0], [1]: 0xAA, 0xBB

[2]: COMMAND

[3], [4]: LENGTH

[5] đến [5 + payload_len - 1]: PAYLOAD

[5 + payload_len], [6 + payload_len]: CRC

[7 + payload_len]: 0xED
*/

typedef void (*pFunction)(void);

void Jump_To_App(uint32_t app_address)
{
    uint32_t msp_val = *(__IO uint32_t*)app_address;
    uint32_t reset_val = *(__IO uint32_t*)(app_address + 4);
    
    // Kiểm tra tính hợp lệ của Stack Pointer (MSP) và Reset Handler (phải thuộc vùng nhớ Flash)
    if (((msp_val & 0x2FFE0000) == 0x20000000) && (reset_val >= 0x08000000) && (reset_val <= 0x08020000))
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
        __set_MSP(msp_val);
        
        // 6. Lấy địa chỉ của Reset Handler và thực hiện cú nhảy (Jump)
        pFunction Jump = (pFunction) reset_val;
        Jump();
    }
}

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

void Decode_And_Save(uint8_t byte)
{
    switch(rx_state) {
        case STATE_WAIT_START1:
            if(byte == 0xAA) {
                COUNT = 0;
                buffer_input[COUNT++] = byte;
                rx_state = STATE_WAIT_START2;
            }
            break;
            
        case STATE_WAIT_START2:
            if(byte == 0xBB) {
                buffer_input[COUNT++] = byte;
                rx_state = STATE_WAIT_CMD;
            } else {
                rx_state = STATE_WAIT_START1; // Nhiễu, reset lại từ đầu
            }
            break;
            
        case STATE_WAIT_CMD:
            buffer_input[COUNT++] = byte;
            rx_state = STATE_WAIT_LEN_H;
            break;
            
        case STATE_WAIT_LEN_H:
            buffer_input[COUNT++] = byte;
            payload_len = byte << 8; // Giả sử dùng Big Endian
            rx_state = STATE_WAIT_LEN_L;
            break;
            
        case STATE_WAIT_LEN_L:
            buffer_input[COUNT++] = byte;
            payload_len |= byte;
            
            // Bảo vệ RAM: Nếu Length gửi láo > MAX_PAYLOAD_SIZE, hủy ngay frame này
            if(payload_len > MAX_PAYLOAD_SIZE) {
                rx_state = STATE_WAIT_START1;
            } else {
                payload_cnt = 0;
                rx_state = (payload_len > 0) ? STATE_WAIT_PAYLOAD : STATE_WAIT_CRC_H;
            }
            break;
            
        case STATE_WAIT_PAYLOAD:
            buffer_input[COUNT++] = byte;
            payload_cnt++;
            if(payload_cnt >= payload_len) {
                rx_state = STATE_WAIT_CRC_H;
            }
            break;
            
        case STATE_WAIT_CRC_H:
            buffer_input[COUNT++] = byte;
            rx_state = STATE_WAIT_CRC_L;
            break;
            
        case STATE_WAIT_CRC_L:
            buffer_input[COUNT++] = byte;
            rx_state = STATE_WAIT_END;
            break;
            
        case STATE_WAIT_END:
            if(byte == 0xED) {
                buffer_input[COUNT++] = byte;
                FINISH = 1; // Khóa chốt, báo main loop xử lý
            }
            rx_state = STATE_WAIT_START1; // Reset để đón frame mới
            break;
    }
}

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

void Copy_Flash(uint32_t src_addr, uint32_t dest_addr, uint32_t size_in_kb)
{
    Erase_Application_Partition(dest_addr, size_in_kb);
    
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    uint32_t total_words = (size_in_kb * 1024) / 4;
    for (uint32_t i = 0; i < total_words; i++)
    {
        uint32_t addr_offset = i * 4;
        uint32_t data32 = *(volatile uint32_t*)(src_addr + addr_offset);
        FLASH_ProgramWord(dest_addr + addr_offset, data32);
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

int main()
{
	GPIO_InitTypeDef gpioInit;
	
	Delay_Init();
	//Servo_Init();
	
	// Cấu hình LED PC13 để báo hiệu trạng thái hoạt động
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
	gpioInit.GPIO_Pin = GPIO_Pin_13;
	gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &gpioInit);
	GPIO_ResetBits(GPIOC, GPIO_Pin_13); // Bật LED (LED trên Blue Pill nối nguồn 3.3V, kéo chân xuống 0 để bật)

	UARTx_Config(USART1, 115200, UART_Remap_None, NO); // Chuyển sang chế độ NO interrupt để test polling
	//UARTx_SendStr_Polling(USART1, "System Ready!\n");
	//Servo_Controller(0x00);
	
	uint8_t enter_ota_mode = 0;
	
	uint32_t startup_msp = *(volatile uint32_t*)APP1_ADDRESS;
	uint32_t startup_reset = *(volatile uint32_t*)(APP1_ADDRESS + 4);
	
	// Kiểm tra xem đã có App hợp lệ chạy ở APP1_ADDRESS hay chưa (MSP hợp lệ và Reset Handler hợp lệ)
	if (((startup_msp & 0x2FFE0000) == 0x20000000) && (startup_reset >= 0x08004000) && (startup_reset <= 0x0801FFFF))
	{
		// Nháy LED nhanh trong 3 giây để chờ lệnh nạp mới từ UART
		for (uint16_t i = 0; i < 3000; i++)
		{
			if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
			{
				uint8_t byte = USART_ReceiveData(USART1);
				if (byte == 0xAA) // Nhận được byte bắt đầu
				{
					enter_ota_mode = 1;
					Decode_And_Save(byte); // Nạp byte này vào bộ giải mã
					break;
				}
			}
			Delay_Ms(1);
			if (i % 250 == 0)
			{
				GPIO_WriteBit(GPIOC, GPIO_Pin_13, (BitAction)(1 - GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13)));
			}
		}
	}
	else
	{
		// Chưa có App hợp lệ, buộc phải vào chế độ nạp OTA
		enter_ota_mode = 1;
	}
	
	if (!enter_ota_mode)
	{
		// Tắt LED (kéo chân lên 1) và nhảy vào App cũ
		GPIO_SetBits(GPIOC, GPIO_Pin_13);
		Jump_To_App(APP1_ADDRESS);
	}
	
	// Bật LED PC13 sáng liên tục báo hiệu đang ở chế độ nhận OTA
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);
	
	while(1)
	{
		// Đọc trực tiếp bằng cách kiểm tra cờ phần cứng RXNE
		if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
		{
			temp_byte = USART_ReceiveData(USART1);
			//UARTx_SendChar_Polling(USART1, temp_byte); // Echo về ngay lập tức để chẩn đoán phần cứng
			Decode_And_Save(temp_byte);
		}
		
		if(FINISH)
		{
			uint8_t trigger_jump = 0;
			uint16_t crc_data_len = 5 + payload_len;
			
			// Tính CRC từ đầu mảng luôn (buffer_input tương đương &buffer_input[0])
            uint16_t cal_crc = CRC16_Modbus(buffer_input, crc_data_len);
            
            // Vị trí của CRC trong mảng vẫn không đổi: nằm ngay sau Payload
            uint16_t rev_crc = (buffer_input[5 + payload_len] << 8) | buffer_input[6 + payload_len];
			
			//Check CRC
			if(cal_crc == rev_crc)
            {
                uint8_t cmd = buffer_input[2];
                
                // --- Code xử lý Ghi Flash/Reset của cậu ở đây ---
                switch(cmd) {
                    case 0x01: // Bat dau OTA: Xoa phan vung dem App2 va reset dia chi ghi
                        current_write_address = APP2_ADDRESS; 
                        Erase_Application_Partition(APP2_ADDRESS, APP2_SIZE); 
                        break;
                    case 0x02: // Nhan va ghi khoi du lieu vao App2
                        Process_Received_Chunk(&buffer_input[5], payload_len); 
                        break;
                    case 0x03: // Chuan bi copy sang App1 va nhay sau khi phan hoi ACK
                        trigger_jump = 1;
                        break;
                    default:
                        break;
                }
                
                // Báo hiệu OK
                buffer_output[2] = 0x00; 
            }
            else
            {
                // Sai CRC
                buffer_output[2] = 0x01; 
            }
			
			// Đảo trạng thái LED PC13 báo hiệu đã nhận và xử lý xong gói tin
			GPIO_WriteBit(GPIOC, GPIO_Pin_13, (BitAction)(1 - GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13)));
			
			buffer_output[0] = 0xAA;
			buffer_output[1] = 0xBB;
			buffer_output[3] = 0x06;
  
			uint16_t out_crc = CRC16_Modbus(buffer_output, 4);
			buffer_output[4] = (out_crc >> 8) & 0xFF;
			buffer_output[5] = out_crc & 0xFF;
			
			for (uint16_t i = 0; i < 6; i++) {
				UARTx_SendChar_Polling(USART1, buffer_output[i]);
			}
  
			// Đợi gửi xong byte cuối cùng qua đường truyền vật lý (TC = Transmission Complete)
			while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
  
			// Nếu có cờ yêu cầu nhảy, thực hiện sao chép sang App1 và nhảy sang App mới
			if (trigger_jump) {
				Delay_Ms(100); // Delay ngắn để ESP32 nhận trọn vẹn byte cuối của ACK
				
				// Sao chép từ vùng đệm App2 sang App1
				Copy_Flash(APP2_ADDRESS, APP1_ADDRESS, APP1_SIZE);
				
				// Kiểm tra tính hợp lệ và thực hiện nhảy
				if (((*(__IO uint32_t*)APP1_ADDRESS) & 0x2FFE0000 ) == 0x20000000) {
					Jump_To_App(APP1_ADDRESS);
				}
			}

			// Nếu có lỗi CRC, gửi thêm thông tin debug dạng chuỗi ASCII để ESP32 in ra
			if (buffer_output[2] == 0x01) {
				char debug_msg[100];
				snprintf(debug_msg, sizeof(debug_msg), 
						 "[DBG:cal=%04X,rev=%04X,len=%d,cnt=%d,f3=%02X%02X%02X]", 
						 cal_crc, rev_crc, payload_len, COUNT, 
						 buffer_input[0], buffer_input[1], buffer_input[2]);
				UARTx_SendStr_Polling(USART1, debug_msg);
				// Đợi gửi xong chuỗi debug qua đường truyền vật lý
				while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
			}

			// Xóa các cờ lỗi Overrun (ORE), Noise (NE), Framing Error (FE) để tránh treo bộ nhận UART
			if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET || 
				USART_GetFlagStatus(USART1, USART_FLAG_FE) != RESET || 
				USART_GetFlagStatus(USART1, USART_FLAG_NE) != RESET) {
				volatile uint16_t status = USART1->SR;
				volatile uint16_t data = USART1->DR;
				(void)status;
				(void)data;
			}

			// Đọc xả sạch toàn bộ dữ liệu thừa/rác còn tồn dư trong thanh ghi dịch
			while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
				volatile uint8_t temp = USART_ReceiveData(USART1);
				(void)temp;
			}

			FINISH = 0;
		}
	}
}
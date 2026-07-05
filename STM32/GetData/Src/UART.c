#include "UART.h"

//Ring buffer
volatile uint8_t Rx_Ring_Buffer[BUFFER_SIZE], Tx_Ring_Buffer[BUFFER_SIZE];
volatile uint16_t Tx_Head = 0, Tx_Tail = 0, Rx_Head = 0, Rx_Tail = 0;

//Buffer Driver
void Rx_RingBuffer_Write(uint8_t data) {
    uint16_t next_head = (Rx_Head + 1) % BUFFER_SIZE;
    if (next_head != Rx_Tail) {
        Rx_Ring_Buffer[Rx_Head] = data;
        Rx_Head = next_head;
    }
}

int Rx_RingBuffer_Read(uint8_t *data_out) {
    if (Rx_Head == Rx_Tail) return 0; // Buffer r?ng
    *data_out = Rx_Ring_Buffer[Rx_Tail];
    Rx_Tail = (Rx_Tail + 1) % BUFFER_SIZE;
    return 1;
}

void Tx_RingBuffer_Write(uint8_t data) {
    uint16_t next_head = (Tx_Head + 1) % BUFFER_SIZE;
    if (next_head != Tx_Tail) {
        Tx_Ring_Buffer[Tx_Head] = data;
        Tx_Head = next_head;
    }
}

uint16_t Rx_RingBuffer_GetCount(void) {
    if (Rx_Head >= Rx_Tail) return (Rx_Head - Rx_Tail);
    return (BUFFER_SIZE - Rx_Tail + Rx_Head);
}

int Rx_RingBuffer_HasNewline(void) {
    uint16_t curr_tail = Rx_Tail;
    while (curr_tail != Rx_Head) {
        if (Rx_Ring_Buffer[curr_tail] == '\n') return 1;
        curr_tail = (curr_tail + 1) % BUFFER_SIZE;
    }
    return 0;
}

//-----------------------------------



//UART Driver
void UARTx_Config(USART_TypeDef *USARTx, uint32_t baudrate, UART_Remap_t remap, UART_INTR_t intr) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    GPIO_TypeDef *GPIO_PORT;
    uint16_t TX_PIN, RX_PIN;
    uint32_t RCC_APB_USART, RCC_APB_GPIO;
    uint8_t IRQ_Channel;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

		//Remap
    if (USARTx == USART1) {
        RCC_APB_USART = RCC_APB2Periph_USART1;
        RCC_APB2PeriphClockCmd(RCC_APB_USART, ENABLE);
        IRQ_Channel = USART1_IRQn;
        
        if (remap == UART_Remap_None) {
            RCC_APB_GPIO = RCC_APB2Periph_GPIOA;
            GPIO_PORT = GPIOA;
            TX_PIN = GPIO_Pin_9;
            RX_PIN = GPIO_Pin_10;
        } else { // Full remap
            GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);
            RCC_APB_GPIO = RCC_APB2Periph_GPIOB;
            GPIO_PORT = GPIOB;
            TX_PIN = GPIO_Pin_6;
            RX_PIN = GPIO_Pin_7;
        }
    } 
    else if (USARTx == USART2) {
        RCC_APB_USART = RCC_APB1Periph_USART2;
        RCC_APB1PeriphClockCmd(RCC_APB_USART, ENABLE);
        IRQ_Channel = USART2_IRQn;
        RCC_APB_GPIO = RCC_APB2Periph_GPIOA;
        GPIO_PORT = GPIOA;
        TX_PIN = GPIO_Pin_2;
        RX_PIN = GPIO_Pin_3;
    }
    else { // USART3
      RCC_APB_USART = RCC_APB1Periph_USART3;
      RCC_APB1PeriphClockCmd(RCC_APB_USART, ENABLE);
      IRQ_Channel = USART3_IRQn;
        
			if(remap == UART_Remap_None)
			{
				GPIO_PinRemapConfig(GPIO_PartialRemap_USART3, DISABLE);
				GPIO_PinRemapConfig(GPIO_FullRemap_USART3, DISABLE);
				RCC_APB_GPIO = RCC_APB2Periph_GPIOB;
				GPIO_PORT = GPIOB;
				TX_PIN = GPIO_Pin_10;
				RX_PIN = GPIO_Pin_11;
			}
      else //Partial remap
			{
				GPIO_PinRemapConfig(GPIO_PartialRemap_USART3, ENABLE);
				GPIO_PinRemapConfig(GPIO_FullRemap_USART3, DISABLE);
				RCC_APB_GPIO = RCC_APB2Periph_GPIOC;
				GPIO_PORT = GPIOC;
				TX_PIN = GPIO_Pin_10;
				RX_PIN = GPIO_Pin_11;
			}
    }

    // GPIO
    RCC_APB2PeriphClockCmd(RCC_APB_GPIO, ENABLE);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    // TX Pin
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = TX_PIN;
    GPIO_Init(GPIO_PORT, &GPIO_InitStructure);
    
    // RX Pin
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin = RX_PIN;
    GPIO_Init(GPIO_PORT, &GPIO_InitStructure);

    //NVIC
    if (intr != NO) {
        NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
        NVIC_InitStructure.NVIC_IRQChannel = IRQ_Channel;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }

    //USART
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USARTx, &USART_InitStructure);

    //Interrupt
    if (intr == RX_INTR_ONLY || intr == TX_RX_INTR) {
        USART_ITConfig(USARTx, USART_IT_RXNE, ENABLE);
    }
    
    USART_Cmd(USARTx, ENABLE);
}

void UARTx_SendData(USART_TypeDef* USARTx, uint8_t *data) {
    //Truyen data vao buffer
    while (*data) {
        Tx_RingBuffer_Write(*data++);
    }
    
    USART_ITConfig(USARTx, USART_IT_TXE, ENABLE);
}

void UARTx_SendArray(USART_TypeDef* USARTx, uint8_t *data, uint16_t len)
{
    for(uint16_t i = 0; i < len; i++)
    {
        Tx_RingBuffer_Write(data[i]);
    }

    USART_ITConfig(USARTx, USART_IT_TXE, ENABLE);
}

//Lay 1 byte
int UARTx_ReceiveData(USART_TypeDef* USARTx, uint8_t *data) {
    return Rx_RingBuffer_Read(data);
}

int UARTx_ReceiveMsg_ByLength(uint16_t length, uint8_t *data_out) {
    if (Rx_RingBuffer_GetCount() >= length) {
        for (uint16_t i = 0; i < length; i++) {
            Rx_RingBuffer_Read(&data_out[i]);
        }
        data_out[length] = '\0';
        return 1;
    }
    return 0;
}

int UARTx_ReceiveMsg_ByNewline(uint8_t *data_out) {
    if (Rx_RingBuffer_HasNewline()) {
        uint8_t temp_char;
        uint16_t index = 0;
        do {
            Rx_RingBuffer_Read(&temp_char);
            data_out[index++] = temp_char;
        } while (temp_char != '\n');
        data_out[index] = '\0';
        return 1;
    }
    return 0;
}

void USART1_IRQHandler(void) {
    //Rx
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        Rx_RingBuffer_Write(USART_ReceiveData(USART1));
    }
    
    //Tx
    if (USART_GetITStatus(USART1, USART_IT_TXE) != RESET) {
        if (Tx_Head != Tx_Tail) {
            USART_SendData(USART1, Tx_Ring_Buffer[Tx_Tail]);
            Tx_Tail = (Tx_Tail + 1) % BUFFER_SIZE;
        } else {
            USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
        }
    }
}

void USART2_IRQHandler(void) {
    //Rx
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        Rx_RingBuffer_Write(USART_ReceiveData(USART2));
    }
    
    //Tx
    if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET) {
        if (Tx_Head != Tx_Tail) {
            USART_SendData(USART2, Tx_Ring_Buffer[Tx_Tail]);
            Tx_Tail = (Tx_Tail + 1) % BUFFER_SIZE;
        } else {
            USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
        }
    }
}

void USART3_IRQHandler(void) {
    //Rx
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
        Rx_RingBuffer_Write(USART_ReceiveData(USART3));
    }
    
    //Tx
    if (USART_GetITStatus(USART3, USART_IT_TXE) != RESET) {
        if (Tx_Head != Tx_Tail) {
            USART_SendData(USART3, Tx_Ring_Buffer[Tx_Tail]);
            Tx_Tail = (Tx_Tail + 1) % BUFFER_SIZE;
        } else {
            USART_ITConfig(USART3, USART_IT_TXE, DISABLE);
        }
    }
}

void UARTx_SendChar_Polling(USART_TypeDef* USARTx, uint8_t ch) {
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET);
    USART_SendData(USARTx, ch);
}

void UARTx_SendStr_Polling(USART_TypeDef* USARTx, char *str) {
    while (*str) {
        UARTx_SendChar_Polling(USARTx, *str++);
    }
}


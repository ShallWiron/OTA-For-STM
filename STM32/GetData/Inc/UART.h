#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "stm32f10x.h"
#include "stm32f10x_usart.h"

//Ring Buffer 
#define BUFFER_SIZE 256

extern volatile uint8_t Rx_Ring_Buffer[BUFFER_SIZE];
extern volatile uint8_t Tx_Ring_Buffer[BUFFER_SIZE];
extern volatile uint16_t Tx_Head, Tx_Tail, Rx_Head, Rx_Tail;

//------------------------------------------

//Enum
typedef enum {
    UART_Remap_None = 0,
    UART_Remap_Partial = 1,
    UART_Remap_Full = 2
} UART_Remap_t;

typedef enum {
    NO = 0,
    RX_INTR_ONLY = 1,
    TX_INTR_ONLY = 2,
    TX_RX_INTR = 3
} UART_INTR_t;

//--------------------------------

//Buffer Driver
void Rx_RingBuffer_Write(uint8_t data);
int Rx_RingBuffer_Read(uint8_t *data_out);
void Tx_RingBuffer_Write(uint8_t data);
uint16_t Rx_RingBuffer_GetCount(void);
int Rx_RingBuffer_HasNewline(void);
//-----------------------------------

//UART Driver
void UARTx_Config(USART_TypeDef *USARTx, uint32_t baudrate, UART_Remap_t remap, UART_INTR_t intr);
void UARTx_SendData(USART_TypeDef* USARTx, uint8_t *data);
void UARTx_SendArray(USART_TypeDef* USARTx, uint8_t *data, uint16_t len);
int UARTx_ReceiveData(USART_TypeDef* USARTx, uint8_t *data);
int UARTx_ReceiveMsg_ByLength(uint16_t length, uint8_t *data_out);
int UARTx_ReceiveMsg_ByNewline(uint8_t *data_out);

#endif
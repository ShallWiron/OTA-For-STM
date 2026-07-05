#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>
#include "stm32f10x.h"                  // Device header

extern __IO uint32_t usTicks;

void Delay_Init();
void Delay_Us(uint32_t us);
void Delay_Ms(uint32_t ms);

#endif
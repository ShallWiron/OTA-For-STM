#include "delay.h"
#include "stm32f10x.h"                  // Device header

// For store tick counts in ms
__IO uint32_t msTicks;


// SysTick_Handler function will be called every 1 ms
void SysTick_Handler()
{
    if (msTicks != 0)
    {
        msTicks--;
    }
}

void Delay_Init()
{
    // Update SystemCoreClock value
    SystemCoreClockUpdate();
    // Configure the SysTick timer to overflow every 1 ms
    SysTick_Config(SystemCoreClock / 1000);
}

void Delay_Ms(uint32_t ms)
{
    // Reload ms value
    msTicks = ms;
    // Wait until msTick reach zero
    while (msTicks);
}
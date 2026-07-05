#include "servo.h"
#include "stm32f10x.h"                  // Device header
#include "stm32f10x_gpio.h"             // Keil::Device:StdPeriph Drivers:GPIO
#include "stm32f10x_rcc.h"              // Keil::Device:StdPeriph Drivers:RCC
#include "stm32f10x_tim.h"              // Keil::Device:StdPeriph Drivers:TIM
#include "stm32f10x_exti.h"             // Keil::Device:StdPeriph Drivers:EXTI

void Servo_Init()
{
	//GPIO
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef Init_Struct;
		
	Init_Struct.GPIO_Mode = GPIO_Mode_AF_PP;
	Init_Struct.GPIO_Pin = GPIO_Pin_2;
	Init_Struct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &Init_Struct);
	
	//Timer
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_TimeBaseInitTypeDef Timer_Init_Struct;
	Timer_Init_Struct.TIM_CounterMode = TIM_CounterMode_Up;
	Timer_Init_Struct.TIM_Period = 19999;
	Timer_Init_Struct.TIM_Prescaler = 71;
	Timer_Init_Struct.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &Timer_Init_Struct);
	TIM_Cmd(TIM2, ENABLE);
	
	//OC
	TIM_OCInitTypeDef OC_Init_Struct;
	OC_Init_Struct.TIM_OCMode = TIM_OCMode_PWM1;
	OC_Init_Struct.TIM_OutputState = TIM_OutputState_Enable;
	OC_Init_Struct.TIM_OCPolarity = TIM_OCPolarity_High;
	OC_Init_Struct.TIM_Pulse = 0;
	TIM_OC3Init(TIM2, &OC_Init_Struct);
	TIM_CtrlPWMOutputs(TIM2, ENABLE);
	TIM_OC3Init(TIM2, &OC_Init_Struct);
	TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);
}

void Servo_Controller(int angle){
	if (angle > 180) angle = 180;
	int pulse = 500 + (angle * (2500 - 500)) / 180;
	TIM_SetCompare3(TIM2, pulse);
}

#ifndef __STEERING_
#define __STEERING_
#include "sys.h"

#include "stm32f4xx.h"

#define RCC_SIG          RCC_AHB1Periph_GPIOA

#define PORT_SIG         GPIOA

#define GPIO_SIG         GPIO_Pin_6
#define GPIO_SIG_SOURCE  GPIO_PinSource6

#define AF_SIG           GPIO_AF_TIM3

#define RCC_SIG_TIMER    RCC_APB1Periph_TIM3
#define SIG_TIMER        TIM3  //???3

void SG90_Init(void);
void Set_Servo_Angle(unsigned int angle);
unsigned int Get_Servo_Angle(void);
void TIM3_PWM_Init_AllChannel(void);

#endif


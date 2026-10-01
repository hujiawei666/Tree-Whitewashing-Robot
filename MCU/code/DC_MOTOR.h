
#ifndef __DC_MOTOR_H
#define __DC_MOTOR_H
#include "delay.h"
#include "sys.h"

#include "stm32f4xx.h"                  // Device header

#define GO    0//定义电机状态 正转
#define BACK  1//反转
#define STOP  2//停车
extern int PWM_50;
extern uint16_t shudu;
//extern _Motor Moto1,Moto2,Moto3,Moto4;
/* 带参宏，可以像内联函数一样使用 */
#define INA1(a)	if (a)	\
					GPIO_SetBits(GPIOC,GPIO_Pin_0);\
					else		\
					GPIO_ResetBits(GPIOC,GPIO_Pin_0)
					
#define INB2(a)	if (a)	\
					GPIO_SetBits(GPIOC,GPIO_Pin_1);\
					else		\
					GPIO_ResetBits(GPIOC,GPIO_Pin_1)
					
#define INA3(a)	if (a)	\
					GPIO_SetBits(GPIOC,GPIO_Pin_2);\
					else		\
					GPIO_ResetBits(GPIOC,GPIO_Pin_2)
					
#define INB4(a)	if (a)	\
					GPIO_SetBits(GPIOC,GPIO_Pin_3);\
					else		\
					GPIO_ResetBits(GPIOC,GPIO_Pin_3)
					
#define INA5(a)	if (a)	\
					GPIO_SetBits(GPIOC,GPIO_Pin_4);\
					else		\
					GPIO_ResetBits(GPIOC,GPIO_Pin_4)
					
#define INB6(a)	if (a)	\
					GPIO_SetBits(GPIOC,GPIO_Pin_4);\
					else		\
					GPIO_ResetBits(GPIOC,GPIO_Pin_4)
					
#define INA7(a)	if (a)	\
					GPIO_SetBits(GPIOC,GPIO_Pin_8);\
					else		\
					GPIO_ResetBits(GPIOC,GPIO_Pin_8)
					
#define INB8(a)	if (a)	\
					GPIO_SetBits(GPIOC,GPIO_Pin_9);\
					else		\
					GPIO_ResetBits(GPIOC,GPIO_Pin_9)

void DC_MOTOR_GPIO_Config(void);
void DC_MOTOR_PWM_Config(int num);
void MOTOR_ZQ(char state);
void MOTOR_YQ(char state);
void MOTOR_ZH(char state);
void MOTOR_YH(char state);
					
void Car_Go(void);
void Car_Back(void);
void Car_Turn_Right(void);
void Car_Turn_Left(void);
void Car_Right(void);
void Car_Left(void);				
void Car_Stop(void);
void Car_Go_Right(void);
void Car_Go_Left(void);
void Car_Back_Left(void);
void Car_Back_Right(void);
void Car_Left_Right(void);
void Car_Right_Left(void);
void Car_RRight(void);
void PWM_Control(int PS2_RX,int PS2_RY);
void Analog_stick_model(int PS2_LX,int PS2_LY);
//void Analog_stick_model(void);//遥杆模式
//void Keyboard_model(void); //按键模式
#endif


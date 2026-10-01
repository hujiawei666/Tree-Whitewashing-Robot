/**
 ******************************************************************************
 * @file    bsp_servo.h
 * @brief   SG90 舵机 ×2(TIM3 CH1/CH2)
 *
 * 迁移自 code/steering.h / steering.c。
 *
 * 硬件对应:TIM3 CH1 -> PA6,CH2 -> PA7;50Hz(20ms 周期)
 *   0.5ms 脉宽 -> 0°,1.5ms -> 90°,2.5ms -> 180°
 *   在 200 个计数的周期里对应:5 / 15 / 25
 ******************************************************************************
 */
#ifndef __BSP_SERVO_H
#define __BSP_SERVO_H

#include "bsp_pin.h"

/*
 * 注:安全角度 SAFE_ANGLE(原工程 main.c 里定义为 25)属于**应用层业务参数**,
 *     不在这里定义 —— 舵机驱动层不该替应用决定"多大角度算安全"。
 *     它现在定义在 app_main.c 里。
 */

extern unsigned int ServoAngle;   /* 最近一次写入的比较值 */
extern unsigned int Servo_Angle;  /* 原工程保留变量(见 .c 里的说明) */

/**
 * @brief  启动 TIM3 两路 PWM 输出
 * @note   必须在 MX_TIM3_Init() 之后调用。
 */
void bsp_servo_init(void);

/**
 * @brief  设置两个舵机的角度
 * @param  angle 0~180(度);超过 180 会被夹到 180
 */
void Set_Servo_Angle(unsigned int angle);

/**
 * @brief  读取"当前角度"
 * @note   [!] 原实现返回的 Servo_Angle 从来没被写过,永远返回 0。
 *         本函数全工程无调用点(死代码),这里**忠实保留原行为**,
 *         没有顺手"修好它" —— 免得迁移期间混入行为变化。
 *         如果以后要用,应该让它返回最近一次 Set_Servo_Angle 的角度。
 */
unsigned int Get_Servo_Angle(void);

#endif /* __BSP_SERVO_H */

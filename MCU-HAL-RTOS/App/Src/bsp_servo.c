/**
 ******************************************************************************
 * @file    bsp_servo.c
 * @brief   SG90 舵机实现
 *
 ******************************************************************************
 */

#include "bsp_servo.h"
#include "tim.h"       /* extern TIM_HandleTypeDef htim3 */

unsigned int ServoAngle  = 0;   /* 最近一次写入的比较值 */
unsigned int Servo_Angle = 0;   /* 原工程保留:从未被写,见头文件说明 */

void bsp_servo_init(void)
{
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

void Set_Servo_Angle(unsigned int angle)
{
    if (angle > 180) {
        angle = 180;
    }

    /* 原公式,逐字保留 */
    ServoAngle = (unsigned int)((0.5 + (angle / 180.0) * 2.0) / 20.0 * 200);

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ServoAngle);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, ServoAngle);
}

unsigned int Get_Servo_Angle(void)
{
    return Servo_Angle;   /* 忠实保留原行为(恒为 0) */
}

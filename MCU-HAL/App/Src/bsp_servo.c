/**
 ******************************************************************************
 * @file    bsp_servo.c
 * @brief   SG90 舵机实现
 *
 * 迁移自 code/steering.c。改动只有两点:
 *
 *   1. SG90_Init() 删除
 *      —— 内部的 RCC / GPIO 复用 / TIM3 时基 / 两路 PWM OC 配置全是标准库 API,
 *         迁移后由 MX_TIM3_Init() 与 MX_GPIO_Init() 完成
 *         (你已核对:PSC=8399 / ARR=199 -> 50Hz / Pulse 初值 15 / preload 开)。
 *      保留 bsp_servo_init() 做通道粒度启动。
 *
 *   2. TIM_SetCompare1/2(TIM3, v) -> __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1/2, v)
 *
 * 角度换算公式**逐字保留**:
 *     ServoAngle = (0.5 + angle/180.0 * 2.0) / 20.0 * 200
 *                = 0.5ms~2.5ms 脉宽 在 20ms/200计数 周期里对应的比较值
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

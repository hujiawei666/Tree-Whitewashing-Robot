/**
 ******************************************************************************
 * @file    bsp_ultrasonic.h
 * @brief   超声波回波捕获 ×3(TIM5 三路输入捕获)
 *
 * 迁移自 code/time.c 的 TIM5_Cap_Init() 与 TIM5_IRQHandler()。
 *
 * 硬件对应:
 *   TIM5 CH1 -> PA0  Echo1(左)
 *   TIM5 CH2 -> PA1  Echo2(前)
 *   TIM5 CH3 -> PA2  Echo3(右)
 *   TIM5 设为 1MHz 计数(84MHz/(83+1)),所以**计数值 1 个数 = 1us**,
 *   高电平持续时间直接就是"多少个 us",不用再换算。
 *
 * TRIG 触发脉冲在 code/hcsr04.c 里(那是普通 GPIO,不属于本模块)。
 ******************************************************************************
 */
#ifndef __BSP_ULTRASONIC_H
#define __BSP_ULTRASONIC_H

#include "bsp_pin.h"

/* 传感器编号(与原工程 hcsr04.h 一致) */
#define Left    1
#define Front   2
#define Right   3

/*
 * 三路捕获的状态字,位定义与原工程一致:
 *   bit7    = 1  本次捕获已完成
 *   bit6    = 1  已捕获到上升沿(正在等下降沿)
 *   bit5~0       计数器溢出次数(32 位定时器溢出很慢,仅用于超时判断)
 *
 * 变量名保持原样(TIM5CH1_CAPTURE_STA 等)—— main.c 的 hcsr04_nonblock()
 * 直接读它们算距离,改名会让那边全部要动。
 */
extern u8  TIM5CH1_CAPTURE_STA;
extern u32 TIM5CH1_CAPTURE_VAL;
extern u8  TIM5CH2_CAPTURE_STA;
extern u32 TIM5CH2_CAPTURE_VAL;
extern u8  TIM5CH3_CAPTURE_STA;
extern u32 TIM5CH3_CAPTURE_VAL;

/**
 * @brief  启动 TIM5 的三路输入捕获 + 更新中断
 * @note   必须在 MX_TIM5_Init() 之后调用。
 *         注意这里要启动**两类**中断:
 *           三路捕获:HAL_TIM_IC_Start_IT(CH1/2/3)
 *           溢出计数:HAL_TIM_Base_Start_IT()  ← 漏掉它超时判断就失效
 */
void bsp_ultrasonic_init(void);

/*
 * 捕获事件的处理是 HAL_TIM_IC_CaptureCallback —— 它直接定义在 bsp_ultrasonic.c 里。
 * 那个回调名没有被 CubeMX 占用,可以自己写,所以不需要在这里声明转发函数。
 * (对比:更新事件回调 HAL_TIM_PeriodElapsedCallback 归 CubeMX 所有,只能挂钩子。)
 */

/**
 * @brief  TIM5 更新(溢出)事件处理,由 main.c 的 HAL_TIM_PeriodElapsedCallback 转发进来
 * @note   用于给三路各自累加溢出次数,配合超时判断。
 */
void bsp_ultrasonic_on_period_elapsed(TIM_HandleTypeDef *htim);

/**
 * @brief  给指定超声波模块发一个 TRIG 触发脉冲
 * @param  pos Left / Front / Right
 * @note   迁移自 code/hcsr04.c 的同名函数。
 *         TRIG 是普通 GPIO(位带别名),所以内部逻辑**逐字未改**;
 *         原 Hcsr04_Init() 里的 GPIO 配置已由 MX_GPIO_Init() 接管,该函数被删除。
 */
void Trig_start(u8 pos);

#endif /* __BSP_ULTRASONIC_H */

/**
 ******************************************************************************
 * @file    delay.h
 * @brief   延时接口 —— 替代标准库 SYSTEM/delay/delay.h
 *
 * 函数名与原标准库版本保持一致(delay_init / delay_us / delay_ms),
 * 这样调用点无需改动 —— 迁移时一次只动一层。
 ******************************************************************************
 */
#ifndef __DELAY_H
#define __DELAY_H

#include <stdint.h>

/**
 * @brief  初始化延时模块(DWT 周期计数器)
 * @note   必须在 HAL_Init() 之后调用 —— 它依赖 SystemCoreClock 已被正确设置。
 *         原版本签名是 delay_init(u8 SYSCLK),现在不需要传主频了:
 *         主频由 HAL 在 SystemCoreClock 里维护,传参反而容易写错。
 */
void delay_init(void);

/**
 * @brief  微秒级延时(忙等,不释放 CPU)
 * @param  us 延时长度,单位微秒
 */
void delay_us(uint32_t us);

/**
 * @brief  毫秒级延时
 * @param  ms 延时长度,单位毫秒
 * @note   内部用 HAL_Delay,依赖中断使能(TIM1 中断里累加 tick)。
 *         不要在关中断的临界区里调用。
 */
void delay_ms(uint32_t ms);

#endif /* __DELAY_H */

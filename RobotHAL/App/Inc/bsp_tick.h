/**
 ******************************************************************************
 * @file    bsp_tick.h
 * @brief   1ms 软件节拍(TIM7 更新中断驱动)
 *
 * 迁移自 code/time.c 的 TIM7_Int_Init() + USER/main.c 的 TIM7_IRQHandler()。
 *
 * 原工程的做法(称之为"定时器驱动"模式):
 *   TIM7 每 1ms 中断一次 -> system_time_ms++ -> 主循环靠它计时:
 *       while (system_time_ms == last_system_time) { }   // 空转等下一个节拍
 *
 * 这个变量在 main.c 里被大量当成"时间戳"用(比如 system_time_ms - last > 100),
 * 所以原样保留 —— 交付 B 重构任务划分时再考虑要不要换成 vTaskDelayUntil。
 ******************************************************************************
 */
#ifndef __BSP_TICK_H
#define __BSP_TICK_H

#include "bsp_pin.h"

/**
 * 系统运行毫秒数,由 TIM7 更新中断每 1ms 自增一次。
 * 原定义在 USER/main.c,迁移后归本模块所有。
 * volatile 是必须的 —— 它在中断里被改、在主循环里被读。
 */
extern volatile uint32_t system_time_ms;

/**
 * @brief  启动 TIM7 的更新中断
 * @note   必须在 MX_TIM7_Init() 之后调用。
 *         定时器的时钟/分频/周期/NVIC 都已由 CubeMX 配好,
 *         这里只是"让它跑起来"。
 */
void bsp_tick_init(void);

/**
 * @brief  TIM7 更新事件处理,由 main.c 里的 HAL_TIM_PeriodElapsedCallback 转发进来
 * @param  htim 触发回调的定时器句柄
 * @note   为什么不直接写成 HAL_TIM_PeriodElapsedCallback:
 *         那个名字已经被 CubeMX 用在 TIM1 时基上了(main.c 里),重定义会链接报错。
 *         调用点写在 main.c 的 USER CODE 区里 —— 只有那里不会被 CubeMX 覆盖。
 */
void bsp_tick_on_period_elapsed(TIM_HandleTypeDef *htim);

#endif /* __BSP_TICK_H */

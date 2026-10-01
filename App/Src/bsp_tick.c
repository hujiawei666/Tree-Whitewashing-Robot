/**
 ******************************************************************************
 * @file    bsp_tick.c
 * @brief   1ms 软件节拍实现
 *
 * 迁移自 code/time.c 的 TIM7_Int_Init() 与 USER/main.c 的 TIM7_IRQHandler()。
 * *
 ******************************************************************************
 */

#include "bsp_tick.h"
#include "tim.h"      /* CubeMX 生成:extern TIM_HandleTypeDef htim7 */

volatile uint32_t system_time_ms = 0;

void bsp_tick_init(void)
{
    /* 启动更新中断:每个计数周期(1ms)触发一次 HAL_TIM_PeriodElapsedCallback */
    (void)HAL_TIM_Base_Start_IT(&htim7);
}

/**
 * @brief  TIM7 更新事件处理(每 1ms 一次)
 * @param  htim 触发本次回调的定时器句柄
 * @note   本函数**不是** HAL 回调本身,而是被 main.c 里的
 *         HAL_TIM_PeriodElapsedCallback 转发进来的 —— 因为那个名字归 CubeMX 所有。
 *         main.c 的 USER CODE 区里有这一行:
 *             bsp_tick_on_period_elapsed(htim);
 */
void bsp_tick_on_period_elapsed(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM7) {
        /* 每 1ms 自增一次 —— 原 TIM7_IRQHandler 的全部工作 */
        system_time_ms++;
    }
}

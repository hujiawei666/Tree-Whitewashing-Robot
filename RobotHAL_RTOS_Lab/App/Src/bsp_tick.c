/**
 ******************************************************************************
 * @file    bsp_tick.c
 * @brief   1ms 软件节拍实现
 *
 * 迁移自 code/time.c 的 TIM7_Int_Init() 与 USER/main.c 的 TIM7_IRQHandler()。
 *
 * 对比原来的写法:
 *
 *   原来(TIM7_Int_Init)                      现在(bsp_tick_init)
 *   ---------------------------------------  ---------------------------------
 *   RCC_APB1PeriphClockCmd(TIM7, ENABLE);    ——— 由 MX_TIM7_Init() 开时钟
 *   TIM_TimeBaseInit(TIM7, {arr, psc, ...}); ——— 由 MX_TIM7_Init() 配 PSC/ARR
 *   TIM_ITConfig(TIM7, TIM_IT_Update, EN);   ——— 由 HAL_TIM_Base_Start_IT 开
 *   TIM_Cmd(TIM7, ENABLE);                   ——— 同上
 *   NVIC_Init(...TIM7_IRQn...);              ——— 由 MX_TIM7_Init() 配 NVIC
 *
 * 也就是说:**初始化整件事都被 CubeMX 接管了**,这里只剩"启动"。
 *
 *   原来(TIM7_IRQHandler)                    现在(bsp_tick_on_period_elapsed)
 *   ---------------------------------------  ---------------------------------
 *   if (TIM_GetITStatus(...) == SET) {        ——— HAL 已确认是更新事件才会调进来
 *       system_time_ms++;                     system_time_ms++;
 *       TIM_ClearITPendingBit(...);           ——— HAL 已清标志
 *   }
 *
 * [!] 重要:HAL_TIM_PeriodElapsedCallback 这个名字**已经被 CubeMX 占用了**
 *    (它把 HAL 时基放在 TIM1 上,生成的 main.c 里定义了这个回调用来喂 HAL_IncTick)。
 *    所以本模块**不能**再定义它,否则链接期报 "multiply defined"。
 *    正确做法:本模块只提供一个带自己前缀的处理函数,
 *    由 main.c 的 USER CODE 区(唯一不会被 CubeMX 覆盖的地方)调用过来。
 *
 *    中断入口 TIM7_IRQHandler 本身由 CubeMX 生成在 Core/Src/stm32f4xx_it.c,
 *    内容是 HAL_TIM_IRQHandler(&htim7),不需要动。
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

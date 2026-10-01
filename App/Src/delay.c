/**
 ******************************************************************************
 * @file    delay.c
 * @brief   DWT 版延时 —— 替代标准库 SYSTEM/delay/delay.c
 *
 * 【为什么必须换掉原实现】
 *
 * 原 delay_us / delay_xms 直接改写并关闭 SysTick 寄存器:
 *
 *     SysTick->LOAD = nus * fac_us;      // 改写装载值
 *     SysTick->CTRL |= ENABLE;           // 启动
 *     while (!COUNTFLAG);
 *     SysTick->CTRL &= ~ENABLE;          // ★ 用完把 SysTick 关掉
 *
 * 它把 SysTick 当"私有定时器"用。但 SysTick 是 Cortex-M 的**内核节拍源**:
 *   - HAL 的 HAL_Delay / HAL_GetTick 默认靠它
 *   - FreeRTOS 的内核节拍**也是它**
 *
 * 谁先占用谁赢,另一个彻底错乱 —— 表现为延时长度完全不对,或直接卡死、调度器停摆。
 *
 * 本工程已在 CubeMX 里把 HAL 时基搬到 TIM1,所以交付 A 阶段 SysTick 暂时空闲、
 * 冲突不暴露;但交付 B 加 FreeRTOS 时 SysTick 必须完整让给内核,原实现会立刻致命。
 * 因此这一步在交付 A 就必须做掉。
 *
 * 【为什么用 DWT】
 *
 * DWT(Data Watchpoint and Trace)的 CYCCNT 是 Cortex-M4 **自带的 CPU 周期计数器**:
 *   - 不占用任何外设定时器
 *   - 独立于 SysTick
 *   - 不受中断开关影响,可在临界区里安全使用
 *   - 精度 = 1 个 CPU 周期(168MHz 下约 6ns)
 *
 * 而 HAL_Delay 只有毫秒精度,给不了 5us/10us 这种量级 ——
 * PS2 手柄协议、步进驱动器唤醒脉冲、超声波 TRIG 脉冲都依赖微秒级时序。
 ******************************************************************************
 */

#include "delay.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

/* 1 微秒对应的 CPU 周期数(168MHz 主频下为 168) */
static uint32_t s_cycles_per_us = 0U;

void delay_init(void)
{
    /* 使能 DWT 的跟踪与周期计数器(Cortex-M4 内核自带,无需外设时钟) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* SystemCoreClock 由 HAL 在 SystemInit/时钟配置后维护,此处为 168000000 */
    s_cycles_per_us = SystemCoreClock / 1000000U;
}

void delay_us(uint32_t us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * s_cycles_per_us;

    /*
     * 用**无符号**减法比较,而不是 (DWT->CYCCNT >= start + cycles):
     * CYCCNT 是 32 位、会回绕,无符号减法在回绕前后结果依然正确。
     * 前提是单次延时小于 2^32 个周期(168MHz 下约 25 秒),远大于本工程用到的 100us。
     */
    while ((DWT->CYCCNT - start) < cycles) {
        /* 忙等 —— 但只占本线程的 CPU,不碰任何中断,也不改任何寄存器 */
    }
}

void delay_ms(uint32_t ms)
{
    /* 调度器已经跑起来 -> 用 vTaskDelay,本任务进【阻塞态】,不占 CPU,
     * 其它任务可以运行。这是 RTOS 相对裸机最本质的收益。
     *
     * 注意:不能直接调 vTaskDelay。app_init() 里有 delay_ms(200),
     * 那时调度器还没启动,调 vTaskDelay 会触发 configASSERT 挂掉,
     * 所以必须先判断调度器状态。 */
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        /* 调度器未启动(初始化阶段):只能忙等。
         * HAL_Delay 等的是 TIM1 中断累加的 uwTick。 */
        HAL_Delay(ms);
    }
}

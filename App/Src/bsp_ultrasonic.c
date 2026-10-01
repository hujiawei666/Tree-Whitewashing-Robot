/**
 ******************************************************************************
 * @file    bsp_ultrasonic.c
 * @brief   超声波回波捕获实现(TIM5 三路输入捕获)
 *
 ******************************************************************************
 */

#include "bsp_ultrasonic.h"
#include "delay.h"      /* delay_ms / delay_us */
#include "tim.h"        /* extern TIM_HandleTypeDef htim5 */

/* ---- 三路捕获状态(变量名与原工程一致,main.c 直接使用) ---- */
u8  TIM5CH1_CAPTURE_STA = 0;
u32 TIM5CH1_CAPTURE_VAL = 0;
u8  TIM5CH2_CAPTURE_STA = 0;
u32 TIM5CH2_CAPTURE_VAL = 0;
u8  TIM5CH3_CAPTURE_STA = 0;
u32 TIM5CH3_CAPTURE_VAL = 0;

void bsp_ultrasonic_init(void)
{
    /* 启动三路捕获中断 */
    (void)HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_1);
    (void)HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_2);
    (void)HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_3);

    /* 启动更新中断 —— 用于给三路累加"计数器溢出次数"(超时判断用)。
     * [!] 漏掉这一句不会报错,但溢出永远为 0,超时逻辑失效。 */
    (void)HAL_TIM_Base_Start_IT(&htim5);
}

/**
 * @brief  发一个 TRIG 触发脉冲
 * @param  pos Left / Front / Right
 * @note   迁移自 code/hcsr04.c 的同名函数,**逻辑逐字未改**。
 *         先用普通 GPIO 输出一个 10us 的高脉冲,超声波模块收到后开始测距,
 *         回波由 TIM5 的输入捕获负责(见本文件其余部分)。
 *         TRIG 用的是位带别名(Left_TRIG_Send 等),不依赖任何库,HAL 下同样有效。
 */
void Trig_start(u8 pos)
{
    switch (pos) {
        case Left:
            Left_TRIG_Send = 0;
            delay_ms(1);
            Left_TRIG_Send = 1;
            delay_us(10);
            Left_TRIG_Send = 0;
            delay_ms(1);
            break;

        case Front:
            Front_TRIG_Send = 0;
            delay_ms(1);
            Front_TRIG_Send = 1;
            delay_us(10);
            Front_TRIG_Send = 0;
            delay_ms(1);
            break;

        case Right:
            Right_TRIG_Send = 0;
            delay_ms(1);
            Right_TRIG_Send = 1;
            delay_us(10);
            Right_TRIG_Send = 0;
            delay_ms(1);
            break;

        default:
            break;
    }
}

/* ===========================================================================
 * 溢出(更新事件)处理 —— 对应原 ISR 里 `if (TIM_GetITStatus(TIM5, TIM_IT_Update))` 那几段
 * =========================================================================== */
static void overflow_one(u8 *sta, u32 *val)
{
    if ((*sta & 0x80) == 0) {          /* 本路尚未捕获完成 */
        if (*sta & 0x40) {             /* 已经在等高电平结束 */
            if ((*sta & 0x3F) == 0x3F) {
                *sta |= 0x80;          /* 溢出次数到顶 —— 认为超时,给最大值 */
                *val = 0xFFFFFFFF;
            } else {
                (*sta)++;              /* 记一次溢出 */
            }
        }
    }
}

void bsp_ultrasonic_on_period_elapsed(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM5) {
        return;
    }

    overflow_one(&TIM5CH1_CAPTURE_STA, &TIM5CH1_CAPTURE_VAL);
    overflow_one(&TIM5CH2_CAPTURE_STA, &TIM5CH2_CAPTURE_VAL);
    overflow_one(&TIM5CH3_CAPTURE_STA, &TIM5CH3_CAPTURE_VAL);
}

/* ===========================================================================
 * 捕获事件处理 —— 对应原 ISR 里 `if (TIM_GetITStatus(TIM5, TIM_IT_CCn))` 那几段
 *
 * 逻辑逐字照搬原工程 CH1 那一份(`TIM_GetCapture1` / `TIM_OC1PolarityConfig`
 * 换成对应的 HAL 宏,通道号参数化)。
 * =========================================================================== */
static void capture_one(TIM_HandleTypeDef *htim, uint32_t channel, u8 *sta, u32 *val)
{
    if ((*sta & 0x80) == 0) {          /* 本路尚未捕获完成 */

        if (*sta & 0x40) {
            /* 之前已经捕到过上升沿 —— 这次是下降沿,高电平结束 */
            *sta |= 0x80;                                   /* 标记本路完成 */
            *val = HAL_TIM_ReadCapturedValue(htim, channel); /* 记下持续计数值 */
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, channel, TIM_INPUTCHANNELPOLARITY_RISING);
        } else {
            /* 第一次捕获(上升沿)—— 重新开始计时 */
            *sta  = 0;
            *val  = 0;
            *sta |= 0x40;                                   /* 标记"已捕到上升沿" */
            __HAL_TIM_DISABLE(htim);                        /* 停下定时器才能安全清计数 */
            __HAL_TIM_SET_COUNTER(htim, 0);
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, channel, TIM_INPUTCHANNELPOLARITY_FALLING);
            __HAL_TIM_ENABLE(htim);                         /* 重新计数,等下降沿 */
        }
    }
}

/**
 * @brief  定时器输入捕获回调(HAL 对所有做过输入捕获的定时器共用这一个)
 * @note   这个回调名**没有被 CubeMX 占用**(它只占用了 HAL_TIM_PeriodElapsedCallback),
 *         所以可以直接在这里定义。
 *         对比:更新事件那个回调归 CubeMX 所有,只能在 main.c 的 USER CODE 区挂钩子。
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM5) {
        return;
    }

    /* [!] htim->Channel 告诉我们是哪一路触发的 —— 不判断的话三路数据会互相覆盖 */
    switch (htim->Channel) {
        case HAL_TIM_ACTIVE_CHANNEL_1:
            capture_one(htim, TIM_CHANNEL_1, &TIM5CH1_CAPTURE_STA, &TIM5CH1_CAPTURE_VAL);
            break;
        case HAL_TIM_ACTIVE_CHANNEL_2:
            capture_one(htim, TIM_CHANNEL_2, &TIM5CH2_CAPTURE_STA, &TIM5CH2_CAPTURE_VAL);
            break;
        case HAL_TIM_ACTIVE_CHANNEL_3:
            capture_one(htim, TIM_CHANNEL_3, &TIM5CH3_CAPTURE_STA, &TIM5CH3_CAPTURE_VAL);
            break;
        default:
            break;
    }
}

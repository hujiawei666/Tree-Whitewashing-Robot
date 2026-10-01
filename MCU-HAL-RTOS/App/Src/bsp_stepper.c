/**
 ******************************************************************************
 * @file    bsp_stepper.c
 * @brief   步进电机实现(TIM8 单脉冲 + 重复计数,T 型加减速)
 *

 ******************************************************************************
 */

#include "bsp_stepper.h"
#include "delay.h"
#include "tim.h"        /* extern TIM_HandleTypeDef htim8 */
#include <stdlib.h>     /* labs */

/* ---- 全局状态(与原工程一致) ---- */
u8  rcr_remainder = 0;
u8  is_rcr_finish = 1;
long rcr_integer = 0;
long target_pos = 0;
long current_pos = 0;
DIR_Type motor_dir = CW;
u8  is_accel_enabled = 0;
T_Profile t_profile;

void bsp_stepper_init(void)
{
    /*
     * CubeMX 配不了的位:CR1 的 URS(Update Request Source)
     *
     * 原工程在 TIM8_OPM_RCR_Init() 里有:
     *     TIM_UpdateRequestConfig(TIM8, TIM_UpdateSource_Regular);
     * 它的含义是"只有计数器真正溢出才产生更新事件,**软件置 UG 位不算**"。
     *
     * 为什么这里非有不可:下面的中断每次都调 HAL_TIM_GenerateEvent(..., UPDATE)
     * 来重装 RCR 并重启脉冲。若不设 URS,这一句**每次都会额外触发一次更新中断**,
     * 中断被调用两倍,加减速节奏就乱了。
     *
     * HAL 的 TIM_Base_SetConfig 会整体重写 CR1(把 URS 清掉),而 CubeMX 界面里
     * 没有这个选项 —— 所以只能在这里手动置位。
     */
    htim8.Instance->CR1 |= TIM_CR1_URS;

    /* 启动 CH2 的 PWM 输出 */
    (void)HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);

    /* 启动更新中断(加减速算法在中断里跑) */
    (void)HAL_TIM_Base_Start_IT(&htim8);
}

/* ===========================================================================
 * T 型速度曲线参数计算
 * =========================================================================== */
static void T_Profile_Init(long steps, float start_freq, float max_freq, float accel)
{
    float accel_time, decel_time;

    t_profile.total_steps  = steps;
    t_profile.start_freq   = start_freq;
    t_profile.max_freq     = max_freq;
    t_profile.acceleration = accel;
    t_profile.current_step = 0;
    t_profile.current_freq = start_freq;
    t_profile.state        = ACCEL;

    /* 1. 跑到最高频所需时间 */
    accel_time = (max_freq - start_freq) / accel;

    /* 2. 加速段步数(梯形面积公式) */
    t_profile.accel_steps = (u32)((start_freq + max_freq) * accel_time / 2);

    /* 3. 减速段用同样的加速度 */
    decel_time = (max_freq - start_freq) / accel;
    t_profile.decel_steps = (u32)((start_freq + max_freq) * decel_time / 2);

    /* 4. 总步数够不够放加减速段 */
    if (t_profile.accel_steps + t_profile.decel_steps > (u32)steps) {
        float reduction_ratio = (float)steps / (t_profile.accel_steps + t_profile.decel_steps);
        t_profile.accel_steps = (u32)(t_profile.accel_steps * reduction_ratio);
        t_profile.decel_steps = (u32)(t_profile.decel_steps * reduction_ratio);
    }

    /* 5. 匀速段步数 */
    t_profile.const_steps = (u32)steps - t_profile.accel_steps - t_profile.decel_steps;

    /* 6. 每步的频率增量 */
    if (t_profile.accel_steps > 0) {
        t_profile.accel_increment = (max_freq - start_freq) / t_profile.accel_steps;
    } else {
        t_profile.accel_increment = 0;
    }

    if (t_profile.decel_steps > 0) {
        t_profile.decel_increment = (max_freq - start_freq) / t_profile.decel_steps;
    } else {
        t_profile.decel_increment = 0;
    }
}

/* ===========================================================================
 * 启动 TIM8 输出
 * =========================================================================== */
void TIM8_Startup(u32 frequency)
{
    u16 temp_arr;

    if (frequency < MIN_FREQUENCY) frequency = MIN_FREQUENCY;
    if (frequency > MAX_FREQUENCY) frequency = MAX_FREQUENCY;

    /* ARR 换算(1MHz 计数) */
    temp_arr = (u16)(1000000 / frequency - 1);

    if (temp_arr < 2)      temp_arr = 2;
    if (temp_arr > 0xFFFF) temp_arr = 0xFFFF;

    __HAL_TIM_SET_AUTORELOAD(&htim8, temp_arr);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, temp_arr >> 1);
    __HAL_TIM_SET_COUNTER(&htim8, 0);
    __HAL_TIM_ENABLE(&htim8);
}

void Set_Acceleration(float accel)
{
    t_profile.acceleration = accel;
}

void Enable_Acceleration(u8 enable)
{
    is_accel_enabled = enable;
}

/* ===========================================================================
 * 更新中断:加减速算法核心
 * =========================================================================== */
void bsp_stepper_on_period_elapsed(TIM_HandleTypeDef *htim)
{
    long steps_done;
    u32  new_freq;
    u16  temp_arr;
    u32  remaining_steps;
    static u8 end_delay_cnt = 0;   /* 减速段结束前的延时计数,防止过早停机 */

    if (htim->Instance != TIM8) {
        return;                    /* 本回调是所有定时器共用的,先认领 */
    }

    /* 原版在这里查/清 TIM_IT_Update 标志,然后才进入下面一整套。
     * HAL 已经把标志清好了,能进回调就说明是更新事件。 */

    if (is_rcr_finish == 0) {
        if (is_accel_enabled) {
            /* ---- 带加减速的 T 型曲线 ---- */
            steps_done = htim8.Instance->RCR + 1;

            current_pos += (motor_dir == CW) ? steps_done : -steps_done;
            t_profile.current_step += (u32)steps_done;

            switch (t_profile.state) {
                case ACCEL:
                    t_profile.current_freq += t_profile.accel_increment * steps_done;
                    if (t_profile.current_freq >= t_profile.max_freq) {
                        t_profile.current_freq = t_profile.max_freq;
                        if (t_profile.current_step < t_profile.total_steps - t_profile.decel_steps) {
                            t_profile.state = CONSTANT;
                        } else {
                            t_profile.state = DECEL;
                            end_delay_cnt = 0;
                        }
                    }
                    break;

                case CONSTANT:
                    t_profile.current_freq = t_profile.max_freq;
                    if (t_profile.current_step >= t_profile.total_steps - t_profile.decel_steps) {
                        t_profile.state = DECEL;
                        end_delay_cnt = 0;
                    }
                    break;

                case DECEL:
                    t_profile.current_freq -= t_profile.decel_increment * steps_done;
                    if (t_profile.current_freq < MIN_FREQUENCY) {
                        t_profile.current_freq = MIN_FREQUENCY;
                    }
                    /* 收尾延时:多走几步再停,避免最后一拍丢步 */
                    if (t_profile.current_step >= t_profile.total_steps - 3) {
                        if (end_delay_cnt++ > 3) {
                            is_rcr_finish = 1;
                            __HAL_TIM_MOE_DISABLE(&htim8);
                            __HAL_TIM_DISABLE(&htim8);
                            return;
                        }
                    }
                    break;
            }

            new_freq = (u32)t_profile.current_freq;
            if (new_freq < MIN_FREQUENCY) new_freq = MIN_FREQUENCY;
            if (new_freq > MAX_FREQUENCY) new_freq = MAX_FREQUENCY;

            temp_arr = (u16)(1000000 / new_freq - 1);
            if (temp_arr < 2)      temp_arr = 2;
            if (temp_arr > 0xFFFF) temp_arr = 0xFFFF;

            __HAL_TIM_SET_AUTORELOAD(&htim8, temp_arr);
            __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, temp_arr >> 1);

            remaining_steps = t_profile.total_steps - t_profile.current_step;

            /* 重设 RCR —— HAL 没有对应宏,直接写寄存器(与原工程一致) */
            if (remaining_steps > RCR_VAL) {
                htim8.Instance->RCR = RCR_VAL;
            } else if (remaining_steps > 0) {
                htim8.Instance->RCR = remaining_steps - 1;
            } else {
                is_rcr_finish = 1;
                __HAL_TIM_MOE_DISABLE(&htim8);
                __HAL_TIM_DISABLE(&htim8);
                return;
            }

            /* 重装 RCR 并重启输出 */
            (void)HAL_TIM_GenerateEvent(&htim8, TIM_EVENTSOURCE_UPDATE);
            __HAL_TIM_MOE_ENABLE(&htim8);
            __HAL_TIM_ENABLE(&htim8);

        } else {
            /* ---- 无加减速的简模式:按 RCR 整批/余数走 ---- */
            if (rcr_integer != 0) {
                htim8.Instance->RCR = RCR_VAL;
                rcr_integer--;
                current_pos += (motor_dir == CW) ? (RCR_VAL + 1) : -(RCR_VAL + 1);
            } else if (rcr_remainder != 0) {
                htim8.Instance->RCR = rcr_remainder - 1;
                current_pos += (motor_dir == CW) ? rcr_remainder : -rcr_remainder;
                rcr_remainder = 0;
                is_rcr_finish = 1;
            } else {
                is_rcr_finish = 1;
            }

            if (!is_rcr_finish) {
                (void)HAL_TIM_GenerateEvent(&htim8, TIM_EVENTSOURCE_UPDATE);
                __HAL_TIM_MOE_ENABLE(&htim8);
                __HAL_TIM_ENABLE(&htim8);
            } else {
                __HAL_TIM_MOE_DISABLE(&htim8);
                __HAL_TIM_DISABLE(&htim8);
            }
        }
    } else {
        /* 运动已完成,确保输出关掉 */
        __HAL_TIM_MOE_DISABLE(&htim8);
        __HAL_TIM_DISABLE(&htim8);
    }
}

/* ===========================================================================
 * 定位
 * =========================================================================== */

/**
 * @note 唤醒脉冲循环里写的是 GPIOC Pin_7 —— 但该引脚在初始化后被 TIM8 复用为
 *       CH2(AF 模式),此时写 ODR **不会真的改变引脚电平**。
 *       原工程就是这样,这里忠实照搬未改动。
 *       若 L2 上板发现"驱动器没被唤醒",就是这里的问题 —— 届时应在输出脉冲前
 *       临时把 PC7 切回 GPIO 模式。
 */
static void send_wakeup_pulses(DIR_Type dir)
{
    u8  wakeup_pulses = WAKEUP_PULSES;
    int i;

    DRIVER_OE = 0;          /* 使能驱动器(低有效) */
    delay_us(100);

    for (i = 0; i < wakeup_pulses; i++) {
        DRIVER_DIR = dir;
        delay_us(50);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
        delay_us(5);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
        delay_us(100);
    }
}

void Locate_Rle(long num, u32 frequency, DIR_Type dir)
{
    long steps;

    send_wakeup_pulses(dir);

    steps = num;

    /* 参数与状态检查 */
    if (steps <= 0) return;
    if (htim8.Instance->CR1 & TIM_CR1_CEN) return;    /* 定时器还忙着 */
    if (frequency < MIN_FREQUENCY || frequency > MAX_FREQUENCY) return;

    motor_dir = dir;
    DRIVER_DIR = motor_dir;

    /* 目标位置 */
    if (motor_dir == CW) {
        target_pos = current_pos + steps;
    } else {
        target_pos = current_pos - steps;
    }

    /* 启动运动 */
    if (is_accel_enabled) {
        T_Profile_Init(steps, MIN_FREQUENCY, (float)frequency, t_profile.acceleration);
        TIM8_Startup((u32)t_profile.start_freq);
    } else {
        rcr_integer   = steps / (RCR_VAL + 1);
        rcr_remainder = (u8)(steps % (RCR_VAL + 1));
        TIM8_Startup(frequency);
    }

    is_rcr_finish = 0;
}

void Locate_Abs(long num, u32 frequency)
{
    long steps;

    if (htim8.Instance->CR1 & TIM_CR1_CEN) return;
    if (frequency < MIN_FREQUENCY || frequency > MAX_FREQUENCY) return;

    target_pos = num;
    steps = labs(target_pos - current_pos);

    if (steps == 0) return;

    if (target_pos > current_pos) {
        motor_dir = CW;
    } else {
        motor_dir = CCW;
    }
    DRIVER_DIR = motor_dir;

    send_wakeup_pulses(motor_dir);

    if (is_accel_enabled) {
        T_Profile_Init(steps, MIN_FREQUENCY, (float)frequency, t_profile.acceleration);
        TIM8_Startup((u32)t_profile.start_freq);
    } else {
        rcr_integer   = steps / (RCR_VAL + 1);
        rcr_remainder = (u8)(steps % (RCR_VAL + 1));
        TIM8_Startup(frequency);
    }

    is_rcr_finish = 0;
}

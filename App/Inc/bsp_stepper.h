/**
 ******************************************************************************
 * @file    bsp_stepper.h
 * @brief   步进电机驱动器(TIM8 CH2 单脉冲 + 重复计数器,涂白机构)
 *
 * 迁移自 code/driver.h / driver.c。
 *
 * 硬件对应:
 *   TIM8 CH2 -> PC7  脉冲输出(PUL)
 *   PE5      -> DRIVER_DIR  方向
 *   PE6      -> DRIVER_OE   使能(低有效)
 *
 * 工作原理(原工程的 T 型加减速算法):
 *   TIM8 配成"单脉冲 + 重复计数(RCR)"模式,一个 RCR 周期输出 RCR_VAL+1 个脉冲。
 *   每完成一组脉冲就进一次更新中断,在中断里按 T 型曲线算出**下一个频点**,
 *   重设 ARR / CCR / RCR 并重新触发 —— 于是脉冲频率沿"加速→匀速→减速"变化。
 ******************************************************************************
 */
#ifndef __BSP_STEPPER_H
#define __BSP_STEPPER_H

#include "bsp_pin.h"

/* ---- 引脚 ---- */
#define DRIVER_DIR   PEout(5)   /* 方向 */
#define DRIVER_OE    PEout(6)   /* 使能(低有效) */

/* ---- 参数(与原工程同名同值) ---- */
#define RCR_VAL         255      /* 重复计数器最大值 */
#define MIN_FREQUENCY   1600     /* 最低频率 Hz */
#define MAX_FREQUENCY   10000    /* 最高频率 Hz */
#define DEFAULT_ACCEL   3500     /* 默认加速度 Hz/s */
#define WAKEUP_PULSES   25       /* 上电唤醒脉冲个数 */

/* ---- 类型(与原工程一致) ---- */
typedef enum {
    CW  = 1,   /* 顺时针 */
    CCW = 0    /* 逆时针 */
} DIR_Type;

typedef enum {
    ACCEL,     /* 加速段 */
    CONSTANT,  /* 匀速段 */
    DECEL      /* 减速段 */
} MotionState;

typedef struct {
    u32 total_steps;      /* 总步数 */
    u32 accel_steps;      /* 加速段步数 */
    u32 decel_steps;      /* 减速段步数 */
    u32 const_steps;      /* 匀速段步数 */
    u32 current_step;     /* 当前已走步数 */
    MotionState state;    /* 当前状态 */

    float start_freq;     /* 起始频率 */
    float end_freq;       /* 结束频率 */
    float max_freq;       /* 最高频率 */
    float acceleration;   /* 加速度 Hz/s */

    float current_freq;   /* 当前频率 */
    float accel_increment;/* 加速段每步的频率增量 */
    float decel_increment;/* 减速段每步的频率增量 */
} T_Profile;

/* ---- 全局状态(与原工程同名,外部可能读) ---- */
extern long     target_pos;
extern long     current_pos;
extern DIR_Type motor_dir;
extern T_Profile t_profile;
extern u8       is_accel_enabled;

/* ---- 接口 ---- */
/**
 * @brief  启动 TIM8 的单脉冲输出与更新中断
 * @note   必须在 MX_TIM8_Init() 之后调用。
 *         除了启动 PWM,本函数还负责补一条 CubeMX 配不了的位:
 *         CR1 的 URS —— 否则每次软件触发 Update 都会多进一次中断(见 .c 里的说明)。
 */
void bsp_stepper_init(void);

/**
 * @brief  TIM8 更新事件处理(加减速算法的核心)
 * @note   由 main.c 的 HAL_TIM_PeriodElapsedCallback 转发进来
 *         —— 那个回调名归 CubeMX 所有,只能挂钩子。
 */
void bsp_stepper_on_period_elapsed(TIM_HandleTypeDef *htim);

void TIM8_Startup(u32 frequency);
void Set_Acceleration(float accel);
void Enable_Acceleration(u8 enable);
void Locate_Rle(long num, u32 frequency, DIR_Type dir);
void Locate_Abs(long num, u32 frequency);

#endif /* __BSP_STEPPER_H */

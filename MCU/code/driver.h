#ifndef __DRIVER_H
#define __DRIVER_H

#include "stm32f4xx.h"
#include <math.h>

//引脚定义
#define DRIVER_DIR   PEout(5)  // 方向引脚
#define DRIVER_OE    PEout(6)  // 使能引脚 
#define RCR_VAL      255       // 重复计数器最大值
#define PULSE_PORT   GPIOC
#define PULSE_PIN    GPIO_Pin_7

// 电机参数
#define MIN_FREQUENCY         1600     // 启动频率(Hz)
#define MAX_FREQUENCY         10000    // 最大频率(Hz)
#define DEFAULT_ACCEL         3500    // 默认加速度(Hz/s)
#define WAKEUP_PULSES         25       // 唤醒脉冲数量

//方向
typedef enum {
    CW = 1,  // 顺时针
    CCW = 0  // 逆时针
} DIR_Type;

// 运动状态
typedef enum {
    ACCEL,        // 加速
    CONSTANT,     // 匀速
    DECEL         // 减速
} MotionState;

// T型算法
typedef struct {
    u32 total_steps;          // 总步数
    u32 accel_steps;          // 加速步数
    u32 decel_steps;          // 减速步数
    u32 const_steps;          // 匀速步数
    u32 current_step;         // 当前步数
    MotionState state;        // 当前状态
    
    float start_freq;         // 起始频率
    float end_freq;           // 结束频率
    float max_freq;           // 最大频率
    float acceleration;       // 加速度(Hz/s)
    
    float current_freq;       // 当前频率
    float accel_increment;    // 加速段频率增量  每次增加的速度
    float decel_increment;    // 减速段频率增量
} T_Profile;

// 全局变量
extern long target_pos;
extern long current_pos;
extern DIR_Type motor_dir;
extern T_Profile t_profile;
extern u8 is_accel_enabled;

// 函数声明
void Driver_Init(void);
void TIM8_OPM_RCR_Init(u16 arr, u16 psc);
void TIM8_Startup(u32 frequency);
void Locate_Rle(long num, u32 frequency, DIR_Type dir);
void Locate_Abs(long num, u32 frequency);
void Set_Acceleration(float accel);
void Enable_Acceleration(u8 enable);

#endif

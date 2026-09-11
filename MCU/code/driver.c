#include "driver.h"
#include "delay.h"
#include "usart.h"
#include <stdlib.h> 

// 全局变量
u8 rcr_remainder = 0;
u8 is_rcr_finish = 1;
long rcr_integer = 0;
long target_pos = 0;
long current_pos = 0;
DIR_Type motor_dir = CW;
u8 is_accel_enabled = 0;
T_Profile t_profile;

/************** 驱动器初始化 ****************/
void Driver_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
 
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6|GPIO_Pin_3;//E3水泵
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOE, &GPIO_InitStructure);
    
    GPIO_SetBits(GPIOE, GPIO_Pin_5);    // 初始方向
    GPIO_ResetBits(GPIOE, GPIO_Pin_6);  // 初始使能
    
    // 脉冲引脚初始化
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOC, GPIO_Pin_7);  //初始化低电平
}

/***********************************************
// TIM8_CH2(PC7) 单脉冲+重复计数模式初始化
************************************************/
void TIM8_OPM_RCR_Init(u16 arr, u16 psc)
{		 					 
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_TIM8);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseStructure);
    TIM_ClearITPendingBit(TIM8, TIM_IT_Update);

    TIM_UpdateRequestConfig(TIM8, TIM_UpdateSource_Regular);
    TIM_SelectOnePulseMode(TIM8, TIM_OPMode_Single);
 
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_Pulse = arr >> 1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init(TIM8, &TIM_OCInitStructure);

    TIM_OC2PreloadConfig(TIM8, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM8, ENABLE);
    
    TIM_ITConfig(TIM8, TIM_IT_Update, ENABLE);
 
    NVIC_InitStructure.NVIC_IRQChannel = TIM8_UP_TIM13_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    TIM_ClearITPendingBit(TIM8, TIM_IT_Update);
    TIM_Cmd(TIM8, ENABLE);
}

/******* TIM8中断处理函数-核心T算法实现 *********/
void TIM8_UP_TIM13_IRQHandler(void)
{
    long steps_done;
    u32 new_freq;
    u16 temp_arr;
    u32 remaining_steps;
    static u8 end_delay_cnt = 0;  // 添加结束延时计数器
    
    if (TIM_GetITStatus(TIM8, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM8, TIM_IT_Update);
        
        if (is_rcr_finish == 0)
        {
            if (is_accel_enabled)
            {
                //获取本次中断完成的步数
                steps_done = TIM8->RCR + 1;
                
                // 更新位置
                current_pos += (motor_dir == CW) ? steps_done : -steps_done;
                t_profile.current_step += steps_done;
                
                // 状态机处理
                switch(t_profile.state) {
                    case ACCEL:
                        // 加速阶段：频率线性增加
                        t_profile.current_freq += t_profile.accel_increment * steps_done;
                        
                        // 检测是否到达最大频率
                        if (t_profile.current_freq >= t_profile.max_freq) {
                            t_profile.current_freq = t_profile.max_freq;
                            
                            // 判断是否需要进入匀速阶段
                            if (t_profile.current_step < t_profile.total_steps - t_profile.decel_steps) {
                                t_profile.state = CONSTANT;
                            } else {
                                t_profile.state = DECEL;
                                end_delay_cnt = 0;  //重置结束延时计数器
                            }
                        }
                        break;
                        
                    case CONSTANT:
                        // 匀速阶段：保持最大频率
                        t_profile.current_freq = t_profile.max_freq;
                        
                        // 检测是否进入减速阶段
                        if (t_profile.current_step >= t_profile.total_steps - t_profile.decel_steps) {
                            t_profile.state = DECEL;
                            end_delay_cnt = 0;  // 重置结束延时计数器
                        }
                        break;
                        
                    case DECEL:
                        // 减速阶段：频率线速性减小
                        t_profile.current_freq -= t_profile.decel_increment * steps_done;
                        
                        // 确保频率不低于最小值
                        if (t_profile.current_freq < MIN_FREQUENCY) {
                            t_profile.current_freq = MIN_FREQUENCY;
                        }
                        
                        // 结束延时处理（防止电机失步）
                        if (t_profile.current_step >= t_profile.total_steps - 3) {
                            if (end_delay_cnt++ > 3) {
                                is_rcr_finish = 1;
                                TIM_CtrlPWMOutputs(TIM8, DISABLE);
                                TIM_Cmd(TIM8, DISABLE);
                                return;
                            }
                        }
                        break;
                }
                
                // 计算新频率
                new_freq = (u32)t_profile.current_freq;
                
                // 限制频率范围
                if (new_freq < MIN_FREQUENCY) new_freq = MIN_FREQUENCY;
                if (new_freq > MAX_FREQUENCY) new_freq = MAX_FREQUENCY;
                
                // 计算ARR值(1MHz时钟)
                temp_arr = 1000000 / new_freq - 1; 
                
                // 限制ARR范围
                if (temp_arr < 2) temp_arr = 2;
                if (temp_arr > 0xFFFF) temp_arr = 0xFFFF;
                
                // 跟新定时器参数
                TIM_SetAutoreload(TIM8, temp_arr);
                TIM_SetCompare2(TIM8, temp_arr >> 1);
                
                // 计算剩余步数
                remaining_steps = t_profile.total_steps - t_profile.current_step;
                
                // 设置RCR值
                if (remaining_steps > RCR_VAL) {
                    TIM8->RCR = RCR_VAL;
                } else if (remaining_steps > 0) {
                    TIM8->RCR = remaining_steps - 1;
                } else {
                    // 运动完成
                    is_rcr_finish = 1;
                    TIM_CtrlPWMOutputs(TIM8, DISABLE);
                    TIM_Cmd(TIM8, DISABLE);
                    return;
                }
                
                // 重新启动定时器
                TIM_GenerateEvent(TIM8, TIM_EventSource_Update);
                TIM_CtrlPWMOutputs(TIM8, ENABLE);
                TIM_Cmd(TIM8, ENABLE);
            }
            else
            {
                // 无加速度控制的简单模式（保持原有逻辑）
                if (rcr_integer != 0) {
                    TIM8->RCR = RCR_VAL;
                    rcr_integer--;
                    current_pos += (motor_dir == CW) ? (RCR_VAL + 1) : -(RCR_VAL + 1);
                } else if (rcr_remainder != 0) {
                    TIM8->RCR = rcr_remainder - 1;
                    current_pos += (motor_dir == CW) ? rcr_remainder : -rcr_remainder;
                    rcr_remainder = 0;
                    is_rcr_finish = 1;
                } else {
                    is_rcr_finish = 1;
                }
                
                if (!is_rcr_finish) {
                    TIM_GenerateEvent(TIM8, TIM_EventSource_Update);
                    TIM_CtrlPWMOutputs(TIM8, ENABLE);
                    TIM_Cmd(TIM8, ENABLE);
                } else {
                    TIM_CtrlPWMOutputs(TIM8, DISABLE);
                    TIM_Cmd(TIM8, DISABLE);
                }
            }
        }
        else
        {
            // 运动完成，关闭输出
            TIM_CtrlPWMOutputs(TIM8, DISABLE);
            TIM_Cmd(TIM8, DISABLE);
        }
    }
}

/***************** 启动TIM8 *****************/
void TIM8_Startup(u32 frequency)
{
    u16 temp_arr;
    
    // 限制频率范围
    if (frequency < MIN_FREQUENCY) frequency = MIN_FREQUENCY;
    if (frequency > MAX_FREQUENCY) frequency = MAX_FREQUENCY;
    
    // 计算ARR值(1MHz时钟)
    temp_arr = 1000000 / frequency - 1; 
    
    // 限制ARR范围
    if (temp_arr < 2) temp_arr = 2;
    if (temp_arr > 0xFFFF) temp_arr = 0xFFFF;
    
    // 配置定时器
    TIM_SetAutoreload(TIM8, temp_arr);
    TIM_SetCompare2(TIM8, temp_arr >> 1);
    TIM_SetCounter(TIM8, 0);
    TIM_Cmd(TIM8, ENABLE);
}

/*************加速定时器 *************/
void Set_Acceleration(float accel)
{
    t_profile.acceleration = accel;
}

void Enable_Acceleration(u8 enable)
{
    is_accel_enabled = enable;
}

/************* T型曲线初始化 *************/
static void T_Profile_Init(long steps, float start_freq, float max_freq, float accel)
{
    float accel_time, decel_time;
    
    // 基本参数初始化
    t_profile.total_steps = steps;
    t_profile.start_freq = start_freq; //起始频率(Hz)
    t_profile.max_freq = max_freq;     //最大频率
    t_profile.acceleration = accel;    //加速度
    t_profile.current_step = 0;
    t_profile.current_freq = start_freq;
    t_profile.state = ACCEL;
    
    // 1. 计算到达最大频率所需时间
    accel_time = (max_freq - start_freq) / accel;
    
    // 2. 计算加速段段步数（梯形面积公式）
    t_profile.accel_steps = (u32)((start_freq + max_freq) * accel_time / 2);
    
    // 3. 减速段使用与加速段相同的加速度
    decel_time = (max_freq - start_freq) / accel;
    t_profile.decel_steps = (u32)((start_freq + max_freq) * decel_time / 2);
    
    // 4. 检查总步数是否足够
    if (t_profile.accel_steps + t_profile.decel_steps > steps) {
        // 调整加速和减速步数
        float reduction_ratio = (float)steps / (t_profile.accel_steps + t_profile.decel_steps);
        t_profile.accel_steps = (u32)(t_profile.accel_steps * reduction_ratio);
        t_profile.decel_steps = (u32)(t_profile.decel_steps * reduction_ratio);
    }
    
    // 5.计算匀速段步数
    t_profile.const_steps = steps - t_profile.accel_steps - t_profile.decel_steps;
    
    // 6. 计算每步的频率变化量
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

/********************************************
// 相对定位函数 
*********************************************/
void Locate_Rle(long num, u32 frequency, DIR_Type dir)
{
    u8 wakeup_pulses;
    int i;
    long steps;
    
    // 使能驱动器
    DRIVER_OE = 0;
    delay_us(100);
    
    // 发送唤醒脉冲
    wakeup_pulses = WAKEUP_PULSES;
    for (i = 0; i < wakeup_pulses; i++) {
        DRIVER_DIR = dir;
        delay_us(50);
        GPIO_SetBits(GPIOC, GPIO_Pin_7);
        delay_us(5);
        GPIO_ResetBits(GPIOC, GPIO_Pin_7);
        delay_us(100);
    }
    
    steps = num;

    // 参数检查
    if (steps <= 0) return;
    if (TIM8->CR1 & 0x01) return;
    if (frequency < MIN_FREQUENCY || frequency > MAX_FREQUENCY) return;

    // 设置方向
    motor_dir = dir;
    DRIVER_DIR = motor_dir;
    
    // 计算目标位置
    if (motor_dir == CW) {
        target_pos = current_pos + steps;
    } else {
        target_pos = current_pos - steps;
    }
    
    // 初始化速度曲线
    if (is_accel_enabled) {
        T_Profile_Init(steps, MIN_FREQUENCY, frequency, t_profile.acceleration);
        TIM8_Startup((u32)t_profile.start_freq);
    } else {
        rcr_integer = steps / (RCR_VAL + 1);
        rcr_remainder = steps % (RCR_VAL + 1);
        TIM8_Startup(frequency);
    }
    
    // 启动运动
    is_rcr_finish = 0;
}

/********************************************
// 绝对定位函数
*********************************************/
void Locate_Abs(long num, u32 frequency)
{
    u8 wakeup_pulses;
    int i;
    long steps;
    
    // 检查定时器是否忙
    if (TIM8->CR1 & 0x01) return;
    if (frequency < MIN_FREQUENCY || frequency > MAX_FREQUENCY) return;
    
    // 计算目标位置
    target_pos = num;
    steps = labs(target_pos - current_pos);  // ??labs()?????
    
    if (steps == 0) return;
    
    // 设置方向
    if (target_pos > current_pos) {
        motor_dir = CW;
    } else {
        motor_dir = CCW;
    }
    DRIVER_DIR = motor_dir;
    
    // 使能驱动器
    DRIVER_OE = 0;
    delay_us(100);
    
    // 发送唤醒脉冲
    wakeup_pulses = WAKEUP_PULSES;
    for (i = 0; i < wakeup_pulses; i++) {
        DRIVER_DIR = motor_dir;
        delay_us(50);
        GPIO_SetBits(GPIOC, GPIO_Pin_7);
        delay_us(5);
        GPIO_ResetBits(GPIOC, GPIO_Pin_7);
        delay_us(100);
    }
    
    // 初始化速度曲线
    if (is_accel_enabled) {
        T_Profile_Init(steps, MIN_FREQUENCY, frequency, t_profile.acceleration);
        TIM8_Startup((u32)t_profile.start_freq);
    } else {
        rcr_integer = steps / (RCR_VAL + 1);
        rcr_remainder = steps % (RCR_VAL + 1);
        TIM8_Startup(frequency);
    }
    
    // 启动运动
    is_rcr_finish = 0;
}

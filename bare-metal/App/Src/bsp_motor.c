/**
 ******************************************************************************
 * @file    bsp_motor.c
 * @brief   底盘直流电机实现
 *
 * 迁移自 code/DC_MOTOR.c。改动:
 *
 *   1. DC_MOTOR_GPIO_Config() 删除
 *      —— 它内部是 TIM4 的时钟/时基/四路 OC 配置 + GPIOC 方向脚配置,全是标准库 API。
 *         迁移后由 CubeMX 的 MX_TIM4_Init() 与 MX_GPIO_Init() 完成
 *         (你已核对:PSC=71 / ARR=999 / ARR preload=Enable / 四路均为 PWM mode 1)。
 *      保留 bsp_motor_init() —— HAL 的 PWM 输出是**通道粒度**启动的,要启 4 次。
 *
 *   2. TIM_SetCompare1~4(TIM4, n) -> __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_x, n)
 *
 *   3. 方向宏 INA1~INB8 内部由 GPIO_Set/ResetBits 改为 HAL_GPIO_WritePin
 *      (宏定义在 bsp_motor.h;**调用点的写法一处都不用改**)。
 *
 *   4. 删除了 PWM_Control() 与 Analog_stick_model() —— 它们全工程无调用点,
 *      且其功能已被 main.c 里的 app_ps2_deal() 取代(死代码)。
 *      正本保留在 主控代码/code/DC_MOTOR.c 供对照。
 *
 *   5. Car_* 这一组整车动作函数**逐字保留**(它们是底盘的动作集,自动模式以后可能用)。
 ******************************************************************************
 */

#include "bsp_motor.h"
#include "tim.h"       /* extern TIM_HandleTypeDef htim4 */

void bsp_motor_init(void)
{
    /* HAL 的 PWM 启动是通道粒度的 —— 四路就得启四次 */
    (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
    (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
    (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
}

/**
 * @brief  设置四路 PWM 占空比(四轮同速)
 * @param  num 比较值 0~999
 * @note   对应原 DC_MOTOR_PWM_Config()。
 */
void DC_MOTOR_PWM_Config(int num)
{
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, (uint32_t)num);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, (uint32_t)num);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, (uint32_t)num);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, (uint32_t)num);
}

/* ===========================================================================
 * 单个电机的方向控制(仅本文件内部使用,故声明为 static)
 * 注意:电机驱动是"一高一低"决定转向,两脚同电平 = 刹车/停止
 * =========================================================================== */

/* 左前:INA1(PC0) / INB2(PC1) */
static void MOTOR_ZQ(char state)
{
    if (state == GO)   { INA1(0); INB2(1); }
    if (state == BACK) { INA1(1); INB2(0); }
    if (state == STOP) { INA1(0); INB2(0); }
}

/* 右前:INA3(PC2) / INB4(PC3) */
static void MOTOR_YQ(char state)
{
    if (state == GO)   { INA3(0); INB4(1); }
    if (state == BACK) { INA3(1); INB4(0); }
    if (state == STOP) { INA3(0); INB4(0); }
}

/* 左后:INA5(PC4) / INB6(PC5) */
static void MOTOR_ZH(char state)
{
    if (state == GO)   { INA5(0); INB6(1); }
    if (state == BACK) { INA5(1); INB6(0); }
    if (state == STOP) { INA5(0); INB6(0); }
}

/* 右后:INA7(PC8) / INB8(PC9) */
static void MOTOR_YH(char state)
{
    if (state == GO)   { INA7(0); INB8(1); }
    if (state == BACK) { INA7(1); INB8(0); }
    if (state == STOP) { INA7(0); INB8(0); }
}

/* ===========================================================================
 * 整车动作(与原工程逐字一致)
 * =========================================================================== */

void Car_Go(void)          { MOTOR_ZQ(GO);   MOTOR_YQ(GO);   MOTOR_ZH(GO);   MOTOR_YH(GO);   }
void Car_Back(void)        { MOTOR_ZQ(BACK); MOTOR_YQ(BACK); MOTOR_ZH(BACK); MOTOR_YH(BACK); }
void Car_Right(void)       { MOTOR_ZQ(GO);   MOTOR_YQ(BACK); MOTOR_ZH(BACK); MOTOR_YH(GO);   }
void Car_Left(void)        { MOTOR_ZQ(BACK); MOTOR_YQ(GO);   MOTOR_ZH(GO);   MOTOR_YH(BACK); }
void Car_Turn_Right(void)  { MOTOR_ZQ(GO);   MOTOR_YQ(BACK); MOTOR_ZH(GO);   MOTOR_YH(BACK); }
void Car_Turn_Left(void)   { MOTOR_ZQ(BACK); MOTOR_YQ(GO);   MOTOR_ZH(BACK); MOTOR_YH(GO);   }

/* 停止:原工程只停了左前/右前两轮(后两轮被注释掉了),此处**忠实保留原行为** */
void Car_Stop(void)
{
    MOTOR_ZQ(STOP);
    MOTOR_YQ(STOP);
    /* MOTOR_ZH(STOP);  MOTOR_YH(STOP);   ← 原工程注释掉了 */
}

void Car_Go_Right(void)    { MOTOR_ZQ(GO);   MOTOR_YQ(STOP); MOTOR_ZH(STOP); MOTOR_YH(GO);   }
void Car_Go_Left(void)     { MOTOR_ZQ(STOP); MOTOR_YQ(GO);   MOTOR_ZH(GO);   MOTOR_YH(STOP); }
void Car_Back_Left(void)   { MOTOR_ZQ(BACK); MOTOR_YQ(STOP); MOTOR_ZH(STOP); MOTOR_YH(BACK); }
void Car_Back_Right(void)  { MOTOR_ZQ(STOP); MOTOR_YQ(BACK); MOTOR_ZH(BACK); MOTOR_YH(STOP); }
void Car_Left_Right(void)  { MOTOR_ZQ(GO);   MOTOR_YQ(BACK); MOTOR_ZH(STOP); MOTOR_YH(STOP); }
void Car_Right_Left(void)  { MOTOR_ZQ(BACK); MOTOR_YQ(GO);   MOTOR_ZH(STOP); MOTOR_YH(STOP); }
void Car_RRight(void)      { MOTOR_ZQ(GO);   MOTOR_YQ(BACK); MOTOR_ZH(GO);   MOTOR_YH(BACK); }

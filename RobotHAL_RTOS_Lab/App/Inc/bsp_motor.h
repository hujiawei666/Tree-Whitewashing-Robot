/**
 ******************************************************************************
 * @file    bsp_motor.h
 * @brief   底盘直流电机 ×4(方向 GPIO + TIM4 四路 PWM)
 *
 * 迁移自 code/DC_MOTOR.h / DC_MOTOR.c。
 *
 * 硬件对应:
 *   TIM4 CH1~CH4 -> PB6/PB7/PB8/PB9  四路 PWM(调速)
 *   方向由 8 个普通 GPIO 控制,成对使用(INA/INB 一高一低决定转向)
 *     INA1/INB2 -> PC0/PC1  左前
 *     INA3/INB4 -> PC2/PC3  右前
 *     INA5/INB6 -> PC4/PC5  左后
 *     INA7/INB8 -> PC8/PC9  右后
 ******************************************************************************
 */
#ifndef __BSP_MOTOR_H
#define __BSP_MOTOR_H

#include "bsp_pin.h"

/* 方向参数(与原工程同名) */
#define GO    0
#define BACK  1
#define STOP  2

/*
 * 电机方向宏
 *
 * 原写法(标准库):
 *     #define INA1(a)  if (a) GPIO_SetBits(GPIOC,GPIO_Pin_0);\
 *                            GPIO_ResetBits(GPIOC,GPIO_Pin_0)
 *
 * 新写法(HAL):只把内部两个调用换掉,**宏的用法 `INA1(1)` 完全不变**。
 *
 * [!] 原宏有个隐患:它依赖 `if (a) 语句1; 语句2` 的悬空写法 ——
 *    如果调用点写成 `if (x) INA1(1); else ...`,那个 else 会绑到宏内部的 if 上。
 *    原工程都是当作独立语句用的(`INA1(0);`),所以没暴露。
 *    这里用 do{}while(0) 包起来把它修掉,调用点仍然不用改。
 */
#define INA1(a)  do { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, (a) ? GPIO_PIN_SET : GPIO_PIN_RESET); } while (0)
#define INB2(a)  do { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, (a) ? GPIO_PIN_SET : GPIO_PIN_RESET); } while (0)
#define INA3(a)  do { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, (a) ? GPIO_PIN_SET : GPIO_PIN_RESET); } while (0)
#define INB4(a)  do { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, (a) ? GPIO_PIN_SET : GPIO_PIN_RESET); } while (0)
#define INA5(a)  do { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, (a) ? GPIO_PIN_SET : GPIO_PIN_RESET); } while (0)
#define INB6(a)  do { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, (a) ? GPIO_PIN_SET : GPIO_PIN_RESET); } while (0)
#define INA7(a)  do { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, (a) ? GPIO_PIN_SET : GPIO_PIN_RESET); } while (0)
#define INB8(a)  do { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, (a) ? GPIO_PIN_SET : GPIO_PIN_RESET); } while (0)

/*
 * [!] 拼写陷阱(本步已经踩过一次,记在这里):
 *   标准库: GPIO_Pin_0   ← 小写 in
 *   HAL   : GPIO_PIN_0   ← 大写 IN
 *   两者是不同的标识符,HAL 里没有 GPIO_Pin_x,写错就是 24 个 "undefined" 错误。
 */

/*
 * [!][!] 已知原工程 BUG,本次**忠实照搬未修**,待 L2 上板验证时确认 [!][!]
 *
 *   上面 INB6 用的是 GPIOC Pin_4 —— 和 INA5 是**同一个引脚**。
 *   原工程 DC_MOTOR.h 就是这么写的。
 *
 *   后果:MOTOR_ZH(左后电机)的两个方向脚永远同电平,
 *         左后电机**只能停或按某一个固定方向转,无法换向**。
 *
 *   线索:DC_MOTOR_GPIO_Config() 把 PC0~PC5 全配成了输出,
 *         其中 **PC5 被配置了却没有任何宏去写它** —— 强烈暗示硬件上
 *         第 6 个方向脚接的是 PC5,这个 Pin_4 是复制粘贴笔误。
 *
 *   L2 验收时请专门测这一项:让左后电机正转、再反转,看方向会不会变。
 *   如果确实不变,把上面 INB6 的 Pin_4 改成 Pin_5 即可修复。
 */

/**
 * @brief  启动 TIM4 的四路 PWM 输出
 * @note   必须在 MX_TIM4_Init() 之后调用。
 *         HAL 的 PWM 启动是**通道粒度**的,所以这里要启动 4 次。
 */
void bsp_motor_init(void);

/** 设置四路 PWM 的占空比(四轮同速) */
void DC_MOTOR_PWM_Config(int num);

/* ---- 整车动作(与原工程同名,内部逻辑未改) ---- */
void Car_Go(void);
void Car_Back(void);
void Car_Right(void);
void Car_Left(void);
void Car_Stop(void);
void Car_Go_Left(void);
void Car_Go_Right(void);
void Car_Back_Left(void);
void Car_Back_Right(void);
void Car_Turn_Left(void);
void Car_Turn_Right(void);
void Car_Left_Right(void);
void Car_Right_Left(void);
void Car_RRight(void);

#endif /* __BSP_MOTOR_H */

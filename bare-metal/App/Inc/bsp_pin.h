/**
 ******************************************************************************
 * @file    bsp_pin.h
 * @brief   引脚别名与 Cortex-M 位带宏(集中版)
 *
 * 原工程里这些东西散落在 5 个头文件中:
 *   SYSTEM/sys/sys.h        —— 位带宏本体
 *   code/beep.h             —— BEEP
 *   HARDWARE/LED/led.h      —— LED0 / LED1
 *   HARDWARE/KEY/key.h      —— KEY0~KEY2 / WK_UP
 *   code/driver.h           —— DRIVER_DIR / DRIVER_OE
 *   code/hcsr04.h           —— 三路 TRIG
 * 这里集中到一处,避免迁移过程中到处找。
 *
 * 【为什么这一层在标准库→HAL 迁移中"一行都不用改"】
 *
 * 位带(Bit-Band)是 Cortex-M 内核的特性:把外设寄存器的**每一位**映射到
 * 别名区的一段地址上。通过别名地址读写某一位,等价于:
 *   - 不需要"读-改-写"三步(因此天然原子,不会被中断打断)
 *   - 一条 STR 指令直接写单比特
 *
 * 它只用到 **CMSIS 的寄存器基址宏(GPIOx_BASE)+ 地址运算**,不调用标准库任何函数,
 * 所以 StdPeriph 卸掉之后依然完全有效。
 *
 * 唯一注意:下面的 PAout/PEin 这类宏**只对单个 IO 有效**,且 n 必须小于 16。
 ******************************************************************************
 */
#ifndef __BSP_PIN_H
#define __BSP_PIN_H

#include <stdint.h>
#include "stm32f4xx_hal.h"   /* 提供 GPIOA_BASE..GPIOI_BASE 等 CMSIS 寄存器基址 */

/* ---------------------------------------------------------------------------
 * 原工程的类型简写
 * StdPeriph 的 stm32f4xx.h 里带这些 typedef,HAL 头文件没有。
 * 补在这里,让迁移过来的代码不必把所有 u8 改成 uint8_t(减少无谓改动)。
 * 新写的代码建议直接用 uint8_t/uint16_t/uint32_t —— 那是标准的写法。
 * ------------------------------------------------------------------------ */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;

/* ---------------------------------------------------------------------------
 * 位带地址运算
 * ------------------------------------------------------------------------ */
#define BITBAND(addr, bitnum)   ((addr & 0xF0000000) + 0x2000000 + ((addr & 0xFFFFF) << 5) + (bitnum << 2))
#define MEM_ADDR(addr)          *((volatile unsigned long *)(addr))
#define BIT_ADDR(addr, bitnum)  MEM_ADDR(BITBAND(addr, bitnum))

/* 输出寄存器(ODR)与输入寄存器(IDR)的地址:GPIOx_BASE + 偏移 */
#define GPIOA_ODR_Addr  (GPIOA_BASE + 20)
#define GPIOB_ODR_Addr  (GPIOB_BASE + 20)
#define GPIOC_ODR_Addr  (GPIOC_BASE + 20)
#define GPIOD_ODR_Addr  (GPIOD_BASE + 20)
#define GPIOE_ODR_Addr  (GPIOE_BASE + 20)
#define GPIOF_ODR_Addr  (GPIOF_BASE + 20)
#define GPIOG_ODR_Addr  (GPIOG_BASE + 20)
#define GPIOH_ODR_Addr  (GPIOH_BASE + 20)
#define GPIOI_ODR_Addr  (GPIOI_BASE + 20)

#define GPIOA_IDR_Addr  (GPIOA_BASE + 16)
#define GPIOB_IDR_Addr  (GPIOB_BASE + 16)
#define GPIOC_IDR_Addr  (GPIOC_BASE + 16)
#define GPIOD_IDR_Addr  (GPIOD_BASE + 16)
#define GPIOE_IDR_Addr  (GPIOE_BASE + 16)
#define GPIOF_IDR_Addr  (GPIOF_BASE + 16)
#define GPIOG_IDR_Addr  (GPIOG_BASE + 16)
#define GPIOH_IDR_Addr  (GPIOH_BASE + 16)
#define GPIOI_IDR_Addr  (GPIOI_BASE + 16)

/* 单比特读写别名(n < 16) */
#define PAout(n)  BIT_ADDR(GPIOA_ODR_Addr, n)
#define PAin(n)   BIT_ADDR(GPIOA_IDR_Addr, n)
#define PBout(n)  BIT_ADDR(GPIOB_ODR_Addr, n)
#define PBin(n)   BIT_ADDR(GPIOB_IDR_Addr, n)
#define PCout(n)  BIT_ADDR(GPIOC_ODR_Addr, n)
#define PCin(n)   BIT_ADDR(GPIOC_IDR_Addr, n)
#define PDout(n)  BIT_ADDR(GPIOD_ODR_Addr, n)
#define PDin(n)   BIT_ADDR(GPIOD_IDR_Addr, n)
#define PEout(n)  BIT_ADDR(GPIOE_ODR_Addr, n)
#define PEin(n)   BIT_ADDR(GPIOE_IDR_Addr, n)
#define PFout(n)  BIT_ADDR(GPIOF_ODR_Addr, n)
#define PFin(n)   BIT_ADDR(GPIOF_IDR_Addr, n)
#define PGout(n)  BIT_ADDR(GPIOG_ODR_Addr, n)
#define PGin(n)   BIT_ADDR(GPIOG_IDR_Addr, n)
#define PHout(n)  BIT_ADDR(GPIOH_ODR_Addr, n)
#define PHin(n)   BIT_ADDR(GPIOH_IDR_Addr, n)
#define PIout(n)  BIT_ADDR(GPIOI_ODR_Addr, n)
#define PIin(n)   BIT_ADDR(GPIOI_IDR_Addr, n)

/* ---------------------------------------------------------------------------
 * 业务别名(引脚功能)
 * 引脚编号与原工程一致;CubeMX 生成的 XXX_Pin / XXX_GPIO_Port 宏在 main.h 里,
 * 两者指向同一个物理引脚,这里保留位带写法以做到"调用点零改动"。
 * ------------------------------------------------------------------------ */
#define BEEP             PBout(0)    /* PB0  蜂鸣器,高电平响   */
#define LED0             PBout(1)    /* PB1  指示灯0,低电平亮  */
#define LED1             PCout(13)   /* PC13 指示灯1,低电平亮  */

#define DRIVER_DIR       PEout(5)    /* PE5  步进驱动器方向     */
#define DRIVER_OE        PEout(6)    /* PE6  步进驱动器使能(低有效) */

#define Left_TRIG_Send   PBout(2)    /* PB2  左超声波 TRIG      */
#define Front_TRIG_Send  PBout(3)    /* PB3  前超声波 TRIG      */
#define Right_TRIG_Send  PBout(4)    /* PB4  右超声波 TRIG      */

#define KEY0             PEin(4)     /* PE4  按键0(上拉,按下为低) */
#define KEY2             PEin(2)     /* PE2  按键1(上拉,按下为低) */

/*
 * 说明:原 key.h 里还有 KEY1(PE3) 和 WK_UP(PA0),本工程中已废弃 ——
 *   PE3 被 driver.c 重配为"水泵"输出;
 *   PA0 被 time.c 重配为 TIM5_CH1(超声波 Echo1)。
 * 两者都不再作为按键使用,故此处不提供别名。
 */

#endif /* __BSP_PIN_H */

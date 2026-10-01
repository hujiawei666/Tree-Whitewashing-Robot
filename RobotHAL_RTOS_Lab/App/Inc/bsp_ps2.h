/**
 ******************************************************************************
 * @file    bsp_ps2.h
 * @brief   PS2 手柄驱动接口(软件位操作,非硬件 SPI)
 *
 * 迁移自 HARDWARE/PSTwo/pstwo.h。改动很小:
 *   - 删掉了 PS2_*_PIN/PORT/RCC 那组宏 —— 它们用的是标准库拼写(GPIO_Pin_12),
 *     而且只在已被删除的 PS2_Init() 里用到;
 *   - 位带别名 DI/DO_H/... 原样保留(加了一层括号);
 *   - PS2_Init() 更名为 bsp_ps2_init() —— 它不再配置 GPIO,只把三条线置到空闲态。
 ******************************************************************************
 */
#ifndef __BSP_PS2_H
#define __BSP_PS2_H

#include "bsp_pin.h"

/* ---- 位操作引脚 ----------------------------------------------------------
 * 物理引脚:PB12=CS  PB13=CLK  PB14=DO(主->柄)  PB15=DI(柄->主)
 * 注意这些宏**自带赋值**,用法是当作语句:  DO_H;  而不是  DO_H = 1;
 * ------------------------------------------------------------------------ */
#define DI     PBin(15)          /* 输入:手柄返回的数据位 */
#define DO_H   (PBout(14) = 1)   /* 输出拉高 */
#define DO_L   (PBout(14) = 0)   /* 输出拉低 */
#define CS_H   (PBout(12) = 1)   /* 片选拉高(空闲) */
#define CS_L   (PBout(12) = 0)   /* 片选拉低(选中) */
#define CLK_H  (PBout(13) = 1)   /* 时钟拉高 */
#define CLK_L  (PBout(13) = 0)   /* 时钟拉低 */

/* ---- 按键码 ------------------------------------------------------------- */
#define PSB_SELECT      1
#define PSB_L3          2
#define PSB_R3          3
#define PSB_START       4
#define PSB_PAD_UP      5
#define PSB_PAD_RIGHT   6
#define PSB_PAD_DOWN    7
#define PSB_PAD_LEFT    8
#define PSB_L2          9
#define PSB_R2          10
#define PSB_L1          11
#define PSB_R1          12
#define PSB_GREEN       13
#define PSB_RED         14
#define PSB_BLUE        15
#define PSB_PINK        16
#define PSB_TRIANGLE    13
#define PSB_CIRCLE      14
#define PSB_CROSS       15
#define PSB_SQUARE      16

/* ---- 摇杆索引 ----------------------------------------------------------- */
#define PSS_RX  5
#define PSS_RY  6
#define PSS_LX  7
#define PSS_LY  8

/* ---- 对外数据 ----------------------------------------------------------- */
extern u8  Data[9];      /* 最近一次读回的 9 字节帧 */
extern u16 MASK[20];     /* 按键位 -> 按键码 的映射表 */
extern u16 Handkey;      /* Data[4]<<8 | Data[3] 合成的按键位图 */

/* ---- 接口 --------------------------------------------------------------- */
/* 只把三条控制线置为空闲高电平;GPIO 模式由 CubeMX 的 MX_GPIO_Init() 配置 */
void bsp_ps2_init(void);

u8   PS2_RedLight(void);          /* 0=红绿灯模式(模拟), 1=普通模式 */
u8   PS2_ReadData(u8 command);    /* 与手柄交换一个字节 */
void PS2_Cmd(u8 CMD);             /* 发送一条命令 */
u8   PS2_DataKey(void);           /* 读按键,返回按键码;0 表示无按键 */
u8   PS2_AnologData(u8 button);   /* 读摇杆模拟量,0~255 */
void PS2_ClearData(void);         /* 清空数据缓冲 */

#endif /* __BSP_PS2_H */

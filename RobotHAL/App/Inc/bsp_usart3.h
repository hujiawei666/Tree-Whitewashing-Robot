/**
 ******************************************************************************
 * @file    bsp_usart3.h
 * @brief   USART3 —— 单向数据接收(当前只用来收,发送函数保留备用)
 *
 * 迁移自 code/usart3.c。全局变量沿用原名 USART3_RX_BUF / USART3_RX_STA,
 * main.c 里的解析代码一处都不用改。
 ******************************************************************************
 */
#ifndef __BSP_USART3_H
#define __BSP_USART3_H

#include "bsp_pin.h"

/* 接收缓冲区字节数(与原工程 usart3.h 同名同值) */
#define USART3_REC_LEN  200

extern u8  USART3_RX_BUF[USART3_REC_LEN];
extern u16 USART3_RX_STA;      /* 帧状态,位定义同 USART1 */

/**
 * @brief  启动 USART3 的接收中断
 */
void bsp_usart3_init(void);

/**
 * @brief  USART3 的接收处理(帧解析 + 重新上膛)
 * @note   不直接注册成 HAL 回调 —— HAL 的 HAL_UART_RxCpltCallback 是全串口共用的,
 *         只有一个函数能占用它。它放在 bsp_usart1.c 里,按 Instance 分派过来。
 */
void bsp_usart3_on_rx(void);

/* 发送(当前工程暂无调用点,保留作为串口的另一半) */
void USART3_SendChar(u8 ch);
void USART3_SendString(char *str);

#endif /* __BSP_USART3_H */

/**
 ******************************************************************************
 * @file    bsp_usart1.h
 * @brief   USART1 —— 串口屏(HMI)的双向链路
 *
 * 角色说明(从原工程的实际调用关系看出来的):
 *   - 发送:HMI_send_string() / HMI_send_number() 内部用 printf(),
 *          而 printf 经 fputc 重定向到 USART1。所以**串口屏接在 USART1 上**。
 *   - 接收:main.c 里解析 USART_RX_STA / USART_RX_BUF,
 *          识别 '1'~'9' / '@' / 'A' / '2' 等模式切换指令。
 *
 * 全局变量沿用原工程的名字(USART_RX_BUF / USART_RX_STA),这样 main.c 迁移时
 * 调用点一处都不用改。注意它们没有 "1" 后缀 —— 那是原工程的命名。
 ******************************************************************************
 */
#ifndef __BSP_USART1_H
#define __BSP_USART1_H

#include "bsp_pin.h"

/* 接收缓冲区字节数(与原工程 usart.h 同名同值) */
#define USART_REC_LEN   200

extern u8  USART_RX_BUF[USART_REC_LEN];   /* 收到的原始字节 */
extern u16 USART_RX_STA;                  /* 帧状态:
                                           *   bit15 = 1  一帧接收完成
                                           *   bit14 = 1  已收到 0x0D
                                           *   bit13~0    当前已收字节数
                                           */

/**
 * @brief  启动 USART1 的接收中断
 * @note   必须在 MX_USART1_UART_Init() 之后调用。
 *         HAL 是在这里"上膛"的 —— 不调用它就一个字节都收不到。
 */
void bsp_usart1_init(void);

#endif /* __BSP_USART1_H */

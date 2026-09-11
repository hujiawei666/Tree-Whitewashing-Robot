#ifndef __USART2_H
#define __USART2_H

#include "stm32f4xx.h"
#include "sys.h"

// 配置参数
#define USART2_REC_LEN         200     // 最大接收字节数
#define EN_USART2_RX           1       // 使能(1)/禁止(0)接收功能

// 接收状态寄存器说明:
// bit15 - 接收完成标志 (1:完成)
// bit14 - 接收到0x0D标志 (回车)
// bit13~0 - 已接收的有效字节数
extern u16 USART2_RX_STA;              // 接收状态标记
extern u8 USART2_RX_BUF[USART2_REC_LEN]; // 接收缓冲区

// 函数声明
void uart2_init(u32 bound);  // 初始化串口2
void USART2_SendString(const char *);  // 添加const

#endif

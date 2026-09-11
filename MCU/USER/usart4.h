#ifndef __USART4_H  // 修改头文件保护
#define __USART4_H

#include "stm32f4xx.h"
#include "sys.h"

#define USART4_REC_LEN  20      // 接收缓冲区长度  // 修改宏定义
#define EN_USART4_RX    1       // 使能接收  // 修改宏定义

extern u8 USART4_RX_BUF[USART4_REC_LEN];  // 修改缓冲区名
extern u16 USART4_RX_STA;  // 修改状态标志名

void uart4_init(u32 bound);  // 修改函数名
void USART4_SendChar(u8 ch);  // 修改函数名
void USART4_SendString(char* str);  // 修改函数名

#endif


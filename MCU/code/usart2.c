#include "usart2.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// 接收缓冲区与状态变量
#if EN_USART2_RX
u16 USART2_RX_STA = 0;                // 接收状态标记
u8 USART2_RX_BUF[USART2_REC_LEN];     // 接收缓冲区
#endif

// ======================================================================
// 串口2基础功能实现
// ======================================================================

/**
  * @brief  初始化串口2
  * @param  bound: 波特率 (如9600, 115200)
  * @retval 无
  */
void uart2_init(u32 bound) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. 使能时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);    // GPIOA时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);   // USART2时钟(APB1)

    // 2. 配置GPIO引脚复用功能
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);  // PA2复用为USART2_TX
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);  // PA3复用为USART2_RX

    // 3. 初始化GPIO参数
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;   // PA2(TX), PA3(RX)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;             // 复用功能
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;        // 速度50MHz
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;           // 推挽输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;             // 上拉模式
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 配置串口参数
    USART_InitStructure.USART_BaudRate = bound;              // 波特率设置
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; // 8位数据
    USART_InitStructure.USART_StopBits = USART_StopBits_1;   // 1位停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;      // 无校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 收发模式
    USART_Init(USART2, &USART_InitStructure);

    // 5. 使能串口
    USART_Cmd(USART2, ENABLE);

#if EN_USART2_RX
    // 6. 配置接收中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);  // 使能接收中断

    // 7. 配置中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;         // USART2中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; // 抢占优先级3
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        // 子优先级3
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           // 使能中断
    NVIC_Init(&NVIC_InitStructure);
#endif
}

#if EN_USART2_RX
/**
  * @brief  USART2中断服务函数
  * @retval 无
  */
void USART2_IRQHandler(void) {
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        u8 Res = USART_ReceiveData(USART2);  // 读取接收数据
        
        // 状态机处理接收数据
        if ((USART2_RX_STA & 0x8000) == 0) {  // 未完成接收
            if (USART2_RX_STA & 0x4000) {     // 已收到0x0D(回车)
                if (Res != 0x0A) {             // 不是0x0A(换行)
                    USART2_RX_STA = 0;         // 接收错误，重置
                } else {
                    USART2_RX_STA |= 0x8000;   // 标记接收完成
                }
            } else {                           // 未收到0x0D
                if (Res == 0x0D) {
                    USART2_RX_STA |= 0x4000;   // 标记收到回车
                } else {
                    // 存储数据并更新计数器
                    if ((USART2_RX_STA & 0x3FFF) < USART2_REC_LEN) {
                        USART2_RX_BUF[USART2_RX_STA & 0x3FFF] = Res;
                        USART2_RX_STA++;
                    } else {
                        USART2_RX_STA |= 0x8000; // 强制标记完成(缓冲区满)
                    }
                }
            }
        }
    }
}
#endif


/**
  * @brief  发送单个字符到USART2
  * @param  ch: 要发送的字符
  * @retval 无
  */
static void USART2_SendChar(char ch) {
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET); // 等待发送就绪
    USART_SendData(USART2, (uint8_t)ch);
}

/**
  * @brief  发送字符串到USART2
  * @param  str: 要发送的字符串
  * @retval 无
  */
void USART2_SendString(const char* str) {    while (*str) {
        USART2_SendChar(*str++);
    }
}


#include "sys.h"         // 系统配置头文件（时钟、延时等）
#include "usart3.h"      // USART3模块自定义头文件

#if EN_USART3_RX           // 条件编译：如果启用USART3接收功能
u8 USART3_RX_BUF[USART3_REC_LEN];  // 接收缓冲区（长度由USART3_REC_LEN定义）
u16 USART3_RX_STA = 0;     // 接收状态寄存器（位定义见下方说明）
#endif

// USART3初始化函数
// 参数bound：波特率（如115200）
void uart3_init(u32 bound) {
    GPIO_InitTypeDef GPIO_InitStructure;   // GPIO配置结构体
    USART_InitTypeDef USART_InitStructure; // USART配置结构体
    NVIC_InitTypeDef NVIC_InitStructure;   // 中断配置结构体

    // 1. 使能时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);  // 使能GPIOB时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE); // 使能USART3时钟

    // 2. 引脚复用配置
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3); // PB10复用为USART3_TX
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3); // PB11复用为USART3_RX

    // 3. GPIO参数配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11; // 操作PB10和PB11
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;             // 复用模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;        // 高速模式
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;           // 推挽输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;             // 上拉模式
    GPIO_Init(GPIOB, &GPIO_InitStructure);                   // 应用配置

    // 4. USART参数配置
    USART_InitStructure.USART_BaudRate = bound;              // 设置波特率
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; // 8位数据位
    USART_InitStructure.USART_StopBits = USART_StopBits_1;   // 1位停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;      // 无校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 使能收发模式
    USART_Init(USART3, &USART_InitStructure);                // 应用配置

    // 5. 使能USART3
    USART_Cmd(USART3, ENABLE);  // 启动USART3

#if EN_USART3_RX
    // 6. 使能接收中断
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); // 使能RXNE（接收缓冲区非空）中断

    // 7. 配置USART3中断
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn; // 选择USART3中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; // 抢占优先级2
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;        // 子优先级2
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           // 使能中断通道
    NVIC_Init(&NVIC_InitStructure);                           // 应用配置
#endif
}

#if EN_USART3_RX
// USART3中断服务函数
void USART3_IRQHandler(void) {
    u8 Res; // 临时存储接收的字节
    
    // 检测是否发生RXNE中断（接收数据寄存器非空）
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
        Res = USART_ReceiveData(USART3); // 读取接收到的数据
        
        // 状态寄存器说明：
        // bit15: 0x8000 -> 完成接收标志
        // bit14: 0x4000 -> 接收到0x0D（回车符）
        // bit13~0: 接收字节计数器（最大14位，对应USART3_REC_LEN-1）
        if ((USART3_RX_STA & 0x8000) == 0) { // 如果未完成接收
            
            if (Res == 0x0D) { // 接收到回车符\r
                USART3_RX_STA |= 0x4000; // 设置0x0D接收标志
            }
            else if (Res == 0x0A) { // 接收到换行符\n
                // 若已收到0x0D（回车符）
                if (USART3_RX_STA & 0x4000) {
                    USART3_RX_STA |= 0x8000; // 设置完成接收标志（同时保留0x4000标志）
                }
                // 若未收到0x0D直接收到0x0A（异常情况）
                else {
                    USART3_RX_STA |= 0x8000; // 强制结束接收（异常处理）
                }
            }
            else { // 接收到普通数据
                // 检查缓冲区是否未满
                if ((USART3_RX_STA & 0x3FFF) < (USART3_REC_LEN - 1)) {
                    // 将数据存入缓冲区（低14位作为索引）
                    USART3_RX_BUF[USART3_RX_STA & 0x3FFF] = Res;
                    USART3_RX_STA++; // 计数器加1
                    
                    // 清除0x0D标志（因为收到新数据）
                    USART3_RX_STA &= ~0x4000;
                } 
                else { // 缓冲区已满
                    USART3_RX_STA |= 0x8000; // 强制结束接收
                }
            }
        }
    }
}
#endif

// 发送单个字符
void USART3_SendChar(u8 ch) {
    while ((USART3->SR & USART_SR_TXE) == 0); // 等待发送缓冲区空（TXE=1）
    USART3->DR = ch; // 写入数据寄存器
}

// 发送字符串
void USART3_SendString(char* str) {
    while (*str) { // 遍历字符串直到结束符'\0'
        USART3_SendChar(*str++); // 逐字符发送
    }
}

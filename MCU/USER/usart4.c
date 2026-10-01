#include "sys.h"
#include "usart4.h"  // 修改头文件名

#if EN_USART4_RX  // 修改宏定义
u8 USART4_RX_BUF[USART4_REC_LEN];     // 接收缓冲区
u16 USART4_RX_STA = 0;                // 接收状态标志
#endif

void uart4_init(u32 bound) {  // 修改函数名
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    // 1. 使能时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);  // 改为GPIOC
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);  // 改为UART4
    
    // 2. 引脚复用
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_UART4); // PC10 = TX  // 改为UART4
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_UART4); // PC11 = RX  // 改为UART4
    
    // 3. GPIO初始化
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;  // 修改引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);  // 改为GPIOC
    
    // 4. USART初始化
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(UART4, &USART_InitStructure);  // 改为UART4
    
    // 5. 使能USART
    USART_Cmd(UART4, ENABLE);  // 改为UART4
    
#if EN_USART4_RX  // 修改宏定义
    // 6. 使能接收中断
    USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);  // 改为UART4
    
    // 7. 中断配置
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;  // 改为UART4中断
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
#endif
}

#if EN_USART4_RX  // 修改宏定义
void UART4_IRQHandler(void) {  // 修改中断处理函数名
    static u8 rx_count = 0; // 临时接收计数器
    
    if (USART_GetITStatus(UART4, USART_IT_RXNE) != RESET) {  // 改为UART4
        u8 Res = USART_ReceiveData(UART4);  // 改为UART4
        
        // 帧头检测
        if (rx_count == 0 && Res != 0xAA) return;
        if (rx_count == 1 && Res != 0xAA) {
            rx_count = 0;
            return;
        }
        
        // 存储数据
        if (rx_count < USART4_REC_LEN) {  // 修改缓冲区长度
            USART4_RX_BUF[rx_count] = Res;  // 修改缓冲区名
        }
        
        rx_count++;
        
        // 重置数据包接收
        if (rx_count >= USART4_REC_LEN) {  // 修改缓冲区长度
            USART4_RX_STA |= 0x8000; // 设置接收完成标志
            rx_count = 0; // 重置计数器
        }
    }
}
#endif

// 发送字符
void USART4_SendChar(u8 ch) {  // 修改函数名
    while ((UART4->SR & USART_SR_TXE) == 0); // 等待发送完成
    UART4->DR = ch;  // 改为UART4
}

// 发送字符串
void USART4_SendString(char* str) {  // 修改函数名
    while (*str) {
        USART4_SendChar(*str++);  // 改为调用USART4发送
    }
}

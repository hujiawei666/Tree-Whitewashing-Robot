/**
 ******************************************************************************
 * @file    bsp_usart3.c
 * @brief   USART3 收发
 *
 * 迁移自 code/usart3.c。改动:
 ******************************************************************************
 */

#include "bsp_usart3.h"
#include "usart.h"       /* extern UART_HandleTypeDef huart3 */

/* ---- 接收缓冲与状态 ---- */
u8  USART3_RX_BUF[USART3_REC_LEN];
u16 USART3_RX_STA = 0;

/* HAL 把收到的字节放这里 */
static uint8_t s_usart3_rx_byte;

void bsp_usart3_init(void)
{
    (void)HAL_UART_Receive_IT(&huart3, &s_usart3_rx_byte, 1U);
}

/* ===========================================================================
 * 接收处理(帧解析)
 * =========================================================================== */
void bsp_usart3_on_rx(void)
{
    u8 Res = s_usart3_rx_byte;

    /* ---- 原 USART3_IRQHandler 的逻辑,逐字保留 ---- */
    if ((USART3_RX_STA & 0x8000) == 0) {          /* 本帧尚未完成 */

        if (Res == 0x0D) {                        /* 收到回车 \r */
            USART3_RX_STA |= 0x4000;
        }
        else if (Res == 0x0A) {                   /* 收到换行 \n */
            if (USART3_RX_STA & 0x4000) {
                USART3_RX_STA |= 0x8000;          /* \r\n 齐全,正常结束 */
            } else {
                USART3_RX_STA |= 0x8000;          /* 没等到 \r 就来 \n,异常但强行结束 */
            }
        }
        else {                                    /* 普通数据字节 */
            if ((USART3_RX_STA & 0x3FFF) < (USART3_REC_LEN - 1)) {
                USART3_RX_BUF[USART3_RX_STA & 0x3FFF] = Res;
                USART3_RX_STA++;
                USART3_RX_STA &= ~0x4000;         /* 有正常数据,清掉 \r 标志 */
            } else {
                USART3_RX_STA |= 0x8000;          /* 缓冲满,强行结束 */
            }
        }
    }
    /* ---- 解析结束 ---- */

    /* 重新上膛 —— 同 USART1,不重新调用就只能收到一个字节 */
    (void)HAL_UART_Receive_IT(&huart3, &s_usart3_rx_byte, 1U);
}

/* ===========================================================================
 * 发送
 * =========================================================================== */
void USART3_SendChar(u8 ch)
{
    (void)HAL_UART_Transmit(&huart3, &ch, 1U, 100U);
}

void USART3_SendString(char *str)
{
    while (*str) {
        USART3_SendChar((u8)(*str++));
    }
}

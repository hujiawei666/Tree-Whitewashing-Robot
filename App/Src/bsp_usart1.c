/**
 ******************************************************************************
 * @file    bsp_usart1.c
 * @brief   USART1 收发 —— 串口屏(HMI)链路 + printf 重定向
 *
 ******************************************************************************
 */

#include "bsp_usart1.h"
#include "bsp_usart3.h"
#include "usart.h"        /* CubeMX 生成:extern UART_HandleTypeDef huart1 / huart3 */
#include <stdio.h>

/* ===========================================================================
 * 一、printf 重定向到 USART1
 * ===========================================================================
 * =========================================================================== */
#pragma import(__use_no_semihosting)

struct __FILE {
    int handle;
};

FILE __stdout;

void _sys_exit(int x)
{
    x = x;
}

int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;
    (void)HAL_UART_Transmit(&huart1, &c, 1U, 100U);
    return ch;
}

/* ===========================================================================
 * 二、接收缓冲与状态
 * =========================================================================== */
u8  USART_RX_BUF[USART_REC_LEN];
u16 USART_RX_STA = 0;

/* HAL 把收到的字节放这里,再由回调搬进 USART_RX_BUF */
static uint8_t s_usart1_rx_byte;

/* ===========================================================================
 * 三、接收上膛
 * =========================================================================== */
void bsp_usart1_init(void)
{
    /* 告诉 HAL:收满 1 个字节就调用一次 HAL_UART_RxCpltCallback */
    (void)HAL_UART_Receive_IT(&huart1, &s_usart1_rx_byte, 1U);
}

/* ===========================================================================
 * 四、接收完成回调(HAL 对所有串口共用这一个回调,所以要按 Instance 分派)
 * =========================================================================== */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {

        u8 Res = s_usart1_rx_byte;

        /* ---- 原 USART1_IRQHandler 的帧解析逻辑,逐字保留 ---- */
        if ((USART_RX_STA & 0x8000) == 0) {           /* 本帧尚未接收完成 */
            if (USART_RX_STA & 0x4000) {              /* 已经收到 0x0D */
                if (Res != 0x0a)
                    USART_RX_STA = 0;                 /* 收到的不是 0x0A,作废重来 */
                else
                    USART_RX_STA |= 0x8000;           /* \r\n 齐全,一帧完成 */
            } else {                                  /* 还没收到 0x0D */
                if (Res == 0x0d) {
                    USART_RX_STA |= 0x4000;
                } else {
                    USART_RX_BUF[USART_RX_STA & 0X3FFF] = Res;
                    USART_RX_STA++;
                    if (USART_RX_STA > (USART_REC_LEN - 1))
                        USART_RX_STA = 0;             /* 溢出,丢弃重来 */
                }
            }
        }
        /* ---- 解析结束 ---- */

        /* [!] 关键:重新上膛。HAL_UART_Receive_IT 是一次性的,
         *    收满就自动关闭。不重新调用,以后再也收不到字节。 */
        (void)HAL_UART_Receive_IT(&huart1, &s_usart1_rx_byte, 1U);

    } else if (huart->Instance == USART3) {
        /* USART3 的帧解析在 bsp_usart3.c 里(HAL 回调是共用的,只能有一个) */
        bsp_usart3_on_rx();
    }
}

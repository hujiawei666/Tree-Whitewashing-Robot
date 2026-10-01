/**
 ******************************************************************************
 * @file    bsp_usart1.c
 * @brief   USART1 收发 —— 串口屏(HMI)链路 + printf 重定向
 *
 * 迁移自 SYSTEM/usart/usart.c。改动集中在两处:
 *
 *   1. uart_init() 删除
 *      原来在这里做 RCC/GPIO/USART_Init/NVIC(全是标准库 API)。
 *      迁移后由 CubeMX 的 MX_USART1_UART_Init() 完成。
 *      但 HAL 的中断接收需要"上膛",所以保留了 bsp_usart1_init() 做这件事。
 *
 *   2. USART1_IRQHandler -> HAL_UART_RxCpltCallback
 *      原版在中断里自己查标志、读数据;HAL 把这些包了,
 *      只把"一个字节收到了"这件事通过回调告诉你。
 *      代价是:**必须自己在回调里重新上膛**,否则只能收到一个字节。
 *
 * 帧解析逻辑(0x0D 后跟 0x0A 才算完整帧)**逐字保留**。
 ******************************************************************************
 */

#include "bsp_usart1.h"
#include "bsp_usart3.h"
#include "usart.h"        /* CubeMX 生成:extern UART_HandleTypeDef huart1 / huart3 */
#include <stdio.h>

/* ===========================================================================
 * 一、printf 重定向到 USART1
 * ===========================================================================
 * 和原版一样用"不依赖半主机"的方式。区别只有 fputc 内部:
 *   原版  : while(USART1->SR & TXE == 0); USART1->DR = ch;   （直接操作寄存器）
 *   新版  : HAL_UART_Transmit(&huart1, &c, 1, 超时);          （走 HAL）
 *
 * 为什么用 HAL 而不是继续直接写寄存器:
 *   统一走 HAL 的状态机。HAL 的发送用 gState、中断接收用 RxState,两者互不干扰,
 *   可以同时进行 —— 所以"printf 发送"和"接收中断"不会打架。
 *
 * [!] 超时用有限值而不是 HAL_MAX_DELAY:
 *   串口屏拔掉时,发送永远等不到 TXE,用 HAL_MAX_DELAY 会把整个程序**永久卡死**
 *   在 printf 里。给 100ms 上限,最坏情况丢一行调试输出,不至于死机。
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

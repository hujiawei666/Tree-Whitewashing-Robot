/**
 ******************************************************************************
 * @file    bsp_ps2.c
 * @brief   PS2 手柄驱动(软件位操作)
 *
 * 迁移自 HARDWARE/PSTwo/pstwo.c。相对原版的**全部改动**只有三处:
 *
 *   1. PS2_Init() -> bsp_ps2_init()
 *      原版在这里做 RCC 时钟使能 + GPIO_Init(标准库 API)。
 *      迁移到 HAL 后,这些由 CubeMX 生成的 MX_GPIO_Init() 统一完成,
 *      所以本函数只剩"把三条控制线置到空闲高电平"这一件事。
 *
 *   2. 头文件: pstwo.h -> bsp_ps2.h,并改包含新的 delay.h。
 *      原版还包含了 usart.h,但代码里并没有用到,顺带去掉。
 *
 *   3. 删除了文件末尾的两大段注释掉的旧实现(死代码),保持文件干净。
 *      正本仍完整保留在 主控代码/HARDWARE/PSTwo/pstwo.c 供对照。
 *
 * 其余逻辑(时序、位操作、按键映射)**逐字未动**。
 ******************************************************************************
 */

#include "bsp_ps2.h"
#include "delay.h"       /* delay_us() —— 现在是 DWT 版 */

/* ---- 数据 ---------------------------------------------------------------- */
u16 Handkey;
u8  Comd[2] = {0x01, 0x42};                                     /* 初始化命令:进入"红灯"配置模式 */
u8  Data[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
u8  scan[9] = {0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};  /* 读取用的命令帧 */

u16 MASK[] = {
    PSB_SELECT, PSB_L3, PSB_R3, PSB_START,
    PSB_PAD_UP, PSB_PAD_RIGHT, PSB_PAD_DOWN, PSB_PAD_LEFT,
    PSB_L2, PSB_R2, PSB_L1, PSB_R1,
    PSB_GREEN, PSB_RED, PSB_BLUE, PSB_PINK,
    PSB_TRIANGLE, PSB_CIRCLE, PSB_CROSS, PSB_SQUARE
};  /* 按键位序号 -> 按键码 */

/**
 * @brief  PS2 引脚的空闲态设置
 * @note   GPIO 的方向/上下拉已由 MX_GPIO_Init() 配置好,这里只置电平。
 *         手柄协议要求空闲时 CS/CLK/DO 全为高。
 */
void bsp_ps2_init(void)
{
    DO_H;
    CLK_H;
    CS_H;
}

/**
 * @brief  与手柄交换一个字节(MSB 先出,时钟下降沿后从 DI 采样)
 * @param  command 要发送的字节
 * @return 手柄同时返回的字节
 */
u8 PS2_ReadData(u8 command)
{
    u8 i, j = 1;
    u8 res = 0;

    for (i = 0; i <= 7; i++) {
        if (command & 0x01)
            DO_H;
        else
            DO_L;
        command = command >> 1;
        delay_us(6);
        CLK_L;
        delay_us(6);
        if (DI)
            res = res + j;
        j = j << 1;
        CLK_H;
        delay_us(6);
    }
    DO_H;
    delay_us(55);
    return res;
}

/**
 * @brief  读一次按键,返回按键码
 * @return 按键码(PSB_xxx);0 表示没有按键按下
 * @note   位为 0 表示按下,所以下面用 (Handkey & bit) == 0 判断
 */
unsigned char PS2_DataKey(void)
{
    u8 index = 0, i = 0;

    PS2_ClearData();
    CS_L;
    for (i = 0; i < 9; i++) {
        Data[i] = PS2_ReadData(scan[i]);
    }
    CS_H;

    Handkey = (Data[4] << 8) | Data[3];
    for (index = 0; index < 16; index++) {
        if ((Handkey & (1 << (MASK[index] - 1))) == 0)
            return index + 1;
    }
    return 0;
}

/**
 * @brief  向手柄发送一条命令,并读回一字节到 Data[1]
 */
void PS2_Cmd(u8 CMD)
{
    volatile u16 ref = 0x01;

    Data[1] = 0;
    for (ref = 0x01; ref < 0x0100; ref <<= 1) {
        if (ref & CMD) {
            DO_H;
        } else {
            DO_L;
        }

        CLK_H;
        delay_us(50);
        CLK_L;
        delay_us(50);
        CLK_H;
        if (DI)
            Data[1] = ref | Data[1];
    }
}

/**
 * @brief  判断手柄当前是"红灯(模拟)"模式还是普通模式
 * @return 0 = 红灯模式,1 = 普通模式
 */
u8 PS2_RedLight(void)
{
    CS_L;
    PS2_Cmd(Comd[0]);   /* 0x01 起始 */
    PS2_Cmd(Comd[1]);   /* 0x42 请求配置 */
    CS_H;
    if (Data[1] == 0x73)
        return 0;
    else
        return 1;
}

/**
 * @brief  读某个摇杆的模拟量
 * @param  button 摇杆索引 PSS_RX / PSS_RY / PSS_LX / PSS_LY
 * @return 0~255
 */
u8 PS2_AnologData(u8 button)
{
    return Data[button];
}

/**
 * @brief  清空 9 字节数据缓冲
 */
void PS2_ClearData(void)
{
    u8 a;
    for (a = 0; a < 9; a++)
        Data[a] = 0x00;
}

/**
 ******************************************************************************
 * @file    app_main.h
 * @brief   应用层入口 —— 原 USER/main.c 的迁移
 *
 * 为什么要拆成 app_init() + app_loop():
 *   CubeMX 生成的 main.c 里,main() 的结构是固定的
 *     HAL_Init() -> SystemClock_Config() -> MX_xxx_Init()...
 *     while (1) { ... }
 *   它给我们留了两个 USER CODE 区(唯一不会被重新生成覆盖的地方):
 *     一个是"初始化全部完成之后"那一处,
 *     另一个在 while(1) 循环体内。
 *   我们把原来的"初始化段"和"主循环体"分别放进去,
 *   这样 CubeMX 以后重新生成也不会冲掉应用代码。
 ******************************************************************************
 */
#ifndef __APP_MAIN_H
#define __APP_MAIN_H

/**
 * @brief  应用初始化(原 main() 开头那一大段 xxx_Init)
 * @note   调用时机:必须在 CubeMX 的 MX_xxx_Init() 全部执行完之后
 *         —— 放在 main.c 的 USER CODE BEGIN 2 区里。
 */
void app_init(void);

/**
 * @brief  主循环体(原 main() 里 while(1) 的内容)
 * @note   放在 main.c 的 USER CODE BEGIN WHILE 区里,由 CubeMX 的 while(1) 反复调用。
 *         调用一次 = 原来跑一圈。
 */
void app_loop(void);

#endif /* __APP_MAIN_H */

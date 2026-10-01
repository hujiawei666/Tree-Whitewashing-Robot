/**
 ******************************************************************************
 * @file    app_tasks.h
 * @brief   把原来的裸机主循环拆成 FreeRTOS 任务
 ******************************************************************************
 */
#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include "FreeRTOS.h"
#include "event_groups.h"

/**
 * @brief  初始化 RTOS 内核对象(printf 互斥量、涂白事件组)
 * @note   必须在 osKernelInitialize() 之后调用。
 *         调用点:Core/Src/freertos.c 的 MX_FREERTOS_Init(),用户代码区 Init 段。
 */
void app_rtos_init(void);

/**
 * @brief  创建应用任务
 * @note   调用点:Core/Src/freertos.c 的 MX_FREERTOS_Init(),用户代码区 RTOS_THREADS 段。
 */
void app_tasks_create(void);

/* printf 互斥锁 ---- 给 app_main.c 的 HMI_send_string / HMI_send_number 用。
 * 内核启动前句柄还是 NULL,那时只有主线程在 printf,不需要锁 ---- 直接放行 */
void app_printf_lock(void);
void app_printf_unlock(void);

/* ---- 涂白事件组的事件位 ---- */
#define EG_BIT_TREE_DONE  ((EventBits_t)(1 << 0))   /* 一棵树涂白完成 */
#define EG_BIT_ALL_DONE   ((EventBits_t)(1 << 1))   /* 全部涂完,回到手动模式 */

/* 设置涂白事件位(广播给所有等待的任务) */
void app_paint_event_set(EventBits_t bits);

#endif /* __APP_TASKS_H */

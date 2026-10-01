/**
 ******************************************************************************
 * @file    app_tasks.c
 * @brief   把原来的裸机主循环拆成 3 个 FreeRTOS 任务
 *
 * 【和原程序的关键差别】
 *   原来:main() 里一个 while(1),三件事依次调用;
 *         只要一件耗时(例如自动模式里的 delay_ms(12000)),整机全停。
 *   现在:每件事各有自己的任务。
 *         某个任务延时(vTaskDelay)时它进入阻塞态,不占 CPU,其它任务照常跑。
 *
 * 【任务划分怎么来的】
 *   从原 app_loop() 里拆出两件"既不共享状态、又会长时间阻塞"的事:
 *     HIM_chuan_shu()   -> TaskComm   (内部有 22 处 delay_ms,最长 12 秒)
 *     hcsr04_nonblock() -> TaskUltra  (TIM5 中断驱动,需要高频推进)
 *   剩下的 HMI 模式切换 / PS2 检查 / 模式分发 都靠 current_state 紧耦合,
 *   拆开要加锁,不值得,统一留在 TaskDrive。
 ******************************************************************************
 */

#include "app_tasks.h"
#include "app_main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

/* ---------------------------------------------------------------------------
 * 优先级
 * 已核实:cmsis_os2.c 把 CMSIS 优先级原样当 FreeRTOS 优先级用,没有换算。
 * CubeMX 的 defaultTask 在 CubeMX 里设成了 osPriorityLow(=8),
 * 所以我们的任务数值必须大于 8 才排在它前面。
 * ------------------------------------------------------------------------- */
#define PRIO_DRIVE    12   /* 最高:驾驶逻辑,最不能等 */
#define PRIO_ULTRA    11
#define PRIO_COMM     11
#define PRIO_REPORT   10   /* 最低:刷新显示,最不急 */

/* ---------------------------------------------------------------------------
 * 栈大小(单位:word,4 字节 ---- 不是字节!)
 *   TaskDrive 256 word = 1024 字节
 *   TaskUltra 128 word =  512 字节
 *   TaskComm  256 word = 1024 字节
 *   TaskReport 128 word =  512 字节
 * 这只是估算值。上板后用 uxTaskGetStackHighWaterMark 实测水位再收窄。
 * ------------------------------------------------------------------------- */
#define STK_DRIVE     256
#define STK_ULTRA     128
#define STK_COMM      256
#define STK_REPORT    128

/* ---------------------------------------------------------------------------
 * 内核对象句柄
 * ------------------------------------------------------------------------- */
static SemaphoreHandle_t  s_printf_mutex = NULL;
static EventGroupHandle_t s_eg_paint     = NULL;
static TaskHandle_t       s_drive = NULL, s_ultra = NULL, s_comm = NULL, s_report = NULL;

/* ===========================================================================
 * 任务函数
 * =========================================================================== */

/**
 * @brief  驾驶任务:原来是 while(1) 里的"怎么走"部分
 * @note   内部逻辑(app_loop)一字未改,只是从 while(1) 里搬进来。
 *         vTaskDelay(1) 代替原来的空转 1ms:
 *         原来是空转占着 CPU,现在是主动让出去,让别的任务能跑。
 */
static void TaskDrive(void *argument)
{
    (void)argument;
    for (;;) {
        app_loop();
        vTaskDelay(1);
    }
}

/**
 * @brief  超声波任务:测距状态机轮询
 * @note   内部由 TIM5 中断驱动一个推进状态,所以 1ms 跑一次足够。
 */
static void TaskUltra(void *argument)
{
    (void)argument;
    for (;;) {
        hcsr04_nonblock();
        vTaskDelay(1);
    }
}

/**
 * @brief  通信任务:HMI 串口指令处理
 * @note   它处理的是"按键速度"级别的数据,2ms 一次绰绰有余。
 */
static void TaskComm(void *argument)
{
    (void)argument;
    for (;;) {
        HIM_chuan_shu();
        vTaskDelay(2);
    }
}

/**
 * @brief  报告任务:等待涂白事件,刷新 HMI
 *
 * 【事件组的典型用法】
 *    TaskDrive 涂完一棵树 -> 置位 -> 本任务被唤醒 -> 刷新 HMI -> 回去继续等
 *
 * 【和互斥量的区别】
 *    互斥量是"资源",借了必须还;事件组是"公告板",贴上就一直在。
 *    而且一个位被置上后,所有在等它的任务【一起醒】(广播)。
 */
static void TaskReport(void *argument)
{
    EventBits_t bits;
    (void)argument;
    for (;;) {
        /* 等 bit0(一棵树完成) 或 bit1(全部完成) 任意一个变成 1。
         * 参数逐个看:
         *   ① s_eg_paint                      等哪个事件组
         *   ② EG_BIT_TREE_DONE|EG_BIT_ALL_DONE 关心哪几个位
         *   ③ pdTRUE  等到后【自动清掉】这些位,下次要等新的置位
         *   ④ pdFALSE 只要【任意一个】位满足就返回(或关系)
         *   ⑤ portMAX_DELAY 一直等,不超时 */
        bits = xEventGroupWaitBits(s_eg_paint,
                                   EG_BIT_TREE_DONE | EG_BIT_ALL_DONE,
                                   pdTRUE,
                                   pdFALSE,
                                   portMAX_DELAY);
        (void)bits;   /* 用不到返回值,留着方便调试 */

        HMI_send_string("box.t3.txt", "涂白记录已更新");
    }
}

/* ===========================================================================
 * 初始化与创建
 * =========================================================================== */
void app_rtos_init(void)
{
    /* printf 互斥量:三个任务都会调 HMI_send_*,需要互斥 */
    s_printf_mutex = xSemaphoreCreateMutex();

    /* 涂白事件组:Task 12 会用到 */
    s_eg_paint = xEventGroupCreate();
}

void app_tasks_create(void)
{
    xTaskCreate(TaskDrive, "TaskDrive", STK_DRIVE, NULL, PRIO_DRIVE, &s_drive);
    xTaskCreate(TaskUltra, "TaskUltra", STK_ULTRA, NULL, PRIO_ULTRA, &s_ultra);
    xTaskCreate(TaskComm,  "TaskComm",  STK_COMM,  NULL, PRIO_COMM,  &s_comm);
    xTaskCreate(TaskReport, "TaskReport", STK_REPORT, NULL, PRIO_REPORT, &s_report);
}

/* ===========================================================================
 * 涂白事件(给 Task 12 用)
 * =========================================================================== */
void app_paint_event_set(EventBits_t bits)
{
    if (s_eg_paint != NULL) {
        xEventGroupSetBits(s_eg_paint, bits);
    }
}

/* ===========================================================================
 * printf 互斥
 * =========================================================================== */
/*
 * 为什么必须加锁:
 *   HMI_send_string / HMI_send_number 最终走到
 *   printf -> fputc -> HAL_UART_Transmit(&huart1, ...)。
 *
 *   HAL_UART_Transmit 有全局状态机(huart1.gState)且是阻塞式的。
 *   两个任务同时进来:
 *     - 打印内容会互相穿插(指令和数字混在一起)
 *     - 更糟的是 huart1.gState 被踩,某一方会一直卡住
 *
 * 为什么用互斥量而不是二值信号量:
 *   HAL_UART_Transmit 是阻塞的,持有锁的时间不短。
 *   互斥量有"优先级继承":低优先级任务持锁时,若高优先级任务来抢,
 *   低优先级会临时继承高优先级,尽快跑完释放,避免优先级反转。
 *   二值信号量没有这个机制。
 *
 * 注意:互斥量不能在中断服务程序里使用。
 */
void app_printf_lock(void)
{
    /* osKernelStart 之前句柄还是 NULL,那时只有主线程在 printf,不用锁 */
    if (s_printf_mutex != NULL) {
        (void)xSemaphoreTake(s_printf_mutex, portMAX_DELAY);
    }
}

void app_printf_unlock(void)
{
    if (s_printf_mutex != NULL) {
        (void)xSemaphoreGive(s_printf_mutex);
    }
}

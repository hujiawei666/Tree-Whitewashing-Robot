# RobotHAL → FreeRTOS 教学式移植：设计与大纲


> ### 关于文中的目录名
>
> 本文档写于 2026-09-30 重构过程中，当时的目录命名与现在仓库中的不一致。
> **正文保留原样（作为工作记录），对应关系如下：**
>
> | 文中名称 | 现仓库位置 | 说明 |
> |---|---|---|
> | `RobotHAL_RTOS_Lab/` | `MCU-HAL-RTOS/` | HAL + FreeRTOS 版（本次重构产物） |
> | ├ Keil 工程 | `MCU-HAL-RTOS/MDK-ARM/TreeWhitewash_RTOS.uvprojx` | 后续去掉了 "Lab" 后缀 |
> | `RobotHAL/` | `MCU-HAL/` | HAL 裸机版（重构的起点） |
> | ├ Keil 工程 | `MCU-HAL/MDK-ARM/RobotHAL.uvprojx` | 文件名未变 |
> | `RobotHAL_RTOS/` | — | 当时的 CMSIS-RTOS2 参考构建，**未收录本仓库** |
>
> 文中出现的命令（如 `cp -r RobotHAL RobotHAL_RTOS_Lab`）是当时实际执行的原始记录。

- 日期：2026-09-30
- 学习材料：百问网《FreeRTOS 入门与工程实践》 https://rtos.100ask.net/docs/DShanMCU-F103/ （19 章，已通读）
- 参考实现：`RobotHAL_RTOS\`（已有，2026-09-30 06:03 编译通过，用的是 CMSIS-RTOS2）
- 本次工作目录：`RobotHAL_RTOS_Lab\`（从 `RobotHAL\` 复制，原工程不动）

## 1. 目标

从 `RobotHAL`（HAL 裸机）出发，**由用户自己动手**，用**原生 FreeRTOS API** 把 `app_loop()` 的 `while(1)` 拆成多任务。
`RobotHAL_RTOS` 作为对照答案保留。

**本质目的是学习**，不是产出代码。因此每一步先讲"为什么"，再动手，动完停下确认。

## 2. 硬约束

| 约束 | 影响 |
|---|---|
| 机器人**不在手边** | 只能做 L0（编译）+ L1（与参考实现逐行对照）；**不得声称"迁移完成"** |
| 用**原生 FreeRTOS API**（`xTaskCreate`/`xSemaphoreCreateMutex`/`xEventGroup*`） | 与 100ask 教程、FreeRTOS 官方文档对齐；代价是无法与 CMSIS 版逐行比对 |
| CubeMX 只能生成 CMSIS_V1/V2 封装 | 我们仍照常启用，但代码里直接调原生 API；`cmsis_os2.c` 编译进去不使用 |

## 3. 关键坑（必须在对应步骤讲清）

### 3.1 栈大小单位差 4 倍

| API | 参数 | 单位 |
|---|---|---|
| `osThreadAttr_t.stack_size`（CMSIS） | `stack_size` | **字节** |
| `xTaskCreate`（原生） | `usStackDepth` | **word（4 字节）** |

教程 9.2.2 原文："传入 100，表示栈的大小为 100 word，即 400 字节"。
参考实现里 `TaskDrive` 写的是 `1024`（字节）；换成原生要写 `256`（word）。
**单位写错 4 倍，栈溢出或内存浪费都很难查。**

### 3.2 printf 竞争（教程第 10 章的"抢厕所"）

`HMI_send_*` → `printf` → `fputc` → `HAL_UART_Transmit(&huart1, ...)`。
`hal_uart.c` 的 `gState` 是共享状态机，两个任务同时进会互相踩。
教程 10.2 用 `LCD_PrintString` + `static int bCanUse` 演示了为什么"全局标志位"不保险
（判断与清零之间可被打断；"减一"本身也是读-改-写三步，同样可被打断）。
→ 用 `xSemaphoreCreateMutex`，讲优先级反转与优先级继承（教程 13.1 / 13.3）。

### 3.3 中断优先级

`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`。
抢占优先级必须 **≥ 5** 才能在该 ISR 里调 `...FromISR` API。
当前 `Core/Src/tim.c`：TIM8 = 5、TIM7 = 5、TIM5 = 6 → 已满足。

### 3.4 DWT 延时

原 `SYSTEM/delay/delay.c` 的 `delay_us` 会关 SysTick → 与 FreeRTOS tick 冲突。
已在 `App/Src/delay.c` 用 DWT `CYCCNT` 重写（注意 32 位回绕用减法比较）。

### 3.5 用户代码区

CubeMX 重新生成会删掉不在 `/* USER CODE BEGIN xxx */ ... /* USER CODE END xxx */`
之间的代码（教程 6.6.2）。所有挂载点都必须落在这些区内。

### 3.6 `defaultTask` 删不掉

CubeMX 生成的默认任务只能改名/改内容（教程 6.4.2）。

### 3.7 🔴 `delay_ms` 是忙等，会饿死其它任务（**2026-10-01 发现**）

`delay_ms()` → `HAL_Delay()` → 原地轮询 `uwTick`，**不让出 CPU**；`delay_us()` 是 DWT 忙等，同理。

全工程 **33 处** `delay_ms`，最大的三块：

| 函数 | 处数 | 最长 |
|---|---|---|
| `HIM_chuan_shu()` | 22 | 12000 ms |
| `auto_mode_state_machine()` | 6 | 5000 ms |
| `app_loop()` | 1 | 1000 ms |

**后果**：`TaskDrive` 是最高优先级。它在 `app_loop()` 里忙等期间**一直是就绪态**，
于是 `TaskUltra` / `TaskComm` **永远轮不到** —— 移植完不解决问题，和裸机一样卡。

**参考答案 `RobotHAL_RTOS` 有同样的缺陷**（`delay.c:70-74` 也是 `HAL_Delay`），
因为没上板所以没暴露。**本次必须修正。**

**修法**：`delay_ms` 改成调度器感知 —— 运行中用 `vTaskDelay`，未启动时退回 `HAL_Delay`：

```c
if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    vTaskDelay(pdMS_TO_TICKS(ms));
} else {
    HAL_Delay(ms);
}
```

`delay_us` 不动（微秒级用不了 tick 粒度）。

**这是整个移植里"真正换来什么"的落点**：忙等 → 阻塞，CPU 才让得出去。

## 4. 工作步骤

| 步 | 做什么 | 教程章节 |
|---|---|---|
| 0 | 复制 `RobotHAL` → `RobotHAL_RTOS_Lab`，改工程名/TargetName，先编译验证基线 | — |
| 1 | CubeMX 配时钟；HAL 时基 **SysTick → TIM1** | 6.2 |
| 2 | 启用 FreeRTOS：CMSIS_V2 接口、tick 1000Hz、heap_4、`TOTAL_HEAP_SIZE`、栈溢出检查、malloc 失败钩子 | 6.4 / 8.2.4 |
| 3 | 讲启动链路：`osKernelInitialize` → `MX_FREERTOS_Init` → `osKernelStart`（不返回） | 7.6 |
| 4 | 无 RTOS 基础概念：任务/栈/优先级/状态/阻塞 | 9.1 / 9.3 / 9.4 |
| 5 | **从 `app_loop()` 推导任务划分** | 2.2 / 9.2 |
| 6 | `xTaskCreate` 建任务（重点讲 word 单位与栈估算） | 9.2.2 |
| 7 | 周期与延时：`vTaskDelay` vs `xTaskDelayUntil`、tick、`pdMS_TO_TICKS` | 9.3.2 / 9.6 |
| 8 | printf 互斥：`xSemaphoreCreateMutex`，优先级反转/继承 | 10.2 / 13.1 / 13.3 |
| 9 | 中断侧：优先级阈值、`FromISR`、`pxHigherPriorityTaskWoken`、`portYIELD_FROM_ISR` | 17.1 |
| 10 | 栈水位实测：`uxTaskGetStackHighWaterMark`，回头修正栈大小 | 19.1.5 / 19.2.1 |
| 11 | 编译；与 `RobotHAL_RTOS` 逐行对照（任务划分/栈/优先级/中断配置） | — |
| 12 | （已确认要做）事件组：找一个真实场景落地 | 14 |

## 5. 参考实现的任务划分（供第 5 步对照，不是要照抄）

| 任务 | 内容 | 栈 | 优先级 | 周期 |
|---|---|---|---|---|
| TaskDrive | `app_loop()`：模式切换 + PS2 + 自动状态机 | 1024 B | High | 1 ms |
| TaskUltra | `hcsr04_nonblock()` 超声波轮询 | 512 B | AboveNormal | 1 ms |
| TaskComm | `HIM_chuan_shu()` HMI 串口指令 | 1024 B | AboveNormal | 2 ms |

原计划的第 4 个任务 `TaskPaint` 被取消：涂白动作由 `auto_mode_state_machine()` 直接驱动，
属于"怎么走"，并入 TaskDrive。

### 5.1 参考实现的 FreeRTOSConfig 取值（第 2 步、第 11 步对照用）

| 配置项 | 参考实现取值 |
|---|---|
| `configUSE_PREEMPTION` | 1（可抢占） |
| `configTICK_RATE_HZ` | 1000（1ms 一拍） |
| `configMAX_PRIORITIES` | 56 |
| `configTOTAL_HEAP_SIZE` | 15360 |
| 内存管理文件 | `heap_4.c`（相邻空闲块可合并） |
| `configUSE_MUTEXES` | 1 |
| `configCHECK_FOR_STACK_OVERFLOW` | 2（填 0xa5 检测，教程 19.1.5 方法 2） |
| `configUSE_MALLOC_FAILED_HOOK` | 1 |
| `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` | 5 |

`heap_4` + 15360 字节的实际占用（参考实现编译结果，第 11 步可比对）：

```
Code=28992  RO-data=912  RW-data=368  ZI-data=21320
```

三个任务栈合计 1024+512+1024 = 2560 字节，加上 TCB、互斥量、`defaultTask`(128*4=512)，
在 15360 的堆里是够的 —— 第 10 步用 `uxTaskGetStackHighWaterMark` 实测收敛。

## 6. 验收

| 层 | 内容 | 能证明 |
|---|---|---|
| L0 | 每阶段末尾编译，0 error | 语法/链接正确 |
| L1 | 与 `RobotHAL_RTOS` 对照：任务划分、栈大小、优先级、中断优先级、printf 加锁点 | 设计一致 |

**做不到**（硬件不在）：任务真的能调度、栈真的够、锁真的有效、TIM 中断真的进得来。

## 7. 边界

- 不改 `RobotHAL\` 和 `RobotHAL_RTOS\`
- 不顺手清理历史遗留（如 `app_main.c` 里 `last_system_time` 这类残留）
- 不引入事件组以外的额外机制（队列/任务通知/软件定时器）——除非某一步确实需要

## 8. 环境备忘

- 构建必须用 `D:\keil5-new\UV4\UV4.exe`（MDK 5.24a / ARMCC V5.06u5 = AC5），
  与参考实现构建日志一致（`Toolchain Path: D:\keil5-new\ARM\ARMCC\Bin`）。
- `App/` 下源文件是 **GBK** 编码，`Core/`（CubeMX 生成）是 UTF-8。
  往 `App/` 写中文注释时不能含 GBK 无法表示的字符（如 `→`、`⚠️`）。
- 构建命令：
  `cd MDK-ARM && "D:/keil5-new/UV4/UV4.exe" -b <工程>.uvprojx -t <TargetName> -o cli_build.log -j0`
  注意 Keil 退出码 1 = 仅警告，不是失败；以日志中的 `N Error(s)` 与是否生成 `.axf` 为准。

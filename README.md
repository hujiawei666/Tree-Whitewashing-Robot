# 绿耘智涂 —— 履带式自适应树木涂白机器人

基于 STM32F407 主控 + K230 AI 视觉的履带式树木涂白机器人。

单棵 15cm 胸径树木涂白约 30 秒（人工约 3 分钟），支持自动/手动两种作业模式。

## 硬件

| 部件 | 型号 |
|---|---|
| 主控 | STM32F407VET6 |
| 视觉 | K230 |
| 执行机构 | 直流电机 ×2（履带）、步进电机、舵机 |
| 传感器 | HC-SR04 超声波 ×3、PS2 手柄 |
| 人机交互 | 串口 HMI 触摸屏 |

## 仓库内容

本仓库包含**两个版本的固件**，用于对比"裸机"与"RTOS"两种程序设计方式：

| 目录 | 说明 |
|---|---|
| `RobotHAL/` | **HAL 裸机版**。CubeMX 生成外设骨架，主循环是一个 `while(1)` 依次调用各功能 |
| `RobotHAL_RTOS_Lab/` | **HAL + FreeRTOS 版**。把主循环拆成多个任务，用**原生 FreeRTOS API** 实现 |

### 两版的关键差异

| | RobotHAL（裸机） | RobotHAL_RTOS_Lab（RTOS） |
|---|---|---|
| 主循环 | `main()` 里一个 `while(1)` | 拆成 4 个任务 `TaskDrive` / `TaskUltra` / `TaskComm` / `TaskReport` |
| 任务调度 | 无。一件事耗时就整机停摆 | 优先级 + 抢占；阻塞的任务不占 CPU |
| 延时 | `HAL_Delay()` 忙等，抱着 CPU 空转 | 调度器运行时用 `vTaskDelay()`，真正让出 CPU |
| 串口打印 | 单线程，无需保护 | `xSemaphoreCreateMutex` 保护，防两个任务同时踩 `huart1` 状态机 |
| 任务间通信 | 靠全局变量轮询 | 事件组广播（涂白完成事件） |
| 中断优先级 | 无需考虑 | 提到 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 之上，可用 `FromISR` API |

### 任务划分

| 任务 | 职责 | 栈 | 优先级 | 周期 |
|---|---|---|---|---|
| `TaskDrive` | 模式切换 + PS2 手柄 + 自动状态机 | 1024 B | 12 | 1 ms |
| `TaskUltra` | 超声波测距状态机轮询 | 512 B | 11 | 1 ms |
| `TaskComm` | HMI 串口指令处理 | 1024 B | 11 | 2 ms |
| `TaskReport` | 等待涂白完成事件，刷新 HMI | 512 B | 10 | 事件驱动 |

## 开发环境

| | |
|---|---|
| IDE | Keil MDK-ARM (μVision V5.24.2, ARMCC V5.06 update 5) |
| 配置工具 | STM32CubeMX 6.17.0 |
| 芯片包 | Keil.STM32F4xx_DFP |
| FreeRTOS | CubeMX 内置中间件（CMSIS_V2 接口，代码使用原生 API） |

### 编译

用 Keil 打开 `MDK-ARM/` 下的 `.uvprojx`，直接 Build 即可。

命令行编译：

```bash
cd MDK-ARM
UV4.exe -b RobotHAL_RTOS_Lab.uvprojx -t RobotHAL_RTOS_Lab -o build.log -j0
```

> Keil 退出码约定：`0` = 无错无警告，`1` = 仅警告，`2` = 错误。以日志里的 `N Error(s)` 为准。

## 代码结构

```
├── App/           应用层（自己写的业务代码）
│   ├── Src/
│   │   ├── app_main.c    主循环 body / 自动状态机 / HMI 协议
│   │   ├── app_tasks.c   FreeRTOS 任务与同步对象
│   │   ├── bsp_*.c       各外设驱动（电机/舵机/超声波/PS2/串口）
│   │   └── delay.c       微秒级 DWT 延时 + 毫秒级延时
│   └── Inc/
├── Core/          CubeMX 生成（main.c / 外设初始化 / FreeRTOSConfig.h / freertos.c）
├── Drivers/       STM32 HAL 库 + CMSIS
└── Middlewares/   FreeRTOS 内核源码
```

## 控制流程

```
上电
 └─ HAL_Init / 时钟 / 外设初始化 / app_init()
 └─ osKernelInitialize()
 └─ MX_FREERTOS_Init()            ← 创建互斥量、事件组、4 个任务
 └─ osKernelStart()               ← 调度器启动，永不返回

运行中：
 TaskDrive ── 1ms 周期推进自动状态机
    └─ 一棵树涂白完成 → xEventGroupSetBits(EG_BIT_TREE_DONE)
                          └─ TaskReport 被唤醒 → 刷新 HMI
 TaskUltra ── 1ms 周期推进超声波测距状态机（数据由 TIM5 捕获中断填充）
 TaskComm  ── 2ms 周期处理 HMI 指令
```

## 已知限制

- 本轮开发**未上板验证**（硬件不在手边），仅完成编译与静态对照。
- `TaskReport` 的 HMI 文案、任务的栈大小（`uxTaskGetStackHighWaterMark`）建议上板后按实测收紧。

# 绿耘智涂 —— 履带式自适应树木涂白机器人

基于 STM32F407 主控 + K230 AI 视觉的履带式树木涂白机器人固件。

单棵 15cm 胸径树木涂白约 30 秒（人工约 3 分钟），支持自动 / 手动两种作业模式。

> **本仓库根目录 = HAL + FreeRTOS 版**（原生 FreeRTOS API 实现多任务）。
> 同时保留 HAL 裸机版于 [`bare-metal/`](bare-metal/)，用于对比两种程序设计方式。

---

## 硬件

| 部件 | 型号 |
|---|---|
| 主控 | STM32F407VET6 |
| 视觉 | K230 |
| 执行机构 | 直流电机 ×2（履带）、步进电机、舵机 |
| 传感器 | HC-SR04 超声波 ×3、PS2 手柄 |
| 人机交互 | 串口 HMI 触摸屏 |

---

## FreeRTOS 版：多任务划分

把原裸机主循环 `while(1)` 拆成 4 个任务：

| 任务 | 职责 | 栈 | 优先级 | 周期 |
|---|---|---|---|---|
| `TaskDrive` | 模式切换 + PS2 手柄 + 自动状态机 | 1024 B | 12 | 1 ms |
| `TaskUltra` | 超声波测距状态机轮询 | 512 B | 11 | 1 ms |
| `TaskComm` | HMI 串口指令处理 | 512 B | 11 | 2 ms |
| `TaskReport` | 等待涂白完成事件，刷新 HMI | 512 B | 10 | 事件驱动 |

**核心代码位置**：

| 文件 | 内容 |
|---|---|
| [`App/Src/app_tasks.c`](App/Src/app_tasks.c) | 任务函数、`xTaskCreate`、互斥量、事件组 |
| [`App/Src/app_main.c`](App/Src/app_main.c) | 业务逻辑：自动状态机、HMI 协议、超声波 |
| [`App/Src/delay.c`](App/Src/delay.c) | DWT 微秒延时 + 调度器感知的毫秒延时 |
| [`Core/Src/freertos.c`](Core/Src/freertos.c) | FreeRTOS 初始化入口、栈溢出/堆失败钩子 |
| [`Core/Inc/FreeRTOSConfig.h`](Core/Inc/FreeRTOSConfig.h) | 内核配置 |

### 用到的 FreeRTOS 机制

| 机制 | 位置 | 解决的问题 |
|---|---|---|
| 任务优先级 + 抢占 | `app_tasks.c` 的 `PRIO_*` | 决定调度顺序，高优先级先跑 |
| `vTaskDelay` 阻塞 | 各任务循环末尾 | 主动让出 CPU，而非忙等 |
| 互斥量 | `app_main.c` 的 `app_printf_lock/unlock` | 多任务同时 `printf` 会踩 `huart1.gState` 状态机 |
| 事件组广播 | `xEventGroupSetBits` / `xEventGroupWaitBits` | 涂白完成后通知报告任务，无需轮询 |
| `FromISR` 中断协作 | NVIC 优先级 ≥ 5 | 允许在中断中安全调用内核 API |

### 相比裸机版的关键改进

| | [`bare-metal/`](bare-metal/)（裸机） | 根目录（FreeRTOS） |
|---|---|---|
| 主循环 | `main()` 里一个 `while(1)` | 拆成 4 个独立任务 |
| 一件事耗时时 | **整机停摆**（如涂白 12 秒内不响应任何输入） | 只有该任务阻塞，其余任务照常运行 |
| 延时实现 | `HAL_Delay()` 忙等，抱着 CPU 空转 | `vTaskDelay()` 阻塞，主动让出 CPU |
| 串口打印 | 单线程，无需保护 | 互斥量保护，防并发踩状态机 |
| 任务间同步 | 全局变量轮询 | 事件组，事件驱动 |

---

## 控制流程

```
上电
 └─ HAL_Init / 时钟 / 外设初始化 / app_init()
 └─ osKernelInitialize()
 └─ MX_FREERTOS_Init()      ← 创建互斥量、事件组、4 个任务
 └─ osKernelStart()         ← 启动调度器，永不返回

运行中：
 TaskDrive ── 1ms 周期推进自动状态机
    └─ 一棵树涂白完成 → xEventGroupSetBits(EG_BIT_TREE_DONE)
                          └─ TaskReport 被唤醒 → 刷新 HMI
 TaskUltra ── 1ms 周期推进超声波测距状态机（数据由 TIM5 捕获中断填充）
 TaskComm  ── 2ms 周期处理 HMI 指令
```

---

## 开发环境

| | |
|---|---|
| IDE | Keil MDK-ARM（μVision V5.24.2，ARMCC V5.06 update 5） |
| 配置工具 | STM32CubeMX 6.17.0 |
| 芯片包 | Keil.STM32F4xx_DFP |
| FreeRTOS | CubeMX 内置中间件（CMSIS_V2 接口，代码直接使用原生 API） |

### 编译

用 Keil 打开 `MDK-ARM/TreeWhitewash_RTOS.uvprojx`，直接 Build。

命令行：

```bash
cd MDK-ARM
UV4.exe -b TreeWhitewash_RTOS.uvprojx -t TreeWhitewash_RTOS -o build.log -j0
```

> Keil 退出码约定：`0` = 无错无警告，`1` = 仅警告，`2` = 错误。以日志中的 `N Error(s)` 为准。

### FreeRTOS 内核配置要点

| 配置项 | 值 | 说明 |
|---|---|---|
| `configTICK_RATE_HZ` | 1000 | 1 tick = 1 ms |
| `configMAX_PRIORITIES` | 56 | CMSIS 优先级枚举最大值 55 + 1 |
| `configTOTAL_HEAP_SIZE` | 15360 | 任务 / 队列等内核对象的堆 |
| 内存管理 | `heap_4` | 首次适应 + 合并相邻空闲块 |
| `configCHECK_FOR_STACK_OVERFLOW` | 2 | 填 0xa5 检测栈尾 |
| `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` | 5 | 中断中可调内核 API 的门槛 |

---

## 代码结构

```
├── App/                    应用层（业务代码，非 CubeMX 生成）
│   ├── Src/
│   │   ├── app_main.c      自动状态机 / HMI 协议 / 超声波 / PS2
│   │   ├── app_tasks.c     FreeRTOS 任务与同步对象   ★
│   │   ├── bsp_*.c         各外设驱动
│   │   └── delay.c         DWT 微秒延时 + 毫秒延时
│   └── Inc/
├── Core/                   CubeMX 生成
│   ├── Src/main.c          启动流程
│   ├── Src/freertos.c      内核对象与任务创建入口
│   └── Inc/FreeRTOSConfig.h
├── Drivers/                STM32 HAL 库 + CMSIS
├── Middlewares/            FreeRTOS 内核源码
├── MDK-ARM/                Keil 工程
└── bare-metal/             HAL 裸机版（对照用，结构同上）
```

---

## 已知限制

- 本轮开发**未上板验证**（硬件不在手边），仅完成编译（0 error）与静态对照。
- 任务栈大小为估算值，建议上板后用 `uxTaskGetStackHighWaterMark()` 实测收紧。
- `TaskReport` 目前仅刷新一处 HMI 文本，可扩展为完整的涂白进度显示。

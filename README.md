# Tree-Whitewashing-Robot（履带式自适应树木涂白机器人）

2025 第八届全国大学生嵌入式芯片与系统设计竞赛 · 国家二等奖项目

基于 **STM32F407** 与 **K230 AI 视觉** 的履带式涂白机器人，具备遥控与半自主两种作业模式，通过串口屏 UI 界面操作显示。视觉识别与超声波融合定位树干，控制执行机构完成精准喷涂，提升作业效率与标准化水平。

## 项目架构

- **主控**：STM32F407，提供**三个固件版本**（标准库裸机 → HAL 裸机 → HAL + FreeRTOS）
- **视觉**：K230 AI 摄像头，树干目标检测，UART 传输目标点数/距离/中线偏移
- **定位**：超声波测距 + 视觉构建二维坐标系，实现树干精确定位与自适应喷涂
- **执行机构**：步进电机、舵机、直流电机（PWM / PID / 闭环控制）
- **交互**：PS2 无线手柄 + 串口触摸屏（480272.HMI）双重控制，实时调节机械爪角度、车速等参数

---

## 固件版本演进

本仓库提供同一台机器人的三个固件版本，用于对比不同程序设计方式的差异：

| 目录 | 版本 | 说明 |
|---|---|---|
| [`MCU/`](MCU/) | **标准库 + 裸机** | 正点原子模板 + STM32 标准外设库，`main()` 内一个 `while(1)` 状态机 |
| [`MCU-HAL/`](MCU-HAL/) | **HAL + 裸机** | 用 STM32CubeMX 重建外设骨架迁移到 HAL，逻辑结构不变 |
| [`MCU-HAL-RTOS/`](MCU-HAL-RTOS/) | **HAL + FreeRTOS** ★ | 把主循环拆成多任务，用**原生 FreeRTOS API** 实现任务调度与同步 |

### HAL + FreeRTOS 版：任务划分

| 任务 | 职责 | 栈 | 优先级 | 周期 |
|---|---|---|---|---|
| `TaskDrive` | 模式切换 + PS2 手柄 + 自动状态机 | 1024 B | 12 | 1 ms |
| `TaskUltra` | 超声波测距状态机轮询 | 512 B | 11 | 1 ms |
| `TaskComm` | HMI 串口指令处理 | 512 B | 11 | 2 ms |
| `TaskReport` | 等待涂白完成事件，刷新 HMI | 512 B | 10 | 事件驱动 |

**核心代码位置**：

| 文件 | 内容 |
|---|---|
| [`MCU-HAL-RTOS/App/Src/app_tasks.c`](MCU-HAL-RTOS/App/Src/app_tasks.c) | 任务函数、`xTaskCreate`、互斥量、事件组 |
| [`MCU-HAL-RTOS/App/Src/app_main.c`](MCU-HAL-RTOS/App/Src/app_main.c) | 业务逻辑：自动状态机、HMI 协议、超声波 |
| [`MCU-HAL-RTOS/App/Src/delay.c`](MCU-HAL-RTOS/App/Src/delay.c) | DWT 微秒延时 + 调度器感知的毫秒延时 |
| [`MCU-HAL-RTOS/Core/Src/freertos.c`](MCU-HAL-RTOS/Core/Src/freertos.c) | 内核对象与任务创建入口、栈溢出/堆失败钩子 |
| [`MCU-HAL-RTOS/Core/Inc/FreeRTOSConfig.h`](MCU-HAL-RTOS/Core/Inc/FreeRTOSConfig.h) | 内核配置 |

**用到的 FreeRTOS 机制**：

| 机制 | 解决的问题 |
|---|---|
| 任务优先级 + 抢占 | 决定调度顺序，高优先级先跑 |
| `vTaskDelay` 阻塞 | 主动让出 CPU，而非忙等 |
| 互斥量（`xSemaphoreCreateMutex`） | 多任务同时 `printf` 会踩 `huart1.gState` 状态机 |
| 事件组（`xEventGroupSetBits/WaitBits`） | 涂白完成后通知报告任务，无需轮询 |
| `FromISR` 中断协作 | NVIC 优先级 ≥ `configMAX_SYSCALL_INTERRUPT_PRIORITY`，允许在中断中安全调用内核 API |

**相比裸机版的改进**：

| | 裸机版（`MCU/`、`MCU-HAL/`） | FreeRTOS 版（`MCU-HAL-RTOS/`） |
|---|---|---|
| 主循环 | `main()` 里一个 `while(1)` | 拆成 4 个独立任务 |
| 一件事耗时时 | **整机停摆**（如涂白 12 秒内不响应任何输入） | 只有该任务阻塞，其余任务照常运行 |
| 延时实现 | `HAL_Delay()` 忙等，抱着 CPU 空转 | `vTaskDelay()` 阻塞，主动让出 CPU |
| 串口打印 | 单线程，无需保护 | 互斥量保护，防并发踩状态机 |
| 任务间同步 | 全局变量轮询 | 事件组，事件驱动 |

---

## 目录结构

```
├── MCU/                    # 固件 ①：STM32F407 标准库工程（Keil MDK）
│   ├── USER/               #   main.c 及系统配置
│   ├── code/               #   外设驱动（ADC/蜂鸣器/直流电机/舵机/超声波/串口）
│   ├── HARDWARE/           #   按键/LED/PS2 手柄驱动
│   ├── SYSTEM/             #   delay/sys/usart 基础库
│   └── CORE/               #   Cortex-M4 内核文件
├── MCU-HAL/                # 固件 ②：主控工程 HAL 重构版（裸机）
│   ├── App/                #   应用层（业务逻辑与驱动）
│   ├── Core/               #   CubeMX 生成（main.c / 外设初始化）
│   ├── Drivers/            #   STM32 HAL 库 + CMSIS
│   └── MDK-ARM/            #   Keil 工程
├── MCU-HAL-RTOS/           # 固件 ③：HAL + FreeRTOS 多任务版  ★
│   ├── App/Src/app_tasks.c #   任务划分与同步对象
│   ├── Core/               #   含 freertos.c / FreeRTOSConfig.h
│   ├── Middlewares/        #   FreeRTOS 内核源码
│   └── MDK-ARM/            #   Keil 工程（TreeWhitewash_RTOS.uvprojx）
├── K230/                   # K230 AI 视觉识别脚本
├── demo/                   # 项目演示视频
└── docs/                   # 项目报告
```

---

## 关键技术点

- 状态机多任务协同调度：视觉识别、电机控制、超声波测距、人机交互
- **FreeRTOS 重构**：任务划分、优先级调度、互斥量、事件组实现任务同步与通信
- K230 视觉 + 超声波融合定位，UART 串口通信
- PID 闭环控制、T 型加减速算法（步进电机）

## 开发环境

| | |
|---|---|
| IDE | Keil MDK-ARM（μVision V5.24.2，ARMCC V5.06 update 5） |
| 配置工具 | STM32CubeMX 6.17.0 |
| 芯片包 | Keil.STM32F4xx_DFP |
| FreeRTOS | CubeMX 内置中间件（CMSIS_V2 接口，代码直接使用原生 API） |

### 编译

用 Keil 打开对应版本的 `.uvprojx` 直接 Build：

| 版本 | 工程文件 |
|---|---|
| 标准库 | `MCU/USER/USART.uvprojx` |
| HAL 裸机 | `MCU-HAL/MDK-ARM/RobotHAL.uvprojx` |
| HAL + FreeRTOS | `MCU-HAL-RTOS/MDK-ARM/TreeWhitewash_RTOS.uvprojx` |

命令行：

```bash
cd MCU-HAL-RTOS/MDK-ARM
UV4.exe -b TreeWhitewash_RTOS.uvprojx -t TreeWhitewash_RTOS -o build.log -j0
```

> Keil 退出码约定：`0` = 无错无警告，`1` = 仅警告，`2` = 错误。以日志中的 `N Error(s)` 为准。

---

## 演示视频

见 `demo/demo.mp4`（或点击仓库内视频文件在线播放）。

## 荣誉

- 2025 第八届全国大学生嵌入式芯片与系统设计竞赛 国家二等奖
- 国家发明专利受理：《一种树木养护机器人及其作业方法》

## 说明

- `MCU-HAL-RTOS/` 的 FreeRTOS 版已完成编译验证（0 error）与静态对照，**尚未上板验证**（硬件不在手边）。
- 任务栈大小为估算值，建议上板后用 `uxTaskGetStackHighWaterMark()` 实测收紧。

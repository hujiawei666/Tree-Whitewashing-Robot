# RobotHAL → FreeRTOS 教学式移植 实施计划

> **给执行者：** 这是一个**教学计划**，不是自动化任务清单。每一步由**用户本人操作**，执行者（Claude）负责讲"为什么"、给出该写的代码、跑编译验证。步骤用 `- [ ]` 跟踪。

**Goal:** 从 `RobotHAL`（HAL 裸机）复制出 `RobotHAL_RTOS_Lab`，用原生 FreeRTOS API 把 `app_loop()` 的 `while(1)` 拆成多任务，过程中把 FreeRTOS 的核心概念讲透。

**Architecture:** CubeMX 只负责生成骨架与 FreeRTOS 内核（CMSIS_V2 接口，但代码里直接调原生 API）。业务循环体从 `app_main.c` 暴露出来后，用 `xTaskCreate` 建任务；printf 用互斥量保护；涂白完成事件用事件组广播。

**Tech Stack:** STM32F407VETx / HAL / FreeRTOS 内核（CubeMX 中间件）/ 原生 FreeRTOS API / Keil MDK 5.24a (ARMCC 5.06u5, AC5)

**设计依据:** `docs/superpowers/specs/2026-09-30-robot-hal-to-freertos-design.md`

---

## 关于"测试"的说明（重要）

本计划**不用 TDD**。原因：这是个裸机固件工程，没有单元测试框架；且**机器人硬件不在手边**，无法上板运行。

替代的验证手段只有两层：

| 层 | 手段 | 能证明 | 不能证明 |
|---|---|---|---|
| **L0** | Keil 编译 0 error | 语法/链接正确 | 逻辑对不对 |
| **L1** | 与 `RobotHAL_RTOS` 逐行对照 | 设计一致 | 真的能跑起来 |

因此每个任务末尾的"验证"都是这两层，**不要**声称"迁移完成"或"已测试通过"。

## 关于存档

`RobotHAL_RTOS_Lab` 是个新目录，没有 git。为了让每个绿色编译点可回退，**Task 0 里会 `git init`**（只在新副本里，不动原工程）。之后每个任务绿了就 commit。

---

## Task 0: 建工作副本并验证基线

**Files:**
- Create: `RobotHAL_RTOS_Lab/`（由 `RobotHAL/` 整树复制）

- [ ] **Step 0.1: 复制工程**

```bash
cd "/c/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人"
cp -r RobotHAL RobotHAL_RTOS_Lab
```

- [ ] **Step 0.2: 重命名 Keil 工程文件**

```bash
cd RobotHAL_RTOS_Lab/MDK-ARM
mv RobotHAL.uvprojx RobotHAL_RTOS_Lab.uvprojx
mv RobotHAL.uvoptx RobotHAL_RTOS_Lab.uvoptx 2>/dev/null
```

- [ ] **Step 0.3: 改工程内的名字**

编辑 `RobotHAL_RTOS_Lab/MDK-ARM/RobotHAL_RTOS_Lab.uvprojx`，把这三处改成 `RobotHAL_RTOS_Lab`：

```xml
<TargetName>RobotHAL</TargetName>   →  <TargetName>RobotHAL_RTOS_Lab</TargetName>
<OutputName>RobotHAL</OutputName>   →  <OutputName>RobotHAL_RTOS_Lab</OutputName>
```

把 RTE 引用目录也一起改（`MDK-ARM/RTE/_RobotHAL/` → `MDK-ARM/RTE/_RobotHAL_RTOS_Lab/`），并在 uvprojx 里把 `_RobotHAL` 替换为 `_RobotHAL_RTOS_Lab`。
`OutputDirectory` 保持 `RobotHAL_RTOS_Lab\`（相对 MDK-ARM）。

**为什么**：`TargetName` 就是构建时 `-t` 参数的值，名字对不上 Keil 会报 "Target does not exist"。

- [ ] **Step 0.4: 编译验证基线**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab/MDK-ARM"
rm -f cli_build.log
"D:/keil5-new/UV4/UV4.exe" -b RobotHAL_RTOS_Lab.uvprojx -t RobotHAL_RTOS_Lab -o cli_build.log -j0
grep -E "Error\(s\)|Target not created|Program Size" cli_build.log
```

Expected：日志最后一行是 `"...axf" - 0 Error(s), N Warning(s).`，且 `MDK-ARM/RobotHAL_RTOS_Lab/` 下生成 `.axf` 和 `.hex`。

> Keil 的**退出码 1 = 仅有警告**，不是失败。以日志里的 `N Error(s)` 为准。

**为什么先编一次**：不然后面出错时分不清是新引入的，还是复制过程带进来的。

- [ ] **Step 0.5: 建 git 存档点**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab"
printf 'MDK-ARM/*/[!.]*\nMDK-ARM/*.log\n*.uvguix.*\nDebugConfig/\n' > .gitignore
git init -q && git add -A && git commit -qm "chore: 从 RobotHAL 复制的工作副本，编译基线通过"
```

- [ ] **Step 0.6: 讲一段**

> 讲什么：`RobotHAL` 是"轮询 + 定时器驱动"的裸机程序，`main()` 里一个 `while(1)` 依次调用若干件事（教程第 2 章的三种裸机模式）。RTOS 要解决的就是"其中一件事耗时长，整机都卡住"。

---

## Task 1: 核验 HAL 时基 —— 已在交付 A 完成，无需操作

> **2026-10-01 核对结论**：`RobotHAL` 里 HAL 时基**已经是 TIM1**，本任务从"改造"降级为"核验"。

**证据**：`.ioc` 中 `NVIC.TimeBase=TIM1_UP_TIM10_IRQn`、`NVIC.TimeBaseIP=TIM1`；
且 `Core/Src/stm32f4xx_hal_timebase_tim.c` 存在，里面 `htim1.Instance = TIM1`。

- [ ] **Step 1.1: 核验（只读）**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab"
grep -n "TimeBaseIP" RobotHAL_RTOS_Lab.ioc
ls Core/Src/stm32f4xx_hal_timebase_tim.c
grep -n "TIM1" Core/Src/stm32f4xx_hal_timebase_tim.c | head -5
```

Expected：`NVIC.TimeBaseIP=TIM1`；`stm32f4xx_hal_timebase_tim.c` 存在且实现 `HAL_InitTick` 用 TIM1。

- [ ] **Step 1.2: 讲一段（这一步的全部内容）**

> SysTick 是 Cortex-M 的**内核**定时器，FreeRTOS 的心跳（tick）必须独占它——
> 每个 tick 中断里内核都要做调度决策、维护阻塞链表。
>
> 而 `HAL_Delay()` / `HAL_GetTick()` 默认也挂在 SysTick 上。两边抢同一个中断，
> 结果是 **tick 丢拍** 或 **`HAL_Delay` 死等**，都像死机。
>
> 所以 CubeMX 一启用 FreeRTOS 就会提示"换时基"：**SysTick 归内核，HAL 用 TIM1**。
> 交付 A 已经把这件事做了，我们现在白捡一步。（教程 6.2）

- [ ] **Step 1.3: 顺带记一笔**

`Core/Src/stm32f4xx_it.c:189` 现在有一个**空的** `SysTick_Handler`。
它是个强符号，会顶掉 FreeRTOS `port.c` 里的实现 → 链接时报重复定义。
CubeMX 在启用 FreeRTOS 重新生成时会自动删掉它（`RobotHAL_RTOS` 里就没有）。
**Task 2 生成后要回头确认它确实没了。**

- [ ] **Step 1.4: 无需存档**

本任务没有改动文件，不产生 commit。

---

## Task 2: 启用 FreeRTOS 中间件并配参数

- [ ] **Step 2.1: 启用中间件**

CubeMX → `Middleware and Software Packs` → `FREERTOS` → `Interface` 选 **`CMSIS_V2`**。

**为什么是 V2**：教程 6.4 的建议。虽然我们代码里用原生 API，但接口选 V2 生成的内核版本更新、`FreeRTOSConfig.h` 更完整。

- [ ] **Step 2.2: 按表配置**

> **页签更正（2026-10-01）**：下面这些项**全部在 `FREERTOS` → `Config parameters` 页**，
> 不在 `Advanced settings`。`Advanced settings` 页只有 `Newlib settings` 和
> `Project settings`（`Use FW pack heap file`），跟我们无关，不用动。

| 项 | 值 |
|---|---|
| `TICK_RATE_HZ` | `1000` |
| `MAX_PRIORITIES` | `56` |
| `MINIMAL_STACK_SIZE` | `128` |
| `TOTAL_HEAP_SIZE` | `15360` |
| `USE_PREEMPTION` | `Enabled` |
| `USE_MUTEXES` | `Enabled` |
| `CHECK_FOR_STACK_OVERFLOW` | `Option2` |
| `USE_MALLOC_FAILED_HOOK` | `Enabled` |
| `MAX_SYSCALL_INTERRUPT_PRIORITY` | `5` |

`Memory management`（同页底部的下拉框）选 **`heap_4`**。

`Config parameters` 页项目很多，可先用页内搜索框（`Ctrl+F`）逐个定位。

**为什么 heap_4**：教程 8.2.4 —— 首次适应 + 相邻空闲块合并，能缓解碎片。
**为什么栈溢出检查选 Option2**：教程 19.1.5 方法 2（创建时把栈填 `0xa5`，切换时查尾部），比方法 1 精确。
**各项含义的完整讲解见对话记录（2026-10-01）**，含 tick 频率、MAX_PRIORITIES=56 为何是 CMSIS 逼出来的、
TOTAL_HEAP_SIZE 怎么算、heap_4 与 heap_2 的差别。

- [ ] **Step 2.3: 把 defaultTask 优先级调低**

`FREERTOS` → `Tasks and Queues` → `defaultTask` → `Priority` 改成 **`osPriorityLow`**。

**为什么**：CubeMX 的 `defaultTask` 删不掉（教程 6.4.2）。如果不把它压到最低，它可能抢在我们自己的任务前面跑。

- [ ] **Step 2.4: 生成代码**

- [ ] **Step 2.5: 验证**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab"
grep -n "configTICK_RATE_HZ\|configMAX_PRIORITIES\|configTOTAL_HEAP_SIZE\|configUSE_MUTEXES\|configCHECK_FOR_STACK_OVERFLOW\|configUSE_MALLOC_FAILED_HOOK\|configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY" Core/Inc/FreeRTOSConfig.h
ls Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
grep -n "heap_4" MDK-ARM/RobotHAL_RTOS_Lab.uvprojx | head -3
```

Expected：各配置项值与上表一致；`heap_4.c` 存在且已进工程。

- [ ] **Step 2.6: 编译**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab/MDK-ARM"
rm -f cli_build.log
"D:/keil5-new/UV4/UV4.exe" -b RobotHAL_RTOS_Lab.uvprojx -t RobotHAL_RTOS_Lab -o cli_build.log -j0
grep -E "Error\(s\)|Target not created" cli_build.log
```

Expected：`0 Error(s)`。

- [ ] **Step 2.7: 存档**

```bash
git add -A && git commit -qm "feat: 启用 FreeRTOS(CMSIS_V2) 中间件，heap_4 + 栈溢出/malloc 钩子"
```

---

## Task 3: 看懂启动链路

**Files:** 只读，不改

- [ ] **Step 3.1: 打开 `Core/Src/main.c` 看 main() 尾部**

应能看到：

```c
/* Init scheduler */
osKernelInitialize();
MX_FREERTOS_Init();

/* Start scheduler */
osKernelStart();

/* We should never get here as control is now taken by the scheduler */
while (1) { }
```

- [ ] **Step 3.2: 打开 `Core/Src/freertos.c` 看 MX_FREERTOS_Init()**

应能看到它创建了 `defaultTask`。

- [ ] **Step 3.3: 讲一段**

> 三个阶段的职责：
> - `osKernelInitialize()` —— 准备内核运行环境（变量、就绪表），**此时还没有任务在跑**。
> - `MX_FREERTOS_Init()` —— 创建任务/互斥量/事件组等内核对象。**必须在这之后才能创建对象。**
> - `osKernelStart()` —— 启动调度器，选优先级最高的就绪任务开始跑。**它永不返回**。
>
> 所以 `osKernelStart()` 之后那个 `while(1){}` 是**死代码**，永远不会执行。
> 这正是"原来 `main()` 里的业务循环必须搬家"的根本原因：它要是留在 `osKernelStart()` 后面，就永远跑不到。
> （教程 7.6）

- [ ] **Step 3.4: 检查点**

用户能自己回答：*"为什么原来的 `while(1)` 不能留在 main 里？"* 答不上就重讲。

---

## Task 4: 无 RTOS 基础概念

**Files:** 无

- [ ] **Step 4.1: 讲任务与栈**（教程 9.1 / 9.2.1）

> 任务就是一个**永不返回**的函数：`void ATaskFunction(void *pvParameters)`。
> 每个任务有**自己的栈**，所以函数里的局部变量天然是各任务独立的副本；
> 但**全局变量/静态变量只有一份**，多任务访问就要防冲突（Task 8 会用到）。

- [ ] **Step 4.2: 讲状态与调度**（教程 9.3 / 9.4）

> 四个状态：运行 / 就绪(Ready) / 阻塞(Blocked) / 挂起(Suspended)。
> 调度器**永远**选**最高优先级的就绪态**任务来跑。
> 同优先级就轮流（时间片轮转）。
> 关键：**阻塞的任务不占 CPU**。所以"等一个事件"要阻塞着等，不要忙等。

- [ ] **Step 4.3: 讲 tick**（教程 9.3.2）

> `configTICK_RATE_HZ = 1000` → 每 1ms 一次 tick 中断 → 时间片 1ms。
> `vTaskDelay(1)` = 让出 CPU 至少 1 个 tick。

- [ ] **Step 4.4: 检查点**

用户能自己回答：*"任务 A 死循环里不放任何延时，任务 B 会怎样？"*（答案：同优先级时 B 靠时间片还能跑；B 优先级更低则被饿死。）

---

## Task 5: 从 app_loop() 推导任务划分

**Files:** 只读 `App/Src/app_main.c`

- [ ] **Step 5.1: 读 `app_loop()`（约 744-800 行）**

里面有三件事：

1. HMI 模式切换指令（`USART_RX_STA`）+ PS2 手柄 + 自动状态机 `auto_mode_state_machine()`
2. `HIM_chuan_shu()` —— HMI 串口指令处理
3. `hcsr04_nonblock()` —— 超声波轮询

- [ ] **Step 5.2: 一起判断"哪些该独立成任务"**

判断标准：**这件事会不会卡住别的**？

- 超声波 `hcsr04_nonblock()` 里有 TIM5 驱动的 50ms 状态机推进 → 要高频轮询，独立
- `HIM_chuan_shu()` 处理串口指令 → 独立
- 剩下的（PS2 + 模式 + 自动状态机）互相耦合紧、又都是"怎么走" → 合成一个

结论：**3 个任务**。原计划里的 `TaskPaint` 取消——涂白动作其实由 `auto_mode_state_machine()` 直接驱动（`AUTO_PAINTING` / `AUTO_RETREATING` 两个 case），属于"怎么走"，并进 TaskDrive。

- [ ] **Step 5.3: 与参考实现对照**

| 任务 | 内容 | 周期 |
|---|---|---|
| TaskDrive | `app_loop()` | 1 ms |
| TaskUltra | `hcsr04_nonblock()` | 1 ms |
| TaskComm | `HIM_chuan_shu()` | 2 ms |

- [ ] **Step 5.4: ⚠️ 决定：从 `app_loop()` 里**删掉**这两处调用**

`app_loop()` 现在的第 775 行和第 807 行：

```c
    /* ---- HMI 指令 ---- */
    HIM_chuan_shu();          /* 775 行 */

    ...

    /* ---- 超声波轮询 ---- */
    hcsr04_nonblock();        /* 807 行 */
```

**这两行必须删掉**（换成注释说明已搬走）。原因：

- 它们的"新家"是 `TaskComm` 和 `TaskUltra`。如果 `app_loop()` 里留着，
  就会变成**两个任务同时调同一个函数**
- `HIM_chuan_shu()` 内部有 `static` 变量和 22 处延时 —— 被两个任务同时调用，
  状态会互相踩，而且 `TaskDrive` 也会跟着做那些长延时，等于白拆

参考实现 `RobotHAL_RTOS` 的 `app_loop()` 里，这两行已经被替换成注释
（"【交付B】的代码 HIM_chuan_shu() 已挪到 TaskComm 里面" / "hcsr04_nonblock() 已挪到 TaskUltra 里面"）。
**实际改动在 Task 6 落地。**

- [ ] **Step 5.5: 检查点**

用户能自己说清：*"为什么超声波要单独一个任务？"*、*"为什么不需要 TaskPaint？"*

---

## Task 6: 把循环体暴露出来 + 建任务

**Files:**
- Create: `App/Inc/app_tasks.h`
- Create: `App/Src/app_tasks.c`
- Modify: `App/Src/app_main.c`（去掉 3 个函数定义前的 `static`）
- Modify: `Core/Src/freertos.c`（挂载点）
- Modify: `MDK-ARM/RobotHAL_RTOS_Lab.uvprojx`（把新文件加进工程）

- [ ] **Step 6.1: 去掉 `app_main.c` 里 5 个函数的 static**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab"
grep -n "^static void app_loop\|^static void hcsr04_nonblock\|^static void HIM_chuan_shu\|^static void HMI_send_string\|^static void HMI_send_number" App/Src/app_main.c
```

Expected（当前应有 5 行输出）：

```
132:static void HMI_send_string(char *name, char *showdata)
137:static void HMI_send_number(char *name, int num)
158:static void hcsr04_nonblock(void)
278:static void HIM_chuan_shu(void)
744:static void app_loop(void)
```

把它们的 `static` 全部删掉：

| 函数 | 谁要用 |
|---|---|
| `app_loop` | `TaskDrive`（Task 6.3） |
| `hcsr04_nonblock` | `TaskUltra`（Task 6.3） |
| `HIM_chuan_shu` | `TaskComm`（Task 6.3） |
| `HMI_send_string` / `HMI_send_number` | `TaskReport`（Task 12.3） |

同时在 `App/Inc/app_main.h` 里补上这 5 个声明（照抄 `RobotHAL_RTOS/App/Inc/app_main.h` 的写法）：

```c
void HMI_send_string(char *name, char *showdata);
void HMI_send_number(char *name, int num);
void hcsr04_nonblock(void);
void HIM_chuan_shu(void);
void app_loop(void);
```

> `RobotHAL_RTOS` 已经这么改过了，可以直接对照。

- [ ] **Step 6.2: 新建 `App/Inc/app_tasks.h`**

用 **GBK** 编码保存（和 `App/` 下其它文件一致），内容：

```c
/**
 ******************************************************************************
 * @file    app_tasks.h
 * @brief   把原来的裸机主循环拆成 FreeRTOS 任务
 *
 * 注意:归档 FreeRTOS 之后,CubeMX 生成的 main() 是
 *         osKernelInitialize();  MX_FREERTOS_Init();  osKernelStart();
 *       osKernelStart() 不会返回,它后面的 while(1) 是死代码。
 *       原来的 app_loop() 就搬到本文件创建的任务里。
 ******************************************************************************
 */
#ifndef __APP_TASKS_H
#define __APP_TASKS_H

/**
 * @brief  初始化 RTOS 内核对象(目前是 printf 互斥量、涂白事件组)
 * @note   必须在 osKernelInitialize() 之后调用。
 *         调用点:Core/Src/freertos.c 的 MX_FREERTOS_Init() 里,
 *         用户代码区 Init 段。
 */
void app_rtos_init(void);

/**
 * @brief  创建应用任务
 * @note   调用点:Core/Src/freertos.c 的 MX_FREERTOS_Init() 里,
 *         用户代码区 RTOS_THREADS 段。
 */
void app_tasks_create(void);

/* printf 互斥锁 —— 给 app_main.c 的 HMI_send_string / HMI_send_number 用。
 * 内核启动前句柄还是 NULL,这时只有主线程在 printf,不需要锁 —— 直接放行 */
void app_printf_lock(void);
void app_printf_unlock(void);

#endif /* __APP_TASKS_H */
```

> **别在注释里写 `/* */`**：块注释里再出现 `/*` 会触发 `#9-D: nested comment`，C4065。
> **别写 GBK 装不下的字符**（`→`、`⚠️` 之类）：Keil 工程用 GBK，这些字符会导致文件保存/转码失败。

- [ ] **Step 6.3: 新建 `App/Src/app_tasks.c`**（同样 GBK）

```c
/**
 ******************************************************************************
 * @file    app_tasks.c
 * @brief   把原来的裸机主循环拆成 3 个 FreeRTOS 任务
 *
 * 【和原程序的关键差别】
 *   原来:main() 里一个 while(1),三件事依次调用;
 *         只要一件耗时(例如自动模式里的 delay_ms(12000)),整机全停。
 *   现在:每件事各有自己的任务。
 *         某个任务忙或延时(vTaskDelay),其它任务照常跑。
 *
 * 【栈大小的单位】
 *   原生 xTaskCreate 的 usStackDepth 单位是 word(4 字节),不是字节。
 *   教程 9.2.2:传入 100 = 100 word = 400 字节。
 *   下面 256 表示 1024 字节。
 *
 * 【优先级的取值】
 *   原生 API 用裸数字,越大越高,范围 0 ~ configMAX_PRIORITIES-1。
 ******************************************************************************
 */

#include "app_tasks.h"
#include "app_main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

/* app_main.c 里的三个循环体(已去掉 static) */
extern void app_loop(void);
extern void hcsr04_nonblock(void);
extern void HIM_chuan_shu(void);

/* ---------------------------------------------------------------------------
 * 优先级
 * 【2026-10-01 已核实】cmsis_os2.c:464-465 把 CMSIS 优先级 **原样** 当 FreeRTOS
 * 优先级用,没有任何换算:
 *     if (attr->priority != osPriorityNone) { prio = (UBaseType_t)attr->priority; }
 *
 * 所以:CubeMX 的 defaultTask 在 CubeMX 里设了 osPriorityLow(=8),
 *       它的 FreeRTOS 优先级就是 8。
 *       我们自己的任务必须 > 8 才排在它前面。
 * ------------------------------------------------------------------------- */
#define PRIO_DRIVE    12   /* 最高,自动状态机+PS2 */
#define PRIO_ULTRA    11
#define PRIO_COMM     11
#define PRIO_REPORT   10   /* Task 12 的报告任务,最低 */

/* ---------------------------------------------------------------------------
 * 栈大小(单位:word,4 字节)
 *   STK_DRIVE 256 = 1024 字节
 *   STK_ULTRA 128 =  512 字节
 *   STK_COMM  256 = 1024 字节
 * 这只是估算值,Task 10 用 uxTaskGetStackHighWaterMark 实测后再收窄。
 * ------------------------------------------------------------------------- */
#define STK_DRIVE     256
#define STK_ULTRA     128
#define STK_COMM      256

/* ---------------------------------------------------------------------------
 * 内核对象句柄
 * ------------------------------------------------------------------------- */
static SemaphoreHandle_t   s_printf_mutex = NULL;
static EventGroupHandle_t  s_eg_paint     = NULL;
static TaskHandle_t        s_drive = NULL, s_ultra = NULL, s_comm = NULL;

/* ===========================================================================
 * 任务函数
 * =========================================================================== */

/**
 * @brief  驾驶任务:原来是 while(1) 里的"怎么走"部分
 * @note   内部逻辑(app_loop)一字未改,只是从 while(1) 里搬进来。
 *         vTaskDelay(1) 代替原来的"空转 1ms 再来":
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
 * @note   内部由 TIM5 中断驱动一个 50ms 的推进状态,所以 1ms 跑一次足够。
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
}

/* ===========================================================================
 * printf 互斥
 * =========================================================================== */
/*
 * 为什么必须加锁:
 *   HMI_send_string / HMI_send_number 最终走到
 *   printf -> fputc -> HAL_UART_Transmit(&huart1, ...)。
 *
 *   HAL_UART_Transmit 有 **全局状态机**(huart1.gState)且是阻塞式的。
 *   两个任务同时进来:
 *     - 打印内容会互相穿插(指令和数字混在一起)
 *     - 更糟的是 huart1.gState 被踩,某一方会一直卡住
 *
 * 为什么用互斥量而不是二值信号量:
 *   HAL_UART_Transmit 是阻塞的,持有锁的时间不短。
 *   互斥量有"优先级继承":低优先级任务持锁时,若高优先级任务来抢,
 *   低优先级会临时继承高优先级,尽快跑完释放,避免优先级反转。
 *   二值信号量没有这个机制。
 *   (教程 13.1 / 13.3)
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
```

- [ ] **Step 6.4: 挂载到 `Core/Src/freertos.c`**

在 `freertos.c` 顶部 include 区加：

```c
/* USER CODE BEGIN Includes */
#include "app_tasks.h"
/* USER CODE END Includes */
```

在 `MX_FREERTOS_Init()` 的两个用户代码区加调用：

```c
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  app_rtos_init();
  /* USER CODE END Init */
  ...
  /* USER CODE BEGIN RTOS_THREADS */
  app_tasks_create();
  /* USER CODE END RTOS_THREADS */
  ...
}
```

> **必须在 `USER CODE BEGIN/END` 之间**，否则下次 CubeMX 重新生成会删掉（教程 6.6.2）。

- [ ] **Step 6.5: 把 app_tasks.c 加进 Keil 工程**

在 `RobotHAL_RTOS_Lab.uvprojx` 的 `App` 组里，仿照 `app_main.c` 的写法加一条：

```xml
<File>
  <FileName>app_tasks.c</FileName>
  <FileType>1</FileType>
  <FilePath>..\App\Src\app_tasks.c</FilePath>
</File>
```

> **路径必须写全到文件名**。写成 `..\App\Src` 会报 `C4065E: type of input file '..\App\Src' unknown`。
> 加完核对一下 App 组的文件数（应为 11：原有 10 个 + app_tasks.c）。

- [ ] **Step 6.6: 核实 defaultTask 的实际优先级（2026-10-01 已提前查清）**

**已查清，无需再查**：`Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c:464-465`

```c
if (attr->priority != osPriorityNone) {
  prio = (UBaseType_t)attr->priority;   /* 原样传递，不换算 */
}
```

CMSIS 优先级数值：`osPriorityLow=8`、`osPriorityNormal=24`、`osPriorityAboveNormal=32`、`osPriorityHigh=40`。

→ `defaultTask` 若为 `osPriorityLow`，FreeRTOS 优先级就是 **8**；我们自己的任务取 12/11/11/10，全部排在它前面。✅

**只需在执行时核对一眼**（防止 CubeMX 里那格没改成）：

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab"
grep -n "priority" Core/Src/freertos.c | head -3
```

Expected：`.priority = (osPriority_t) osPriorityLow`。
若仍是 `osPriorityNormal`(=24)，则我们的任务必须改成 **28/27/27/26**，否则会被 defaultTask 饿死。

- [ ] **Step 6.7: 编译**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab/MDK-ARM"
rm -f cli_build.log
"D:/keil5-new/UV4/UV4.exe" -b RobotHAL_RTOS_Lab.uvprojx -t RobotHAL_RTOS_Lab -o cli_build.log -j0
grep -E "error|Error\(s\)|Target not created" cli_build.log | head -20
```

Expected：`0 Error(s)`。

- [ ] **Step 6.8: 检查点**

用户能自己回答：
- *"`xTaskCreate` 的栈参数写 256 是什么意思？"*（1024 字节）
- *"为什么用 `vTaskDelay(1)` 而不是原来的空转？"*（让出 CPU）

- [ ] **Step 6.9: 存档**

```bash
git add -A
git commit -qm "feat: 拆出 TaskDrive/TaskUltra/TaskComm，用原生 xTaskCreate 创建"
```

---

## Task 7: 让延时真正让出 CPU（**核心修复**）

**Files:**
- Modify: `App/Src/delay.c`（只改 `delay_ms`，不动 `delay_us`）

> **2026-10-01 发现**：`delay_ms()` 走的是 `HAL_Delay()`，**是忙等**（原地轮询 `uwTick`），
> **不会让出 CPU**。`delay_us()` 是 DWT 忙等，同理。
>
> 全工程 33 处 `delay_ms` 调用，其中 `HIM_chuan_shu()` 占 22 处（最长 `delay_ms(12000)`）、
> `auto_mode_state_machine()` 6 处（最长 5000）、`app_loop()` 1 处（1000）。
>
> **后果**：`TaskDrive` 是最高优先级（12）。它在 `app_loop()` 里忙等 12 秒期间一直是
> **就绪态**，于是 `TaskUltra`(11)、`TaskComm`(11) **永远轮不到** ——
> **移植完不但没解决问题，反而和裸机一样卡。**
>
> **参考答案 `RobotHAL_RTOS` 也有同样的问题**（它的 `delay.c:70-74` 同样是 `HAL_Delay`）。
> 因为它没上过板，这个缺陷没暴露。**本任务是对参考答案的修正。**

- [ ] **Step 7.1: 改 `delay_ms`，让它在调度器运行时阻塞**

`App/Src/delay.c` 的 `delay_ms` 改成：

```c
#include "FreeRTOS.h"
#include "task.h"

void delay_ms(uint32_t ms)
{
    /* 调度器已经跑起来 -> 用 vTaskDelay 真正让出 CPU。
     * 这时本任务进入"阻塞态",其它任务可以运行 —— 这正是 RTOS 的意义。
     *
     * 调度器还没启动(例如 app_init() 阶段) -> 只能忙等。
     * 这时调 vTaskDelay 是非法的(会 configASSERT 挂掉)。 */
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        HAL_Delay(ms);
    }
}
```

**`delay_us` 不动** —— 微秒级延时用不了 `vTaskDelay`（1 tick = 1ms，粒度太粗），
而且 `delay_us(5)` 这种极短忙等无所谓。

- [ ] **Step 7.2: 讲为什么这么改**

> 关键差别（教程 9.4.1）：
>
> | | 忙等（`HAL_Delay`） | 阻塞（`vTaskDelay`） |
> |---|---|---|
> | 任务状态 | **就绪/运行** | **阻塞** |
> | 占不占 CPU | 占满 | 不占 |
> | 同/低优先级任务 | **轮不到** | 正常运行 |
>
> `app_loop()` 里 `delay_ms(12000)` 那 12 秒，改之前是"抱着 CPU 空转 12 秒"，
> 改之后是"睡 12 秒，CPU 交给别人"。**这才是把裸机改成 RTOS 真正换来的东西。**
>
> 为什么用 `xTaskGetSchedulerState()` 而不是直接写 `vTaskDelay`：
> `app_init()` 里有 `delay_ms(200)`，那时调度器还没启动，
> 调 `vTaskDelay` 会触发 FreeRTOS 的断言直接挂掉。

- [ ] **Step 7.3: 编译**

Expected：`0 Error(s)`。

- [ ] **Step 7.4: 讲 `vTaskDelay` vs `xTaskDelayUntil`**（教程 9.6）

> `vTaskDelay(n)`：从**调用时刻**起至少阻塞 n 个 tick。函数体执行时间会累积到周期里 → 周期变长。
> `xTaskDelayUntil(&pre, n)`：从**上次唤醒时刻**起算，补偿了执行时间 → 真正的固定周期。

- [ ] **Step 7.5: 判断我们该用哪个**

`TaskUltra` 跑的是 TIM5 驱动的状态机推进，不是严格周期任务；`TaskDrive` 有大量变长阻塞。
**结论：三个任务都用 `vTaskDelay`，不用 `xTaskDelayUntil`。**

- [ ] **Step 7.6: 讲 tick 换算**（教程 9.3.2）

> `configTICK_RATE_HZ=1000` → 1 tick = 1ms。
> 写"延时 200ms"应该写 `vTaskDelay(pdMS_TO_TICKS(200))`，而不是硬写 `vTaskDelay(200)`
> —— 前者跟 `configTICK_RATE_HZ` 解耦，后者绑死。

- [ ] **Step 7.7: 检查点**

用户能自己回答：
- *"改之前，`TaskDrive` 忙等 12 秒时 `TaskComm` 在干嘛？"*（轮不到，一直就绪着干等）
- *"任务每轮耗时 3ms、想每 10ms 跑一次，`vTaskDelay` 和 `xTaskDelayUntil` 分别是多长周期？"*（13ms / 10ms）

- [ ] **Step 7.8: 存档**

```bash
git add -A && git commit -qm "fix: delay_ms 在调度器运行时改用 vTaskDelay，避免忙等饿死其它任务"
```

---

## Task 8: printf 互斥

**Files:**
- Modify: `App/Src/app_main.c`（在 `HMI_send_string` / `HMI_send_number` 里加锁）
- Modify: `App/Inc/app_main.h`（若还没 include `app_tasks.h`）

- [ ] **Step 8.1: 看当前 HMI 发送函数**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab"
python -c "
d=open('App/Src/app_main.c','rb').read().decode('gbk')
L=d.splitlines()
for i in range(126,160):
    if i < len(L): print(i+1, L[i])
"
```

- [ ] **Step 8.2: 在 `app_main.c` 顶部 include `app_tasks.h`**

```c
#include "app_main.h"
#include "app_tasks.h"      /* 加这一行，为了 app_printf_lock/unlock */
```

- [ ] **Step 8.3: 在两个 HMI 发送函数里加锁**

原代码（`app_main.c:132-140`，去 static 后）：

```c
void HMI_send_string(char *name, char *showdata)
{
    printf("%s=\"%s\"\xff\xff\xff", name, showdata);
}

void HMI_send_number(char *name, int num)
{
    printf("%s=%d\xff\xff\xff", name, num);
}
```

改成：

```c
void HMI_send_string(char *name, char *showdata)
{
    app_printf_lock();
    printf("%s=\"%s\"\xff\xff\xff", name, showdata);
    app_printf_unlock();
}

void HMI_send_number(char *name, int num)
{
    app_printf_lock();
    printf("%s=%d\xff\xff\xff", name, num);
    app_printf_unlock();
}
```

> 只包 `printf` 那一行，**别把整个函数包住** —— 锁的持有时间越短越好。
> 参考实现 `RobotHAL_RTOS/App/Src/app_main.c` 里就是这么写的，可直接对照。

- [ ] **Step 8.4: 讲一段**（教程 10.2 / 13.1 / 13.3）

> 教程第 10 章用 `LCD_PrintString` + `static int bCanUse` 举例：
> 第一次改进"判断和清零分开" —— 还是不行，因为清零那一瞬间的中间态能被抢；
> 第二次改成"进入时减一" —— 还是不行，因为减一本身是**读-改-写三步**，中间也能被抢。
>
> 我们的场景一模一样：三个任务都调 `HMI_send_*`，最终都进 `HAL_UART_Transmit(&huart1, ...)`。
> `huart1.gState` 是共享状态机 —— 两个任务同时进，打印内容互相穿插，还可能有一方永久卡住。
>
> 为什么用互斥量不用二值信号量：`HAL_UART_Transmit` 是阻塞的，持锁时间不短。
> 互斥量有**优先级继承** —— 低优先级任务持锁时若被高优先级任务抢，会临时继承高优先级，
> 尽快跑完释放。二值信号量没这个机制，会出现**优先级反转**（教程 13.3 的 car1/car2/car3 实验）。
>
> 注意：互斥量**不能在 ISR 里用**（教程 13.2.2）。

- [ ] **Step 8.5: 编译**

同上 Task 6.7 的命令。Expected：`0 Error(s)`。

- [ ] **Step 8.6: 检查点**

用户能自己回答：*"为什么不直接把整个 `HMI_send_string` 函数加锁？"*

- [ ] **Step 8.7: 存档**

```bash
git add -A && git commit -qm "feat: HMI 发送用互斥量保护，避免 huart1 状态机被并发踩"
```

---

## Task 9: 中断侧检查

**Files:** 只读核对 `Core/Src/tim.c`

- [ ] **Step 9.1: 查中断优先级**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab"
grep -n "HAL_NVIC_SetPriority" Core/Src/tim.c Core/Src/usart.c Core/Src/gpio.c 2>/dev/null
grep -n "configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY" Core/Inc/FreeRTOSConfig.h
```

**2026-10-01 实测当前值（`RobotHAL` 原样）——这些都要改：**

| 中断 | 当前位置 | 目标 | 文件 |
|---|---|---|---|
| `TIM8_UP_TIM13_IRQn` | 1 | **5** | `Core/Src/tim.c:330` |
| `TIM7_IRQn` | 1 | **5** | `Core/Src/tim.c:379` |
| `TIM5_IRQn` | 2 | **6** | `Core/Src/tim.c:364` |
| `USART1_IRQn` | 3 | 3（不调 FreeRTOS API 可不动） | `Core/Src/usart.c:114` |
| `USART3_IRQn` | 2 | 2（同上） | `Core/Src/usart.c:141` |

阈值 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`。
**抢占优先级数值 ≥ 5 的 ISR 才能调 `...FromISR` API**；数值 < 5 的 ISR 优先级高于内核可屏蔽范围，
在里面调 FreeRTOS API 会破坏内核数据结构。

参考实现 `RobotHAL_RTOS` 里 TIM8=5、TIM7=5、TIM5=6，本次照此调整。
USART 中断当前不调用 FreeRTOS API，可不动；若以后要在里面发队列，必须一并调到 ≥5。

- [ ] **Step 9.2: 若有中断优先级 < 5，调上去**

`HAL_NVIC_SetPriority(TIMx_IRQn, 5, 0);`（数字越小优先级越高，所以数字必须**够大**才安全）。

- [ ] **Step 9.3: 确认 `SysTick_Handler` 已被 CubeMX 移除**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab"
grep -rn "void SysTick_Handler" Core/Src/ App/
```

Expected：**没有输出**。

> Task 0 时这个函数是存在的（`Core/Src/stm32f4xx_it.c:189`，空函数体）。
> 它是**强符号**，而 FreeRTOS `port.c` 通过
> `#define xPortSysTickHandler SysTick_Handler`（在 `FreeRTOSConfig.h` 里）也定义了同名符号
> → 两者冲突，链接报 **multiply defined**。
> CubeMX 在启用 FreeRTOS 后重新生成 `stm32f4xx_it.c` 时会自动删掉它。
> **如果这一步还有输出**，说明生成没生效，要手工从 `stm32f4xx_it.c` 里删掉整个函数。

- [ ] **Step 9.4: 确认 `delay.c` 已经是 DWT 版**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab"
grep -n "DWT->CYCCNT\|SysTick->" App/Src/delay.c
```

Expected：能看到 `DWT->CYCCNT`，**看不到** `SysTick->LOAD` 之类的写操作（注释里提到的不算）。

> **为什么这条必须在 RTOS 之前确认**：原始的 `delay_us` 是拿 SysTick 当私有定时器用的
> —— 写 `SysTick->LOAD`、开 `CTRL`、延时、再关掉。
> 而 FreeRTOS 把 SysTick 当成了自己的心跳：每次中断都要做调度决策。
> 两边同时改同一个寄存器，结果是**调度器停摆或 delay 失灵**。
> `RobotHAL` 里这份 `delay.c` 已经改成用 DWT(CPU 周期计数器)计时，所以直接复制过来就是对的。
> 如果发现还是旧版，必须先用 DWT 重写再继续。

- [ ] **Step 9.5: 讲一段**（教程 17.1）

> 为什么有 `FromISR` 后缀：ISR 里**不能阻塞**。任务版的 API 在队列满/空时会阻塞，ISR 版不会。
>
> `pxHigherPriorityTaskWoken`：ISR 里调用 FromISR 函数唤醒了一个更高优先级任务时，**不在函数内部切任务**（为了效率，一个 ISR 可能连调多次），而是把标志置 `pdTRUE`，由你在 ISR 返回前统一调 `portYIELD_FROM_ISR(x)`。
>
> `configMAX_SYSCALL_INTERRUPT_PRIORITY`：FreeRTOS 靠屏蔽低于这个阈值的优先级来保护临界区。
> 优先级**高于**阈值的 ISR 可以照常打断内核，但**它里面绝对不能调任何 FreeRTOS API**。
> 所以能在 ISR 里用 API 的中断，抢占优先级数值必须 ≥ 阈值。

- [ ] **Step 9.6: 检查点**

用户能自己回答：*"TIM5 的中断优先级设成 3 会怎样？"*（低于阈值，内核临界区屏蔽不住它，在里面调 FromISR 会破坏内核数据结构；FreeRTOS 的 `configASSERT` 会抓到这个错误）

---

## Task 10: 栈水位实测（需硬件，本轮跳过，只留方法）

- [ ] **Step 10.1: 记录方法**（教程 19.1.5 / 19.2.1）

> 加一个低优先级任务定期打印各任务栈水位：

```c
/* 需要 Task 6 里保存的任务句柄 s_drive / s_ultra / s_comm */
printf("drive=%u ultra=%u comm=%u\r\n",
       (unsigned)uxTaskGetStackHighWaterMark(s_drive),
       (unsigned)uxTaskGetStackHighWaterMark(s_ultra),
       (unsigned)uxTaskGetStackHighWaterMark(s_comm));
```

> 返回值单位是 **word**，表示"历史最小剩余栈深度"。
> 若某任务返回 5，说明它最紧张时只剩 5 word = 20 字节 → 栈给少了。
> 经验上留 20%~30% 余量。

- [ ] **Step 10.2: 标注为待办**

**硬件不在手边，这一步只能等上板。** 在 `app_tasks.c` 的栈大小注释旁写一句"待实测收紧"，不要凭空改数值。

---

## Task 11: 与参考实现逐行对照（L1）

**Files:** 只读

- [ ] **Step 11.1: 对照任务划分**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人"
grep -n "osThreadNew\|xTaskCreate" RobotHAL_RTOS/App/Src/app_tasks.c RobotHAL_RTOS_Lab/App/Src/app_tasks.c
```

- [ ] **Step 11.2: 对照栈大小（注意单位差 4 倍）**

| 任务 | 参考实现（字节） | Lab（word） | Lab 换算成字节 |
|---|---|---|---|
| Drive | 1024 | 256 | 1024 |
| Ultra | 512 | 128 | 512 |
| Comm | 1024 | 256 | 1024 |

- [ ] **Step 11.3: 对照 FreeRTOSConfig**

```bash
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人"
diff <(grep -E "^#define config" RobotHAL_RTOS/Core/Inc/FreeRTOSConfig.h) <(grep -E "^#define config" RobotHAL_RTOS_Lab/Core/Inc/FreeRTOSConfig.h)
```

预期有差异（CubeMX 版本/生成参数不同），**逐条判断差异是否可接受**，把结论记下来。

- [ ] **Step 11.4: 对照内存占用**

参考实现：`Code=28992  RO-data=912  RW-data=368  ZI-data=21320`。
Lab 的从 `cli_build.log` 的 `Program Size:` 行读。差异应能解释（多了事件组、任务数不同等）。

- [ ] **Step 11.5: 出 L0/L1 结论**

写明：*"L0 通过（0 error）；L1 通过（任务划分/栈/优先级/中断配置一致）。硬件待验。"*
**不要写"迁移完成"。**

---

## Task 12: 事件组（涂白完成事件）

**Files:**
- Modify: `App/Src/app_tasks.c`（加 TaskReport + 事件位定义）
- Modify: `App/Src/app_main.c`（在涂白完成处 set 事件位）

**场景**（来自 `app_main.c:656-676`，`AUTO_RETREATING` 里"这棵树涂完了"的那个分支）：

```c
if (system_time_ms - state_start_time > 10000) {
    Car_Stop();
    if (tree_count > 1) {
        tree_count--;
        current_state = AUTO_ROTATING;
        ...
    } else {
        ...
        release_auto_resources();
        current_state = MANUAL_MODE;
        ...
    }
}
```

- [ ] **Step 12.1: 讲事件组**（教程 14.1 / 14.2）

> 事件组就是一个整数，**每一位代表一个事件**。
> 和队列/信号量的两个关键区别：
> 1. **广播**：条件满足时唤醒**所有**等待它的任务（信号量只唤醒一个）
> 2. **不清除**：被唤醒的任务自己决定要不要清位
>
> `xEventGroupWaitBits(eg, bits, xClearOnExit, xWaitForAllBits, timeout)`：
> - `xWaitForAllBits = pdFALSE` → **或**（任意一个位满足就醒）
> - `xWaitForAllBits = pdTRUE` → **与**（所有位都满足才醒）
> - `xClearOnExit = pdTRUE` → 醒来的同时把等待的位清掉，且**测试+清零是一次原子操作**

- [ ] **Step 12.2: 在 `app_tasks.h` 里加事件位定义与声明**

事件位**只在这里定义一次**（`app_tasks.c` 和 `app_main.c` 都 include 这个头文件，不要在两处各定义一遍）。

`app_tasks.h` 顶部加 include（`EventBits_t` 类型需要）：

```c
#include "FreeRTOS.h"
#include "event_groups.h"
```

`#endif` 之前加：

```c
/* ---- 涂白事件组的事件位 ---- */
#define EG_BIT_TREE_DONE  ((EventBits_t)(1 << 0))   /* 一棵树涂白完成 */
#define EG_BIT_ALL_DONE   ((EventBits_t)(1 << 1))   /* 全部涂完,回到手动模式 */

/* 设置涂白事件位(广播给所有等待的任务) */
void app_paint_event_set(EventBits_t bits);
```

- [ ] **Step 12.3: 加一个报告任务**

**在 `app_tasks.c` 的栈定义区加上报告任务的栈大小**（`PRIO_REPORT` 已在 Task 6.3 定义过，值是 10）：

```c
#define STK_REPORT    128   /* 512 字节 */
```

**在句柄声明区加上**（改写 Task 6.3 里那一行）：

```c
static TaskHandle_t s_drive = NULL, s_ultra = NULL, s_comm = NULL, s_report = NULL;
```

**加报告任务**：

```c
/**
 * @brief  报告任务:等待涂白事件,刷新 HMI
 * @note   这是事件组的典型用法 -- 广播 + 或等待。
 *         TaskDrive 里涂完一棵树就 set 一次位,本任务被唤醒一次。
 */
static void TaskReport(void *argument)
{
    EventBits_t bits;
    (void)argument;
    for (;;) {
        /* 等待 bit0 或 bit1 任意一个;等到后自动清掉 */
        bits = xEventGroupWaitBits(s_eg_paint,
                                   EG_BIT_TREE_DONE | EG_BIT_ALL_DONE,
                                   pdTRUE,      /* xClearOnExit:退出时清除 */
                                   pdFALSE,     /* xWaitForAllBits:或 */
                                   portMAX_DELAY);
        (void)bits;
        HMI_send_string("box.t3.txt", "涂白记录已更新");
    }
}
```

**在 `app_tasks_create()` 末尾创建它**：

```c
    xTaskCreate(TaskReport, "TaskReport", STK_REPORT, NULL, PRIO_REPORT, &s_report);
```

- [ ] **Step 12.4: 在 `app_main.c` 涂白完成处 set 事件位**

**先在 `app_tasks.c` 里实现 `app_paint_event_set`**（放在 `app_tasks_create()` 之后）：

```c
void app_paint_event_set(EventBits_t bits)
{
    if (s_eg_paint != NULL) {
        xEventGroupSetBits(s_eg_paint, bits);
    }
}
```

在 `app_main.c` 的 `AUTO_RETREATING` 分支里加 set：

```c
if (system_time_ms - state_start_time > 10000) {
    Car_Stop();

    /* 【交付B】一棵树涂白完成 -- 广播出去 */
    app_paint_event_set(EG_BIT_TREE_DONE);

    if (tree_count > 1) {
        tree_count--;
        current_state = AUTO_ROTATING;
        state_start_time = system_time_ms;
        HMI_send_string("box.t3.txt", "准备涂刷下一棵树");
    } else {
        Enable_Acceleration(1);
        Locate_Rle(5000, 3000, CCW);
        Car_Turn_Right();
        delay_ms(4000);
        Car_Stop();
        release_auto_resources();
        current_state = MANUAL_MODE;

        /* 【交付B】全部完成 */
        app_paint_event_set(EG_BIT_ALL_DONE);

        HMI_send_string("moshi.t2.txt", "模式1");
        HMI_send_string("box.t3.txt", "自动涂白完成");
    }
}
```

> 中文文案按 `app_main.c` 里已有的写法照抄（GBK 编码）。

- [ ] **Step 12.5: 编译**

Expected：`0 Error(s)`。

- [ ] **Step 12.6: 做"与"的实验**

把 `TaskReport` 里的 `pdFALSE` 改成 `pdTRUE` 再编译一次，比较行为差异：

- `pdFALSE`（或）：涂完**任意一棵**就醒一次
- `pdTRUE`（与）：只有 `TREE_DONE` 和 `ALL_DONE` **都置位**才醒（即全部涂完才醒）

**这就是教程 14.4 与 14.5 的全部差别——只差一个参数。**

- [ ] **Step 12.7: 讲 `xEventGroupSync`（同步点，教程 14.2.5）**

> 三个任务各干各的，**齐了才继续**（炒菜/买酒/摆台，齐了才开饭）。
> 各任务调 `xEventGroupSync(eg, 我完成的位, 要等的位, timeout)`：
> 先设置自己的位，再等所有人；全部到齐才一起返回，并**自动清除**。
> 机器人里没有天然场景，不用硬加 —— 知道有这个能力即可。

- [ ] **Step 12.8: 检查点**

用户能自己回答：*"事件组和信号量最大的区别是什么？"*（广播 vs 只唤醒一个）
*"`xClearOnExit=pdTRUE` 为什么比 set 完再手动 clear 好？"*（测试+清零原子，中间不会被抢）

- [ ] **Step 12.9: 存档**

```bash
git add -A && git commit -qm "feat: 涂白完成用事件组广播，新增 TaskReport"
```

- [ ] **Step 12.10: 最终结论**

写明：*"L0 通过；L1 通过。任务能否真调度、栈是否够、锁是否有效、中断是否正常 —— 需上板验证。"*

---

## 附：常用命令速查

```bash
# 构建
cd "C:/Users/hujiawei/Desktop/基于STM32F407主控与AI视觉的履带式自适应树木涂白机器人/RobotHAL_RTOS_Lab/MDK-ARM"
rm -f cli_build.log
"D:/keil5-new/UV4/UV4.exe" -b RobotHAL_RTOS_Lab.uvprojx -t RobotHAL_RTOS_Lab -o cli_build.log -j0
grep -E "error|Error\(s\)|Program Size|Target not created" cli_build.log
```

```bash
# 读 GBK 编码的 App/ 源文件某几行
python -c "
d=open('App/Src/app_main.c','rb').read().decode('gbk')
L=d.splitlines()
for i in range(740,780):
    if i < len(L): print(i+1, L[i])
"
```

**三个必须记住的坑**：
1. `xTaskCreate` 栈单位是 **word**（4 字节），CMSIS 的 `stack_size` 是**字节**
2. `.uvprojx` 里的 `<FilePath>` 必须**写全到文件名**，漏了文件名报 `C4065E`
3. App/ 下文件是 **GBK**，注释里不能有 `/* */` 嵌套、不能有 GBK 装不下的字符

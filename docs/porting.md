# MoteOS 移植教程（手把手）

> 本文假设读者：会用 IDE 打开工程、会点编译按钮，仅此而已。
> 每一步都写了"点哪里、看到什么、为什么"。照着做就行，不需要懂原理。

---

## 第 0 章：先花三分钟，搞懂 MoteOS 到底需要什么

MoteOS 是个"事件驱动的协作式内核"。它跑在你的单片机上，只向你要三样东西：

| 要的东西 | 是什么 | 谁提供 |
|---|---|---|
| **tick**（节拍） | 每 1ms 来一次的"心跳"，由某个硬件定时器中断产生，中断里调用 `mote_tick()` | 芯片的 SysTick（绝大多数芯片都有） |
| **wfi**（低功耗） | 没事干时让 CPU 睡觉的指令，来中断会自动醒 | ARM/RISC-V 都有 `wfi` 指令 |
| **临界区** | "关中断/开中断"两个操作，保护共享数据 | 芯片内核头文件里现成的函数 |

所以移植 = **给 tick 中断接一根线 + 让 CPU 会睡觉 + 告诉内核怎么开关中断**。没有汇编，没有链接脚本，没有魔法。

**开始前准备一张对照表**（后面会反复用）：

| 你的芯片 | 用哪个 IDE | 用哪个 port 目录 |
|---|---|---|
| CH32V003/V007/V203/V307（WCH RISC-V） | MounRiver Studio | `moteos/port/ch32v/` |
| CIU32F003（华大电子 M0+） | Keil MDK | `moteos/port/cm0plus/` |
| CH32M030（WCH M0+） | Keil MDK | `moteos/port/cm0plus/` |
| STM32F030（M0+） | Keil MDK | `moteos/port/cm0plus/` |
| STM32F103（M3） | Keil MDK | `moteos/port/cm3/` |
| 其他芯片 | 任意 | 看第 4 章（从模板写） |

---

## 第 1 章：路线 A —— Keil MDK + STM32F103（ARM 芯片）

### 1.1 新建一个空白工程

1. 打开 Keil µVision5 → 菜单 `Project → New µVision Project...`
2. 选一个文件夹（比如 `D:\mote_demo`），工程名填 `blink`，点保存
3. 弹出芯片选择框：搜索框输入 `STM32F103C8`，选中 → 点 OK
4. 弹出 "Manage Run-Time Environment" 窗口 → **直接点 Cancel**（我们用不到 Keil 的软件包）

### 1.2 从 SDK 里拿三个启动文件

STM32F103 的芯片启动文件（没有它芯片不会跑）要从 ST 官方标准库（或你手头的例程）里复制：

1. 找到你的 STM32F10x 标准库目录，找到这三个文件：
   - `Libraries\CMSIS\CM3\DeviceSupport\ST\STM32F10x\startup\arm\startup_stm32f10x_md.s`
   - `Libraries\CMSIS\CM3\DeviceSupport\ST\STM32F10x\system_stm32f10x.c`
   - `Libraries\CMSIS\CM3\CoreSupport\core_cm3.c`（可省略，但建议加上）
2. 把这三个文件复制到 `D:\mote_demo` 里
3. 回到 Keil，在左侧工程树里右键 `Target 1` → `Add Group...`，名字输入 `Startup`
4. 右键刚建的 `Startup` 组 → `Add Existing Files to Group 'Startup'...`，把那三个文件加进来

### 1.3 复制 MoteOS 文件

把 MoteOS 仓库里这些文件复制到 `D:\mote_demo\moteos\`（保持目录结构）：

```
moteos/mote.c                ← 内核，必须
moteos/mote.h                ← 内核头文件，必须
moteos/mote_config.h         ← 配置文件，必须
moteos/mote_task.c           ← 任务层（可选，本例用了）
moteos/mote_mail.c           ← 邮箱（可选，本例用了）
moteos/port/mote_port.c      ← 移植实现（SysTick 接管），必须
moteos/port/cm3/mote_port.h  ← 你的芯片对应 port 头文件，必须
```

> 只复制你需要的。最小系统只需要 `mote.c`、`mote.h`、`mote_config.h`、
> `mote_port.c` 和对应的 `mote_port.h` 五个文件。

回到 Keil，重复第 1.2 步的"建组加文件"操作：

1. 新建组 `MoteOS`
2. 把 `mote.c`、`mote_task.c`、`mote_mail.c`、`port/mote_port.c` 加进 `MoteOS` 组

### 1.4 添加头文件路径（最常见的报错根源）

编译器默认只在工程目录找头文件，要告诉它 MoteOS 的头文件在哪：

1. 点工具栏的魔术棒按钮（`Options for Target`，快捷键 `Alt+F7`）
2. 切到 `C/C++` 标签页
3. 找到 `Include Paths` 那一栏，点右边的 `...` 按钮
4. 点文件夹加号，添加这三条路径：
   ```
   ..\mote_demo\moteos
   ..\mote_demo\moteos\port\cm3
   ..\mote_demo\moteos\port
   ```
   > 也把你 SDK 的 CMSIS 头文件路径加进来（startup 相关头文件）。
5. 依次点 OK 关闭

### 1.5 粘贴例程代码

在 Keil 里新建组 `App`，新建文件 `main.c`（`File → New`，另存为 `D:\mote_demo\main.c`），
内容直接用 `examples/stm32f103/main.c` 的完整代码（LED 500ms 闪烁 + 串口回环 + 心跳任务）。

### 1.6 编译

按 `F7`。出现 `0 Error(s)` 即成功。

### 1.7 报错了？对照这张表

| 报错信息 | 原因 | 怎么改 |
|---|---|---|
| `cannot open source input file "mote.h"` | 头文件路径没加或加错 | 回 1.4 检查三条路径 |
| `SysTick_Handler multiply defined` | 你的工程里已有 SysTick_Handler（startup 文件里是**弱符号**不会冲突；报这个说明哪里又定义了一个） | 删掉你自己写的那个 SysTick_Handler，tick 交给 MoteOS |
| `'NULL' undeclared` | 老版本 mote.h 缺 `<stddef.h>` | 更新 MoteOS 到最新（已修复） |
| `USART1_IRQHandler multiply defined` | 你的代码里也写了串口中断函数 | 二选一：删掉其中一个，或把 `mote_mail_send` 加进你自己的 USART1_IRQHandler 里 |
| `GPIO_CNF_... undeclared` | 你用的库没有 ST 老版寄存器宏 | 用你 SDK 自带的外设库函数写初始化（参考第 5 章） |

### 1.8 下载验证

`Flash → Download`（或 F8）。预期现象：
- LED 每 500ms 翻转一次（定时器驱动）
- 串口助手发什么回什么（邮箱回环）
- 另一颗 LED 每 1s 翻转一次（任务层驱动）

---

## 第 2 章：路线 B —— MounRiver Studio + CH32V003（WCH RISC-V）

### 2.1 新建工程

1. 打开 MounRiver Studio → `File → New → MounRiver Project`
2. 模板选 `CH32V003`（或你的具体型号），填工程名 → Finish
3. 等它生成完成，确认工程里已经有 `ch32v00x.h`、`core_riscv.h`、`startup` 等 WCH 官方文件

### 2.2 复制 MoteOS 文件

复制到工程目录（保持结构）：

```
moteos/mote.c
moteos/mote.h
moteos/mote_config.h
moteos/mote_task.c          ← 可选
moteos/mote_mail.c          ← 可选
moteos/port/mote_port.c
moteos/port/ch32v/mote_port.h   ← 注意：RISC-V 用 ch32v 目录！
```

### 2.3 添加源文件到工程

1. 在左侧工程树右键工程名 → `Import...` → `General → File System` → Next
2. From directory 选 `你的工程目录\moteos`，勾选那 4 个 `.c` 文件（含 port/mote_port.c）→ Finish
3. 确认 4 个文件出现在工程树里，**且不是灰色**（灰色 = 未参与编译）

### 2.4 添加头文件路径

1. 右键工程名 → `Properties`
2. `C/C++ Build → Settings → GNU RISC-V Cross C Compiler → Includes`
3. 在 `Include paths (-I)` 里点加号，添加：
   ```
   ${workspace_loc:/${ProjName}/moteos}
   ${workspace_loc:/${ProjName}/moteos/port/ch32v}
   ${workspace_loc:/${ProjName}/moteos/port}
   ```
4. Apply and Close

### 2.5 替换 main.c

用 `examples/ch32v003/main.c` 的内容替换工程里的 main.c。

> **注意编码**：MRS 新建工程的 main.c 通常是 **GBK 编码**。如果你粘贴进去的
> MoteOS 代码带 UTF-8 中文注释，可能乱码或编译告警。两种办法：
> ① 在 MRS 里对 main.c 右键 → Properties → Resource，把 Text file encoding 改成 UTF-8；
> ② 保持英文注释。

### 2.6 编译与烧录

1. 点锤子图标编译，`0 errors` 即成功
2. 点甲虫图标（Debug），用 WCH-Link 烧录运行
3. 现象同 1.8：LED 500ms 翻转 + 串口回环 + 1s 心跳

### 2.7 报错对照表（CH32V003 专版）

| 报错信息 | 原因 | 怎么改 |
|---|---|---|
| `unknown type name 'IRQn_Type'` | 你的 SDK 版本 core_riscv.h 不能单独包含 | 更新 MoteOS（port/ch32v 已改含 ch32v00x.h） |
| `implicit declaration of function 'SysTick_Config'` | 该版 SDK 没这个函数 | 更新 MoteOS（port/ch32v 里已内联补上） |
| `'GPIO_CNF_...' undeclared` | 例程用了 ST 风格宏，WCH 没有 | 已改用寄存器字面量，更新例程 |
| 中文注释乱码 | GBK/UTF-8 混用 | 按 2.5 的编码说明处理 |

---

## 第 3 章：已有工程怎么接入（不动你的 SysTick）

如果你的工程已经跑起来了（有自己的 SysTick、延时函数、外设库），不想被 MoteOS 接管 SysTick：

1. **不要**把 `mote_port.c` 加进工程
2. 在你的 SysTick 中断函数（一般在 `stm32f10x_it.c` 或 `main.c`）里加一行：

```c
void SysTick_Handler(void)
{
    mote_tick();   /* ← 就加这一行 */
    // ...你原来的代码，比如 Delay_Dec() 之类
}
```

3. 在你的 main.c 里补一个 `mote_idle`：

```c
void mote_idle(void)
{
    __WFI();   /* 你的芯片头文件自带 */
}
```

4. 头文件路径照第 1.4/2.4 节加。

主循环同理：如果你必须保留自己的 `while(1)`，就用 `mote_poll()` 代替 `mote_loop()`：

```c
while (1) {
    if (!mote_poll()) {
        /* MoteOS 没事件可处理，做你自己的事 */
    }
}
```

---

## 第 4 章：没有现成 port 的芯片（照模板抄）

打开 `moteos/port/mote_port_template.c`，它就是全部答案。三步：

### 4.1 tick：让定时器中断叫内核

```c
void Timer1_ISR(void)          /* 你的 1ms 定时器中断，名字按芯片文档 */
{
    mote_tick();               /* 内核内部只做 count++，极快 */
    clear_timer1_flag();       /* 清你的中断标志 */
}
```

中断里**只干这两件事**。所有"到点了要做什么"都由内核在主循环里完成。

### 4.2 睡觉：wfi

```c
void mote_idle(void)
{
    __asm volatile("wfi");     /* ARM/RISC-V 通用；其他架构查手册 */
}
```

**注意两个契约**（内核依赖它们消除"漏睡"竞态）：

1. **本函数在关中断状态下被调用**。内核的睡眠流程是：
   "关中断 → 检查队列确实为空 → 执行 wfi → 恢复中断"。
   ARM/RISC-V 的 wfi 在存在 pending 中断时会立即醒来，
   醒来后内核先恢复中断、让中断先处理，事件不会睡过头。
   所以你的 `mote_idle()` 实现要极短（wfi 级别），不要在里面开中断。

2. **tick 必须持续运行**。唤醒依赖中断（默认 SysTick 每 1ms 一次），
   最坏唤醒延迟 = 1 个 tick。**深度睡眠（STOP/STANDBY 等停掉 tick
   时钟的模式）不支持**：内核没有自带唤醒源，使用这些模式需要你
   自行处理唤醒竞态（先退出深睡、再按上述流程重查队列）。

   > 只有"wfi 级"轻睡眠，才能拍胸脯说"不需要额外唤醒逻辑"；
   > 深睡模式这句话不成立。

### 4.3 临界区：四个小函数（保存/恢复式）

新建 `mote_port.h`，提供四个接口：

```c
#ifndef MOTE_PORT_H
#define MOTE_PORT_H

typedef uint32_t mote_crit_state_t;          /* 中断状态类型 */

static inline mote_crit_state_t mote_crit_enter(void)
{
    /* 1) 读出当前中断开关状态并保存 */
    /* 2) 关全局中断 */
    /* 3) 返回保存的状态 */
}

static inline void mote_crit_exit(mote_crit_state_t s)
{
    /* 恢复 s 里的中断状态（不是无条件打开！） */
}

static inline uint32_t mote_crit_active(void)
{
    /* 返回当前是否处于"关中断"（1=关，0=开） */
}

#endif
```

**为什么必须保存/恢复而不是"关/开"？**
因为内核可能在你已经关了中断的上下文里运行（你自己的临界区、嵌套调用）。
如果 exit 无条件打开中断，就会破坏调用方的原子性——这是经典的隐蔽 bug。
保存/恢复式天然支持嵌套，怎么嵌套都不会错。

**参考现成实现**（直接抄对应的）：

| 内核 | 状态来源 | 实现 |
|---|---|---|
| Cortex-M0+/M3 | PRIMASK | `__get_PRIMASK()` / `__set_PRIMASK()`（见 `port/cm0plus`、`port/cm3`） |
| WCH RISC-V | INTSYSCR（CSR 0x800） | `csrr/csrw 0x800`（见 `port/ch32v`） |
| AVR 等 | 状态寄存器 SREG | `in`/`out` 指令保存恢复 SREG |

---

## 第 5 章：用起来——四件套迷你教程

（以 STM32F103 为例，代码可直接粘贴到你的 main.c 附近）

### 5.1 定时器：LED 闪烁

```c
enum { EVT_LED = 0 };

static mote_timer_t led_timer;

static void led_handler(uint16_t evt, void *param, void *ctx)
{
    GPIOB->ODR ^= (1u << 0);    /* 翻转 PB0 */
}

static const mote_evt_entry_t table[] = {
    [EVT_LED] = MOTE_ENTRY(led_handler, NULL),
};

int main(void)
{
    /* 参数 = MOTE_TICK_MS 毫秒内的时钟周期数（不是频率）。
     * 例：72MHz、1ms 节拍 → 72000000/1000 = 72000 个周期 */
    SysTick_Config(SystemCoreClock / (1000 / MOTE_TICK_MS));
    mote_init(table, sizeof(table) / sizeof(table[0]));
    mote_timer_start(&led_timer, EVT_LED, NULL, 500, true);  /* 每500ms触发 */
    mote_loop();
}
```

### 5.2 事件：按键通知主循环

```c
enum { EVT_LED = 0, EVT_KEY = 1 };

/* 按键中断里： */
void EXTI0_IRQHandler(void)
{
    mote_event_post(EVT_KEY, NULL);          /* 从任意中断投递事件 */
    EXTI->PR = EXTI_PR_PR0;                  /* 清中断标志 */
}

/* 主循环里： */
static void key_handler(uint16_t evt, void *param, void *ctx)
{
    led_handler(evt, param, ctx);            /* 按键 = 手动闪一下灯 */
}
```

### 5.3 值传递：用 MOTE_P / MOTE_U32

```c
/* 中断里投递 ADC 值（禁止传栈变量！） */
mote_event_post(EVT_ADC, MOTE_P(adc_result));   /* 32位值塞进指针 */

/* handler 里取回： */
static void adc_handler(uint16_t evt, void *param, void *ctx)
{
    uint32_t v = MOTE_U32(param);
    if (v > 3000) { /* 过压处理 */ }
}
```

### 5.4 邮箱：串口接收缓冲（大块数据）

```c
MOTE_MAILBOX_DEF(uart_mb, EVT_UART, 8, 64);     /* 8槽×64字节 */

void USART1_IRQHandler(void)
{
    uint8_t c = USART1->DR;
    mote_mail_send(&uart_mb, &c, 1);            /* 中断里深拷贝入槽 */
}

static void uart_handler(uint16_t evt, void *param, void *ctx)
{
    mote_mail_t *mb = param;                     /* param = 邮箱指针 */
    uint8_t buf[64];
    int n;
    while ((n = mote_mail_recv(mb, buf)) > 0) {
        /* 处理 buf 里 n 字节 */
    }
}
```

### 5.5 任务层：周期心跳

```c
static void heartbeat(uint16_t evt, void *param, void *ctx)
{
    /* 每 1000ms 自动被调用一次 */
}

static const mote_task_desc_t tasks[] = {
    MOTE_TASK_DEF(1000, heartbeat, NULL),
};

int main(void)
{
    /* ...mote_init 之后 */
    mote_task_init(tasks, sizeof(tasks) / sizeof(tasks[0]));
    mote_task_start(0);    /* 启动 0 号任务；不启动不占 RAM */
}
```

---

## 第 6 章：FAQ（踩坑大全）

**Q1：LED 不闪，程序像死了一样？**
检查：① `SysTick_Config` 参数算对没有；② `mote_port.c` 加进工程没有；③ 主循环是不是 `mote_loop()` 而不是你自己的 `while(1)`。

**Q2：串口狂收数据时会丢字符？**
邮箱槽数调大（`MOTE_MAILBOX_DEF` 的第 3 个参数）；同时把队列槽数 `MOTE_EVT_QUEUE_SIZE` 调大。注意 `mote_mail_send` 返回 `MOTE_ERR_FULL` 时说明配置小了。

**Q3：handler 里能调什么 API？**
handler 运行在主循环上下文，所以**全部 API 都能调**：`mote_event_post*`、`mote_mail_send/recv`、`mote_timer_start/stop`、`mote_task_start/stop` 都没问题。

**Q4：中断里能调什么？**
只有 `mote_event_post*`、`mote_mail_send`、`mote_tick`。定时器、任务 API 都只能在主循环上下文用。

**Q5：handler 里写 while 等标志行不行？**
不行。handler 必须毫秒级返回，否则整个系统卡死。长流程拆状态机 + 定时器（参考第 5 章），或分多步投递事件给自己。

**Q6：wfi 没省电？**
用电流表测：空闲时电流应明显下降。没降？检查 `mote_idle()` 是不是真的执行了 wfi（有没有被强符号覆盖，见第 3 章）。

**Q7：MOTE_TICK_MS 改成 10 会怎样？**
tick 变成 10ms 一拍，`SysTick_Config(SystemCoreClock / (1000 / MOTE_TICK_MS))` 保持这个公式即可，定时器精度变粗但更省电。

**Q8：事件 ID 可以不连续吗？**
可以，但注册表是数组，ID 越大 Flash 浪费越多。建议枚举连续定义从 0 开始。

**Q9：队列满了怎么办？**
`mote_event_post` 返回 `MOTE_ERR_FULL`，你可以重试或丢弃；状态类事件用 `mote_event_post_replace`（同 ID 只保留最新）。

**Q10：handler 执行时间有没有上限？**
协作式内核没有强制上限，全靠自觉（铁律 1）。实测经验值：48MHz 下 handler 1ms 内返回，系统 10ms 级事件都毫无压力。

---

## 第 7 章：移植成功检查清单

- [ ] 编译 0 error 0 warning（开 `-Wall -Wextra -Werror`，与 CI 同口径）
- [ ] 内核体积核对：map 文件里内核 text <2.5KB、RAM <512B（实测 RV32EC 2.0KB/280B，见 README）
- [ ] map 文件里 `SysTick_Handler` 只出现一次（在 mote_port.o 里）
- [ ] LED 闪烁周期用逻辑分析仪/示波器实测 ≈ 设定值
- [ ] 串口高波特率（115200 以上）连续收发不丢字
- [ ] 空闲电流明显下降（wfi 生效）
- [ ] 重启 100 次无异常（看门狗场景下无卡死）

# MoteOS 使用教程（从零开始）

> 本文假设读者：会 C 语言基础（if/for/函数），已经按《移植教程》把工程跑通。
> 每个概念都从"这词是什么意思"开始讲，再给能直接抄的代码。
> 看到不认识的词，翻第 0 章术语表。

---

## 第 0 章：术语表（不认识哪个查哪个）

这一章解释后面代码里出现的**每一个**名词。先看一遍有个印象，遇到忘了就回来查。

### 0.1 名字和缩写

| 词 | 全称/含义 | 一句话解释 |
|---|---|---|
| **event / evt** | event = 事件 | "发生了什么事"的通知，比如"按键按下了""串口来数据了""时间到了"。`evt` 只是 `event` 的缩写，代码里用短的 |
| **handler** | 处理函数 | 专门处理某一类事件的函数。事件一到，内核就调用它 |
| **回调（callback）** | — | handler 这类函数的统称。特点：**不是你主动调用它，而是系统"反过来"调用它**（"回调"由此得名） |
| **param** | parameter = 参数 | 事件附带的一个"小口袋"（一个指针），可以装一个数值或指向某块数据 |
| **ctx** | context = 上下文 | 注册 handler 时你交给它的"自备工作台"。内核在调用 handler 时原样转交。用不用随你 |
| **post** | 投递 | 把事件放进队列的动作。不是"直接执行"，而是"放进筐里排队" |
| **队列（queue）** | — | 先进先出的筐：先放进去的纸条先被处理 |
| **tick** | 节拍 | 内核的心跳，每 `MOTE_TICK_MS`（默认 1ms）跳一次，由硬件定时器中断驱动 |
| **注册表** | — | 一张对照表：事件编号 → 对应的 handler。内核靠它知道"纸条该给谁" |
| **定时器（timer）** | — | 闹钟。到点自动投递一张纸条 |
| **邮箱（mailbox）** | — | 快递柜。传递"一大块数据"（数组/结构体）用的 |
| **任务（task）** | — | 打卡机。周期性地固定执行某个 handler |
| **ISR / 中断** | Interrupt Service Routine | 硬件事件触发的一段特殊代码（比如串口收到一个字节）。中断里的代码要尽量短 |

### 0.2 代码里的符号

| 写法 | 含义 | 例子 |
|---|---|---|
| `_t` 结尾的类型 | `_t` = type。C 语言惯例：**以 _t 结尾的名字是"自定义类型"** | `mote_timer_t` = MoteOS 的定时器类型；`uint32_t` = 32 位无符号整数 |
| `MOTE_` 前缀 | MoteOS 专属标记 | 防止和你的代码、其他库撞名。看到 `MOTE_` 开头就知道是内核的东西 |
| `void *` | 万能指针 | 可以指向任何类型的数据。`param` 就用它，所以能装任何东西 |
| `uint8_t / uint16_t / uint32_t` | 定宽整数 | 8/16/32 位的无符号整数。跨芯片大小永远不变，嵌入式代码的标准写法 |
| `static` | 两种作用 | ① 修饰变量：让变量**永久存在**（放全局区，函数返回也不消失）；② 修饰函数：只在本文件内可见 |
| `const` | 只读 | 配合芯片特性，const 数据通常被放进 Flash，省 RAM |
| `NULL` | 空指针 = "没有" | 用在"不需要带东西"的场合 |
| `&x` | 取地址 | 把变量 x 的"位置"告诉别人，别人通过指针操作它 |
| `(uint8_t)x` | 强制类型转换 | 把 x 按"8 位无符号整数"重新看待（截断高位） |
| `enum` | 枚举 | 给编号起名字。`EVT_LED` 比 `0` 好记一万倍 |
| `sizeof(x)` | 求大小 | 算出 x 占多少字节。`sizeof(arr)/sizeof(arr[0])` = 数组元素个数 |

### 0.3 两个关键观念

**观念一：handler 是"被叫去的"，不是"主动跑"的。**
传统写法：你写 main，你控制流程。MoteOS 写法：你写 handler，**内核在事件到来时调用它**。
handler 干完活必须马上返回（毫秒级），控制权交回内核。

**观念二：中断里只许"递纸条"，不许"干活"。**
中断打断了正在运行的代码，中断里待得越久，其他事情就被拖得越久。
所以中断里的代码唯一任务：把事件投递出去（post），真正的活留给 handler 在主循环里干。

---

## 第 1 章：MoteOS 的思考方式 + 第一个程序（点灯）

### 1.1 一张图看懂内核

把 MoteOS 想象成一家工厂：

- **事件（event）** = 一张纸条，写着"发生了什么事"
- **handler** = 一个工人，专门处理某一类纸条
- **post** = 把纸条塞进传达室门口的筐（队列）里
- **主循环（mote_loop）** = 传达室大爷，不断从筐里拿纸条、看编号、喊对应工人来干

写 MoteOS 程序 = **只做三件事**：

1. 定义"纸条有哪些种类"（事件 ID 枚举）
2. 定义"每类纸条谁来处理"（handler + 注册表）
3. 在需要的时候"塞纸条"（post）

再记住一条：**工人干活要快（毫秒级），干不完就撕成几张小纸条分几次干。**

### 1.2 点灯程序，每一行、每个词都解释

```c
#include "mote.h"          /* 包含内核头文件。头文件 = 声明"内核提供哪些函数"。
                            * include = 把这份声明抄进来，编译器才知道有这些函数 */

enum { EVT_LED = 0 };      /* enum 枚举：给纸条种类起名字。
                            * EVT_LED = 0 号纸条，意思是"灯该翻转了"。
                            * 编号必须从 0 连续编（为什么：见第 7 章铁律 4）。
                            * EVT 是 Event 的缩写，前缀统一好辨认 */

static mote_timer_t led_timer;  /* 定义一个"闹钟"变量。
                                 * mote_timer_t：_t 结尾 = 类型名（见术语表）。
                                 * static：让这个变量永久存在（函数返回也不消失）。
                                 * 闹钟要一直活到"响"，所以必须 static 或全局。
                                 * 如果写在函数里且不加 static，函数一返回闹钟就废了 */

static void led_handler(uint16_t evt, void *param, void *ctx)
{   /* 定义一个 handler（工人）。签名（参数列表）照着抄就行，内核要求这个格式：
     *   evt   = 收到的是几号纸条（一个 handler 注册多个事件时用它区分）
     *   param = 纸条上粘的东西（小口袋，现在用不上）
     *   ctx   = 注册时你交给它的工作台（现在用不上）
     *   static：这个函数只在本文件用，加 static 是好习惯 */
    GPIOB->ODR ^= (1u << 0);         /* 干活的代码：翻转 PB0 引脚的灯 */
}

static const mote_evt_entry_t table[] = {
    /* 值班表（注册表）！一个数组，每一项 = "几号纸条 → 找谁"。
     * const：这张表运行时永远不变，编译器把它放进 Flash，不占 RAM。
     * [EVT_LED] = ... ：只填第 0 格，其他格子自动为"空" */
    [EVT_LED] = MOTE_ENTRY(led_handler, NULL),
    /* MOTE_ENTRY(handler, ctx)：打包成表项。
     * 第一个参数 = 找谁（led_handler）；
     * 第二个参数 = 工作台（ctx），这里不需要，填 NULL（空指针="没有"） */
};

int main(void)
{
    SysTick_Config(SystemCoreClock / 1000);
    /* 让芯片每 1ms 产生一次 tick 中断（内核心跳，详见《移植教程》）。
     * SysTick_Config 的参数 = 两次中断之间数多少个时钟周期（不是频率）。
     * 72MHz 芯片：1ms 有 72000 个周期，SystemCoreClock/1000 = 72000 → 正好 1ms */

    mote_init(table, 1);
    /* 把值班表交给内核。第二个参数 = 表有几项 = 最大编号+1。
     * 我们只有 0 号纸条，所以是 1 */

    mote_timer_start(&led_timer,   /* 开闹钟。& = 取地址：把闹钟变量的位置告诉内核 */
                     EVT_LED,      /* 闹钟响了，投 0 号纸条 */
                     NULL,         /* 纸条上不粘东西 */
                     500,          /* 500ms 响一次 */
                     true);        /* true = 循环闹钟（响完自动定下一次） */
    mote_loop();                   /* 内核主循环开始运行，永不返回 */
    /* 此后的世界：闹钟响 → 自动投 EVT_LED 纸条 → 大爷拿纸条 → 查表 → 喊 led_handler → 灯翻转 */
}
```

**程序跑起来后发生了什么？**（跟着走一遍就全懂了）

```
芯片每 1ms 产生 tick 中断 → 内核的心跳计数器 +1
数到 500 → 内核发现闹钟到期 → 自动往筐里塞一张 [EVT_LED] 纸条
主循环从筐里拿出纸条 → 查值班表 → 喊 led_handler 干活 → 灯翻转
筐空了 → 让 CPU 睡觉（省电）
1ms 后 tick 中断叫醒 CPU → 继续数数……
```

**要点：灯闪得准不准，取决于闹钟（定时器），不取决于 handler 写得快慢——这就是实时性的来源。**

---

## 第 2 章：纸条（事件）怎么递——post 三兄弟

### 2.1 普通投递 mote_event_post

```c
mote_status_t r = mote_event_post(EVT_LED, NULL);
/* mote_status_t = 内核的"返回状态"类型，用来告诉你结果 */
```

- 谁都能递：**中断里、handler 里、main 里**都行
- 返回值必须看一眼：`MOTE_OK` = 递进去了；`MOTE_ERR_FULL` = 筐满了（默认 16 张纸条全没处理完）。满的时候你自己决定：重试、丢弃、还是调大 `MOTE_EVT_QUEUE_SIZE`

### 2.2 覆盖投递 mote_event_post_replace

场景：按键手抖，中断风暴一口气递了 20 张"按键按下"纸条——筐爆了。

```c
mote_event_post_replace(EVT_KEY, NULL);
```

效果：筐里如果已有 EVT_KEY 纸条，**只更新最新那张，不再新增**。
手再抖，筐里永远只有一张按键纸条。

**什么时候用**：事件代表"当前状态"而不是"发生次数"时——按键、ADC 当前值、界面刷新。这类事件一律用 replace。

### 2.3 延时投递 mote_event_post_delayed

```c
mote_event_post_delayed(EVT_LED, NULL, 100);   /* 100ms 后再把纸条放进筐 */
```

适合"延迟关屏""开机 3 秒后自检"。同时最多 `MOTE_DELAYED_MAX`（默认 4）张在路上，超了返回 `MOTE_ERR_FULL`。

### 2.4 纸条上带东西：MOTE_P / MOTE_U32

纸条的 param 是个 `void *`（万能指针），能粘一个数值或指向数据。

```c
uint32_t adc_val;                    /* 全局变量：永远存在，可以粘 */
/* 中断里：把数值刻在纸条上（≤32 位都能刻） */
mote_event_post(EVT_ADC, MOTE_P(adc_val));
/* MOTE_P(值) = 把数值伪装成指针塞进纸条。P = Param 的缩写 */

/* handler 里：把数值取回来 */
static void adc_handler(uint16_t evt, void *param, void *ctx)
{
    uint32_t v = MOTE_U32(param);    /* 把纸条上的指针还原成 32 位数值 */
    if (v > 3000) { /* 电压超了 */ }
}
```

**为什么不能粘栈变量？** 看反面教材：

```c
void some_function(void)
{
    uint8_t data[4] = {1,2,3,4};     /* data 是局部变量，住在"栈"上 */
    mote_event_post(EVT_RX, data);   /* 把 data 的地址粘到纸条上 */
}   /* ← 函数返回，栈上这块内存立刻被回收、被别的东西占用 */

/* 等大爷喊工人处理纸条时，data 地址指向的内容已经变成垃圾 → 程序莫名抽风 */
```

规则就一句话：**粘全局/静态变量（永远在），或粘 MOTE_P(数值)（刻在纸条上）。大块数据走第 4 章邮箱。**

---

## 第 3 章：闹钟（定时器）怎么定

### 3.1 单次闹钟（响一次就扔）

```c
static mote_timer_t t;                    /* 闹钟变量必须 static 或全局 */

mote_timer_start(&t, EVT_XXX, NULL, 1000, false);
/* 参数顺序：闹钟变量 → 响了投几号纸条 → 纸条上粘什么 → 多少 ms 响 → 是否循环 */
/* false = 只响一次 */
```

### 3.2 循环闹钟（一直响）

```c
mote_timer_start(&t, EVT_XXX, NULL, 50, true);       /* 每 50ms 响一次 */
```

### 3.3 关掉 / 改时间

```c
mote_timer_stop(&t);               /* 关掉。没开过也安全，不会出错 */
mote_timer_restart(&t, 2000);      /* 把时间改成 2 秒后响。前提：它当前是开着的 */
```

### 3.4 三个要注意的点

1. **变量必须 static**（或全局）：闹钟要持续存在直到响（原因见 1.2）
2. **handler 里可以随意开/关闹钟**——这是把"长流程拆成多步"的官方姿势（见 7.1）
3. **中断里不能碰定时器**——定时器 API 只能在主循环上下文用（铁律 3）

---

## 第 4 章：快递柜（邮箱）——大块数据怎么传

纸条只能粘一个小数值。要传**一坨数据**（串口字节流、传感器报文），用邮箱。

邮箱 = 快递柜：发送方把货**复印一份**放进格子，接收方凭纸条（事件）来取。数据是"复印"进去的，所以原数据之后怎么变都不影响柜子里的副本。

```c
MOTE_MAILBOX_DEF(uart_mb,      /* 柜子名字，随便起（宏会帮你生成对应变量） */
                 EVT_UART,     /* 有货到柜时，投几号纸条（取件通知） */
                 8,            /* 柜子有几个格子 */
                 64);          /* 每个格子多大（字节） */
```

### 4.1 经典用法：中断放货，handler 取货

```c
/* 串口中断里——放货（中断里唯一允许的"数据类"操作） */
void USART1_IRQHandler(void)
{
    uint8_t c = (uint8_t)USART1->DR;   /* 读串口寄存器，截成 8 位存进 c */
    mote_mail_send(&uart_mb, &c, 1);
    /* 三个参数：柜子 → 货物的地址（&c = c 的位置）→ 复印多少字节 */
}

/* handler 里——取货 */
static void uart_handler(uint16_t evt, void *param, void *ctx)
{
    /* 纸条的 param 就是"哪个柜子来货了"（内核自动粘上的柜子指针） */
    mote_mail_t *mb = (mote_mail_t *)param;   /* 把万能指针还原成"柜子类型"指针 */
    uint8_t buf[64];
    int n;

    while ((n = mote_mail_recv(mb, buf)) > 0) {   /* 取一箱货；n = 取出多少字节；空柜返回 -1 */
        for (int i = 0; i < n; i++) {
            /* 处理 buf[i]，直到把所有格子清空 */
        }
    }
}
```

### 4.2 格子数怎么算（防丢数据）

公式：`格子数 ≥ 中断最坏情况下一口气来的字节数 ÷ 每格字节数`

例：串口 115200bps = 每秒约 11520 字节。假设最忙时 handler 10ms 没空处理，来了 115 字节；
每格 64 字节 → 格子数取 `115÷64 向上取整 + 1` ≈ 3。取 4~8 更保险。

`mote_mail_send` 返回 `MOTE_ERR_FULL` = 格子全满 = 配置小了，调大或降波特率。

### 4.3 注意

- 发送超过格子大小的数据会被**静默截断**（只复印前 N 字节，N = 格子大小）
- `mote_mail_recv` 返回取出的字节数（=格子大小），空柜返回 -1
- 一个 handler 可以管多个柜子：靠 `param` 区分是哪个柜子来的

---

## 第 5 章：打卡机（任务层）——周期性的活

闹钟和打卡机的区别：

| | 闹钟（定时器） | 打卡机（任务层） |
|---|---|---|
| 工作方式 | 响一次 → 递一张纸条 → handler 收纸条干活 | 到点直接喊 handler 干活，不走纸条筐 |
| 状态 | 无（闹钟自己没记忆） | 有专属状态槽 |
| 适合 | 零散事件、事件流 | 固定的周期工作：按键扫描、屏刷、喂狗 |

```c
static void key_scan(uint16_t evt, void *param, void *ctx)
{
    /* 每 10ms 自动被叫来一次：读按键、消抖、发 EVT_KEY 纸条 */
}

static const mote_task_desc_t tasks[] = {
    /* 任务名单（描述符表）：放 Flash，占的 RAM 可忽略 */
    MOTE_TASK_DEF(10, key_scan),
    /* MOTE_TASK_DEF(周期ms, handler) = "每 10ms 叫一次 key_scan" */
};

int main(void)
{
    /* 在 mote_init 之后 */
    mote_task_init(tasks, 1);   /* 把名单交给内核。1 = 名单上有 1 个任务 */
    mote_task_start(0);         /* 0 号任务打卡上班（按名单上的顺序编号） */
    /* 不 start 的任务 = 不占 RAM（名单在 Flash 里） */
    /* 想停：mote_task_stop(0); */
}
```

**槽位池**：`MOTE_TASK_SLOT_MAX`（默认 4）= 同时上班的任务上限。
名单可以写 20 个任务，但同时只能有 4 个在打卡（第 5 个 `mote_task_start` 返回 `MOTE_ERR_FULL`）。
停掉一个就能再开一个。任务 handler 收到的 evt 固定是 `MOTE_EVT_TASK`（内核专用编号）。

---

## 第 6 章：完整实战——智能小夜灯

需求：光线暗时 LED 渐亮渐暗（呼吸）；按键切换模式；串口可查询状态。

```c
#include "mote.h"

/* 1. 纸条种类：连续编号，从 0 开始 */
enum {
    EVT_ADC = 0,     /* ADC 采完了（中断递） */
    EVT_KEY = 1,     /* 按键按下了（消抖后递） */
    EVT_QUERY = 2,   /* 串口来了查询命令（邮箱递） */
};

/* 2. 全局状态（纸条要粘的、多个 handler 要共享的，都放这） */
static uint32_t g_brightness;      /* 当前亮度 0~100 */
static bool g_breath_on;           /* 呼吸模式开关 */

/* 3. 柜子与闹钟 */
MOTE_MAILBOX_DEF(uart_mb, EVT_QUERY, 4, 32);
static mote_timer_t breath_timer;

/* 4. 工人：各管一摊 */
static void adc_handler(uint16_t evt, void *param, void *ctx)
{
    uint32_t v = MOTE_U32(param);
    if (v < 500) {                        /* 光线暗 */
        g_breath_on = true;               /* 开呼吸 */
    }
}

static void key_handler(uint16_t evt, void *param, void *ctx)
{
    g_breath_on = !g_breath_on;           /* 按键切换模式 */
}

static void query_handler(uint16_t evt, void *param, void *ctx)
{
    char reply[32];
    int n = sprintf(reply, "brightness=%lu breath=%d\r\n",
                    g_brightness, g_breath_on ? 1 : 0);
    /* 把 reply 前 n 字节通过串口发出去 */
}

static void breath_step(uint16_t evt, void *param, void *ctx)
{
    /* 每 20ms 被喊一次：亮度走一步，形成呼吸效果 */
    static int8_t dir = 1;               /* static 局部变量：函数结束也不丢，记住方向 */
    g_brightness += dir;
    if (g_brightness >= 100) dir = -1;
    if (g_brightness <= 5)   dir = 1;
    pwm_set(g_brightness);
}

/* 5. 值班表 + 名单 */
static const mote_evt_entry_t table[] = {
    [EVT_ADC]   = MOTE_ENTRY(adc_handler, NULL),
    [EVT_KEY]   = MOTE_ENTRY(key_handler, NULL),
    [EVT_QUERY] = MOTE_ENTRY(query_handler, NULL),
};

static const mote_task_desc_t tasks[] = {
    MOTE_TASK_DEF(20, breath_step),
};

int main(void)
{
    SysTick_Config(SystemCoreClock / 1000);   /* 参数 = 1ms 内的时钟周期数（见 1.2） */
    mote_init(table, sizeof(table) / sizeof(table[0]));
    /* sizeof(table)/sizeof(table[0]) = 表有几项：总字节数 ÷ 每项字节数。
     * 用这个写法，以后加纸条不用改这个数字 */

    mote_task_init(tasks, 1);

    mote_task_start(0);                  /* 呼吸任务上班 */
    mote_timer_start(&breath_timer, EVT_ADC, NULL, 100, true); /* 每 100ms 采一次光 */

    mote_loop();
}

/* 6. 中断们：只负责递纸条，绝不多干 */
void ADC_IRQHandler(void)
{
    mote_event_post(EVT_ADC, MOTE_P(adc_result));   /* 采样完成，把数值刻在纸条上 */
}

void KEY_IRQHandler(void)
{
    mote_event_post_replace(EVT_KEY, NULL);         /* 防抖：纸条只留一张 */
}

void USART1_IRQHandler(void)
{
    uint8_t c = USART1->DR;
    mote_mail_send(&uart_mb, &c, 1);                /* 放货 */
}
```

**拆解：为什么这么设计**
- 中断全部一行流（递纸条/放货）→ 中断里待的时间最短 → 实时性最好
- 每个功能一个 handler，互不干扰 → 好改、好测、好复用
- 呼吸效果放任务层（固定节奏），灯光判断放事件（异步到达）→ 各回各家

---

## 第 7 章：四条铁律（违反的后果都演示给你看）

### 铁律 1：handler 必须毫秒级返回，绝不阻塞

**反面教材**（千万别这么写）：

```c
static void bad_handler(uint16_t evt, void *param, void *ctx)
{
    while (!(USART1->SR & USART_SR_RXNE)) { }   /* 死等串口数据 → 全厂停工！ */
}
```

后果：主循环卡在这个 handler 里，筐里的纸条越堆越高，其他 handler 全部饿死（系统假死）。
**正确姿势**：把长流程拆成多步，用闹钟/延时纸条推进：

```c
static void step1(uint16_t evt, void *param, void *ctx)
{
    start_something();                          /* 启动动作（不等待） */
    mote_event_post_delayed(EVT_STEP2, NULL, 100);   /* 100ms 后再走下一步 */
}
```

### 铁律 2：纸条只粘永久变量或数值（反面教材见 2.4）

### 铁律 3：哪些 API 能在哪里调（背不下来就抄）

| API | 中断里 | handler/主循环里 |
|---|---|---|
| `mote_event_post*` | 可以 | 可以 |
| `mote_mail_send` | 可以 | 可以 |
| `mote_tick` | 可以（移植层专用） | 可以 |
| `mote_timer_start/stop/restart` | 不行 | 可以 |
| `mote_task_start/stop` | 不行 | 可以 |
| `mote_mail_recv` | 不行 | 可以 |

### 铁律 4：事件 ID 从 0 连续枚举

ID 就是值班表的下标，表按"最大 ID+1"占 Flash。ID 写成 200 号，前面 200 格就白占了。

---

## 第 8 章：出毛病了？排查表

| 症状 | 最可能的原因 | 查哪里 |
|---|---|---|
| LED 完全不闪 | tick 没接上 | `SysTick_Config` 调了没？`mote_port.c` 加工程没？ |
| 事件递了没反应 | 值班表没登记 / ID 超界 | `[EVT_X] = MOTE_ENTRY(...)` 写了没？表大小传对没？ |
| 偶尔丢数据 | 队列/柜子小了 | `MOTE_EVT_QUEUE_SIZE`、邮箱槽数调大，注意返回值 |
| 中断里改全局变量偶发抽风 | 中断和 handler 抢数据 | 数据只走纸条/柜子传，共享变量加临界区 |
| 省不了电 | `mote_idle` 没生效 | 见移植教程 FAQ Q6 |
| 系统周期性卡一下 | 某个 handler 太慢 | 用 `mote_ticks()` 在 handler 头尾打点计时 |

**看时间**：`uint32_t now = mote_ticks();` 返回系统节拍数（单位 `MOTE_TICK_MS`）。

**在 PC 上先测逻辑**：MoteOS 内核可以在电脑上跑（`cmake --build build && ctest`），
业务逻辑先在电脑上验证，再上板，事半功倍。

---

## 第 9 章：FAQ

**Q1：handler 里能 sleep/延时吗？**
没有这个 API。想要"过一会再干"→ `mote_event_post_delayed` 或定时器。

**Q2：一个 handler 能注册多个事件吗？**
能。在值班表里多写几行 `[EVT_A] = MOTE_ENTRY(h, NULL), [EVT_B] = MOTE_ENTRY(h, NULL)`，
handler 里用 `evt` 参数区分是哪个纸条。

**Q3：ctx 参数是干嘛的？**
`MOTE_ENTRY(handler, ctx)` 的第二个参数，会原样传给 handler 的第三个参数。
用来给 handler 配"工作台"：`MOTE_ENTRY(h, &my_device_config)`，一个 handler 服务多个设备。
不需要就传 NULL。

**Q4：post 的纸条一定按顺序处理吗？**
筐是 FIFO（先进先出）。同一时刻最多排队 `MOTE_EVT_QUEUE_SIZE`（默认 16）张。

**Q5：没有注册的 ID 递进去会怎样？**
纸条被内核默默丢掉，不会崩。这是安全网，但也说明你的值班表漏登记了。

**Q6：任务和"定时器+事件"到底选哪个？**
任务 = 固定节奏的周期活（扫描、刷新、喂狗），且不需要在别处被触发；
定时器+事件 = 触发式、灵活（暂停/改周期/多种事件混流）。

**Q7：MOTE_DELAYED_MAX 用完了还能递延时纸条吗？**
返回 `MOTE_ERR_FULL`。要么调大配置，要么改用定时器（定时器无数量上限）。

**Q8：中断里想干点复杂的活？**
正确姿势：中断只递纸条，把活写在 handler 里。这就是 MoteOS 的全部哲学。

**Q9：handler 里用 static 局部变量和用全局变量有区别吗？**
功能上都是"永久存在"；区别是作用域：static 局部变量只有这个 handler 能碰，更安全，推荐。

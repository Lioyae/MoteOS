<p align="right">
  简体中文 | <a href="README_EN.md">English</a>
</p>

<p align="center">
  <img src="brand/moteos-icon.svg" alt="MoteOS Logo" width="160">
</p>

<h1 align="center">MoteOS</h1>

<p align="center">
  <strong>为小容量单片机而生的事件驱动协作式内核</strong><br>
  零汇编 · 零动态内存分配 · 全部资源占用编译期确定
</p>

<p align="center">
  <a href="https://github.com/Lioyae/MoteOS/actions/workflows/build.yml">
    <img src="https://img.shields.io/github/actions/workflow/status/Lioyae/MoteOS/build.yml?style=for-the-badge" alt="Build Status">
  </a>
  <a href="https://github.com/Lioyae/MoteOS">
    <img src="https://img.shields.io/badge/language-C99-2b6cb0?style=for-the-badge" alt="C99">
  </a>
  <a href="https://github.com/Lioyae/MoteOS/stargazers">
    <img src="https://img.shields.io/github/stars/Lioyae/MoteOS?style=for-the-badge&color=d69e2e" alt="Stars">
  </a>
  <a href="https://github.com/Lioyae/MoteOS/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/Lioyae/MoteOS?style=for-the-badge&color=38a169" alt="License">
  </a>
</p>

---

## 简介

MoteOS 是面向小容量单片机（2KB RAM / 16KB Flash 级别）的 C99 事件驱动协作式内核。

- 无汇编、无动态内存分配、无阻塞延时 API
- 全部 RAM/Flash 用量在编译期确定，链接器可验证，CI 交叉编译并断言内核体积
- 支持 ARM Cortex-M0+/M3 与 RISC-V；中断延迟 = tick 中断 + 事件入队/邮箱拷贝的微秒级临界区（与邮箱 item_size 成正比）

## 支持平台

| 内核 | 芯片示例 |
|---|---|
| RISC-V（WCH 青稞） | CH32V003 / CH32V007 / CH32V203 / CH32V307 |
| Cortex-M0+ | CIU32F003 / CH32M030 / STM32F030 |
| Cortex-M3 | STM32F103 |
| x86（宿主机） | PC 上运行内核单元测试 |

## 资源占用

以 CH32V003（16KB Flash / 2KB RAM，开启全部模块）为基准：

| 项 | 占用 |
|---|---|
| 内核 Flash | 约 2KB |
| 内核 RAM | 约 300B（事件队列 16 槽 + 延时槽 4 + 任务槽 4） |
| 留给应用 | 约 14KB Flash / 1.7KB RAM |

## 模块

| 模块 | 说明 |
|---|---|
| 事件队列 | `mote_event_post` / `mote_event_post_replace`（同 ID 只留最新）/ `mote_event_post_delayed`；内置丢弃计数 `mote_dropped_count()` |
| 注册表 | C99 指定初始化器，事件 ID 即下标，O(1) 派发，表常驻 Flash |
| 定时器 | 静态定义；32 位回绕安全；到期自动投递事件；单次定时器"至少一次"送达（队列满时延迟重试）；周期定时器满队丢当次并计数 |
| 任务层 | 周期回调便捷层：描述符在 Flash（handler + ctx + 周期），状态槽池在 RAM，未启动的任务不占 RAM（可选编译） |
| 邮箱 | 静态槽深拷贝，入箱与事件入队在同一临界区原子完成（全有或全无，无竞态窗口）（可选编译） |
| 低功耗 | 队列空闲自动进入 `mote_idle()`（默认 wfi），tick 中断唤醒 |
| 临界区 | 保存/恢复式（PRIMASK / INTSYSCR），支持嵌套 |
| 可观测性 | `mote_dropped_count()` 统一丢事件计数 + `mote_set_drop_hook()` 丢事件回调 |

## 快速开始

```c
#include "mote.h"

enum { EVT_BLINK = 0 };

static mote_timer_t blink_timer;

static void blink_handler(uint16_t evt, void *param, void *ctx)
{
    led_toggle();
}

static const mote_evt_entry_t evt_table[] = {
    [EVT_BLINK] = MOTE_ENTRY(blink_handler, NULL),
};

int main(void)
{
    systick_start(1);  /* 1ms tick，ISR 内调用 mote_tick() */
    mote_init(evt_table, sizeof(evt_table) / sizeof(evt_table[0]));
    mote_timer_start(&blink_timer, EVT_BLINK, NULL, 500, true);

    mote_loop();  /* 永不返回 */
}
```

## 文档

- [移植教程](docs/porting.md)：Keil / MounRiver 工程集成、SysTick 冲突处理、非 CMSIS 芯片移植、检查清单
- [使用教程](docs/usage.md)：术语表、事件 / 定时器 / 邮箱 / 任务层逐行详解、完整实战项目

## 使用规则

1. handler 非阻塞，毫秒级内返回；长流程拆状态机（内核不提供阻塞延时）
2. 事件 param 只传全局/静态指针，或 ≤32bit 值用 `MOTE_P()/MOTE_U32()`；大块数据走邮箱
3. 定时器/任务 API 仅限主循环上下文；`mote_event_post*`、`mote_mail_send` 可进中断
4. 事件 ID 从 0 连续枚举（ID 即注册表下标）

## 配置

所有可配置项集中在 `moteos/mote_config.h`：

```c
#define MOTE_TICK_MS        1    /* 节拍毫秒 */
#define MOTE_EVT_QUEUE_SIZE 16   /* 事件队列槽数 */
#define MOTE_DELAYED_MAX    4    /* 延时投递并发数 */
#define MOTE_ENABLE_TASK    1    /* 任务层开关 */
#define MOTE_TASK_SLOT_MAX  4    /* 同时活跃任务上限 */
#define MOTE_ENABLE_MAILBOX 1    /* 邮箱开关 */
```

## 构建与测试

内核为纯逻辑，单元测试在 PC 上运行：

```bash
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 目录结构

```
moteos/
├── mote.h / mote.c          # 内核
├── mote_config.h            # 唯一配置点
├── mote_task.c              # 任务层（可选编译）
├── mote_mail.c              # 邮箱（可选编译）
└── port/                    # 移植层（按内核分目录：ch32v / cm0plus / cm3 / host）
examples/                    # 各芯片例程
tests/                       # PC 单元测试
docs/                        # 移植与使用教程
brand/                       # 品牌资源
```

## 开源协议

本项目采用 [Apache License 2.0](LICENSE)。

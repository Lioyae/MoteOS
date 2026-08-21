<p align="right">
  简体中文 | <a href="README_EN.md">English</a>
</p>

<p align="center">
  <img src="brand/moteos-icon.svg" alt="MoteOS Logo" width="160">
</p>

<h1 align="center">MoteOS</h1>

<p align="center">
  <strong>为小容量单片机而生的事件驱动协作式内核</strong><br>
  无独立汇编文件（临界区/休眠为内联汇编） · 零动态内存分配 · 全部资源占用编译期确定
</p>

<p align="center">
  <a href="https://github.com/Lioyae/MoteOS/actions/workflows/build.yml">
    <img src="https://img.shields.io/github/actions/workflow/status/Lioyae/MoteOS/build.yml?style=for-the-badge" alt="Build Status">
  </a>
  <a href="https://github.com/Lioyae/MoteOS/tags">
    <img src="https://img.shields.io/github/v/tag/Lioyae/MoteOS?style=for-the-badge&color=2b6cb0" alt="Version">
  </a>
  <a href="https://github.com/Lioyae/MoteOS">
    <img src="https://img.shields.io/badge/language-C99-2b6cb0?style=for-the-badge" alt="C99">
  </a>
  <a href="https://moteos.zane-leo.top/">
    <img src="https://img.shields.io/badge/Docs-中文文档站-dd6b20?style=for-the-badge" alt="MoteOS Docs">
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

- 内核无汇编源文件（移植层使用内联汇编；仍需厂商启动文件/向量表）、无动态内存分配、无阻塞延时 API
- 全部 RAM/Flash 用量在编译期确定，链接器可验证，CI 交叉编译并断言内核体积
- 支持 ARM Cortex-M0+/M3 与 RISC-V；中断延迟 = tick 中断 + 内核临界区（事件入队 O(1)；邮箱拷贝与 item_size 成正比；post_replace 与队列长度成正比）。临界区时长随配置与主频变化，**需按平台实测**（估算公式与实测方法见 [使用教程附录 A](docs/usage.md)）

## 项目状态（重要）

**开发预览（v0.x），CH32V003 已完成首轮板级验证。**

- ✅ 已验证：宿主机单元测试/交错测试（含 ASan/UBSan、多随机种子）、断言开启构建、最坏配置构建（队列 255）、**QEMU 冒烟测试**（Cortex-M3 固定拍：启动/向量表/SysTick/tick→定时器→事件流；**tickless 档：空闲入账/nap 重装/固定拍恢复/漂移检测，时间膨胀执行**）、**gcov 覆盖率门槛（行覆盖 ≥85%）**、**cppcheck 静态分析**、M0+/M3/RV32 交叉编译与体积断言、真实 SDK 例程编译——全部由 CI 自动化
- ✅ 已上板：CH32V003（MounRiver + WCH 官方 EVT SDK）完成 Stage1-16：固定拍、队列压力、replace、延时投递、邮箱、任务、timer 控制、drop hook、`mote_loop/mote_sleep/mote_next_due`、tickless 入账、WFI 唤醒、综合长跑与低功耗测量专用例程；Stage15 tickless + WCH `__WFI()` 长跑到 671000 ticks，`drop=0 err=0`
- ✅ 已实测：Stage16 专用低功耗例程启动后关闭 USART，默认每 2 秒用 PC1/PC0 打短脉冲；10Ω/5Ω shunt 两组数据一致，板级低平台约 9.4mA、LED 亮平台约 10.6mA、唤醒/活动尖峰约 12.3mA。该数字包含板载/外接负载，不代表芯片裸片睡眠电流
- ❌ 未验证：除 CH32V003 外的真实芯片尚未上板。中断延迟、临界区最坏时长、芯片级低功耗电流仍需按目标板测量；CH32V003 已确认 WFI 可唤醒，但具体电流取决于板载 LED、串口、稳压器、调试器和外设连接
- ⚠️ 生产项目使用前，请先按 [移植检查清单](docs/porting.md) 完成板级验证。v1.0.x 的"生产就绪"标签已撤销（见 [更新日志](CHANGELOG.md)）

## 支持平台

| 内核 | 芯片示例 |
|---|---|
| RISC-V（WCH 青稞） | CH32V003 / CH32V007 / CH32V203 / CH32V307 |
| Cortex-M0+ | CIU32F003 / CH32M030 / STM32F030 |
| Cortex-M3 | STM32F103 |
| x86（宿主机） | PC 上运行内核单元测试 |

> CH32V003 已完成首轮上板验证；其他芯片目前仍仅通过交叉编译或 QEMU 验证。

## 资源占用

| 项 | 占用 |
|---|---|
| 内核 Flash（三件套） | Cortex-M0+ 实测 2297B、RV32 以 CI 为准（交叉编译 -Os 实测 mote.o/mote_task.o/mote_mail.o；断言 M0+ <2.5KB、RV32 <2.75KB） |
| 移植层 Flash | `mote_port.o` 固定拍 <512B（CI 单独断言）；tickless 另加约 320~360B（仅 port 层） |
| 内核 RAM | 默认配置约 280B（事件队列 16 槽 + 延时槽 4 + 任务槽 4），CI 断言 <512B（含移植层静态量） |
| 完整点灯例程 | CH32V003 简单例程手动实测：FLASH 2.7KB / RAM 712B；Stage15 综合 tickless/WFI 压测工程：FLASH 9628B / RAM 880B；Stage16 低功耗测量工程：FLASH 3212B / RAM 396B（均含启动文件与栈；**CI 不校验例程体积**）。注意：2KB RAM 芯片上应用数据与栈预算很紧 |

## 模块

| 模块 | 说明 |
|---|---|
| 事件队列 | `mote_event_post` / `mote_event_post_replace`（同 ID 只留最新）/ `mote_event_post_delayed`（含 `_replace` 与 `mote_event_cancel_delayed`）；内置丢弃计数 `mote_dropped_count()` |
| 注册表 | C99 指定初始化器，事件 ID 即下标，O(1) 派发，表常驻 Flash |
| 定时器 | 静态定义；32 位回绕安全；链表按到期时刻排序，到期扫描只遍历到期节点（poll 空转 O(1)）；周期定时器按绝对相位触发（错过拍合并追赶，无累积漂移）；满队策略可选：重试 / 丢弃（严格截止）/ 最新（replace 语义）——注意：**周期定时器满队时丢当次、下一拍正常；单次 RETRY 定时器满队时下一拍重试至送达（重试不计丢弃数、不触发钩子）** |
| 任务层 | 周期回调便捷层：描述符在 Flash（handler + ctx + 周期），状态槽池在 RAM，未启动的任务不占 RAM（可选编译）。注意：**不是 RTOS 任务**——不抢占、handler 被主循环直接同步调用、与事件队列无关 |
| 邮箱 | 静态槽深拷贝，**先入队后入箱**、与事件入队同一临界区原子完成（入队失败邮箱不动，全有或全无，无回滚窗口）；变长消息（每槽 1..item_size 字节且 item_size≤255，`recv` 返回实际存入长度，超长拒绝不截断，每槽额外 1 字节长度开销）；非法构造（slots==0/空指针等）运行时拒绝（可选编译） |
| 低功耗 | deadline 感知：`mote_next_due()` 暴露最近到期时刻，空闲时队列空且无到期项才进 `mote_idle(next_due)`；可选 **tickless**（`MOTE_TICKLESS=1`）按下一 deadline 重装 SysTick 再 wfi，唤醒后恢复固定拍。CH32V003 已验证 WCH `__WFI()` 在 MoteOS 临界区路径下可被 SysTick 唤醒；Stage16 提供启动后关串口的电流测量例程；其他芯片仍需板级确认 |
| 临界区 | 保存/恢复式（PRIMASK / mstatus），支持嵌套 |
| 可观测性 | `mote_dropped_count()` 统一丢事件计数 + `mote_set_drop_hook()` 丢事件回调（钩子仅限事件/邮箱 API） |

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

- 🌐 [MoteOS 中文文档（在线）](https://moteos.zane-leo.top/)：使用教程 / 移植教程 / 提问指南
- [移植教程](docs/porting.md)：Keil / MounRiver 工程集成、SysTick 冲突处理、非 CMSIS 芯片移植、检查清单
- [使用教程](docs/usage.md)：术语表、事件 / 定时器 / 邮箱 / 任务层逐行详解、完整实战项目
- [测试文档](docs/test.md)：测试矩阵、交错测试设计、QEMU 冒烟、覆盖率与静态分析、本地运行方法

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
#define MOTE_TICKLESS       1    /* tickless 空闲（需下面这项） */
#define MOTE_PORT_HCLK_HZ   48000000u  /* 内核主频 Hz，仅 tickless 使用 */
```

> `MOTE_TICKLESS` / `MOTE_PORT_HCLK_HZ` 必须**工程级全局定义**（port 层
> `mote_port.c` 也要编译到），不能只定义在某个 .c 文件里。tickless 使用前
> 请完成 [移植教程](docs/porting.md) 中的 tickless 板级验证清单。
>
> CH32 port 默认由 `mote_port.c` 强接管 `SysTick_Handler`。如果你的工程
> 已有自己的 `SysTick_Handler`，请工程级定义
> `MOTE_PORT_DEFAULT_SYSTICK_HANDLER=0`，并在自定义入口里调用
> `mote_port_systick_handler()`；tickless 工程不要直接调用 `mote_tick()`。

## 构建与测试

内核为纯逻辑，单元测试在 PC 上运行（`ctest` 默认跑三档：常规配置、断言开启构建 `test_moteos_assert`、最坏配置构建 `test_moteos_max`——队列 255/延时槽 16/任务槽 16）：

```bash
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

> 交错测试验证的是内核对**建模并发语义**（临界区内不抢占）的一致性，
> 伪中断注入窗口覆盖 `mote_event_post*` / `mote_mail_send`（入临界区前）/
> `mote_poll`（单步前）/ `mote_process_timers`（定时器遍历中）。
> 这不构成硬件验证；真实硬件时序以板级验证为准（见上方项目状态）。

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

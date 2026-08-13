/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MOTE_CONFIG_H
#define MOTE_CONFIG_H

/* 节拍周期（毫秒） */
#ifndef MOTE_TICK_MS
#define MOTE_TICK_MS 1
#endif

/* 事件队列槽数（每槽 8 字节 RAM） */
#ifndef MOTE_EVT_QUEUE_SIZE
#define MOTE_EVT_QUEUE_SIZE 16
#endif

/* mote_event_post_delayed 最大并发数 */
#ifndef MOTE_DELAYED_MAX
#define MOTE_DELAYED_MAX 4
#endif

/* 任务层开关 */
#ifndef MOTE_ENABLE_TASK
#define MOTE_ENABLE_TASK 1
#endif

/* 任务状态槽数（同时活跃任务上限，描述符本身在 Flash） */
#ifndef MOTE_TASK_SLOT_MAX
#define MOTE_TASK_SLOT_MAX 4
#endif

/* 邮箱开关 */
#ifndef MOTE_ENABLE_MAILBOX
#define MOTE_ENABLE_MAILBOX 1
#endif

/* tickless 空闲：空闲时按下一 deadline 重装 SysTick 再睡（真实低功耗，
 * 而非固定拍唤醒）。需要 MOTE_PORT_HCLK_HZ（内核主频 Hz，ms→周期换算），
 * 参见 docs/porting.md 的 tickless 小节与板级验证清单。0 = 固定拍（默认） */
#ifndef MOTE_TICKLESS
#define MOTE_TICKLESS 0
#endif

/* 内核主频（Hz）。仅 MOTE_TICKLESS=1 时使用；tickless 下必须 ≥1000 */
#ifndef MOTE_PORT_HCLK_HZ
#define MOTE_PORT_HCLK_HZ 0u
#endif

/* 周期定时器/任务追赶上限：落后超过该拍数时放弃旧相位、从当前时刻
 * 重新对齐（防御性上限，防止极端落后时推进循环过长） */
#ifndef MOTE_TIMER_CATCHUP_MAX
#define MOTE_TIMER_CATCHUP_MAX 1000
#endif

/* 断言钩子：内核内部不变量检查。
 * 默认开启：违反时回调 mote_assert_fail()（弱符号实现见 port/mote_port.c，
 * 可重定义为日志/复位等自己的错误处理；默认实现为停机死循环）。
 * 生产环境如需彻底关闭（省 Flash/周期），在构建中自行定义：
 *   -DMOTE_ASSERT(x)=((void)0)   */
#ifndef MOTE_ASSERT
#define MOTE_ASSERT(x)                                                       \
    do {                                                                     \
        if (!(x)) {                                                          \
            mote_assert_fail(__FILE__, __LINE__);                            \
        }                                                                    \
    } while (0)
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* 断言失败钩子（供 MOTE_ASSERT 默认实现调用，用户可重定义为强符号） */
void mote_assert_fail(const char *file, int line);
#ifdef __cplusplus
}
#endif

/* ---- 配置边界校验（编译期 #error，防止静默内存损坏） ----
 * 内核队列计数与状态槽均使用 uint8_t，越界配置会导致
 * 计数回绕、除零或死循环，必须在编译期拦截 */
#if MOTE_TICK_MS < 1 || MOTE_TICK_MS > 1000
#error "MOTE_TICK_MS must be in 1..1000"
#endif
#if MOTE_EVT_QUEUE_SIZE < 1 || MOTE_EVT_QUEUE_SIZE > 255
#error "MOTE_EVT_QUEUE_SIZE must be in 1..255 (kernel uses uint8_t counters)"
#endif
#if MOTE_TASK_SLOT_MAX < 1 || MOTE_TASK_SLOT_MAX > 255
#error "MOTE_TASK_SLOT_MAX must be in 1..255"
#endif
#if MOTE_TICKLESS && MOTE_PORT_HCLK_HZ < 1000
#error "MOTE_TICKLESS requires MOTE_PORT_HCLK_HZ >= 1000 (core clock in Hz)"
#endif

#endif

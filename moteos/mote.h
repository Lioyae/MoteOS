/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MOTE_H
#define MOTE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "mote_config.h"
#include "mote_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MoteOS 使用规约（四条铁律）：
 *
 * 1. handler 必须非阻塞，毫秒级内返回；长流程拆成状态机
 *    （内核不提供阻塞延时 API，从根源杜绝 busy-wait）
 * 2. 传给事件的 param 必须指向全局/静态变量，或 ≤32bit 值用
 *    MOTE_P()/MOTE_U32() 转换；禁止传栈变量；大块数据走邮箱
 * 3. 定时器/任务 API 仅限主循环上下文调用；mote_event_post* 才允许
 *    在中断上下文调用
 * 4. 事件 ID 用连续枚举（从 0 起）；ID 即注册表下标，稀疏会浪费 Flash
 */

typedef void (*mote_handler_t)(uint16_t evt, void *param, void *ctx);

typedef enum {
    MOTE_OK = 0,
    MOTE_ERR_FULL,       /* 队列/槽池已满 */
    MOTE_ERR_NOT_FOUND,  /* 对象不存在（如未启动的定时器） */
    MOTE_ERR_PARAM,      /* 参数非法 */
} mote_status_t;

typedef struct {
    mote_handler_t handler;
    void *ctx;
} mote_evt_entry_t;

typedef struct mote_timer {
    struct mote_timer *next;
    uint32_t due;     /* 绝对到期节拍 */
    uint32_t period;  /* 0 = 单次，非 0 = 周期（ms） */
    uint16_t evt;
    void *param;
    uint8_t policy;   /* 满队行为策略，见 mote_timer_policy_t */
} mote_timer_t;

/* 定时器到期投递遇队列满时的策略 */
typedef enum {
    MOTE_TIMER_POLICY_RETRY = 0,  /* 单次：投递失败保留定时器，下一拍重试
                                   * （至少一次，默认）；周期：等同 DROP */
    MOTE_TIMER_POLICY_DROP,       /* 到期即投，失败即弃并释放定时器（严格截止） */
    MOTE_TIMER_POLICY_LATEST,     /* replace 语义投递：队列里同 ID 只留最新一份 */
} mote_timer_policy_t;

/* 任务层周期触发时传给 handler 的事件值 */
#define MOTE_EVT_TASK 0xFFFFu

/* mote_next_due() 返回值：当前无任何待到期项 */
#define MOTE_TICK_NONE 0xFFFFFFFFu

/* 注册表项 / 值参转换宏 */
#define MOTE_ENTRY(handler, ctx) { (handler), (ctx) }
#define MOTE_P(v)   ((void *)(uintptr_t)(v))
#define MOTE_U32(p) ((uint32_t)(uintptr_t)(p))

void mote_init(const mote_evt_entry_t *evt_table, uint16_t evt_count);
/* 契约：仅允许启动时调用一次（不重置丢事件钩子与测试注入点） */

/* 因队列满/事件无效而被实际丢弃的事件总数（用于可靠性监测）。
 * 注意口径：单次 RETRY 定时器满队时的暂缓重试不计入——事件最终
 * 仍送达，不是丢弃；周期定时器满队丢当次、DROP 策略失败即弃等
 * 真实丢失才计数。 */
uint32_t mote_dropped_count(void);

/* 丢事件钩子：每丢弃一次事件回调一次。
 * 注意：① 在关中断上下文中被调用，必须极短；
 *      ② 钩子可能在中断上下文触发，只允许调用事件/邮箱类 API
 *         （mote_event_post*、mote_mail_send），禁止定时器/任务 API；
 *         mote_mail_send 的拷贝成本（与发送长度成正比，≤item_size）会叠加到
 *         当前临界区时长上，大格子邮箱请勿在钩子内使用；
 *      ③ 钩子内再次触发的丢弃不会递归回调本钩子（防重入） */
typedef void (*mote_drop_hook_t)(uint16_t evt);
/* 注册/替换丢事件钩子（内部带临界区，任意上下文调用均安全；
 * 推荐仅在启动时调用一次）。传 NULL 取消。 */
void mote_set_drop_hook(mote_drop_hook_t hook);

/* 主循环单步：处理定时器/任务，分发一个事件；返回是否分发了事件 */
bool mote_poll(void);

/* 永不返回的主循环：无事件且无到期项时进 mote_idle() 低功耗 */
void mote_loop(void);

/* 由移植层提供：进低功耗。入参为内核已知的下一到期节拍（见 mote_next_due），
 * MOTE_TICK_NONE 表示无待到期项（可长睡到任意中断）。
 * 契约：内核在关中断状态下调用本函数（临界区内）。
 * 固定拍实现应仅为一条 wfi；tickless 实现为编译期常量乘加、
 * 寄存器写与 wfi（无运行时 64 位除法，见 port/mote_port.c），
 * 均应在数十周期量级。pending 中断会唤醒 CPU，唤醒后内核先恢复中断。
 * tickless 移植（MOTE_TICKLESS=1）应：先追平提前唤醒已流逝的时基、
 * 再按 next_due 重装 SysTick、最后 wfi——参考 port/mote_port.c 的
 * 参考实现。深度睡眠（STOP/STANDBY 等会停掉 tick 时钟的模式）不支持，
 * 需自行处理唤醒竞态与唤醒源 */
void mote_idle(uint32_t next_due);

mote_status_t mote_event_post(uint16_t evt, void *param);
mote_status_t mote_event_post_replace(uint16_t evt, void *param);
/* ms 上限 2^31-1（约 24.8 天，回绕比较的数学边界）；ms==0 或超限返回
 * MOTE_ERR_PARAM（0 延时语义含糊，直接拒绝，与定时器 API 同口径） */
mote_status_t mote_event_post_delayed(uint16_t evt, void *param, uint32_t ms);
/* 同 ID 延时投递只留最新：槽池中已存在同 evt 的未到期槽则原地替换
 * due/param（replace 语义），否则占用空闲槽；槽池满返回 MOTE_ERR_FULL。
 * ms 校验与 mote_event_post_delayed 同口径 */
mote_status_t mote_event_post_delayed_replace(uint16_t evt, void *param,
                                              uint32_t ms);
/* 取消未到期的延时投递：释放首个 evt 与 param 均匹配的槽；
 * 无匹配返回 MOTE_ERR_NOT_FOUND（已到期并投递的无法取消） */
mote_status_t mote_event_cancel_delayed(uint16_t evt, void *param);

/* 由移植层的中断服务程序调用，周期 = MOTE_TICK_MS。
 * 契约：只允许单一 tick 中断源调用（默认 SysTick）。
 * 内核对时基 s_tick 的一切读写都在临界区内完成（本函数自带临界区），
 * 与主循环/其它中断上下文并发均安全 */
void mote_tick(void);

/* 可变步长时基推进（供 tickless 移植层在 SysTick 覆盖多拍时调用，
 * 语义 = 连续 advance(ms) 次 mote_tick）。自带临界区，任意上下文安全。
 * 单次推进不建议超过 2^30（回绕比较数学留裕量） */
void mote_tick_advance(uint32_t ms);

/* 设置系统节拍（多用于测试与对时） */
void mote_tick_set(uint32_t ticks);

uint32_t mote_ticks(void);

/* 内核已知的下一到期节拍（定时器 + 延时投递中的最早者）；
 * 无任何待到期项返回 MOTE_TICK_NONE。自带临界区，任意上下文安全，
 * 供 tickless 移植层与低功耗应用决策休眠时长。
 * 注意：返回的是"节拍时刻"而非"剩余毫秒"，须与 mote_ticks() 做
 * 回绕安全比较 ((int32_t)(due - now) > 0 表示尚未到期) */
uint32_t mote_next_due(void);

/* 启动定时器（默认策略：单次=RETRY 至少一次送达，周期=DROP 满队丢当次）。
 * ms 上限 2^31-1（约 24.8 天，回绕比较的数学边界），0 或超限返回 MOTE_ERR_PARAM；
 * policy 越界同样返回 MOTE_ERR_PARAM（运行时校验，不依赖 MOTE_ASSERT）。
 * 内核定时器链表按到期时刻排序，到期扫描只遍历到期节点
 * （poll 空转 O(1)，与定时器数量无关）。
 * 周期定时器按绝对相位触发：due 按周期推进（due += period），错过拍合并追赶，
 * handler/主循环延迟不会造成相位逐周期累积漂移；落后超过
 * MOTE_TIMER_CATCHUP_MAX（默认 1000 拍）时放弃旧相位重新对齐。
 * 需要严格截止时间（超时检测）用 mote_timer_start_ex + MOTE_TIMER_POLICY_DROP，
 * 或保持默认并在 handler 内核对 mote_ticks()。 */
mote_status_t mote_timer_start(mote_timer_t *t, uint16_t evt, void *param,
                               uint32_t ms, bool periodic);
/* 指定满队策略的启动（策略含义见 mote_timer_policy_t） */
mote_status_t mote_timer_start_ex(mote_timer_t *t, uint16_t evt, void *param,
                                  uint32_t ms, bool periodic,
                                  mote_timer_policy_t policy);
mote_status_t mote_timer_stop(mote_timer_t *t);
/* 重新计时：更新 due 并按新到期时刻重排链表；周期定时器同步更新周期。
 * 未启动（不在链表中）返回 MOTE_ERR_NOT_FOUND */
mote_status_t mote_timer_restart(mote_timer_t *t, uint32_t ms);

/* ---- 内部接口（内核模块间使用，应用代码不要直接调用） ---- */

/* 在已进入临界区的上下文中入队；失败自动计入丢弃统计 */
mote_status_t mote_event_enqueue(uint16_t evt, void *param);

/* 记录一次丢弃（统一统计口径并触发钩子） */
void mote_note_dropped(uint16_t evt);

/* 单次睡眠判定（mote_loop 内部使用）：队列空且无到期项时调用
 * mote_idle(next_due)；暴露给测试以覆盖判定分支，应用不要调用 */
void mote_sleep(void);

/* ---- 测试注入（仅定义 MOTE_TEST_INJECT_ENABLE 时可用）：
 *      宿主机交错测试在 API 内部窗口插入"伪中断"执行点，
 *      发布构建不编译、零开销。
 *      注入窗口：mote_event_post*、mote_mail_send（入临界区前）、
 *      mote_poll 单步前、mote_process_timers 列表遍历中 ---- */
#ifdef MOTE_TEST_INJECT_ENABLE
extern void (*s_test_inject)(void);
void mote_test_inject_set(void (*fn)(void));
/* do-while 形式：避免三元 void 分支（GCC 扩展），-pedantic 兼容 */
#define MOTE_TEST_INJECT()                                                     \
    do {                                                                       \
        if (s_test_inject != NULL) {                                           \
            s_test_inject();                                                   \
        }                                                                      \
    } while (0)
#else
#define MOTE_TEST_INJECT() ((void)0)
#endif

#if MOTE_ENABLE_TASK
typedef struct {
    mote_handler_t handler;
    void *ctx;        /* 原样传给 handler 的第三个参数 */
    uint32_t period_ms;
} mote_task_desc_t;

#define MOTE_TASK_DEF(period_ms, handler, ctx) { (handler), (ctx), (period_ms) }

void mote_task_init(const mote_task_desc_t *table, uint16_t count);
/* 启动任务；描述符 period_ms 为 0 或 ≥2^31 时返回 MOTE_ERR_PARAM
 * （0 会退化为每 poll 同步调用，≥2^31 破坏回绕比较数学） */
mote_status_t mote_task_start(uint16_t id);
mote_status_t mote_task_stop(uint16_t id);

/* 内部接口：由 mote_poll 调用 */
void mote_process_tasks(void);

#endif

#if MOTE_ENABLE_MAILBOX

typedef struct {
    uint16_t evt;        /* 有消息时投递的事件 */
    uint16_t item_size;  /* 每槽最大字节数（1..255，见 MOTE_MAILBOX_DEF） */
    uint8_t slots;       /* 槽数 */
    uint8_t head;
    uint8_t count;
    uint8_t *buf;        /* 数据静态存储 */
    uint8_t *lens;       /* 每槽实际存入长度（变长支持，每槽 +1 字节 RAM） */
} mote_mail_t;

#define MOTE_MAILBOX_DEF(name, evt_id, slot_count, item_bytes)                 \
    static uint8_t name##_buf[((slot_count) >= 1 && (slot_count) <= 255        \
                               && (item_bytes) >= 1 && (item_bytes) <= 255)    \
                                  ? (slot_count) * (item_bytes)                \
                                  : -1];                                       \
    static uint8_t name##_lens[((slot_count) >= 1 && (slot_count) <= 255       \
                                && (item_bytes) >= 1 && (item_bytes) <= 255)   \
                                   ? (slot_count)                              \
                                   : -1];                                      \
    static mote_mail_t name = { (evt_id), (item_bytes), (slot_count), 0, 0,    \
                                name##_buf, name##_lens }

/* 入箱：深拷贝 data 前 len 字节（契约：1 ≤ len ≤ item_size ≤ 255；len 超格、
 * 为 0、或邮箱字段非法（slots==0 / buf/lens 为 NULL / item_size 越界）返回
 * MOTE_ERR_PARAM，不静默截断）。入箱与事件入队在同一临界区原子完成。
 * 中断上下文可调用（拷贝成本与 len 成正比，计入中断延迟）。 */
mote_status_t mote_mail_send(mote_mail_t *mb, const void *data, uint16_t len);
/* 取最早一箱：返回实际存入的字节数（1..item_size），空箱返回 -1；
 * 邮箱字段非法（含槽长度域被写坏为 0 或 >item_size）同样返回 -1。
 * 仅限主循环上下文调用。 */
int mote_mail_recv(mote_mail_t *mb, void *data);

#endif

#ifdef __cplusplus
}
#endif

#endif

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

typedef uint16_t mote_evt_id_t;

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
    MOTE_TIMER_POLICY_RETRY = 0,  /* 单次：重试至送达（至少一次，默认）；
                                   * 周期：等同 DROP */
    MOTE_TIMER_POLICY_DROP,       /* 到期即投，失败即弃并释放定时器（严格截止） */
    MOTE_TIMER_POLICY_LATEST,     /* replace 语义投递：队列里同 ID 只留最新一份 */
} mote_timer_policy_t;

/* 任务层周期触发时传给 handler 的事件值 */
#define MOTE_EVT_TASK 0xFFFFu

/* 注册表项 / 值参转换宏 */
#define MOTE_ENTRY(handler, ctx) { (handler), (ctx) }
#define MOTE_P(v)   ((void *)(uintptr_t)(v))
#define MOTE_U32(p) ((uint32_t)(uintptr_t)(p))

void mote_init(const mote_evt_entry_t *evt_table, uint16_t evt_count);
/* 契约：仅允许启动时调用一次（不重置丢事件钩子与测试注入点） */

/* 因队列满/事件无效而被丢弃的事件总数（用于可靠性监测） */
uint32_t mote_dropped_count(void);

/* 丢事件钩子：每丢弃一次事件回调一次。
 * 注意：① 在关中断上下文中被调用，必须极短；
 *      ② 钩子可能在中断上下文触发，只允许调用事件/邮箱类 API
 *         （mote_event_post*、mote_mail_send），禁止定时器/任务 API；
 *         mote_mail_send 的拷贝成本（与 item_size 成正比）会叠加到
 *         当前临界区时长上，大格子邮箱请勿在钩子内使用；
 *      ③ 钩子内再次触发的丢弃不会递归回调本钩子（防重入） */
typedef void (*mote_drop_hook_t)(uint16_t evt);
/* 注册/替换丢事件钩子（内部带临界区，任意上下文调用均安全；
 * 推荐仅在启动时调用一次）。传 NULL 取消。 */
void mote_set_drop_hook(mote_drop_hook_t hook);

/* 主循环单步：处理定时器/任务，分发一个事件；返回是否分发了事件 */
bool mote_poll(void);

/* 永不返回的主循环：无事件时进 mote_idle() 低功耗 */
void mote_loop(void);

/* 由移植层提供：进低功耗。
 * 契约：内核在关中断状态下调用本函数（临界区内），实现必须极短
 * （wfi 级别）；pending 中断会唤醒 CPU，唤醒后内核先恢复中断。
 * 深度睡眠（STOP/STANDBY 等会停掉 tick 时钟的模式）不支持，
 * 需自行处理唤醒竞态与唤醒源 */
void mote_idle(void);

mote_status_t mote_event_post(uint16_t evt, void *param);
mote_status_t mote_event_post_replace(uint16_t evt, void *param);
/* ms 上限 2^31-1（约 24.8 天，回绕比较的数学边界）；ms==0 或超限返回
 * MOTE_ERR_PARAM（0 延时语义含糊，直接拒绝，与定时器 API 同口径） */
mote_status_t mote_event_post_delayed(uint16_t evt, void *param, uint32_t ms);

/* 由移植层的中断服务程序调用，周期 = MOTE_TICK_MS。
 * 契约：只允许单一 tick 中断源调用（默认 SysTick）。
 * 内核对时基 s_tick 的一切读写都在临界区内完成（本函数自带临界区），
 * 与主循环/其它中断上下文并发均安全 */
void mote_tick(void);

/* 设置系统节拍（多用于测试与对时） */
void mote_tick_set(uint32_t ticks);

uint32_t mote_ticks(void);

/* 启动定时器（默认策略：单次=RETRY 至少一次送达，周期=DROP 满队丢当次）。
 * ms 上限 2^31-1（约 24.8 天，回绕比较的数学边界），0 或超限返回 MOTE_ERR_PARAM。
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
mote_status_t mote_timer_restart(mote_timer_t *t, uint32_t ms);

/* ---- 内部接口（内核模块间使用，应用代码不要直接调用） ---- */

/* 在已进入临界区的上下文中入队；失败自动计入丢弃统计 */
mote_status_t mote_event_enqueue(uint16_t evt, void *param);

/* 记录一次丢弃（统一统计口径并触发钩子） */
void mote_note_dropped(uint16_t evt);

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
    uint16_t item_size;  /* 每槽字节数 */
    uint8_t slots;       /* 槽数 */
    uint8_t head;
    uint8_t count;
    uint8_t *buf;        /* 静态存储 */
} mote_mail_t;

#define MOTE_MAILBOX_DEF(name, evt_id, slot_count, item_bytes)                 \
    static uint8_t name##_buf[((slot_count) >= 1 && (slot_count) <= 255        \
                               && (item_bytes) >= 1)                           \
                                  ? (slot_count) * (item_bytes)                \
                                  : -1];                                       \
    static mote_mail_t name = { (evt_id), (item_bytes), (slot_count), 0, 0,    \
                                name##_buf }

mote_status_t mote_mail_send(mote_mail_t *mb, const void *data, uint16_t len);
int mote_mail_recv(mote_mail_t *mb, void *data);

#endif

#ifdef __cplusplus
}
#endif

#endif

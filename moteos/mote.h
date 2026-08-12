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
} mote_timer_t;

/* 任务层周期触发时传给 handler 的事件值 */
#define MOTE_EVT_TASK 0xFFFFu

/* 注册表项 / 值参转换宏 */
#define MOTE_ENTRY(handler, ctx) { (handler), (ctx) }
#define MOTE_P(v)   ((void *)(uint32_t)(v))
#define MOTE_U32(p) ((uint32_t)(p))

void mote_init(const mote_evt_entry_t *evt_table, uint16_t evt_count);

/* 主循环单步：处理定时器/任务，分发一个事件；返回是否分发了事件 */
bool mote_poll(void);

/* 永不返回的主循环：无事件时进 mote_idle() 低功耗 */
void mote_loop(void);

/* 由移植层提供：进低功耗（默认 wfi） */
void mote_idle(void);

mote_status_t mote_event_post(uint16_t evt, void *param);
mote_status_t mote_event_post_replace(uint16_t evt, void *param);
mote_status_t mote_event_post_delayed(uint16_t evt, void *param, uint32_t ms);

/* 由移植层的中断服务程序调用，周期 = MOTE_TICK_MS */
void mote_tick(void);

/* 设置系统节拍（多用于测试与对时） */
void mote_tick_set(uint32_t ticks);

uint32_t mote_ticks(void);

mote_status_t mote_timer_start(mote_timer_t *t, uint16_t evt, void *param,
                               uint32_t ms, bool periodic);
mote_status_t mote_timer_stop(mote_timer_t *t);
mote_status_t mote_timer_restart(mote_timer_t *t, uint32_t ms);

#if MOTE_ENABLE_TASK

typedef struct {
    mote_handler_t handler;
    uint32_t period_ms;
} mote_task_desc_t;

#define MOTE_TASK_DEF(period_ms, handler) { (handler), (period_ms) }

void mote_task_init(const mote_task_desc_t *table, uint16_t count);
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
    static uint8_t name##_buf[(slot_count) * (item_bytes)];                    \
    static mote_mail_t name = { (evt_id), (item_bytes), (slot_count), 0, 0,    \
                                name##_buf }

mote_status_t mote_mail_send(mote_mail_t *mb, const void *data, uint16_t len);
int mote_mail_recv(mote_mail_t *mb, void *data);

#endif

#ifdef __cplusplus
}
#endif

#endif

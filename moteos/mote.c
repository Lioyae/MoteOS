/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mote.h"

typedef struct {
    uint16_t evt;
    void *param;
} mote_qitem_t;

static struct {
    mote_qitem_t items[MOTE_EVT_QUEUE_SIZE];
    uint8_t head;
    uint8_t count;
} s_q;

static const mote_evt_entry_t *s_evt_table;
static uint16_t s_evt_count;

static volatile uint32_t s_tick;
static mote_timer_t *s_timers;

#if MOTE_DELAYED_MAX > 0
static struct {
    uint32_t due;
    uint16_t evt;
    void *param;
    uint8_t used;
} s_delayed[MOTE_DELAYED_MAX];
#endif

/* ---------------- 事件队列 ---------------- */

static void mote_q_push(uint16_t evt, void *param)
{
    uint8_t idx = (uint8_t)((s_q.head + s_q.count) % MOTE_EVT_QUEUE_SIZE);
    s_q.items[idx].evt = evt;
    s_q.items[idx].param = param;
    s_q.count++;
}

static bool mote_q_pop(uint16_t *evt, void **param)
{
    if (s_q.count == 0) {
        return false;
    }
    *evt = s_q.items[s_q.head].evt;
    *param = s_q.items[s_q.head].param;
    s_q.head = (uint8_t)((s_q.head + 1) % MOTE_EVT_QUEUE_SIZE);
    s_q.count--;
    return true;
}

void mote_init(const mote_evt_entry_t *evt_table, uint16_t evt_count)
{
    s_q.head = 0;
    s_q.count = 0;
    s_evt_table = evt_table;
    s_evt_count = evt_count;
    s_tick = 0;
    s_timers = NULL;
#if MOTE_DELAYED_MAX > 0
    for (int i = 0; i < MOTE_DELAYED_MAX; i++) {
        s_delayed[i].used = 0;
    }
#endif
}

mote_status_t mote_event_post(uint16_t evt, void *param)
{
    mote_status_t st;

    MOTE_ENTER_CRITICAL();
    if (s_q.count >= MOTE_EVT_QUEUE_SIZE) {
        st = MOTE_ERR_FULL;
    } else {
        mote_q_push(evt, param);
        st = MOTE_OK;
    }
    MOTE_EXIT_CRITICAL();
    return st;
}

mote_status_t mote_event_post_replace(uint16_t evt, void *param)
{
    mote_status_t st;

    MOTE_ENTER_CRITICAL();
    /* 从新到旧查找：覆盖最新一条同 ID 事件（latest wins） */
    for (uint8_t i = s_q.count; i > 0; i--) {
        uint8_t idx = (uint8_t)((s_q.head + i - 1) % MOTE_EVT_QUEUE_SIZE);
        if (s_q.items[idx].evt == evt) {
            s_q.items[idx].param = param;
            MOTE_EXIT_CRITICAL();
            return MOTE_OK;
        }
    }
    if (s_q.count >= MOTE_EVT_QUEUE_SIZE) {
        st = MOTE_ERR_FULL;
    } else {
        mote_q_push(evt, param);
        st = MOTE_OK;
    }
    MOTE_EXIT_CRITICAL();
    return st;
}

/* ---------------- 时基与定时器 ---------------- */

void mote_tick(void)
{
    s_tick++;
}

void mote_tick_set(uint32_t ticks)
{
    s_tick = ticks;
}

uint32_t mote_ticks(void)
{
    return s_tick;
}

static void mote_timer_unlink(mote_timer_t *t)
{
    mote_timer_t **pp = &s_timers;

    while (*pp != NULL) {
        if (*pp == t) {
            *pp = t->next;
            t->next = NULL;
            return;
        }
        pp = &(*pp)->next;
    }
}

static bool mote_timer_linked(mote_timer_t *t)
{
    for (mote_timer_t *p = s_timers; p != NULL; p = p->next) {
        if (p == t) {
            return true;
        }
    }
    return false;
}

mote_status_t mote_timer_start(mote_timer_t *t, uint16_t evt, void *param,
                               uint32_t ms, bool periodic)
{
    if (t == NULL || ms == 0) {
        return MOTE_ERR_PARAM;
    }
    mote_timer_stop(t); /* 重复 start 视为重启 */
    t->due = s_tick + ms;
    t->period = periodic ? ms : 0;
    t->evt = evt;
    t->param = param;
    t->next = s_timers;
    s_timers = t;
    return MOTE_OK;
}

mote_status_t mote_timer_stop(mote_timer_t *t)
{
    if (t == NULL) {
        return MOTE_ERR_PARAM;
    }
    mote_timer_unlink(t);
    return MOTE_OK;
}

mote_status_t mote_timer_restart(mote_timer_t *t, uint32_t ms)
{
    if (t == NULL || ms == 0) {
        return MOTE_ERR_PARAM;
    }
    if (!mote_timer_linked(t)) {
        return MOTE_ERR_NOT_FOUND;
    }
    t->due = s_tick + ms;
    if (t->period != 0) {
        t->period = ms;
    }
    return MOTE_OK;
}

mote_status_t mote_event_post_delayed(uint16_t evt, void *param, uint32_t ms)
{
#if MOTE_DELAYED_MAX > 0
    int free_slot = -1;
    mote_status_t st;

    MOTE_ENTER_CRITICAL();
    for (int i = 0; i < MOTE_DELAYED_MAX; i++) {
        if (!s_delayed[i].used && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot < 0) {
        st = MOTE_ERR_FULL;
    } else {
        s_delayed[free_slot].used = 1;
        s_delayed[free_slot].due = s_tick + ms;
        s_delayed[free_slot].evt = evt;
        s_delayed[free_slot].param = param;
        st = MOTE_OK;
    }
    MOTE_EXIT_CRITICAL();
    return st;
#else
    (void)evt;
    (void)param;
    (void)ms;
    return MOTE_ERR_FULL;
#endif
}

/* ---------------- 分发与主循环 ---------------- */

static void mote_process_timers(void)
{
    mote_timer_t *t = s_timers;

    while (t != NULL) {
        mote_timer_t *next = t->next; /* 安全迭代：先缓存后继 */
        if ((int32_t)(s_tick - t->due) >= 0) {
            if (t->period != 0) {
                t->due = s_tick + t->period;
                mote_event_post(t->evt, t->param);
            } else {
                mote_timer_unlink(t);
                mote_event_post(t->evt, t->param);
            }
        }
        t = next;
    }

#if MOTE_DELAYED_MAX > 0
    for (int i = 0; i < MOTE_DELAYED_MAX; i++) {
        uint16_t evt;
        void *param;
        bool fire;

        MOTE_ENTER_CRITICAL();
        fire = s_delayed[i].used && (int32_t)(s_tick - s_delayed[i].due) >= 0;
        if (fire) {
            s_delayed[i].used = 0;
            evt = s_delayed[i].evt;
            param = s_delayed[i].param;
        }
        MOTE_EXIT_CRITICAL();
        if (fire) {
            mote_event_post(evt, param);
        }
    }
#endif
}

bool mote_poll(void)
{
    uint16_t evt;
    void *param;
    bool got;

    mote_process_timers();
#if MOTE_ENABLE_TASK
    mote_process_tasks();
#endif
    MOTE_ENTER_CRITICAL();
    got = mote_q_pop(&evt, &param);
    MOTE_EXIT_CRITICAL();

    if (got) {
        if (evt < s_evt_count && s_evt_table != NULL) {
            const mote_evt_entry_t *e = &s_evt_table[evt];
            if (e->handler != NULL) {
                e->handler(evt, param, e->ctx);
            }
        }
        return true;
    }
    return false;
}

void mote_loop(void)
{
    for (;;) {
        if (!mote_poll()) {
            mote_idle();
        }
    }
}

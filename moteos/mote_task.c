/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mote.h"

#if MOTE_ENABLE_TASK

typedef struct {
    uint16_t id;
    uint32_t due;
    bool active;
} mote_task_slot_t;

static const mote_task_desc_t *s_task_table;
static uint16_t s_task_count;
static mote_task_slot_t s_slots[MOTE_TASK_SLOT_MAX];

void mote_task_init(const mote_task_desc_t *table, uint16_t count)
{
    s_task_table = table;
    s_task_count = (table != NULL) ? count : 0;
    for (uint8_t i = 0; i < MOTE_TASK_SLOT_MAX; i++) {
        s_slots[i].active = false;
    }
}

mote_status_t mote_task_start(uint16_t id)
{
    if (s_task_table == NULL || id >= s_task_count) {
        return MOTE_ERR_PARAM;
    }
    /* 与定时器同口径的运行时校验（此前仅靠默认关闭的 MOTE_ASSERT）：
     * period_ms==0 会在 catch-up 空转后每 poll 同步调用一次 handler，
     * 与 spin 无异；period_ms≥2^31 会使回绕比较数学失效 */
    if (s_task_table[id].handler == NULL ||
        s_task_table[id].period_ms == 0 ||
        s_task_table[id].period_ms >= 0x80000000u) {
        return MOTE_ERR_PARAM;
    }
    for (uint8_t i = 0; i < MOTE_TASK_SLOT_MAX; i++) {
        if (s_slots[i].active && s_slots[i].id == id) {
            return MOTE_OK; /* 已启动 */
        }
    }
    for (uint8_t i = 0; i < MOTE_TASK_SLOT_MAX; i++) {
        if (!s_slots[i].active) {
            s_slots[i].id = id;
            s_slots[i].due = mote_ticks() + s_task_table[id].period_ms;
            s_slots[i].active = true;
            return MOTE_OK;
        }
    }
    return MOTE_ERR_FULL; /* 槽池耗尽 */
}

mote_status_t mote_task_stop(uint16_t id)
{
    for (uint8_t i = 0; i < MOTE_TASK_SLOT_MAX; i++) {
        if (s_slots[i].active && s_slots[i].id == id) {
            s_slots[i].active = false;
            return MOTE_OK;
        }
    }
    return MOTE_ERR_NOT_FOUND;
}

void mote_process_tasks(void)
{
    uint32_t now = mote_ticks();

    if (s_task_table == NULL) {
        return;
    }
    for (uint8_t i = 0; i < MOTE_TASK_SLOT_MAX; i++) {
        mote_task_slot_t *s = &s_slots[i];
        if (s->active && (int32_t)(now - s->due) >= 0) {
            const mote_task_desc_t *d;

            if (s->id >= s_task_count) {
                s->active = false;
                continue;
            }
            d = &s_task_table[s->id];
            if (d->handler == NULL || d->period_ms == 0 ||
                d->period_ms >= 0x80000000u) {
                s->active = false;
                continue;
            }
            /* 与定时器同语义：相位稳定推进（due += period），
             * 落后超 MOTE_TIMER_CATCHUP_MAX 拍重建相位 */
            uint32_t n = 0;
            do {
                s->due += d->period_ms;
            } while ((int32_t)(now - s->due) >= 0 &&
                     ++n < MOTE_TIMER_CATCHUP_MAX);
            if ((int32_t)(now - s->due) >= 0) {
                s->due = now + d->period_ms;
            }
            d->handler(MOTE_EVT_TASK, NULL, d->ctx);
        }
    }
}

uint32_t mote_task_next_due(void)
{
    uint32_t best = MOTE_TICK_NONE;

    if (s_task_table == NULL) {
        return best;
    }
    for (uint8_t i = 0; i < MOTE_TASK_SLOT_MAX; i++) {
        const mote_task_slot_t *s = &s_slots[i];
        const mote_task_desc_t *d;

        if (!s->active || s->id >= s_task_count) {
            continue;
        }
        d = &s_task_table[s->id];
        if (d->handler == NULL || d->period_ms == 0 ||
            d->period_ms >= 0x80000000u) {
            continue;
        }
        if (best == MOTE_TICK_NONE || (int32_t)(s->due - best) < 0) {
            best = s->due;
        }
    }
    return best;
}

#endif

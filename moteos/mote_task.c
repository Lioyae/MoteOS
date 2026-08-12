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
    s_task_count = count;
    for (uint8_t i = 0; i < MOTE_TASK_SLOT_MAX; i++) {
        s_slots[i].active = false;
    }
}

mote_status_t mote_task_start(uint16_t id)
{
    if (id >= s_task_count) {
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

    for (uint8_t i = 0; i < MOTE_TASK_SLOT_MAX; i++) {
        mote_task_slot_t *s = &s_slots[i];
        if (s->active && (int32_t)(now - s->due) >= 0) {
            s->due = now + s_task_table[s->id].period_ms;
            s_task_table[s->id].handler(MOTE_EVT_TASK, NULL, NULL);
        }
    }
}

#endif

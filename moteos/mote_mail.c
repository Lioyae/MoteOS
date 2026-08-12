/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mote.h"
#include <string.h>

#if MOTE_ENABLE_MAILBOX

mote_status_t mote_mail_send(mote_mail_t *mb, const void *data, uint16_t len)
{
    mote_status_t st;
    mote_crit_state_t cs;

    if (mb == NULL || (data == NULL && len != 0)) {
        return MOTE_ERR_PARAM;
    }

    cs = mote_crit_enter();
    if (mb->count >= mb->slots) {
        st = MOTE_ERR_FULL;
    } else {
        uint16_t n = (len > mb->item_size) ? mb->item_size : len;
        uint8_t idx = (uint8_t)((mb->head + mb->count) % mb->slots);
        MOTE_ASSERT(mb->count < mb->slots);
        memcpy(&mb->buf[idx * mb->item_size], data, n);
        mb->count++;
        st = MOTE_OK;
    }
    mote_crit_exit(cs);

    if (st == MOTE_OK && mote_event_post(mb->evt, (void *)mb) != MOTE_OK) {
        /* 数据已入箱但事件投递失败：回滚入箱操作。
         * 保证 send 全有或全无，杜绝"有货无通知"的滞留 */
        cs = mote_crit_enter();
        mb->count--;
        mote_crit_exit(cs);
        return MOTE_ERR_FULL;
    }
    return st;
}

int mote_mail_recv(mote_mail_t *mb, void *data)
{
    int n;
    mote_crit_state_t cs;

    if (mb == NULL || data == NULL) {
        return -1;
    }

    cs = mote_crit_enter();
    if (mb->count == 0) {
        n = -1;
    } else {
        memcpy(data, &mb->buf[mb->head * mb->item_size], mb->item_size);
        mb->head = (uint8_t)((mb->head + 1) % mb->slots);
        mb->count--;
        n = mb->item_size;
    }
    mote_crit_exit(cs);

    return n;
}

#endif

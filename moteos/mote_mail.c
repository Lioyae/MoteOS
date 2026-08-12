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

    if (mb == NULL || (data == NULL && len != 0)) {
        return MOTE_ERR_PARAM;
    }

    MOTE_ENTER_CRITICAL();
    if (mb->count >= mb->slots) {
        st = MOTE_ERR_FULL;
    } else {
        uint16_t n = (len > mb->item_size) ? mb->item_size : len;
        uint8_t idx = (uint8_t)((mb->head + mb->count) % mb->slots);
        memcpy(&mb->buf[idx * mb->item_size], data, n);
        mb->count++;
        st = MOTE_OK;
    }
    MOTE_EXIT_CRITICAL();

    if (st == MOTE_OK) {
        st = mote_event_post(mb->evt, (void *)mb);
    }
    return st;
}

int mote_mail_recv(mote_mail_t *mb, void *data)
{
    int n;

    if (mb == NULL || data == NULL) {
        return -1;
    }

    MOTE_ENTER_CRITICAL();
    if (mb->count == 0) {
        n = -1;
    } else {
        memcpy(data, &mb->buf[mb->head * mb->item_size], mb->item_size);
        mb->head = (uint8_t)((mb->head + 1) % mb->slots);
        mb->count--;
        n = mb->item_size;
    }
    MOTE_EXIT_CRITICAL();

    return n;
}

#endif

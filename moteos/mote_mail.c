/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mote.h"

#if MOTE_ENABLE_MAILBOX

/* 内核不依赖 libc：对齐感知的拷贝（32 位字拷贝 + 头尾字节）。
 * M0+/RV32EC 不支持非对齐访存，源地址未对齐时按字节组装字 */
static void mote_copy(void *dst, const void *src, uint16_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    while (n != 0 && ((uintptr_t)d & 3u) != 0u) {
        *d++ = *s++;
        n--;
    }
    while (n >= 4) {
        uint32_t w = (uint32_t)s[0]
                   | ((uint32_t)s[1] << 8)
                   | ((uint32_t)s[2] << 16)
                   | ((uint32_t)s[3] << 24);
        *(uint32_t *)d = w;
        d += 4;
        s += 4;
        n -= 4;
    }
    while (n != 0) {
        *d++ = *s++;
        n--;
    }
}

mote_status_t mote_mail_send(mote_mail_t *mb, const void *data, uint16_t len)
{
    mote_status_t st;
    mote_crit_state_t cs;

    if (mb == NULL || (data == NULL && len != 0)) {
        return MOTE_ERR_PARAM;
    }

    /* 入箱与事件入队必须在同一个临界区内完成：
     * 中断不可能插进拷贝与入队之间，回滚也是原子的，
     * 杜绝"数据已入箱但事件没送达"与"回滚挤掉他人数据"两类竞态 */
    cs = mote_crit_enter();
    if (mb->count >= mb->slots) {
        mote_note_dropped(mb->evt); /* 口径统一：被拒绝的入箱也计入 */
        st = MOTE_ERR_FULL;
    } else {
        uint16_t n = (len > mb->item_size) ? mb->item_size : len;
        uint8_t idx = (uint8_t)((mb->head + mb->count) % mb->slots);
        MOTE_ASSERT(mb->count < mb->slots);
        mote_copy(&mb->buf[idx * mb->item_size], data, n);
        mb->count++;
        st = mote_event_enqueue(mb->evt, (void *)mb);
        if (st != MOTE_OK) {
            mb->count--; /* 同一临界区内回滚：全有或全无 */
        }
    }
    mote_crit_exit(cs);

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
        mote_copy(data, &mb->buf[mb->head * mb->item_size], mb->item_size);
        mb->head = (uint8_t)((mb->head + 1) % mb->slots);
        mb->count--;
        n = mb->item_size;
    }
    mote_crit_exit(cs);

    return n;
}

#endif

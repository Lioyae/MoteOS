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
        /* 字节存储而非强转指针写入：规避严格别名 UB；
         * 编译器 -Os 会自动合并为字存储 */
        d[0] = (uint8_t)(w >> 0);
        d[1] = (uint8_t)(w >> 8);
        d[2] = (uint8_t)(w >> 16);
        d[3] = (uint8_t)(w >> 24);
        d += 4;
        s += 4;
        n -= 4;
    }
    while (n != 0) {
        *d++ = *s++;
        n--;
    }
}

/* 邮箱字段合法性：手工构造的 mote_mail_t 可能出现 slots==0（除零）、
 * 缓冲区/长度表为空指针（野指针）、item_size 越界（lens 为 uint8_t，
 * 长度会截断）、head/count 越界（越界读）等，运行时一律拒绝而不是崩溃 */
static bool mote_mail_invalid(const mote_mail_t *mb)
{
    return mb == NULL || mb->buf == NULL || mb->lens == NULL ||
           mb->slots == 0 || mb->item_size == 0 || mb->item_size > 255 ||
           mb->head >= mb->slots || mb->count > mb->slots;
}

mote_status_t mote_mail_send(mote_mail_t *mb, const void *data, uint16_t len)
{
    mote_status_t st;
    mote_crit_state_t cs;

    /* 定长上限 + 变长下限：len 必须 1..item_size。
     * 此前超长静默截断、不足 item_size 时 recv 回吐整格残留垃圾，
     * 接收端无法知道实际长度；现在每槽记录实际存入长度 */
    if (mote_mail_invalid(mb) || data == NULL || len == 0 ||
        len > mb->item_size) {
        return MOTE_ERR_PARAM;
    }

    MOTE_TEST_INJECT(); /* 交错测试窗口：入临界区前伪中断可插入 */

    /* 入箱与事件入队必须在同一个临界区内完成：
     * 中断不可能插进拷贝与入队之间，回滚也是原子的，
     * 杜绝"数据已入箱但事件没送达"与"回滚挤掉他人数据"两类竞态 */
    cs = mote_crit_enter();
    if (mb->count >= mb->slots) {
        mote_note_dropped(mb->evt); /* 口径统一：被拒绝的入箱也计入 */
        st = MOTE_ERR_FULL;
    } else {
        unsigned idx = (unsigned)mb->head + mb->count;

        MOTE_ASSERT(mb->count < mb->slots);
        if (idx >= mb->slots) {
            idx -= mb->slots; /* head+count < 2*slots，一次减法足够 */
        }
        mote_copy(&mb->buf[idx * mb->item_size], data, len);
        mb->lens[idx] = (uint8_t)len;
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

    if (mote_mail_invalid(mb) || data == NULL) {
        return -1;
    }

    cs = mote_crit_enter();
    if (mb->count == 0) {
        n = -1;
    } else if (mb->count > mb->slots || mb->head >= mb->slots) {
        n = -1; /* 结构被写坏：拒绝而不是越界读 */
    } else {
        n = mb->lens[mb->head]; /* 实际存入长度，不是整格 */
        mote_copy(data, &mb->buf[mb->head * mb->item_size], (uint16_t)n);
        if (mb->head + 1 >= mb->slots) {
            mb->head = 0;
        } else {
            mb->head++;
        }
        mb->count--;
    }
    mote_crit_exit(cs);

    return n;
}

#endif

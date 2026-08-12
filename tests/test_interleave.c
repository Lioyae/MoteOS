/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mote.h"
#include "mote_test.h"

/* 单线程交错测试骨架：
 * 用伪随机序列在"主循环操作"与"伪中断操作"之间交错执行，
 * 伪中断遵守硬件规则——临界区内不执行。
 * 验证内核并发语义的一致性：每次投递尝试要么最终被派发、
 * 要么被计数丢弃；邮箱无滞留；临界区不泄漏。 */

#define IV_EVT_MAIL  0
#define IV_EVT_PLAIN 1
#define IV_SLOTS 4
#define IV_SIZE  8

MOTE_MAILBOX_DEF(iv_mb, IV_EVT_MAIL, IV_SLOTS, IV_SIZE);

static uint32_t s_plain_calls;
static uint32_t s_recv_bytes;

static void plain_handler(uint16_t evt, void *param, void *ctx)
{
    (void)evt;
    (void)param;
    (void)ctx;
    s_plain_calls++;
}

static void mail_handler(uint16_t evt, void *param, void *ctx)
{
    (void)evt;
    (void)ctx;
    mote_mail_t *mb = (mote_mail_t *)param;
    uint8_t buf[IV_SIZE];
    int n;

    while ((n = mote_mail_recv(mb, buf)) > 0) {
        s_recv_bytes += (uint32_t)n;
    }
}

static const mote_evt_entry_t iv_table[] = {
    [IV_EVT_MAIL]  = MOTE_ENTRY(mail_handler, NULL),
    [IV_EVT_PLAIN] = MOTE_ENTRY(plain_handler, NULL),
};

static uint32_t s_rng = 0x12345678u;

static uint32_t iv_rand(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

/* 伪中断：模拟硬件行为，临界区内不可抢占 */
static void iv_do_isr(uint32_t *attempts)
{
    if (mote_crit_active() != 0) {
        return; /* 硬件语义：关中断期间 ISR 不执行 */
    }
    (*attempts)++;
    if (iv_rand() & 1u) {
        mote_event_post(IV_EVT_PLAIN, MOTE_P(iv_rand()));
    } else {
        uint8_t d = (uint8_t)iv_rand();
        mote_mail_send(&iv_mb, &d, 1);
    }
}

static void iv_do_main(uint32_t *attempts, uint32_t *poll_true)
{
    switch (iv_rand() % 3) {
    case 0:
        (*attempts)++;
        mote_event_post(IV_EVT_PLAIN, MOTE_P(iv_rand()));
        break;
    case 1:
        (*attempts)++;
        {
            uint8_t d = (uint8_t)iv_rand();
            mote_mail_send(&iv_mb, &d, 1);
        }
        break;
    default:
        if (mote_poll()) {
            (*poll_true)++;
        }
        break;
    }
}

static void test_interleave_consistency(void)
{
    uint32_t attempts = 0;
    uint32_t poll_true = 0;
    uint8_t buf[IV_SIZE];

    mote_init(iv_table, 2);
    s_plain_calls = 0;
    s_recv_bytes = 0;

    for (int i = 0; i < 4000; i++) {
        if (iv_rand() & 1u) {
            iv_do_isr(&attempts);
        } else {
            iv_do_main(&attempts, &poll_true);
        }
        /* 内核不得泄漏临界区 */
        TEST_ASSERT(mote_crit_active() == 0);
    }

    while (mote_poll()) {
        poll_true++;
    }

    /* 总账一：每次尝试要么最终派发、要么被计数丢弃 */
    TEST_ASSERT(attempts == poll_true + mote_dropped_count());
    /* 总账二：邮箱全清，无滞留数据 */
    TEST_ASSERT(mote_mail_recv(&iv_mb, buf) == -1);
    /* 总账三：派发的普通事件数与邮箱出货量对得上 */
    TEST_ASSERT(s_plain_calls + (s_recv_bytes / IV_SIZE) == poll_true);
}

void suite_interleave(void)
{
    test_interleave_consistency();
}

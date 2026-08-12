/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mote.h"
#include "mote_test.h"
#include <string.h>

#define MB_SLOTS 3
#define MB_SIZE  8

static uint8_t s_recv_buf[MB_SIZE];
static int s_recv_len = -1;

MOTE_MAILBOX_DEF(mb, 0, MB_SLOTS, MB_SIZE);

static void mail_handler(uint16_t evt, void *param, void *ctx)
{
    (void)evt;
    (void)ctx;
    s_recv_len = mote_mail_recv((mote_mail_t *)param, s_recv_buf);
}

static const mote_evt_entry_t table[] = {
    [0] = MOTE_ENTRY(mail_handler, NULL),
    [1] = MOTE_ENTRY(NULL, NULL), /* 空 handler，用于占满队列 */
};

static void test_send_recv_roundtrip(void)
{
    uint8_t data[MB_SIZE] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    mote_init(table, 1);
    s_recv_len = -1;

    TEST_ASSERT(mote_mail_send(&mb, data, sizeof(data)) == MOTE_OK);
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_recv_len == MB_SIZE);
    TEST_ASSERT(memcmp(s_recv_buf, data, MB_SIZE) == 0);
    TEST_ASSERT(mote_poll() == false);
}

static void test_send_full(void)
{
    uint8_t data = 0xAA;

    mote_init(table, 1);
    for (int i = 0; i < MB_SLOTS; i++) {
        TEST_ASSERT(mote_mail_send(&mb, &data, 1) == MOTE_OK);
    }
    TEST_ASSERT(mote_mail_send(&mb, &data, 1) == MOTE_ERR_FULL);
    /* 全部取出，满槽状态恢复 */
    for (int i = 0; i < MB_SLOTS; i++) {
        TEST_ASSERT(mote_poll() == true);
        TEST_ASSERT(s_recv_len == MB_SIZE);
        TEST_ASSERT(s_recv_buf[0] == 0xAA);
    }
    TEST_ASSERT(mote_mail_send(&mb, &data, 1) == MOTE_OK);
    TEST_ASSERT(mote_poll() == true); /* 收尾 drain，恢复空箱 */
}

static void test_recv_empty(void)
{
    static uint8_t buf[MB_SIZE];
    MOTE_MAILBOX_DEF(mb2, 1, MB_SLOTS, MB_SIZE);

    mote_init(table, 1);
    TEST_ASSERT(mote_mail_recv(&mb2, buf) == -1);
}

static void test_send_truncates(void)
{
    uint8_t data[16];

    memset(data, 0x5A, sizeof(data));
    mote_init(table, 1);
    s_recv_len = -1;
    TEST_ASSERT(mote_mail_send(&mb, data, sizeof(data)) == MOTE_OK);
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_recv_len == MB_SIZE);           /* 截断到槽大小 */
    TEST_ASSERT(s_recv_buf[MB_SIZE - 1] == 0x5A);
}

static void test_send_rollback_on_queue_full(void)
{
    uint8_t data = 0x55;
    static uint8_t buf[MB_SIZE];

    mote_init(table, 2);
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_event_post(1, MOTE_P(i)) == MOTE_OK);
    }
    /* 队列满：send 必须整体失败，邮箱不得滞留数据 */
    TEST_ASSERT(mote_mail_send(&mb, &data, 1) == MOTE_ERR_FULL);
    TEST_ASSERT(mote_mail_recv(&mb, buf) == -1);

    /* 队列清空后 send 恢复正常 */
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_poll() == true); /* 丢弃占位事件 */
    }
    s_recv_len = -1;
    TEST_ASSERT(mote_mail_send(&mb, &data, 1) == MOTE_OK);
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_recv_len == MB_SIZE);
    TEST_ASSERT(s_recv_buf[0] == 0x55);
}

void suite_mail(void)
{
    test_send_recv_roundtrip();
    test_send_full();
    test_recv_empty();
    test_send_truncates();
    test_send_rollback_on_queue_full();
}

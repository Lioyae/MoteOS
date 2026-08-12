/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mote.h"
#include "mote_test.h"

static uint32_t s_last_evt = 0xFFFF;
static uint32_t s_last_param;
static uint32_t s_calls;

static void handler(uint16_t evt, void *param, void *ctx)
{
    (void)ctx;
    s_last_evt = evt;
    s_last_param = MOTE_U32(param);
    s_calls++;
}

static const mote_evt_entry_t table[] = {
    [0] = MOTE_ENTRY(handler, NULL),
    [1] = MOTE_ENTRY(handler, NULL),
};

static void test_post_and_dispatch(void)
{
    mote_init(table, 2);
    s_calls = 0;
    TEST_ASSERT(mote_event_post(0, MOTE_P(42)) == MOTE_OK);
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_calls == 1);
    TEST_ASSERT(s_last_evt == 0);
    TEST_ASSERT(s_last_param == 42);
    TEST_ASSERT(mote_poll() == false);
}

static void test_queue_full_returns_error(void)
{
    mote_init(table, 2);
    mote_status_t st = MOTE_OK;
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        st = mote_event_post(0, MOTE_P(i));
    }
    TEST_ASSERT(st == MOTE_OK);
    TEST_ASSERT(mote_event_post(0, MOTE_P(99)) == MOTE_ERR_FULL);
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_poll() == true);
    }
    TEST_ASSERT(s_last_param == MOTE_EVT_QUEUE_SIZE - 1);
    TEST_ASSERT(mote_poll() == false);
}

static void test_post_replace_overwrites(void)
{
    mote_init(table, 2);
    s_calls = 0;
    TEST_ASSERT(mote_event_post(0, MOTE_P(1)) == MOTE_OK);
    TEST_ASSERT(mote_event_post_replace(0, MOTE_P(2)) == MOTE_OK);
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_last_param == 2);
    TEST_ASSERT(mote_poll() == false); /* 只有一条事件 */
}

static void test_replace_on_full_queue(void)
{
    mote_init(table, 2);
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_event_post(0, MOTE_P(i)) == MOTE_OK);
    }
    /* 满队列替换同 ID：成功且不增加条目 */
    TEST_ASSERT(mote_event_post_replace(0, MOTE_P(777)) == MOTE_OK);
    /* 满队列替换不同 ID：失败 */
    TEST_ASSERT(mote_event_post_replace(1, MOTE_P(888)) == MOTE_ERR_FULL);
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_poll() == true);
    }
    TEST_ASSERT(s_last_param == 777);
    TEST_ASSERT(mote_poll() == false);
}

static void test_unregistered_id_dropped(void)
{
    mote_init(table, 2);
    s_last_evt = 0xFFFF;
    TEST_ASSERT(mote_event_post(5, MOTE_P(1)) == MOTE_OK); /* ID 越界也入队 */
    TEST_ASSERT(mote_poll() == true);  /* 被取出但安全丢弃 */
    TEST_ASSERT(s_last_evt == 0xFFFF); /* handler 未被调用 */
}

static void test_null_handler_dropped(void)
{
    static const mote_evt_entry_t t2[] = {
        [3] = MOTE_ENTRY(NULL, NULL), /* 显式空 handler */
    };
    mote_init(t2, 4);
    s_last_evt = 0xFFFF;
    TEST_ASSERT(mote_event_post(3, MOTE_P(1)) == MOTE_OK);
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_last_evt == 0xFFFF);
}

static void test_dropped_count(void)
{
    mote_init(table, 2);
    TEST_ASSERT(mote_dropped_count() == 0);
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_event_post(0, MOTE_P(i)) == MOTE_OK);
    }
    TEST_ASSERT(mote_event_post(0, MOTE_P(9)) == MOTE_ERR_FULL);
    TEST_ASSERT(mote_event_post_replace(1, MOTE_P(8)) == MOTE_ERR_FULL);
    TEST_ASSERT(mote_dropped_count() == 2);
}

static void test_crit_nesting(void)
{
    mote_init(table, 2);

    /* 外部进入临界区后调用内核 API：内核内部的进入/退出
     * 必须恢复调用方状态，而不是强行打开中断 */
    mote_crit_state_t cs = mote_crit_enter();
    TEST_ASSERT(mote_crit_active() == 1);
    TEST_ASSERT(mote_event_post(0, MOTE_P(1)) == MOTE_OK);
    TEST_ASSERT(mote_event_post_replace(0, MOTE_P(2)) == MOTE_OK);
    TEST_ASSERT(mote_crit_active() == 1); /* 仍处于调用方关闭状态 */
    mote_crit_exit(cs);
    TEST_ASSERT(mote_crit_active() == 0);

    /* 嵌套两层 */
    mote_crit_state_t c1 = mote_crit_enter();
    mote_crit_state_t c2 = mote_crit_enter();
    TEST_ASSERT(mote_crit_active() == 1);
    mote_crit_exit(c2);
    TEST_ASSERT(mote_crit_active() == 1);
    mote_crit_exit(c1);
    TEST_ASSERT(mote_crit_active() == 0);
}

static uint16_t s_hook_evt;
static uint32_t s_hook_calls;

static void drop_hook(uint16_t evt)
{
    s_hook_evt = evt;
    s_hook_calls++;
}

static void test_drop_hook(void)
{
    mote_init(table, 2);
    s_hook_calls = 0;
    mote_set_drop_hook(drop_hook);

    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_event_post(0, MOTE_P(i)) == MOTE_OK);
    }
    TEST_ASSERT(mote_event_post(0, MOTE_P(9)) == MOTE_ERR_FULL);
    TEST_ASSERT(s_hook_calls == 1);
    TEST_ASSERT(s_hook_evt == 0);
    TEST_ASSERT(mote_dropped_count() == 1);

    /* 未注册事件丢弃也触发钩子 */
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_poll() == true);
    }
    TEST_ASSERT(mote_event_post(7, MOTE_P(1)) == MOTE_OK);
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_hook_calls == 2);
    TEST_ASSERT(s_hook_evt == 7);

    /* 取消钩子后不再回调 */
    mote_set_drop_hook(NULL);
    TEST_ASSERT(mote_event_post(9, MOTE_P(1)) == MOTE_OK);
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_hook_calls == 2);
}

void suite_queue(void)
{
    test_post_and_dispatch();
    test_queue_full_returns_error();
    test_post_replace_overwrites();
    test_replace_on_full_queue();
    test_unregistered_id_dropped();
    test_null_handler_dropped();
    test_dropped_count();
    test_drop_hook();
    test_crit_nesting();
}

/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mote.h"
#include "mote_test.h"

static uint32_t s_last_evt;
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

static void test_timer_one_shot(void)
{
    static mote_timer_t t;

    mote_init(table, 2);
    s_calls = 0;
    TEST_ASSERT(mote_timer_start(&t, 0, MOTE_P(7), 10, false) == MOTE_OK);
    for (int i = 0; i < 9; i++) {
        mote_tick();
        TEST_ASSERT(mote_poll() == false);
    }
    mote_tick();
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_calls == 1);
    TEST_ASSERT(s_last_param == 7);
    TEST_ASSERT(mote_poll() == false); /* 单次定时器不再触发 */
}

static void test_timer_periodic(void)
{
    static mote_timer_t t;

    mote_init(table, 2);
    s_calls = 0;
    TEST_ASSERT(mote_timer_start(&t, 1, MOTE_P(0), 5, true) == MOTE_OK);
    for (int i = 0; i < 15; i++) {
        mote_tick();
        mote_poll();
    }
    TEST_ASSERT(s_calls == 3); /* 第 5/10/15 拍各一次 */
}

static mote_timer_t s_self;
static uint32_t s_self_calls;

static void self_handler(uint16_t evt, void *param, void *ctx)
{
    (void)evt;
    (void)param;
    (void)ctx;
    s_self_calls++;
    mote_timer_stop(&s_self);
}

static const mote_evt_entry_t self_table[] = {
    [0] = MOTE_ENTRY(self_handler, NULL),
};

static void test_stop_self_in_handler(void)
{
    mote_init(self_table, 1);
    s_self_calls = 0;
    TEST_ASSERT(mote_timer_start(&s_self, 0, NULL, 1, true) == MOTE_OK);
    for (int i = 0; i < 5; i++) {
        mote_tick();
        mote_poll();
    }
    TEST_ASSERT(s_self_calls == 1); /* 第一次触发后自我停止 */
}

static void test_restart(void)
{
    static mote_timer_t t;

    mote_init(table, 2);
    s_calls = 0;
    TEST_ASSERT(mote_timer_start(&t, 0, MOTE_P(1), 10, false) == MOTE_OK);
    for (int i = 0; i < 5; i++) {
        mote_tick();
        mote_poll();
    }
    TEST_ASSERT(mote_timer_restart(&t, 10) == MOTE_OK); /* 重新计时 */
    for (int i = 0; i < 9; i++) {
        mote_tick();
        TEST_ASSERT(mote_poll() == false);
    }
    mote_tick();
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_last_param == 1);

    /* 未启动的定时器 restart 报错 */
    static mote_timer_t t2;
    TEST_ASSERT(mote_timer_restart(&t2, 10) == MOTE_ERR_NOT_FOUND);
}

static void test_tick_overflow(void)
{
    static mote_timer_t t;

    mote_init(table, 2);
    s_calls = 0;
    mote_tick_set(0xFFFFFFF0u);
    TEST_ASSERT(mote_timer_start(&t, 0, MOTE_P(5), 100, false) == MOTE_OK);
    for (int i = 0; i < 99; i++) {
        mote_tick();
        TEST_ASSERT(mote_poll() == false);
    }
    mote_tick();
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_last_param == 5);
    TEST_ASSERT(mote_poll() == false);
}

static void test_post_delayed(void)
{
    mote_init(table, 2);
    s_calls = 0;
    TEST_ASSERT(mote_event_post_delayed(0, MOTE_P(3), 10) == MOTE_OK);
    for (int i = 0; i < 9; i++) {
        mote_tick();
        TEST_ASSERT(mote_poll() == false);
    }
    mote_tick();
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_last_param == 3);

    /* 槽耗尽 */
    for (int i = 0; i < MOTE_DELAYED_MAX; i++) {
        TEST_ASSERT(mote_event_post_delayed(0, MOTE_P(i), 100) == MOTE_OK);
    }
    TEST_ASSERT(mote_event_post_delayed(0, MOTE_P(9), 100) == MOTE_ERR_FULL);
    /* 到期后槽释放可复用 */
    for (int i = 0; i < 100; i++) {
        mote_tick();
        mote_poll();
    }
    TEST_ASSERT(mote_event_post_delayed(0, MOTE_P(10), 1) == MOTE_OK);
}

void suite_timer(void)
{
    test_timer_one_shot();
    test_timer_periodic();
    test_stop_self_in_handler();
    test_restart();
    test_tick_overflow();
    test_post_delayed();
}

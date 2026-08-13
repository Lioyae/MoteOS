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

static void test_one_shot_survives_full_queue(void)
{
    static mote_timer_t t;

    mote_init(table, 2);
    s_calls = 0;
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_event_post(1, MOTE_P(i)) == MOTE_OK);
    }
    TEST_ASSERT(mote_timer_start(&t, 0, MOTE_P(7), 5, false) == MOTE_OK);
    for (int i = 0; i < 5; i++) {
        mote_tick();
    }
    /* 到期但队列满：事件未入队，定时器保留重试 */
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_poll() == true);
    }
    /* 队列空了，下一次 poll 定时器重试成功 */
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_last_param == 7);
    TEST_ASSERT(mote_poll() == false); /* 单次定时器已释放 */
}

static void test_drop_policy_strict_deadline(void)
{
    static mote_timer_t t;

    mote_init(table, 2);
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE; i++) {
        TEST_ASSERT(mote_event_post(1, MOTE_P(i)) == MOTE_OK);
    }
    TEST_ASSERT(mote_timer_start_ex(&t, 0, MOTE_P(7), 5, false,
                                    MOTE_TIMER_POLICY_DROP) == MOTE_OK);
    for (int i = 0; i < 5; i++) {
        mote_tick();
    }
    TEST_ASSERT(mote_poll() == true); /* 到期：投递失败被计数，定时器释放 */
    for (int i = 0; i < MOTE_EVT_QUEUE_SIZE - 1; i++) {
        TEST_ASSERT(mote_poll() == true);
    }
    /* DROP 策略：事件不迟到 */
    TEST_ASSERT(mote_poll() == false);
    TEST_ASSERT(s_last_param != 7);
    /* 定时器已释放 */
    TEST_ASSERT(mote_timer_restart(&t, 10) == MOTE_ERR_NOT_FOUND);
}

static void test_latest_coalesces(void)
{
    static mote_timer_t t;

    mote_init(table, 2);
    s_calls = 0;
    TEST_ASSERT(mote_timer_start_ex(&t, 0, MOTE_P(42), 5, true,
                                    MOTE_TIMER_POLICY_LATEST) == MOTE_OK);
    /* 预塞同 ID 事件，模拟"上一拍还没被 handler 处理" */
    TEST_ASSERT(mote_event_post(0, MOTE_P(1)) == MOTE_OK);
    for (int i = 0; i < 5; i++) {
        mote_tick();
    }
    TEST_ASSERT(mote_poll() == true);  /* 到期 replace：队列仍 1 条 → 派发 */
    TEST_ASSERT(s_last_param == 42);   /* 派发的是最新参数 */
    TEST_ASSERT(mote_poll() == false); /* 无第二条积压 */
    mote_timer_stop(&t);
}

static void test_periodic_no_drift(void)
{
    static mote_timer_t t;

    /* 相位稳定回归：主循环/处理延迟不得造成周期相位逐周期累积漂移
     * （旧实现 due = now + period，迟到触发后相位永久后移） */
    mote_init(table, 2);
    s_calls = 0;
    TEST_ASSERT(mote_timer_start(&t, 0, MOTE_P(0), 10, true) == MOTE_OK);
    for (int i = 1; i <= 9; i++) {
        mote_tick();
        TEST_ASSERT(mote_poll() == false);
    }
    mote_tick();                      /* 第 10 拍 */
    TEST_ASSERT(mote_poll() == true); /* 第一次到期 */
    TEST_ASSERT(s_calls == 1);
    /* 主循环繁忙：tick 走到第 23 拍才有机会 poll */
    for (int i = 11; i <= 23; i++) {
        mote_tick();
    }
    TEST_ASSERT(mote_poll() == true); /* 迟到触发（第 23 拍，不可避免） */
    TEST_ASSERT(s_calls == 2);
    /* 相位被保留：下一次到期仍是第 30 拍（旧实现会漂到第 33 拍） */
    for (int i = 24; i <= 29; i++) {
        mote_tick();
        TEST_ASSERT(mote_poll() == false);
    }
    mote_tick();                      /* 第 30 拍 */
    TEST_ASSERT(mote_poll() == true);
    TEST_ASSERT(s_calls == 3);
    mote_timer_stop(&t);
}

static void test_ms_bound(void)
{
    static mote_timer_t t;

    /* 时长运行时校验：ms==0 与 ms>=2^31 必须返回 MOTE_ERR_PARAM，
     * 不能依赖默认关闭的 MOTE_ASSERT（生产构建下会静默失效） */
    mote_init(table, 2);
    TEST_ASSERT(mote_timer_start(&t, 0, MOTE_P(0), 0, false)
                == MOTE_ERR_PARAM);
    TEST_ASSERT(mote_timer_start(&t, 0, MOTE_P(0), 0x80000000u, false)
                == MOTE_ERR_PARAM);
    TEST_ASSERT(mote_event_post_delayed(0, MOTE_P(0), 0)
                == MOTE_ERR_PARAM);
    TEST_ASSERT(mote_event_post_delayed(0, MOTE_P(0), 0x80000000u)
                == MOTE_ERR_PARAM);
    TEST_ASSERT(mote_event_post_delayed(0, MOTE_P(0), 0x7FFFFFFFu) == MOTE_OK);
    TEST_ASSERT(mote_timer_start(&t, 0, MOTE_P(0), 0x7FFFFFFFu, false)
                == MOTE_OK);
    TEST_ASSERT(mote_timer_restart(&t, 0x80000000u) == MOTE_ERR_PARAM);
    TEST_ASSERT(mote_timer_restart(&t, 0) == MOTE_ERR_PARAM);
    TEST_ASSERT(mote_timer_restart(&t, 0x7FFFFFFFu) == MOTE_OK);
    mote_timer_stop(&t);
}

void suite_timer(void)
{
    test_timer_one_shot();
    test_timer_periodic();
    test_stop_self_in_handler();
    test_restart();
    test_tick_overflow();
    test_post_delayed();
    test_one_shot_survives_full_queue();
    test_drop_policy_strict_deadline();
    test_latest_coalesces();
    test_periodic_no_drift();
    test_ms_bound();
}

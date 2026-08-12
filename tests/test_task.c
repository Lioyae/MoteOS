/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mote.h"
#include "mote_test.h"

static uint32_t s_calls0;
static uint32_t s_calls1;
static uint16_t s_last_evt;
static void *s_ctx_seen;

static void tsk0(uint16_t evt, void *param, void *ctx)
{
    (void)param;
    (void)ctx;
    s_calls0++;
    s_last_evt = evt;
}

static void tsk1(uint16_t evt, void *param, void *ctx)
{
    (void)param;
    (void)ctx;
    s_calls1++;
    s_last_evt = evt;
}

static void tsk_ctx(uint16_t evt, void *param, void *ctx)
{
    (void)evt;
    (void)param;
    s_ctx_seen = ctx;
}

static const mote_task_desc_t tasks[] = {
    MOTE_TASK_DEF(10, tsk0, NULL),
    MOTE_TASK_DEF(20, tsk1, NULL),
};

static void test_task_periodic(void)
{
    mote_init(NULL, 0);
    mote_task_init(tasks, 2);
    s_calls0 = s_calls1 = 0;

    TEST_ASSERT(mote_task_start(0) == MOTE_OK);
    for (int i = 0; i < 30; i++) {
        mote_tick();
        mote_poll();
    }
    TEST_ASSERT(s_calls0 == 3); /* 10/20/30ms */
    TEST_ASSERT(s_calls1 == 0); /* 未启动不执行 */

    TEST_ASSERT(mote_task_start(1) == MOTE_OK);
    for (int i = 0; i < 20; i++) {
        mote_tick();
        mote_poll();
    }
    TEST_ASSERT(s_calls0 == 5);
    TEST_ASSERT(s_calls1 == 1);
    TEST_ASSERT(s_last_evt == MOTE_EVT_TASK);
}

static void test_task_stop(void)
{
    mote_init(NULL, 0);
    mote_task_init(tasks, 2);
    s_calls0 = 0;

    TEST_ASSERT(mote_task_start(0) == MOTE_OK);
    for (int i = 0; i < 10; i++) {
        mote_tick();
        mote_poll();
    }
    TEST_ASSERT(s_calls0 == 1);
    TEST_ASSERT(mote_task_stop(0) == MOTE_OK);
    TEST_ASSERT(mote_task_stop(0) == MOTE_ERR_NOT_FOUND);
    for (int i = 0; i < 20; i++) {
        mote_tick();
        mote_poll();
    }
    TEST_ASSERT(s_calls0 == 1); /* 停止后不再执行 */

    TEST_ASSERT(mote_task_start(9) == MOTE_ERR_PARAM); /* 越界 id */
}

static void test_task_slot_pool(void)
{
    /* 槽数 = MOTE_TASK_SLOT_MAX；定义 6 个任务，同时活跃数不能超过槽数 */
    static const mote_task_desc_t many[] = {
        MOTE_TASK_DEF(10, tsk0, NULL),
        MOTE_TASK_DEF(10, tsk0, NULL),
        MOTE_TASK_DEF(10, tsk0, NULL),
        MOTE_TASK_DEF(10, tsk0, NULL),
        MOTE_TASK_DEF(10, tsk0, NULL),
        MOTE_TASK_DEF(10, tsk0, NULL),
    };

    mote_init(NULL, 0);
    mote_task_init(many, 6);

    int ok = 0;
    for (int i = 0; i < 6; i++) {
        if (mote_task_start((uint16_t)i) == MOTE_OK) {
            ok++;
        }
    }
    TEST_ASSERT(ok == MOTE_TASK_SLOT_MAX);      /* 只能启动槽数个 */
    TEST_ASSERT(mote_task_stop(0) == MOTE_OK);  /* 释放一个槽 */
    TEST_ASSERT(mote_task_start(0) == MOTE_OK); /* 重新启动被停止的 */
    TEST_ASSERT(mote_task_stop(0) == MOTE_OK);
    TEST_ASSERT(mote_task_start(4) == MOTE_OK); /* 空槽可复用 */
}

static void test_task_ctx(void)
{
    static uint32_t ctx_val = 1234;
    static const mote_task_desc_t ctasks[] = {
        MOTE_TASK_DEF(10, tsk_ctx, &ctx_val),
    };

    mote_init(NULL, 0);
    mote_task_init(ctasks, 1);
    s_ctx_seen = NULL;

    TEST_ASSERT(mote_task_start(0) == MOTE_OK);
    for (int i = 0; i < 10; i++) {
        mote_tick();
        mote_poll();
    }
    TEST_ASSERT(s_ctx_seen == &ctx_val);
}

void suite_task(void)
{
    test_task_periodic();
    test_task_stop();
    test_task_slot_pool();
    test_task_ctx();
}

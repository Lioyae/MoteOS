/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mote.h"

typedef struct {
    uint16_t evt;
    void *param;
} mote_qitem_t;

static struct {
    mote_qitem_t items[MOTE_EVT_QUEUE_SIZE];
    uint8_t head;
    uint8_t count;
} s_q;

static const mote_evt_entry_t *s_evt_table;
static uint16_t s_evt_count;

static volatile uint32_t s_tick;
static mote_timer_t *s_timers;

static uint32_t s_dropped;
static mote_drop_hook_t s_drop_hook;
static uint8_t s_in_drop_hook;

#ifdef MOTE_TEST_INJECT_ENABLE
static void (*s_test_inject)(void);
/* do-while 形式：避免三元 void 分支（GCC 扩展），-pedantic 兼容 */
#define MOTE_TEST_INJECT()                                                     \
    do {                                                                       \
        if (s_test_inject != NULL) {                                           \
            s_test_inject();                                                   \
        }                                                                      \
    } while (0)
void mote_test_inject_set(void (*fn)(void))
{
    s_test_inject = fn;
}
#else
#define MOTE_TEST_INJECT() ((void)0)
#endif

#if MOTE_DELAYED_MAX > 0
static struct {
    uint32_t due;
    uint16_t evt;
    void *param;
    uint8_t used;
} s_delayed[MOTE_DELAYED_MAX];
#endif

/* ---------------- 事件队列 ---------------- */

static void mote_q_push(uint16_t evt, void *param)
{
    uint8_t idx = (uint8_t)((s_q.head + s_q.count) % MOTE_EVT_QUEUE_SIZE);
    MOTE_ASSERT(s_q.count < MOTE_EVT_QUEUE_SIZE);
    s_q.items[idx].evt = evt;
    s_q.items[idx].param = param;
    s_q.count++;
}

static bool mote_q_pop(uint16_t *evt, void **param)
{
    if (s_q.count == 0) {
        return false;
    }
    *evt = s_q.items[s_q.head].evt;
    *param = s_q.items[s_q.head].param;
    s_q.head = (uint8_t)((s_q.head + 1) % MOTE_EVT_QUEUE_SIZE);
    s_q.count--;
    return true;
}

void mote_init(const mote_evt_entry_t *evt_table, uint16_t evt_count)
{
    s_q.head = 0;
    s_q.count = 0;
    s_evt_table = evt_table;
    s_evt_count = evt_count;
    s_tick = 0;
    s_timers = NULL;
    s_dropped = 0;
#if MOTE_DELAYED_MAX > 0
    for (int i = 0; i < MOTE_DELAYED_MAX; i++) {
        s_delayed[i].used = 0;
    }
#endif
}

uint32_t mote_dropped_count(void)
{
    uint32_t n;
    mote_crit_state_t cs = mote_crit_enter();
    n = s_dropped;
    mote_crit_exit(cs);
    return n;
}

void mote_set_drop_hook(mote_drop_hook_t hook)
{
    s_drop_hook = hook;
}

void mote_note_dropped(uint16_t evt)
{
    s_dropped++;
    /* 防重入：钩子内再触发丢弃只计数、不再递归回调 */
    if (s_drop_hook != NULL && !s_in_drop_hook) {
        s_in_drop_hook = 1;
        s_drop_hook(evt);
        s_in_drop_hook = 0;
    }
}

mote_status_t mote_event_enqueue(uint16_t evt, void *param)
{
    if (s_q.count >= MOTE_EVT_QUEUE_SIZE) {
        mote_note_dropped(evt);
        return MOTE_ERR_FULL;
    }
    mote_q_push(evt, param);
    return MOTE_OK;
}

mote_status_t mote_event_post(uint16_t evt, void *param)
{
    mote_status_t st;
    mote_crit_state_t cs;

    MOTE_TEST_INJECT();
    cs = mote_crit_enter();
    st = mote_event_enqueue(evt, param);
    mote_crit_exit(cs);
    MOTE_TEST_INJECT();
    return st;
}

mote_status_t mote_event_post_replace(uint16_t evt, void *param)
{
    mote_status_t st;
    mote_crit_state_t cs;

    MOTE_TEST_INJECT();
    cs = mote_crit_enter();

    /* 从新到旧查找：覆盖最新一条同 ID 事件（latest wins） */
    for (uint8_t i = s_q.count; i > 0; i--) {
        uint8_t idx = (uint8_t)((s_q.head + i - 1) % MOTE_EVT_QUEUE_SIZE);
        if (s_q.items[idx].evt == evt) {
            s_q.items[idx].param = param;
            mote_crit_exit(cs);
            MOTE_TEST_INJECT();
            return MOTE_OK;
        }
    }
    if (s_q.count >= MOTE_EVT_QUEUE_SIZE) {
        st = MOTE_ERR_FULL;
    } else {
        mote_q_push(evt, param);
        st = MOTE_OK;
    }
    if (st == MOTE_ERR_FULL) {
        mote_note_dropped(evt);
    }
    mote_crit_exit(cs);
    MOTE_TEST_INJECT();
    return st;
}

/* ---------------- 时基与定时器 ---------------- */

void mote_tick(void)
{
    /* 统一契约：s_tick 的一切访问都在临界区内完成（含本函数）。
     * M0+ 上 32 位读改写非原子，依赖"关中断"保证与主循环侧读写互斥 */
    mote_crit_state_t cs = mote_crit_enter();
    s_tick++;
    mote_crit_exit(cs);
}

void mote_tick_set(uint32_t ticks)
{
    mote_crit_state_t cs = mote_crit_enter();
    s_tick = ticks;
    mote_crit_exit(cs);
}

uint32_t mote_ticks(void)
{
    uint32_t t;
    mote_crit_state_t cs = mote_crit_enter();
    t = s_tick; /* M0+ 上 32 位读非原子，关中断保证完整性 */
    mote_crit_exit(cs);
    return t;
}

static void mote_timer_unlink(mote_timer_t *t)
{
    mote_timer_t **pp = &s_timers;

    while (*pp != NULL) {
        if (*pp == t) {
            *pp = t->next;
            t->next = NULL;
            return;
        }
        pp = &(*pp)->next;
    }
}

static bool mote_timer_linked(mote_timer_t *t)
{
    for (mote_timer_t *p = s_timers; p != NULL; p = p->next) {
        if (p == t) {
            return true;
        }
    }
    return false;
}

mote_status_t mote_timer_start(mote_timer_t *t, uint16_t evt, void *param,
                               uint32_t ms, bool periodic)
{
    return mote_timer_start_ex(t, evt, param, ms, periodic,
                               MOTE_TIMER_POLICY_RETRY);
}

mote_status_t mote_timer_start_ex(mote_timer_t *t, uint16_t evt, void *param,
                                  uint32_t ms, bool periodic,
                                  mote_timer_policy_t policy)
{
    mote_crit_state_t cs;

    /* ms 上限运行时校验（此前仅靠默认关闭的 MOTE_ASSERT，生产构建会静默失效）：
     * 回绕比较的数学边界，上限约 24.8 天 */
    if (t == NULL || ms == 0 || ms >= 0x80000000u) {
        return MOTE_ERR_PARAM;
    }
    MOTE_ASSERT((uint8_t)policy <= MOTE_TIMER_POLICY_LATEST);
    mote_timer_stop(t); /* 重复 start 视为重启 */
    cs = mote_crit_enter(); /* s_tick 由 tick 中断更新，读取必须关中断 */
    t->due = s_tick + ms;
    mote_crit_exit(cs);
    t->period = periodic ? ms : 0;
    t->evt = evt;
    t->param = param;
    t->policy = (uint8_t)policy;
    t->next = s_timers;
    s_timers = t;
    return MOTE_OK;
}

mote_status_t mote_timer_stop(mote_timer_t *t)
{
    if (t == NULL) {
        return MOTE_ERR_PARAM;
    }
    mote_timer_unlink(t);
    return MOTE_OK;
}

mote_status_t mote_timer_restart(mote_timer_t *t, uint32_t ms)
{
    mote_crit_state_t cs;

    if (t == NULL || ms == 0 || ms >= 0x80000000u) {
        return MOTE_ERR_PARAM;
    }
    if (!mote_timer_linked(t)) {
        return MOTE_ERR_NOT_FOUND;
    }
    cs = mote_crit_enter(); /* s_tick 由 tick 中断更新，读取必须关中断 */
    t->due = s_tick + ms;
    mote_crit_exit(cs);
    if (t->period != 0) {
        t->period = ms;
    }
    return MOTE_OK;
}

mote_status_t mote_event_post_delayed(uint16_t evt, void *param, uint32_t ms)
{
    /* ms 上限运行时校验（回绕比较的数学边界，约 24.8 天）：
     * 与定时器 API 同口径，不依赖可被关闭的 MOTE_ASSERT */
    if (ms >= 0x80000000u) {
        return MOTE_ERR_PARAM;
    }
#if MOTE_DELAYED_MAX > 0
    int free_slot = -1;
    mote_status_t st;
    mote_crit_state_t cs;

    cs = mote_crit_enter();

    for (int i = 0; i < MOTE_DELAYED_MAX; i++) {
        if (!s_delayed[i].used && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot < 0) {
        mote_note_dropped(evt); /* 口径统一：被拒绝的延时投递也计入 */
        st = MOTE_ERR_FULL;
    } else {
        s_delayed[free_slot].used = 1;
        s_delayed[free_slot].due = s_tick + ms;
        s_delayed[free_slot].evt = evt;
        s_delayed[free_slot].param = param;
        st = MOTE_OK;
    }
    mote_crit_exit(cs);
    MOTE_TEST_INJECT();
    return st;
#else
    (void)evt;
    (void)param;
    (void)ms;
    {
        /* 与其它调用点一致：note_dropped 必须在临界区内调用 */
        mote_crit_state_t cs = mote_crit_enter();
        mote_note_dropped(evt); /* 口径统一：功能关闭时视为拒绝 */
        mote_crit_exit(cs);
    }
    return MOTE_ERR_FULL;
#endif
}

/* ---------------- 分发与主循环 ---------------- */

static void mote_process_timers(void)
{
    uint32_t now = mote_ticks(); /* 单一快照：M0+ 上裸读 32 位 s_tick 会撕裂 */
    mote_timer_t *t = s_timers;

    while (t != NULL) {
        mote_timer_t *next = t->next; /* 安全迭代：先缓存后继 */
        if ((int32_t)(now - t->due) >= 0) {
            if (t->period != 0) {
                /* 相位稳定：due 按周期推进而不是"从现在重算"（due += period），
                 * handler/主循环延迟不会造成相位逐周期累积漂移；
                 * 错过多拍只合并投递一次（本拍），相位照旧；
                 * 落后超过 MOTE_TIMER_CATCHUP_MAX 拍时放弃旧相位重新对齐，
                 * 防止极端落后时的长循环 */
                uint32_t n = 0;
                do {
                    t->due += t->period;
                } while ((int32_t)(now - t->due) >= 0 &&
                         ++n < MOTE_TIMER_CATCHUP_MAX);
                if ((int32_t)(now - t->due) >= 0) {
                    t->due = now + t->period;
                }
                if (t->policy == MOTE_TIMER_POLICY_LATEST) {
                    mote_event_post_replace(t->evt, t->param);
                } else {
                    /* 周期：RETRY/DROP 都等同丢当次（下一拍正常） */
                    mote_event_post(t->evt, t->param);
                }
            } else {
                switch (t->policy) {
                case MOTE_TIMER_POLICY_DROP:
                    /* 严格截止：失败即弃并释放 */
                    mote_event_post(t->evt, t->param);
                    mote_timer_unlink(t);
                    break;
                case MOTE_TIMER_POLICY_LATEST:
                    /* replace 失败说明满队且无同 ID：截止已过，释放 */
                    mote_event_post_replace(t->evt, t->param);
                    mote_timer_unlink(t);
                    break;
                case MOTE_TIMER_POLICY_RETRY:
                default:
                    /* 至少一次：送达才释放，满队保留重试 */
                    if (mote_event_post(t->evt, t->param) == MOTE_OK) {
                        mote_timer_unlink(t);
                    }
                    break;
                }
            }
        }
        t = next;
    }

#if MOTE_DELAYED_MAX > 0
    for (int i = 0; i < MOTE_DELAYED_MAX; i++) {
        uint16_t evt;
        void *param;
        bool fire;
        mote_crit_state_t cs = mote_crit_enter();

        fire = s_delayed[i].used && (int32_t)(s_tick - s_delayed[i].due) >= 0;
        if (fire) {
            s_delayed[i].used = 0;
            evt = s_delayed[i].evt;
            param = s_delayed[i].param;
        }
        mote_crit_exit(cs);
        if (fire) {
            mote_event_post(evt, param); /* 失败计丢失数 */
        }
    }
#endif
}

bool mote_poll(void)
{
    uint16_t evt;
    void *param;
    bool got;
    mote_crit_state_t cs;

    mote_process_timers();
#if MOTE_ENABLE_TASK
    mote_process_tasks();
#endif
    cs = mote_crit_enter();
    got = mote_q_pop(&evt, &param);
    mote_crit_exit(cs);

    if (got) {
        if (evt < s_evt_count && s_evt_table != NULL) {
            const mote_evt_entry_t *e = &s_evt_table[evt];
            if (e->handler != NULL) {
                e->handler(evt, param, e->ctx);
                return true;
            }
        }
        /* 未注册/越界事件：安全丢弃并计数。
         * 这是受支持的运行时行为（用户忘记登记 ID），不是内部不变量，
         * 因此不设断言——断言构建下恶意/越界事件也必须走到这条丢弃路径 */
        cs = mote_crit_enter();
        mote_note_dropped(evt);
        mote_crit_exit(cs);
        return true;
    }
    return false;
}

/* 临界区内检查并睡眠：消除"查空 → 中断投递 → WFI 漏睡"竞态。
 * ARM/RISC-V 的 wfi 在 pending 中断存在时立即唤醒，
 * 唤醒后先恢复中断再返回，事件不会睡过头 */
static void mote_sleep(void)
{
    mote_crit_state_t cs = mote_crit_enter();

    if (s_q.count == 0) {
        mote_idle(); /* 在关中断状态下调用（契约见 mote.h） */
    }
    mote_crit_exit(cs);
}

void mote_loop(void)
{
    for (;;) {
        if (!mote_poll()) {
            mote_sleep();
        }
    }
}

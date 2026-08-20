/*
 * MoteOS config for CH32V003 low-power current measurement.
 *
 * This is intentionally small: no task layer, no mailbox, only one timer event.
 */
#ifndef MOTE_CONFIG_H
#define MOTE_CONFIG_H

#ifndef MOTE_TICK_MS
#define MOTE_TICK_MS 1
#endif

#ifndef MOTE_EVT_QUEUE_SIZE
#define MOTE_EVT_QUEUE_SIZE 4
#endif

#ifndef MOTE_DELAYED_MAX
#define MOTE_DELAYED_MAX 1
#endif

#ifndef MOTE_ENABLE_TASK
#define MOTE_ENABLE_TASK 0
#endif

#ifndef MOTE_TASK_SLOT_MAX
#define MOTE_TASK_SLOT_MAX 1
#endif

#ifndef MOTE_ENABLE_MAILBOX
#define MOTE_ENABLE_MAILBOX 0
#endif

#ifndef MOTE_TICKLESS
#define MOTE_TICKLESS 1
#endif

#ifndef MOTE_PORT_HCLK_HZ
#define MOTE_PORT_HCLK_HZ 48000000u
#endif

#ifndef MOTE_TIMER_CATCHUP_MAX
#define MOTE_TIMER_CATCHUP_MAX 1000
#endif

#ifndef MOTE_ASSERT
#define MOTE_ASSERT(x)                                                       \
    do {                                                                     \
        if (!(x)) {                                                          \
            mote_assert_fail(__FILE__, __LINE__);                            \
        }                                                                    \
    } while (0)
#endif

#ifdef __cplusplus
extern "C" {
#endif
void mote_assert_fail(const char *file, int line);
#ifdef __cplusplus
}
#endif

#if MOTE_TICK_MS < 1 || MOTE_TICK_MS > 1000
#error "MOTE_TICK_MS must be in 1..1000"
#endif
#if MOTE_EVT_QUEUE_SIZE < 1 || MOTE_EVT_QUEUE_SIZE > 255
#error "MOTE_EVT_QUEUE_SIZE must be in 1..255"
#endif
#if MOTE_TASK_SLOT_MAX < 1 || MOTE_TASK_SLOT_MAX > 255
#error "MOTE_TASK_SLOT_MAX must be in 1..255"
#endif
#if MOTE_TICKLESS && MOTE_PORT_HCLK_HZ < 1000
#error "MOTE_TICKLESS requires MOTE_PORT_HCLK_HZ >= 1000"
#endif

#endif

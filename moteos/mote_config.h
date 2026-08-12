/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MOTE_CONFIG_H
#define MOTE_CONFIG_H

/* 节拍周期（毫秒） */
#ifndef MOTE_TICK_MS
#define MOTE_TICK_MS 1
#endif

/* 事件队列槽数（每槽 8 字节 RAM） */
#ifndef MOTE_EVT_QUEUE_SIZE
#define MOTE_EVT_QUEUE_SIZE 16
#endif

/* mote_event_post_delayed 最大并发数 */
#ifndef MOTE_DELAYED_MAX
#define MOTE_DELAYED_MAX 4
#endif

/* 任务层开关 */
#ifndef MOTE_ENABLE_TASK
#define MOTE_ENABLE_TASK 1
#endif

/* 任务状态槽数（同时活跃任务上限，描述符本身在 Flash） */
#ifndef MOTE_TASK_SLOT_MAX
#define MOTE_TASK_SLOT_MAX 4
#endif

/* 邮箱开关 */
#ifndef MOTE_ENABLE_MAILBOX
#define MOTE_ENABLE_MAILBOX 1
#endif

/* 断言钩子：内核内部不变量检查（生产环境可置空） */
#ifndef MOTE_ASSERT
#define MOTE_ASSERT(x) ((void)0)
#endif

/* ---- 配置边界校验（编译期 #error，防止静默内存损坏） ----
 * 内核队列计数与状态槽均使用 uint8_t，越界配置会导致
 * 计数回绕、除零或死循环，必须在编译期拦截 */
#if MOTE_TICK_MS < 1 || MOTE_TICK_MS > 1000
#error "MOTE_TICK_MS must be in 1..1000"
#endif
#if MOTE_EVT_QUEUE_SIZE < 1 || MOTE_EVT_QUEUE_SIZE > 255
#error "MOTE_EVT_QUEUE_SIZE must be in 1..255 (kernel uses uint8_t counters)"
#endif
#if MOTE_TASK_SLOT_MAX < 1 || MOTE_TASK_SLOT_MAX > 255
#error "MOTE_TASK_SLOT_MAX must be in 1..255"
#endif

#endif

/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOTE_PORT_H
#define MOTE_PORT_H

/* 主机（PC 单元测试）移植：
 * 用共享变量模拟"全局中断开关"，完整实现保存/恢复语义，
 * 内核临界区是否破坏调用方中断状态可在宿主机上直接断言 */

#include <stdint.h>

#define MOTE_PORT_HOST

typedef uint32_t mote_crit_state_t;

extern uint32_t mote_host_primask;

static inline mote_crit_state_t mote_crit_enter(void)
{
    mote_crit_state_t s = mote_host_primask;
    mote_host_primask = 1;
    return s;
}

static inline void mote_crit_exit(mote_crit_state_t s)
{
    mote_host_primask = s;
}

static inline uint32_t mote_crit_active(void)
{
    return mote_host_primask;
}

#endif

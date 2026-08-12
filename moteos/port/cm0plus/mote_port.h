/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOTE_PORT_H
#define MOTE_PORT_H

/* ARM Cortex-M0+（CIU32F003、CH32M030、STM32F030 等）
 * 依赖 CMSIS core_cm0plus.h（芯片 SDK/器件包提供）
 * 临界区采用 PRIMASK 保存/恢复，支持嵌套、不会破坏调用方状态 */

#include "core_cm0plus.h"

typedef uint32_t mote_crit_state_t;

static inline mote_crit_state_t mote_crit_enter(void)
{
    mote_crit_state_t s = __get_PRIMASK();
    __disable_irq();
    return s;
}

static inline void mote_crit_exit(mote_crit_state_t s)
{
    __set_PRIMASK(s);
}

static inline uint32_t mote_crit_active(void)
{
    return __get_PRIMASK();
}

#endif

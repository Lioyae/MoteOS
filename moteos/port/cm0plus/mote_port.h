/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MOTE_PORT_H
#define MOTE_PORT_H

/* ARM Cortex-M0+（CIU32F003、CH32M030、STM32F030 等）
 * 依赖 CMSIS core_cm0plus.h（芯片 SDK/器件包提供） */

#include "core_cm0plus.h"

#define MOTE_ENTER_CRITICAL() __disable_irq()
#define MOTE_EXIT_CRITICAL()  __enable_irq()

#endif

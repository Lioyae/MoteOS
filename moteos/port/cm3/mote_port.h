/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MOTE_PORT_H
#define MOTE_PORT_H

/* ARM Cortex-M3（STM32F103 等）
 * 依赖 CMSIS core_cm3.h（芯片 SDK/器件包提供） */

#include "core_cm3.h"

#define MOTE_ENTER_CRITICAL() __disable_irq()
#define MOTE_EXIT_CRITICAL()  __enable_irq()

#endif

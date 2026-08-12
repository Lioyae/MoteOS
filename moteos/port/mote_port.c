/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mote.h"

#ifdef MOTE_PORT_HOST

uint32_t mote_host_primask;

void mote_idle(void)
{
}

#else

void SysTick_Handler(void)
{
    mote_tick();
}

void mote_idle(void)
{
#if defined(__CC_ARM)
    __asm { wfi }
#else
    __asm volatile ("wfi" ::: "memory");
#endif
}

#endif

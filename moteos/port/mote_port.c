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

/* 弱符号：用户已有自己的 SysTick（延时函数等）时，只需在工程里
 * 重定义一个强符号 SysTick_Handler（记得在里面调用 mote_tick()），
 * 链接器会自动选强符号，无需把 mote_port.c 从工程剔除 */
MOTE_WEAK void SysTick_Handler(void)
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

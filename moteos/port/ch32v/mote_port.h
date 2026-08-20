/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOTE_PORT_H
#define MOTE_PORT_H

/* WCH RISC-V（CH32V003/V007/V203/V307 等，WCH SPL）
 *
 * 器件头选择：默认 V2 代（CH32V003/V007）的 ch32v00x.h；
 * V3 代（CH32V203/V307）在工程宏定义里覆盖：
 *   -DMOTE_CH32_HAL_HEADER=<ch32v20x.h>   （V3 中型）
 *   -DMOTE_CH32_HAL_HEADER=<ch32v30x.h>   （V3 大型）
 * 各代 SDK 的器件头都在定义 IRQn_Type 之后才包含 core_riscv.h，
 * 因此换头后 __enable_irq/__disable_irq/SysTick 布局均可用。
 *
 * 必须包含器件头文件：IRQn_Type 在其中定义，
 * 且它在定义 IRQn_Type 之后才包含 core_riscv.h。
 * 直接包含 core_riscv.h 会因 IRQn_Type 未定义而编译失败。
 *
 * 临界区保存/恢复 mstatus：
 *   本 SDK 的 __enable_irq/__disable_irq 操作 mstatus 的 0x88 位组。
 *   临界区必须保存并恢复同一个寄存器，支持嵌套、不破坏调用方状态。 */

#ifndef MOTE_CH32_HAL_HEADER
#define MOTE_CH32_HAL_HEADER <ch32v00x.h>
#endif
#include MOTE_CH32_HAL_HEADER

/* 体系标签：mote_port.c 的 tickless 实现据此选择青稞 SysTick
 * 访问方式（64 位比较寄存器、向上计数） */
#define MOTE_PORT_CH32 1

/* 与 WCH SDK core_riscv.h 的 __enable_irq/__disable_irq 保持同一口径。 */
#ifndef MOTE_CH32_MSTATUS_IRQ_MASK
#define MOTE_CH32_MSTATUS_IRQ_MASK 0x88u
#endif

typedef uint32_t mote_crit_state_t;

/* 弱符号关键字：mote_port.c 的 SysTick_Handler 用弱符号定义，
 * 用户已有 SysTick 时直接重定义强符号即可接管，无需剔除 mote_port.c */
#define MOTE_WEAK __attribute__((weak))

static inline mote_crit_state_t mote_crit_enter(void)
{
    mote_crit_state_t s;

    __asm volatile("csrr %0, mstatus" : "=r"(s) : : "memory");
    __disable_irq();
    return s;
}

static inline void mote_crit_exit(mote_crit_state_t s)
{
    __asm volatile("csrw mstatus, %0" : : "r"(s) : "memory");
}

static inline uint32_t mote_crit_active(void)
{
    uint32_t s;

    __asm volatile("csrr %0, mstatus" : "=r"(s) : : "memory");
    return ((s & MOTE_CH32_MSTATUS_IRQ_MASK) == MOTE_CH32_MSTATUS_IRQ_MASK) ? 0u : 1u;
}

/* 本 SDK 的 core_riscv.h 未提供 SysTick_Config，这里补齐（CMSIS 兼容签名）：
 * 节拍时钟 = HCLK，比较匹配后自动清零计数并产生 SysTick 中断 */
__attribute__((always_inline)) static inline uint32_t SysTick_Config(uint32_t ticks)
{
    if (ticks == 0) {
        return 1;
    }
    SysTick->SR = 0;
    SysTick->CNT = 0;
    SysTick->CMP = ticks - 1;
    SysTick->CTLR = 0xF;  /* STE | STIE | STCLK(HCLK) | STRE */
    return 0;
}

#endif

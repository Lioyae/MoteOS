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
 * 必须包含器件头文件 ch32v00x.h：IRQn_Type 在其中定义，
 * 且它在定义 IRQn_Type 之后才包含 core_riscv.h。
 * 直接包含 core_riscv.h 会因 IRQn_Type 未定义而编译失败。
 *
 * 临界区保存/恢复 INTSYSCR（CSR 0x800，青稞的中断系统控制寄存器）：
 *   __enable_irq  = 写 0x6088（bit7=1 中断开启）
 *   __disable_irq = 写 0x6000（bit7=0 中断关闭）
 * 完整保存恢复该寄存器，支持嵌套、不破坏调用方状态 */

#include "ch32v00x.h"

/* 青稞中断系统控制寄存器（INTSYSCR）的 CSR 编号。
 * ⚠ 同一 port 头文件服务 V2 代（CH32V003/V007）与 V3 代（CH32V203/V307），
 * 不同代次手册的 CSR 映射存在差异，默认值 0x800 以本 SDK 的 core_riscv.h
 * 口径为准；若目标代次不同，用 -DMOTE_CH32_INTSYSCR=<值> 覆盖。
 * 此寄存器直接决定临界区与 WFI 行为，上板前必须按目标芯片手册实测核验。 */
#ifndef MOTE_CH32_INTSYSCR
#define MOTE_CH32_INTSYSCR 0x800
#endif

typedef uint32_t mote_crit_state_t;

/* 弱符号关键字：mote_port.c 的 SysTick_Handler 用弱符号定义，
 * 用户已有 SysTick 时直接重定义强符号即可接管，无需剔除 mote_port.c */
#define MOTE_WEAK __attribute__((weak))

static inline mote_crit_state_t mote_crit_enter(void)
{
    mote_crit_state_t s;
    __asm volatile("csrr %0, %[csr]" : "=r"(s) : [csr] "i" (MOTE_CH32_INTSYSCR));
    __disable_irq();
    return s;
}

static inline void mote_crit_exit(mote_crit_state_t s)
{
    __asm volatile("csrw %[csr], %0" : : [csr] "i" (MOTE_CH32_INTSYSCR),
                   "r"(s) : "memory");
}

static inline uint32_t mote_crit_active(void)
{
    uint32_t s;
    __asm volatile("csrr %0, %[csr]" : "=r"(s) : [csr] "i" (MOTE_CH32_INTSYSCR));
    return (s & 0x80u) ? 0u : 1u; /* bit7=0 表示中断被关闭 */
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

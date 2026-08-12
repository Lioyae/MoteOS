#ifndef MOTE_PORT_H
#define MOTE_PORT_H

/* WCH RISC-V（CH32V003 等，WCH SPL）
 * 必须包含器件头文件 ch32v00x.h：IRQn_Type 在其中定义，
 * 且它在定义 IRQn_Type 之后才包含 core_riscv.h。
 * 直接包含 core_riscv.h 会因 IRQn_Type 未定义而编译失败 */

#include "ch32v00x.h"

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

#define MOTE_ENTER_CRITICAL() __disable_irq()
#define MOTE_EXIT_CRITICAL()  __enable_irq()

#endif

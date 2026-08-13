/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mote.h"

#ifdef MOTE_PORT_HOST

#include <stdlib.h>

uint32_t mote_host_primask;
uint32_t mote_host_idle_count;
uint32_t mote_host_idle_last_due;

/* 宿主机不睡眠：记录调用供测试验证 mote_sleep 的 deadline 判定 */
void mote_idle(uint32_t next_due)
{
    mote_host_idle_count++;
    mote_host_idle_last_due = next_due;
}

/* 宿主机断言失败：直接 abort，让测试立刻变红而不是挂死 */
void mote_assert_fail(const char *file, int line)
{
    (void)file;
    (void)line;
    abort();
}

#else

#if MOTE_TICKLESS

/* ---- tickless 空闲：按下一 deadline 重装 SysTick 再 wfi ----
 *
 * 协议（详见 docs/porting.md「tickless 空闲」）：
 *  - 时基只在两处入账：SysTick_Handler 把"满拍"入账并恢复固定拍；
 *    mote_idle 把"进入 idle 时已流逝的部分拍"入账（含"匹配已发生但
 *    中断被关、尚未处理"的整拍，靠 COUNTFLAG/CNTIF 读清零判别）。
 *  - s_anchor_cycles 是入账锚点（上次入账时的计数器快照），每次入账
 *    与每次重装后重锚定，杜绝重复/漏计入账。
 *  - s_acc_cycles 保留未达 1ms 的周期余数，跨唤醒零漂移。
 *  - s_nap_cycles==0 表示启动后尚未 tickless 编程过（固定拍运行）。 */

static volatile uint32_t s_nap_ms = MOTE_TICK_MS; /* 当前编程拍长（ms） */
static volatile uint32_t s_nap_cycles;            /* 当前编程拍长（周期） */
static volatile uint32_t s_anchor_cycles;         /* 入账锚点（周期快照） */
static volatile uint32_t s_acc_cycles;            /* 已流逝未入账周期 */

#if defined(MOTE_PORT_CORTEXM)
/* Cortex-M SysTick：24 位向下计数。裸寄存器地址访问，
 * 不依赖 CMSIS 头文件（port 层零依赖契约） */
#define MOTE_SYST_CTRL (*(volatile uint32_t *)0xE000E010u)
#define MOTE_SYST_LOAD (*(volatile uint32_t *)0xE000E014u)
#define MOTE_SYST_VAL  (*(volatile uint32_t *)0xE000E018u)
#define MOTE_SYST_MAX_CYCLES 0x00FFFFFFu
#else
/* WCH 青稞 SysTick：64 位比较寄存器、向上计数（ch32v00x.h 提供寄存器）。
 * 单次 nap 上限取 31 位（回绕比较数学留裕量） */
#define MOTE_SYST_MAX_CYCLES 0x7FFFFFFFu
#endif

/* 当前拍剩余周期数 */
static uint32_t mote_systick_remaining(void)
{
#if defined(MOTE_PORT_CORTEXM)
    return MOTE_SYST_VAL & MOTE_SYST_MAX_CYCLES;
#else
    return (uint32_t)(SysTick->CMP - SysTick->CNT);
#endif
}

/* 本拍是否已匹配（读清零：Cortex-M COUNTFLAG / 青稞 CNTIF）。
 * 返回 true 时标志已消费，后续调用者不会再看到 */
static bool mote_systick_matched(void)
{
#if defined(MOTE_PORT_CORTEXM)
    return (MOTE_SYST_CTRL & 0x00010000u) != 0u;
#else
    return (SysTick->SR & 1u) != 0u;
#endif
}

/* 把累计周期入账为毫秒（保留周期余数，跨唤醒零漂移）。
 * 返回本次可推进时基的毫秒数。 */
static uint32_t mote_acc_flush_ms(void)
{
    uint32_t ms = (uint32_t)((uint64_t)s_acc_cycles * 1000u
                             / MOTE_PORT_HCLK_HZ);

    if (ms != 0u) {
        s_acc_cycles -= (uint32_t)((uint64_t)ms * MOTE_PORT_HCLK_HZ / 1000u);
    }
    return ms;
}

/* 重装 SysTick 为指定毫秒拍长（从当前时刻起算，计数器复位），
 * 并重锚定入账锚点 */
static void mote_systick_set(uint32_t ms)
{
    uint32_t cycles = (uint32_t)(((uint64_t)ms * MOTE_PORT_HCLK_HZ + 999u)
                                 / 1000u);

    if (cycles > MOTE_SYST_MAX_CYCLES) {
        cycles = MOTE_SYST_MAX_CYCLES;
    }
    s_nap_cycles = cycles;
#if defined(MOTE_PORT_CORTEXM)
    MOTE_SYST_LOAD = cycles - 1u;
    MOTE_SYST_VAL = 0; /* 先 LOAD 后 VAL：VAL 清零触发立即从新 LOAD 重载 */
#else
    SysTick->CNT = 0;
    SysTick->CMP = (uint64_t)cycles - 1u;
#endif
    s_anchor_cycles = mote_systick_remaining();
}

/* 单次 nap 上限（ms）：受 SysTick 计数器位宽与 2^31 回绕数学共同约束 */
static uint32_t mote_max_nap_ms(void)
{
    uint64_t cap = (uint64_t)MOTE_SYST_MAX_CYCLES * 1000u
                   / MOTE_PORT_HCLK_HZ;

    if (cap < 1u) {
        cap = 1u;
    }
    if (cap > 0x40000000u) {
        cap = 0x40000000u; /* 2^30：回绕比较数学留裕量 */
    }
    return (uint32_t)cap;
}

/* 弱符号：用户已有自己的 SysTick（延时函数等）时，只需在工程里
 * 重定义一个强符号 SysTick_Handler（记得在里面调用 mote_tick()
 * 或 mote_tick_advance()），链接器会自动选强符号，
 * 无需把 mote_port.c 从工程剔除 */
MOTE_WEAK void SysTick_Handler(void)
{
    if (s_nap_cycles == 0u) {
        mote_tick(); /* 启动后尚未 tickless 编程：固定拍 */
        return;
    }
    /* 匹配已由 mote_idle 抢先消化（中断被关期间）时标志已被读清零，
     * 走到这里直接跳过；否则满拍入账 */
    if (mote_systick_matched()) {
        s_acc_cycles += s_nap_cycles;
        mote_tick_advance(mote_acc_flush_ms());
        s_anchor_cycles = mote_systick_remaining();
        if (s_nap_ms != MOTE_TICK_MS) {
            /* 长拍到期：恢复固定拍 */
            mote_systick_set(MOTE_TICK_MS);
            s_nap_ms = MOTE_TICK_MS;
        }
    }
}

void mote_idle(uint32_t next_due)
{
    uint32_t nap;
    bool do_sleep = true;

    /* ① 时基追平：进入 idle 时把已流逝周期入账——
     *    匹配已发生（中断被关、pending 未处理）→ 整拍入账；
     *    否则按锚点差入账部分拍（提前唤醒场景） */
    if (s_nap_cycles != 0u) {
        if (mote_systick_matched()) {
            s_acc_cycles += s_nap_cycles;
        } else {
            s_acc_cycles += s_anchor_cycles - mote_systick_remaining();
        }
        mote_tick_advance(mote_acc_flush_ms());
        s_anchor_cycles = mote_systick_remaining();
    }
    /* ② 按下一 deadline 计算 nap */
    nap = mote_max_nap_ms(); /* 无到期项：长睡等任意中断 */
    if (next_due != MOTE_TICK_NONE) {
        uint32_t now = mote_ticks();

        if ((int32_t)(next_due - now) <= 0) {
            do_sleep = false; /* 追平后已到期：不睡，主循环立即处理 */
        } else {
            uint32_t d = next_due - now;

            if (d < nap) {
                nap = d;
            }
        }
    }
    if (!do_sleep) {
        return;
    }
    /* ③ 重装 SysTick 后 wfi（关中断状态下调用；pending 中断会唤醒） */
    mote_systick_set(nap);
    s_nap_ms = nap;
#if defined(__CC_ARM)
    __asm { wfi }
#else
    __asm volatile ("wfi" ::: "memory");
#endif
}

#else /* !MOTE_TICKLESS：固定拍 */

/* 弱符号：用户已有自己的 SysTick（延时函数等）时，只需在工程里
 * 重定义一个强符号 SysTick_Handler（记得在里面调用 mote_tick()），
 * 链接器会自动选强符号，无需把 mote_port.c 从工程剔除 */
MOTE_WEAK void SysTick_Handler(void)
{
    mote_tick();
}

void mote_idle(uint32_t next_due)
{
    (void)next_due;
#if defined(__CC_ARM)
    __asm { wfi }
#else
    __asm volatile ("wfi" ::: "memory");
#endif
}

#endif /* MOTE_TICKLESS */

/* 断言失败默认处理：停机。弱符号，用户可重定义为自己的错误处理
 * （记录断言位置后复位、进入 bootloader 等）。 */
MOTE_WEAK void mote_assert_fail(const char *file, int line)
{
    (void)file;
    (void)line;
    for (;;) {
    }
}

#endif /* !MOTE_PORT_HOST */

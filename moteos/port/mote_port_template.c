/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * MoteOS 移植模板（非 CMSIS 内核/裸机工具链用）
 *
 * 移植只需三步，零汇编：
 *
 * 1) 让某个硬件定时器以 MOTE_TICK_MS 为周期产生中断，
 *    在中断服务函数里调用 mote_tick()；
 *    不要做任何其他事，中断越短越好。
 *
 * 2) 提供 mote_idle()：进入芯片低功耗模式。
 *    ARM/RISC-V 内核一条 wfi 即可；其他架构换成对应指令。
 *
 * 3) 提供临界区 API（保存/恢复式，支持嵌套）：
 *      mote_crit_state_t  中断状态类型
 *      mote_crit_enter()  保存当前状态并关中断，返回保存的状态
 *      mote_crit_exit(s)  恢复保存的状态（禁止无条件打开中断）
 *      mote_crit_active() 查询当前是否处于关中断（1=关）
 *    参考现成实现：Cortex-M 用 PRIMASK（port/cm0plus、port/cm3），
 *    WCH RISC-V 用 INTSYSCR（port/ch32v），AVR 用 SREG。
 *
 * 把本文件改名为 mote_port.c 放进工程，并把本目录加入头文件搜索路径。
 */

#include "mote.h"

/* 例：你的 1ms 定时器中断 */
void Timer1_ISR(void)
{
    mote_tick();
    /* 清除中断标志 */
    clear_timer1_flag();
}

void mote_idle(void)
{
    /* ARM / RISC-V 通用低功耗指令；其他架构换成对应指令 */
    __asm volatile("wfi");
}

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
 * 3) 定义临界区宏 MOTE_ENTER_CRITICAL()/MOTE_EXIT_CRITICAL()：
 *    关全局中断 / 开全局中断。
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

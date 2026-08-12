/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * MoteOS 例程：CH32V003（WCH SPL）
 *
 * 功能：LED 周期闪烁（定时器）、UART 回环（邮箱，中断接收→事件处理→回发）、
 *       心跳任务（任务层，1s 翻转另一 LED）
 *
 * 工程配置（MRS / EIDE）：
 *   - 源文件：本文件 + moteos/mote.c + moteos/mote_task.c + moteos/mote_mail.c
 *             + moteos/port/mote_port.c
 *   - 头文件路径：moteos/ 和 moteos/port/ch32v/
 *   - 无需定义任何宏；SysTick 由 mote_port.c 接管
 */

#include "mote.h"
#include "ch32v00x.h"

/* ---- 事件 ID（连续枚举，从 0 起） ---- */
enum {
    EVT_LED = 0,   /* 周期闪烁 */
    EVT_UART = 1,  /* 收到串口数据（邮箱事件） */
};

/* ---- 邮箱：4 槽 × 32 字节 ---- */
MOTE_MAILBOX_DEF(uart_mb, EVT_UART, 4, 32);

static mote_timer_t blink_timer;

/* ---- 任务层描述符（Flash，不启动不占 RAM） ---- */
static void heartbeat(uint16_t evt, void *param, void *ctx);
static const mote_task_desc_t tasks[] = {
    MOTE_TASK_DEF(1000, heartbeat, NULL),
};

/* ---- 事件处理器 ---- */
static void led_handler(uint16_t evt, void *param, void *ctx)
{
    (void)evt; (void)param; (void)ctx;
    GPIOC->OUTDR ^= GPIO_Pin_0;  /* 翻转 PC0 */
}

static void uart_handler(uint16_t evt, void *param, void *ctx)
{
    (void)evt; (void)ctx;
    mote_mail_t *mb = (mote_mail_t *)param;
    uint8_t buf[32];
    int n;

    while ((n = mote_mail_recv(mb, buf)) > 0) {
        for (int i = 0; i < n; i++) {
            while (!(USART1->STATR & USART_STATR_TC)) { }
            USART1->DATAR = buf[i];
        }
    }
}

static void heartbeat(uint16_t evt, void *param, void *ctx)
{
    (void)evt; (void)param; (void)ctx;
    GPIOC->OUTDR ^= GPIO_Pin_1;  /* 翻转 PC1 */
}

static const mote_evt_entry_t evt_table[] = {
    [EVT_LED]  = MOTE_ENTRY(led_handler, NULL),
    [EVT_UART] = MOTE_ENTRY(uart_handler, NULL),
};

/* ---- 外设初始化 ---- */
static void gpio_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD
                      | RCC_APB2Periph_USART1;

    /* PC0/PC1：10MHz 推挽输出 */
    GPIOC->CFGLR &= ~(0xFF << 0);
    GPIOC->CFGLR |= (0x1 << 0) | (0x1 << 4);

    /* PD5 = USART1_TX：50MHz 复用推挽；PD6 = USART1_RX：浮空输入 */
    GPIOD->CFGLR &= ~(0xFF << 20);
    GPIOD->CFGLR |= (0xB << 20) | (0x4 << 24);

    USART1->BRR = SystemCoreClock / 115200;
    USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE | USART_CTLR1_RE
                    | USART_CTLR1_RXNEIE;
    NVIC_EnableIRQ(USART1_IRQn);
}

int main(void)
{
    SystemInit();
    gpio_init();

    /* 1ms 节拍：SysTick 中断由 port 层接管（SysTick_Handler → mote_tick） */
    SysTick_Config(SystemCoreClock / (1000 / MOTE_TICK_MS));

    mote_init(evt_table, sizeof(evt_table) / sizeof(evt_table[0]));
    mote_task_init(tasks, sizeof(tasks) / sizeof(tasks[0]));

    mote_timer_start(&blink_timer, EVT_LED, NULL, 500, true);
    mote_task_start(0);  /* 心跳任务 */

    mote_loop();  /* 永不返回 */
}

/* 串口接收中断：只做深拷贝投递，中断最短 */
void USART1_IRQHandler(void)
{
    if (USART1->STATR & USART_STATR_RXNE) {
        uint8_t c = (uint8_t)(USART1->DATAR & 0xFF);
        mote_mail_send(&uart_mb, &c, 1);
    }
}

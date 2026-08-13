/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * MoteOS 例程：STM32F103（Blue Pill，寄存器级）
 *
 * 功能：LED 周期闪烁（定时器）、UART 回环（邮箱，中断接收→事件处理→回发）、
 *       心跳任务（任务层，1s 翻转另一 LED）
 *
 * 工程配置（Keil / CMake）：
 *   - 源文件：本文件 + moteos/mote.c + moteos/mote_task.c + moteos/mote_mail.c
 *             + moteos/port/mote_port.c
 *   - 头文件路径：moteos/ 和 moteos/port/cm3/
 *   - 无需定义任何宏；SysTick 由 mote_port.c 接管
 *   - MoteOS 内核不依赖 CMSIS，无包含顺序要求；本例程外设代码
 *     使用 CMSIS 器件头（stm32f1xx.h）
 *     旧版标准库（SPL）用户把 stm32f1xx.h 换回 stm32f10x.h 即可
 */

/* 器件选择宏：STM32F103xB = F103C8/CB（Blue Pill），按你的型号改 */
#define STM32F103xB
#include "stm32f1xx.h"
#include "mote.h"

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
    GPIOC->ODR ^= (1u << 13);  /* 翻转 PC13 */
}

static void uart_handler(uint16_t evt, void *param, void *ctx)
{
    (void)evt; (void)ctx;
    mote_mail_t *mb = (mote_mail_t *)param;
    uint8_t buf[32];
    int n;

    /* 注意：这里等 TXE（数据寄存器空）即可——上一字节从 DR 搬进移位寄存器
     * 后就能写下一字节，只等 0~1 个字节时间。
     * ⚠ 不要等 TC（传输完成）：那要等整个字节从引脚发完，32 字节回环会
     * 阻塞约 2.8ms，违反铁律 1（handler 毫秒级返回）。
     * 更严谨的姿势是"环形缓冲 + TXE 发送中断"状态机（见 docs/usage.md
     * 附录 B），handler 完全不碰忙等 */
    while ((n = mote_mail_recv(mb, buf)) > 0) {
        for (int i = 0; i < n; i++) {
            while (!(USART1->SR & USART_SR_TXE)) { }
            USART1->DR = buf[i];
        }
    }
}

static void heartbeat(uint16_t evt, void *param, void *ctx)
{
    (void)evt; (void)param; (void)ctx;
    GPIOC->ODR ^= (1u << 14);  /* 翻转 PC14 */
}

static const mote_evt_entry_t evt_table[] = {
    [EVT_LED]  = MOTE_ENTRY(led_handler, NULL),
    [EVT_UART] = MOTE_ENTRY(uart_handler, NULL),
};

/* ---- 外设初始化 ---- */
static void gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN
                    | RCC_APB2ENR_IOPAEN;

    /* PC13/PC14：2MHz 推挽输出 */
    GPIOC->CRH &= ~(0xFF << 20);
    GPIOC->CRH |= (0x2 << 20) | (0x2 << 24);

    /* PA9 = USART1_TX：复用推挽；PA10 = USART1_RX：浮空输入 */
    GPIOA->CRH &= ~(0xFF << 4);
    GPIOA->CRH |= (0xB << 4) | (0x4 << 8);

    USART1->BRR = SystemCoreClock / 115200;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE
                  | USART_CR1_RXNEIE;
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
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t c = (uint8_t)(USART1->DR & 0xFF);
        mote_mail_send(&uart_mb, &c, 1);
    }
}

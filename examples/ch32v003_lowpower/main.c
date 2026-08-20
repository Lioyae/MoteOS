/*
 * MoteOS - CH32V003 low-power current measurement example.
 *
 * Purpose:
 *   Measure board current with minimum application noise:
 *   - boot print once on USART1 TX PD5
 *   - turn PC0 LED off, then put PC0 into analog input
 *   - disable USART1 and put PD5/PD6 into analog input after the boot print
 *   - run tickless MoteOS with one long-period wake event
 *
 * Required project-wide MoteOS config:
 *   MOTE_TICKLESS=1
 *   MOTE_PORT_HCLK_HZ=48000000u
 */

#include "mote.h"
#include "ch32v00x.h"

#if !MOTE_TICKLESS
#error "This low-power example requires project-wide MOTE_TICKLESS=1."
#endif

#if MOTE_PORT_HCLK_HZ != 48000000u
#error "This example expects MOTE_PORT_HCLK_HZ=48000000u for CH32V003 at 48MHz."
#endif

#ifndef STAGE16_WAKE_PERIOD_MS
#define STAGE16_WAKE_PERIOD_MS 2000u
#endif

/* Set to 0 for the cleanest current measurement.
 * Default 1 gives a short PC1 pulse at every wake, useful as a scope trigger. */
#ifndef STAGE16_MARKER_ENABLE
#define STAGE16_MARKER_ENABLE 1
#endif

/* Keep the marker intentionally visible on a seconds/div scope view.
 * Disable STAGE16_MARKER_ENABLE for final clean-current measurement. */
#ifndef STAGE16_MARKER_PULSE_LOOPS
#define STAGE16_MARKER_PULSE_LOOPS 200000u
#endif

enum {
    EVT_WAKE = 0,
};

static mote_timer_t wake_timer;
static volatile uint32_t s_wake_count;

static void uart_putc(char c)
{
    if (c == '\n') {
        uart_putc('\r');
    }
    while ((USART1->STATR & USART_STATR_TXE) == 0u) {
    }
    USART1->DATAR = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

static void uart_put_u32(uint32_t v)
{
    char buf[10];
    uint8_t n = 0;

    if (v == 0u) {
        uart_putc('0');
        return;
    }
    while (v != 0u && n < sizeof(buf)) {
        buf[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0u) {
        uart_putc(buf[--n]);
    }
}

static void gpio_uart_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD |
                      RCC_APB2Periph_USART1;

    /* PC0 starts as push-pull high so the active-low LED is off during boot.
     * PC1 is an optional wake marker output. */
    GPIOC->CFGLR &= ~(0xFFu << 0);
    GPIOC->CFGLR |= (0x1u << 0) | (0x1u << 4);
    GPIOC->OUTDR |= GPIO_Pin_0;
    GPIOC->OUTDR &= (uint16_t)~GPIO_Pin_1;

    /* PD5 = USART1_TX alternate push-pull, PD6 = floating input. */
    GPIOD->CFGLR &= ~(0xFFu << 20);
    GPIOD->CFGLR |= (0xBu << 20) | (0x4u << 24);

    USART1->BRR = SystemCoreClock / 115200u;
    USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE;
}

static void quiet_pins_after_boot(void)
{
    while ((USART1->STATR & USART_STATR_TC) == 0u) {
    }
    USART1->CTLR1 = 0;
    RCC->APB2PCENR &= (uint32_t)~RCC_APB2Periph_USART1;

    /* PD5/PD6 analog input: remove UART output/input bias from the measurement. */
    GPIOD->CFGLR &= ~(0xFFu << 20);

    /* PC0 analog input: external pull-up keeps the active-low LED off without
     * driving the pin. Keep PC1 as output only when marker is enabled. */
    GPIOC->CFGLR &= ~(0xFu << 0);
#if STAGE16_MARKER_ENABLE
    GPIOC->CFGLR &= ~(0xFu << 4);
    GPIOC->CFGLR |= (0x1u << 4);
#else
    GPIOC->CFGLR &= ~(0xFu << 4);
    RCC->APB2PCENR &= (uint32_t)~(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD);
#endif
}

static void wake_handler(uint16_t evt, void *param, void *ctx)
{
    (void)evt;
    (void)param;
    (void)ctx;

    s_wake_count++;

#if STAGE16_MARKER_ENABLE
    GPIOC->OUTDR |= GPIO_Pin_1;
    for (volatile uint32_t i = 0; i < STAGE16_MARKER_PULSE_LOOPS; i++) {
        __asm volatile ("nop");
    }
    GPIOC->OUTDR &= (uint16_t)~GPIO_Pin_1;
#endif
}

static const mote_evt_entry_t evt_table[] = {
    [EVT_WAKE] = MOTE_ENTRY(wake_handler, NULL),
};

int main(void)
{
    uint32_t systick_reload;

    SystemInit();
    gpio_uart_init();

    uart_puts("\n==== MoteOS CH32V003 Stage16 ====\n");
    uart_puts("low-power current measurement, tickless + WCH __WFI\n");
    uart_puts("boot print only; USART off after this banner\n");
    uart_puts("PC0 LED off/analog; wake_ms=");
    uart_put_u32(STAGE16_WAKE_PERIOD_MS);
    uart_puts("; marker_pc1=");
    uart_put_u32(STAGE16_MARKER_ENABLE);
    uart_puts("\n");

    systick_reload = (SystemCoreClock / 1000u) * MOTE_TICK_MS +
                     ((SystemCoreClock % 1000u) * MOTE_TICK_MS) / 1000u;
    if (systick_reload == 0u || SysTick_Config(systick_reload) != 0u) {
        for (;;) {
        }
    }

    mote_init(evt_table, sizeof(evt_table) / sizeof(evt_table[0]));
    if (mote_timer_start(&wake_timer, EVT_WAKE, NULL,
                         STAGE16_WAKE_PERIOD_MS, true) != MOTE_OK) {
        for (;;) {
        }
    }

    quiet_pins_after_boot();
    mote_loop();
}

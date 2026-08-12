<p align="center">
  <img src="brand/moteos-icon.svg" alt="MoteOS Logo" width="160">
</p>

<h1 align="center">MoteOS</h1>

<p align="center">
  <strong>An event-driven cooperative kernel for tiny MCUs</strong><br>
  No assembly · No dynamic memory allocation · Resource usage fixed at compile time
</p>

<p align="center">
  <a href="https://github.com/Lioyae/MoteOS/actions/workflows/build.yml">
    <img src="https://img.shields.io/github/actions/workflow/status/Lioyae/MoteOS/build.yml?style=for-the-badge" alt="Build Status">
  </a>
  <a href="https://github.com/Lioyae/MoteOS">
    <img src="https://img.shields.io/badge/language-C99-2b6cb0?style=for-the-badge" alt="C99">
  </a>
  <a href="https://github.com/Lioyae/MoteOS/stargazers">
    <img src="https://img.shields.io/github/stars/Lioyae/MoteOS?style=for-the-badge&color=d69e2e" alt="Stars">
  </a>
  <a href="https://github.com/Lioyae/MoteOS/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/Lioyae/MoteOS?style=for-the-badge&color=38a169" alt="License">
  </a>
</p>

<p align="right">
  <a href="README.md">简体中文</a> | English
</p>

---

## About

MoteOS is a C99 event-driven cooperative kernel for small MCUs (2KB RAM / 16KB Flash class).

- No assembly, no dynamic memory allocation, no blocking delay APIs
- All RAM/Flash usage is fixed at compile time and verifiable by the linker
- Supports ARM Cortex-M0+/M3 and RISC-V; interrupt latency is affected only by the tick interrupt

## Supported Platforms

| Core | Example chips |
|---|---|
| RISC-V (WCH QingKe) | CH32V003 / CH32V007 / CH32V203 / CH32V307 |
| Cortex-M0+ | CIU32F003 / CH32M030 / STM32F030 |
| Cortex-M3 | STM32F103 |
| x86 (host) | Runs kernel unit tests on PC |

## Resource Usage

Baseline: CH32V003 (16KB Flash / 2KB RAM, all modules enabled).

| Item | Usage |
|---|---|
| Kernel Flash | ~2KB |
| Kernel RAM | ~300B (event queue 16 slots + delayed slots 4 + task slots 4) |
| Left for the application | ~14KB Flash / 1.7KB RAM |

## Modules

| Module | Description |
|---|---|
| Event queue | `mote_event_post` / `mote_event_post_replace` (latest wins per ID) / `mote_event_post_delayed` |
| Dispatch table | C99 designated initializers; event ID is the index; O(1) dispatch; table lives in Flash |
| Timers | Statically defined, unlimited count; 32-bit wraparound safe; auto-post events on expiry |
| Task layer | Descriptors in Flash, state slot pool in RAM; inactive tasks consume no RAM (optional) |
| Mailbox | Static slots with deep copy; fill from ISR, drain from handler (optional) |
| Low power | Enters `mote_idle()` (wfi by default) when idle; woken by the tick interrupt |

## Quick Start

```c
#include "mote.h"

enum { EVT_BLINK = 0 };

static mote_timer_t blink_timer;

static void blink_handler(uint16_t evt, void *param, void *ctx)
{
    led_toggle();
}

static const mote_evt_entry_t evt_table[] = {
    [EVT_BLINK] = MOTE_ENTRY(blink_handler, NULL),
};

int main(void)
{
    systick_start(1);  /* 1ms tick, call mote_tick() in the ISR */
    mote_init(evt_table, sizeof(evt_table) / sizeof(evt_table[0]));
    mote_timer_start(&blink_timer, EVT_BLINK, NULL, 500, true);

    mote_loop();  /* Never returns */
}
```

## Documentation

- [Porting guide](docs/porting.md) (Chinese): Keil / MounRiver integration, SysTick conflicts, non-CMSIS chips, checklist
- [Usage guide](docs/usage.md) (Chinese): glossary, line-by-line walkthrough of events / timers / mailboxes / tasks, full example project

## Rules

1. Handlers must be non-blocking and return within milliseconds; split long flows into state machines (the kernel provides no blocking delay)
2. Event params must point to global/static storage, or hold ≤32-bit values via `MOTE_P()/MOTE_U32()`; use mailboxes for large data
3. Timer/task APIs are main-loop-context only; `mote_event_post*` and `mote_mail_send` may be called from ISRs
4. Event IDs are a contiguous enum starting from 0 (the ID is the dispatch table index)

## Configuration

All tunables live in `moteos/mote_config.h`:

```c
#define MOTE_TICK_MS        1    /* tick period in milliseconds */
#define MOTE_EVT_QUEUE_SIZE 16   /* event queue slots */
#define MOTE_DELAYED_MAX    4    /* concurrent delayed posts */
#define MOTE_ENABLE_TASK    1    /* task layer switch */
#define MOTE_TASK_SLOT_MAX  4    /* max simultaneously active tasks */
#define MOTE_ENABLE_MAILBOX 1    /* mailbox switch */
```

## Build and Test

The kernel is pure logic; unit tests run on PC:

```bash
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Directory Structure

```
moteos/
├── mote.h / mote.c          # kernel
├── mote_config.h            # single configuration point
├── mote_task.c              # task layer (optional)
├── mote_mail.c              # mailbox (optional)
└── port/                    # porting layer (per core: ch32v / cm0plus / cm3 / host)
examples/                    # per-chip examples
tests/                       # PC unit tests
docs/                        # porting and usage guides
brand/                       # brand assets
```

## License

Licensed under the [Apache License 2.0](LICENSE).

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
  <a href="https://github.com/Lioyae/MoteOS/tags">
    <img src="https://img.shields.io/github/v/tag/Lioyae/MoteOS?style=for-the-badge&color=2b6cb0" alt="Version">
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

- No assembly source files in the kernel (port layer uses inline assembly; vendor startup files/vector tables are still required), no dynamic memory allocation, no blocking delay APIs
- All RAM/Flash usage is fixed at compile time; CI cross-compiles and asserts kernel size
- Supports ARM Cortex-M0+/M3 and RISC-V; interrupt latency = tick interrupt + kernel critical sections (event enqueue O(1); mailbox copy proportional to item_size; post_replace proportional to queue length). Critical-section duration depends on configuration and clock, and **must be measured per platform** (estimation formulas and measurement methods in the [usage guide appendix A](docs/usage.md))

## Project Status (Important)

**Development preview (v0.x), not board-verified.**

- ✅ Verified: host unit/interleave tests (ASan/UBSan, multi-seed), assert-enabled build, worst-case config build (queue 255), **QEMU smoke test** (Cortex-M3: boot/vector table/SysTick/tick→timer→event flow), **gcov coverage gate (≥85% lines)**, **cppcheck static analysis**, M0+/M3/RV32 cross-compilation with size assertions, real-SDK example compilation — all automated by CI
- ❌ Not verified: the kernel has never run on real silicon. Interrupt timing, measured critical-section duration, WFI low-power wakeup (including QingKe INTSYSCR/WFI interaction), and periodic-timer phase drift have no board-level measurements
- ⚠️ Before production use, complete the board-level verification per the [porting checklist](docs/porting.md). The v1.0.x "production ready" tags have been retracted (see [CHANGELOG](CHANGELOG.md))

## Supported Platforms

| Core | Example chips |
|---|---|
| RISC-V (WCH QingKe) | CH32V003 / CH32V007 / CH32V203 / CH32V307 |
| Cortex-M0+ | CIU32F003 / CH32M030 / STM32F030 |
| Cortex-M3 | STM32F103 |
| x86 (host) | Runs kernel unit tests on PC |

> Except for the host, all platforms above are **verified by cross-compilation only; never run on hardware**.

## Resource Usage

| Item | Usage |
|---|---|
| Kernel Flash | RV32 ~2.7KB, Cortex-M0+ ~2.2KB (CI cross-compiles the three kernel .o at -Os; asserts RV32 <2.75KB, M0+ <2.5KB) |
| Kernel RAM | ~280B with default config (event queue 16 slots + delayed 4 + task slots 4); CI asserts <512B |
| Full blink example | Manually measured on CH32V003: FLASH 2.7KB / RAM 712B (including startup and stack; **example size is not CI-asserted**). Note: 712B is 35% of a 2KB RAM — the rest must cover app data and stack |

## Modules

| Module | Description |
|---|---|---|
| Event queue | `mote_event_post` / `mote_event_post_replace` (latest wins per ID) / `mote_event_post_delayed` (with `_replace` and `mote_event_cancel_delayed`); drop counter `mote_dropped_count()` |
| Dispatch table | C99 designated initializers; event ID is the index; O(1) dispatch; table lives in Flash |
| Timers | Statically defined; 32-bit wraparound safe; list sorted by due time so expiry scanning only visits due nodes (idle poll is O(1)); periodic timers fire on absolute phase (missed ticks coalesce, no cumulative drift); selectable full-queue policy: retry / drop (strict deadline) / latest (replace semantics) — note: **periodic timers drop the beat on a full queue and proceed next beat; one-shot RETRY timers retry on the next tick until delivered** |
| Task layer | Periodic-callback convenience layer: descriptors in Flash (handler + ctx + period), state slot pool in RAM; inactive tasks consume no RAM (optional). Note: **not RTOS tasks** — no preemption, handlers are called synchronously by the main loop, unrelated to the event queue |
| Mailbox | Static slots with deep copy; slot insert and event enqueue are atomic within one critical section (all-or-nothing, no race window); variable-length items (1..item_size bytes per slot with item_size≤255, `recv` returns the actual stored length, oversize sends are rejected — never truncated, +1 byte length overhead per slot); invalid constructions (slots==0, NULL buffers, etc.) are rejected at runtime (optional) |
| Low power | Deadline-aware: `mote_next_due()` exposes the next expiry; the kernel sleeps (into `mote_idle(next_due)`) only when the queue is empty and nothing is due; optional **tickless** idle (`MOTE_TICKLESS=1`) reloads SysTick to the next deadline before wfi and restores the fixed rate on wake. Race handling is correct by reasoning, but **WFI behavior on each chip (especially QingKe) is not board-verified** |
| Critical section | Save/restore style (PRIMASK / INTSYSCR), nesting-safe |
| Observability | `mote_dropped_count()` unified drop counter + `mote_set_drop_hook()` drop callback (event/mailbox APIs only inside the hook) |

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
- [Test documentation](docs/test.md) (Chinese): test matrix, interleave test design, QEMU smoke, coverage & static analysis, local run instructions

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
#define MOTE_TICKLESS       1    /* tickless idle (requires the next line) */
#define MOTE_PORT_HCLK_HZ   48000000u  /* core clock in Hz, tickless only */
```

> `MOTE_TICKLESS` / `MOTE_PORT_HCLK_HZ` must be defined **project-wide**
> (`mote_port.c` compiles against them too), not just in one .c file.
> Complete the tickless board-verification checklist in the [porting guide](docs/porting.md) before use.

## Build and Test

The kernel is pure logic; unit tests run on PC (`ctest` runs three builds by default: regular config, assert-enabled `test_moteos_assert`, and worst-case `test_moteos_max` — queue 255 / delayed 16 / task slots 16):

```bash
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

> The interleave tests verify the kernel's consistency against a **modeled concurrency semantics** (no preemption inside critical sections); pseudo-interrupt injection windows cover `mote_event_post*` / `mote_mail_send` (before critical section) / `mote_poll` (before each step) / `mote_process_timers` (during list traversal). They do not constitute hardware verification; real hardware timing must be verified on the board (see Project Status above).

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

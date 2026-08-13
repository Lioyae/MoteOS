# MoteOS 测试文档

> 本文汇总 MoteOS 的全部验证手段、当前结果与已知局限。
> 一句话结论：**逻辑正确性有相当厚的自动化验证，硬件行为没有板级数据。**
> 测试代码在 `tests/`，CI 定义在 `.github/workflows/build.yml`。

---

## 测试矩阵

| # | 层 | 手段 | 验证什么 | CI job |
|---|---|---|---|---|
| 1 | 逻辑 | 宿主机单元测试 | 事件队列/定时器/任务/邮箱的 API 语义 | host-tests |
| 2 | 并发 | 交错测试（多种子） | 对"建模并发语义"的一致性（见局限声明） | host-tests / sanitizers |
| 3 | 防御 | 断言开启构建 | `MOTE_ASSERT` 路径被真实编译并运行 | host-tests（ctest 内置） |
| 4 | 边界 | 最坏配置构建（队列 255 等） | 极端配置下的正确性 | host-tests + cross-compile |
| 5 | 内存 | ASan/UBSan | 内存错误、未定义行为 | sanitizers |
| 6 | 真实启动 | QEMU 冒烟（Cortex-M3） | 启动/向量表/SysTick/tick→定时器→事件流 | qemu-smoke |
| 7 | 可编译性 | 交叉编译 + 体积断言 | M0+/M3/RV32 可编译、体积不失控 | cross-compile |
| 8 | 集成 | 真实 SDK 例程编译 | CMSIS/WCH SDK 真实头文件下例程可编译 | cross-compile |
| 9 | 覆盖 | gcovr 行覆盖 ≥85% 门槛 | 测试没有大面积盲区 | coverage |
| 10 | 静态 | cppcheck | warning/performance/portability | cppcheck |

---

## 1. 宿主机单元测试（`tests/test_*.c`）

内核是纯逻辑，全部在 PC 上跑（`moteos/port/host` 用共享变量模拟中断开关，
临界区是否破坏调用方中断状态可在宿主机直接断言）。

| 套件 | 覆盖点 |
|---|---|
| `suite_queue` | post/派发、满队报错、replace 覆盖、越界 ID 安全丢弃、空 handler、丢弃计数、drop hook（含重入）、临界区嵌套 |
| `suite_timer` | 单次/周期、handler 内自停、restart、tick 回绕、延时投递、满队三策略（RETRY/DROP/LATEST）、**相位稳定无漂移**、**ms 边界运行时校验** |
| `suite_task` | 周期触发、停止、槽池（随配置伸缩）、ctx 透传、**相位无漂移**、**period_ms 边界校验** |
| `suite_mail` | 收发往返、满箱、空箱、截断、**满队整体回滚**（全有或全无） |
| `suite_interleave` | 见下节 |

当前断言规模（本地实测，2026-08-13）：

```
test_moteos     （默认配置）       4524 asserts, 0 failures
test_moteos_max（队列 255 最坏配置）7643 asserts, 0 failures
```

---

## 2. 交错测试（`tests/test_interleave.c`）

**设计**：单线程内用伪随机序列交错执行"主循环操作"与"伪中断操作"。
伪中断遵守建模硬件规则——临界区内不执行；同时模拟 tick 中断
（`mote_tick`）驱动一个周期定时器，真实驱动 `mote_process_timers` 路径。

**注入窗口**（`MOTE_TEST_INJECT_ENABLE`，发布构建零开销，见 `mote.h`）：

| 窗口 | 位置 | 覆盖的竞态 |
|---|---|---|
| `mote_event_post` / `post_replace` | 临界区前后 | 入队与队列操作的交错 |
| `mote_event_post_delayed` | 投递路径 | 延时槽池竞争 |
| `mote_mail_send` | 入临界区前 | **入箱+入队+回滚路径**的竞态 |
| `mote_poll` | 单步前 | 派发与投递的交错 |
| `mote_process_timers` | 定时器列表遍历中 | 定时器派发与投递的交错 |

**总账等式**（每次运行必须精确成立）：

```
显式投递尝试 + 定时器触发 = 最终派发数 + 丢弃计数
且：定时器触发数 > 50（证明 tick 真的驱动了定时器）
邮箱无滞留（末态 recv 返回 -1）
派发的普通事件数与邮箱出货量对得上
每步之后临界区不泄漏（mote_crit_active() == 0）
```

**种子**：默认固定种子；环境变量 `MOTE_TEST_SEED` 可换轨迹。
CI 跑 20 个种子（常规构建）+ 10 个（最坏配置构建）+ 5 个（ASan）；
本地验证跑过 1~50 全绿。

**局限声明（重要）**：交错测试验证的是内核对**测试自行定义的建模语义**
（"临界区内 ISR 不执行"是测试假设的硬件行为）的一致性。它能证明
"内核符合我假设的模型"，证明不了"内核符合真实硬件"——后者需要板级验证。

---

## 3. 断言开启构建（`test_moteos_assert`）

`mote_config.h` 里 `MOTE_ASSERT` 默认为 `((void)0)`——断言路径**从不参与编译**。
该目标强制 `-include tests/mote_assert.h`（在 `mote_config.h` 之前生效），
把 `MOTE_ASSERT` 替换为"打印位置 + abort"：

- 内核所有断言路径被真实编译进测试二进制并运行
- 测试全程触发任何断言 → 立即失败（CTest 报红）

当前全部测试在断言开启下不触发任何断言。

---

## 4. 最坏配置构建（`test_moteos_max`）

```
MOTE_EVT_QUEUE_SIZE = 255   （replace 全扫描的最长路径）
MOTE_DELAYED_MAX    = 16
MOTE_TASK_SLOT_MAX  = 16
```

跑全套单元测试 + 交错测试（多种子）。同时交叉编译 job 增加
`-DMOTE_EVT_QUEUE_SIZE=255` 的 M0+/RV32 构建，验证极端配置仍能编译。

---

## 5. QEMU 冒烟测试（`tests/qemu/cm3/`）

比交叉编译多一层：真的把内核**链接成固件并跑起来**。

- 机器：`stm32vldiscovery`（STM32F100，Cortex-M3，与 `port/cm3` 对应）
- 最小启动代码：`startup.c`（向量表含 SysTick 项）+ `link.ld`，无器件库依赖
- 验证链：Reset 进 main → SysTick 向量接到弱符号 `SysTick_Handler`
  → `mote_tick` → 500ms 周期定时器到期 → 事件投递 → handler 派发
- 判定：semihosting 打印 `QEMU_PASS` / `QEMU_FAIL: timer/event flow broken`，
  随后 SYS_EXIT；向量表/SysTick 断掉则死循环 → CI `timeout 60s` 判失败

**实现中踩过的坑（记录在案）**：

1. semihosting 内联汇编的入参不能放 r0（会被 syscall 号覆盖）——
   用 `register ... __asm__("r1")` 钉住寄存器
2. QEMU 的 semihosting 退出码在各平台透传不一致（本地 Windows 版任何
   退出码都返回 1，实测 `exit(42)` 也返回 1）——**因此判定不依赖退出码**，
   只看 stdout 关键字

**非板级验证**：外设时序、临界区实测时长不在此列。

---

## 6. 覆盖率（gcovr，CI 门槛 85%）

本地实测（`--coverage -O0`，三个测试目标全跑）：

```
moteos/mote.c              226 行  207 覆盖   91%
moteos/mote_mail.c          52 行   48 覆盖   92%
moteos/mote_task.c          41 行   39 覆盖   95%
moteos/port/host/mote_port.h 9 行    9 覆盖  100%
moteos/port/mote_port.c      2 行    0 覆盖    0%   ← 仅目标机分支（SysTick/wfi），QEMU 冒烟覆盖
TOTAL                      330 行  303 覆盖   91.8%
lines 91.8% | functions 91.4% | branches 80.2%
```

`moteos/port/mote_port.c` 的 0% 是 `#else`（目标机）分支——宿主机不编译它，
实际由 QEMU 冒烟测试在 Cortex-M3 上覆盖。CI 以 `--fail-under-line 85` 守护，
覆盖回归会直接判红。

---

## 7. 静态分析（cppcheck）

```
cppcheck --enable=warning,performance,portability --std=c99 \
         --inline-suppr --error-exitcode=1 -Imoteos -Imoteos/port/host -Itests moteos
```

本地 2.21.0 实测：5 文件 0 告警（exit=0）。CI 上任何 error 即失败。

---

## 8. CI 工作流（`.github/workflows/build.yml`）

| job | 内容 |
|---|---|
| host-tests | `-Werror` 构建 + ctest 三目标 + 20/10 种子交错 |
| sanitizers | ASan/UBSan 构建 + ctest + 5 种子 |
| cross-compile | M0+(-Os+体积断言)/M3/RV32(-Os+体积断言) + 最坏配置构建 + STM32F103/CH32V003 真实 SDK 例程编译（钉 SDK 提交） |
| qemu-smoke | 见第 5 节 |
| coverage | gcovr，行覆盖 <85% 判红 |
| cppcheck | error 即失败 |

---

## 9. 本地运行

```bash
# 全部测试（常规 + 断言开启 + 最坏配置三目标）
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# 交错测试多种子
MOTE_TEST_SEED=1 ./build/test_moteos   # Linux/macOS
$env:MOTE_TEST_SEED=1; ./build/test_moteos.exe   # PowerShell

# 覆盖率
cmake -S . -B build-cov -DCMAKE_C_FLAGS="-Werror --coverage -O0"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --exclude tests --exclude 'build.*' --print-summary

# QEMU 冒烟（需 arm-none-eabi-gcc 与 qemu-system-arm）
arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Os -mcpu=cortex-m3 -mthumb \
  -ffreestanding -nostdlib -T tests/qemu/cm3/link.ld -o qemu.elf \
  tests/qemu/cm3/main.c tests/qemu/cm3/startup.c \
  moteos/mote.c moteos/mote_task.c moteos/mote_mail.c moteos/port/mote_port.c \
  -Imoteos -Imoteos/port/cm3
qemu-system-arm -M stm32vldiscovery -cpu cortex-m3 -nographic -monitor none \
  -semihosting-config enable=on,target=native -kernel qemu.elf
# 期望输出 QEMU_PASS

# 静态分析
cppcheck --enable=warning,performance,portability --std=c99 \
  --inline-suppr --error-exitcode=1 -Imoteos -Imoteos/port/host -Itests moteos
```

---

## 10. 已知局限（诚实声明）

1. **无板级验证**：内核尚未在任何真实芯片上运行。中断时序、临界区实测
   时长、WFI 低功耗唤醒（含青稞 INTSYSCR 与 WFI 的交互）、周期相位漂移，
   均无实测数据——板级验证清单见 `docs/porting.md` 第 7 章
2. 交错测试验证的是建模并发语义，不是真实硬件时序（见第 2 节局限声明）
3. QEMU 冒烟只覆盖 Cortex-M3 一条启动链；M0+/RV32 仅有交叉编译
4. 覆盖率门槛只防大面积盲区，100% 覆盖不等于无 bug
5. 断言构建只证明"断言路径能编译、常规路径不触发断言"，
   未做"故意触发断言"的负向用例（会 abort 整进程，需进程级测试才可做）

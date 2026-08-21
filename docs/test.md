# MoteOS 测试文档

> 本文汇总 MoteOS 的全部验证手段、**实测结果明细**与结论。
> 测试代码在 `tests/`，CI 定义在 `.github/workflows/build.yml`。
> 自动化测试数字来自 2026-08-13 的实际运行；CH32V003 上板验证记录来自
> 2026-08-21 的 MounRiver/WCH-Link 实测输出。

---

## 测试矩阵

| # | 层 | 手段 | 验证什么 | CI job |
|---|---|---|---|---|
| 1 | 逻辑 | 宿主机单元测试 | 事件队列/定时器/任务/邮箱的 API 语义 | host-tests |
| 2 | 并发 | 交错测试（多种子） | 对"建模并发语义"的一致性（见局限声明） | host-tests / sanitizers |
| 3 | 防御 | 断言开启构建 | `MOTE_ASSERT` 路径被真实编译并运行 | host-tests（ctest 内置） |
| 4 | 边界 | 最坏配置构建（队列 255 等） | 极端配置下的正确性 | host-tests + cross-compile |
| 5 | 内存 | ASan/UBSan | 内存错误、未定义行为 | sanitizers |
| 6 | 真实启动 | QEMU 冒烟（Cortex-M3，固定拍 + tickless 双档） | 启动/向量表/SysTick/tick→定时器→事件流；tickless：入账追平/nap 重装/固定拍恢复/漂移检测（时间膨胀执行） | qemu-smoke |
| 7 | 可编译性 | 交叉编译 + 体积断言 | M0+/M3/RV32 可编译、内核三件套 + 移植层分账体积不失控 | cross-compile |
| 8 | 集成 | 真实 SDK 例程编译 | CMSIS/WCH SDK 真实头文件下例程可编译（CI 全局启用 tickless） | cross-compile |
| 9 | 可编译性 | tickless 交叉编译（`MOTE_TICKLESS=1`） | 三种架构的 SysTick 重装/追平代码可编译 | cross-compile |
| 10 | 覆盖 | gcovr 行覆盖 ≥85% 门槛 | 测试没有大面积盲区 | coverage |
| 11 | 静态 | cppcheck | warning/performance/portability | cppcheck |
| 12 | 板级 | CH32V003 Stage1-16 | 固定拍、队列/定时器/邮箱/任务 API、drop hook、`mote_loop`、tickless 入账、WFI 唤醒、综合长跑、低功耗测量例程 | 手动上板 |

---

## 一、各测试手段说明

### 1. 宿主机单元测试（`tests/test_*.c`）

内核是纯逻辑，全部在 PC 上跑（`moteos/port/host` 用共享变量模拟中断开关，
临界区是否破坏调用方中断状态可在宿主机直接断言）。

| 套件 | 覆盖点 |
|---|---|
| `suite_queue` | post/派发、满队报错、replace 覆盖、越界 ID 安全丢弃、空 handler、丢弃计数、drop hook（含重入）、临界区嵌套 |
| `suite_timer` | 单次/周期、handler 内自停、restart、tick 回绕、延时投递（含 replace/cancel）、满队三策略（RETRY/DROP/LATEST）、**相位稳定无漂移**、**ms 边界运行时校验**、**policy 越界运行时校验**、**排序链表触发顺序/重排**、**`mote_next_due` deadline 计算**、**`mote_sleep` 睡眠判定（宿主机 idle 观测）** |
| `suite_task` | 周期触发、停止、槽池（随配置伸缩）、ctx 透传、**相位无漂移**、**period_ms 边界校验** |
| `suite_mail` | 收发往返、满箱、空箱、超长拒绝、**满队整体失败不滞留**（先入队后入箱，全有或全无）、**钩子重入同一邮箱**、**非法构造（slots==0/空指针/item_size 越界）运行时拒绝**、**槽长度域写坏（0 / >item_size）运行时拒绝** |
| `suite_interleave` | 见下节 |

### 2. 交错测试（`tests/test_interleave.c`）

**设计**：单线程内用伪随机序列交错执行"主循环操作"与"伪中断操作"。
伪中断遵守建模硬件规则——临界区内不执行；同时模拟 tick 中断
（`mote_tick`）驱动一个周期定时器，真实驱动 `mote_process_timers` 路径。

**注入窗口**（`MOTE_TEST_INJECT_ENABLE`，发布构建零开销，见 `mote.h`）：

| 窗口 | 位置 | 覆盖的竞态 |
|---|---|---|
| `mote_event_post` / `post_replace` | 临界区前后 | 入队与队列操作的交错 |
| `mote_event_post_delayed` | 投递路径 | 延时槽池竞争 |
| `mote_mail_send` | 入临界区前 | **先入队后入箱**顺序与失败原子性的竞态 |
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

**局限声明（重要）**：交错测试验证的是内核对**测试自行定义的建模语义**
（"临界区内 ISR 不执行"是测试假设的硬件行为）的一致性。它能证明
"内核符合我假设的模型"，证明不了"内核符合真实硬件"——后者需要板级验证。

### 3. 断言开启构建（`test_moteos_assert`）

`mote_config.h` 里 `MOTE_ASSERT` 默认开启——回调弱符号 `mote_assert_fail`
（`mote_port.c` 的宿主机实现为 abort，芯片实现为停机死循环）。该目标用
`-include tests/mote_assert.h` 强制在 `mote_config.h` **之前**生效，
把 `MOTE_ASSERT` 替换为"打印文件行号 + abort"版本，让失败现场一眼可读：

- 内核所有断言路径被真实编译进测试二进制并运行
- 测试全程触发任何断言 → 立即失败（CTest 报红）

### 4. 最坏配置构建（`test_moteos_max`）

```
MOTE_EVT_QUEUE_SIZE = 255   （replace 全扫描的最长路径）
MOTE_DELAYED_MAX    = 16
MOTE_TASK_SLOT_MAX  = 16
```

跑全套单元测试 + 交错测试（多种子）。同时交叉编译 job 增加
`-DMOTE_EVT_QUEUE_SIZE=255` 的 M0+/RV32 构建，验证极端配置仍能编译。

### 5. QEMU 冒烟测试（`tests/qemu/cm3/`）

比交叉编译多一层：真的把内核**链接成固件并跑起来**。

- 机器：`stm32vldiscovery`（STM32F100，Cortex-M3，与 `port/cm3` 对应）
- 最小启动代码：`startup.c`（向量表含 SysTick 项）+ `link.ld`，无器件库依赖
- 固定拍档（`main.c`）：验证链 Reset 进 main → SysTick 向量接到弱符号
  `SysTick_Handler` → `mote_tick` → 500ms 周期定时器到期 → 事件投递 →
  handler 派发
- **tickless 档（`main_tickless.c`，`-DMOTE_TICKLESS=1 -DMOTE_PORT_HCLK_HZ=24000u`）**：
  覆盖固定拍不编译的整个 tickless 空闲分支——
  1000ms 周期定时器，`mote_sleep`→`mote_idle` 的时基追平
  （COUNTFLAG 读清零、重装值差额、周期余数）、nap 钳制与唤醒后 ISR
  恢复固定拍。**漂移检测**：第 3 拍落在 tick [3000, 4000]——下界 3000
  抓"入账跑快"（双重入账/重复计整拍会让定时器提前触发）；上界 4000
  是给 QEMU 模型行为留的余量（SysTick ptimer 在 wrap 后"计数器 0 等待
  重载"的状态会被单个 TCG 批拉长，批执行期间事件不处理、真实时间不
  入账，滞后随宿主负载波动，实测数十 ms 量级——属仿真模型行为，非
  内核缺陷；真实芯片按 3000±20 校核即可）；另断言 `mote_dropped_count()==0`
  - **时间膨胀**：QEMU 的 WFI 模型在"重装 SysTick 后立即 wfi"时产生
    伪唤醒（ptimer 重载事件唤醒被 halt 的 vCPU），深睡退化为微秒级
    轮询；按真实 24MHz 换算，3000 拍需执行约 720 亿条宿主指令，CI
    时限内跑不完。故把换算常量 HCLK 设为 24kHz——1 个 tick-ms = 24 个
    计数器周期，入账数学与换算率无关，全部路径照常执行且漂移检测
    精度不受影响（计数器周期精确）。代价：深睡（wfi 长保持）与 wrap
    标志路径在 QEMU 下无法验证，仍需板级实测
  - 本档曾实抓出旧入账协议的真实 bug（回读计数器做锚点 → 0 窗口竞态
    → 无符号下溢 → 时基一次跳变约 179 秒），详情见 CHANGELOG
- 判定：semihosting 打印 `QEMU_PASS` / `QEMU_FAIL`，随后 SYS_EXIT；
  向量表/SysTick 断掉则死循环 → CI `timeout 60s` 判失败

**实现中踩过的坑（记录在案）**：

1. semihosting 内联汇编的入参不能放 r0（会被 syscall 号覆盖）——
   用 `register ... __asm__("r1")` 钉住寄存器
2. QEMU 的 semihosting 退出码在各平台透传不一致（本地 Windows 版任何
   退出码都返回 1，实测 `exit(42)` 也返回 1）——**因此判定不依赖退出码**，
   只看 stdout 关键字

**非板级验证**：外设时序、临界区实测时长不在此列。

### 6. 覆盖率（gcovr）

`--coverage -O0` 构建三个测试目标全跑后统计；CI 以 `--fail-under-line 85`
守护，覆盖回归直接判红。

### 7. 静态分析（cppcheck）

```
cppcheck --enable=warning,performance,portability --std=c99 \
         --inline-suppr --error-exitcode=1 -Imoteos -Imoteos/port/host -Itests moteos
```

任何 error 即失败。

---

## 二、测试结果明细（2026-08-13 实测）

### 2.1 宿主机单元测试

| 构建 | 断言数 | 失败 | 结果 |
|---|---|---|---|
| `test_moteos`（默认配置） | 4843 | 0 | ALL PASSED |
| `test_moteos_assert`（断言开启） | 4843 | 0 | ALL PASSED（全程未触发任何内核断言） |
| `test_moteos_max`（队列 255 等最坏配置） | 8452 | 0 | ALL PASSED |

### 2.2 交错测试多种子

| 构建 | 种子范围 | 结果 |
|---|---|---|
| 常规 | 1~20 | 20/20 通过 |
| 最坏配置 | 1~10 | 10/10 通过 |
| CI 常规 job | 1~20 | 通过（自动化） |
| CI 最坏配置 job | 1~10 | 通过（自动化） |
| CI ASan job | 1~5 | 通过（自动化，Ubuntu） |

> 本地 MinGW 工具链不带 libasan/libubsan 运行时，ASan/UBSan 仅由 CI
> （Ubuntu）执行——这是本地环境限制，不是测试缺口。

### 2.3 覆盖率（gcovr，`--coverage -O0`，三目标全跑）

```
moteos/mote.c               308 行  288 覆盖   93%   （未覆盖：MOTE_DELAYED_MAX=0 分支、
                                                        tick 回绕保护路径等）
moteos/mote_mail.c           67 行   64 覆盖   95%   （未覆盖：非对齐拷贝头/结构写坏分支）
moteos/mote_task.c           41 行   39 覆盖   95%
moteos/port/host/mote_port.h  9 行    9 覆盖  100%
moteos/port/mote_port.c       6 行    4 覆盖   66%   ← 仅宿主分支参与 gcovr；目标机分支
                                                      （SysTick/tickless/wfi）由交叉编译
                                                      + QEMU 冒烟覆盖（tickless 档实际
                                                      执行 tickless 空闲路径）
TOTAL                       431 行  404 覆盖   93.7%
```

```
lines:     93.7% (404/431)   ← CI 门槛 85%
functions: 95.6% (43/45)
branches:  84.2% (224/266)
```

> gcovr 8.x 对多目标合并更严格（断言构建的 -include 会平移行号），
> CI 命令已加 `--merge-mode-functions=separate` 避免误判。

### 2.4 静态分析（cppcheck）

```
Checking moteos/mote.c ... （含 MOTE_DELAYED_MAX=1 等配置变体）
Checking moteos/mote_mail.c ...
Checking moteos/mote_task.c ...
Checking moteos/port/mote_port.c ...
Checking moteos/port/mote_port_template.c ...
5/5 files checked 100% done
0 告警，exit=0
```

### 2.5 QEMU 冒烟（Cortex-M3，stm32vldiscovery）

```
固定拍：   QEMU_PASS
tickless： QEMU_PASS
```

判定通过：Reset 正常进入 main；SysTick 中断向量正确命中弱符号
`SysTick_Handler`；tick 驱动 500ms 周期定时器到期 2 次；事件投递与
handler 派发链路完整。tickless 档另验证（时间膨胀，1 tick-ms = 24
计数器周期）：1000ms 周期定时器经分段 nap 3 拍后 ticks 落在
[3000, 4000]（下界抓入账跑快，上界为 QEMU ptimer 重载滞后的仿真
余量；无漏记/重复入账类漂移），丢事件计数为 0。整个过程 semihosting
无异常。

### 2.6 交叉编译与体积（arm-none-eabi-gcc，`-Wall -Wextra -Werror`）

| 目标 | 配置 | text | data+bss | 断言 |
|---|---|---|---|---|
| Cortex-M0+ | 默认，`-Os` | 内核三件套 2297 B + port.o 14 B | 280 B | CI：内核 text <2560、port text <512、RAM <512 ✅ |
| Cortex-M0+ | 队列 255 / 延时 16，`-Os` | 仅记录 | 2384 B | 仅编译（最坏配置体积仅记录） |
| Cortex-M3 | 默认，无 `-Os` | 仅记录 | 280 B | 仅编译 ✅ |
| RV32IMC（青稞） | 默认，`-Os` | 内核 text <2816（本机无 RV32 工具链，数字以 CI 为准） | 280 B | CI：内核 text <2816、port text <512、RAM <512 ✅ |
| Cortex-M0+/M3/RV32 | `MOTE_TICKLESS=1`（port.o 增量） | +320~360 B（仅 port 层） | +12 B | 编译 + QEMU tickless 冒烟（M3） ✅ |

> 体积断言为**分账口径**：内核三件套（mote.o/mote_task.o/mote_mail.o）
> 与移植层 mote_port.o（SysTick/临界区/idle）分别设限，移植层不再游离
> 在断言之外。M0+ 数字为本机实测（评审整改后三件套 2297 B，含 RETRY
> raw 入队等新增路径；**体积随工具链版本小幅浮动——gcc 14.3.1 实测
> 2343 B，仍在 CI 阈值 <2560 内，以你的 map 文件与 CI 断言为准**）。
> 队列 255 配置的 RAM 2.3KB 是用户把队列开到
> 极限的代价——内核本身不失控，但 `mote_event_post_replace` 的临界区
> 时长也随队列长度线性增长（见 usage.md 附录 A 延迟预算）。
> tickless 的体积增量只在 `mote_port.o`，不占内核三件套的预算。

### 2.7 集成编译（CI）

- STM32F103 例程 + CMSIS_5 / cmsis-device-f1 真实头文件（钉提交）✅
- CH32V003 例程 + WCH 官方 SDK 真实头文件（钉提交）✅
- 以上两步均带 `-DMOTE_TICKLESS=1 -DMOTE_PORT_HCLK_HZ=...`（示例源码默认固定拍，CI 通过命令行全局启用 tickless）

### 2.8 CH32V003 上板验证（MounRiver + WCH EVT SDK）

硬件环境：CH32V003，HCLK=48MHz，LED 使用 PC0（外部上拉，低电平点亮），
USART1 TX=PD5，115200。工程基于 WCH 官方裸机 EVT 例程迁移，下载目标和
map 文件均按对应 Stage 工程核对。

| 阶段 | 覆盖内容 | 实测结论 |
|---|---|---|
| Stage1 | 固定拍 SysTick、串口、PC0 LED | 串口持续输出，LED 500ms 闪烁 |
| Stage2 | 多定时器负载、printf 路径 | 10 分钟 `drop=0` |
| Stage3 | 队列压力、满队丢弃计数 | 16 槽队列按预期接收 16、丢弃 16 |
| Stage4 | `post_replace` latest-wins | replace 全成功，full 策略按预期失败，`err=0` |
| Stage5 | delayed post/replace/cancel | 每周期计数符合预期，`drop=0 err=0` |
| Stage6 | mailbox API 压力 | ok/full/param/msg/drop 均符合预期，校验和一致 |
| Stage7 | task API | fast/mid/slow 与 tick 推导值一致，`api_err=0 drop=0 err=0` |
| Stage8 | timer control API | start/stop/restart/once/periodic 控制路径通过 |
| Stage9 | drop hook 与非法事件 | drop hook 与非法 ID 路径通过 |
| Stage10 | `mote_loop` / `mote_sleep` / `mote_next_due` | `next`/`due_bad` 符合预期，LED 与 heartbeat 正常 |
| Stage11 | tickless 入账（无 WFI） | SysTick 重装/追平路径通过 |
| Stage12 | 综合 tickless 长跑（无 WFI） | 修正测试期望的 `uint8_t` 校验和回绕后，长跑 `err=0` |
| Stage13 | WFI 唤醒（IRQ enabled） | raw wfi 与 WCH `__WFI()` 均可被 SysTick 唤醒，`irq_bad=0 err=0` |
| Stage14 | WCH `__WFI()` 在关中断入口下的行为 | `__WFI()` 可睡眠并唤醒，返回后中断开启，`ret_dis=0 irq_bad=0 err=0` |
| Stage15 | 综合 tickless + WCH `__WFI()` 长跑 | 跑到 671000 ticks，所有 a/b 字段相等，`bad=0/0/0 api_err=0 drop=0 err=0` |
| Stage16 | 低功耗测量专用例程 | 启动只打印一次，随后关闭 USART；默认 2s 周期 PC1/PC0 短脉冲；`SysTick_Handler` 可由用户工程强符号接管并转调 `mote_port_systick_handler()` |

Stage15 构建体积：FLASH 9628 B / 16 KB（58.76%），RAM 880 B / 2 KB（42.97%）。
这说明综合压测工程能装进 CH32V003，但 2KB RAM 余量有限，产品代码仍需看
map 文件和栈深。

Stage16 构建体积：FLASH 3212 B / 16 KB（19.60%），RAM 396 B / 2 KB
（19.34%）。该工程只保留一个 2s 周期事件，启动 banner 后关闭 USART，
用于把功能压测噪声从低功耗电流测量中剥离。

Stage16 板级电流实测（非芯片裸片电流）：

| 采样电阻 | 波形电压 | 换算电流 | 现象 |
|---|---:|---:|---|
| 10Ω | 93.6mV | 9.36mA | 低平台 |
| 10Ω | 105.6mV | 10.56mA | PC0 LED 亮平台 |
| 10Ω | 123.2mV | 12.32mA | 唤醒/活动尖峰 |
| 5Ω（两个 10Ω 并联） | 47.2mV | 9.44mA | 低平台 |
| 5Ω（两个 10Ω 并联） | 53.0mV | 10.60mA | PC0 LED 亮平台 |
| 5Ω（两个 10Ω 并联） | 61.6mV | 12.32mA | 唤醒/活动尖峰 |

两组 shunt 互相校验：低平台约 9.4mA，LED 亮时增加约 1.2mA，唤醒/活动段
约 12.3mA。该数值包含 LED、GPIO 标记、板载稳压器、调试器/外设连接等影响，
不能外推为 CH32V003 芯片级睡眠电流。

---

## 三、结论

### 3.1 每条宣称的支撑证据对照

| 宣称 | 证据 | 强度 |
|---|---|---|
| API 语义正确（队列/定时器/任务/邮箱） | 4843~8452 条断言全绿 | 强 |
| 周期定时器/任务无相位漂移 | 漂移回归测试（含迟到触发场景） | 强（逻辑层面） |
| ms/period/policy 边界运行时校验生效 | test_ms_bound / test_task_ms_bound / test_policy_invalid | 强 |
| deadline 计算与睡眠判定（next_due/sleep） | test_next_due / test_sleep_deadline | 强（逻辑层面） |
| 并发账目一致性（投递=派发+丢弃，邮箱无滞留，临界区不泄漏） | 交错测试多种子全绿 | 强（**对建模语义**） |
| 断言路径真实可编译、常规路径不触发断言 | test_moteos_assert 全绿 | 中（无负向触发用例，见局限 5） |
| 内存安全/无 UB | ASan/UBSan（CI，Ubuntu） | 中（CI 环境，非板级） |
| 启动链正确（向量表/SysTick/tick→定时器→事件流） | QEMU 冒烟（Cortex-M3，固定拍） | 中（模拟器，非真硅片） |
| tickless 空闲路径（长拍重装/时基追平/无漂移） | QEMU tickless 冒烟（Cortex-M3） | 中（模拟器，非真硅片） |
| CH32V003 固定拍与 API 上板行为 | Stage1-10 串口/LED/计数器实测 | 强（CH32V003 实板，手动） |
| CH32V003 tickless/WFI 唤醒行为 | Stage11-16，含关中断入口 `__WFI()`、综合长跑与低功耗专用例程 | 强（CH32V003 实板，手动） |
| 三内核可编译、体积分账受控（三件套+移植层） | 交叉编译 + 分账体积断言（含 tickless 档） | 强 |
| 覆盖无大面积盲区 | 行覆盖 93.7%（门槛 85%） | 中 |
| 无静态分析告警 | cppcheck 0 告警 | 中 |

> 注意：板级工程构建已实际抓到过 CI 漏掉的 bug（ch32v 临界区恢复的
> 汇编操作数编号错误，见 CHANGELOG v0.5.0）——交叉编译/模拟器绿 ≠
> 真机正确，本表"强"仅指逻辑层。

### 3.2 不能下的结论（诚实边界）

1. **中断延迟数字**：usage.md 附录 A 的微秒级估算**没有任何实测数据背书**，
   必须按板级检查清单用 DWT/示波器实测
2. **芯片级低功耗电流数字**：CH32V003 已验证 WCH `__WFI()` 可睡眠并被
   SysTick 唤醒，也已有 Stage16 板级电流记录；但这个数字包含 LED、串口、
   稳压器、调试器和外部连接，不能当作芯片裸片睡眠电流
3. **其他芯片的 WFI 行为**：CH32V003 以外的 WCH/ARM 目标仍需独立上板验证，
   不能把 CH32V003 的结论外推成全系列保证
4. **真实硬件时序**：临界区时长、tick 抖动、相位漂移的**实际值**，
   宿主机测试与 QEMU 都测不出来
5. **M0+/其他 RV32 的启动链**：QEMU 冒烟只覆盖 Cortex-M3 一条链，CH32V003
   覆盖一条 WCH RV32 链；其余芯片仍需实板
6. **断言负向路径**：没做过"故意触发断言"的测试（需进程级用例）

### 3.3 总体结论

- **内核逻辑是可信的**：API 语义、并发一致性（建模语义下）、边界校验、
  内存安全（CI）、启动链（M3 模拟器）、可编译性、体积、覆盖率，均有
  自动化证据支撑——对一个 v0.x 开发预览项目，这已经是相当厚的验证层
- **CH32V003 功能路径已上板闭环**：固定拍、核心 API、tickless 入账、WCH
  `__WFI()` 唤醒、综合长跑和低功耗测量例程已通过；但中断延迟、芯片级电流、
  抖动这些量化指标仍需按最终目标板继续测
- **其他芯片仍不能直接背书**：CH32V003 的结论不能自动外推到 CH32V007/V203/V307
  或 ARM 目标。生产决策必须基于目标芯片、目标板、目标时钟和目标外设连接的实测
- 定位与 README「项目状态」一致：值得作为**学习项目与内核逻辑参考**使用；
  "生产就绪"不成立，上板前必须按 `docs/porting.md` 第 7 章检查清单完成验证

---

## 四、CI 工作流（`.github/workflows/build.yml`）

| job | 内容 |
|---|---|
| host-tests | `-Werror` 构建 + ctest 三目标 + 20/10 种子交错 |
| sanitizers | ASan/UBSan 构建 + ctest + 5 种子 |
| cross-compile | M0+(-Os+分账体积断言)/M3/RV32(-Os+分账体积断言) + 最坏配置构建 + tickless 档（三架构 `MOTE_TICKLESS=1`）+ STM32F103/CH32V003 真实 SDK 例程编译（钉 SDK 提交，带 tickless 宏） |
| qemu-smoke | 固定拍 + tickless 双档冒烟（见第一节第 5 条） |
| coverage | gcovr，行覆盖 <85% 判红 |
| cppcheck | error 即失败 |

---

## 五、本地运行

```bash
# 全部测试（常规 + 断言开启 + 最坏配置三目标）
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# 交错测试多种子
MOTE_TEST_SEED=1 ./build/test_moteos          # Linux/macOS
$env:MOTE_TEST_SEED=1; ./build/test_moteos.exe # PowerShell

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

# QEMU tickless 冒烟（加 -DMOTE_TICKLESS=1 -DMOTE_PORT_HCLK_HZ=24000u，
# 用 main_tickless.c；期望输出 QEMU_PASS。24000 是时间膨胀值，见第一节
# 第 5 条；真实主频下 CI 时限内跑不完）

# 静态分析
cppcheck --enable=warning,performance,portability --std=c99 \
  --inline-suppr --error-exitcode=1 -Imoteos -Imoteos/port/host -Itests moteos
```

# 更新日志

## v0.4.0 - 2026-08-13（开发预览）

> **版本回退说明**：v1.0.0/v1.0.1 标签已撤销——"生产就绪"宣称撤回：
> 本内核从未在任何真实芯片上运行，无板级实测数据。回到 v0.x 开发预览，
> 以 README「项目状态」为准。原 v1.0.x 的修复内容全部保留（并入本版）。

### 修复

- **周期定时器相位漂移**（P0）：到期推进由 `due = now + period` 改为
  `due += period`（绝对相位 + 错过拍合并追赶）；落后超过
  `MOTE_TIMER_CATCHUP_MAX`（默认 1000 拍）放弃旧相位重新对齐。
  任务层同修。handler/主循环延迟不再逐周期累积漂移
- **s_tick 访问统一走临界区**（P0）：`mote_tick` 自带临界区；
  `mote_timer_start_ex`/`mote_timer_restart` 的时基读取包临界区
  （此前 M0+ 上存在 32 位撕裂风险）；`mote_process_timers` 改用
  `mote_ticks()` 单一快照，消除"检查与重排两次裸读"
- **ms 时长上限改为运行时校验**（P0）：`mote_timer_start*`/
  `mote_timer_restart`/`mote_event_post_delayed` 对 `ms ≥ 2^31` 返回
  `MOTE_ERR_PARAM`。此前仅靠默认关闭的 `MOTE_ASSERT`，生产构建静默失效
- 移除 `mote_poll` 中与"未注册事件安全丢弃"设计相矛盾的断言
  （该路径是受支持的运行时行为，非内部不变量；断言开启构建下
  恶意/越界事件也必须走到丢弃路径）

### CI / 测试

- **断言开启构建**（P1）：新目标 `test_moteos_assert` 强制
  `-include tests/mote_assert.h`，内核断言路径首次被真实编译并运行
- **最坏配置构建**（P1）：新目标 `test_moteos_max`（队列 255 / 延时槽 16 /
  任务槽 16）宿主机跑全套测试 + 多种子交错；交叉编译 job 增加
  M0+/RV32 的 `-DMOTE_EVT_QUEUE_SIZE=255` 构建
- 新增回归测试：定时器/任务层相位漂移、ms 上限运行时校验
- 修正 `test_task_slot_pool` 对槽数配置的硬编码假设

### 文档

- README（中英）：新增「项目状态」——明示 v0.x 开发预览、未经板级验证，
  撤销"生产就绪"与"微秒级临界区"等未经实测的宣称；资源占用表区分
  CI 断言与手动实测，并提示 712B 占 2KB RAM 的 35%
- 使用教程：新增「附录 A：中断延迟预算」（临界区时长估算公式 +
  DWT CYCCNT / GPIO 示波器两套实测方法，明确标注为估算值）；
  新增 3.6 周期相位稳定语义；任务层文档明确"不是抢占式 RTOS 任务"
- 移植教程：检查清单新增中断延迟/低功耗唤醒/周期相位三项板级验证项；
  注明青稞 WFI 与 INTSYSCR 的交互需板级确认；修正 Q10 的"实测经验值"表述
- 交错测试定位澄清：验证内核对建模并发语义（临界区内不抢占）的一致性，
  不构成硬件验证

### 保留自原 v1.0.x 的修复

- Cortex-M 移植层去 CMSIS 依赖（裸汇编临界区，兼容 GCC/ArmClang/armcc）；
  STM32F103 例程改用 CMSIS 器件头，CI 双仓库钉版本编译
- 配置边界编译期 `#error` 块；WFI 竞态处理与 `mote_idle` 契约成文；
  `mote_init` 契约成文

## ~~v1.0.1~~ - 2026-08-13（已撤销，内容并入 v0.4.0）

### 修复

- Cortex-M 移植层（cm0plus/cm3）不再依赖 CMSIS 头文件：临界区改为裸内联汇编
  （`MRS PRIMASK` / `CPSID i` / `MSR`，兼容 GCC / ArmClang / armcc）。
  修复内核在真实工程中因 `core_cm3.h` 依赖器件头定义 `IRQn_Type`
  而无法独立编译的问题，包含顺序不再有任何要求
- STM32F103 例程改用 CMSIS 器件头 `stm32f1xx.h`（补器件选择宏），
  引脚宏改为位字面量，兼容 SPL 与 Cube 两套头文件
- CI：STM32F103 例程真实头文件编译改为双仓库钉版本
  （CMSIS_5 的 `CMSIS/Core/Include` + STMicroelectronics/cmsis-device-f1）

## ~~v1.0.0~~ - 2026-08-13（已撤销，内容并入 v0.4.0）

> 原"首个生产就绪版本"宣称已撤回：无板级验证支撑。修复内容本身有效，
> 详见 v0.4.0「保留自原 v1.0.x 的修复」。

### 本版修复（最终审查收尾）

- **P1 配置边界校验**：`mote_config.h` 增加编译期 `#error` 块（`MOTE_TICK_MS` 1~1000、
  `MOTE_EVT_QUEUE_SIZE` 1~255、`MOTE_TASK_SLOT_MAX` 1~255），杜绝 uint8_t 计数回绕/
  除零/死循环类静默内存损坏；`MOTE_MAILBOX_DEF` 增加槽数与格大小的编译期检查
- **P2 WFI 竞态**：睡眠改为"关中断 → 查空 → wfi → 恢复中断"的临界区模式，
  消除"查空与睡眠之间投递事件导致漏睡"的竞态；`mote_idle()` 契约成文
  （关中断上下文、必须 wfi 级、深度睡眠不支持）
- **P2 时长边界**：定时器/延时投递增加 `ms < 2^31`（约 24.8 天）断言与文档说明
- **P2 mote_tick 契约成文**：仅允许单一 tick 中断源调用
- **P2 任务周期校验**：`period_ms = 0` 增加断言（避免退化为每 poll 触发）
- **P3 mote_init 契约成文**：仅启动时调用一次，不重置钩子与注入点
- **P3 STM32F103 例程进入 CI**：真实 CMSIS 头文件（钉 ARM-software/CMSIS_5 提交）编译

## v0.3.3 - 2026-08-13

### 修复

- 邮箱拷贝写侧改为字节存储：消除严格别名 UB（此前 `*(uint32_t *)d = w` 强转写入，字面上违反 C 标准）
- `MOTE_TEST_INJECT` 宏改为 do-while 形式，`-pedantic` 兼容
- CI：WCH SDK 改为单次 fetch（git init + fetch SHA），省一半网络时间

### 其他

- README（中英）新增 tag 版本徽章

## v0.3.2 - 2026-08-13

### 修复与加固

- `mote_event_post_delayed` 禁用分支的丢弃计数包临界区（契约统一：note_dropped 调用方持临界区）
- 邮箱拷贝由字节循环升级为对齐感知的 32 位字拷贝（头尾字节处理非对齐），临界区内延迟常数因子显著下降
- `mote_timer_start_ex` 增加 policy 范围断言
- drop hook 文档收紧：仅允许事件/邮箱类 API，禁止定时器/任务 API
- CI：WCH SDK 钉 commit（2ac6803）；宿主机与 ASan 任务各跑 1+20 / 1+5 个随机种子

### 测试

- 交错测试支持内核内部注入点（`MOTE_TEST_INJECT_ENABLE`，发布构建零开销）与多种子轨迹

## v0.3.1 - 2026-08-13

### 修复

- CI RISC-V 工具链问题：新版 GCC 需显式 `zicsr` 扩展（`-march=rv32imc_zicsr`）
- 内核移除 libc 依赖（邮箱拷贝改为内置字节循环），裸工具链/无 newlib 环境可编译
- CI 改用 xpack riscv-none-elf-gcc（自带 newlib），支持真实 SDK 例程编译

## v0.3.0 - 2026-08-13

### 新增

- 定时器满队策略：`mote_timer_start_ex` + `MOTE_TIMER_POLICY_RETRY / DROP / LATEST`
  （至少一次送达 / 严格截止 / replace 语义只留最新）
  注：`mote_timer_start` 默认策略为单次=RETRY、周期=DROP（与旧行为等价，兼容）
- 交错测试骨架：随机交错主循环与伪中断操作，验证并发一致性（投递账目、邮箱无滞留、临界区不泄漏）
- CI：真实 WCH SDK（openwch/ch32v003）编译 CH32V003 例程；体积断言收紧为 text <2.5KB / RAM <512B（-Os 实测）
- drop hook 防重入保护

### 修复

- `mote_event_post_delayed` 在 `MOTE_DELAYED_MAX==0` 时漏记丢弃（口径统一）

### 文档

- 使用教程：定时器三种策略与选型速查表
- README：资源占用改为实测数据（内核 RV32EC 2.0KB / M0+ 1.2KB / RAM 280B；完整点灯工程 2.7KB/712B）

## v0.2.0 - 2026-08-13

### 破坏性变更

- `MOTE_TASK_DEF` 由 2 参数改为 3 参数（新增 `ctx`），`mote_task_desc_t` 增加 `ctx` 字段
- 临界区接口由宏（`MOTE_ENTER_CRITICAL` / `MOTE_EXIT_CRITICAL`）改为函数（`mote_crit_enter` / `mote_crit_exit` / `mote_crit_active`），采用保存/恢复语义；自定义移植层需按新接口实现

### 新增

- `mote_dropped_count()`：统一丢事件计数（post / replace / delayed / 邮箱拒绝 / 未注册事件全部计入）
- `mote_set_drop_hook()`：丢事件回调（关中断上下文调用）
- 单次定时器"至少一次送达"：队列满时保留重试，事件绝不蒸发
- 任务层支持每任务 `ctx` 上下文
- CI 三档：`-Werror` 宿主机测试、ASan/UBSan、ARM（M0+/M3）与 RISC-V 交叉编译 + 内核体积断言

### 修复

- 临界区改为保存/恢复式（Cortex-M 用 PRIMASK，WCH RISC-V 用 INTSYSCR），支持嵌套、不破坏调用方中断状态
- `mote_ticks()` 原子读（M0+ 上 32 位读非原子）
- 邮箱 send 原子化：入箱与事件入队在同一临界区内完成，回滚无竞态窗口
- `MOTE_P` / `MOTE_U32` 改用 `uintptr_t`，消除 64 位宿主机指针转换告警

### 文档

- 移植教程：临界区新接口与三内核实现对照
- 使用教程：定时器超时语义警告、邮箱中断延迟预算、丢事件监控
- README：中断延迟口径修正、模块表同步

## v0.1.0 - 2026-08-13

- 首个版本：事件队列（post / post_replace / post_delayed）、注册表 O(1) 派发、定时器、任务层、邮箱、低功耗、四套移植层（ch32v / cm0plus / cm3 / host）、移植与使用教程、PC 单元测试

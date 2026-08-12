# 更新日志

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

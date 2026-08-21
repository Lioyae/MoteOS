# CH32V003 低功耗电流测量例程

这个例程用于测 MoteOS tickless + WCH `__WFI()` 的板级电流，不用于功能压测。

## 工程要求

- 芯片：CH32V003，HCLK 48MHz
- LED：PC0，低电平点亮
- 唤醒标记：PC1，默认每 2 秒输出一个短脉冲
- 串口：USART1 TX=PD5，只在启动时打印一次，随后关闭 USART 和 PD5/PD6
- MoteOS 配置必须工程级开启：

```c
#define MOTE_TICKLESS     1
#define MOTE_PORT_HCLK_HZ 48000000u
```

建议同时把配置调小：

```c
#define MOTE_EVT_QUEUE_SIZE 4
#define MOTE_DELAYED_MAX    1
#define MOTE_ENABLE_TASK    0
#define MOTE_TASK_SLOT_MAX  1
#define MOTE_ENABLE_MAILBOX 0
```

本例程在 `User/main.c` 放了一个强符号 `SysTick_Handler`，用于确认向量表确实
打到用户工程；handler 内部调用 `mote_port_systick_handler()`，实际 tickless
入账仍由 `moteos/port/mote_port.c` 的 port 状态机完成。因此配置里必须保留：

```c
#define MOTE_PORT_DEFAULT_SYSTICK_HANDLER 0
```

否则 CH32 port 默认的 `SysTick_Handler` 与本例程的 `SysTick_Handler` 会同时
成为强符号，链接时报 multiple definition。

## 预期启动信息

烧录正确版本后，串口应只在启动时看到一次：

```text
==== MoteOS CH32V003 Stage16 ====
low-power current measurement, tickless + WCH __WFI
boot print only; USART off after this banner
wake_ms=2000; marker_pc1=1; led_debug=1
```

如果这里不是 `wake_ms=2000`，或者仍然在持续打印，说明你烧录的不是 Stage16
最新低功耗测量版本。

## 预期波形

启动时会有一段高电流，因为 GPIO、USART 和启动打印都在工作。打印完成后：

- 默认诊断模式下，PC0 LED 每 2 秒短亮一次，方便肉眼确认调度周期
- PC1 每 2 秒输出一个加宽的同步脉冲，适合示波器触发
- USART1 关闭，PD5/PD6 进入模拟输入
- 只保留一个 2 秒周期事件
- 空闲时 `mote_loop()` 进入 tickless `mote_idle()`，port 层调用 WCH `__WFI()`

示波器上应该看到：

```text
启动高电流 -> 长时间低平台 -> 到期短尖峰 -> 长时间低平台
```

若要测更纯的电流，把 `STAGE16_MARKER_ENABLE` 和
`STAGE16_LED_DEBUG_ENABLE` 都改成 `0` 后重新编译。此时 PC0 不再参与测量，
PC1 也不再输出标记；只能从 shunt 电流波形判断唤醒尖峰。

## 已记录的 CH32V003 板级电流

下面是一次 CH32V003 板级测量结果，包含板子本身、PC0 LED、GPIO 标记和外部
连接带来的电流，不代表芯片裸片睡眠电流。

| 采样电阻 | 波形电压 | 换算电流 | 现象 |
|---|---:|---:|---|
| 10Ω | 93.6mV | 9.36mA | 低平台 |
| 10Ω | 105.6mV | 10.56mA | PC0 LED 亮平台 |
| 10Ω | 123.2mV | 12.32mA | 唤醒/活动尖峰 |
| 5Ω（两个 10Ω 并联） | 47.2mV | 9.44mA | 低平台 |
| 5Ω（两个 10Ω 并联） | 53.0mV | 10.60mA | PC0 LED 亮平台 |
| 5Ω（两个 10Ω 并联） | 61.6mV | 12.32mA | 唤醒/活动尖峰 |

这两组数据互相吻合：LED 大约增加 1.2mA，唤醒/活动段约 12.3mA。最终产品
电流必须在关闭 LED、关闭标记 GPIO、断开调试器和不必要外设后重新测。

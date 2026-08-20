# CH32V003 低功耗电流测量例程

这个例程用于测 MoteOS tickless + WCH `__WFI()` 的低功耗电流，不用于功能压测。

## 工程要求

- 芯片：CH32V003，HCLK 48MHz
- LED：PC0，低电平点亮；本例程会关闭它
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

## 预期波形

启动时会有一段高电流，因为 GPIO、USART 和启动打印都在工作。打印完成后：

- PC0 进入模拟输入，LED 不再耗电
- USART1 关闭，PD5/PD6 进入模拟输入
- 只保留一个 10 秒周期事件
- 空闲时 `mote_loop()` 进入 tickless `mote_idle()`，port 层调用 WCH `__WFI()`

示波器上应该看到：

```text
启动高电流 -> 长时间低平台 -> 到期短尖峰 -> 长时间低平台
```

PC1 默认输出一个很短的唤醒标记脉冲，方便示波器触发。若要测更纯的电流，把
`STAGE16_MARKER_ENABLE` 改成 `0`。

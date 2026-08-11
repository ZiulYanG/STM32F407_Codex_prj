# Application SPI NOR JEDEC ID验证

日期：2026-08-11
目标：验证Application的SPI1 BSP、软件片选和最小SPI NOR器件层，并确认板载Flash身份。

## 1. 验收配置

| 项目 | 配置 |
|---|---|
| SPI | SPI1，Mode 0，MSB First，软件NSS |
| 时钟 | APB2 84 MHz，16分频，SCK约5.25 MHz |
| 引脚 | PB3/SCK、PB4/MISO、PB5/MOSI、PB14/CS |
| 串口 | USART1，COM3，115200 8N1 |
| 烧录器 | DAPLink/CMSIS-DAP，序列号`B1897547C3840B15B1FE4CDBBF997647` |
| APP区域 | `0x08040000–0x080FFFFF` |

驱动不创建任务、不使用动态内存，只通过注入的SPI传输和片选回调访问BSP。器件表当前包含：

- `NM25Q128EVB`：`52 21 18`；
- `W25Q128`：`EF 40 18`。

## 2. 自动验证命令

在仓库根目录运行：

```text
python Firmware/Application/script/build.py --clean
python Firmware/Application/script/verify_spi_nor.py
```

脚本会先校验APP ELF地址边界，再通过OpenOCD烧录、校验并复位，最后捕获COM3日志。只有同时出现以下三行才返回成功：

```text
SPI NOR JEDEC : 52 21 18
SPI NOR model : NM25Q128EVB
SPI NOR check : SUPPORTED
```

## 3. 实机结果

- Clean Build通过，共43个目标文件；只有ST HAL原有的未使用参数警告。
- ELF/HEX有效范围为`0x08040000–0x0804774B`，没有越过APP分区。
- DAPLink编程和Verify均成功。
- Bootloader判断APP有效并跳转到`0x0804xxxx`。
- 上述JEDEC结果在完整验收后又独立重复一次，结果一致，脚本两次返回0。

公开Flash支持列表将`52-21-18`列为`NM25Q128EVB`。这是当前型号名称的资料匹配依据；尚未读取芯片表面丝印，因此应记录为高可信推断，而不是物理丝印确认。

## 4. 故障定位与结论

首次直接发送`0x9F`时返回`FF FF FF`。加入`0xAB`退出掉电后返回稳定的`52 21 18`。随后采用单变量方式验证：

1. 将命令和接收改为单次4字节全双工事务，结果不变；
2. SPI从5.25 MHz降至328 kHz，结果不变；
3. 发送`0x66/0x99`复位序列，结果不变；
4. 从Mode 0切换为Mode 3，结果不变；
5. SWD确认PB3/PB4/PB5为AF5、PB14空闲为高、SPI1已启用且无溢出标志。

因此`52 21 18`是有效且稳定的器件身份，不是速率、模式或HAL拆分事务造成的乱码。原理图/项目约定为W25Q128，而当前板卡很可能使用了兼容的同容量器件。

加入退出掉电后的`HAL_Delay(1)`曾使APP在跳转后无输出。根因是Bootloader按设计保持PRIMASK，APP在FreeRTOS启动前尚未恢复全局中断，依赖TIM7时基的`HAL_Delay()`无法推进。现已在Application入口、`HAL_Init()`之前显式执行`__enable_irq()`，随后构建和两次实机验证均通过。

## 5. 下一步边界

下一步只扩展SPI NOR最小擦写能力：读取状态、Write Enable、4 KB扇区擦除、页编程、普通读取和回读比较。测试地址必须位于规划的Reserved区域，并固定为一个明确的4 KB测试扇区，禁止触碰Candidate、Golden、Metadata、Event Log和Crash Dump。

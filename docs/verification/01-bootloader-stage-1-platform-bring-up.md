# Bootloader 阶段 1：基础平台验证报告

## 1. 验收范围

本报告对应固件版本 `v0.2.0`，验证以下第一阶段目标：

- STM32F407ZGT6 运行于 168 MHz；
- TIM7 提供 STM32 HAL 1 ms 时基；
- SysTick 仅提供 FreeRTOS 1 kHz 系统节拍；
- USART1 使用 PA9/PA10、115200 8N1；
- 静态日志任务通过静态队列集中发送日志；
- 上电输出基础运行信息；
- PF9/PF10 LED 每 500 ms 交替；
- 构建、烧录、校验、复位和运行状态可重复验证。

## 2. 接线与端口

| 功能 | 连接 |
|---|---|
| SWDIO | DAPLink SWDIO → PA13 |
| SWCLK | DAPLink SWCLK → PA14 |
| SWD GND | DAPLink GND → 开发板 GND |
| 串口日志 | 开发板 PA9/TX → USB 转串口 RX |
| 串口接收 | 开发板 PA10/RX ← USB 转串口 TX |
| 串口 GND | USB 转串口 GND → 开发板 GND |

串口电平必须为 3.3 V TTL。当前 Windows 环境中，独立 USB 转串口为 `COM3`；DAPLink 附带接口为 `COM11`，不用于本工程日志。COM 编号不是固件配置，换电脑后需要重新确认。

## 3. 构建和烧录

在普通 CMD 中执行：

```cmd
cd /d D:\ZzlWorkDir\STM32\STM32F407\Codex_STM32F407\Firmware\Bootloader
python script\build.py --clean
python script\flash.py --dap-serial B1897547C3840B15B1FE4CDBBF997647
```

验收结果：

- ELF、BIN、HEX 均成功生成；
- Flash 使用 26,468 B / 256 KB；
- RAM 使用 29,904 B / 128 KB；
- OpenOCD 显示 `Programming Finished`、`Verified OK` 和 `Resetting Target`。

工具链中的 STM32 HAL `stm32f4xx_hal_flash_ex.c` 有 3 个未使用形参警告，属于厂商库源码，不影响本阶段固件构建；项目自有代码没有编译警告。

## 4. 串口端到端验证

打开 COM3，设置为 115200、8 数据位、无校验、1 停止位、无流控，然后复位目标板。实际捕获内容为：

```text
================================
STM32F407 Bootloader
Version       : 0.2.0
Build         : Aug  8 2026 15:53:51
Reset reason  : Software reset
SYSCLK        : 168000000 Hz
HCLK          : 168000000 Hz
PCLK1         : 42000000 Hz
PCLK2         : 84000000 Hz
SystemCoreClk : 168000000 Hz
HAL timebase  : TIM7
RTOS tick     : 1000 Hz
USART1        : 115200 8N1
APP address   : 0x08040000
Boot state    : DEVELOPMENT
================================
```

测试同时捕获过 `Pin reset`，说明复位来源读取和清除逻辑能够区分外部/调试复位与软件复位。`Build` 字段来自当前构建，每次重新编译都会变化。

## 5. SWD 运行态验证

串口输出用于功能验收，SWD 回读用于验证底层配置没有被字符串或编译期常量掩盖。

| 检查项 | 实测值 | 结论 |
|---|---:|---|
| RCC CFGR | `0x0000940A` | SYSCLK=PLL、AHB/1、APB1/4、APB2/2 |
| FLASH ACR | `0x00000705` | 5 WS，预取与 Cache 已启用 |
| SystemCoreClock | `0x0A037A00` | 168,000,000 Hz |
| TIM7 PSC | `83` | 84 MHz / 84 = 1 MHz |
| TIM7 ARR | `999` | 1 MHz / 1000 = 1 kHz |
| TIM7 CR1/DIER | CEN=1、UIE=1 | HAL tick 正在运行 |
| TIM7 IRQ 优先级 | `0` | 不调用 FreeRTOS API |
| SysTick LOAD | `167999` | 168 MHz 下产生 1 kHz tick |
| SysTick IRQ 优先级 | `15` | FreeRTOS 内核最低优先级 |
| USART1 BRR | `0x02D9` | PCLK2=84 MHz 下为 115200 |
| USART1 CR1 | `0x200C` | USART、发送和接收已启用 |
| GPIOA AFRH | PA9/PA10=AF7 | USART1 引脚映射正确 |
| HAL/RTOS tick | 两次读取均递增 | 两套时基正常工作 |
| 日志队列/任务 | 句柄非空 | 静态对象创建成功 |
| 日志丢弃计数 | `0` | 启动日志无队列/发送丢弃 |
| GPIOF ODR | 观察到 `0x0400`、`0x0200` | PF9/PF10 交替输出 |

## 6. 通过标准

只有同时满足以下条件才允许创建 `v0.2.0` 标签：

1. Clean Build 成功并生成三种固件产物；
2. DAPLink 烧录校验通过并复位运行；
3. COM3 捕获的运行时频率和时基信息与设计一致；
4. SWD 寄存器回读与串口信息一致；
5. FreeRTOS/HAL tick 持续增长，日志对象有效且无丢弃；
6. PF9/PF10 持续交替。

本次六项均通过。

## 7. 修改 CubeMX 后的回归要求

`.ioc` 是配置源之一，但不能替代生成源码和目标板运行验证。每次重新生成代码后至少检查：

- `main.c` 的 AHB/APB 分频与 Flash latency；
- `stm32f4xx_hal_conf.h` 的 TIM 模块和 tick 优先级；
- `stm32f4xx_it.c` 中 SysTick 不再调用 `HAL_IncTick()`；
- `stm32f4xx_hal_timebase_tim.c` 仍参与 CMake 构建；
- `usart.c` 仍使用 PA9/PA10，而不是 PB7；
- 可选 SDIO 不会在无卡时阻断 Bootloader 启动；
- CMake 中 `app/`、`bsp/`、`components/logging/` 源文件仍被编译。

完成审查后必须重新执行本报告第 3～6 节的完整验收。

# CubeMX 配置基线

## 1. 工具与器件

- MCU：STM32F407ZGT6，LQFP144。
- 当前本机 STM32CubeMX：6.12.0-RC9。
- Bootloader 和 APP 各自维护一份 `.ioc`。
- 两份 `.ioc` 使用相同 MCU、系统时钟和基础引脚命名。
- 正式开始生成工程时固定 STM32CubeMX 和 STM32CubeF4 Firmware Package 版本，并在 CI 构建清单中记录。

当前安装的是 RC 版本。首个发布基线建立前应评估迁移到稳定 GA 版本；一旦进入功能开发，不应在没有生成代码差异审查的情况下升级 CubeMX。

## 2. 系统与调试

| CubeMX 项目 | 配置 |
|---|---|
| SYS Debug | Serial Wire |
| Timebase Source | TIM7 |
| Cortex-M4 FPU | Enabled |
| NVIC Priority Group | 4 bits pre-emption priority |
| HSE | Crystal/Ceramic Resonator |
| LSE | 首版可关闭；RTC 阶段再开启 |

使用 TIM7 作为 HAL Tick，SysTick 专供 FreeRTOS。这样 HAL 时基和 RTOS Tick 的职责清晰，同时保留 TIM6 供未来 DAC 触发使用。

## 3. 168 MHz 时钟树

板载 HSE 为 8 MHz，推荐统一配置：

| 参数 | 值 |
|---|---:|
| PLLM | 8 |
| PLLN | 336 |
| PLLP | 2 |
| PLLQ | 7 |
| SYSCLK | 168 MHz |
| AHB | 168 MHz |
| APB1 | 42 MHz |
| APB1 Timer | 84 MHz |
| APB2 | 84 MHz |
| APB2 Timer | 168 MHz |
| USB Clock | 48 MHz |

Bootloader 即使首版不启用 USB，也保持与 APP 相同的系统时钟配置，减少驱动和超时行为差异。

## 4. Bootloader 首版外设

### 4.1 GPIO

| Label | Pin | 配置 | 初始状态 |
|---|---|---|---|
| `FLASH_CS` | PB14 | Output Push-Pull | High |
| `LED0_N` | PF9 | Output Push-Pull | High/Off |
| `LED1_N` | PF10 | Output Push-Pull | High/Off |

### 4.2 SPI1 - W25Q128

| 信号 | Pin | 配置 |
|---|---|---|
| SCK | PB3 | AF5 |
| MISO | PB4 | AF5 |
| MOSI | PB5 | AF5 |

首版建议：

- Master；
- Full Duplex；
- 8 bit；
- CPOL Low / CPHA 1 Edge，即 SPI Mode 0；
- MSB First；
- Software NSS；
- 先使用阻塞式传输和保守分频，完成稳定性测试后再引入 DMA/提速。

PB3/PB4 需要 SYS 选择 Serial Wire 以释放 JTAG 复用。

### 4.3 USART1 - 调试与升级

| 参数 | 首版值 |
|---|---|
| TX/RX | PA9/PA10 |
| Baud | 115200 |
| Data/Parity/Stop | 8/N/1 |
| Flow Control | None |
| Direction | TX + RX |

升级接收最终采用 DMA 环形缓冲区和 IDLE 检测；P0/P1 阶段可先用中断或阻塞收发验证基础链路。DMA 具体 Stream/Channel 必须由 CubeMX 冲突检查后固定在 `.ioc`，不能只写在业务代码中。

## 5. APP 首版增量外设

### 5.1 I2C1 - 24C02

| 信号 | Pin | 配置 |
|---|---|---|
| SCL | PB8 | AF4 Open-Drain |
| SDA | PB9 | AF4 Open-Drain |

首版使用 100 kHz，验证板载上拉和总线波形后再评估 400 kHz。

### 5.2 按键

| Label | Pin | 配置 |
|---|---|---|
| `KEY0` | PE4 | Input Pull-Down |
| `WK_UP` | PA0 | Input Pull-Down |

消抖由服务层定时处理，按键 ISR 不执行产品业务。

### 5.3 Modbus RTU

首个练习复用 USART1。APP 运行时 USART1 承载 Modbus RTU Server；复位进入 Bootloader 后同一物理串口承载升级协议。

不在 APP 中通过首字节猜测同时复用两种协议。WinForms 通过明确的“请求进入 Bootloader”流程切换设备运行阶段。

## 6. USB CDC 学习工程

| 项目 | 配置 |
|---|---|
| USB Mode | Device Only |
| USB Core | OTG_FS |
| D-/D+ | PA11/PA12 |
| VBUS Sensing | Disabled |
| SOF | 按 USB 栈要求配置 |
| Class | CDC ACM |
| Clock | 48 MHz from PLLQ |

TinyUSB 和 USBX 工程使用相同的时钟、端点规模、CDC 缓冲区目标和测试负载，以保证比较公平。

## 7. FreeRTOS 基线

- 使用静态任务、队列、信号量和软件定时器创建 API。
- Bootloader 禁止动态分配支持。
- 开启栈溢出检测和运行期栈水位采集。
- 开启断言，并将断言处理接入崩溃记录。
- 所有调用 FreeRTOS `FromISR` API 的中断优先级必须满足 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 约束。
- 中断优先级数值和 `FreeRTOSConfig.h` 必须在专门的中断表中维护，不能散落在驱动源码。
- Bootloader 与 APP 使用独立 `FreeRTOSConfig.h`，不能互相复制后长期漂移。

## 8. 可选 Profile

首版 `.ioc` 不同时打开以下功能：

| Profile | 功能 | 主要原因 |
|---|---|---|
| `USB_DEVICE_LAB` | USB CDC | 独立学习工程 |
| `SD_CARD_PROFILE` | SDIO TF 卡 | 与 DCMI D2..D4 冲突 |
| `CAMERA_PROFILE` | DCMI 摄像头 | 与 SDIO 和部分未来 RMII 资源冲突 |
| `FSMC_PROFILE` | 外部 SRAM | 大量固定 IO，占用面广 |
| `LCD_PROFILE` | FSMC LCD | 依赖 FSMC，首版不需要 |
| `CAN_BOARD` | CAN2 | 需要外部收发器底板 |
| `RS485_BOARD` | USART2 RS485 | 需要外部收发器底板 |

## 9. 数据库复核

已使用本机 CubeMX MCU 数据库 `STM32F407Z(E-G)Tx.xml` 复核以下关键复用：

- PA9/PA10：USART1 TX/RX；
- PB3/PB4/PB5：SPI1 SCK/MISO/MOSI；
- PB8/PB9：I2C1 SCL/SDA；
- PA11/PA12：USB OTG FS DM/DP；
- PB12/PB13：CAN2 RX/TX；
- PA2/PA3：USART2 TX/RX。

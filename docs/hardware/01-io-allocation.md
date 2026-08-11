# STM32F407 核心板 IO 分配

## 1. 文档规则

状态定义：

- **固定**：原理图已连接到板载器件或晶振，不能自由改配。
- **首版**：Bootloader/APP 第一阶段启用。
- **预留**：为后续底板或功能保留，当前不初始化。
- **可选冲突**：只有选择对应 BSP 功能组合时才能启用。
- **空闲**：当前原理图未发现板载负载，仍需用实板复核。

所有 CubeMX 工程必须与本表同步。引脚变更先修改本文档和 BSP，再修改 `.ioc`。

## 2. 首版有效配置

| 功能 | 外设 | 引脚 | 模式/备注 | Bootloader | APP |
|---|---|---|---|---:|---:|
| HSE | RCC | PH0/PH1 | 板载 8 MHz | 是 | 是 |
| LSE | RCC | PC14/PC15 | 板载 32.768 kHz；首版可先关闭 RTC | 可选 | 可选 |
| SWD | SYS | PA13/PA14 | Serial Wire，不能启用完整 JTAG | 是 | 是 |
| 调试/升级串口 | USART1 | PA9/PA10 | TX/RX，连接 CH340 和 JP3 | 是 | 是 |
| 外部 Flash | SPI1 | PB3/PB4/PB5 | SCK/MISO/MOSI，AF5 | 是 | 是 |
| 外部 Flash CS | GPIO | PB14 | `F_CS`，推挽输出，空闲为高 | 是 | 是 |
| EEPROM | I2C1 | PB8/PB9 | SCL/SDA，板载 4.7 kΩ 上拉 | 可选 | 是 |
| 状态 LED0 | GPIO | PF9 | 低电平点亮 | 是 | 是 |
| 状态 LED1 | GPIO | PF10 | 低电平点亮 | 是 | 是 |
| 用户键 KEY0 | GPIO | PE4 | 按下为高，建议内部下拉 | 可选 | 是 |
| 唤醒键 | GPIO | PA0 | `WK_UP`，按下为高，建议内部下拉 | 可选 | 是 |
| USB Device D-/D+ | OTG_FS | PA11/PA12 | Device Only；禁止 VBUS sensing | 否，后续 | 学习工程 |

### 2.1 必须遵守的初始化约束

1. PB3/PB4 是 JTAG 复用脚。CubeMX 的 Debug 只能选择 `Serial Wire`，释放 PB3/PB4 给 SPI1。
2. W25Q128 的 CS 在 SPI 初始化前必须先输出高，防止上电误选中。
3. PF9/PF10 LED 为低有效；复位初期应输出高或保持高阻。
4. PA9 同时具备 USB VBUS 复用能力，但已连接 USART1 TX。USB 工程必须关闭 VBUS sensing。
5. USB 时钟必须为精确 48 MHz，时钟树变更需要重新验证枚举。
6. Type-C 接口原理图未显示 CC1/CC2 下拉。学习阶段优先使用 USB-A 转 Type-C 线；正式底板应补充规范的 CC 电阻。

## 3. 板载固定资源

### 3.1 W25Q128 与 24C02

| 器件 | 信号 | MCU 引脚 |
|---|---|---|
| W25Q128 | SPI1_SCK | PB3 |
| W25Q128 | SPI1_MISO | PB4 |
| W25Q128 | SPI1_MOSI | PB5 |
| W25Q128 | F_CS | PB14 |
| W25Q128 | /WP | 固定接 3.3 V，不占用 MCU IO |
| W25Q128 | /HOLD | 固定接 3.3 V，不占用 MCU IO |
| 24C02 | I2C1_SCL | PB8 |
| 24C02 | I2C1_SDA | PB9 |

24C02的SCL、SDA分别通过4.7 kΩ上拉到3.3 V，因此MCU端使用复用开漏、无内部上下拉。A0/A1/A2均接地，7位设备地址为`0x50`；WP接地，器件允许写入。

### 3.2 TF 卡

| 信号 | MCU 引脚 | 冲突 |
|---|---|---|
| SDIO_D0 | PC8 | DCMI_D2 |
| SDIO_D1 | PC9 | DCMI_D3 |
| SDIO_D2 | PC10 | DCMI_D4 |
| SDIO_D3 | PC11 | — |
| SDIO_CK | PC12 | — |
| SDIO_CMD | PD2 | — |

TF 卡和摄像头 DCMI 不能按当前原理图同时完整启用。必须通过不同 BSP 功能配置二选一。

### 3.3 外部 SRAM/FSMC

| 总线 | 信号 | MCU 引脚 |
|---|---|---|
| Data | D0/D1 | PD14/PD15 |
| Data | D2/D3 | PD0/PD1 |
| Data | D4..D12 | PE7..PE15 |
| Data | D13..D15 | PD8..PD10 |
| Address | A0..A5 | PF0..PF5 |
| Address | A6..A9 | PF12..PF15 |
| Address | A10..A15 | PG0..PG5 |
| Address | A16..A18 | PD11..PD13 |
| Control | NOE/NWE | PD4/PD5 |
| Control | NBL0/NBL1 | PE0/PE1 |
| SRAM CS | NE3 | PG10 |
| LCD CS | NE4 | PG12 |

FSMC 占用大量引脚。Bootloader 不启用 FSMC；APP 首版也不依赖外部 SRAM，待基础升级闭环稳定后再增加 `BSP_FEATURE_FSMC_SRAM`。

### 3.4 LCD 触摸与板载 UI

| 功能 | MCU 引脚 | 备注 |
|---|---|---|
| LED0 | PF9 | 低有效 |
| LED1 | PF10 | 低有效 |
| LCD Backlight | PB15 | `LCD_BL` |
| Touch SCK | PB0 | `T_SCK` |
| Touch PEN | PB1 | `T_PEN` |
| Touch MISO | PB2 | 同时是 BOOT1 复用脚 |
| Touch MOSI | PF11 | `T_MOSI` |
| Touch CS | PC13 | `T_CS` |

### 3.5 DCMI 摄像头

| 信号 | MCU 引脚 | 主要冲突 |
|---|---|---|
| HREF | PA5 | — |
| PCLK | PA7 | RMII_CRS_DV |
| XCLK | PA8 | — |
| VSYNC | PB7 | — |
| D0/D1 | PC6/PC7 | — |
| D2/D3/D4 | PC8/PC9/PC10 | SDIO D0/D1/D2 |
| D5 | PB6 | — |
| D6/D7 | PE5/PE6 | — |
| SCCB SCL/SDA | PD6/PD7 | — |
| PWDN | PG11 | — |
| RESET | PG15 | — |

## 4. 扩展接口预留

### 4.1 RS485

首选 USART2，避免占用板载 USART1：

| 信号 | 建议引脚 | 状态 |
|---|---|---|
| USART2_TX | PA2 | 预留 |
| USART2_RX | PA3 | 预留 |
| RS485_DE | PA1 | 预留 |

该分配与未来 RMII 的 REF_CLK/MDIO 规划冲突，因此以太网底板需要独立 BSP 重新装配。

### 4.2 CAN

首选 CAN2，避免占用 USB 的 PA11/PA12、I2C1 的 PB8/PB9 和 FSMC 的 PD0/PD1：

| 信号 | 建议引脚 | 状态 |
|---|---|---|
| CAN2_RX | PB12 | 预留，需外部收发器 |
| CAN2_TX | PB13 | 预留，需外部收发器 |

使用 CAN2 时仍需按 STM32F4 参考手册启用 CAN1 时钟域，并单独验证过滤器分配。

### 4.3 以太网 RMII

RMII 与 PA1、PA2、PA7、PB11、PB12、PB13、PC1、PC4、PC5 等资源相关，会与 RS485 预留、CAN2 预留和摄像头部分信号冲突。以太网不进入当前核心板首版 IO 基线；后续建立独立 Ethernet 底板 BSP。

## 5. Bootloader 与 APP 引脚子集

### 5.1 Bootloader 首版

```text
PH0/PH1     HSE
PA13/PA14   SWD
PA9/PA10    USART1
PB3/4/5     SPI1
PB14        W25Q128 CS
PF9/PF10    状态 LED
```

Bootloader 不初始化 EEPROM、TF、FSMC、LCD、DCMI、CAN 和 USB。后续 USB 恢复入口选型完成后再加入 PA11/PA12。

### 5.2 APP 首版

```text
Bootloader 首版全部引脚
PB8/PB9     I2C1 / 24C02
PE4         KEY0
PA0         WK_UP
```

USART1 在 APP 中首先承载 Modbus RTU Server；进入 Bootloader 后承载升级协议，两种协议不在同一运行阶段混用。

## 6. 待实板验证项

- USB Type-C 使用不同线缆时的 VBUS 和枚举行为。
- PF9/PF10、PE4、PA0 的实际有效电平和上电默认状态。
- SPI1 在 PB3/PB4 JTAG 复用释放后的稳定性。
- W25Q128 最高可靠 SPI 时钟和长时间擦写行为。
- CAN2 与后续底板收发器的引脚、终端和待机控制。
- 用户资料包中的原理图文件名为 `STM32F407_CORE_BOARD_V1.4.pdf`，但第 1～3 页标题栏均为 V1.0；该文件标记差异已确认，实际板卡版本仍需结合 PCB 丝印复核。

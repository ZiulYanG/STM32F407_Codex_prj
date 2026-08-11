# W25Q128 连接引脚与 CubeMX 配置审计

审计日期：2026-08-10
审计范围：板载 W25Q128 的标准 SPI 连接、控制引脚、当前 Bootloader/Application `.ioc` 及生成代码
结论状态：静态审计完成；Application配置与链接分区已整改并通过构建验证；Bootloader配置及JEDEC ID实机验证待完成

## 1. 审计依据

- 仓库硬件分配文档与 `io-allocation.csv`；
- 用户提供的本机原理图 `D:\BaiduNetdiskDownload\【正点原子】STM32F407最小系统板资料\3，STM32F407最小系统板原理图\3，STM32F407最小系统板原理图\STM32F407_CORE_BOARD_V1.4.pdf`；
- `Firmware/Bootloader/Bootloader.ioc`、`Firmware/Application/Application.ioc`；
- 两套工程当前生成的 GPIO、SPI、HAL 配置和初始化顺序；
- Winbond W25Q128 数据手册对标准 SPI、`/CS`、`/WP`、`/HOLD` 的定义。

已对该 PDF 全部 4 页进行文本提取，并以 200 DPI 渲染相关页面做视觉核验：第 1 页 MCU 网络确认 PB3/PB4/PB5/PB14 分配；第 2 页 FLASH 电路确认 W25Q128 各引脚连接。文件 SHA-256 为 `2B15810ABBC471B3B4BECFB7D97AB8DA94F68DFBE3AA58050623D42EB808F6FC`。

原理图文件名标为 V1.4，但第 1～3 页标题栏均标为 V1.0，日期为 2021/7/27。这一文件内部标记差异已由原文件直接确认；它不影响 W25Q128 网络审计，但实际板卡版本仍需通过 PCB 丝印复核。原理图当前位于仓库外，后续如需长期可复现审计，可再决定是否将其归档到仓库。

## 2. 已确认硬件连接

| W25Q128 信号 | MCU 连接 | 固件配置 | 审计结论 |
|---|---|---|---|
| CLK | PB3 / SPI1_SCK / AF5 | SPI1 主机时钟 | 已确认 |
| DO / IO1 | PB4 / SPI1_MISO / AF5 | SPI1 主机输入 | 已确认 |
| DI / IO0 | PB5 / SPI1_MOSI / AF5 | SPI1 主机输出 | 已确认 |
| /CS | PB14 / GPIO | 软件片选，低有效 | 已确认 |
| /WP / IO2 | 固定接 3.3 V | 不配置 MCU IO | 已确认，标准 SPI 下禁用硬件写保护输入 |
| /HOLD / IO3 | 固定接 3.3 V | 不配置 MCU IO | 已确认，标准 SPI 下禁用保持输入 |

因此首版标准 SPI 驱动只需要 PB3、PB4、PB5 和 PB14 四个 MCU 引脚，不应为 `/WP` 或 `/HOLD` 分配 GPIO。

原理图第 2 页还确认 U3 电源为 VCC3.3、GND 接地，并在电源旁配置 C30 `104` 去耦电容。

## 3. IO 冲突审计

### 3.1 当前必须处理的复用关系

- PB3、PB4 与完整 JTAG 复用；两套 `.ioc` 均把 SYS Debug 配成 `Serial Wire`，PA13/PA14 保留 SWD，PB3/PB4 已释放给 SPI1。
- PB5 当前没有被两套工程中的其他已启用功能占用。
- PB14 当前没有被其他已启用功能占用；SPI1 使用软件 NSS，因此 PB14 必须保持普通 GPIO 输出。
- `/WP` 和 `/HOLD` 固定接高，不存在 MCU IO 冲突。

### 3.2 与可选 Profile 的关系

- 后续不得在启用 W25Q128 时把 PB3/PB4 恢复为 JTAG。
- PB3/PB4/PB5/PB14 属于板载固定资源，即使某个固件暂时不用 Flash，也不应在同一硬件 Profile 中分配给其他外设。
- 当前 SDIO、USB、I2C1 引脚与 W25Q128 四个引脚没有直接重叠，但 Bootloader `.ioc` 同时保留这些非首版功能，违反最小 Bootloader 基线，增加重新生成代码时的漂移面。

## 4. 当前 `.ioc` 和生成代码差异

### 4.1 Bootloader

已正确配置：

- SPI1 全双工主机、8 bit、MSB First、软件 NSS；
- PB3/PB4/PB5 分别为 SPI1 SCK/MISO/MOSI；
- SPI Mode 0；
- GPIO 在 SPI1 之前初始化。

必须在进入驱动实现前修正：

1. `PB14` 没有 `FLASH_CS` 标签，也没有配置默认高电平；当前生成的 `gpio.c` 明确先写低电平，会在启动阶段选中 W25Q128。
2. SPI1 当前为 APB2 84 MHz 的 2 分频，即 42 MHz；这不符合首个 JEDEC ID 验证采用保守低速的既定策略。首验建议改为 16 分频，即 5.25 MHz。
3. `.ioc` 仍启用 SDIO、USB OTG FS，并残留 PB8/PB9 的 I2C1 复用；生成代码也仍包含相关初始化或引脚配置。它们与 W25Q128 无直接引脚冲突，但与 Bootloader 首版最小外设集合不一致。

### 4.2 Application

必须在进入驱动实现前补齐：

1. `.ioc` 未启用 SPI1，也未分配 PB3/PB4/PB5。
2. PB14 未配置为 Flash 软件片选，Application 无法主动保证 `/CS` 空闲为高。
3. `HAL_SPI_MODULE_ENABLED` 当前关闭，生成工程没有 SPI1 初始化代码。

### 4.3 两套工程共同约束

- 先把 PB14 数据寄存器预置为高，再配置为推挽输出，然后初始化 SPI1。
- PB14 使用软件控制，不启用硬件 NSS。
- 首个实机验收只读取 JEDEC ID；在低速稳定后再单独提速并验证。
- CubeMX 重新生成后必须检查 `.ioc`、GPIO 初始电平、HAL 模块开关、生成源码和 CMake 源文件清单，不能只检查 GUI。

## 5. 审计结论与下一步

W25Q128 的实际连接和 IO 冲突已经审清，没有硬件引脚阻塞。当前阻塞来自 CubeMX 配置漂移：Bootloader 会错误地把 `/CS` 拉低，Application 则尚无 SPI1/CS 配置。

下一项单一交付：只修正两套 `.ioc` 及其生成代码，使 W25Q128 引脚、PB14 默认高、SPI1 低速 Mode 0 和 Bootloader 最小外设集合与基线一致；完成 Git diff 审查和两套工程 Clean Build。该步骤完成前不加入 W25Qxx 驱动。

## 6. 2026-08-11 Application整改跟进

Application侧已完成：

- PB3/PB4/PB5配置为SPI1 SCK/MISO/MOSI，保持Serial Wire释放JTAG复用脚；
- PB14配置为软件片选，标签为`F_CS`，初始化时预置高电平；
- SPI1使用Mode 0、软件NSS、16分频，首验时钟约5.25 MHz；
- HAL SPI和CMake源文件清单已同步，Application Clean Build通过；
- APP专用链接脚本移至`Firmware/Application/linker/STM32F407ZGTx_APP_FLASH.ld`，固定`0x08040000/768K`，避免继续依赖CubeMX生成的整片Flash脚本；
- `build.py`和`flash.py`会校验ELF/HEX必须位于`0x08040000–0x080FFFFF`；
- ELF与HEX正向检查范围均为`0x08040000–0x0804667F`；使用`0x08000000`的Bootloader ELF做负向测试时，APP烧录脚本在启动OpenOCD前正确拒绝镜像。

## 7. 2026-08-11 实机识别跟进

Application已加入SPI BSP和通用SPI NOR最小驱动，并在Mode 0、5.25 MHz下完成Clean Build、DAPLink烧录及COM3验收。实机连续两次读取到稳定JEDEC ID `52 21 18`，公开兼容列表将其对应为`NM25Q128EVB`，不是Winbond W25Q128的`EF 40 18`。

因此原理图中的W25Q128应理解为电路设计/兼容类别，当前板卡很可能发生了同容量兼容换料。固件型号表同时接受`NM25Q128EVB (52 21 18)`和`W25Q128 (EF 40 18)`；具体贴装型号仍应通过芯片丝印最终确认。

诊断过程还确认：

- `0xAB`退出掉电后器件才开始响应；Bootloader仍把PB14默认拉低，可能影响器件上电状态，必须在Bootloader存储阶段前整改；
- 将SPI降至328 kHz、切换Mode 3、发送`0x66/0x99`复位均不改变`52 21 18`，说明该结果不是高速采样错误或随机乱码；
- Application入口必须恢复Bootloader交接时保留的PRIMASK，否则调度器前的`HAL_Delay()`会因TIM7中断关闭而阻塞。

完整验证记录见`docs/verification/03-application-spi-nor-jedec.md`。Bootloader侧PB14默认低、SPI1 42 MHz及非首版外设清理仍待后续单独整改。

# STM32F407 通用固件平台

本仓库用于构建基于 STM32F407ZGT6 的通用固件平台，目标包括：

- 固定 Bootloader 与可升级 APP 两套独立 FreeRTOS 工程；
- W25Q128 暂存、签名验证、断电恢复和失败回滚；
- 静态装配、分层 BSP 与可复用设备驱动；
- USART、USB、CAN、TF 卡及后续网络升级入口；
- Modbus RTU/TCP 组件；
- 集成升级、USB 测试、Modbus 和日志功能的 WinForms 工具；
- TinyUSB 与 USBX 的两个独立学习对比工程。

当前已完成 Bootloader 第二阶段最小启动闭环：Bootloader 和 Application 两套独立 FreeRTOS 工程均可构建、烧录和输出运行信息；Bootloader 能校验 APP 初始 MSP/Reset_Handler，安全切换 VTOR/MSP 并跳转。升级传输、镜像签名、外部 Flash 和 WinForms 工具仍按路线图逐步实现。

## 当前版本

| 项目 | 内容 |
|---|---|
| 版本 | `v0.3.0` |
| 日期 | `2026-08-10` |
| 阶段 | Bootloader 阶段 2：独立 APP 与安全跳转 |
| 分支 | `main` |
| 仓库 | [ZiulYanG/STM32F407_Codex_prj](https://github.com/ZiulYanG/STM32F407_Codex_prj) |

本版本新增 `Firmware/Application` 独立工程，APP 固定链接到 `0x08040000`，向量表同步重定位。Bootloader 在启动后校验 APP 向量，刷新 USART1 队列后关闭中断和遗留外设、切换 VTOR/MSP 并跳转；无效向量则留在 Bootloader。完整验证记录见 [阶段 2 验证报告](docs/verification/02-bootloader-application-jump.md)，改动与踩坑见 [CHANGELOG.md](CHANGELOG.md)。

## 编译与烧录 Bootloader

请从普通 Windows CMD 运行脚本；已确认该终端中的 `python` 可用。Codex 的受限终端可能不会继承系统 Python 的 PATH，不影响你在本机 CMD 中执行。

当前仓库已经包含与 `.ioc` 对齐并通过验证的生成代码，日常构建无需先运行 CubeMX。只有修改 `.ioc` 后才重新生成代码，并且必须复核 CubeMX 对用户代码、CMake 源文件和可选外设初始化路径的影响。随后在仓库根目录运行 Python 脚本：

```text
python Firmware/Bootloader/script/build.py
```

该命令编译 `Debug` 配置，并在 `Firmware/Bootloader/build/Debug/` 中生成 `.elf`、`.bin` 和 `.hex`。通过 DAPLink 烧录：

```text
python Firmware/Bootloader/script/flash.py
```

脚本默认采用 DAPLink 的 CMSIS-DAP 接口和 1000 kHz SWD 时钟。需要完整重编译可执行 `python Firmware/Bootloader/script/build.py --clean`；连接不稳定时可降速，例如 `python Firmware/Bootloader/script/flash.py --adapter-speed 100`。多把 DAPLink 同时连接时，可用 `--dap-serial` 选择，例如 `python Firmware/Bootloader/script/flash.py --dap-serial B1897547C3840B15B1FE4CDBBF997647`。

## 编译与烧录 Application

Application 使用相同的 Python/CMake/Ninja/GNU Arm/DAPLink 工具链：

```text
python Firmware/Application/script/build.py --clean
python Firmware/Application/script/flash.py --adapter-speed 100 --dap-serial B1897547C3840B15B1FE4CDBBF997647
```

Application ELF/BIN/HEX 输出到 `Firmware/Application/build/Debug/`，链接起始地址为 `0x08040000`。烧录 APP 后复位会先运行 Bootloader，再由 Bootloader 校验并跳转到 APP。

## 串口启动日志

调试串口为 USART1：`PA9/TX`、`PA10/RX`、115200 8N1。当前电脑通过独立 USB 转串口连接后枚举为 `COM3`；`COM11` 是 DAPLink 的附带虚拟串口，不用于本工程日志。COM 编号由 Windows 动态分配，换电脑或 USB 口后应以设备管理器显示为准。

复位后应能看到如下关键内容：

```text
STM32F407 Bootloader
Version       : 0.3.0
SYSCLK        : 168000000 Hz
HCLK          : 168000000 Hz
PCLK1         : 42000000 Hz
PCLK2         : 84000000 Hz
HAL timebase  : TIM7
RTOS tick     : 1000 Hz
USART1        : 115200 8N1
APP address   : 0x08040000
APP check     : VALID
Boot action   : JUMP TO APPLICATION

STM32F407 Application
Version       : 0.3.0
Vector table  : 0x08040000
```

## 版本记录

| 版本 | 日期 | 实现功能 | 主要改动文件 | 验证 | 踩坑 |
|---|---|---|---|---|---|
| `v0.3.0` | 2026-08-10 | 独立 APP、`0x08040000` 分区与 VTOR、APP 向量校验、安全跳转、无效 APP 驻留 | `Firmware/Application/`、`Firmware/Bootloader/components/boot/`、`boot_app.c`、`boot_log.*`、`freertos.c`、`.ioc`、阶段 2 文档 | 双工程 Clean Build、DAPLink 校验烧录、有效跳转、Sector 6 擦除拒跳、恢复 APP、SWD PC/VTOR/故障寄存器验证通过 | 512 B 默认任务栈被 `vsnprintf` 压穿并覆盖 Timer 栈；跳转交接窗口过早开中断；CH340 Code 10/掉线影响日志抓取 |
| `v0.2.0` | 2026-08-08 | 168 MHz、TIM7 HAL 时基、FreeRTOS SysTick、静态 USART1 日志、启动信息 | `Firmware/Bootloader/Core/`、`app/`、`bsp/`、`components/logging/`、`docs/verification/` | Clean Build、DAPLink 烧录校验、SWD 寄存器回读、COM3 启动报文和 LED 验证通过 | `.ioc` 与生成源码漂移；TIM HAL 宏缺失导致链接失败；误将 COM11 当作板级日志串口 |
| `v0.1.0` | 2026-08-08 | Bootloader 基线、RTOS LED、DAPLink 构建烧录、故障定位 | `Firmware/Bootloader/`、`docs/`、`README.md` | 编译、烧录、复位和 GPIO 寄存器验证通过 | SDIO 无卡阻塞启动；OpenOCD Tcl 路径转义；CubeMX 配置与生成源码漂移 |

每次提交和上传版本必须同步更新本表及 [CHANGELOG.md](CHANGELOG.md)，记录版本、日期、实现功能、改动文件、验证方式和踩坑。具体规则见 [版本与提交规范](docs/development/01-versioning-and-commit-process.md)。

文档入口见 [docs/README.md](docs/README.md)。

# STM32F407 通用固件平台

本仓库用于构建基于 STM32F407ZGT6 的通用固件平台，目标包括：

- 固定 Bootloader 与可升级 APP 两套独立 FreeRTOS 工程；
- W25Q128 暂存、签名验证、断电恢复和失败回滚；
- 静态装配、分层 BSP 与可复用设备驱动；
- USART、USB、CAN、TF 卡及后续网络升级入口；
- Modbus RTU/TCP 组件；
- 集成升级、USB 测试、Modbus 和日志功能的 WinForms 工具；
- TinyUSB 与 USBX 的两个独立学习对比工程。

当前已完成 Bootloader 第一阶段基础平台：168 MHz 时钟、FreeRTOS/TIM7 双时基分工、USART1 异步日志和运行时信息输出。APP 与 WinForms 工程仍将按路线图逐步建立。

## 当前版本

| 项目 | 内容 |
|---|---|
| 版本 | `v0.2.0` |
| 日期 | `2026-08-08` |
| 阶段 | Bootloader 阶段 1：基础平台与串口日志 |
| 分支 | `main` |
| 仓库 | [ZiulYanG/STM32F407_Codex_prj](https://github.com/ZiulYanG/STM32F407_Codex_prj) |

本版本将目标板运行主频提升并验证为 168 MHz，HAL 使用 TIM7 作为 1 ms 时基，FreeRTOS 独占 SysTick；USART1 固定为 PA9/PA10、115200 8N1，并由一个静态日志任务独占，通过静态队列接收其他模块的日志。上电会打印版本、编译时间、复位原因、各级时钟、时基和 APP 地址。完整验证记录见 [阶段 1 验证报告](docs/verification/01-bootloader-stage-1-platform-bring-up.md)，改动与踩坑见 [CHANGELOG.md](CHANGELOG.md)。

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

## 串口启动日志

调试串口为 USART1：`PA9/TX`、`PA10/RX`、115200 8N1。当前电脑通过独立 USB 转串口连接后枚举为 `COM3`；`COM11` 是 DAPLink 的附带虚拟串口，不用于本工程日志。COM 编号由 Windows 动态分配，换电脑或 USB 口后应以设备管理器显示为准。

复位后应能看到如下关键内容：

```text
STM32F407 Bootloader
Version       : 0.2.0
SYSCLK        : 168000000 Hz
HCLK          : 168000000 Hz
PCLK1         : 42000000 Hz
PCLK2         : 84000000 Hz
HAL timebase  : TIM7
RTOS tick     : 1000 Hz
USART1        : 115200 8N1
APP address   : 0x08040000
```

## 版本记录

| 版本 | 日期 | 实现功能 | 主要改动文件 | 验证 | 踩坑 |
|---|---|---|---|---|---|
| `v0.2.0` | 2026-08-08 | 168 MHz、TIM7 HAL 时基、FreeRTOS SysTick、静态 USART1 日志、启动信息 | `Firmware/Bootloader/Core/`、`app/`、`bsp/`、`components/logging/`、`docs/verification/` | Clean Build、DAPLink 烧录校验、SWD 寄存器回读、COM3 启动报文和 LED 验证通过 | `.ioc` 与生成源码漂移；TIM HAL 宏缺失导致链接失败；误将 COM11 当作板级日志串口 |
| `v0.1.0` | 2026-08-08 | Bootloader 基线、RTOS LED、DAPLink 构建烧录、故障定位 | `Firmware/Bootloader/`、`docs/`、`README.md` | 编译、烧录、复位和 GPIO 寄存器验证通过 | SDIO 无卡阻塞启动；OpenOCD Tcl 路径转义；CubeMX 配置与生成源码漂移 |

每次提交和上传版本必须同步更新本表及 [CHANGELOG.md](CHANGELOG.md)，记录版本、日期、实现功能、改动文件、验证方式和踩坑。具体规则见 [版本与提交规范](docs/development/01-versioning-and-commit-process.md)。

文档入口见 [docs/README.md](docs/README.md)。

# STM32F407 通用固件平台

本仓库用于构建基于 STM32F407ZGT6 的通用固件平台，目标包括：

- 固定 Bootloader 与可升级 APP 两套独立 FreeRTOS 工程；
- W25Q128 暂存、签名验证、断电恢复和失败回滚；
- 静态装配、分层 BSP 与可复用设备驱动；
- USART、USB、CAN、TF 卡及后续网络升级入口；
- Modbus RTU/TCP 组件；
- 集成升级、USB 测试、Modbus 和日志功能的 WinForms 工具；
- TinyUSB 与 USBX 的两个独立学习对比工程。

当前已创建 Bootloader 的 CubeMX/CMake 工程基线；APP 与 WinForms 工程仍将按路线图逐步建立。

## 当前版本

| 项目 | 内容 |
|---|---|
| 版本 | `v0.1.0` |
| 日期 | `2026-08-08` |
| 阶段 | P0：Bootloader 工程基线 |
| 分支 | `main` |
| 仓库 | [ZiulYanG/STM32F407_Codex_prj](https://github.com/ZiulYanG/STM32F407_Codex_prj) |

本版本实现了 Bootloader CubeMX/CMake/FreeRTOS 工程、256 KB 链接分区、PF9/PF10 LED 500 ms 交替闪烁、DAPLink 编译烧录脚本和 OpenOCD 故障定位文档。完整的改动文件、验证记录和踩坑见 [CHANGELOG.md](CHANGELOG.md)。

## 编译与烧录 Bootloader

请从普通 Windows CMD 运行脚本；已确认该终端中的 `python` 可用。Codex 的受限终端可能不会继承系统 Python 的 PATH，不影响你在本机 CMD 中执行。

先在 STM32CubeMX 中打开 `Firmware/Bootloader/Bootloader.ioc`，点击 `GENERATE CODE`。随后在仓库根目录运行 Python 脚本：

```text
python Firmware/Bootloader/script/build.py
```

该命令编译 `Debug` 配置，并在 `Firmware/Bootloader/build/Debug/` 中生成 `.elf`、`.bin` 和 `.hex`。通过 DAPLink 烧录：

```text
python Firmware/Bootloader/script/flash.py
```

脚本默认采用 DAPLink 的 CMSIS-DAP 接口和 1000 kHz SWD 时钟。需要完整重编译可执行 `python Firmware/Bootloader/script/build.py --clean`；连接不稳定时可降速，例如 `python Firmware/Bootloader/script/flash.py --adapter-speed 100`。多把 DAPLink 同时连接时，可用 `--dap-serial` 选择，例如 `python Firmware/Bootloader/script/flash.py --dap-serial B1897547C3840B15B1FE4CDBBF997647`。

## 版本记录

| 版本 | 日期 | 实现功能 | 主要改动文件 | 验证 | 踩坑 |
|---|---|---|---|---|---|
| `v0.1.0` | 2026-08-08 | Bootloader 基线、RTOS LED、DAPLink 构建烧录、故障定位 | `Firmware/Bootloader/`、`docs/`、`README.md` | 编译、烧录、复位和 GPIO 寄存器验证通过 | SDIO 无卡阻塞启动；OpenOCD Tcl 路径转义；CubeMX 配置与生成源码漂移 |

每次提交和上传版本必须同步更新本表及 [CHANGELOG.md](CHANGELOG.md)，记录版本、日期、实现功能、改动文件、验证方式和踩坑。具体规则见 [版本与提交规范](docs/development/01-versioning-and-commit-process.md)。

文档入口见 [docs/README.md](docs/README.md)。

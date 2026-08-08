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

## 编译与烧录 Bootloader

请从普通 Windows CMD 运行脚本；已确认该终端中的 `python` 可用。Codex 的受限终端可能不会继承系统 Python 的 PATH，不影响你在本机 CMD 中执行。

先在 STM32CubeMX 中打开 `firmware/Bootloader/Bootloader.ioc`，点击 `GENERATE CODE`。随后在仓库根目录运行 Python 脚本：

```text
python firmware/Bootloader/script/build.py
```

该命令编译 `Debug` 配置，并在 `firmware/Bootloader/build/Debug/` 中生成 `.elf`、`.bin` 和 `.hex`。通过 DAPLink 烧录时显式加上 `--flash`：

```text
python firmware/Bootloader/script/flash.py
```

脚本默认采用 DAPLink 的 CMSIS-DAP 接口和 1000 kHz SWD 时钟。需要完整重编译可加 `python firmware/Bootloader/script/build.py --clean`；连接不稳定时可降速，例如 `python firmware/Bootloader/script/flash.py --adapter-speed 100`。多把 DAPLink 同时连接时，可用 `--dap-serial` 选择，例如 `python firmware/Bootloader/script/flash.py --dap-serial B1897547C3840B15B1FE4CDBBF997647`。

文档入口见 [docs/README.md](docs/README.md)。

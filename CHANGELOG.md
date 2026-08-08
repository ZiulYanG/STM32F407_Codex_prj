# 修改日志

本文件记录每次提交对应的版本、日期、功能、改动文件、验证结果和踩坑。最新记录位于最上方。

## [v0.1.0] - 2026-08-08

### 版本目标

建立可编译、可烧录、可调试的 STM32F407 FreeRTOS Bootloader 基线，为后续 168 MHz 串口日志、APP 跳转和 IAP 升级提供稳定起点。

### 实现功能

- 创建 STM32F407ZGT6 Bootloader CubeMX/CMake/FreeRTOS 工程；
- 内部 Flash Bootloader 分区限制为 `0x08000000` 起始的 256 KB；
- PF9/PF10 上拉 LED 以低电平点亮，每 500 ms 互补交替；
- 使用 Python 调用 CMake、Ninja 和 GNU Arm 工具链，生成 ELF/BIN/HEX；
- 使用 DAPLink CMSIS-DAP + OpenOCD 完成烧录、校验和复位；
- 建立 `app/`、`bsp/`、`components/` 用户代码分层；
- 建立 PC/LR/Error_Handler 故障定位文档；
- 建立 Git 仓库、忽略规则和 GitHub 远端备份。

### 主要改动文件

- `Firmware/Bootloader/Bootloader.ioc`
- `Firmware/Bootloader/CMakeLists.txt`
- `Firmware/Bootloader/STM32F407ZGTx_FLASH.ld`
- `Firmware/Bootloader/Core/Src/main.c`
- `Firmware/Bootloader/Core/Src/freertos.c`
- `Firmware/Bootloader/app/boot_app.c`
- `Firmware/Bootloader/app/led_task.c`
- `Firmware/Bootloader/bsp/bsp_board.c`
- `Firmware/Bootloader/script/build.py`
- `Firmware/Bootloader/script/flash.py`
- `docs/architecture/01-system-architecture.md`
- `docs/update/01-memory-and-update-design.md`
- `docs/debug/01-openocd-pc-lr-fault-localization.md`
- `.gitignore`
- `README.md`

CubeMX 生成的 `Core/`、`Drivers/`、`Middlewares/` 和启动文件也纳入版本管理，以保证当前基线可重复构建。

### 验证结果

- CMake Debug 编译成功；
- Flash 使用约 20 KB，未超过 256 KB Bootloader 分区；
- DAPLink 识别 STM32F407，烧录完成并显示 `Verified OK`；
- 烧录结束执行目标复位；
- GPIOF MODER 确认 PF9/PF10 为输出；
- GPIOF ODR 在 `0x0400` 和 `0x0200` 之间变化；
- 实物 PF9/PF10 LED 按 500 ms 间隔交替工作。

### 踩坑与解决办法

1. **SDIO 无卡导致系统不启动**
   - 现象：烧录成功但 LED 不闪，PC 始终位于 `Error_Handler()`；
   - 定位：在 `Error_Handler` 设置硬件断点，通过 LR 定位到 `MX_SDIO_SD_Init()`；
   - 处理：当前阶段不在启动路径强制初始化可选 SD 卡，后续由存储模块按需初始化。

2. **OpenOCD 无法打开 Windows ELF 路径**
   - 现象：路径中的反斜杠被 Tcl 吞掉；
   - 处理：转换为正斜杠并使用花括号，例如 `program {D:/.../Bootloader.elf}`。

3. **CubeMX `.ioc` 与生成源码配置漂移**
   - 现象：`.ioc` 为 168 MHz/TIM7，但旧生成源码仍表现为 84 MHz/SysTick；
   - 处理：下一阶段先统一 CubeMX 配置并重新生成，再以串口运行时频率输出验收。

4. **Codex 终端与普通 CMD 的 Python PATH 不同**
   - 现象：普通 CMD 可运行 Python 3.12，Codex 受限终端无法解析 `python`；
   - 处理：用户操作从普通 CMD 执行，自动验证使用工作区 Python 运行时。

5. **GitHub URL 被全局 Git 配置重写到镜像站**
   - 现象：远端显示为 GitHub 代理镜像，可能不能推送；
   - 处理：仅为本仓库添加 `directgh:` 本地映射，保持 GitHub 直连，不修改用户全局配置。

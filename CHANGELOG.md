# 修改日志

本文件记录每次提交对应的版本、日期、功能、改动文件、验证结果和踩坑。最新记录位于最上方。

## [v0.4.0] - 2026-08-11

### 版本目标

完成 P1 基础 BSP 与外部存储，形成可供 USART1 IAP 使用的稳定存储基线。

### 实现功能

- Application 配置 SPI1 PB3/PB4/PB5、PB14 软件片选和 I2C1 PB8/PB9；
- 新增 BSP SPI/I2C 层、NM25Q128EVB/W25Q128 兼容 SPI NOR 驱动和 24C02 驱动；
- 新增 `storage_device + storage_ops` 统一存储接口及两类 Adapter；
- 新增 Candidate、Golden、Metadata A/B 和 Driver Test 有界分区视图；
- APP 专用链接脚本和 Python 脚本共同阻止错误镜像覆盖 Bootloader；
- Debug 执行可恢复的双存储自检，Release 从源码和 ELF 中移除破坏性入口；
- defaultTask、日志任务和日志队列全部静态装配；
- 增加任务栈高水位、动态堆未使用和日志丢弃健康门禁；
- 新增主机存储测试、串口硬件验证和存储循环脚本。

### 主要改动文件

- `Firmware/Application/Application.ioc`
- `Firmware/Application/Core/`
- `Firmware/Application/app/`
- `Firmware/Application/bsp/`
- `Firmware/Application/components/drivers/`
- `Firmware/Application/components/storage/`
- `Firmware/Application/components/logging/`
- `Firmware/Application/linker/`
- `Firmware/Application/script/`
- `Firmware/Application/tests/storage/`
- `docs/hardware/03-w25q128-pin-and-ioc-audit.md`
- `docs/architecture/02-linux-inspired-storage-driver-model.md`
- `docs/verification/03-application-spi-nor-jedec.md` 至 `10-p1-exit-storage-rtos-soak.md`
- `PROJECT_HANDOFF.md`

### 验证结果

- 主机 GCC 严格警告模式下统一存储与分区测试 PASS；
- Debug APP 范围 `0x08040000-0x0804AF4F`，双存储全部正向/负向测试 PASS；
- Release APP 范围 `0x08040000-0x08047727`，破坏性自检均 DISABLED；
- Debug 栈余量：defaultTask 313 words、app_log 428 words；
- Release 栈余量：defaultTask 324 words、app_log 437 words；
- 两配置均显示应用 RTOS 对象 STATIC、heap UNUSED、health PASS、log drops 0；
- 存储循环累计 100/100 轮 PASS，验证上轮清理状态跨复位保持；
- 当前板卡 JEDEC ID `52 21 18`，按 NM25Q128EVB 兼容料识别。

### 踩坑与解决办法

1. CubeMX 再生成会恢复整片内部 Flash 链接脚本，现将 APP 专用脚本移出生成区并增加 ELF/HEX 地址门禁。
2. CubeMX 曾把默认任务栈从 512 words 回退至 128 words，已同步修正 `.ioc` 与源码并加入运行时高水位检查。
3. Flash 最末扇区“地址+1、擦4 KiB”同时越界和未对齐，统一层按顺序返回 RANGE；测试改成单一约束后语义明确。
4. `heap_4` 从未分配时余量接口返回 `0/0`，并非耗尽；P1 将该状态定义为 `UNUSED`，任何首次动态分配都会变为 `TOUCHED` 并判 FAIL。
5. 实物 JEDEC ID 与原理图 Winbond 标称不同；驱动型号表同时支持实测兼容料和 W25Q128。

## [v0.3.0] - 2026-08-10

### 版本目标

建立 Bootloader 到独立 Application 的最小可信启动闭环，验证有效 APP 跳转、无效 APP 拒跳和恢复后再次跳转。

### 实现功能

- 新增 `Firmware/Application` 独立 CubeMX/CMake/FreeRTOS 工程；
- APP 内部 Flash 链接区固定为 `0x08040000` 起始、768 KB 长度；
- APP 编译时定义 `USER_VECT_TAB_ADDRESS` 和 `VECT_TAB_OFFSET=0x00040000U`；
- APP 使用 168 MHz、TIM7 HAL tick、FreeRTOS SysTick、USART1 PA9/PA10 和 PF9/PF10；
- APP 启动输出版本、各级时钟、VTOR、时基、串口和运行状态；
- Bootloader 校验 APP 向量表对齐、初始 MSP 范围/对齐和 Thumb Reset_Handler 地址；
- 跳转前刷新串口日志、关闭 SysTick、复位 TIM7/USART1、清理 NVIC pending/enable 状态；
- 切换 VTOR、CONTROL 和 MSP 后保持 PRIMASK，交由 APP FreeRTOS 启动代码安全开中断；
- 无效 APP 时输出具体原因并留在 Bootloader；
- Bootloader 默认控制任务栈由 512 B 提升到 2 KB，并同步 `.ioc`；
- Bootloader 与 Application 正式版本统一为 `0.3.0`。

### 主要改动文件

- `Firmware/Application/`
- `Firmware/Bootloader/Bootloader.ioc`
- `Firmware/Bootloader/CMakeLists.txt`
- `Firmware/Bootloader/Core/Src/freertos.c`
- `Firmware/Bootloader/app/boot_app.c`
- `Firmware/Bootloader/app/boot_version.h`
- `Firmware/Bootloader/components/boot/app_launcher.c`
- `Firmware/Bootloader/components/boot/app_launcher.h`
- `Firmware/Bootloader/components/logging/boot_log.c`
- `Firmware/Bootloader/components/logging/boot_log.h`
- `docs/verification/02-bootloader-application-jump.md`
- `docs/README.md`
- `README.md`
- `CHANGELOG.md`

### 验证结果

- Bootloader Clean Build 成功，Debug ELF 使用 Flash 28,296 B、静态 RAM 30,056 B；
- Application Clean Build 成功，Debug ELF 使用 Flash 25,632 B、静态 RAM 29,816 B；
- ELF 检查确认 APP `.isr_vector=0x08040000`、MSP=`0x20020000`、Reset_Handler=`0x080456E5`；
- DAPLink CMSIS-DAP 以 100 kHz 完成两套 ELF 的烧录、校验和复位；
- COM3 捕获 Bootloader `APP check : VALID` 和 `JUMP TO APPLICATION`；
- COM3 随后捕获 Application `Vector table : 0x08040000` 和 168/168/42/84 MHz；
- SWD 运行态读取 PC=`0x08044208`、VTOR=`0x08040000`、CFSR/HFSR=`0`；
- 临时擦除 Sector 6 后向量为 `0xFFFFFFFF/0xFFFFFFFF`，Bootloader 输出 `INVALID (initial MSP)` 并留在 Bootloader；
- 重新烧录 Application 后再次完成有效校验和跳转，硬件恢复正常运行。

### 踩坑与解决办法

1. **默认任务栈溢出破坏 FreeRTOS Timer 栈**
   - 现象：Bootloader 无新日志，CPU 进入 HardFault，CFSR=`0x00020000`（INVSTATE）；
   - 定位：在 HardFault 入口断点读取异常栈，发现 PendSV 恢复的 LR 从 `0xFFFFFFFD` 变为 RAM 地址；再在 SVC 入口观察 Timer 栈原本有效，确认是在 `boot_app_process()` 运行后被覆盖；
   - 根因：`defaultTask` 只有 512 B，却在任务上下文调用多次 `vsnprintf`，栈向下溢出到相邻的静态 Timer 栈；
   - 处理：将默认控制任务栈提升为 2 KB，并同步 `Bootloader.ioc`，Clean Build 后故障消失。

2. **跳转交接窗口过早打开中断**
   - 现象：最初跳转汇编在切换 VTOR/MSP 后立即执行 `cpsie i`，存在新旧 RTOS 上下文交界处响应异常的风险；
   - 处理：跳转时保持 PRIMASK，APP 的 FreeRTOS `prvPortStartFirstTask()` 在建立自身 MSP 后负责开中断。

3. **烧录时捕获到旧版本日志**
   - 现象：烧录新版 ELF 时先看到旧版 `0.2.0`；
   - 根因：OpenOCD 编程前连接/复位阶段让旧固件短暂运行；
   - 处理：同时检查 ELF 内嵌字符串、烧录校验结果，并在烧录完成后单独复位抓取日志。

4. **CH340 间歇掉线与 Code 10**
   - 现象：COM3 消失或显示 `CM_PROB_FAILED_START`，Python 报 `Could not find file 'COM3'`；
   - 处理：用串口 API、注册表和 `pnputil` 区分 COM3 CH340 与 COM11 DAPLink，重新连接 CH340 后确认状态为 `Started` 再测试。

## [v0.2.0] - 2026-08-08

### 版本目标

完成 Bootloader 第一阶段基础平台，使时钟、RTOS/HAL 时基、板级调试串口和异步日志具备可观测、可重复验证的运行基线。

### 实现功能

- 使用 8 MHz HSE 和 PLL 将 SYSCLK/HCLK 配置为 168 MHz；
- APB1 配置为 42 MHz，APB2 配置为 84 MHz，Flash 等待周期配置为 5 WS；
- TIM7 以 1 kHz 中断维护 HAL tick，优先级为 0；
- SysTick 仅维护 FreeRTOS 1 kHz tick，优先级为 15；
- USART1 固定使用 PA9/TX、PA10/RX、115200 8N1；
- 新增静态日志任务和静态消息队列，只有日志任务能够访问 USART1 BSP；
- 启动时输出 Bootloader 版本、构建时间、复位原因、运行时钟、时基、串口参数和 APP 地址；
- 保留 PF9/PF10 低电平点亮、每 500 ms 交替的 FreeRTOS LED 验证任务；
- 将版本号集中到 `boot_version.h`，当前版本为 `0.2.0`；
- 新增阶段 1 软硬件验收和复现文档。

### 主要改动文件

- `Firmware/Bootloader/CMakeLists.txt`
- `Firmware/Bootloader/Core/Inc/stm32f4xx_hal_conf.h`
- `Firmware/Bootloader/Core/Src/main.c`
- `Firmware/Bootloader/Core/Src/freertos.c`
- `Firmware/Bootloader/Core/Src/stm32f4xx_it.c`
- `Firmware/Bootloader/Core/Src/stm32f4xx_hal_timebase_tim.c`
- `Firmware/Bootloader/Core/Src/usart.c`
- `Firmware/Bootloader/app/boot_app.c`
- `Firmware/Bootloader/app/boot_version.h`
- `Firmware/Bootloader/bsp/bsp_board.c`
- `Firmware/Bootloader/bsp/bsp_uart1.c`
- `Firmware/Bootloader/components/logging/boot_log.c`
- `docs/verification/01-bootloader-stage-1-platform-bring-up.md`
- `README.md`
- `CHANGELOG.md`

### 验证结果

- `python script/build.py --clean` 编译成功并生成 ELF/BIN/HEX；
- Debug ELF 使用 Flash 26,468 B、RAM 29,904 B，未超过 Bootloader 分区；
- DAPLink CMSIS-DAP 烧录完成，OpenOCD 显示 `Verified OK` 并复位运行；
- SWD 回读 `RCC_CFGR=0x0000940A`，确认 PLL、AHB/1、APB1/4、APB2/2；
- `SystemCoreClock=168000000`，Flash ACR latency 为 5；
- TIM7 的 PSC=83、ARR=999、CR1.CEN=1、DIER.UIE=1；
- SysTick LOAD=167999，TIM7 优先级为 0，SysTick 优先级为 15；
- USART1 BRR=`0x02D9`，GPIOA PA9/PA10 均为 AF7；
- HAL tick 与 FreeRTOS tick 均持续递增，日志队列和日志任务句柄有效，丢弃计数为 0；
- GPIOF ODR 观察到 `0x0400` 与 `0x0200` 状态，LED 任务持续运行；
- COM3 在 115200 8N1 下捕获完整启动报文，运行时打印的时钟为 168/168/42/84 MHz，并正确识别 Pin reset 与 Software reset。

### 踩坑与解决办法

1. **`.ioc` 与生成源码不一致**
   - 现象：`.ioc` 已配置 168 MHz、TIM7 和 PA10，但源码仍为 84 MHz、SysTick HAL tick 和 PB7 RX；
   - 根因：旧生成代码未与最新 `.ioc` 同步，CubeMX 无界面生成在当前环境中超时；
   - 处理：逐项审计并同步时钟、时基和引脚源码，通过运行时寄存器回读验证，避免只相信 `.ioc`。

2. **TIM7 API 链接失败**
   - 现象：`HAL_TIM_Base_Init`、`HAL_TIM_Base_Start_IT` 和 `HAL_TIM_IRQHandler` 未定义；
   - 根因：`stm32f4xx_hal_conf.h` 未启用 `HAL_TIM_MODULE_ENABLED`，定时器驱动 API 被条件编译掉；
   - 处理：启用 TIM HAL 模块并重新 Clean Build。

3. **误用 DAPLink 虚拟串口**
   - 现象：COM11 可以打开但收不到 PA9 启动报文；
   - 根因：COM11 属于 DAPLink 虚拟串口，板级 PA9/PA10 实际通过独立 USB 转串口枚举为 COM3；
   - 处理：固件日志验收固定使用当前机器的 COM3；换环境时按实际 USB 转串口设备重新确认端口号。

4. **CubeMX 重新生成存在覆盖风险**
   - 现象：直接重新生成可能恢复可选 SDIO 的启动初始化、时基或用户 CMake 配置；
   - 处理：日常构建不需要重新生成；修改 `.ioc` 后必须审查 Git diff 并重新执行完整硬件验收。

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

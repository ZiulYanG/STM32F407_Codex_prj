# STM32F407 项目新对话交接摘要

更新时间：2026-08-11
当前版本：`v0.5.0`
当前分支：`main`
远端仓库：<https://github.com/ZiulYanG/STM32F407_Codex_prj>

## 0. 工作进度同步区

后续接手者必须先读取本节，再结合后文和仓库实际状态开展工作。

| 项目 | 当前状态 |
|---|---|
| 最近同步日期 | 2026-08-11 |
| 基线提交 | `v0.5.0`（以该标签指向的Git提交为准） |
| 已发布标签 | `v0.5.0` |
| 已完成阶段 | P0：双工程与安全跳转；P1：基础BSP与存储；P5基础USART1 YMODEM文件运输闭环 |
| 当前阶段 | P5：USART1基础IAP |
| 最近完成 | P5-4命令触发YMODEM会话、运行模式协作、Candidate 2500B双向硬件回环和CAN取消恢复 |
| 当前任务 | v0.5.0基础文件运输闭环已验证并完成版本归档 |
| 下一项单一交付 | P5-5定义文件Manifest并实现整文件CRC32/文件类型/长度校验，成功后才产生Candidate READY状态 |
| 硬件验证状态 | COM3 PC→Candidate→PC 2500B逐字节PASS；SYSTEM/SERIAL模式1、LED关闭经SWD确认；取消后NORMAL；drop/error=0，Console/Serial/Update栈176/416/266 words |
| 阻塞项 | DAPLink/软件复位后曾进入`0x1FFF3DA0`系统Boot ROM，需检查BOOT0跳线/下拉；Bootloader自身PB14默认低和SPI1 42 MHz仍待整改 |

### 进度同步要求

每次出现实质性进展时，在结束当前任务前更新本文件：

1. 更新“最近同步日期”“当前任务”和“下一项单一交付”；
2. 只有经过构建、烧录、串口或 SWD 等相应验证后，才能把功能写入“已完成阶段”；
3. 将新确认的硬件连接、架构决策、验证证据、故障根因和遗留风险写入对应章节；
4. 如果仓库现状与本文冲突，以源码、`.ioc`、硬件实测和 Git 记录为依据，并立即修正文档；
5. 保留仍影响后续工作的结论，删除已经失效的临时状态，避免文档无限堆积；
6. 版本发布时同步更新 `README.md`、`CHANGELOG.md`、版本号、提交和标签信息；
7. 对话记忆不能替代本文件；新对话必须以本文件和仓库内容恢复上下文。

## 1. 项目目标

基于 STM32F407ZGT6 建立一套可学习、可扩展并贴近企业项目的通用固件平台：

- Bootloader 与 Application 使用两套相互独立的 CubeMX/CMake 工程；
- Bootloader 和 Application 统一使用 FreeRTOS；
- MCU 代码使用 C，PC 工具使用 WinForms；
- 固件支持串口起步，并为 USB、CAN、TF 卡、以太网、Wi-Fi、4G 等升级入口预留适配层；
- 引入 Modbus RTU/TCP 组件；
- TinyUSB 与 USBX 分成两个独立学习工程，对相同 USB CDC 功能进行比较，不做运行时切换；
- 驱动采用 Platform、BSP、Device Driver、Service、Application 分层，驱动本身不创建任务；
- 首版从简单、可验证的闭环开始，逐阶段迭代。

## 2. 已确定的升级策略

- APP 负责复杂通信入口、远程下载和将完整 Candidate 镜像写入外部 Flash；
- Bootloader 负责最小恢复入口、镜像校验、版本策略和安装 APP；
- Bootloader 尽量保持固定且不可被普通升级流程改写；
- Candidate 完整接收并验证后才能进入安装流程；
- 使用双副本 Metadata、状态机和幂等操作实现安装阶段断电恢复；
- 正常情况下禁止固件降级；
- 紧急回退必须使用独立的专用授权签名，普通发布签名无权绕过防降级规则；
- 后续实现 SHA-256、ECDSA-P256、TRIAL/CONFIRMED、失败回滚和 Golden 恢复策略；
- 已同意先实现串口升级，其余接口只预留统一传输入口。

## 3. 硬件和基础配置

| 项目 | 当前约定或验证结果 |
|---|---|
| MCU | STM32F407ZGT6，1 MB Flash，128 KB 主 SRAM，64 KB CCM |
| 核心板 | 正点原子 STM32F407 核心板 V1.4 |
| 系统时钟 | SYSCLK/HCLK 168 MHz，PCLK1 42 MHz，PCLK2 84 MHz |
| HAL 时基 | TIM7，1 kHz，中断优先级 0 |
| RTOS Tick | SysTick，1 kHz，中断优先级 15 |
| 日志串口 | USART1，PA9/TX、PA10/RX，115200 8N1 |
| PC 串口 | 当前机器为 COM3；COM11 是 DAPLink 虚拟串口，不用于 PA9/PA10 日志 |
| 调试器 | DAPLink / CMSIS-DAP，设备名 `PW_WINUSB_CMSIS-DAP` |
| DAP 序列号 | `B1897547C3840B15B1FE4CDBBF997647` |
| 指示灯 | PF9、PF10，低电平点亮，已验证 500 ms 交替闪烁 |
| USB | OTG FS Device 方向，后续分别建立 TinyUSB 和 USBX 工程 |

外部 SPI Flash 在原理图和项目最初约定中为 `W25Q128`。2026-08-10 已读取用户资料包中的 `STM32F407_CORE_BOARD_V1.4.pdf`，并对相关页面进行渲染核验：SPI1 使用 PB3/SCK、PB4/MISO、PB5/MOSI，软件片选 `/CS` 使用 PB14；`/WP` 和 `/HOLD` 固定接 3.3 V，不占 MCU IO。PB3/PB4 与完整 JTAG 复用，因此两套工程必须保持 `Serial Wire`。原理图 SHA-256 为 `2B15810ABBC471B3B4BECFB7D97AB8DA94F68DFBE3AA58050623D42EB808F6FC`，详见 `docs/hardware/03-w25q128-pin-and-ioc-audit.md`。

2026-08-11 实机读取到 JEDEC ID `52 21 18`，而不是 Winbond W25Q128 的 `EF 40 18`。公开 Flash 支持列表将 `52-21-18` 对应为 `NM25Q128EVB`；因此当前固件按“板卡发生兼容换料”处理，并在通用 `spi_nor` 型号表中同时支持两者。具体丝印仍需物理查看后最终确认，但 SPI 通信和 128 Mbit 容量码已经连续两次实机验证。

该 PDF 文件名标为 V1.4，但第 1～3 页标题栏均标为 V1.0、日期 2021/7/27；文件内部版本差异已经确认，不影响 W25Q128 网络结论，实际 PCB 版本仍待通过板卡丝印复核。

Bootloader生成代码目前仍把PB14初始拉低，且SPI1为42 MHz，后续在Bootloader接入外部Flash前必须整改。Application已正确生成SPI1、PB3/PB4/PB5、PB14默认高、`F_CS`标签和HAL SPI，实机速率为16分频约5.25 MHz。APP专用链接脚本现位于`Firmware/Application/linker/STM32F407ZGTx_APP_FLASH.ld`，CMake不再引用CubeMX可能重建的整片Flash脚本；构建和烧录脚本还会校验ELF/HEX必须从`0x08040000`开始且不超过`0x080FFFFF`。2026-08-11 SPI NOR版本Clean Build确认镜像范围为`0x08040000–0x0804774B`，并通过DAPLink烧录及COM3连续两次验证。`24C02`后续可在CubeMX配好对应I2C外设后手工编写驱动，不需要CubeMX提供专用器件组件。

24C02已确认使用I2C1 PB8/SCL、PB9/SDA、100 kHz，板载SCL/SDA各有4.7 kΩ上拉；A0/A1/A2接地形成7位地址`0x50`，WP接地允许写入。驱动按`bsp_i2c1`与无HAL依赖的`eeprom_24c02`两层实现，支持容量检查、8字节页边界检查、自动跨页写和写周期ACK轮询。CubeMX重生成曾将Application默认任务栈从512 words回退到128 words；本次已同步修复`.ioc`与生成代码为512 words（2 KB）。

24C02 Debug启动自测试使用`0xF6–0xFF`共10字节：先备份原值，再跨8字节页写入、回读、执行越界和非法单页写拒绝测试，最后恢复并回读确认原值。Release及其他非Debug构建会编译移除该写测试，只保留地址探测；构建脚本同时检查宏、ELF标记和测试函数符号。2026-08-11 Debug Clean Build范围为`0x08040000–0x08049F13`，所有EEPROM测试和原值恢复PASS；Release范围为`0x08040000–0x080468AF`，实机只打印`EEPROM probe : PASS`和`EEPROM self-test: DISABLED`。详见`docs/verification/07-application-eeprom-24c02.md`。

统一存储层已采用Linux式“数据对象+操作表+私有上下文”模型：`storage_device`通过`storage_ops`暴露open/read/write/erase/sync/close，SPI NOR和24C02由独立Adapter接入。统一层静态分配，集中检查生命周期、能力、容量范围和擦除对齐；SPI NOR显式声明写前需擦除，24C02不支持erase。当前不引入VFS、文件描述符、动态注册或动态内存，也暂不内置锁；多任务并发出现时应在设备服务/总线层统一加FreeRTOS mutex。主机接口测试、Debug全量实机测试和Release非破坏性启动均通过，详见`docs/architecture/02-linux-inspired-storage-driver-model.md`与`docs/verification/08-unified-storage-interface.md`。

`storage_partition`现将父`storage_device`转换为有界分区视图：分区相对地址在转发前检查，bind拒绝父设备越界和擦除不对齐定义，关闭视图不关闭共享父设备。Application静态装配Candidate、Golden、Metadata A/B及Driver Test；布局带编译期不重叠/末地址断言。Debug自检已改为只持有4 KiB Driver Test视图，实机全部PASS；Release分区READY且自检关闭。详见`docs/verification/09-storage-partitions.md`。

SPI NOR最小擦写接口已加入容量边界、页边界、4 KB对齐、WEL确认和Busy超时保护。Reserved区最后一个4 KB扇区`0x00FFF000–0x00FFFFFF`已固定为开发测试扇区；启动自测试写入64字节模式数据，完成回读比较后再次擦除并确认前64字节均为`0xFF`。2026-08-11 Clean Build镜像范围为`0x08040000–0x08047F4F`，两次独立DAPLink/COM3验收全部PASS。该启动擦写属于开发阶段行为，发布版必须关闭。

自动分页接口`spi_nor_write()`会按当前页剩余空间拆分Page Program，并在开始前一次性校验完整地址范围。实机已验证从`0x00FFF0F0`写64字节可跨越页边界，且从`0x00FFFFC0`写64字节恰好结束于Flash末字节；跨页调用底层单页接口、末地址加2字节的读写、非4 KB对齐擦除均被拒绝。Clean Build镜像范围更新为`0x08040000–0x0804829B`，正式修复后连续两次全部PASS。

破坏性启动自测试现由CMake按构建类型强制控制：Debug定义`APP_ENABLE_SPI_NOR_SELF_TEST=1`，所有非Debug配置定义为0。`build.py`会检查`app_main.c`编译参数、ELF标记字符串和`app_flash_run_rw_test`符号，防止Release误带自测试入口。Debug Clean Build范围为`0x08040000–0x080482CB`并通过完整擦写验收；Release范围为`0x08040000–0x080457B7`，ELF门禁确认自测试符号不存在，实机只打印`SPI NOR self-test: DISABLED`。当前开发板最后烧录的是Release镜像。

## 4. 固件内存布局

| 固件 | 起始地址 | 长度 | 说明 |
|---|---:|---:|---|
| Bootloader | `0x08000000` | 256 KB | Sector 0～5 |
| Application | `0x08040000` | 768 KB | Sector 6～11 |

Application 的链接地址和向量表均为 `0x08040000`。Bootloader 当前会检查 APP 向量表对齐、初始 MSP 范围与对齐、Reset_Handler Thumb 位及地址范围。

## 5. 当前已完成状态

`v0.4.0` 已完成 P0/P1 并通过实机验证：

- Bootloader 与 Application 两套独立 FreeRTOS/CMake 工程；
- 两套工程均能用 Python 脚本 Clean Build，生成 ELF/BIN/HEX；
- DAPLink + OpenOCD 能完成烧录、校验、复位运行；
- Bootloader 输出 168 MHz、TIM7、FreeRTOS、USART1 和 APP 地址等启动信息；
- Application 固定链接到 `0x08040000` 并正确重定位 VTOR；
- 有效 APP：Bootloader 校验后安全跳转，APP 正常启动；
- 无效 APP：擦除 Sector 6 后 Bootloader 拒绝跳转并驻留；
- 恢复 APP：重新烧录后再次正常跳转；
- 运行态确认 PC 位于 `0x0804xxxx`、VTOR=`0x08040000`、CFSR/HFSR 均为 0；
- GitHub 远端 `main` 和标签 `v0.3.0` 均指向提交 `c6a7406ed4315e07713dfcf433619ad328f51caf`。
- P1 存储与 RTOS 资源基线将在本次提交后形成 `v0.4.0` 标签。

## 6. 构建、烧录和观察日志

在普通 Windows CMD 的仓库根目录运行：

```text
python Firmware/Bootloader/script/build.py --clean
python Firmware/Application/script/build.py --clean

python Firmware/Application/script/flash.py --adapter-speed 100 --dap-serial B1897547C3840B15B1FE4CDBBF997647
python Firmware/Bootloader/script/flash.py --adapter-speed 100 --dap-serial B1897547C3840B15B1FE4CDBBF997647
```

串口助手设置：COM3、115200、8 数据位、无校验、1 停止位、无流控。烧录 APP 后再烧录 Bootloader，复位后应依次看到 Bootloader 校验/跳转日志和 Application 启动日志。

## 7. 已踩过的重要问题

1. CubeMX `.ioc` 和生成源码可能漂移；修改 `.ioc` 并重新生成后必须检查 Git diff，再进行完整构建和硬件验收。
2. SDIO 无卡时曾在启动阶段进入 `Error_Handler()`；可选外设不应在基础启动路径中被强制初始化。
3. OpenOCD 的 Windows 路径必须转换为正斜杠并用 Tcl 花括号保护，否则反斜杠会被吞掉。
4. COM11 属于 DAPLink 虚拟串口，PA9/PA10 的日志通过独立 USB 转串口 COM3 接收。
5. Bootloader 默认任务原 512 B 栈被 `vsnprintf()` 压穿，并覆盖相邻 FreeRTOS Timer 栈，造成 `INVSTATE` HardFault；现已将默认任务栈调整为 2 KB。
6. Bootloader 跳转 APP 时保持 PRIMASK，APP必须在入口用户代码区显式执行`__enable_irq()`后再调用`HAL_Init()`；否则调度器启动前调用依赖TIM7的`HAL_Delay()`会永久阻塞。
7. GitHub 曾受全局 URL 镜像重写和网络复位影响；本仓库使用局部 `directgh:` 映射，最近一次推送及远端校验已成功。
8. CubeMX重新生成Application时会把默认链接脚本恢复为整片Flash；现已将APP专用链接脚本移出生成区，并在构建和烧录脚本增加地址边界校验，禁止错误镜像覆盖Bootloader。
9. SPI NOR开发擦写只能使用`0x00FFF000`测试扇区；自测试结束必须再次擦除。不得用Candidate、Golden、Metadata、Event Log或Crash Dump区域做驱动测试。
10. APP启动日志曾在第24条后消失；根因是`app_main_init()`在调度器启动前一次性填满24深度队列。临时扩到32后原脚本立即转绿，确认不是Flash卡死。正式修复为调度器启动后在默认任务执行初始化、队列满时最多等待100 ms，并将默认任务栈从512 B同步提升到2 KB；回归日志`APP log drops : 0`。
11. 破坏性硬件自测试不能只依靠运行时`if`或人工约定关闭；当前由构建类型定义宏，并由ELF级策略检查阻止Release包含自测试入口。
12. 2026-08-11 DAPLink 后续复位偶发使 PC 位于`0x1FFF3DA0`，即STM32系统Boot ROM；这不是Console代码地址。应检查BOOT0跳线和下拉是否稳定。测试中曾临时整片擦除内部Flash，随后Bootloader/Application Release均已重新烧录并Verify；外部Flash未改动。
13. Console到YMODEM切换初版触发FreeRTOS StreamBuffer单Reader断言：Console仍阻塞读取时Update任务成为第二Reader。Serial Manager现使用非阻塞短轮询并逐轮复核模式，旧Reader先退出后新Reader接管；2500B硬件回环通过。
14. Update任务768 words栈在完整会话后只剩10 words；已提升至1024 words，最终余量266 words。

## 8. 下一阶段建议

P0 与 P1 已完成。下一对话进入 P5 USART1 IAP，并仍采用“一步实现、一步构建、一步烧录、一步串口或 SWD 验证”的节奏：

1. 已完成 W25Q128 原理图连接、`.ioc` 和 IO 冲突审计；
2. 修正两套 `.ioc` 及生成代码，并完成 diff 审查和 Clean Build；
3. 已建立 SPI BSP 和无任务、无动态内存的通用 SPI NOR 驱动；
4. 已通过USART1完成JEDEC ID实机闭环；板载实测为`52 21 18`，匹配`NM25Q128EVB`兼容项；
5. 已在`0x00FFF000`开发测试扇区完成页编程、4 KB擦除、回读比较和最终清理，两次实机验收通过；
6. 已完成自动跨页写入与越界、非对齐等负向边界测试；
7. 已完成破坏性启动自测试Debug/Release编译策略，Release默认禁用并通过ELF及实机验证；
8. 已完成24C02/I2C原理图、CubeMX配置和默认任务栈回退审计；
9. 已完成I2C BSP、24C02容量/页边界/跨页驱动、Debug/Release策略和实机验收；
10. 已完成统一存储接口、两类Adapter、能力/范围/对齐门禁、主机测试和实机验收；
11. 已完成轻量存储分区视图及Candidate、Golden、Metadata A/B、Driver Test静态隔离；
12. 已完成任务栈高水位、heap未使用监测、跨复位稳定态和长时间循环测试；
13. 已完成静态RTOS对象、栈高水位/heap未使用门禁和累计100轮存储循环；P1验收通过；
14. 已完成P5-1 USART1 Serial Manager：只有该静态任务可调用UART BSP写接口，日志走24深度普通队列，协议控制字节预留8深度高优先级队列；USART1 IRQ优先级5，BSP使用2048字节SPSC环形缓冲，Manager使用2048字节RX StreamBuffer；
15. Release实测串口任务剩余422 words，heap保持UNUSED；COM3注入256字节后SWD读取`rx_bytes=256`、`rx_dropped=0`、硬件overrun/error=0；
16. 已完成无HAL/RTOS/存储依赖的YMODEM-1K RX/TX状态机及主机测试，覆盖正常传输、CRC错误、ACK丢失、取消和EOT；
17. 已完成`ymodem_storage` Source/Sink Adapter：每进入新4KB区域按需擦除，YMODEM的1KB写由SPI NOR驱动拆成4次256B Page Program，不使用额外4KB缓存；
18. 已完成共享命令解析器和Boot/App独立Console：协议应答走高优先级队列，任务日志走普通队列，支持查询、运行时参数、分级日志、复位以及Boot驻留/跳转；两套Console均通过COM3实测；
19. 已完成Application静态YMODEM会话任务、Candidate绑定、Console/YMODEM模式切换和系统模式协作；
20. COM3已将2500B模式数据接收到Candidate再发送回PC并逐字节PASS；双CAN可取消并恢复Console/NORMAL；
21. 当前只能称为文件运输闭环，不能称为可信升级闭环；下一步先实现Manifest和整文件CRC32/类型/长度校验；
22. 写入中随机断电、双副本Metadata、版本签名和回滚仍属于后续P5/P6，不得提前宣称完成。

不要在这一阶段同时展开 Modbus、USB 和 IAP，以免硬件驱动问题与协议问题互相干扰。

## 9. 新对话启动提示词

新建一个以当前仓库为工作区的 Codex 对话，然后发送：

```text
请先完整阅读根目录 PROJECT_HANDOFF.md，并遵守根目录 AGENTS.md。v0.5.0已完成USART1命令触发的YMODEM Candidate双向文件运输闭环。下一步只定义文件Manifest并实现整文件CRC32、文件类型和长度校验，只有验证通过才能写Candidate READY；暂不同时展开签名、回滚、USB或Modbus。每一步先主机测试和构建，再烧录，再用COM3或SWD验证。
```

## 10. 关键文档

- `README.md`
- `CHANGELOG.md`
- `docs/architecture/01-system-architecture.md`
- `docs/architecture/02-linux-inspired-storage-driver-model.md`
- `docs/architecture/03-serial-manager-and-protocol-seam.md`
- `docs/roadmap/01-implementation-roadmap.md`
- `docs/hardware/01-io-allocation.md`
- `docs/hardware/02-cubemx-baseline.md`
- `docs/hardware/03-w25q128-pin-and-ioc-audit.md`
- `docs/update/01-memory-and-update-design.md`
- `docs/verification/01-bootloader-stage-1-platform-bring-up.md`
- `docs/verification/02-bootloader-application-jump.md`
- `docs/verification/03-application-spi-nor-jedec.md`
- `docs/verification/04-application-spi-nor-read-write.md`
- `docs/verification/05-application-spi-nor-boundaries.md`
- `docs/verification/06-spi-nor-self-test-build-policy.md`
- `docs/verification/07-application-eeprom-24c02.md`
- `docs/verification/08-unified-storage-interface.md`
- `docs/verification/09-storage-partitions.md`
- `docs/verification/10-p1-exit-storage-rtos-soak.md`
- `docs/verification/11-p5-serial-manager.md`
- `docs/architecture/04-ymodem-and-console.md`
- `docs/verification/12-p5-ymodem-console.md`
- `docs/verification/13-p5-ymodem-session.md`
- `docs/debug/01-openocd-pc-lr-fault-localization.md`

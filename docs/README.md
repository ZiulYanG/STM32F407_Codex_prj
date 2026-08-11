# 文档索引

## 基线文档

| 文档 | 内容 |
|---|---|
| [系统架构](architecture/01-system-architecture.md) | 工程边界、软件分层、任务模型和仓库结构 |
| [Linux 风格统一存储模型](architecture/02-linux-inspired-storage-driver-model.md) | storage_device/ops、Adapter、能力语义和并发边界 |
| [IO 分配](hardware/01-io-allocation.md) | 板载固定资源、首版启用引脚、扩展预留和复用冲突 |
| [IO 分配 CSV](hardware/io-allocation.csv) | 可排序、可供脚本检查的引脚分配源数据 |
| [CubeMX 基线](hardware/02-cubemx-baseline.md) | 时钟、调试、RTOS 时基和首版外设参数 |
| [存储与升级设计](update/01-memory-and-update-design.md) | 内外部 Flash 分区、镜像格式、升级状态机和掉电恢复 |
| [实施路线图](roadmap/01-implementation-roadmap.md) | 小步迭代阶段、交付物和验收条件 |
| [OpenOCD PC/LR 故障定位](debug/01-openocd-pc-lr-fault-localization.md) | 读取 PC、断点捕获 Error_Handler、使用 LR 追溯故障调用者 |
| [版本与提交规范](development/01-versioning-and-commit-process.md) | 版本号、提交说明、修改日志、验证和踩坑记录规则 |
| [Bootloader 阶段 1 验证](verification/01-bootloader-stage-1-platform-bring-up.md) | 168 MHz、双时基、USART1 日志、SWD 回读和 COM3 端到端验收 |
| [Bootloader 阶段 2 验证](verification/02-bootloader-application-jump.md) | APP 分区、向量校验、安全跳转、无效镜像驻留和栈溢出定位 |
| [统一存储接口验证](verification/08-unified-storage-interface.md) | 主机测试、Debug/Release 构建与双存储实机验收 |
| [外部 Flash 分区验证](verification/09-storage-partitions.md) | 分区地址隔离、父设备生命周期、Debug/Release 实机验收 |
| [P1 阶段验收](verification/10-p1-exit-storage-rtos-soak.md) | 静态 RTOS 资源、100 轮存储循环和 P1 退出结论 |

## 已确认的工程决策

- MCU 为 STM32F407ZGT6，固件使用 C。
- Bootloader 和 APP 是两套独立工程，均使用 FreeRTOS。
- 使用单仓库管理双工程、公共组件、示例和 PC 工具。
- Bootloader 固定且量产后不远程更新；维护使用 SWD 或芯片 ROM Bootloader。
- APP 使用内部单运行区；W25Q128 保存 Candidate、Golden 和事务记录。
- 正常远程下载由 APP 负责，Bootloader 负责可信安装和最小恢复。
- 第一版升级闭环使用 USART1；USB、TF 卡、CAN 逐步加入。
- 固件强制 SHA-256 与 ECDSA-P256 验证，默认防降级。
- 第一版只支持完整、未压缩、已签名但不加密的 APP 镜像。
- 驱动以静态装配为主，业务层不得直接依赖 CubeMX 全局 HAL 句柄。
- Bootloader 全静态分配；APP 初始化后不使用通用堆。
- Modbus 组件支持 Client/Server，首个练习实现 USART1 RTU Server。
- TinyUSB 和 USBX 分别建立独立 CDC 学习工程，完成对比后再选择正式 USB 栈。
- PC 端使用一个 C# WinForms 工具集成多个模块。

## 硬件依据

当前 IO 基线来自：

`D:\BaiduNetdiskDownload\【正点原子】STM32F407最小系统板资料\3，STM32F407最小系统板原理图\3，STM32F407最小系统板原理图\STM32F407_CORE_BOARD_V1.4.pdf`

原理图文件名标记为 V1.4，但图纸标题栏标记为 V1.0。后续若硬件存在不同批次，必须在 BSP 中建立明确的 `board_revision`，不能仅依靠文件名判断。

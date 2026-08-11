# 实施路线图

## P0：工程骨架

交付物：

- Bootloader 与 APP 两套独立 CubeMX/CMake 工程；
- 公共组件目录和最小接口；
- WinForms Solution 与模块项目；
- GCC + CMake + Ninja 可重复构建；
- 链接脚本落实 256 KB / 768 KB 分区。

验收：两套 MCU 工程可以独立构建和下载，APP 能由 Bootloader 正确跳转。

## P1：基础 BSP 与存储

交付物：

- 时钟、SWD、USART1、LED、SPI1 BSP；
- W25Q128 驱动；
- 块设备和分区接口；
- 结构化日志和复位原因；
- FreeRTOS 静态任务与栈监测。

验收：W25Q128 擦写校验、边界测试、掉电后数据一致性和长时间循环测试通过。

## P2：Modbus 练习

交付物：

- APP 中 USART1 Modbus RTU Server；
- 版本化寄存器映射；
- WinForms Modbus 页面；
- 异常帧、超时和边界测试。

验收：持续轮询、批量读写和异常恢复稳定，不阻塞健康监控任务。

## P3：TinyUSB CDC 学习工程

交付物：独立 `.ioc`、FreeRTOS 配置、CDC ACM、WinForms 压测页面和资源报告。

## P4：USBX CDC 学习工程

交付物：独立 `.ioc`、USBX Standalone + FreeRTOS、相同测试和对比报告。

P3/P4 共同验收：枚举、吞吐、重复拔插、异常复位、Flash/RAM 和任务栈数据可直接比较。

## P5：USART1 基础 IAP

交付物：

- WinForms 升级页面；
- 升级会话和分块协议；
- Candidate 完整暂存；
- Bootloader 安装 APP；
- 进度和错误码。

验收：不同大小固件可稳定传输、安装并启动；任意分块重复发送不会破坏会话。

## P6：可信升级与回滚

交付物：SHA-256、ECDSA-P256、版本策略、双副本 Metadata、TRIAL/CONFIRMED 和 Golden 回滚。

验收：错误签名、错误硬件型号、降级包和损坏包均被拒绝；升级各阶段随机断电后均能恢复。

## P7：正式 USB 升级

依据 P3/P4 的量化结果选择一个 USB 栈，将 CDC 适配器接入正式升级传输层，不改变升级核心。

## P8：扩展入口

按优先级增加 TF 卡、CAN、RS485、以太网、Wi-Fi 和 4G。复杂网络下载由 APP 承担，Bootloader 只保留最小恢复入口。

## P9：生产化

- 密钥和设备身份注入；
- WRP/RDP 配置；
- 构建清单、哈希和签名审计；
- PC 单元测试、MCU 集成测试和硬件在环；
- 数百至数千轮随机断电；
- 发布版本和依赖版本固定。

## 当前下一步

P0、P1 已以 v0.4.0 完成：静态 RTOS 对象、栈高水位/heap 未使用门禁和累计 100 轮存储硬件在环循环均通过。下一阶段选择 P5 USART1 IAP，第一步解决日志与升级帧的唯一串口所有权并实现无 HAL 依赖的升级会话核心；暂不同时展开 Modbus、USB、签名或回滚。

# P1 基础 BSP 与存储阶段验收

日期：2026-08-11
版本：v0.4.0

## 验收范围

- Application SPI1/I2C1 BSP；
- NM25Q128EVB/W25Q128 兼容 SPI NOR 驱动；
- 24C02 EEPROM 驱动；
- Linux 风格统一存储接口与 Adapter；
- Candidate、Golden、Metadata A/B、Driver Test 分区视图；
- Debug/Release 破坏性自检策略；
- FreeRTOS 静态应用对象与资源健康门禁；
- 跨复位存储稳定态和循环硬件在环测试。

## RTOS 资源基线

Application 的 defaultTask、日志任务和日志队列均使用静态存储。`heap_4` 保留在供应商工程中，但 P1 应用路径未触发首次动态分配；运行时输出 `RTOS heap state : UNUSED`。

| 配置 | defaultTask 最小剩余 | app_log 最小剩余 | RTOS health |
|---|---:|---:|---|
| Debug | 313 words | 428 words | PASS |
| Release | 324 words | 437 words | PASS |

固件门槛为每个应用任务至少剩余 64 words；任何动态堆触发均把健康状态改为 FAIL。

## 存储循环

运行：

```cmd
cd Firmware\Application
python script\storage_soak.py --cycles 100 --port COM3 --image build\Debug\Application.elf
```

本次分两批完成，累计 100/100 轮 PASS：

- 20 轮：143.3 秒；
- 80 轮：572.3 秒。

每轮均通过 DAPLink 真实编程/复位和 COM3 日志验收，并检查：

- Driver Test 分区在本轮首次擦除前仍保持上轮清理后的 `0xFF`；
- SPI NOR 擦除、页写、跨页、末地址、越界拒绝和最终清理；
- 24C02 备份、跨页写、回读、边界拒绝和原值恢复；
- 两个物理设备 OPEN，五个 Flash 分区 READY；
- RTOS 静态对象、栈高水位、堆未使用和日志零丢失。

测试只使用 `0x00FFF000-0x00FFFFFF` Driver Test 分区。Candidate、Golden 和 Metadata 未参与破坏性循环。

## Debug/Release 镜像

- Debug：`0x08040000-0x0804AF4F`，存储自检启用；
- Release：`0x08040000-0x08047727`，SPI NOR/EEPROM 自检均禁用；
- 两者均严格位于 APP 的 768 KiB 内部 Flash 分区。

## 掉电语义说明

P1 已验证完整事务结束后的稳定数据可跨复位保持。没有宣称“写入中随机断电仍保持本次写入原子性”；SPI NOR Page Program 或 Sector Erase 被中断时，目标区域内容本来就可能不确定。Candidate 下载断点、双副本 Metadata、幂等安装和随机断电恢复属于 P5/P6 的事务状态机验收。

## 结论

P1 基础 BSP 与外部存储阶段通过，可以冻结为 v0.4.0 基线并进入 P5 USART1 IAP。P5 开始前必须先确定 USART1 日志与二进制升级帧的共享/复用规则，不能让两个模块直接并发访问 UART HAL。

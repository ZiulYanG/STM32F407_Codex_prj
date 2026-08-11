# YMODEM-1K 与串口命令控制台

日期：2026-08-11
阶段：P5-2～P5-4

## 模块边界

`Firmware/Common/ymodem` 是无 HAL、无 FreeRTOS、无存储依赖的纯 C
协议状态机。接收和发送方向只依赖以下回调：

- Transport：发送协议字节；
- File Sink：`begin/write/finish/abort`，用于接收文件；
- File Source：`open/read/close`，用于发送文件；
- 上层周期性传入当前毫秒时间并调用 `feed/poll`。

同一个协议核心因此既能接收固件、字库和图片，也能把日志或诊断文件发送给
WinForms 工具。文件的业务类型、目标分区、CRC32、版本和签名不属于 YMODEM
协议层，由升级/文件服务在开始传输前后处理。

## 1K 数据块与 W25Q128

擦除粒度和写入粒度不是同一个概念，不需要攒满 4 KB 才写：

1. YMODEM 校验一个 1024 B 数据块的 CRC16；
2. `ymodem_storage` 在首次进入新的 4 KB 区域时擦除该扇区；
3. 该 1 KB 调用 `storage_write()`；
4. SPI NOR 驱动按 256 B Page Program 自动拆成最多 4 次写入；
5. 全部写成功后，YMODEM 才向发送端返回 ACK。

这种方式只需要 YMODEM 自身的 1 KB 帧缓冲，不再增加 4 KB RAM 缓冲。顺序
写门禁拒绝跳跃 offset；跨越 4 KB 边界时只擦除一次下一个扇区。传输中断后
部分数据保持为无效 Candidate，后续 P6 由双副本 Metadata 决定重传、续传或
清理，当前不能将它声明为可安装镜像。

## 串口控制台和日志

Bootloader 与 Application 共用纯 C 命令行解析器，但各自拥有命令表和任务。
Serial Manager 是 USART1 唯一所有者：

- 命令应答/YMODEM/Modbus 使用高优先级协议队列；
- 普通任务日志使用普通队列；
- 每个任务可调用 `APP_TASK_LOG_*` 或 `BOOT_TASK_LOG_*`，不直接访问 UART；
- 任务日志零等待投递，队列满时累计 drop 后立即返回，不阻塞业务任务；
- 切换到 YMODEM/Modbus 模式后自动拒绝普通日志，避免污染二进制协议帧；
- 日志等级可在运行时设置为 debug/info/warn/error。

Application 命令：`help`、`info`、`status`、`get/set log_level`、
`get/set heartbeat_ms`、`reset`。Bootloader 另提供 `get/set window_ms`、
`boot stay` 和 `boot app`。当前设置只保存在 RAM，复位后恢复编译期默认值；
持久参数将在后续配置服务中写入 24C02。

## 命令触发会话与任务协作

Application 的 `update_session` 是深 Module，Console 只调用接收、发送、取消和
状态快照 Interface。该 Module 内部负责切换 `SERIAL_MODE_YMODEM`、运行协议、
访问 Candidate、恢复 Console 及发布结果日志。`system_mode` 使用静态 EventGroup
发布 NORMAL/FILE_RECEIVE/FILE_SEND 等模式；心跳任务在文件传输时关闭LED并
等待 NORMAL，Serial Manager 和协议任务保持运行。未来 BLE UART、USB CDC 只需
提供 Transport Adapter，不改文件和升级状态机。

当前命令为 `ymodem rx candidate`、`ymodem tx candidate [size]` 和
`ymodem status`。已通过COM3完成2500字节 Candidate RX/TX逐字节回环。当前只
证明可靠文件运输，尚未实现整文件CRC32、镜像Manifest、版本/签名判定或安装
Metadata，因此收到的文件不能直接标记为可安装固件。

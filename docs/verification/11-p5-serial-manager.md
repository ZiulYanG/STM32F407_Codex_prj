# P5-1 USART1 Serial Manager 验证

日期：2026-08-11

## 验证对象

- USART1 唯一 TX 所有权；
- 普通/协议两级静态 TX 队列；
- USART1 单字节中断接收与 2048 字节 BSP 环形缓冲；
- Serial Manager RX StreamBuffer；
- Console/YMODEM/Modbus 模式入口；
- 日志迁移及静态 RTOS 资源门禁。

## 构建结果

```text
python Firmware/Application/script/test_storage.py
python Firmware/Application/script/build.py --preset Debug --clean
python Firmware/Application/script/build.py --preset Release --clean
```

- 主机存储回归测试：PASS；
- Debug 镜像范围：`0x08040000–0x0804C6A7`；
- Release 镜像范围：`0x08040000–0x0804855F`；
- 两配置均通过 APP 地址、自测试策略和静态 RTOS 对象门禁；
- 静态扫描确认 `components/transport/serial_manager.c` 是
  `bsp_uart1_write()` 的唯一上层调用者。

## COM3 与 SWD 实测

Release 通过 DAPLink 100 kHz 烧录并由 Bootloader 跳转。COM3 启动日志确认：

```text
RTOS serial stack free : 422 words
RTOS heap state        : UNUSED
RTOS health    : PASS
APP log drops  : 0
Serial mode    : CONSOLE
Serial RX/drop : 0/0 bytes
Serial HW overrun/error: 0/0
```

随后通过 COM3 发送 256 个 ASCII 字节，并使用 Release ELF 的符号地址通过 SWD
读取 `manager_stats`：

```text
tx_messages = 39
tx_bytes    = 1164
tx_dropped  = 0
tx_errors   = 0
rx_bytes    = 256
rx_dropped  = 0
rx_overrun  = 0
rx_errors   = 0
```

CPU 被 SWD 暂停时 PC=`0x08045D0A`，位于 Application 区。读取完成后已恢复运行。

## 结论与限制

P5-1 双向字节运输闭环通过。当前只验证串口管理基础设施，尚未实现 Console
命令解析、YMODEM 状态机、文件存储 sink、整文件 CRC32、版本策略或签名验证。

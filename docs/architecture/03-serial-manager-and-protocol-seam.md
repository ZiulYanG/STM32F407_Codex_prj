# USART1 Serial Manager 与协议 seam

日期：2026-08-11
阶段：P5-1

## 目标

Application 中只有 Serial Manager Module 可以调用 USART1 BSP 写接口。日志、
YMODEM、Modbus 和后续 Console 任务均不得直接访问 HAL UART 句柄，也不得同时
竞争同一个原始 RX 队列。

## 分层

```text
app_log / YMODEM / Modbus
          |
          | serial_manager_write/read + mode
          v
Serial Manager（静态任务、两级 TX 队列、RX StreamBuffer）
          |
          v
bsp_uart1（HAL 发送、RX 中断、SPSC 环形缓冲）
          |
          v
USART1 PA9/PA10, 115200 8N1
```

Serial Manager 的外部 Interface 只暴露初始化、收发、模式、统计和栈水位。
FreeRTOS 对象、队列深度、轮询周期和 HAL 超时均隐藏在实现中。

## 所有权与并发约束

- `bsp_uart1_write()` 的唯一调用者是 Serial Manager 任务；
- 多个生产者可调用 `serial_manager_write()`；日志进入普通队列，协议控制字节
  进入高优先级队列；
- USART1 RX 中断只写入 2048 字节 BSP SPSC 环形缓冲，Serial Manager 是唯一
  读取者；
- Serial Manager 再将字节写入 2048 字节 RX StreamBuffer；同一时刻只能有一个
  与当前 `serial_mode_t` 匹配的协议任务读取；
- 切换模式前，上层会话管理者必须先停止旧接收者。模式切换会清空 RX
  StreamBuffer，禁止旧协议残留字节进入新协议；
- 非 Console 模式下普通日志被拒绝并计入丢弃统计，防止日志字节破坏 YMODEM
  或 Modbus 帧；
- 当前 TX 单条消息上限为 160 字节。YMODEM 接收方向的数据块走 RX StreamBuffer，
  ACK/NAK/CAN 等短响应走高优先级 TX 队列，不需要把 1 KiB 数据块复制进 TX 队列。

## 当前资源

| 资源 | 配置 |
|---|---:|
| Serial Manager task stack | 512 words |
| 普通 TX 队列 | 24 × 160-byte message |
| 高优先级 TX 队列 | 8 × 160-byte message |
| BSP RX 环形缓冲 | 2048 bytes |
| RX StreamBuffer | 2048 bytes |
| USART1 IRQ 优先级 | 5 |

所有对象均使用静态内存。USART1 IRQ 当前不调用 FreeRTOS FromISR Interface，
但优先级 5 与当前 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5` 保持兼容，
为以后增加任务通知保留空间。

## 后续接入 YMODEM

YMODEM Module 只依赖 Serial Manager Interface 和后续 `file_sink` Interface，
不依赖 HAL、FreeRTOS 队列或具体 Flash 驱动。进入传输前切换到
`SERIAL_MODE_YMODEM`，结束、取消或超时后再回到 `SERIAL_MODE_CONSOLE`。

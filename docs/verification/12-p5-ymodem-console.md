# P5-2/P5-3 YMODEM 与控制台验证

日期：2026-08-11

## 自动测试和构建

以下检查全部通过：

```text
python Firmware/Common/tests/ymodem/run_tests.py
python Firmware/Common/tests/console/run_tests.py
python Firmware/Application/script/test_ymodem_storage.py
python Firmware/Bootloader/script/build.py --clean --preset Debug
python Firmware/Bootloader/script/build.py --clean --preset Release
python Firmware/Application/script/build.py --clean --preset Debug
python Firmware/Application/script/build.py --clean --preset Release
```

- YMODEM：2500 B 正常 RX/TX、首包 CRC 错误重传、多个 ACK 丢失恢复、取消；
- Console：CR/LF、CRLF、空白分词、退格、行溢出和参数过多；
- Storage Adapter：4 KB 按需擦除、顺序写门禁、Source 读取；
- Bootloader Release：Flash 28,404 B，RAM 37,464 B；
- Application Release：范围 `0x08040000-0x08049593`，Flash 38,292 B，
  RAM 40,032 B，静态 RTOS 与 Release 自测试策略门禁通过。

## COM3 实测

临时使 APP 向量无效后，Bootloader 驻留并通过 COM3 完成 `help/info/status`、
日志等级和 3 s 命令窗口参数读写。状态为：RX overrun/error 0/0、TX drop/error
0/0、Console/Serial 剩余栈 161/421 words。

恢复 Application Release 后，通过 COM3 完成 `help/info/status`、心跳周期
1000→500 ms 和日志等级 INFO→DEBUG。状态为：串口 RX drop/hardware error
均为 0、日志丢弃 0、Console/Serial 剩余栈 180/422 words，heap 仍为 UNUSED。
`reset` 命令应答经过 flush 后完整输出。

最终非阻塞日志优先级调整后重新烧录 Bootloader/Application Release，OpenOCD
均 `Verified OK`；Application 再次响应 `info/status`，日志 drop=0，串口错误为0，
Console/Serial 剩余栈仍为180/422 words。板卡交付时正在运行Application。

## 启动脚风险

本轮 DAPLink 后续硬复位曾捕获 PC=`0x1FFF3DA0`，表明 MCU 进入了系统 Boot ROM，
而非用户 Flash；同一现象也使 `reset` 后的 Bootloader 日志不可见。固件控制台
在用户 Flash 实际运行时已验证，残余问题应先检查核心板 BOOT0 跳线/下拉和
复位瞬间电平。测试过程中曾临时整片擦除内部 Flash，随后已重新烧录并 Verify
Bootloader 与 Application Release 镜像；外部 W25Q128 未被该操作修改。

## 结论

P5-2 的平台无关 YMODEM RX/TX 核心、存储适配层以及 P5-3 双工程命令控制台已
实现。控制台通过实机验证；YMODEM 当前只完成主机级验证，待 MCU 会话任务和
WinForms 端到端传输验证后才可标记为串口文件传输闭环完成。

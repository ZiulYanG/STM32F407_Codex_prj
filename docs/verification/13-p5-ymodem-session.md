# P5-4 命令触发 YMODEM 会话验证

日期：2026-08-11
版本：v0.5.0

## 验证范围

- Console命令触发Candidate接收和发送；
- NORMAL、FILE_RECEIVE、FILE_SEND运行模式切换；
- 非必要心跳任务协作休眠及会话结束恢复；
- W25Q128按需4KB擦除、1KB接收和256B分页写；
- YMODEM双向硬件传输、取消和资源余量。

## 自动与硬件测试

主机侧YMODEM核心、Console解析和存储Adapter测试全部PASS。Bootloader和
Application Debug/Release Clean Build通过，Application Release范围为
`0x08040000-0x0804BA33`，静态RAM 44,384 B，构建策略门禁通过。

通过以下命令执行COM3硬件回环：

```text
python Firmware/Application/script/test_ymodem_hardware.py --port COM3 --size 2500
```

PC生成2500字节确定性模式数据，执行`ymodem rx candidate`写入W25Q128，随后
执行`ymodem tx candidate 2500`读回。YMODEM头、3个1KB块、EOT和最终空头均
完成握手，PC逐字节比较PASS。

传输等待阶段SWD读取：`SYSTEM_MODE=1(FILE_RECEIVE)`、
`SERIAL_MODE=1(YMODEM)`、`GPIOF_ODR=0x600`，确认PF9/PF10均关闭。发送双CAN后
Console恢复，`system_mode=NORMAL`、状态`CANCELLED`。完整回环后串口RX/TX
drop/error为0，Console/Serial/Update栈余量分别为176/416/266 words，heap保持
UNUSED。

## 故障定位与修复

初版收到READY后没有首个`C`。SWD显示CPU停在`xStreamBufferReceive()`的
configASSERT，原因是Console尚在阻塞读取，YMODEM任务又成为第二Reader。
Serial Manager读取改为非阻塞短轮询并在每轮复核模式，模式切换后旧Reader
立即退出，新Reader独占StreamBuffer，回归通过。

初版Update任务完整传输后仅剩10 words栈，低于64 words门槛。任务栈从768
提升至1024 words，余量恢复到266 words。

## 当前边界

P5基础文件运输闭环完成，但它不是可信升级闭环。Candidate尚未包含整文件
CRC32、Manifest、版本、防降级、签名和双副本Metadata；这些验证完成前不得
让Bootloader安装本次测试文件。

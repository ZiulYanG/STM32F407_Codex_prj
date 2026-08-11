# Application SPI NOR最小擦写闭环验证

日期：2026-08-11
目标：在独立开发测试扇区内验证状态读取、写使能、Busy超时、4 KB擦除、页编程、普通读取和回读比较。

## 1. 安全边界

外部Flash容量为16 MB。测试只允许访问最后一个4 KB扇区：

```text
起始地址：0x00FFF000
结束地址：0x00FFFFFF
测试长度：64 bytes
```

该扇区从原Reserved区单独划出为`Driver Test`。Candidate、Golden、Metadata A/B、Event Log和Crash Dump均不在测试范围内。

自测试流程为：

1. 擦除`0x00FFF000`扇区并读取前64字节确认均为`0xFF`；
2. 在该扇区第一页写入64字节`0xA5 ^ index`模式；
3. 读取同一范围并逐字节比较；
4. 再次擦除该扇区，并读取前64字节确认恢复为`0xFF`。

这是破坏性开发测试，发布构建必须关闭，避免每次启动消耗擦写寿命。

## 2. 驱动保护

`spi_nor`器件层当前提供：

- Status Register-1读取；
- WIP Busy轮询和毫秒级有界超时；
- Write Enable命令及WEL位确认；
- 24位地址普通读取；
- 最大256字节且禁止跨页的Page Program；
- 必须4 KB对齐的Sector Erase；
- 基于已识别器件容量的地址加长度边界检查。

驱动不创建FreeRTOS任务、不使用动态内存。时间延迟、SPI传输和软件片选均由Application注入BSP回调。

## 3. 验证命令

在仓库根目录运行：

```text
python Firmware/Application/script/build.py --clean
python Firmware/Application/script/verify_spi_nor.py
```

自动脚本先验证APP镜像范围，再通过DAPLink烧录并复位，最后捕获COM3日志。擦写验收必须包含：

```text
SPI NOR test   : 0x00FFF000, 64 bytes
SPI NOR erase  : PASS
SPI NOR program: PASS
SPI NOR verify : PASS
SPI NOR cleanup: PASS
SPI NOR R/W test: PASS
```

## 4. 实机结果

- Clean Build通过，共43个目标文件；新增代码无编译警告。
- 只有ST HAL原有的未使用参数警告。
- APP镜像范围为`0x08040000–0x08047F4F`，位于768 KB APP分区内。
- DAPLink Program、Verify、Reset成功，Bootloader判断APP有效并正常跳转。
- JEDEC仍稳定为`52 21 18`，匹配`NM25Q128EVB`兼容项。
- 擦除、编程、回读比较和最终清理连续两次独立验收均为PASS，脚本两次返回0。

## 5. 下一步

自动分页写入、Flash末地址和负向边界测试已在后续步骤完成，详见`docs/verification/05-application-spi-nor-boundaries.md`。在破坏性启动自测试具备Release默认禁用策略前，不进入升级镜像分区服务。

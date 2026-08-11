# Linux 风格的统一存储驱动模型

## 1. 采用范围

Application 的外部存储采用“Linux 思想、MCU 约束”的静态驱动模型：

```text
业务/后续分区层
      |
storage_device + storage_ops
      |
W25Q128 Adapter / 24C02 Adapter
      |
spi_nor / eeprom_24c02 器件驱动
      |
BSP SPI1 / BSP I2C1
      |
STM32 HAL
```

核心是把数据对象与操作表分开：`storage_device` 保存名称、能力、容量、私有上下文和打开状态，`storage_ops` 挂接 `open/read/write/erase/sync/close`。业务只依赖 `storage.h`，不直接依赖 HAL、SPI、I2C 或具体芯片。

这不是完整 Linux VFS：当前不实现文件描述符、动态设备注册、路径查找、用户态边界和动态内存。仅保留对裸机/RTOS 项目有直接收益的部分。

## 2. 模块边界

- `components/storage/storage.*`：稳定的统一接口，负责生命周期、能力、范围和擦除对齐检查。
- `storage_spi_nor.*`：把 SPI NOR 驱动适配为统一存储设备。
- `storage_eeprom_24c02.*`：把 24C02 驱动适配为统一存储设备。
- `components/drivers/*`：器件协议与时序，不创建任务，不知道业务分区。
- `bsp/*`：封装 CubeMX/HAL 句柄和板级引脚。

适配器是刻意设置的 seam：更换 Flash、EEPROM 或底层总线时，业务接口保持不变。

## 3. 能力与语义

| 能力 | W25Q128/NM25Q128 | 24C02 |
|---|---:|---:|
| READ | 是 | 是 |
| WRITE | 是 | 是 |
| ERASE | 是，4 KiB 对齐 | 否 |
| WRITE_REQUIRES_ERASE | 是 | 否 |
| program page | 256 B | 8 B |

`storage_write()` 不会替 SPI NOR 自动擦除。调用者必须读取能力并按策略先擦除；这避免隐藏耗时操作、数据破坏和掉电语义。24C02 的 Adapter 允许直接改写，器件驱动内部负责跨页拆分与 ACK 轮询。

所有对象静态分配。`open()` 可重复调用；未打开设备不能读写；越界、能力不支持、状态错误与 I/O 错误使用统一状态码表达。

## 4. 并发约束

当前统一存储层本身不提供锁，不代表底层驱动可重入。现阶段所有调用均在默认任务串行执行。后续出现多个任务访问同一设备时，应在设备服务或总线管理层使用 FreeRTOS mutex 串行化，不能让每个业务模块各自加锁。

## 5. 当前不加入设备注册表的原因

目前只有两个编译期确定的真实设备，Application 直接持有两个静态 `storage_device` 对象即可。现在加入字符串查找、动态注册或通用 VFS 只会扩大接口和错误面。等到升级服务、参数服务等多个消费者确实需要统一发现设备时，再根据实际访问模式增加最小注册机制。

## 6. 下一层

已实现轻量的 `storage_partition` 视图：它引用一个父设备并限制 offset/length，分区层仍调用统一存储接口，不直接调用 SPI NOR 驱动。

分区本身也是 `storage_device`，业务使用从零开始的分区相对地址。统一存储层会先按分区容量拒绝越界请求，Adapter 再将合法地址平移到父设备。父设备生命周期长于所有分区；打开分区会确保父设备已打开，关闭一个分区不会关闭共享父设备。

当前 APP 静态装配 Candidate、Golden、Metadata A、Metadata B 和 Driver Test 五个视图。布局常量包含编译期不重叠及 Flash 末地址断言。整颗 SPI NOR 设备保持在 `app_main.c` 内部，不向业务模块导出。

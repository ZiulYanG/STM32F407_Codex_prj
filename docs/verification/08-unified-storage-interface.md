# 统一存储接口验证记录

日期：2026-08-11

## 1. 交付内容

- 新增 `storage_device + storage_ops` 静态统一存储接口；
- 新增 SPI NOR 与 24C02 两个 Adapter；
- APP 初始化后将设备分别绑定为 `spi_nor0` 和 `eeprom0`；
- 原有 Debug 硬件自检改为经统一接口执行通用读、写、擦除、同步和边界检查；
- 保留器件驱动级的页边界负向测试；
- 新增无硬件主机测试 `script/test_storage.py`。

## 2. 主机测试

运行：

```cmd
cd Firmware\Application
python script\test_storage.py
```

结果：`storage interface tests: PASS`。

覆盖生命周期、重复 open、read/write/sync/close、越界预检查、擦除对齐和能力拒绝。测试使用内存 Adapter，不依赖 STM32 HAL。

## 3. Debug 实机验证

```cmd
python script\build.py --preset Debug
python script\verify_spi_nor.py --port COM3 --expect-self-test enabled --image build\Debug\Application.elf --capture-seconds 12
```

- 镜像范围：`0x08040000-0x0804A8B3`；
- `Storage spi_nor0: OPEN, 16777216 bytes, erase 4096`；
- `Storage eeprom0 : OPEN, 256 bytes, no erase`；
- SPI NOR 擦除、编程、回读、跨页、末地址、边界和清理全部 PASS；
- 24C02 备份、编程、回读、边界、恢复全部 PASS；
- `APP log drops : 0`。

验证中发现最后一个 Flash 扇区的“起点 + 1、长度 4 KiB”同时违反范围与对齐约束，接口按检查顺序返回 `STORAGE_ERR_RANGE`。测试随后改用倒数第二扇区的非对齐地址，仅验证对齐错误，结果通过。这是测试用例歧义，不是设备越界写入。

## 4. Release 实机验证

```cmd
python script\build.py --preset Release
python script\verify_spi_nor.py --port COM3 --expect-self-test disabled --image build\Release\Application.elf --capture-seconds 10
```

- 镜像范围：`0x08040000-0x08047163`；
- 两个统一存储设备均成功打开；
- JEDEC 识别与 EEPROM probe 通过；
- SPI NOR 与 EEPROM self-test 均为 `DISABLED`；
- `APP log drops : 0`。

最终开发板保持运行 Release 镜像。

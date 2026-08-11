# SPI NOR破坏性自测试构建策略验证

日期：2026-08-11
目标：确保开发阶段可自动执行SPI NOR擦写验收，而Release镜像不包含启动自动擦写入口。

## 1. 构建策略

CMake按构建类型定义：

| 构建类型 | `APP_ENABLE_SPI_NOR_SELF_TEST` | 行为 |
|---|---:|---|
| Debug | 1 | 编译并执行`0x00FFF000`破坏性自测试 |
| Release | 0 | 编译排除自测试函数、缓冲区和擦写日志 |
| RelWithDebInfo | 0 | 同Release |
| MinSizeRel | 0 | 同Release |

该规则不使用可被遗忘的运行时变量。Release仍会初始化SPI NOR、读取JEDEC ID并确认型号支持，但不会执行擦除或编程验收。

## 2. 构建门禁

`script/self_test_policy.py`由`build.py`自动调用，检查三层证据：

1. `app_main.c`编译命令必须带期望的宏值；
2. ELF只能包含对应的`ENABLED`或`DISABLED`标记；
3. Debug ELF必须存在`app_flash_run_rw_test`符号，非Debug ELF必须不存在。

任一条件不满足时，构建脚本在生成可交付BIN/HEX前返回失败。

## 3. 验证命令

```text
python Firmware/Application/script/build.py --preset Debug --clean
python Firmware/Application/script/build.py --preset Release --clean

python Firmware/Application/script/verify_spi_nor.py \
  --image Firmware/Application/build/Debug/Application.elf \
  --expect-self-test enabled

python Firmware/Application/script/verify_spi_nor.py \
  --image Firmware/Application/build/Release/Application.elf \
  --expect-self-test disabled
```

禁用模式除了要求`SPI NOR self-test: DISABLED`，还会反向检查串口日志中不得出现test/erase/program/verify/cross-page/flash-end/bounds/cleanup等破坏性测试行。

## 4. 验证结果

### Debug

- Clean Build通过，策略门禁输出`Debug -> enabled`；
- 镜像范围`0x08040000–0x080482CB`；
- DAPLink烧录和Bootloader跳转正常；
- 擦除、单页、跨页、Flash末端、负向边界与清理全部PASS；
- `APP log drops : 0`。

### Release

- Clean Build通过，策略门禁输出`Release -> disabled`；
- 镜像范围`0x08040000–0x080457B7`；
- ELF中不存在`app_flash_run_rw_test`和启用标记；
- DAPLink烧录和Bootloader跳转正常；
- 串口确认JEDEC `52 21 18`、型号支持以及`SPI NOR self-test: DISABLED`；
- 没有任何破坏性测试日志，`APP log drops : 0`。

当前开发板最后烧录的是Release镜像。

## 5. 结论与下一步

SPI NOR基础驱动已完成身份识别、擦写、自动分页、容量边界、负向拒绝和发布构建保护闭环。下一步进入24C02硬件连接与Application I2C配置审计，先完成静态配置和Clean Build，再加入器件驱动。

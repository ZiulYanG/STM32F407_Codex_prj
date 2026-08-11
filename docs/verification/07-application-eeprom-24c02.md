# Application 24C02 驱动与实机验证

日期：2026-08-11

## 1. 验证目标

- 验证I2C1使用PB8/SCL、PB9/SDA和100 kHz时钟；
- 验证板载24C02在7位地址`0x50`应答；
- 验证8字节页边界拆分、跨页写、回读和容量边界保护；
- 验证Debug允许开发自测试，而Release不会包含写测试入口；
- 验证测试结束后恢复EEPROM原有数据。

## 2. 驱动分层

- `bsp/bsp_i2c1.c`：适配STM32 HAL，统一接收7位地址并在调用HAL时左移一位；
- `components/drivers/eeprom_24c02.c`：不依赖HAL，负责256字节容量、8字节页、ACK轮询、自动跨页写和范围检查；
- `app/app_main.c`：装配总线回调、输出探测结果，并只在Debug执行可恢复自测试。

调用者统一使用7位地址`0x50`，不得在设备驱动或业务层传入HAL格式的`0xA0`。

## 3. Debug自测试步骤

测试区域为`0xF6–0xFF`，长度10字节，会跨越`0xF7/0xF8`页边界。

1. 读取并备份测试区域原值；
2. 写入异或测试模式，驱动自动拆成两个Page Write；
3. 回读并逐字节比较；
4. 验证从`0xFF`读写2字节被拒绝；
5. 验证跨页调用底层单页写接口被拒绝；
6. 无论中间验证是否失败，只要备份成功就尝试恢复原值；
7. 回读并确认恢复数据与备份完全一致。

该方案能在正常执行完毕时恢复原数据，但测试过程中掉电仍可能造成测试区域数据变化。因此写测试只允许用于Debug，产品数据不得分配到`0xF6–0xFF`，直到统一存储布局正式确定。

## 4. 构建门禁

Debug定义`APP_ENABLE_EEPROM_SELF_TEST=1`；Release、RelWithDebInfo和MinSizeRel定义为0。`script/self_test_policy.py`检查：

- `app_main.c`编译命令中的宏值；
- ELF只能包含当前构建类型对应的启用或禁用日志标记；
- Debug必须包含`app_eeprom_run_rw_test`，非Debug必须不存在该符号。

## 5. 实机结果

使用DAPLink序列号`B1897547C3840B15B1FE4CDBBF997647`、SWD 100 kHz和COM3 115200 8N1完成验证。

| 项目 | Debug | Release |
|---|---|---|
| APP镜像范围 | `0x08040000–0x08049F13` | `0x08040000–0x080468AF` |
| 地址`0x50`探测 | PASS | PASS |
| 备份/跨页写/回读 | PASS | 编译移除 |
| 越界与非法单页写拒绝 | PASS | 编译移除 |
| 原值恢复与回读 | PASS | 编译移除 |
| 启动日志 | `EEPROM self-test: ENABLED` | `EEPROM self-test: DISABLED` |
| 日志丢包 | 0 | 0 |

最终重新烧录Release镜像，开发板当前运行Release，只执行SPI NOR识别和24C02地址探测，不执行存储器写测试。

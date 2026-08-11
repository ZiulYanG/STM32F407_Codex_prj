# 外部 Flash 分区视图验证记录

日期：2026-08-11

## 交付内容

- 新增 `storage_partition` Adapter；
- 分区继承父设备容量之外的几何和能力信息；
- bind 阶段拒绝超出父设备或不满足擦除对齐的定义；
- 读写擦除使用分区相对地址，合法请求平移到父设备；
- 关闭分区不关闭共享父设备；
- APP 静态绑定 Candidate、Golden、Metadata A/B 和 Driver Test；
- Driver Test 启动自检只能看到自己的 4 KiB 视图；
- 分区布局加入编译期重叠和 Flash 末地址断言。

## 主机测试

`python script\test_storage.py`：PASS。

覆盖相对地址平移、分区越界在父 Adapter 前拒绝、擦除转发、父设备自动打开、分区关闭不关闭父设备，以及错误范围/对齐定义拒绝。

## Debug 实机验证

- 镜像范围：`0x08040000-0x0804AC57`；
- `Storage partitions: READY`；
- Driver Test 物理区域仍为 `0x00FFF000`；
- SPI NOR 全部读写、边界和清理测试 PASS；
- 24C02 全部可逆测试 PASS；
- `APP log drops : 0`。

## Release 实机验证

- 镜像范围：`0x08040000-0x08047517`；
- 两个物理存储设备打开，五个 Flash 分区 READY；
- SPI NOR 与 EEPROM self-test 均为 `DISABLED`；
- `APP log drops : 0`。

最终开发板保持运行 Release 镜像。

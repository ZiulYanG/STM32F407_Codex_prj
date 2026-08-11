# Application SPI NOR自动分页与边界验证

日期：2026-08-11
目标：验证通用写接口自动拆页、Flash容量末端处理，以及非法请求在发命令前被拒绝。

## 1. 自动分页接口

新增`spi_nor_write()`，调用前先校验完整地址范围，然后循环计算：

```text
page_offset = current_address % 256
page_space  = 256 - page_offset
chunk       = min(remaining, page_space)
```

每个分块继续调用已有`spi_nor_page_program()`，因此仍具备WEL确认、单页限制和Busy超时。驱动不隐式擦除；调用者必须保证目标位可从1写为0。

## 2. 正向与负向用例

所有破坏性操作仍限定在`0x00FFF000–0x00FFFFFF`。

| 用例 | 地址与长度 | 预期 |
|---|---|---|
| 自动跨页写 | `0x00FFF0F0`，64字节 | 拆为16+48字节，回读一致 |
| Flash末端写 | `0x00FFFFC0`，64字节 | 最后一字节为`0x00FFFFFF`，回读一致 |
| 单页接口跨页 | `0x00FFF0F0`，64字节 | 拒绝，不发写命令 |
| 越界读 | `0x00FFFFFF`，2字节 | 拒绝 |
| 越界自动写 | `0x00FFFFFF`，2字节 | 拒绝，不发写命令 |
| 非对齐4 KB擦除 | `0x00FFF001` | 拒绝，不发擦除命令 |
| 最终清理 | 擦除`0x00FFF000` | 跨页位置和末端位置均读回`0xFF` |

## 3. 日志丢失故障定位

首次硬件运行时，串口只输出到`SPI NOR flash-end : PASS`，自动脚本因缺少后续三行判定失败。反馈循环为：

```text
python Firmware/Application/script/verify_spi_nor.py
```

该脚本稳定复现“第24条APP日志后无输出”。源码确认日志队列深度恰好为24，且`app_main_init()`在调度器启动前以零等待连续入队。只把队列临时扩为32后，原脚本立即转绿并显示所有Flash测试PASS，证明根因是日志队列溢出，不是Flash边界操作卡死。

正式修复没有保留临时扩容：

- 日志队列恢复24深度；
- `app_main_init()`移至调度器启动后的默认任务；
- 调度器运行时，日志入队允许最多等待100 ms，让日志任务排空队列；
- 默认任务因承担初始化和格式化日志，栈从512 B提升至2 KB，并同步修改`.ioc`；
- 启动日志增加`APP log drops : 0`回归断言。

## 4. 实机结果

- Clean Build通过，新增代码无编译警告；仅保留ST HAL原有警告。
- APP镜像范围为`0x08040000–0x0804829B`。
- DAPLink Program、Verify、Reset与Bootloader跳转正常。
- `cross-page`、`flash-end`、`bounds`、`cleanup`和总测试连续两次全部PASS。
- 两次均报告`APP log drops : 0`，测试扇区最终恢复为擦除态。

## 5. 下一步

破坏性自测试编译策略已经完成并通过Debug/Release双构建及实机验证，详见`docs/verification/06-spi-nor-self-test-build-policy.md`。SPI NOR基础驱动里程碑现已收敛，下一步进入24C02/I2C配置审计。

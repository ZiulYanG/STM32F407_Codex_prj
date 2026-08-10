# Bootloader 阶段 2：独立 Application 与安全跳转验证

## 1. 验证目标

验证 STM32F407ZGT6 在 FreeRTOS Bootloader 中能够识别位于 `0x08040000` 的独立 Application，在输出完整日志后安全切换向量表和栈并运行 APP；无效 APP 必须留在 Bootloader。

## 2. 固件布局

| 固件 | Flash 起始地址 | 长度 | 向量表 |
|---|---:|---:|---:|
| Bootloader | `0x08000000` | 256 KB | `0x08000000` |
| Application | `0x08040000` | 768 KB | `0x08040000` |

APP 链接脚本使用 `ORIGIN = 0x08040000`，CMake 为所有 APP 源文件定义 `USER_VECT_TAB_ADDRESS` 和 `VECT_TAB_OFFSET=0x00040000U`。

## 3. Bootloader 校验规则

启动时读取 APP 向量表前两个 32 位字：

1. 向量表地址必须按 `0x200` 对齐；
2. 初始 MSP 必须按 8 字节对齐，并位于主 SRAM `0x20000000–0x20020000` 或 CCM `0x10000000–0x10010000`；
3. Reset_Handler 必须设置 Thumb 位；
4. Reset_Handler 去除 Thumb 位后必须落在 `0x08040000–0x080FFFFF`。

任一检查失败时，Bootloader 输出失败原因并保持运行，不执行间接跳转。

## 4. 构建与烧录

在仓库根目录的普通 CMD 中执行：

```text
python Firmware/Bootloader/script/build.py --clean
python Firmware/Application/script/build.py --clean
python Firmware/Application/script/flash.py --adapter-speed 100 --dap-serial B1897547C3840B15B1FE4CDBBF997647
python Firmware/Bootloader/script/flash.py --adapter-speed 100 --dap-serial B1897547C3840B15B1FE4CDBBF997647
```

串口使用 CH340 的 COM3、115200 8N1；COM11 是 DAPLink 虚拟串口，不用于 PA9/PA10 日志。

## 5. 有效 APP 验证

预期关键日志：

```text
STM32F407 Bootloader
Version       : 0.3.0
APP address   : 0x08040000
APP check     : VALID
APP MSP       : 0x20020000
Boot action   : JUMP TO APPLICATION

STM32F407 Application
Version       : 0.3.0
SYSCLK        : 168000000 Hz
Vector table  : 0x08040000
APP state     : DEVELOPMENT
```

SWD 运行态检查结果：

- PC 位于 `0x0804xxxx`；
- VTOR 寄存器 `0xE000ED08` 等于 `0x08040000`；
- CFSR `0xE000ED28` 和 HFSR `0xE000ED2C` 均为 0；
- PF9/PF10 为输出，APP 心跳任务周期运行。

## 6. 无效 APP 验证与恢复

Sector 6 对应 `0x08040000–0x0805FFFF`。仅在已确认 Application ELF 可用于恢复时临时擦除该扇区：

```text
flash erase_sector 0 6 6
```

回读 `0x08040000` 应为：

```text
0x08040000: ffffffff ffffffff
```

Bootloader 预期输出：

```text
APP check     : INVALID (initial MSP)
Boot action   : STAY IN BOOTLOADER
```

随后立即运行 Application `flash.py` 恢复 APP，并再次确认有效跳转。本次验收已完成擦除、拒跳、恢复和再次跳转的完整闭环。

## 7. HardFault 定位记录

最初版本在 `boot_app_process()` 中进入 HardFault：

- CFSR=`0x00020000`，含义为 UsageFault `INVSTATE`；
- HardFault 的 LR=`0xFFFFFFF1`，说明异常发生在异常处理上下文；
- 在 HardFault 入口读取标准异常栈，发现 PendSV 恢复任务时 PC/LR 变成普通 RAM 地址；
- 在首次 SVC 入口设置断点，确认 Timer 任务初始上下文中的 `0xFFFFFFFD` 原本正确；
- `boot_app_process()` 运行后 Timer 栈被覆盖，说明破坏发生在默认任务执行期间；
- 默认任务只有 512 B，却调用 `boot_log_printf()`/`vsnprintf()`，最终定位为任务栈溢出；
- 将默认任务栈提升为 2 KB 后，有效 APP、无效 APP 和恢复场景全部通过。

该案例说明：RTOS HardFault 不应只看当前 PC。应同时读取 CFSR/HFSR、EXC_RETURN、MSP/PSP、异常栈 PC/LR，并在 SVC/PendSV 边界设置断点验证上下文在何时被破坏。

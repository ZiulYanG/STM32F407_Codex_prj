# 使用 OpenOCD、PC 和 LR 定位 STM32 启动故障

本文记录本项目定位“烧录成功但 LED 不运行”故障时使用的方法。它适用于以下典型现象：

- 固件烧录和校验成功，但预期功能没有运行；
- MCU 没有明显输出，无法判断卡在初始化、RTOS 还是任务内部；
- 多个 HAL 初始化函数都可能调用同一个 `Error_Handler()`；
- 需要在没有 printf 日志的 Bootloader 中定位故障源。

示例环境：

- MCU：STM32F407ZGT6；
- 调试器：DAPLink，CMSIS-DAPv2；
- 构建类型：CMake `Debug`；
- 调试工具：OpenOCD、GNU Arm `addr2line`、`nm`、`objdump`、`gdb`；
- 工作目录：`D:\ZzlWorkDir\STM32\STM32F407\Codex_STM32F407\Firmware\Bootloader`。

## 1. 调试前准备

打开普通 Windows CMD：

```cmd
cd /d D:\ZzlWorkDir\STM32\STM32F407\Codex_STM32F407\Firmware\Bootloader
```

重新编译并烧录，确保芯片中的固件与用于地址解析的 ELF 完全一致：

```cmd
python script\build.py --clean
python script\flash.py
```

确认存在：

```text
build\Debug\Bootloader.elf
```

必须保留 ELF。BIN 只保存机器码，没有完整的符号和源码行信息，不能用于后续的函数与行号映射。

建议使用 `Debug` 配置。Release 优化可能合并、重排或内联函数，导致源码行映射不够直观。

## 2. 第一步：读取 CPU 当前执行位置

### 2.1 为什么先看 PC

PC（Program Counter）保存 CPU 当前正在执行的指令地址。如果程序已经进入死循环，暂停多次后 PC 通常保持在同一个小范围内。

例如：

```text
PC = 0x080006f0
```

这个地址本身没有语义，需要结合本次编译生成的 ELF 才能映射回函数和源码。

### 2.2 使用 OpenOCD 运行后暂停

在 CMD 中执行：

```cmd
D:\OpenOCD\xpack-openocd-0.12.0-6\bin\openocd.exe ^
  -s D:\OpenOCD\xpack-openocd-0.12.0-6\openocd\scripts ^
  -f interface/cmsis-dap.cfg ^
  -c "transport select swd" ^
  -f target/stm32f4x.cfg ^
  -c "adapter speed 1000" ^
  -c "init; reset run; sleep 1000; halt; echo PC=[reg pc]; echo LR=[reg lr]; echo MSP=[reg msp]; reset run; exit"
```

该命令依次执行：

1. 连接 CMSIS-DAP；
2. 选择 SWD；
3. 复位并运行 MCU；
4. 等待 1000 ms；
5. 暂停 CPU；
6. 输出 PC、LR 和 MSP；
7. 再次复位运行，避免调试结束后让开发板保持暂停。

典型输出：

```text
PC=pc (/32): 0x080006f0
LR=lr (/32): 0x080008e1
MSP=msp (/32): 0x2001fff0
```

建议连续执行两到三次：

- PC 始终在同一个地址附近：通常处于死循环、错误处理或等待循环；
- PC 在 FreeRTOS Idle Task 或不同任务之间变化：调度器大概率已经运行；
- PC 位于 HardFault：需要进一步读取异常栈帧和故障状态寄存器。

### 2.3 将 PC 映射为函数和源码行

执行：

```cmd
arm-none-eabi-addr2line -e build\Debug\Bootloader.elf -f -C 0x080006f0
```

参数含义：

- `-e`：指定包含调试符号的 ELF；
- `-f`：显示函数名；
- `-C`：还原 C++ 符号名称，对纯 C 工程也可保留；
- 最后的地址：需要解析的 PC。

本次故障得到：

```text
Error_Handler
.../Core/Src/main.c:187
```

这说明程序没有运行到 LED 任务，而是在此前某个初始化步骤失败并进入了 `Error_Handler()`。

## 3. 第二步：从 Error_Handler 追溯调用者

多个外设初始化函数可能调用同一个 `Error_Handler()`。此时只看 PC 无法判断具体是谁失败，需要在函数入口暂停并读取 LR。

### 3.1 查找 Error_Handler 的本次编译地址

每次重新链接后地址都可能变化，不要长期硬编码旧地址。执行：

```cmd
arm-none-eabi-nm -n build\Debug\Bootloader.elf | findstr Error_Handler
```

示例输出：

```text
080006e8 T Error_Handler
```

这里的 `0x080006e8` 就是本次 ELF 中的函数入口。

### 3.2 在函数入口设置硬件断点

将下面命令中的 `0x080006e8` 替换为上一步实际查到的地址：

```cmd
D:\OpenOCD\xpack-openocd-0.12.0-6\bin\openocd.exe ^
  -s D:\OpenOCD\xpack-openocd-0.12.0-6\openocd\scripts ^
  -f interface/cmsis-dap.cfg ^
  -c "transport select swd" ^
  -f target/stm32f4x.cfg ^
  -c "adapter speed 1000" ^
  -c "init; reset halt; bp 0x080006e8 2 hw; resume; wait_halt 5000; echo PC=[reg pc]; echo LR=[reg lr]; echo MSP=[reg msp]; rbp 0x080006e8; reset run; exit"
```

关键命令：

- `reset halt`：复位后立即暂停；
- `bp 地址 2 hw`：在 Thumb 函数入口设置长度为 2 字节的硬件断点；
- `resume`：开始运行；
- `wait_halt 5000`：最多等待 5 秒让断点命中；
- `reg lr`：读取进入错误处理函数时的返回地址；
- `rbp 地址`：移除断点；
- `reset run`：调试结束后恢复运行状态。

本次故障输出：

```text
PC=pc (/32): 0x080006e8
LR=lr (/32): 0x080008e1
```

### 3.3 理解 Thumb 状态位

STM32F407 使用 Thumb 指令集。函数调用返回地址 LR 的最低位通常为 1，用于表示 Thumb 状态，并不是实际指令地址的一部分。

因此：

```text
LR                = 0x080008e1
LR & 0xFFFFFFFE   = 0x080008e0
```

解析源码时使用清除最低位后的地址：

```cmd
arm-none-eabi-addr2line -e build\Debug\Bootloader.elf -f -C 0x080008e0
```

本次结果指向：

```text
MX_SDIO_SD_Init
Core/Src/sdio.c
```

随后检查该位置附近源码和反汇编，确认是 `HAL_SD_Init()` 返回失败后调用了 `Error_Handler()`。

### 3.4 用反汇编确认调用指令

如果 `addr2line` 只显示调用后的下一行，可以查看反汇编：

```cmd
arm-none-eabi-objdump -d -S build\Debug\Bootloader.elf > build\Debug\Bootloader.disasm.txt
notepad build\Debug\Bootloader.disasm.txt
```

搜索：

```text
<Error_Handler>
```

或者搜索清除 Thumb 位后的 LR 附近地址，例如：

```text
080008e0
```

典型调用序列如下：

```text
HAL_SD_Init(...)
比较返回值
条件跳转
bl Error_Handler
返回地址位于 LR 指向的位置
```

## 4. 可选方法：OpenOCD 配合 GDB

当需要调用栈、变量或逐行单步时，可以使用两个 CMD 窗口。

第一个窗口启动 OpenOCD：

```cmd
D:\OpenOCD\xpack-openocd-0.12.0-6\bin\openocd.exe ^
  -s D:\OpenOCD\xpack-openocd-0.12.0-6\openocd\scripts ^
  -f interface/cmsis-dap.cfg ^
  -c "transport select swd" ^
  -f target/stm32f4x.cfg ^
  -c "adapter speed 1000"
```

第二个窗口进入 Bootloader 目录并启动 GDB：

```cmd
arm-none-eabi-gdb build\Debug\Bootloader.elf
```

在 GDB 中依次执行：

```gdb
target extended-remote localhost:3333
monitor reset halt
hbreak Error_Handler
continue
info registers pc lr msp
bt
list
```

结束调试：

```gdb
monitor reset run
monitor shutdown
quit
```

`bt` 会尝试显示调用栈。若编译优化、栈损坏或缺少调试信息，调用栈可能不完整，此时 LR 配合 `addr2line` 通常更直接。

## 5. 验证 GPIO 是否实际变化

修复并重新烧录后，可以直接读取 GPIOF 寄存器，不只依赖肉眼观察 LED。

STM32F407 的 GPIOF 基地址是：

```text
0x40021400
```

常用寄存器：

```text
MODER = 0x40021400
ODR   = 0x40021414
```

示例命令：

```cmd
D:\OpenOCD\xpack-openocd-0.12.0-6\bin\openocd.exe ^
  -s D:\OpenOCD\xpack-openocd-0.12.0-6\openocd\scripts ^
  -f interface/cmsis-dap.cfg ^
  -c "transport select swd" ^
  -f target/stm32f4x.cfg ^
  -c "adapter speed 1000" ^
  -c "init; reset run; sleep 1500; halt; echo MODER=[mrw 0x40021400]; echo ODR_1=[mrw 0x40021414]; resume; sleep 550; halt; echo ODR_2=[mrw 0x40021414]; reset run; exit"
```

本项目中：

```text
MODER = 0x00140000  表示PF9、PF10为输出模式
ODR   = 0x00000400  表示PF10为高、PF9为低
ODR   = 0x00000200  表示PF9为高、PF10为低
```

如果 ODR 在两个值之间变化而实物 LED 不变化，应继续检查 LED 极性、限流电阻、连线和实际板级引脚；如果 ODR 不变化，则继续检查任务与 RTOS 时基。

## 6. 常见问题

### OpenOCD 无法连接目标

检查：

- 目标板是否供电；
- DAPLink GND 是否与开发板共地；
- SWDIO 是否连接 PA13；
- SWCLK 是否连接 PA14；
- 调试器是否读取到目标 3.3 V 电平；
- 必要时将速度降至 `adapter speed 100`；
- 如果芯片反复复位，连接 NRST 并尝试 reset halt。

### wait_halt 超时

说明设定时间内没有执行到断点：

- 确认断点地址来自当前 ELF；
- 确认当前 ELF 已重新烧录；
- 增加等待时间；
- 先读取普通 PC，确认 CPU 是否进入了其他故障路径。

### addr2line 显示 `??:0`

通常表示：

- 使用了 BIN/HEX 而不是 ELF；
- ELF 与芯片内固件不是同一次构建；
- Release 优化删除了相关调试信息；
- 输入地址不是有效代码地址；
- LR 的 Thumb 最低位没有清除。

### OpenOCD 中 Windows 路径损坏

OpenOCD 的 `-c` 内容使用 Tcl 解析，反斜杠可能成为转义字符。烧录镜像时使用正斜杠和花括号：

```text
program {D:/path/to/Bootloader.elf} verify reset exit
```

不要直接在 Tcl 命令中使用未保护的：

```text
D:\path\to\Bootloader.elf
```

## 7. 推荐的标准定位顺序

```text
重新Debug编译并烧录
→ 运行后暂停并读取PC
→ addr2line将PC映射到源码
→ 如果进入Error_Handler，查询其本次构建地址
→ 在Error_Handler入口设置硬件断点
→ 复位运行并读取LR
→ 清除LR最低Thumb位
→ addr2line/objdump定位调用者
→ 修复最小根因
→ 重新编译和烧录
→ 读取外设寄存器验证
→ 最后观察实物行为
```

这套流程的重点是始终使用当前 ELF，把运行时地址逐步还原为具体函数和源码，而不是根据表面现象猜测故障模块。

# Build and flash scripts

Run these commands from a normal Windows CMD at the repository root.

```cmd
python scripts\build.py
python scripts\flash.py
```

`build.py` configures CMake, compiles the `Debug` preset and outputs `Bootloader.elf`, `Bootloader.bin` and `Bootloader.hex`.

`flash.py` programs the Debug ELF through DAPLink/CMSIS-DAP, verifies it, resets the STM32 and lets the firmware start normally.

Useful options:

```cmd
python scripts\build.py --clean
python scripts\build.py --preset Release
python scripts\flash.py --dap-serial B1897547C3840B15B1FE4CDBBF997647
python scripts\flash.py --adapter-speed 100
```

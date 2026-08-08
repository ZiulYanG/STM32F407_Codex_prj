# Bootloader scripts

Open a normal Windows CMD in the `Bootloader` directory, then run:

```cmd
python script\build.py
python script\flash.py
```

`build.py` runs the Debug CMake preset and generates `build\Debug\Bootloader.elf`, `.bin`, and `.hex`.

`flash.py` uses the DAPLink CMSIS-DAP interface to program that ELF, verify it, reset the MCU, and let the firmware run.

Useful options:

```cmd
python script\build.py --clean
python script\build.py --preset Release
python script\flash.py --adapter-speed 100
python script\flash.py --dap-serial B1897547C3840B15B1FE4CDBBF997647
```

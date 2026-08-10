# Application scripts

Open a normal Windows CMD in the `Application` directory, then run:

```cmd
python script\build.py
python script\flash.py
```

`build.py` runs the Debug CMake preset and generates `build\Debug\Application.elf`, `.bin`, and `.hex`.

`flash.py` uses the DAPLink CMSIS-DAP interface to program the APP image at `0x08040000`, verify it, and reset the MCU. Until Bootloader jump support is implemented, reset returns to the Bootloader rather than entering the Application.

Useful options:

```cmd
python script\build.py --clean
python script\build.py --preset Release
python script\flash.py --adapter-speed 100
python script\flash.py --dap-serial B1897547C3840B15B1FE4CDBBF997647
```

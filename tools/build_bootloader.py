#!/usr/bin/env python3
"""Build STM32F407 Bootloader artifacts and optionally flash them via DAPLink."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
PROJECT_DIR = REPOSITORY_ROOT / "firmware" / "Bootloader"
DEFAULT_OPENOCD = Path(r"D:\OpenOCD\xpack-openocd-0.12.0-6\bin\openocd.exe")
DEFAULT_OPENOCD_SCRIPTS = Path(r"D:\OpenOCD\xpack-openocd-0.12.0-6\openocd\scripts")
BUILD_PRESETS = ("Debug", "RelWithDebInfo", "Release", "MinSizeRel")


def run(command: list[str], *, cwd: Path | None = None) -> None:
    print("+", subprocess.list2cmdline(command))
    subprocess.run(command, cwd=cwd, check=True)


def require_command(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"Required command is not available in PATH: {name}")


def openocd_command(args: argparse.Namespace, commands: list[str]) -> list[str]:
    if not args.openocd.is_file():
        raise RuntimeError(f"OpenOCD executable not found: {args.openocd}")
    if not args.openocd_scripts.is_dir():
        raise RuntimeError(f"OpenOCD scripts directory not found: {args.openocd_scripts}")

    command = [str(args.openocd), "-s", str(args.openocd_scripts), "-f", "interface/cmsis-dap.cfg"]
    if args.dap_serial:
        command += ["-c", f"adapter serial {args.dap_serial}"]
    command += ["-c", "transport select swd", "-f", "target/stm32f4x.cfg", "-c", f"adapter speed {args.adapter_speed}"]
    for openocd_command_text in commands:
        command += ["-c", openocd_command_text]
    return command


def probe(args: argparse.Namespace) -> None:
    print("Probing the target through the DAPLink CMSIS-DAP interface...")
    run(openocd_command(args, ["init", "exit"]))


def build(args: argparse.Namespace) -> Path:
    timebase_source = PROJECT_DIR / "Core" / "Src" / "stm32f4xx_hal_timebase_tim.c"
    if not timebase_source.is_file():
        raise RuntimeError(
            "CubeMX code has not been regenerated for the TIM7 HAL time base. "
            f"Open '{PROJECT_DIR / 'Bootloader.ioc'}' in STM32CubeMX and click GENERATE CODE first."
        )
    for command_name in ("cmake", "arm-none-eabi-objcopy", "arm-none-eabi-size"):
        require_command(command_name)

    run(["cmake", "--preset", args.preset], cwd=PROJECT_DIR)
    build_command = ["cmake", "--build", "--preset", args.preset, "--parallel"]
    if args.clean:
        build_command.append("--clean-first")
    run(build_command, cwd=PROJECT_DIR)

    build_dir = PROJECT_DIR / "build" / args.preset
    elf_file = build_dir / "Bootloader.elf"
    bin_file = build_dir / "Bootloader.bin"
    hex_file = build_dir / "Bootloader.hex"
    if not elf_file.is_file():
        raise RuntimeError(f"Build finished but the ELF file was not produced: {elf_file}")
    run(["arm-none-eabi-objcopy", "-O", "binary", str(elf_file), str(bin_file)])
    run(["arm-none-eabi-objcopy", "-O", "ihex", str(elf_file), str(hex_file)])
    run(["arm-none-eabi-size", str(elf_file)])

    print("\nBuild artifacts:")
    print(f"  ELF: {elf_file}")
    print(f"  BIN: {bin_file}")
    print(f"  HEX: {hex_file}")
    return elf_file


def flash(args: argparse.Namespace, elf_file: Path) -> None:
    print("\nFlashing Bootloader through DAPLink CMSIS-DAP...")
    run(openocd_command(args, [f"program {elf_file} verify reset exit"]))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", choices=BUILD_PRESETS, default="Debug")
    parser.add_argument("--clean", action="store_true", help="Clean before building.")
    parser.add_argument("--flash", action="store_true", help="Flash the generated ELF through DAPLink.")
    parser.add_argument("--probe", action="store_true", help="Only verify DAPLink-to-target SWD communication.")
    parser.add_argument("--adapter-speed", type=int, default=1000, help="SWD clock in kHz (default: 1000).")
    parser.add_argument("--dap-serial", help="CMSIS-DAP serial number; omit when only one probe is connected.")
    parser.add_argument("--openocd", type=Path, default=DEFAULT_OPENOCD)
    parser.add_argument("--openocd-scripts", type=Path, default=DEFAULT_OPENOCD_SCRIPTS)
    args = parser.parse_args()
    if args.probe and args.flash:
        parser.error("--probe and --flash cannot be used together.")
    if not 100 <= args.adapter_speed <= 24000:
        parser.error("--adapter-speed must be between 100 and 24000 kHz.")
    return args


def main() -> int:
    args = parse_args()
    try:
        if args.probe:
            probe(args)
            return 0
        elf_file = build(args)
        if args.flash:
            flash(args, elf_file)
        return 0
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

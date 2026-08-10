#!/usr/bin/env python3
"""Flash the Application through DAPLink CMSIS-DAP, verify it, and reset the MCU."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_IMAGE = PROJECT_DIR / "build" / "Debug" / "Application.elf"
DEFAULT_OPENOCD = Path(r"D:\\OpenOCD\\xpack-openocd-0.12.0-6\bin\\openocd.exe")
DEFAULT_OPENOCD_SCRIPTS = Path(r"D:\\OpenOCD\\xpack-openocd-0.12.0-6\\openocd\scripts")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE, help="ELF or HEX image to flash.")
    parser.add_argument("--adapter-speed", type=int, default=1000, help="SWD clock in kHz.")
    parser.add_argument("--dap-serial", help="CMSIS-DAP serial; omit when only one DAPLink is connected.")
    parser.add_argument("--openocd", type=Path, default=DEFAULT_OPENOCD)
    parser.add_argument("--openocd-scripts", type=Path, default=DEFAULT_OPENOCD_SCRIPTS)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    image = args.image.resolve()
    openocd = args.openocd.resolve()
    scripts_dir = args.openocd_scripts.resolve()

    try:
        if not image.is_file():
            raise RuntimeError(f"Firmware image not found: {image}")
        if not openocd.is_file():
            raise RuntimeError(f"OpenOCD executable not found: {openocd}")
        if not scripts_dir.is_dir():
            raise RuntimeError(f"OpenOCD scripts directory not found: {scripts_dir}")
        if not 100 <= args.adapter_speed <= 24000:
            raise RuntimeError("--adapter-speed must be between 100 and 24000 kHz")

        command = [str(openocd), "-s", str(scripts_dir), "-f", "interface/cmsis-dap.cfg"]
        if args.dap_serial:
            command += ["-c", f"adapter serial {args.dap_serial}"]
        command += [
            "-c", "transport select swd",
            "-f", "target/stm32f4x.cfg",
            "-c", f"adapter speed {args.adapter_speed}",
            # OpenOCD parses -c text as Tcl. Braces make the image one Tcl
            # argument; forward slashes avoid Windows backslash escapes such
            # as \b being interpreted as a control character.
            "-c", f"program {{{image.as_posix()}}} verify reset exit",
        ]
        print("+", subprocess.list2cmdline(command))
        subprocess.run(command, cwd=PROJECT_DIR, check=True)
        print("Flash, verification, and reset completed.")
        return 0
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

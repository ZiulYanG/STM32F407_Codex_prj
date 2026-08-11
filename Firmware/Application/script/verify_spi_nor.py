#!/usr/bin/env python3
"""Reset the target, capture COM3 startup logs, and verify board storage."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import serial

from image_layout import validate_application_image


PROJECT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_OPENOCD = Path(r"D:\OpenOCD\xpack-openocd-0.12.0-6\bin\openocd.exe")
DEFAULT_OPENOCD_SCRIPTS = Path(r"D:\OpenOCD\xpack-openocd-0.12.0-6\openocd\scripts")
DEFAULT_DAP_SERIAL = "B1897547C3840B15B1FE4CDBBF997647"
DEFAULT_IMAGE = PROJECT_DIR / "build" / "Debug" / "Application.elf"
EXPECTED_JEDEC_LINE = "SPI NOR JEDEC : 52 21 18"
EXPECTED_MODEL_LINE = "SPI NOR model : NM25Q128EVB"
EXPECTED_CHECK_LINE = "SPI NOR check : SUPPORTED"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--capture-seconds", type=float, default=10.0)
    parser.add_argument(
        "--expect-self-test",
        choices=("enabled", "disabled"),
        default="enabled",
    )
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE)
    parser.add_argument("--adapter-speed", type=int, default=100)
    parser.add_argument("--dap-serial", default=DEFAULT_DAP_SERIAL)
    parser.add_argument("--openocd", type=Path, default=DEFAULT_OPENOCD)
    parser.add_argument("--openocd-scripts", type=Path, default=DEFAULT_OPENOCD_SCRIPTS)
    return parser.parse_args()


def program_target(args: argparse.Namespace) -> None:
    command = [
        str(args.openocd.resolve()),
        "-s",
        str(args.openocd_scripts.resolve()),
        "-f",
        "interface/cmsis-dap.cfg",
        "-c",
        f"adapter serial {args.dap_serial}",
        "-c",
        "transport select swd",
        "-f",
        "target/stm32f4x.cfg",
        "-c",
        f"adapter speed {args.adapter_speed}",
        "-c",
        f"program {{{args.image.resolve().as_posix()}}} verify reset exit",
    ]
    print("+", subprocess.list2cmdline(command))
    subprocess.run(command, cwd=PROJECT_DIR, check=True)


def main() -> int:
    args = parse_args()
    try:
        if not args.openocd.is_file():
            raise RuntimeError(f"OpenOCD executable not found: {args.openocd}")
        if not args.openocd_scripts.is_dir():
            raise RuntimeError(f"OpenOCD scripts directory not found: {args.openocd_scripts}")
        if not args.image.is_file():
            raise RuntimeError(f"Application image not found: {args.image}")
        if args.capture_seconds <= 0.0:
            raise RuntimeError("--capture-seconds must be greater than zero")
        validate_application_image(args.image.resolve())

        serial_port = serial.Serial()
        serial_port.port = args.port
        serial_port.baudrate = args.baudrate
        serial_port.timeout = 0.1
        serial_port.dtr = False
        serial_port.rts = False
        serial_port.xonxoff = False
        serial_port.rtscts = False
        serial_port.dsrdtr = False
        with serial_port:
            serial_port.reset_input_buffer()
            program_target(args)

            deadline = time.monotonic() + args.capture_seconds
            captured = bytearray()
            while time.monotonic() < deadline:
                captured.extend(serial_port.read(serial_port.in_waiting or 1))

        log = captured.decode("utf-8", errors="replace")
        print("===== STARTUP LOG =====")
        print(log)

        expected_lines = [
            EXPECTED_JEDEC_LINE,
            EXPECTED_MODEL_LINE,
            EXPECTED_CHECK_LINE,
            "Storage spi_nor0: OPEN",
            "Storage eeprom0 : OPEN",
            "Storage partitions: READY",
            "EEPROM address: 0x50 (7-bit)",
            "EEPROM probe  : PASS",
            "RTOS objects   : STATIC",
            "RTOS heap state        : UNUSED",
            "RTOS health    : PASS",
            "APP log drops  : 0",
        ]
        destructive_lines = (
            "SPI NOR test   :",
            "SPI NOR retained:",
            "SPI NOR erase  :",
            "SPI NOR program:",
            "SPI NOR verify :",
            "SPI NOR cross-page:",
            "SPI NOR flash-end :",
            "SPI NOR bounds    :",
            "SPI NOR cleanup:",
            "SPI NOR R/W test:",
            "EEPROM test   :",
            "EEPROM backup :",
            "EEPROM program:",
            "EEPROM verify :",
            "EEPROM bounds :",
            "EEPROM restore:",
            "EEPROM R/W test:",
        )
        if args.expect_self_test == "enabled":
            expected_lines.extend(
                (
                    "SPI NOR self-test: ENABLED",
                    "SPI NOR test   : 0x00FFF000, 64 bytes",
                    "SPI NOR retained: PASS",
                    "SPI NOR erase  : PASS",
                    "SPI NOR program: PASS",
                    "SPI NOR verify : PASS",
                    "SPI NOR cross-page: PASS",
                    "SPI NOR flash-end : PASS",
                    "SPI NOR bounds    : PASS",
                    "SPI NOR cleanup: PASS",
                    "SPI NOR R/W test: PASS",
                    "EEPROM self-test: ENABLED",
                    "EEPROM test   : 0xF6, 10 bytes",
                    "EEPROM backup : PASS",
                    "EEPROM program: PASS",
                    "EEPROM verify : PASS",
                    "EEPROM bounds : PASS",
                    "EEPROM restore: PASS",
                    "EEPROM R/W test: PASS",
                )
            )
        else:
            expected_lines.append("SPI NOR self-test: DISABLED")
            expected_lines.append("EEPROM self-test: DISABLED")
            unexpected_lines = [line for line in destructive_lines if line in log]
            if unexpected_lines:
                raise RuntimeError(
                    "disabled SPI NOR self-test emitted destructive-test logs: "
                    + ", ".join(repr(line) for line in unexpected_lines)
                )

        missing_lines = [line for line in expected_lines if line not in log]
        if missing_lines:
            raise RuntimeError(
                "SPI NOR verification failed; missing " + ", ".join(repr(line) for line in missing_lines)
            )

        print("Storage hardware verification passed.")
        return 0
    except (OSError, RuntimeError, serial.SerialException, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

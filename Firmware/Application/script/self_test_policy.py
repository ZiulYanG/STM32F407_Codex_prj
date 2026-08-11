#!/usr/bin/env python3
"""Validate that an Application ELF obeys all storage self-test policies."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path


SELF_TESTS = (
    (
        "SPI NOR",
        "APP_ENABLE_SPI_NOR_SELF_TEST",
        b"SPI NOR self-test: ENABLED",
        b"SPI NOR self-test: DISABLED",
        "app_flash_run_rw_test",
    ),
    (
        "EEPROM",
        "APP_ENABLE_EEPROM_SELF_TEST",
        b"EEPROM self-test: ENABLED",
        b"EEPROM self-test: DISABLED",
        "app_eeprom_run_rw_test",
    ),
)


def _app_main_compile_command(build_dir: Path) -> str:
    database_path = build_dir / "compile_commands.json"
    if not database_path.is_file():
        raise RuntimeError(f"Compile database not found: {database_path}")

    entries = json.loads(database_path.read_text(encoding="utf-8"))
    for entry in entries:
        if Path(entry["file"]).as_posix().endswith("/app/app_main.c"):
            if "command" in entry:
                return entry["command"]
            return subprocess.list2cmdline(entry["arguments"])
    raise RuntimeError("app/app_main.c compile command was not found")


def validate_self_test_policy(elf: Path, preset: str, objdump: str = "arm-none-eabi-objdump") -> None:
    expected_enabled = preset == "Debug"
    build_dir = elf.parent
    compile_command = _app_main_compile_command(build_dir)
    elf_data = elf.read_bytes()
    symbols = subprocess.run(
        [objdump, "-t", str(elf)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout

    for name, define, enabled_marker, disabled_marker, symbol in SELF_TESTS:
        expected_define = f"{define}={1 if expected_enabled else 0}"
        if expected_define not in compile_command:
            raise RuntimeError(
                f"{preset} compile command does not contain -D{expected_define}"
            )

        expected_marker = enabled_marker if expected_enabled else disabled_marker
        forbidden_marker = disabled_marker if expected_enabled else enabled_marker
        if expected_marker not in elf_data:
            raise RuntimeError(
                f"{preset} ELF is missing marker: {expected_marker.decode()}"
            )
        if forbidden_marker in elf_data:
            raise RuntimeError(
                f"{preset} ELF contains forbidden marker: {forbidden_marker.decode()}"
            )

        symbol_present = symbol in symbols
        if symbol_present != expected_enabled:
            expectation = "present" if expected_enabled else "absent"
            raise RuntimeError(
                f"{preset} ELF requires {symbol} to be {expectation}"
            )

        state = "enabled" if expected_enabled else "disabled"
        print(f"{name} self-test policy verified: {preset} -> {state}")

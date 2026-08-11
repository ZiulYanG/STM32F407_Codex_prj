"""Validate that Application images stay inside the assigned Flash partition."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path


APP_FLASH_START = 0x08040000
APP_FLASH_END = 0x08100000
MCU_FLASH_ADDRESS_LIMIT = 0x10000000


def _validate_elf(image: Path) -> tuple[int, int]:
    objdump = shutil.which("arm-none-eabi-objdump")
    if objdump is None:
        raise RuntimeError("Required command is not in PATH: arm-none-eabi-objdump")

    try:
        result = subprocess.run(
            [objdump, "-h", str(image)],
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"Could not inspect ELF image: {image}") from error

    vector_address: int | None = None
    load_ranges: list[tuple[int, int]] = []
    pending_section: tuple[str, int, int] | None = None

    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 5 and fields[0].isdigit():
            name = fields[1]
            size = int(fields[2], 16)
            vma = int(fields[3], 16)
            lma = int(fields[4], 16)
            pending_section = (name, size, lma)
            if name == ".isr_vector":
                vector_address = vma
            continue

        if pending_section is not None:
            flags = {flag.strip(",") for flag in fields}
            name, size, lma = pending_section
            if size and "ALLOC" in flags and "LOAD" in flags:
                load_ranges.append((lma, lma + size))
            pending_section = None

    if vector_address is None:
        raise RuntimeError(f"ELF image has no .isr_vector section: {image}")
    if vector_address != APP_FLASH_START:
        return vector_address, vector_address

    flash_ranges = [
        (start, end)
        for start, end in load_ranges
        if 0x08000000 <= start < MCU_FLASH_ADDRESS_LIMIT
    ]
    if not flash_ranges:
        raise RuntimeError(f"ELF image has no loadable data in Application Flash: {image}")
    return min(start for start, _ in flash_ranges), max(end for _, end in flash_ranges)


def _validate_hex(image: Path) -> tuple[int, int]:
    upper_address = 0
    lowest: int | None = None
    highest: int | None = None

    try:
        lines = image.read_text(encoding="ascii").splitlines()
    except (OSError, UnicodeError) as error:
        raise RuntimeError(f"Could not read Intel HEX image: {image}") from error

    for line_number, line in enumerate(lines, start=1):
        if not line:
            continue
        if not line.startswith(":"):
            raise RuntimeError(f"Invalid Intel HEX record at line {line_number}")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as error:
            raise RuntimeError(f"Invalid Intel HEX data at line {line_number}") from error
        if len(record) < 5 or len(record) != record[0] + 5:
            raise RuntimeError(f"Invalid Intel HEX length at line {line_number}")
        if sum(record) & 0xFF:
            raise RuntimeError(f"Invalid Intel HEX checksum at line {line_number}")

        byte_count = record[0]
        offset = (record[1] << 8) | record[2]
        record_type = record[3]
        data = record[4 : 4 + byte_count]

        if record_type == 0x00:
            start = upper_address + offset
            end = start + byte_count
            lowest = start if lowest is None else min(lowest, start)
            highest = end if highest is None else max(highest, end)
        elif record_type == 0x01:
            break
        elif record_type == 0x02:
            if byte_count != 2:
                raise RuntimeError(f"Invalid extended segment record at line {line_number}")
            upper_address = int.from_bytes(data, "big") << 4
        elif record_type == 0x04:
            if byte_count != 2:
                raise RuntimeError(f"Invalid extended linear record at line {line_number}")
            upper_address = int.from_bytes(data, "big") << 16

    if lowest is None or highest is None:
        raise RuntimeError(f"Intel HEX image contains no data: {image}")
    return lowest, highest


def validate_application_image(image: Path) -> None:
    """Reject images that could overwrite the Bootloader or exceed APP Flash."""

    suffix = image.suffix.lower()
    if suffix == ".elf":
        start, end = _validate_elf(image)
    elif suffix == ".hex":
        start, end = _validate_hex(image)
    else:
        raise RuntimeError("Application flashing accepts only address-aware ELF or HEX images")

    if start != APP_FLASH_START:
        raise RuntimeError(
            f"Unsafe Application image start 0x{start:08X}; expected 0x{APP_FLASH_START:08X}"
        )
    if end > APP_FLASH_END:
        raise RuntimeError(
            f"Application image ends at 0x{end:08X}, beyond 0x{APP_FLASH_END:08X}"
        )

    print(
        f"Application image layout verified: 0x{start:08X}-0x{end - 1:08X} "
        f"within 0x{APP_FLASH_START:08X}-0x{APP_FLASH_END - 1:08X}"
    )

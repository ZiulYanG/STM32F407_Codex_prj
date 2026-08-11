#!/usr/bin/env python3
"""Configure, build, and generate Application ELF/BIN/HEX artifacts."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from image_layout import validate_application_image
from self_test_policy import validate_self_test_policy
from rtos_policy import validate_rtos_policy


PROJECT_DIR = Path(__file__).resolve().parent.parent
PRESETS = ("Debug", "RelWithDebInfo", "Release", "MinSizeRel")


def run(command: list[str]) -> None:
    print("+", subprocess.list2cmdline(command))
    subprocess.run(command, cwd=PROJECT_DIR, check=True)


def require_command(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"Required command is not in PATH: {name}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", choices=PRESETS, default="Debug")
    parser.add_argument("--clean", action="store_true", help="Clean before building.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if not (PROJECT_DIR / "CMakePresets.json").is_file():
            raise RuntimeError(f"CMake project not found: {PROJECT_DIR}")
        for name in (
            "cmake",
            "arm-none-eabi-objcopy",
            "arm-none-eabi-objdump",
            "arm-none-eabi-size",
        ):
            require_command(name)

        validate_rtos_policy()

        run(["cmake", "--preset", args.preset])
        build_command = ["cmake", "--build", "--preset", args.preset, "--parallel"]
        if args.clean:
            build_command.append("--clean-first")
        run(build_command)

        build_dir = PROJECT_DIR / "build" / args.preset
        elf = build_dir / "Application.elf"
        binary = build_dir / "Application.bin"
        hex_file = build_dir / "Application.hex"
        if not elf.is_file():
            raise RuntimeError(f"Expected ELF was not generated: {elf}")

        validate_application_image(elf)
        validate_self_test_policy(elf, args.preset)
        run(["arm-none-eabi-objcopy", "-O", "binary", str(elf), str(binary)])
        run(["arm-none-eabi-objcopy", "-O", "ihex", str(elf), str(hex_file)])
        run(["arm-none-eabi-size", str(elf)])
        print("\nArtifacts:")
        print(f"  ELF: {elf}")
        print(f"  BIN: {binary}")
        print(f"  HEX: {hex_file}")
        return 0
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

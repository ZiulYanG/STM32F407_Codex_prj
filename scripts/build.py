#!/usr/bin/env python3
"""Configure and build the Bootloader CMake project, then create BIN and HEX."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PROJECT_DIR = REPO_ROOT / "firmware" / "Bootloader"
PRESETS = ("Debug", "RelWithDebInfo", "Release", "MinSizeRel")


def run(command: list[str], cwd: Path) -> None:
    print("+", subprocess.list2cmdline(command))
    subprocess.run(command, cwd=cwd, check=True)


def require_command(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"Required command is not in PATH: {name}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", choices=PRESETS, default="Debug", help="CMake build preset.")
    parser.add_argument("--clean", action="store_true", help="Clean the preset build before compiling.")
    parser.add_argument(
        "--project-dir",
        type=Path,
        default=DEFAULT_PROJECT_DIR,
        help="Bootloader CMake project directory.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_dir = args.project_dir.resolve()

    try:
        if not (project_dir / "CMakePresets.json").is_file():
            raise RuntimeError(f"CMake project not found: {project_dir}")
        for name in ("cmake", "arm-none-eabi-objcopy", "arm-none-eabi-size"):
            require_command(name)

        run(["cmake", "--preset", args.preset], project_dir)
        build_command = ["cmake", "--build", "--preset", args.preset, "--parallel"]
        if args.clean:
            build_command.append("--clean-first")
        run(build_command, project_dir)

        build_dir = project_dir / "build" / args.preset
        elf = build_dir / "Bootloader.elf"
        binary = build_dir / "Bootloader.bin"
        hex_file = build_dir / "Bootloader.hex"
        if not elf.is_file():
            raise RuntimeError(f"Expected ELF was not generated: {elf}")

        run(["arm-none-eabi-objcopy", "-O", "binary", str(elf), str(binary)], project_dir)
        run(["arm-none-eabi-objcopy", "-O", "ihex", str(elf), str(hex_file)], project_dir)
        run(["arm-none-eabi-size", str(elf)], project_dir)

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

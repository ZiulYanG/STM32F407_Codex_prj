#!/usr/bin/env python3
"""Build and run the platform-independent command console tests."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


TEST_DIR = Path(__file__).resolve().parent
COMMON_DIR = TEST_DIR.parents[1]
CONSOLE_DIR = COMMON_DIR / "console"
BUILD_DIR = COMMON_DIR / "build" / "host-tests"


def main() -> int:
    compiler = shutil.which("gcc")
    if compiler is None:
        print("ERROR: host GCC not found in PATH", file=sys.stderr)
        return 1

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    executable = BUILD_DIR / "test_command_console.exe"
    command = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        f"-I{CONSOLE_DIR}",
        str(TEST_DIR / "test_command_console.c"),
        str(CONSOLE_DIR / "command_console.c"),
        "-o",
        str(executable),
    ]
    print("+", subprocess.list2cmdline(command))
    subprocess.run(command, check=True, cwd=TEST_DIR)
    subprocess.run([str(executable)], check=True, cwd=TEST_DIR)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Build and run the platform-independent YMODEM RX/TX tests."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


TEST_DIR = Path(__file__).resolve().parent
COMMON_DIR = TEST_DIR.parents[1]
YMODEM_DIR = COMMON_DIR / "ymodem"
BUILD_DIR = COMMON_DIR / "build" / "host-tests"


def main() -> int:
    compiler = shutil.which("gcc")
    if compiler is None:
        print("ERROR: host GCC not found in PATH", file=sys.stderr)
        return 1

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    executable = BUILD_DIR / "test_ymodem.exe"
    command = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        f"-I{YMODEM_DIR}",
        str(TEST_DIR / "test_ymodem.c"),
        str(YMODEM_DIR / "ymodem.c"),
        "-o",
        str(executable),
    ]
    print("+", subprocess.list2cmdline(command))
    subprocess.run(command, check=True, cwd=TEST_DIR)
    subprocess.run([str(executable)], check=True, cwd=TEST_DIR)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

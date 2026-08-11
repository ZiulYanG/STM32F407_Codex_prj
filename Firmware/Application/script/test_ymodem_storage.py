#!/usr/bin/env python3
"""Build and run the host-side YMODEM storage adapter tests."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
ROOT_DIR = PROJECT_DIR.parent.parent
TEST_SOURCE = PROJECT_DIR / "tests" / "update" / "test_ymodem_storage.c"
STORAGE_DIR = PROJECT_DIR / "components" / "storage"
UPDATE_DIR = PROJECT_DIR / "components" / "update"
YMODEM_DIR = ROOT_DIR / "Firmware" / "Common" / "ymodem"
OUTPUT_DIR = PROJECT_DIR / "build" / "host-tests"
OUTPUT = OUTPUT_DIR / "test_ymodem_storage.exe"


def main() -> int:
    compiler = shutil.which("gcc")
    if compiler is None:
        print("ERROR: gcc is not available", file=sys.stderr)
        return 1

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    command = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        f"-I{STORAGE_DIR}",
        f"-I{UPDATE_DIR}",
        f"-I{YMODEM_DIR}",
        str(TEST_SOURCE),
        str(STORAGE_DIR / "storage.c"),
        str(UPDATE_DIR / "ymodem_storage.c"),
        "-o",
        str(OUTPUT),
    ]
    print("+", subprocess.list2cmdline(command))
    try:
        subprocess.run(command, cwd=PROJECT_DIR, check=True)
        subprocess.run([str(OUTPUT)], cwd=PROJECT_DIR, check=True)
    except subprocess.CalledProcessError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return error.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

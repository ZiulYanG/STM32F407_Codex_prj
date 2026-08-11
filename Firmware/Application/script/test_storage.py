#!/usr/bin/env python3
"""Build and run the host-side storage interface tests."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
TEST_SOURCE = PROJECT_DIR / "tests" / "storage" / "test_storage.c"
STORAGE_SOURCE = PROJECT_DIR / "components" / "storage" / "storage.c"
PARTITION_SOURCE = PROJECT_DIR / "components" / "storage" / "storage_partition.c"
STORAGE_INCLUDE = PROJECT_DIR / "components" / "storage"
OUTPUT_DIR = PROJECT_DIR / "build" / "host-tests"
OUTPUT = OUTPUT_DIR / "test_storage.exe"


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
        f"-I{STORAGE_INCLUDE}",
        str(TEST_SOURCE),
        str(STORAGE_SOURCE),
        str(PARTITION_SOURCE),
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

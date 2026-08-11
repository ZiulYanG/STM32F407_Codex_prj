#!/usr/bin/env python3
"""Run repeated reset/storage/RTOS health verification cycles."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
VERIFY_SCRIPT = PROJECT_DIR / "script" / "verify_spi_nor.py"
DEFAULT_IMAGE = PROJECT_DIR / "build" / "Debug" / "Application.elf"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cycles", type=int, default=20)
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE)
    parser.add_argument("--capture-seconds", type=float, default=3.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.cycles <= 0:
        print("ERROR: --cycles must be greater than zero", file=sys.stderr)
        return 1

    started = time.monotonic()
    for cycle in range(1, args.cycles + 1):
        command = [
            sys.executable,
            str(VERIFY_SCRIPT),
            "--port",
            args.port,
            "--image",
            str(args.image),
            "--expect-self-test",
            "enabled",
            "--capture-seconds",
            str(args.capture_seconds),
        ]
        result = subprocess.run(
            command,
            cwd=PROJECT_DIR,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        if result.returncode != 0:
            print(result.stdout, end="")
            print(f"ERROR: storage soak failed at cycle {cycle}", file=sys.stderr)
            return result.returncode
        print(f"Storage soak cycle {cycle}/{args.cycles}: PASS", flush=True)

    elapsed = time.monotonic() - started
    print(
        f"Storage soak: PASS ({args.cycles} cycles, {elapsed:.1f} seconds)",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Reject CubeMX drift from the P1 static RTOS object policy."""

from __future__ import annotations

from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent


def validate_rtos_policy() -> None:
    ioc = (PROJECT_DIR / "Application.ioc").read_text(encoding="utf-8")
    main_source = (PROJECT_DIR / "Core" / "Src" / "main.c").read_text(
        encoding="utf-8"
    )
    log_source = (
        PROJECT_DIR / "components" / "logging" / "app_log.c"
    ).read_text(encoding="utf-8")

    requirements = {
        "CubeMX defaultTask allocation": "defaultTask,24,512,StartDefaultTask,Default,NULL,Static" in ioc,
        "defaultTask control block": ".cb_mem = &defaultTaskControlBlock" in main_source,
        "defaultTask stack": ".stack_mem = defaultTaskStack" in main_source,
        "static log queue": "xQueueCreateStatic" in log_source,
        "static log task": "xTaskCreateStatic" in log_source,
    }
    failed = [name for name, valid in requirements.items() if not valid]
    if failed:
        raise RuntimeError("RTOS static allocation policy failed: " + ", ".join(failed))

    print("RTOS static allocation policy verified.")

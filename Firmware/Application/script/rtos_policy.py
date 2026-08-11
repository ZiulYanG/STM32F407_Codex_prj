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
    serial_source_path = (
        PROJECT_DIR.parent / "Common" / "serial" / "serial_manager.c"
    )
    serial_source = serial_source_path.read_text(encoding="utf-8")
    log_source = (
        PROJECT_DIR / "components" / "logging" / "app_log.c"
    ).read_text(encoding="utf-8")
    update_source = (
        PROJECT_DIR / "components" / "update" / "update_session.c"
    ).read_text(encoding="utf-8")
    mode_source = (PROJECT_DIR / "app" / "system_mode.c").read_text(
        encoding="utf-8"
    )
    upper_layer_sources = sorted(
        (PROJECT_DIR / "app").rglob("*.c")
    ) + sorted((PROJECT_DIR / "components").rglob("*.c")) + [
        serial_source_path
    ]
    bsp_write_callers = [
        source
        for source in upper_layer_sources
        if "bsp_uart1_write(" in source.read_text(encoding="utf-8")
    ]
    direct_hal_uart_callers = [
        source
        for source in upper_layer_sources
        if "HAL_UART_Transmit(" in source.read_text(encoding="utf-8")
    ]

    requirements = {
        "CubeMX defaultTask allocation": "defaultTask,24,512,StartDefaultTask,Default,NULL,Static" in ioc,
        "defaultTask control block": ".cb_mem = &defaultTaskControlBlock" in main_source,
        "defaultTask stack": ".stack_mem = defaultTaskStack" in main_source,
        "static serial queues": serial_source.count("xQueueCreateStatic") >= 2,
        "static serial RX stream": "xStreamBufferCreateStatic" in serial_source,
        "static serial task": "xTaskCreateStatic" in serial_source,
        "static update queue": "xQueueCreateStatic" in update_source,
        "static update task": "xTaskCreateStatic" in update_source,
        "static system mode event": "xEventGroupCreateStatic" in mode_source,
        "serial manager owns UART writes": (
            bsp_write_callers == [
                serial_source_path
            ] and
            not direct_hal_uart_callers and
            "bsp_uart1_write" not in log_source
        ),
    }
    failed = [name for name, valid in requirements.items() if not valid]
    if failed:
        raise RuntimeError("RTOS static allocation policy failed: " + ", ".join(failed))

    print("RTOS static allocation policy verified.")

#include "boot_console.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "boot_app.h"
#include "boot_log.h"
#include "boot_version.h"
#include "command_console.h"
#include "main.h"
#include "serial_manager.h"

#define BOOT_CONSOLE_TASK_STACK_WORDS 384U
#define BOOT_CONSOLE_TASK_PRIORITY    (tskIDLE_PRIORITY + 1U)
#define BOOT_CONSOLE_WRITE_TIMEOUT_MS 100U
#define BOOT_CONSOLE_RESET_FLUSH_MS   1000U
#define BOOT_APPLICATION_ADDRESS      0x08040000UL

static StaticTask_t s_console_task_control;
static StackType_t s_console_task_stack[BOOT_CONSOLE_TASK_STACK_WORDS];
static TaskHandle_t s_console_task_handle;
static command_console_t s_console;

static int boot_console_write(void *context,
                              const uint8_t *data,
                              size_t length)
{
    size_t offset = 0U;

    (void)context;
    if ((data == NULL) || (length == 0U)) {
        return -1;
    }

    while (offset < length) {
        size_t chunk = length - offset;

        if (chunk > SERIAL_MANAGER_MAX_TX_SIZE) {
            chunk = SERIAL_MANAGER_MAX_TX_SIZE;
        }
        if (serial_manager_write(SERIAL_TX_PROTOCOL,
                                 &data[offset],
                                 chunk,
                                 BOOT_CONSOLE_WRITE_TIMEOUT_MS) !=
            SERIAL_STATUS_OK) {
            return -1;
        }
        offset += chunk;
    }
    return 0;
}

static void boot_console_puts(const char *text)
{
    if (text != NULL) {
        (void)boot_console_write(NULL,
                                 (const uint8_t *)text,
                                 strlen(text));
    }
}

static void boot_console_printf(const char *format, ...)
{
    char buffer[192];
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length <= 0) {
        return;
    }

    if ((size_t)length >= sizeof(buffer)) {
        length = (int)(sizeof(buffer) - 1U);
    }

    (void)boot_console_write(NULL, (const uint8_t *)buffer, (size_t)length);
}

static void boot_console_help(void)
{
    boot_console_puts(
        "Commands:\r\n"
        "  help                         Show this help\r\n"
        "  info                         Show firmware and clock information\r\n"
        "  status                       Show runtime and serial statistics\r\n"
        "  get log_level|window_ms      Read a parameter\r\n"
        "  set log_level <level>        debug|info|warn|error\r\n"
        "  set window_ms <0..30000>     Set boot command window\r\n"
        "  boot stay|app                Stay in bootloader or launch APP\r\n"
        "  reset                        Reset the MCU\r\n");
}

static void boot_console_info(void)
{
    boot_console_printf(
        "STM32F407 Bootloader\r\n"
        "Version       : %s\r\n"
        "SYSCLK        : %lu Hz\r\n"
        "HCLK          : %lu Hz\r\n"
        "PCLK1         : %lu Hz\r\n"
        "PCLK2         : %lu Hz\r\n"
        "APP address   : 0x%08lX\r\n",
        BOOTLOADER_VERSION,
        (unsigned long)HAL_RCC_GetSysClockFreq(),
        (unsigned long)HAL_RCC_GetHCLKFreq(),
        (unsigned long)HAL_RCC_GetPCLK1Freq(),
        (unsigned long)HAL_RCC_GetPCLK2Freq(),
        (unsigned long)BOOT_APPLICATION_ADDRESS);
}

static void boot_console_status(void)
{
    serial_manager_stats_t stats = {0};

    serial_manager_get_stats(&stats);

    boot_console_printf(
        "uptime_ms=%lu log_level=%s window_ms=%lu\r\n"
        "serial rx=%lu tx=%lu rx_overrun=%lu rx_error=%lu "
        "tx_drop=%lu tx_error=%lu log_drop=%lu\r\n"
        "stack_free_words console=%lu serial=%lu\r\n",
        (unsigned long)HAL_GetTick(),
        boot_log_level_name(boot_log_get_level()),
        (unsigned long)boot_app_get_window_ms(),
        (unsigned long)stats.rx_bytes,
        (unsigned long)stats.tx_bytes,
        (unsigned long)stats.rx_hardware_overruns,
        (unsigned long)stats.rx_hardware_errors,
        (unsigned long)stats.tx_dropped,
        (unsigned long)stats.tx_errors,
        (unsigned long)boot_log_get_dropped_count(),
        (unsigned long)boot_console_get_stack_high_water_mark_words(),
        (unsigned long)serial_manager_get_stack_high_water_mark_words());
}

static void boot_console_get_parameter(const char *name)
{
    if (strcmp(name, "log_level") == 0) {
        boot_console_printf("log_level=%s\r\n",
                            boot_log_level_name(boot_log_get_level()));
    } else if (strcmp(name, "window_ms") == 0) {
        boot_console_printf("window_ms=%lu\r\n",
                            (unsigned long)boot_app_get_window_ms());
    } else {
        boot_console_puts("ERR unknown parameter\r\n");
    }
}

static void boot_console_set_parameter(const char *name, const char *value)
{
    if (strcmp(name, "log_level") == 0) {
        boot_log_level_t level;

        if (!boot_log_parse_level(value, &level)) {
            boot_console_puts("ERR level must be debug|info|warn|error\r\n");
            return;
        }

        boot_log_set_level(level);
        boot_console_printf("OK log_level=%s\r\n", boot_log_level_name(level));
        return;
    }

    if (strcmp(name, "window_ms") == 0) {
        char *end = NULL;
        unsigned long window_ms = strtoul(value, &end, 10);

        if ((end == value) || (*end != '\0') || (window_ms > 30000UL)) {
            boot_console_puts("ERR window_ms range is 0..30000\r\n");
            return;
        }

        (void)boot_app_set_window_ms((uint32_t)window_ms);
        boot_console_printf("OK window_ms=%lu\r\n", window_ms);
        return;
    }

    boot_console_puts("ERR unknown parameter\r\n");
}

static void boot_console_execute(void *context, int argc, char *argv[])
{
    (void)context;

    if ((argc == 1) && (strcmp(argv[0], "help") == 0)) {
        boot_console_help();
    } else if ((argc == 1) && (strcmp(argv[0], "info") == 0)) {
        boot_console_info();
    } else if ((argc == 1) && (strcmp(argv[0], "status") == 0)) {
        boot_console_status();
    } else if ((argc == 2) && (strcmp(argv[0], "get") == 0)) {
        boot_console_get_parameter(argv[1]);
    } else if ((argc == 3) && (strcmp(argv[0], "set") == 0)) {
        boot_console_set_parameter(argv[1], argv[2]);
    } else if ((argc == 2) && (strcmp(argv[0], "boot") == 0) &&
               (strcmp(argv[1], "stay") == 0)) {
        boot_app_request_stay();
        boot_console_puts("OK staying in bootloader\r\n");
    } else if ((argc == 2) && (strcmp(argv[0], "boot") == 0) &&
               (strcmp(argv[1], "app") == 0)) {
        boot_console_puts("OK launching application\r\n");
        (void)serial_manager_flush(500U);
        boot_app_request_jump();
    } else if ((argc == 1) && (strcmp(argv[0], "reset") == 0)) {
        boot_console_puts("OK resetting\r\n");
        (void)serial_manager_flush(BOOT_CONSOLE_RESET_FLUSH_MS);
        NVIC_SystemReset();
    } else {
        boot_console_puts("ERR unknown command; type help\r\n");
    }
}

static void boot_console_task(void *argument)
{
    uint8_t input[32];

    (void)argument;
    boot_console_puts("Boot console ready; type help\r\n");
    command_console_show_prompt(&s_console);

    for (;;) {
        size_t received = 0U;

        if (serial_manager_read(SERIAL_MODE_CONSOLE,
                                input,
                                sizeof(input),
                                20U,
                                &received) == SERIAL_STATUS_OK) {
            command_console_feed(&s_console, input, received);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
    }
}

bool boot_console_init(void)
{
    if (s_console_task_handle != NULL) {
        return false;
    }
    if (!command_console_init(&s_console,
                              NULL,
                              boot_console_write,
                              boot_console_execute,
                              "boot> ")) {
        return false;
    }

    s_console_task_handle = xTaskCreateStatic(boot_console_task,
                                               "boot_console",
                                               BOOT_CONSOLE_TASK_STACK_WORDS,
                                               NULL,
                                               BOOT_CONSOLE_TASK_PRIORITY,
                                               s_console_task_stack,
                                               &s_console_task_control);
    return s_console_task_handle != NULL;
}

uint32_t boot_console_get_stack_high_water_mark_words(void)
{
    if (s_console_task_handle == NULL) {
        return 0U;
    }

    return (uint32_t)uxTaskGetStackHighWaterMark(s_console_task_handle);
}

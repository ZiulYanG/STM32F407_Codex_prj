#include "app_console.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_log.h"
#include "app_main.h"
#include "app_version.h"
#include "command_console.h"
#include "FreeRTOS.h"
#include "serial_manager.h"
#include "stm32f4xx_hal.h"
#include "system_mode.h"
#include "task.h"
#include "update_session.h"

#define APP_CONSOLE_TASK_STACK_WORDS 384U
#define APP_CONSOLE_READ_TIMEOUT_MS  20U
#define APP_CONSOLE_WRITE_TIMEOUT_MS 100U
#define APP_CONSOLE_RESET_FLUSH_MS   1000U

static StaticTask_t console_task_control;
static StackType_t console_task_stack[APP_CONSOLE_TASK_STACK_WORDS];
static TaskHandle_t console_task;
static command_console_t command_console;

static int app_console_write(void *context,
                             const uint8_t *data,
                             size_t length)
{
    size_t offset = 0U;

    (void)context;
    while (offset < length)
    {
        size_t chunk = length - offset;

        if (chunk > SERIAL_MANAGER_MAX_TX_SIZE)
        {
            chunk = SERIAL_MANAGER_MAX_TX_SIZE;
        }
        if (serial_manager_write(SERIAL_TX_PROTOCOL,
                                 &data[offset],
                                 chunk,
                                 APP_CONSOLE_WRITE_TIMEOUT_MS) !=
            SERIAL_STATUS_OK)
        {
            return -1;
        }
        offset += chunk;
    }
    return 0;
}

static void app_console_printf(const char *format, ...)
{
    char text[SERIAL_MANAGER_MAX_TX_SIZE];
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    if (length > 0)
    {
        size_t send_length = ((size_t)length >= sizeof(text))
                                 ? sizeof(text) - 1U
                                 : (size_t)length;
        (void)app_console_write(NULL,
                                (const uint8_t *)text,
                                send_length);
    }
}

static bool app_console_parse_u32(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;

    if ((text == NULL) || (value == NULL) || (*text == '\0'))
    {
        return false;
    }
    parsed = strtoul(text, &end, 10);
    if ((*end != '\0') || (parsed > UINT32_MAX))
    {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static void app_console_help(void)
{
    app_console_printf("help                         - show commands\r\n");
    app_console_printf("info                         - firmware information\r\n");
    app_console_printf("status                       - runtime health\r\n");
    app_console_printf("get log_level|heartbeat_ms   - read parameter\r\n");
    app_console_printf("set log_level <level>        - debug/info/warn/error\r\n");
    app_console_printf("set heartbeat_ms <100..10000>- set LED period\r\n");
    app_console_printf("ymodem rx candidate            - receive file to Candidate\r\n");
    app_console_printf("ymodem tx candidate [size]     - send Candidate data\r\n");
    app_console_printf("ymodem status                  - transfer state\r\n");
    app_console_printf("reset                        - software reset\r\n");
}

static void app_console_ymodem_status(void)
{
    update_session_snapshot_t snapshot;

    update_session_get_snapshot(&snapshot);
    app_console_printf("ymodem=%s error=%u bytes=%lu/%lu last_rx=%lu\r\n",
                       update_session_state_name(snapshot.state),
                       (unsigned int)snapshot.error,
                       (unsigned long)snapshot.transferred_bytes,
                       (unsigned long)snapshot.file_size,
                       (unsigned long)snapshot.completed_receive_size);
}

static void app_console_ymodem(int argc, char *argv[])
{
    uint32_t file_size = 0U;

    if ((argc == 3) && (strcmp(argv[1], "rx") == 0) &&
        (strcmp(argv[2], "candidate") == 0))
    {
        if (update_session_request_receive())
        {
            app_console_printf("OK READY YMODEM RX\r\n");
        }
        else
        {
            app_console_printf("ERR transfer busy or storage unavailable\r\n");
        }
    }
    else if (((argc == 3) || (argc == 4)) &&
             (strcmp(argv[1], "tx") == 0) &&
             (strcmp(argv[2], "candidate") == 0))
    {
        if ((argc == 4) && !app_console_parse_u32(argv[3], &file_size))
        {
            app_console_printf("ERR invalid size\r\n");
            return;
        }
        if (update_session_request_send(file_size))
        {
            app_console_printf("OK READY YMODEM TX\r\n");
        }
        else
        {
            app_console_printf("ERR no file size, busy, or storage unavailable\r\n");
        }
    }
    else if ((argc == 2) && (strcmp(argv[1], "status") == 0))
    {
        app_console_ymodem_status();
    }
    else
    {
        app_console_printf("ERR use ymodem rx|tx candidate [size]\r\n");
    }
}

static void app_console_get(const char *name)
{
    if (strcmp(name, "log_level") == 0)
    {
        app_console_printf("log_level=%s\r\n",
                           app_log_level_name(app_log_get_level()));
    }
    else if (strcmp(name, "heartbeat_ms") == 0)
    {
        app_console_printf("heartbeat_ms=%lu\r\n",
                           (unsigned long)app_main_get_heartbeat_ms());
    }
    else
    {
        app_console_printf("ERR unknown parameter\r\n");
    }
}

static void app_console_set(const char *name, const char *value)
{
    app_log_level_t level;
    uint32_t period_ms;

    if (strcmp(name, "log_level") == 0)
    {
        if (!app_log_parse_level(value, &level) ||
            !app_log_set_level(level))
        {
            app_console_printf("ERR expected debug/info/warn/error\r\n");
            return;
        }
        app_console_printf("OK log_level=%s\r\n",
                           app_log_level_name(level));
    }
    else if (strcmp(name, "heartbeat_ms") == 0)
    {
        if (!app_console_parse_u32(value, &period_ms) ||
            !app_main_set_heartbeat_ms(period_ms))
        {
            app_console_printf("ERR heartbeat_ms range 100..10000\r\n");
            return;
        }
        app_console_printf("OK heartbeat_ms=%lu\r\n",
                           (unsigned long)period_ms);
    }
    else
    {
        app_console_printf("ERR unknown parameter\r\n");
    }
}

static void app_console_execute(void *context, int argc, char *argv[])
{
    serial_manager_stats_t stats = {0};

    (void)context;
    if (strcmp(argv[0], "help") == 0)
    {
        app_console_help();
    }
    else if (strcmp(argv[0], "info") == 0)
    {
        app_console_printf("Application %s, SYSCLK=%lu, uptime=%lu ms\r\n",
                           APPLICATION_VERSION,
                           (unsigned long)HAL_RCC_GetSysClockFreq(),
                           (unsigned long)HAL_GetTick());
    }
    else if (strcmp(argv[0], "status") == 0)
    {
        serial_manager_get_stats(&stats);
        app_console_printf("mode=%u tx=%lu/%lu rx=%lu drop=%lu hw=%lu/%lu\r\n",
                           (unsigned int)serial_manager_get_mode(),
                           (unsigned long)stats.tx_messages,
                           (unsigned long)stats.tx_errors,
                           (unsigned long)stats.rx_bytes,
                           (unsigned long)stats.rx_dropped,
                           (unsigned long)stats.rx_hardware_overruns,
                           (unsigned long)stats.rx_hardware_errors);
        app_console_printf("log_drop=%lu heap=%lu min=%lu bytes\r\n",
                           (unsigned long)app_log_get_dropped_count(),
                           (unsigned long)xPortGetFreeHeapSize(),
                           (unsigned long)xPortGetMinimumEverFreeHeapSize());
        app_console_printf("stack_free_words console=%lu serial=%lu\r\n",
                           (unsigned long)
                               app_console_get_stack_high_water_mark_words(),
                           (unsigned long)
                               serial_manager_get_stack_high_water_mark_words());
        app_console_printf("system_mode=%s update_stack=%lu\r\n",
                           system_mode_name(system_mode_get()),
                           (unsigned long)
                               update_session_get_stack_high_water_mark_words());
        app_console_ymodem_status();
    }
    else if ((strcmp(argv[0], "get") == 0) && (argc == 2))
    {
        app_console_get(argv[1]);
    }
    else if ((strcmp(argv[0], "set") == 0) && (argc == 3))
    {
        app_console_set(argv[1], argv[2]);
    }
    else if (strcmp(argv[0], "ymodem") == 0)
    {
        app_console_ymodem(argc, argv);
    }
    else if ((strcmp(argv[0], "reset") == 0) && (argc == 1))
    {
        app_console_printf("OK resetting\r\n");
        (void)serial_manager_flush(APP_CONSOLE_RESET_FLUSH_MS);
        NVIC_SystemReset();
    }
    else
    {
        app_console_printf("ERR unknown command; use help\r\n");
    }
}

static void app_console_task(void *argument)
{
    uint8_t data[32];
    size_t received_length;
    bool suspended = false;

    (void)argument;
    command_console_show_prompt(&command_console);
    for (;;)
    {
        if (serial_manager_get_mode() != SERIAL_MODE_CONSOLE)
        {
            suspended = true;
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }
        if (suspended)
        {
            command_console_show_prompt(&command_console);
            suspended = false;
        }
        if (serial_manager_read(SERIAL_MODE_CONSOLE,
                                data,
                                sizeof(data),
                                APP_CONSOLE_READ_TIMEOUT_MS,
                                &received_length) == SERIAL_STATUS_OK)
        {
            command_console_feed(&command_console, data, received_length);
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
    }
}

bool app_console_init(void)
{
    if ((console_task != NULL) ||
        !command_console_init(&command_console,
                              NULL,
                              app_console_write,
                              app_console_execute,
                              "app> "))
    {
        return false;
    }

    console_task = xTaskCreateStatic(app_console_task,
                                     "app_console",
                                     APP_CONSOLE_TASK_STACK_WORDS,
                                     NULL,
                                     tskIDLE_PRIORITY + 1U,
                                     console_task_stack,
                                     &console_task_control);
    return console_task != NULL;
}

uint32_t app_console_get_stack_high_water_mark_words(void)
{
    return (console_task != NULL)
               ? (uint32_t)uxTaskGetStackHighWaterMark(console_task)
               : 0U;
}

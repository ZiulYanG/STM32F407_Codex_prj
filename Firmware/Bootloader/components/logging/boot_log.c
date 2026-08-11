#include "boot_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "serial_manager.h"
#include "task.h"

#define BOOT_LOG_MESSAGE_SIZE  SERIAL_MANAGER_MAX_TX_SIZE
#define BOOT_LOG_QUEUE_WAIT_MS 0U

static uint32_t dropped_message_count;
static volatile boot_log_level_t minimum_level = BOOT_LOG_INFO;

static void boot_log_record_drop(void)
{
    taskENTER_CRITICAL();
    ++dropped_message_count;
    taskEXIT_CRITICAL();
}

static bool boot_log_vwrite(boot_log_level_t level,
                            const char *task_name,
                            const char *format,
                            va_list arguments)
{
    char message[BOOT_LOG_MESSAGE_SIZE] = {0};
    size_t prefix_length = 0U;
    size_t message_length;
    int formatted_length;

    if ((format == NULL) || (level > BOOT_LOG_ERROR))
    {
        return false;
    }
    if (level < boot_log_get_level())
    {
        return true;
    }
    if (task_name != NULL)
    {
        formatted_length = snprintf(message,
                                    sizeof(message),
                                    "[%s][%s] ",
                                    boot_log_level_name(level),
                                    task_name);
        if ((formatted_length < 0) ||
            ((size_t)formatted_length >= sizeof(message)))
        {
            boot_log_record_drop();
            return false;
        }
        prefix_length = (size_t)formatted_length;
    }

    formatted_length = vsnprintf(&message[prefix_length],
                                 sizeof(message) - prefix_length,
                                 format,
                                 arguments);
    if (formatted_length < 0)
    {
        boot_log_record_drop();
        return false;
    }
    message_length = ((size_t)formatted_length >=
                      (sizeof(message) - prefix_length))
                         ? sizeof(message) - 1U
                         : prefix_length + (size_t)formatted_length;
    if (serial_manager_write(SERIAL_TX_LOG,
                             (const uint8_t *)message,
                             message_length,
                             BOOT_LOG_QUEUE_WAIT_MS) != SERIAL_STATUS_OK)
    {
        boot_log_record_drop();
        return false;
    }
    return true;
}

bool boot_log_printf(const char *format, ...)
{
    va_list arguments;
    bool result;

    va_start(arguments, format);
    result = boot_log_vwrite(BOOT_LOG_INFO, NULL, format, arguments);
    va_end(arguments);
    return result;
}

bool boot_log_write(boot_log_level_t level,
                    const char *task_name,
                    const char *format,
                    ...)
{
    va_list arguments;
    bool result;

    va_start(arguments, format);
    result = boot_log_vwrite(level, task_name, format, arguments);
    va_end(arguments);
    return result;
}

bool boot_log_flush(uint32_t timeout_ms)
{
    return serial_manager_flush(timeout_ms);
}

bool boot_log_set_level(boot_log_level_t level)
{
    if (level > BOOT_LOG_ERROR)
    {
        return false;
    }
    taskENTER_CRITICAL();
    minimum_level = level;
    taskEXIT_CRITICAL();
    return true;
}

boot_log_level_t boot_log_get_level(void)
{
    boot_log_level_t level;

    taskENTER_CRITICAL();
    level = minimum_level;
    taskEXIT_CRITICAL();
    return level;
}

const char *boot_log_level_name(boot_log_level_t level)
{
    static const char *const names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

    return (level <= BOOT_LOG_ERROR) ? names[level] : "UNKNOWN";
}

bool boot_log_parse_level(const char *text, boot_log_level_t *level)
{
    static const char *const names[] = {"debug", "info", "warn", "error"};
    boot_log_level_t candidate;

    if ((text == NULL) || (level == NULL))
    {
        return false;
    }
    for (candidate = BOOT_LOG_DEBUG;
         candidate <= BOOT_LOG_ERROR;
         candidate = (boot_log_level_t)(candidate + 1))
    {
        if (strcmp(text, names[candidate]) == 0)
        {
            *level = candidate;
            return true;
        }
    }
    return false;
}

uint32_t boot_log_get_dropped_count(void)
{
    uint32_t count;

    taskENTER_CRITICAL();
    count = dropped_message_count;
    taskEXIT_CRITICAL();
    return count;
}

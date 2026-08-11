#include "app_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "serial_manager.h"
#include "task.h"

#define APP_LOG_MESSAGE_SIZE  SERIAL_MANAGER_MAX_TX_SIZE
#define APP_LOG_QUEUE_WAIT_MS 0U

static uint32_t dropped_message_count;
static volatile app_log_level_t minimum_level = APP_LOG_INFO;

static void app_log_record_drop(void)
{
    taskENTER_CRITICAL();
    ++dropped_message_count;
    taskEXIT_CRITICAL();
}

static bool app_log_vwrite(app_log_level_t level,
                           const char *task_name,
                           const char *format,
                           va_list arguments)
{
    char message[APP_LOG_MESSAGE_SIZE] = {0};
    size_t message_length;
    size_t prefix_length = 0U;
    int formatted_length;

    if ((format == NULL) || (level > APP_LOG_ERROR))
    {
        return false;
    }
    if (level < app_log_get_level())
    {
        return true;
    }
    if (task_name != NULL)
    {
        formatted_length = snprintf(message,
                                    sizeof(message),
                                    "[%s][%s] ",
                                    app_log_level_name(level),
                                    task_name);
        if ((formatted_length < 0) ||
            ((size_t)formatted_length >= sizeof(message)))
        {
            app_log_record_drop();
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
        app_log_record_drop();
        return false;
    }

    message_length = ((size_t)formatted_length >=
                      (sizeof(message) - prefix_length))
                         ? sizeof(message) - 1U
                         : prefix_length + (size_t)formatted_length;
    if (serial_manager_write(SERIAL_TX_LOG,
                             (const uint8_t *)message,
                             message_length,
                             APP_LOG_QUEUE_WAIT_MS) != SERIAL_STATUS_OK)
    {
        app_log_record_drop();
        return false;
    }

    return true;
}

bool app_log_printf(const char *format, ...)
{
    va_list arguments;
    bool result;

    va_start(arguments, format);
    result = app_log_vwrite(APP_LOG_INFO, NULL, format, arguments);
    va_end(arguments);
    return result;
}

bool app_log_write(app_log_level_t level,
                   const char *task_name,
                   const char *format,
                   ...)
{
    va_list arguments;
    bool result;

    va_start(arguments, format);
    result = app_log_vwrite(level, task_name, format, arguments);
    va_end(arguments);
    return result;
}

bool app_log_set_level(app_log_level_t level)
{
    if (level > APP_LOG_ERROR)
    {
        return false;
    }
    taskENTER_CRITICAL();
    minimum_level = level;
    taskEXIT_CRITICAL();
    return true;
}

app_log_level_t app_log_get_level(void)
{
    app_log_level_t level;

    taskENTER_CRITICAL();
    level = minimum_level;
    taskEXIT_CRITICAL();
    return level;
}

const char *app_log_level_name(app_log_level_t level)
{
    static const char *const names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

    return (level <= APP_LOG_ERROR) ? names[level] : "UNKNOWN";
}

bool app_log_parse_level(const char *text, app_log_level_t *level)
{
    static const char *const names[] = {"debug", "info", "warn", "error"};
    app_log_level_t candidate;

    if ((text == NULL) || (level == NULL))
    {
        return false;
    }
    for (candidate = APP_LOG_DEBUG;
         candidate <= APP_LOG_ERROR;
         candidate = (app_log_level_t)(candidate + 1))
    {
        if (strcmp(text, names[candidate]) == 0)
        {
            *level = candidate;
            return true;
        }
    }
    return false;
}

uint32_t app_log_get_dropped_count(void)
{
    uint32_t count;

    taskENTER_CRITICAL();
    count = dropped_message_count;
    taskEXIT_CRITICAL();

    return count;
}

#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_LOG_DEBUG = 0,
    APP_LOG_INFO,
    APP_LOG_WARNING,
    APP_LOG_ERROR
} app_log_level_t;

bool app_log_printf(const char *format, ...);
bool app_log_write(app_log_level_t level,
                   const char *task_name,
                   const char *format,
                   ...);
bool app_log_set_level(app_log_level_t level);
app_log_level_t app_log_get_level(void);
const char *app_log_level_name(app_log_level_t level);
bool app_log_parse_level(const char *text, app_log_level_t *level);
uint32_t app_log_get_dropped_count(void);

#define APP_TASK_LOG_DEBUG(task_name, ...) \
    app_log_write(APP_LOG_DEBUG, task_name, __VA_ARGS__)
#define APP_TASK_LOG_INFO(task_name, ...) \
    app_log_write(APP_LOG_INFO, task_name, __VA_ARGS__)
#define APP_TASK_LOG_WARNING(task_name, ...) \
    app_log_write(APP_LOG_WARNING, task_name, __VA_ARGS__)
#define APP_TASK_LOG_ERROR(task_name, ...) \
    app_log_write(APP_LOG_ERROR, task_name, __VA_ARGS__)

#endif /* APP_LOG_H */

#ifndef BOOT_LOG_H
#define BOOT_LOG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BOOT_LOG_DEBUG = 0,
    BOOT_LOG_INFO,
    BOOT_LOG_WARNING,
    BOOT_LOG_ERROR
} boot_log_level_t;

/** Format and enqueue a log message without blocking the calling task. */
bool boot_log_printf(const char *format, ...);
bool boot_log_write(boot_log_level_t level,
                    const char *task_name,
                    const char *format,
                    ...);
bool boot_log_set_level(boot_log_level_t level);
boot_log_level_t boot_log_get_level(void);
const char *boot_log_level_name(boot_log_level_t level);
bool boot_log_parse_level(const char *text, boot_log_level_t *level);

/** Block the calling task until every previously queued log has been sent. */
bool boot_log_flush(uint32_t timeout_ms);

/** Return how many messages could not be queued or transmitted. */
uint32_t boot_log_get_dropped_count(void);

#define BOOT_TASK_LOG_DEBUG(task_name, ...) \
    boot_log_write(BOOT_LOG_DEBUG, task_name, __VA_ARGS__)
#define BOOT_TASK_LOG_INFO(task_name, ...) \
    boot_log_write(BOOT_LOG_INFO, task_name, __VA_ARGS__)
#define BOOT_TASK_LOG_WARNING(task_name, ...) \
    boot_log_write(BOOT_LOG_WARNING, task_name, __VA_ARGS__)
#define BOOT_TASK_LOG_ERROR(task_name, ...) \
    boot_log_write(BOOT_LOG_ERROR, task_name, __VA_ARGS__)

#endif /* BOOT_LOG_H */

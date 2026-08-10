#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdbool.h>
#include <stdint.h>

bool app_log_init(void);
bool app_log_printf(const char *format, ...);
uint32_t app_log_get_dropped_count(void);

#endif /* APP_LOG_H */

#ifndef BOOT_LOG_H
#define BOOT_LOG_H

#include <stdbool.h>
#include <stdint.h>

/** Create the static log queue and the sole USART1 owner task. */
bool boot_log_init(void);

/** Format and enqueue a log message without blocking the calling task. */
bool boot_log_printf(const char *format, ...);

/** Return how many messages could not be queued or transmitted. */
uint32_t boot_log_get_dropped_count(void);

#endif /* BOOT_LOG_H */

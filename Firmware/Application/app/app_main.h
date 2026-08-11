#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdbool.h>
#include <stdint.h>

/** Queue the Application startup diagnostics before the scheduler starts. */
void app_main_init(void);

/** Run the main Application heartbeat task. This function never returns. */
void app_main_task(void *argument);

bool app_main_set_heartbeat_ms(uint32_t period_ms);
uint32_t app_main_get_heartbeat_ms(void);

#endif /* APP_MAIN_H */

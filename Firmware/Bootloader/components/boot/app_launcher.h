#ifndef APP_LAUNCHER_H
#define APP_LAUNCHER_H

#include <stdint.h>

typedef enum
{
    APP_LAUNCH_VALID = 0,
    APP_LAUNCH_INVALID_ARGUMENT,
    APP_LAUNCH_INVALID_ALIGNMENT,
    APP_LAUNCH_INVALID_STACK,
    APP_LAUNCH_INVALID_RESET_HANDLER
} app_launch_status_t;

typedef struct
{
    uint32_t vector_base;
    uint32_t initial_msp;
    uint32_t reset_handler;
} app_launch_target_t;

/** Inspect the first two vector-table entries without changing MCU state. */
app_launch_status_t app_launcher_inspect(uint32_t vector_base,
                                         uint32_t flash_end,
                                         app_launch_target_t *target);

const char *app_launcher_status_string(app_launch_status_t status);

/** Stop Bootloader execution and transfer control to a validated target. */
__attribute__((noreturn)) void app_launcher_jump(const app_launch_target_t *target);

#endif /* APP_LAUNCHER_H */

#ifndef SYSTEM_MODE_H
#define SYSTEM_MODE_H

#include <stdbool.h>

#include "FreeRTOS.h"

typedef enum
{
    SYSTEM_MODE_NORMAL = 0,
    SYSTEM_MODE_FILE_RECEIVE,
    SYSTEM_MODE_FILE_SEND,
    SYSTEM_MODE_UPGRADE_VERIFY,
    SYSTEM_MODE_REBOOT_PENDING
} system_mode_t;

bool system_mode_init(void);
bool system_mode_enter(system_mode_t mode);
void system_mode_restore_normal(void);
system_mode_t system_mode_get(void);
const char *system_mode_name(system_mode_t mode);
bool system_mode_wait_normal(TickType_t timeout_ticks);

#endif /* SYSTEM_MODE_H */

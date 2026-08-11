#include "system_mode.h"

#include "event_groups.h"
#include "task.h"

#define SYSTEM_MODE_NORMAL_BIT (1UL << 0)

static StaticEventGroup_t mode_event_control;
static EventGroupHandle_t mode_events;
static volatile system_mode_t current_mode = SYSTEM_MODE_NORMAL;

static bool system_mode_is_valid(system_mode_t mode)
{
    return mode <= SYSTEM_MODE_REBOOT_PENDING;
}

bool system_mode_init(void)
{
    if (mode_events != NULL)
    {
        return false;
    }
    mode_events = xEventGroupCreateStatic(&mode_event_control);
    if (mode_events == NULL)
    {
        return false;
    }
    current_mode = SYSTEM_MODE_NORMAL;
    (void)xEventGroupSetBits(mode_events, SYSTEM_MODE_NORMAL_BIT);
    return true;
}

bool system_mode_enter(system_mode_t mode)
{
    bool accepted = false;

    if ((mode_events == NULL) || !system_mode_is_valid(mode) ||
        (mode == SYSTEM_MODE_NORMAL))
    {
        return false;
    }

    taskENTER_CRITICAL();
    if (current_mode == SYSTEM_MODE_NORMAL)
    {
        current_mode = mode;
        accepted = true;
    }
    taskEXIT_CRITICAL();
    if (accepted)
    {
        (void)xEventGroupClearBits(mode_events, SYSTEM_MODE_NORMAL_BIT);
    }
    return accepted;
}

void system_mode_restore_normal(void)
{
    if (mode_events == NULL)
    {
        return;
    }
    taskENTER_CRITICAL();
    current_mode = SYSTEM_MODE_NORMAL;
    taskEXIT_CRITICAL();
    (void)xEventGroupSetBits(mode_events, SYSTEM_MODE_NORMAL_BIT);
}

system_mode_t system_mode_get(void)
{
    system_mode_t mode;

    taskENTER_CRITICAL();
    mode = current_mode;
    taskEXIT_CRITICAL();
    return mode;
}

const char *system_mode_name(system_mode_t mode)
{
    static const char *const names[] = {
        "NORMAL",
        "FILE_RECEIVE",
        "FILE_SEND",
        "UPGRADE_VERIFY",
        "REBOOT_PENDING",
    };

    return system_mode_is_valid(mode) ? names[mode] : "UNKNOWN";
}

bool system_mode_wait_normal(TickType_t timeout_ticks)
{
    EventBits_t bits;

    if (mode_events == NULL)
    {
        return false;
    }
    bits = xEventGroupWaitBits(mode_events,
                               SYSTEM_MODE_NORMAL_BIT,
                               pdFALSE,
                               pdTRUE,
                               timeout_ticks);
    return (bits & SYSTEM_MODE_NORMAL_BIT) != 0U;
}

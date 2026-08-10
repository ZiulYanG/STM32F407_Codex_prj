#include "boot_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp_uart1.h"
#include "queue.h"
#include "task.h"

#define BOOT_LOG_MESSAGE_SIZE       160U
#define BOOT_LOG_QUEUE_DEPTH        24U
#define BOOT_LOG_TASK_STACK_WORDS   512U
#define BOOT_LOG_UART_TIMEOUT_MS    1000U

typedef struct
{
    TaskHandle_t completion_task;
    uint16_t length;
    char text[BOOT_LOG_MESSAGE_SIZE];
} boot_log_message_t;

static StaticQueue_t log_queue_control;
static uint8_t log_queue_storage[BOOT_LOG_QUEUE_DEPTH * sizeof(boot_log_message_t)];
static QueueHandle_t log_queue;

static StaticTask_t log_task_control;
static StackType_t log_task_stack[BOOT_LOG_TASK_STACK_WORDS];
static TaskHandle_t log_task;

static uint32_t dropped_message_count;

static void boot_log_record_drop(void)
{
    taskENTER_CRITICAL();
    ++dropped_message_count;
    taskEXIT_CRITICAL();
}

static void boot_log_task(void *argument)
{
    boot_log_message_t message;

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(log_queue, &message, portMAX_DELAY) == pdPASS)
        {
            if ((message.length > 0U) &&
                (bsp_uart1_write((const uint8_t *)message.text,
                                 message.length,
                                 BOOT_LOG_UART_TIMEOUT_MS) != 0))
            {
                boot_log_record_drop();
            }

            if (message.completion_task != NULL)
            {
                xTaskNotifyGive(message.completion_task);
            }
        }
    }
}

bool boot_log_init(void)
{
    if ((log_queue != NULL) || (log_task != NULL))
    {
        return false;
    }

    log_queue = xQueueCreateStatic(BOOT_LOG_QUEUE_DEPTH,
                                   sizeof(boot_log_message_t),
                                   log_queue_storage,
                                   &log_queue_control);
    if (log_queue == NULL)
    {
        return false;
    }

    log_task = xTaskCreateStatic(boot_log_task,
                                 "boot_log",
                                 BOOT_LOG_TASK_STACK_WORDS,
                                 NULL,
                                 tskIDLE_PRIORITY + 1U,
                                 log_task_stack,
                                 &log_task_control);
    return log_task != NULL;
}

bool boot_log_printf(const char *format, ...)
{
    boot_log_message_t message = {0};
    va_list arguments;
    int formatted_length;

    if ((format == NULL) || (log_queue == NULL))
    {
        return false;
    }

    va_start(arguments, format);
    formatted_length = vsnprintf(message.text,
                                 sizeof(message.text),
                                 format,
                                 arguments);
    va_end(arguments);

    if (formatted_length < 0)
    {
        boot_log_record_drop();
        return false;
    }

    message.length = (formatted_length >= (int)sizeof(message.text))
                         ? (uint16_t)(sizeof(message.text) - 1U)
                         : (uint16_t)formatted_length;

    if (xQueueSend(log_queue, &message, 0U) != pdPASS)
    {
        boot_log_record_drop();
        return false;
    }

    return true;
}

bool boot_log_flush(uint32_t timeout_ms)
{
    boot_log_message_t barrier = {0};
    TickType_t timeout_ticks;

    if ((log_queue == NULL) ||
        (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) ||
        (xTaskGetCurrentTaskHandle() == log_task))
    {
        return false;
    }

    timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    barrier.completion_task = xTaskGetCurrentTaskHandle();

    if (xQueueSend(log_queue, &barrier, timeout_ticks) != pdPASS)
    {
        return false;
    }

    return ulTaskNotifyTake(pdTRUE, timeout_ticks) == 1U;
}

uint32_t boot_log_get_dropped_count(void)
{
    uint32_t count;

    taskENTER_CRITICAL();
    count = dropped_message_count;
    taskEXIT_CRITICAL();

    return count;
}

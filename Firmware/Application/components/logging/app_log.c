#include "app_log.h"

#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "bsp_uart1.h"
#include "queue.h"
#include "task.h"

#define APP_LOG_MESSAGE_SIZE       160U
#define APP_LOG_QUEUE_DEPTH        24U
#define APP_LOG_TASK_STACK_WORDS   512U
#define APP_LOG_UART_TIMEOUT_MS    1000U
#define APP_LOG_QUEUE_WAIT_MS      100U

typedef struct
{
    uint16_t length;
    char text[APP_LOG_MESSAGE_SIZE];
} app_log_message_t;

static StaticQueue_t log_queue_control;
static uint8_t log_queue_storage[APP_LOG_QUEUE_DEPTH * sizeof(app_log_message_t)];
static QueueHandle_t log_queue;

static StaticTask_t log_task_control;
static StackType_t log_task_stack[APP_LOG_TASK_STACK_WORDS];
static TaskHandle_t log_task;

static uint32_t dropped_message_count;

static void app_log_record_drop(void)
{
    taskENTER_CRITICAL();
    ++dropped_message_count;
    taskEXIT_CRITICAL();
}

static void app_log_task(void *argument)
{
    app_log_message_t message;

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(log_queue, &message, portMAX_DELAY) == pdPASS)
        {
            if (bsp_uart1_write((const uint8_t *)message.text,
                                message.length,
                                APP_LOG_UART_TIMEOUT_MS) != 0)
            {
                app_log_record_drop();
            }
        }
    }
}

bool app_log_init(void)
{
    if ((log_queue != NULL) || (log_task != NULL))
    {
        return false;
    }

    log_queue = xQueueCreateStatic(APP_LOG_QUEUE_DEPTH,
                                   sizeof(app_log_message_t),
                                   log_queue_storage,
                                   &log_queue_control);
    if (log_queue == NULL)
    {
        return false;
    }

    log_task = xTaskCreateStatic(app_log_task,
                                 "app_log",
                                 APP_LOG_TASK_STACK_WORDS,
                                 NULL,
                                 tskIDLE_PRIORITY + 1U,
                                 log_task_stack,
                                 &log_task_control);
    return log_task != NULL;
}

bool app_log_printf(const char *format, ...)
{
    app_log_message_t message = {0};
    TickType_t queue_wait = 0U;
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
        app_log_record_drop();
        return false;
    }

    message.length = (formatted_length >= (int)sizeof(message.text))
                         ? (uint16_t)(sizeof(message.text) - 1U)
                         : (uint16_t)formatted_length;

    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        queue_wait = pdMS_TO_TICKS(APP_LOG_QUEUE_WAIT_MS);
    }

    if (xQueueSend(log_queue, &message, queue_wait) != pdPASS)
    {
        app_log_record_drop();
        return false;
    }

    return true;
}

uint32_t app_log_get_dropped_count(void)
{
    uint32_t count;

    taskENTER_CRITICAL();
    count = dropped_message_count;
    taskEXIT_CRITICAL();

    return count;
}

uint32_t app_log_get_stack_high_water_mark_words(void)
{
    if (log_task == NULL)
    {
        return 0U;
    }

    return (uint32_t)uxTaskGetStackHighWaterMark(log_task);
}

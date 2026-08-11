#include "serial_manager.h"

#include <string.h>

#include "FreeRTOS.h"
#include "bsp_uart1.h"
#include "queue.h"
#include "stream_buffer.h"
#include "task.h"

#define SERIAL_NORMAL_TX_QUEUE_DEPTH 24U
#define SERIAL_URGENT_TX_QUEUE_DEPTH 8U
#define SERIAL_RX_STREAM_SIZE        2048U
#define SERIAL_TASK_STACK_WORDS      512U
#define SERIAL_TASK_PRIORITY         (configMAX_PRIORITIES - 2U)
#define SERIAL_TASK_POLL_MS          1U
#define SERIAL_RX_DRAIN_CHUNK_SIZE   64U

typedef struct
{
    TaskHandle_t completion_task;
    uint16_t length;
    uint8_t data[SERIAL_MANAGER_MAX_TX_SIZE];
} serial_tx_message_t;

static StaticQueue_t normal_tx_queue_control;
static uint8_t normal_tx_queue_storage[SERIAL_NORMAL_TX_QUEUE_DEPTH *
                                       sizeof(serial_tx_message_t)];
static QueueHandle_t normal_tx_queue;

static StaticQueue_t urgent_tx_queue_control;
static uint8_t urgent_tx_queue_storage[SERIAL_URGENT_TX_QUEUE_DEPTH *
                                       sizeof(serial_tx_message_t)];
static QueueHandle_t urgent_tx_queue;

static StaticStreamBuffer_t rx_stream_control;
static uint8_t rx_stream_storage[SERIAL_RX_STREAM_SIZE + 1U];
static StreamBufferHandle_t rx_stream;

static StaticTask_t serial_task_control;
static StackType_t serial_task_stack[SERIAL_TASK_STACK_WORDS];
static TaskHandle_t serial_task;

static volatile serial_mode_t active_mode = SERIAL_MODE_CONSOLE;
static serial_manager_stats_t manager_stats;
static bool initialized;

static bool serial_mode_is_valid(serial_mode_t mode)
{
    return (mode == SERIAL_MODE_CONSOLE) ||
           (mode == SERIAL_MODE_YMODEM) ||
           (mode == SERIAL_MODE_MODBUS);
}

static void serial_record_tx_drop(void)
{
    taskENTER_CRITICAL();
    ++manager_stats.tx_dropped;
    taskEXIT_CRITICAL();
}

static void serial_drain_rx(void)
{
    uint8_t data[SERIAL_RX_DRAIN_CHUNK_SIZE];
    size_t read_length;
    size_t accepted_length;

    do
    {
        read_length = bsp_uart1_read(data, sizeof(data));
        if (read_length > 0U)
        {
            accepted_length = xStreamBufferSend(rx_stream,
                                                data,
                                                read_length,
                                                0U);
            taskENTER_CRITICAL();
            manager_stats.rx_bytes += (uint32_t)accepted_length;
            manager_stats.rx_dropped +=
                (uint32_t)(read_length - accepted_length);
            taskEXIT_CRITICAL();
        }
    } while (read_length == sizeof(data));
}

static bool serial_take_next_tx(serial_tx_message_t *message,
                                bool *is_log_message)
{
    if (xQueueReceive(urgent_tx_queue, message, 0U) == pdPASS)
    {
        *is_log_message = false;
        return true;
    }

    if (xQueueReceive(normal_tx_queue,
                      message,
                      pdMS_TO_TICKS(SERIAL_TASK_POLL_MS)) == pdPASS)
    {
        *is_log_message = true;
        return true;
    }

    return false;
}

static void serial_manager_task(void *argument)
{
    serial_tx_message_t message;
    bool is_log_message;

    (void)argument;

    for (;;)
    {
        serial_drain_rx();
        if (serial_take_next_tx(&message, &is_log_message))
        {
            /* A mode change can happen after a log was queued.  Recheck at
               the only UART ownership point so stale logs cannot corrupt a
               YMODEM or Modbus session. */
            if (is_log_message && (message.length > 0U) &&
                (serial_manager_get_mode() != SERIAL_MODE_CONSOLE))
            {
                serial_record_tx_drop();
                continue;
            }
            if ((message.length == 0U) ||
                (bsp_uart1_write(message.data,
                                 message.length,
                                 1000U) == 0))
            {
                taskENTER_CRITICAL();
                if (message.length > 0U)
                {
                    ++manager_stats.tx_messages;
                    manager_stats.tx_bytes += message.length;
                }
                taskEXIT_CRITICAL();
            }
            else
            {
                taskENTER_CRITICAL();
                ++manager_stats.tx_errors;
                taskEXIT_CRITICAL();
            }
            if (message.completion_task != NULL)
            {
                xTaskNotifyGive(message.completion_task);
            }
        }
    }
}

bool serial_manager_init(void)
{
    if (initialized || (normal_tx_queue != NULL) ||
        (urgent_tx_queue != NULL) || (rx_stream != NULL) ||
        (serial_task != NULL))
    {
        return false;
    }

    normal_tx_queue = xQueueCreateStatic(SERIAL_NORMAL_TX_QUEUE_DEPTH,
                                         sizeof(serial_tx_message_t),
                                         normal_tx_queue_storage,
                                         &normal_tx_queue_control);
    urgent_tx_queue = xQueueCreateStatic(SERIAL_URGENT_TX_QUEUE_DEPTH,
                                         sizeof(serial_tx_message_t),
                                         urgent_tx_queue_storage,
                                         &urgent_tx_queue_control);
    rx_stream = xStreamBufferCreateStatic(SERIAL_RX_STREAM_SIZE,
                                          1U,
                                          rx_stream_storage,
                                          &rx_stream_control);
    if ((normal_tx_queue == NULL) || (urgent_tx_queue == NULL) ||
        (rx_stream == NULL) || (bsp_uart1_start_receive() != 0))
    {
        return false;
    }

    serial_task = xTaskCreateStatic(serial_manager_task,
                                    "serial_mgr",
                                    SERIAL_TASK_STACK_WORDS,
                                    NULL,
                                    SERIAL_TASK_PRIORITY,
                                    serial_task_stack,
                                    &serial_task_control);
    if (serial_task == NULL)
    {
        return false;
    }

    initialized = true;
    return true;
}

bool serial_manager_flush(uint32_t timeout_ms)
{
    serial_tx_message_t barrier = {0};
    TickType_t timeout_ticks;

    if (!initialized ||
        (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) ||
        (xTaskGetCurrentTaskHandle() == serial_task))
    {
        return false;
    }

    timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    barrier.completion_task = xTaskGetCurrentTaskHandle();
    if (xQueueSend(normal_tx_queue, &barrier, timeout_ticks) != pdPASS)
    {
        return false;
    }
    return ulTaskNotifyTake(pdTRUE, timeout_ticks) == 1U;
}

serial_status_t serial_manager_write(serial_tx_class_t tx_class,
                                     const uint8_t *data,
                                     size_t length,
                                     uint32_t timeout_ms)
{
    serial_tx_message_t message = {0};
    QueueHandle_t target_queue;
    TickType_t wait_ticks = 0U;

    if ((data == NULL) || (length == 0U) ||
        (length > SERIAL_MANAGER_MAX_TX_SIZE) ||
        ((tx_class != SERIAL_TX_LOG) &&
         (tx_class != SERIAL_TX_PROTOCOL)))
    {
        return SERIAL_STATUS_INVALID;
    }
    if (!initialized)
    {
        return SERIAL_STATUS_NOT_READY;
    }
    if ((tx_class == SERIAL_TX_LOG) &&
        (serial_manager_get_mode() != SERIAL_MODE_CONSOLE))
    {
        serial_record_tx_drop();
        return SERIAL_STATUS_WRONG_MODE;
    }

    message.length = (uint16_t)length;
    memcpy(message.data, data, length);
    target_queue = (tx_class == SERIAL_TX_PROTOCOL)
                       ? urgent_tx_queue
                       : normal_tx_queue;
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        wait_ticks = pdMS_TO_TICKS(timeout_ms);
    }
    if (xQueueSend(target_queue, &message, wait_ticks) != pdPASS)
    {
        serial_record_tx_drop();
        return SERIAL_STATUS_QUEUE_FULL;
    }

    return SERIAL_STATUS_OK;
}

serial_status_t serial_manager_read(serial_mode_t expected_mode,
                                    uint8_t *data,
                                    size_t capacity,
                                    uint32_t timeout_ms,
                                    size_t *received_length)
{
    size_t length;
    TickType_t start_ticks;
    TickType_t timeout_ticks;

    if ((data == NULL) || (capacity == 0U) ||
        (received_length == NULL) ||
        !serial_mode_is_valid(expected_mode))
    {
        return SERIAL_STATUS_INVALID;
    }
    *received_length = 0U;
    if (!initialized)
    {
        return SERIAL_STATUS_NOT_READY;
    }
    if (serial_manager_get_mode() != expected_mode)
    {
        return SERIAL_STATUS_WRONG_MODE;
    }

    start_ticks = xTaskGetTickCount();
    timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    do
    {
        if (serial_manager_get_mode() != expected_mode)
        {
            return SERIAL_STATUS_WRONG_MODE;
        }
        length = xStreamBufferReceive(rx_stream, data, capacity, 0U);
        if (length > 0U)
        {
            if (serial_manager_get_mode() != expected_mode)
            {
                return SERIAL_STATUS_WRONG_MODE;
            }
            *received_length = length;
            return SERIAL_STATUS_OK;
        }
        if ((timeout_ticks == 0U) ||
            ((TickType_t)(xTaskGetTickCount() - start_ticks) >= timeout_ticks))
        {
            return SERIAL_STATUS_TIMEOUT;
        }
        vTaskDelay(1U);
    } while (true);
}

serial_status_t serial_manager_set_mode(serial_mode_t mode)
{
    if (!serial_mode_is_valid(mode))
    {
        return SERIAL_STATUS_INVALID;
    }
    if (!initialized)
    {
        return SERIAL_STATUS_NOT_READY;
    }

    /* The session owner must stop its receiver before changing mode.  Resetting
       here prevents bytes from the preceding protocol from crossing the seam. */
    taskENTER_CRITICAL();
    (void)xStreamBufferReset(rx_stream);
    active_mode = mode;
    taskEXIT_CRITICAL();

    return SERIAL_STATUS_OK;
}

serial_mode_t serial_manager_get_mode(void)
{
    return active_mode;
}

void serial_manager_get_stats(serial_manager_stats_t *stats)
{
    if (stats == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    *stats = manager_stats;
    taskEXIT_CRITICAL();
    stats->rx_hardware_overruns = bsp_uart1_get_rx_overrun_count();
    stats->rx_hardware_errors = bsp_uart1_get_rx_error_count();
}

uint32_t serial_manager_get_stack_high_water_mark_words(void)
{
    if (serial_task == NULL)
    {
        return 0U;
    }

    return (uint32_t)uxTaskGetStackHighWaterMark(serial_task);
}

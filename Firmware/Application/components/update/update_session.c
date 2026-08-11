#include "update_session.h"

#include <string.h>

#include "app_log.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "serial_manager.h"
#include "stm32f4xx_hal.h"
#include "system_mode.h"
#include "task.h"
#include "ymodem_storage.h"

#define UPDATE_SESSION_TASK_STACK_WORDS 1024U
#define UPDATE_SESSION_TASK_PRIORITY    (tskIDLE_PRIORITY + 1U)
#define UPDATE_SESSION_QUEUE_DEPTH      1U
#define UPDATE_SESSION_IO_TIMEOUT_MS    1000U
#define UPDATE_SESSION_PROTOCOL_TIMEOUT_MS 5000U
#define UPDATE_SESSION_MAX_RETRIES      10U

typedef enum
{
    UPDATE_REQUEST_RECEIVE = 0,
    UPDATE_REQUEST_SEND
} update_request_type_t;

typedef struct
{
    update_request_type_t type;
    uint32_t file_size;
} update_request_t;

static StaticQueue_t request_queue_control;
static uint8_t request_queue_storage[UPDATE_SESSION_QUEUE_DEPTH *
                                     sizeof(update_request_t)];
static QueueHandle_t request_queue;

static StaticTask_t session_task_control;
static StackType_t session_task_stack[UPDATE_SESSION_TASK_STACK_WORDS];
static TaskHandle_t session_task_handle;

static struct storage_device *candidate_storage;
static update_session_snapshot_t session_snapshot;
static volatile bool cancel_requested;

static void update_session_set_snapshot(update_session_state_t state,
                                        ymodem_error_t error,
                                        uint32_t transferred,
                                        uint32_t file_size)
{
    taskENTER_CRITICAL();
    session_snapshot.state = state;
    session_snapshot.error = error;
    session_snapshot.transferred_bytes = transferred;
    session_snapshot.file_size = file_size;
    taskEXIT_CRITICAL();
}

static int update_session_transport_send(void *context,
                                         const uint8_t *data,
                                         size_t length)
{
    size_t offset = 0U;

    (void)context;
    while (offset < length)
    {
        size_t chunk = length - offset;

        if (chunk > SERIAL_MANAGER_MAX_TX_SIZE)
        {
            chunk = SERIAL_MANAGER_MAX_TX_SIZE;
        }
        if (serial_manager_write(SERIAL_TX_PROTOCOL,
                                 &data[offset],
                                 chunk,
                                 UPDATE_SESSION_IO_TIMEOUT_MS) !=
            SERIAL_STATUS_OK)
        {
            return -1;
        }
        offset += chunk;
    }
    return 0;
}

static bool update_session_take_cancel(void)
{
    bool cancel;

    taskENTER_CRITICAL();
    cancel = cancel_requested;
    cancel_requested = false;
    taskEXIT_CRITICAL();
    return cancel;
}

static bool update_session_enter_transfer(system_mode_t mode)
{
    if (!system_mode_enter(mode))
    {
        return false;
    }
    /* The Console task has already queued READY and its final prompt.  The
       normal-queue barrier runs only after urgent output has drained. */
    if (!serial_manager_flush(UPDATE_SESSION_IO_TIMEOUT_MS) ||
        (serial_manager_set_mode(SERIAL_MODE_YMODEM) != SERIAL_STATUS_OK))
    {
        system_mode_restore_normal();
        return false;
    }
    return true;
}

static void update_session_leave_transfer(void)
{
    (void)serial_manager_set_mode(SERIAL_MODE_CONSOLE);
    system_mode_restore_normal();
}

static void update_session_run_receive(struct storage_device *storage)
{
    struct ymodem_storage_sink storage_sink;
    ymodem_file_sink_t sink;
    const ymodem_transport_t transport = {
        .context = NULL,
        .send = update_session_transport_send,
    };
    ymodem_config_t config;
    ymodem_rx_t receiver;
    struct storage_info info;
    uint8_t input[64];
    size_t received_length;
    ymodem_status_t status;

    if ((storage_get_info(storage, &info) != STORAGE_OK) ||
        !ymodem_storage_sink_init(&storage_sink, storage))
    {
        update_session_set_snapshot(UPDATE_SESSION_ERROR,
                                    YMODEM_ERROR_FILE_REJECTED,
                                    0U,
                                    0U);
        return;
    }
    config.timeout_ms = UPDATE_SESSION_PROTOCOL_TIMEOUT_MS;
    config.maximum_file_size = info.capacity_bytes;
    config.maximum_retries = UPDATE_SESSION_MAX_RETRIES;
    sink = ymodem_storage_sink_interface(&storage_sink);
    if (!ymodem_rx_init(&receiver, &config, &transport, &sink) ||
        !update_session_enter_transfer(SYSTEM_MODE_FILE_RECEIVE))
    {
        update_session_set_snapshot(UPDATE_SESSION_ERROR,
                                    YMODEM_ERROR_IO,
                                    0U,
                                    0U);
        return;
    }

    update_session_set_snapshot(UPDATE_SESSION_RECEIVING,
                                YMODEM_ERROR_NONE,
                                0U,
                                0U);
    status = ymodem_rx_start(&receiver, HAL_GetTick());
    while (status == YMODEM_STATUS_ACTIVE)
    {
        if (update_session_take_cancel())
        {
            ymodem_rx_cancel(&receiver);
        }
        received_length = 0U;
        if (serial_manager_read(SERIAL_MODE_YMODEM,
                                input,
                                sizeof(input),
                                20U,
                                &received_length) == SERIAL_STATUS_OK)
        {
            status = ymodem_rx_feed(&receiver,
                                    input,
                                    received_length,
                                    HAL_GetTick());
        }
        else
        {
            status = ymodem_rx_poll(&receiver, HAL_GetTick());
            vTaskDelay(pdMS_TO_TICKS(1U));
        }
        update_session_set_snapshot(UPDATE_SESSION_RECEIVING,
                                    receiver.error,
                                    receiver.received_size,
                                    receiver.file.size);
    }

    update_session_leave_transfer();
    if (status == YMODEM_STATUS_COMPLETE)
    {
        taskENTER_CRITICAL();
        session_snapshot.completed_receive_size = receiver.file.size;
        taskEXIT_CRITICAL();
        update_session_set_snapshot(UPDATE_SESSION_COMPLETE,
                                    YMODEM_ERROR_NONE,
                                    receiver.received_size,
                                    receiver.file.size);
        (void)APP_TASK_LOG_INFO("update", "YMODEM RX complete: %lu bytes\r\n",
                                (unsigned long)receiver.received_size);
    }
    else if (status == YMODEM_STATUS_CANCELLED)
    {
        update_session_set_snapshot(UPDATE_SESSION_CANCELLED,
                                    receiver.error,
                                    receiver.received_size,
                                    receiver.file.size);
        (void)APP_TASK_LOG_WARNING("update", "YMODEM RX cancelled\r\n");
    }
    else
    {
        update_session_set_snapshot(UPDATE_SESSION_ERROR,
                                    receiver.error,
                                    receiver.received_size,
                                    receiver.file.size);
        (void)APP_TASK_LOG_ERROR("update", "YMODEM RX error=%u\r\n",
                                 (unsigned int)receiver.error);
    }
}

static void update_session_run_send(struct storage_device *storage,
                                    uint32_t file_size)
{
    struct ymodem_storage_source storage_source;
    ymodem_file_source_t source;
    const ymodem_transport_t transport = {
        .context = NULL,
        .send = update_session_transport_send,
    };
    const ymodem_config_t config = {
        .timeout_ms = UPDATE_SESSION_PROTOCOL_TIMEOUT_MS,
        .maximum_file_size = file_size,
        .maximum_retries = UPDATE_SESSION_MAX_RETRIES,
    };
    ymodem_tx_t sender;
    uint8_t input[32];
    size_t received_length;
    ymodem_status_t status;

    if (!ymodem_storage_source_init(&storage_source,
                                    storage,
                                    "candidate.bin",
                                    file_size))
    {
        update_session_set_snapshot(UPDATE_SESSION_ERROR,
                                    YMODEM_ERROR_FILE_REJECTED,
                                    0U,
                                    file_size);
        return;
    }
    source = ymodem_storage_source_interface(&storage_source);
    if (!ymodem_tx_init(&sender, &config, &transport, &source) ||
        !update_session_enter_transfer(SYSTEM_MODE_FILE_SEND))
    {
        update_session_set_snapshot(UPDATE_SESSION_ERROR,
                                    YMODEM_ERROR_IO,
                                    0U,
                                    file_size);
        return;
    }

    update_session_set_snapshot(UPDATE_SESSION_SENDING,
                                YMODEM_ERROR_NONE,
                                0U,
                                file_size);
    status = ymodem_tx_start(&sender, HAL_GetTick());
    while (status == YMODEM_STATUS_ACTIVE)
    {
        if (update_session_take_cancel())
        {
            ymodem_tx_cancel(&sender);
        }
        received_length = 0U;
        if (serial_manager_read(SERIAL_MODE_YMODEM,
                                input,
                                sizeof(input),
                                20U,
                                &received_length) == SERIAL_STATUS_OK)
        {
            status = ymodem_tx_feed(&sender,
                                    input,
                                    received_length,
                                    HAL_GetTick());
        }
        else
        {
            status = ymodem_tx_poll(&sender, HAL_GetTick());
            vTaskDelay(pdMS_TO_TICKS(1U));
        }
        update_session_set_snapshot(UPDATE_SESSION_SENDING,
                                    sender.error,
                                    sender.sent_size,
                                    sender.file.size);
    }

    update_session_leave_transfer();
    if (status == YMODEM_STATUS_COMPLETE)
    {
        update_session_set_snapshot(UPDATE_SESSION_COMPLETE,
                                    YMODEM_ERROR_NONE,
                                    sender.sent_size,
                                    sender.file.size);
        (void)APP_TASK_LOG_INFO("update", "YMODEM TX complete: %lu bytes\r\n",
                                (unsigned long)sender.sent_size);
    }
    else if (status == YMODEM_STATUS_CANCELLED)
    {
        update_session_set_snapshot(UPDATE_SESSION_CANCELLED,
                                    sender.error,
                                    sender.sent_size,
                                    sender.file.size);
        (void)APP_TASK_LOG_WARNING("update", "YMODEM TX cancelled\r\n");
    }
    else
    {
        update_session_set_snapshot(UPDATE_SESSION_ERROR,
                                    sender.error,
                                    sender.sent_size,
                                    sender.file.size);
        (void)APP_TASK_LOG_ERROR("update", "YMODEM TX error=%u\r\n",
                                 (unsigned int)sender.error);
    }
}

static void update_session_task(void *argument)
{
    update_request_t request;
    struct storage_device *storage;

    (void)argument;
    for (;;)
    {
        if (xQueueReceive(request_queue, &request, portMAX_DELAY) != pdPASS)
        {
            continue;
        }
        taskENTER_CRITICAL();
        storage = candidate_storage;
        cancel_requested = false;
        taskEXIT_CRITICAL();
        if (storage == NULL)
        {
            update_session_set_snapshot(UPDATE_SESSION_ERROR,
                                        YMODEM_ERROR_FILE_REJECTED,
                                        0U,
                                        request.file_size);
            continue;
        }

        if (request.type == UPDATE_REQUEST_RECEIVE)
        {
            update_session_run_receive(storage);
        }
        else
        {
            update_session_run_send(storage, request.file_size);
        }
    }
}

bool update_session_init(void)
{
    if ((request_queue != NULL) || (session_task_handle != NULL))
    {
        return false;
    }
    request_queue = xQueueCreateStatic(UPDATE_SESSION_QUEUE_DEPTH,
                                       sizeof(update_request_t),
                                       request_queue_storage,
                                       &request_queue_control);
    if (request_queue == NULL)
    {
        return false;
    }
    session_task_handle = xTaskCreateStatic(update_session_task,
                                            "update_session",
                                            UPDATE_SESSION_TASK_STACK_WORDS,
                                            NULL,
                                            UPDATE_SESSION_TASK_PRIORITY,
                                            session_task_stack,
                                            &session_task_control);
    return session_task_handle != NULL;
}

bool update_session_bind_candidate(struct storage_device *candidate)
{
    struct storage_info info;

    if ((candidate == NULL) ||
        (storage_get_info(candidate, &info) != STORAGE_OK) ||
        ((info.capabilities & (STORAGE_CAP_READ | STORAGE_CAP_WRITE)) !=
         (STORAGE_CAP_READ | STORAGE_CAP_WRITE)))
    {
        return false;
    }
    taskENTER_CRITICAL();
    candidate_storage = candidate;
    taskEXIT_CRITICAL();
    return true;
}

static bool update_session_request(update_request_type_t type,
                                   uint32_t file_size)
{
    const update_request_t request = {
        .type = type,
        .file_size = file_size,
    };
    bool ready;

    taskENTER_CRITICAL();
    ready = (candidate_storage != NULL) &&
            ((session_snapshot.state == UPDATE_SESSION_IDLE) ||
             (session_snapshot.state == UPDATE_SESSION_COMPLETE) ||
             (session_snapshot.state == UPDATE_SESSION_CANCELLED) ||
             (session_snapshot.state == UPDATE_SESSION_ERROR));
    taskEXIT_CRITICAL();
    if (!ready || (request_queue == NULL) ||
        (xQueueSend(request_queue, &request, 0U) != pdPASS))
    {
        return false;
    }
    update_session_set_snapshot((type == UPDATE_REQUEST_RECEIVE)
                                    ? UPDATE_SESSION_RECEIVING
                                    : UPDATE_SESSION_SENDING,
                                YMODEM_ERROR_NONE,
                                0U,
                                file_size);
    return true;
}

bool update_session_request_receive(void)
{
    return update_session_request(UPDATE_REQUEST_RECEIVE, 0U);
}

bool update_session_request_send(uint32_t file_size)
{
    uint32_t completed_size;
    struct storage_device *storage;
    struct storage_info info;

    taskENTER_CRITICAL();
    completed_size = session_snapshot.completed_receive_size;
    storage = candidate_storage;
    taskEXIT_CRITICAL();
    if (file_size == 0U)
    {
        file_size = completed_size;
    }
    return (storage != NULL) &&
           (storage_get_info(storage, &info) == STORAGE_OK) &&
           (file_size > 0U) && (file_size <= info.capacity_bytes) &&
           update_session_request(UPDATE_REQUEST_SEND, file_size);
}

bool update_session_cancel(void)
{
    bool active;

    taskENTER_CRITICAL();
    active = (session_snapshot.state == UPDATE_SESSION_RECEIVING) ||
             (session_snapshot.state == UPDATE_SESSION_SENDING);
    if (active)
    {
        cancel_requested = true;
    }
    taskEXIT_CRITICAL();
    return active;
}

void update_session_get_snapshot(update_session_snapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
    taskENTER_CRITICAL();
    *snapshot = session_snapshot;
    taskEXIT_CRITICAL();
}

const char *update_session_state_name(update_session_state_t state)
{
    static const char *const names[] = {
        "IDLE", "RECEIVING", "SENDING", "COMPLETE", "CANCELLED", "ERROR",
    };

    return (state <= UPDATE_SESSION_ERROR) ? names[state] : "UNKNOWN";
}

uint32_t update_session_get_stack_high_water_mark_words(void)
{
    return (session_task_handle != NULL)
               ? (uint32_t)uxTaskGetStackHighWaterMark(session_task_handle)
               : 0U;
}

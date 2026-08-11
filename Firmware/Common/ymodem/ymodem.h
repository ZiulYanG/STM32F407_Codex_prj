#ifndef YMODEM_H
#define YMODEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define YMODEM_DATA_SIZE_128  128U
#define YMODEM_DATA_SIZE_1K   1024U
#define YMODEM_MAX_FILE_NAME  64U
#define YMODEM_MAX_FRAME_SIZE (3U + YMODEM_DATA_SIZE_1K + 2U)

typedef enum
{
    YMODEM_STATUS_IDLE = 0,
    YMODEM_STATUS_ACTIVE,
    YMODEM_STATUS_COMPLETE,
    YMODEM_STATUS_CANCELLED,
    YMODEM_STATUS_ERROR
} ymodem_status_t;

typedef enum
{
    YMODEM_ERROR_NONE = 0,
    YMODEM_ERROR_INVALID_ARGUMENT,
    YMODEM_ERROR_IO,
    YMODEM_ERROR_PROTOCOL,
    YMODEM_ERROR_RETRY_LIMIT,
    YMODEM_ERROR_FILE_REJECTED,
    YMODEM_ERROR_FILE_READ,
    YMODEM_ERROR_FILE_WRITE,
    YMODEM_ERROR_FILE_SIZE
} ymodem_error_t;

typedef struct
{
    char name[YMODEM_MAX_FILE_NAME];
    uint32_t size;
} ymodem_file_info_t;

typedef struct
{
    void *context;
    int (*send)(void *context, const uint8_t *data, size_t length);
} ymodem_transport_t;

typedef struct
{
    void *context;
    int (*begin)(void *context, const ymodem_file_info_t *file);
    int (*write)(void *context,
                 uint32_t offset,
                 const uint8_t *data,
                 size_t length);
    int (*finish)(void *context);
    void (*abort)(void *context);
} ymodem_file_sink_t;

typedef struct
{
    void *context;
    int (*open)(void *context, ymodem_file_info_t *file);
    int (*read)(void *context,
                uint32_t offset,
                uint8_t *data,
                size_t capacity,
                size_t *read_length);
    void (*close)(void *context);
} ymodem_file_source_t;

typedef struct
{
    uint32_t timeout_ms;
    uint32_t maximum_file_size;
    uint8_t maximum_retries;
} ymodem_config_t;

typedef struct
{
    ymodem_config_t config;
    ymodem_transport_t transport;
    ymodem_file_sink_t sink;
    ymodem_file_info_t file;
    ymodem_status_t status;
    ymodem_error_t error;
    uint32_t received_size;
    uint32_t last_activity_ms;
    uint16_t frame_expected;
    uint16_t frame_received;
    uint16_t packet_size;
    uint8_t expected_block;
    uint8_t retry_count;
    uint8_t cancel_count;
    uint8_t state;
    bool frame_active;
    bool sink_open;
    uint8_t frame[YMODEM_DATA_SIZE_1K + 4U];
} ymodem_rx_t;

typedef struct
{
    ymodem_config_t config;
    ymodem_transport_t transport;
    ymodem_file_source_t source;
    ymodem_file_info_t file;
    ymodem_status_t status;
    ymodem_error_t error;
    uint32_t sent_size;
    uint32_t pending_size;
    uint32_t last_activity_ms;
    uint16_t last_frame_length;
    uint8_t block_number;
    uint8_t retry_count;
    uint8_t cancel_count;
    uint8_t state;
    bool source_open;
    uint8_t last_frame[YMODEM_MAX_FRAME_SIZE];
} ymodem_tx_t;

uint16_t ymodem_crc16(const uint8_t *data, size_t length);

bool ymodem_rx_init(ymodem_rx_t *receiver,
                    const ymodem_config_t *config,
                    const ymodem_transport_t *transport,
                    const ymodem_file_sink_t *sink);
ymodem_status_t ymodem_rx_start(ymodem_rx_t *receiver, uint32_t now_ms);
ymodem_status_t ymodem_rx_feed(ymodem_rx_t *receiver,
                               const uint8_t *data,
                               size_t length,
                               uint32_t now_ms);
ymodem_status_t ymodem_rx_poll(ymodem_rx_t *receiver, uint32_t now_ms);
void ymodem_rx_cancel(ymodem_rx_t *receiver);

bool ymodem_tx_init(ymodem_tx_t *sender,
                    const ymodem_config_t *config,
                    const ymodem_transport_t *transport,
                    const ymodem_file_source_t *source);
ymodem_status_t ymodem_tx_start(ymodem_tx_t *sender, uint32_t now_ms);
ymodem_status_t ymodem_tx_feed(ymodem_tx_t *sender,
                               const uint8_t *data,
                               size_t length,
                               uint32_t now_ms);
ymodem_status_t ymodem_tx_poll(ymodem_tx_t *sender, uint32_t now_ms);
void ymodem_tx_cancel(ymodem_tx_t *sender);

#endif /* YMODEM_H */

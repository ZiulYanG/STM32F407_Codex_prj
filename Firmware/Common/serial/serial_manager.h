#ifndef SERIAL_MANAGER_H
#define SERIAL_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SERIAL_MANAGER_MAX_TX_SIZE 160U

typedef enum
{
    SERIAL_MODE_CONSOLE = 0,
    SERIAL_MODE_YMODEM,
    SERIAL_MODE_MODBUS
} serial_mode_t;

typedef enum
{
    SERIAL_TX_LOG = 0,
    SERIAL_TX_PROTOCOL
} serial_tx_class_t;

typedef enum
{
    SERIAL_STATUS_OK = 0,
    SERIAL_STATUS_INVALID = -1,
    SERIAL_STATUS_NOT_READY = -2,
    SERIAL_STATUS_WRONG_MODE = -3,
    SERIAL_STATUS_QUEUE_FULL = -4,
    SERIAL_STATUS_TIMEOUT = -5
} serial_status_t;

typedef struct
{
    uint32_t tx_messages;
    uint32_t tx_bytes;
    uint32_t tx_dropped;
    uint32_t tx_errors;
    uint32_t rx_bytes;
    uint32_t rx_dropped;
    uint32_t rx_hardware_overruns;
    uint32_t rx_hardware_errors;
} serial_manager_stats_t;

bool serial_manager_init(void);
bool serial_manager_flush(uint32_t timeout_ms);
serial_status_t serial_manager_write(serial_tx_class_t tx_class,
                                     const uint8_t *data,
                                     size_t length,
                                     uint32_t timeout_ms);
serial_status_t serial_manager_read(serial_mode_t expected_mode,
                                    uint8_t *data,
                                    size_t capacity,
                                    uint32_t timeout_ms,
                                    size_t *received_length);
serial_status_t serial_manager_set_mode(serial_mode_t mode);
serial_mode_t serial_manager_get_mode(void);
void serial_manager_get_stats(serial_manager_stats_t *stats);
uint32_t serial_manager_get_stack_high_water_mark_words(void);

#endif /* SERIAL_MANAGER_H */

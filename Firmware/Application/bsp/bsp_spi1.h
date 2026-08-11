#ifndef BSP_SPI1_H
#define BSP_SPI1_H

#include <stddef.h>
#include <stdint.h>

/**
 * Perform one blocking SPI1 transfer.
 *
 * tx_data or rx_data may be NULL for transmit-only or receive-only transfers.
 * Returns 0 on success and -1 on invalid arguments or HAL failure.
 */
int bsp_spi1_transfer(const uint8_t *tx_data,
                      uint8_t *rx_data,
                      size_t length,
                      uint32_t timeout_ms);

#endif /* BSP_SPI1_H */

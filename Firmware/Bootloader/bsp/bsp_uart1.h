#ifndef BSP_UART1_H
#define BSP_UART1_H

#include <stddef.h>
#include <stdint.h>

/** Write one complete byte span to the Bootloader console UART. */
int bsp_uart1_write(const uint8_t *data, size_t length, uint32_t timeout_ms);
int bsp_uart1_start_receive(void);
size_t bsp_uart1_read(uint8_t *data, size_t capacity);
uint32_t bsp_uart1_get_rx_overrun_count(void);
uint32_t bsp_uart1_get_rx_error_count(void);

#endif /* BSP_UART1_H */

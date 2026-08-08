#ifndef BSP_UART1_H
#define BSP_UART1_H

#include <stddef.h>
#include <stdint.h>

/** Write one complete byte span to the Bootloader console UART. */
int bsp_uart1_write(const uint8_t *data, size_t length, uint32_t timeout_ms);

#endif /* BSP_UART1_H */

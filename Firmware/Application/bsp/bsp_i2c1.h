#ifndef BSP_I2C1_H
#define BSP_I2C1_H

#include <stddef.h>
#include <stdint.h>

/** Probe one 7-bit I2C address with a blocking transaction. */
int bsp_i2c1_is_device_ready(uint8_t address_7bit,
                             uint32_t trials,
                             uint32_t timeout_ms);

/** Read from a device that uses an 8-bit internal memory address. */
int bsp_i2c1_mem_read(uint8_t address_7bit,
                      uint8_t memory_address,
                      uint8_t *data,
                      size_t length,
                      uint32_t timeout_ms);

/** Write to a device that uses an 8-bit internal memory address. */
int bsp_i2c1_mem_write(uint8_t address_7bit,
                       uint8_t memory_address,
                       const uint8_t *data,
                       size_t length,
                       uint32_t timeout_ms);

#endif /* BSP_I2C1_H */

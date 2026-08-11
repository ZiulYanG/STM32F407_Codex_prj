#include "bsp_i2c1.h"

#include "main.h"

extern I2C_HandleTypeDef hi2c1;

static uint16_t bsp_i2c1_hal_address(uint8_t address_7bit)
{
    return (uint16_t)((uint16_t)address_7bit << 1U);
}

int bsp_i2c1_is_device_ready(uint8_t address_7bit,
                             uint32_t trials,
                             uint32_t timeout_ms)
{
    if ((address_7bit > 0x7FU) || (trials == 0U))
    {
        return -1;
    }

    return (HAL_I2C_IsDeviceReady(&hi2c1,
                                  bsp_i2c1_hal_address(address_7bit),
                                  trials,
                                  timeout_ms) == HAL_OK)
               ? 0
               : -1;
}

int bsp_i2c1_mem_read(uint8_t address_7bit,
                      uint8_t memory_address,
                      uint8_t *data,
                      size_t length,
                      uint32_t timeout_ms)
{
    if ((address_7bit > 0x7FU) || (data == NULL) || (length == 0U) ||
        (length > UINT16_MAX))
    {
        return -1;
    }

    return (HAL_I2C_Mem_Read(&hi2c1,
                             bsp_i2c1_hal_address(address_7bit),
                             memory_address,
                             I2C_MEMADD_SIZE_8BIT,
                             data,
                             (uint16_t)length,
                             timeout_ms) == HAL_OK)
               ? 0
               : -1;
}

int bsp_i2c1_mem_write(uint8_t address_7bit,
                       uint8_t memory_address,
                       const uint8_t *data,
                       size_t length,
                       uint32_t timeout_ms)
{
    if ((address_7bit > 0x7FU) || (data == NULL) || (length == 0U) ||
        (length > UINT16_MAX))
    {
        return -1;
    }

    return (HAL_I2C_Mem_Write(&hi2c1,
                              bsp_i2c1_hal_address(address_7bit),
                              memory_address,
                              I2C_MEMADD_SIZE_8BIT,
                              (uint8_t *)data,
                              (uint16_t)length,
                              timeout_ms) == HAL_OK)
               ? 0
               : -1;
}

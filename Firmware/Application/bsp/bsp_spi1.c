#include "bsp_spi1.h"

#include "main.h"

extern SPI_HandleTypeDef hspi1;

int bsp_spi1_transfer(const uint8_t *tx_data,
                      uint8_t *rx_data,
                      size_t length,
                      uint32_t timeout_ms)
{
    HAL_StatusTypeDef status;

    if ((length == 0U) || (length > UINT16_MAX) ||
        ((tx_data == NULL) && (rx_data == NULL)))
    {
        return -1;
    }

    if ((tx_data != NULL) && (rx_data != NULL))
    {
        status = HAL_SPI_TransmitReceive(&hspi1,
                                         (uint8_t *)tx_data,
                                         rx_data,
                                         (uint16_t)length,
                                         timeout_ms);
    }
    else if (tx_data != NULL)
    {
        status = HAL_SPI_Transmit(&hspi1,
                                  (uint8_t *)tx_data,
                                  (uint16_t)length,
                                  timeout_ms);
    }
    else
    {
        status = HAL_SPI_Receive(&hspi1,
                                 rx_data,
                                 (uint16_t)length,
                                 timeout_ms);
    }

    return (status == HAL_OK) ? 0 : -1;
}

#include "bsp_uart1.h"

#include "usart.h"

int bsp_uart1_write(const uint8_t *data, size_t length, uint32_t timeout_ms)
{
    if ((data == NULL) || (length == 0U) || (length > UINT16_MAX))
    {
        return -1;
    }

    return (HAL_UART_Transmit(&huart1,
                              (uint8_t *)data,
                              (uint16_t)length,
                              timeout_ms) == HAL_OK)
               ? 0
               : -1;
}

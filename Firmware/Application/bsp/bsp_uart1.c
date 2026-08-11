#include "bsp_uart1.h"

#include "main.h"

extern UART_HandleTypeDef huart1;

#define BSP_UART1_RX_RING_SIZE 2048U
#define BSP_UART1_RX_RING_MASK (BSP_UART1_RX_RING_SIZE - 1U)

static uint8_t rx_ring[BSP_UART1_RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static uint8_t rx_byte;
static volatile uint32_t rx_overrun_count;
static volatile uint32_t rx_error_count;

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

int bsp_uart1_start_receive(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    rx_overrun_count = 0U;
    rx_error_count = 0U;

    return (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) == HAL_OK) ? 0 : -1;
}

size_t bsp_uart1_read(uint8_t *data, size_t capacity)
{
    size_t length = 0U;

    if ((data == NULL) || (capacity == 0U))
    {
        return 0U;
    }

    while ((length < capacity) && (rx_tail != rx_head))
    {
        data[length] = rx_ring[rx_tail];
        ++length;
        rx_tail = (uint16_t)((rx_tail + 1U) & BSP_UART1_RX_RING_MASK);
    }

    return length;
}

uint32_t bsp_uart1_get_rx_overrun_count(void)
{
    return rx_overrun_count;
}

uint32_t bsp_uart1_get_rx_error_count(void)
{
    return rx_error_count;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint16_t next_head;

    if (huart->Instance != USART1)
    {
        return;
    }

    next_head = (uint16_t)((rx_head + 1U) & BSP_UART1_RX_RING_MASK);
    if (next_head == rx_tail)
    {
        ++rx_overrun_count;
    }
    else
    {
        rx_ring[rx_head] = rx_byte;
        rx_head = next_head;
    }

    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK)
    {
        ++rx_error_count;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    ++rx_error_count;
    /* ORE ends the HAL receive transaction, while noise/frame/parity errors
       may leave it active.  Rearm only after a blocking error ended RX. */
    if ((huart->RxState == HAL_UART_STATE_READY) &&
        (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK))
    {
        ++rx_error_count;
    }
}

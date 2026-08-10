#include "bsp_board.h"

void bsp_board_init(void)
{
    BSP_LED1_OFF();
    BSP_LED2_OFF();
}

const char *bsp_board_get_reset_reason(void)
{
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET)
    {
        return "Power-on reset";
    }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET)
    {
        return "Brown-out reset";
    }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
    {
        return "Software reset";
    }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
    {
        return "Independent watchdog reset";
    }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET)
    {
        return "Window watchdog reset";
    }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET)
    {
        return "Low-power reset";
    }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
    {
        return "Pin reset";
    }
    return "Unknown reset";
}

void bsp_board_clear_reset_flags(void)
{
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

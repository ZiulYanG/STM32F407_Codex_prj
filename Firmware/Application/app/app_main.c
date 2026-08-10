#include "app_main.h"

#include "app_log.h"
#include "app_version.h"
#include "bsp_board.h"
#include "FreeRTOS.h"
#include "stm32f4xx_hal.h"
#include "task.h"

#define APPLICATION_FLASH_BASE 0x08040000UL

void app_main_init(void)
{
    const char *reset_reason;

    bsp_board_init();
    reset_reason = bsp_board_get_reset_reason();

    (void)app_log_printf("\r\n================================\r\n");
    (void)app_log_printf("STM32F407 Application\r\n");
    (void)app_log_printf("Version       : %s\r\n", APPLICATION_VERSION);
    (void)app_log_printf("Build         : %s %s\r\n", __DATE__, __TIME__);
    (void)app_log_printf("Reset reason  : %s\r\n", reset_reason);
    (void)app_log_printf("SYSCLK        : %lu Hz\r\n", (unsigned long)HAL_RCC_GetSysClockFreq());
    (void)app_log_printf("HCLK          : %lu Hz\r\n", (unsigned long)HAL_RCC_GetHCLKFreq());
    (void)app_log_printf("PCLK1         : %lu Hz\r\n", (unsigned long)HAL_RCC_GetPCLK1Freq());
    (void)app_log_printf("PCLK2         : %lu Hz\r\n", (unsigned long)HAL_RCC_GetPCLK2Freq());
    (void)app_log_printf("SystemCoreClk : %lu Hz\r\n", (unsigned long)SystemCoreClock);
    (void)app_log_printf("Vector table  : 0x%08lX\r\n", (unsigned long)SCB->VTOR);
    (void)app_log_printf("HAL timebase  : TIM7\r\n");
    (void)app_log_printf("RTOS tick     : %lu Hz\r\n", (unsigned long)configTICK_RATE_HZ);
    (void)app_log_printf("USART1        : 115200 8N1\r\n");
    (void)app_log_printf("APP address   : 0x%08lX\r\n", (unsigned long)APPLICATION_FLASH_BASE);
    (void)app_log_printf("APP state     : DEVELOPMENT\r\n");
    (void)app_log_printf("================================\r\n");

    bsp_board_clear_reset_flags();
}

void app_main_task(void *argument)
{
    (void)argument;

    for (;;)
    {
        BSP_LED1_ON();
        BSP_LED2_ON();
        vTaskDelay(pdMS_TO_TICKS(100U));

        BSP_LED1_OFF();
        BSP_LED2_OFF();
        vTaskDelay(pdMS_TO_TICKS(900U));
    }
}

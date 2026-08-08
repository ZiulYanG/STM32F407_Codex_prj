#include "boot_app.h"

#include "bsp_board.h"
#include "boot_log.h"
#include "boot_version.h"
#include "FreeRTOS.h"
#include "stm32f4xx_hal.h"

#define APP_FLASH_BASE_ADDRESS 0x08040000UL

void boot_app_init(void)
{
    const char *reset_reason;

    bsp_board_init();
    reset_reason = bsp_board_get_reset_reason();

    (void)boot_log_printf("\r\n================================\r\n");
    (void)boot_log_printf("STM32F407 Bootloader\r\n");
    (void)boot_log_printf("Version       : %s\r\n", BOOTLOADER_VERSION);
    (void)boot_log_printf("Build         : %s %s\r\n", __DATE__, __TIME__);
    (void)boot_log_printf("Reset reason  : %s\r\n", reset_reason);
    (void)boot_log_printf("SYSCLK        : %lu Hz\r\n", (unsigned long)HAL_RCC_GetSysClockFreq());
    (void)boot_log_printf("HCLK          : %lu Hz\r\n", (unsigned long)HAL_RCC_GetHCLKFreq());
    (void)boot_log_printf("PCLK1         : %lu Hz\r\n", (unsigned long)HAL_RCC_GetPCLK1Freq());
    (void)boot_log_printf("PCLK2         : %lu Hz\r\n", (unsigned long)HAL_RCC_GetPCLK2Freq());
    (void)boot_log_printf("SystemCoreClk : %lu Hz\r\n", (unsigned long)SystemCoreClock);
    (void)boot_log_printf("HAL timebase  : TIM7\r\n");
    (void)boot_log_printf("RTOS tick     : %lu Hz\r\n", (unsigned long)configTICK_RATE_HZ);
    (void)boot_log_printf("USART1        : 115200 8N1\r\n");
    (void)boot_log_printf("APP address   : 0x%08lX\r\n", (unsigned long)APP_FLASH_BASE_ADDRESS);
    (void)boot_log_printf("Boot state    : DEVELOPMENT\r\n");
    (void)boot_log_printf("================================\r\n");

    bsp_board_clear_reset_flags();
}

void boot_app_process(void)
{
    /*
     * Future work: evaluate boot metadata, verify the candidate image and
     * either install it or transfer control to the application firmware.
     */
}

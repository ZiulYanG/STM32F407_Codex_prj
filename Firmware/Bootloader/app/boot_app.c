#include "boot_app.h"

#include "app_launcher.h"
#include "bsp_board.h"
#include "boot_log.h"
#include "boot_version.h"
#include "FreeRTOS.h"
#include "stm32f4xx_hal.h"

#define APP_FLASH_BASE_ADDRESS 0x08040000UL
#define APP_FLASH_END_ADDRESS  0x08100000UL
#define BOOT_LOG_FLUSH_TIMEOUT_MS 1000U
#define BOOT_COMMAND_WINDOW_DEFAULT_MS 3000U
#define BOOT_COMMAND_WINDOW_MAX_MS     30000U

static volatile uint32_t command_window_ms =
    BOOT_COMMAND_WINDOW_DEFAULT_MS;
static volatile bool stay_requested;
static volatile bool jump_requested;
static uint32_t command_window_start_ms;
static bool decision_complete;
static bool stay_reported;

void boot_app_init(void)
{
    const char *reset_reason;

    bsp_board_init();
    reset_reason = bsp_board_get_reset_reason();
    command_window_start_ms = HAL_GetTick();

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
    (void)boot_log_printf("Command window: %lu ms\r\n",
                          (unsigned long)command_window_ms);
    (void)boot_log_printf("================================\r\n");

    bsp_board_clear_reset_flags();
}

void boot_app_process(void)
{
    app_launch_target_t target;
    app_launch_status_t status;

    if (stay_requested && !jump_requested)
    {
        if (!stay_reported)
        {
            (void)boot_log_printf("Boot action   : STAY REQUESTED\r\n");
            stay_reported = true;
        }
        return;
    }
    if (decision_complete)
    {
        return;
    }
    if (!jump_requested &&
        ((uint32_t)(HAL_GetTick() - command_window_start_ms) <
         command_window_ms))
    {
        return;
    }
    decision_complete = true;
    jump_requested = false;

    status = app_launcher_inspect(APP_FLASH_BASE_ADDRESS,
                                  APP_FLASH_END_ADDRESS,
                                  &target);
    if (status != APP_LAUNCH_VALID)
    {
        (void)boot_log_printf("APP check     : INVALID (%s)\r\n",
                              app_launcher_status_string(status));
        (void)boot_log_printf("Boot action   : STAY IN BOOTLOADER\r\n");
        (void)boot_log_flush(BOOT_LOG_FLUSH_TIMEOUT_MS);
        return;
    }

    (void)boot_log_printf("APP check     : VALID\r\n");
    (void)boot_log_printf("APP MSP       : 0x%08lX\r\n",
                          (unsigned long)target.initial_msp);
    (void)boot_log_printf("APP reset     : 0x%08lX\r\n",
                          (unsigned long)target.reset_handler);
    (void)boot_log_printf("Boot action   : JUMP TO APPLICATION\r\n");

    if (!boot_log_flush(BOOT_LOG_FLUSH_TIMEOUT_MS))
    {
        Error_Handler();
    }

    app_launcher_jump(&target);
}

bool boot_app_set_window_ms(uint32_t window_ms)
{
    if (window_ms > BOOT_COMMAND_WINDOW_MAX_MS)
    {
        return false;
    }
    command_window_ms = window_ms;
    return true;
}

uint32_t boot_app_get_window_ms(void)
{
    return command_window_ms;
}

void boot_app_request_stay(void)
{
    stay_requested = true;
    jump_requested = false;
}

void boot_app_request_jump(void)
{
    stay_requested = false;
    stay_reported = false;
    jump_requested = true;
    decision_complete = false;
}

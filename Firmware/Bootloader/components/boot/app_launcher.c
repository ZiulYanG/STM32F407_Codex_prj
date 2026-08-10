#include "app_launcher.h"

#include <stddef.h>

#include <stdbool.h>

#include "main.h"

#define VECTOR_TABLE_ALIGNMENT 0x200UL
#define MAIN_SRAM_START        0x20000000UL
#define MAIN_SRAM_END          0x20020000UL
#define CCM_SRAM_START         0x10000000UL
#define CCM_SRAM_END           0x10010000UL
#define STACK_ALIGNMENT        8UL
#define NVIC_REGISTER_COUNT    8U

static bool app_launcher_stack_is_valid(uint32_t stack_pointer)
{
    const bool aligned = (stack_pointer & (STACK_ALIGNMENT - 1UL)) == 0UL;
    const bool in_main_sram = (stack_pointer >= MAIN_SRAM_START) &&
                              (stack_pointer <= MAIN_SRAM_END);
    const bool in_ccm_sram = (stack_pointer >= CCM_SRAM_START) &&
                             (stack_pointer <= CCM_SRAM_END);

    return aligned && (in_main_sram || in_ccm_sram);
}

app_launch_status_t app_launcher_inspect(uint32_t vector_base,
                                         uint32_t flash_end,
                                         app_launch_target_t *target)
{
    uint32_t reset_address;

    if ((target == NULL) || (vector_base >= flash_end))
    {
        return APP_LAUNCH_INVALID_ARGUMENT;
    }
    if ((vector_base & (VECTOR_TABLE_ALIGNMENT - 1UL)) != 0UL)
    {
        return APP_LAUNCH_INVALID_ALIGNMENT;
    }

    target->vector_base = vector_base;
    target->initial_msp = *(const volatile uint32_t *)vector_base;
    target->reset_handler = *(const volatile uint32_t *)(vector_base + sizeof(uint32_t));

    if (!app_launcher_stack_is_valid(target->initial_msp))
    {
        return APP_LAUNCH_INVALID_STACK;
    }

    reset_address = target->reset_handler & ~1UL;
    if (((target->reset_handler & 1UL) == 0UL) ||
        (reset_address < vector_base) ||
        (reset_address >= flash_end))
    {
        return APP_LAUNCH_INVALID_RESET_HANDLER;
    }

    return APP_LAUNCH_VALID;
}

const char *app_launcher_status_string(app_launch_status_t status)
{
    switch (status)
    {
        case APP_LAUNCH_VALID:
            return "valid";
        case APP_LAUNCH_INVALID_ARGUMENT:
            return "invalid argument";
        case APP_LAUNCH_INVALID_ALIGNMENT:
            return "vector alignment";
        case APP_LAUNCH_INVALID_STACK:
            return "initial MSP";
        case APP_LAUNCH_INVALID_RESET_HANDLER:
            return "reset handler";
        default:
            return "unknown";
    }
}

__attribute__((naked, noreturn)) static void app_launcher_branch(
    uint32_t initial_msp __attribute__((unused)),
    uint32_t reset_handler __attribute__((unused)))
{
    __asm volatile(
        "cpsid i                \n"
        "movs r2, #0            \n"
        "msr basepri, r2        \n"
        "msr faultmask, r2      \n"
        "msr control, r2        \n"
        "isb                    \n"
        "msr msp, r0            \n"
        "dsb                    \n"
        "isb                    \n"
        /* Keep PRIMASK set across the hand-off.  The Application startup and
         * FreeRTOS port re-enable interrupts only after rebuilding their own
         * MSP/context, which avoids an exception in the hand-off window. */
        "bx r1                  \n");
}

void app_launcher_jump(const app_launch_target_t *target)
{
    app_launch_target_t validated_target;
    uint32_t index;
    uint32_t initial_msp;
    uint32_t reset_handler;

    if ((target == NULL) ||
        (app_launcher_inspect(target->vector_base,
                              0x08100000UL,
                              &validated_target) != APP_LAUNCH_VALID))
    {
        Error_Handler();
    }

    initial_msp = validated_target.initial_msp;
    reset_handler = validated_target.reset_handler;

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    __HAL_RCC_TIM7_FORCE_RESET();
    __HAL_RCC_TIM7_RELEASE_RESET();
    __HAL_RCC_USART1_FORCE_RESET();
    __HAL_RCC_USART1_RELEASE_RESET();

    for (index = 0U; index < NVIC_REGISTER_COUNT; ++index)
    {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }

    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    SCB->VTOR = validated_target.vector_base;
    __DSB();
    __ISB();

    app_launcher_branch(initial_msp, reset_handler);
}

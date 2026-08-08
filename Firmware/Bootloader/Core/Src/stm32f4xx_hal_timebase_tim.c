/* TIM7 is the STM32 HAL 1 ms timebase; SysTick remains owned by FreeRTOS. */
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"

static TIM_HandleTypeDef hal_tick_timer;

HAL_StatusTypeDef HAL_InitTick(uint32_t tick_priority)
{
    RCC_ClkInitTypeDef clock_config;
    uint32_t flash_latency;
    uint32_t pclk1_frequency;
    uint32_t timer_clock;
    uint32_t prescaler;

    HAL_RCC_GetClockConfig(&clock_config, &flash_latency);
    pclk1_frequency = HAL_RCC_GetPCLK1Freq();
    timer_clock = (clock_config.APB1CLKDivider == RCC_HCLK_DIV1)
                      ? pclk1_frequency
                      : (pclk1_frequency * 2U);

    prescaler = (timer_clock / 1000000U) - 1U;

    __HAL_RCC_TIM7_CLK_ENABLE();

    hal_tick_timer.Instance = TIM7;
    hal_tick_timer.Init.Period = 1000U - 1U;
    hal_tick_timer.Init.Prescaler = prescaler;
    hal_tick_timer.Init.ClockDivision = 0U;
    hal_tick_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    hal_tick_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&hal_tick_timer) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (tick_priority >= (1UL << __NVIC_PRIO_BITS))
    {
        return HAL_ERROR;
    }

    HAL_NVIC_SetPriority(TIM7_IRQn, tick_priority, 0U);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
    uwTickPrio = tick_priority;

    return HAL_TIM_Base_Start_IT(&hal_tick_timer);
}

void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&hal_tick_timer, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&hal_tick_timer, TIM_IT_UPDATE);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *timer)
{
    if (timer->Instance == TIM7)
    {
        HAL_IncTick();
    }
}

void TIM7_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&hal_tick_timer);
}

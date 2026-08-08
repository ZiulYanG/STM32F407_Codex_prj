/**
  ******************************************************************************
  * @file    led_task.c
  * @brief   LED FreeRTOS task - alternately blinks LED1 (PF9) and LED2 (PF10)
  ******************************************************************************
  */
#include "led_task.h"
#include "cmsis_os.h"

/* Task handle and attributes ------------------------------------------------*/
static osThreadId_t ledTaskHandle;
static const osThreadAttr_t ledTask_attributes = {
  .name = "ledTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/**
 * @brief  LED task: drive one LED on at a time for 500 ms per phase.
 *         Both LEDs are active-low because their anodes are pulled up.
  * @param  argument: Not used
  * @retval None
  */
static void LED_Task(void *argument)
{
  (void)argument;

  for (;;)
  {
    BSP_LED1_ON();
    BSP_LED2_OFF();
    osDelay(500);

    BSP_LED1_OFF();
    BSP_LED2_ON();
    osDelay(500);
  }
}

/**
  * @brief  Create the LED task. Call from MX_FREERTOS_Init().
  * @retval None
  */
void LED_TaskInit(void)
{
  ledTaskHandle = osThreadNew(LED_Task, NULL, &ledTask_attributes);
  (void)ledTaskHandle;
}

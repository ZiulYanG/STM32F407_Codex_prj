/**
  ******************************************************************************
  * @file    led_task.h
  * @brief   LED task header - PF9/PF10 LED control and FreeRTOS task interface
  ******************************************************************************
  */
#ifndef __LED_TASK_H
#define __LED_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_board.h"

/* Exported functions --------------------------------------------------------*/
void LED_TaskInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_TASK_H */

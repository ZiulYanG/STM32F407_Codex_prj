#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "main.h"

/** Initialise board-level resources owned by the Bootloader. */
void bsp_board_init(void);

/** Return a stable text description of the reset source flags. */
const char *bsp_board_get_reset_reason(void);

/** Clear reset flags after they have been reported. */
void bsp_board_clear_reset_flags(void);

/* LED control macros (PF9 = LED1, PF10 = LED2) ------------------------------*/
/* 默认上拉、低电平点亮 */
#define BSP_LED1_ON()     HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET)
#define BSP_LED1_OFF()    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET)
#define BSP_LED1_TOGGLE() HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9)

#define BSP_LED2_ON()     HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET)
#define BSP_LED2_OFF()    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET)
#define BSP_LED2_TOGGLE() HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_10)

#endif /* BSP_BOARD_H */

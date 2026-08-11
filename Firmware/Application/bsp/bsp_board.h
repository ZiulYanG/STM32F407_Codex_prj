#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "main.h"

void bsp_board_init(void);
const char *bsp_board_get_reset_reason(void);
void bsp_board_clear_reset_flags(void);
void bsp_board_flash_select(void);
void bsp_board_flash_deselect(void);

/* PF9/PF10 LEDs are active-low on this board. */
#define BSP_LED1_ON()  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET)
#define BSP_LED1_OFF() HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET)
#define BSP_LED2_ON()  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET)
#define BSP_LED2_OFF() HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET)

#endif /* BSP_BOARD_H */

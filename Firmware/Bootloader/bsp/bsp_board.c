#include "bsp_board.h"

void bsp_board_init(void)
{
    /* Board-specific Bootloader initialisation is added here. */

    /* LED initial state: LED1 on, LED2 off (alternating pattern start). */
    BSP_LED1_ON();
    BSP_LED2_OFF();
}

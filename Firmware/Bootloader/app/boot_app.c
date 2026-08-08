#include "boot_app.h"

#include "bsp_board.h"

void boot_app_init(void)
{
    bsp_board_init();
}

void boot_app_process(void)
{
    /*
     * Future work: evaluate boot metadata, verify the candidate image and
     * either install it or transfer control to the application firmware.
     */
}

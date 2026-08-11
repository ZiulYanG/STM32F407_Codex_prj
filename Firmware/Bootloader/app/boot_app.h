#ifndef BOOT_APP_H
#define BOOT_APP_H

#include <stdbool.h>
#include <stdint.h>

/** Initialise Bootloader application services after CubeMX initialisation. */
void boot_app_init(void);

/** Run one iteration of the Bootloader application state machine. */
void boot_app_process(void);

bool boot_app_set_window_ms(uint32_t window_ms);
uint32_t boot_app_get_window_ms(void);
void boot_app_request_stay(void);
void boot_app_request_jump(void);

#endif /* BOOT_APP_H */

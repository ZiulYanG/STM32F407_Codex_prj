#ifndef BOOT_CONSOLE_H
#define BOOT_CONSOLE_H

#include <stdint.h>

#include <stdbool.h>

bool boot_console_init(void);
uint32_t boot_console_get_stack_high_water_mark_words(void);

#endif /* BOOT_CONSOLE_H */

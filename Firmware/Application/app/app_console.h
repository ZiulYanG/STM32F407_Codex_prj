#ifndef APP_CONSOLE_H
#define APP_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

bool app_console_init(void);
uint32_t app_console_get_stack_high_water_mark_words(void);

#endif /* APP_CONSOLE_H */

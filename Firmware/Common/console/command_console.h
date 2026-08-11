#ifndef COMMAND_CONSOLE_H
#define COMMAND_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COMMAND_CONSOLE_LINE_SIZE 128U
#define COMMAND_CONSOLE_MAX_ARGS  8U

typedef int (*command_console_write_fn)(void *context,
                                        const uint8_t *data,
                                        size_t length);
typedef void (*command_console_execute_fn)(void *context,
                                           int argc,
                                           char *argv[]);

typedef struct
{
    void *context;
    command_console_write_fn write;
    command_console_execute_fn execute;
    const char *prompt;
    size_t line_length;
    bool overflowed;
    bool previous_was_cr;
    char line[COMMAND_CONSOLE_LINE_SIZE];
} command_console_t;

bool command_console_init(command_console_t *console,
                          void *context,
                          command_console_write_fn write,
                          command_console_execute_fn execute,
                          const char *prompt);
void command_console_show_prompt(command_console_t *console);
void command_console_feed(command_console_t *console,
                          const uint8_t *data,
                          size_t length);

#endif /* COMMAND_CONSOLE_H */

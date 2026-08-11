#include "command_console.h"

#include <string.h>

static void console_write_text(command_console_t *console, const char *text)
{
    (void)console->write(console->context,
                         (const uint8_t *)text,
                         strlen(text));
}

static void console_execute_line(command_console_t *console)
{
    char *argv[COMMAND_CONSOLE_MAX_ARGS];
    int argc = 0;
    char *cursor;

    if (console->overflowed)
    {
        console_write_text(console, "ERR line too long\r\n");
        return;
    }

    console->line[console->line_length] = '\0';
    cursor = console->line;
    while ((*cursor != '\0') &&
           (argc < (int)COMMAND_CONSOLE_MAX_ARGS))
    {
        while ((*cursor == ' ') || (*cursor == '\t'))
        {
            ++cursor;
        }
        if (*cursor == '\0')
        {
            break;
        }
        argv[argc] = cursor;
        ++argc;
        while ((*cursor != '\0') &&
               (*cursor != ' ') && (*cursor != '\t'))
        {
            ++cursor;
        }
        if (*cursor != '\0')
        {
            *cursor = '\0';
            ++cursor;
        }
    }

    while ((*cursor == ' ') || (*cursor == '\t'))
    {
        ++cursor;
    }
    if (*cursor != '\0')
    {
        console_write_text(console, "ERR too many arguments\r\n");
    }
    else if (argc > 0)
    {
        console->execute(console->context, argc, argv);
    }
}

bool command_console_init(command_console_t *console,
                          void *context,
                          command_console_write_fn write,
                          command_console_execute_fn execute,
                          const char *prompt)
{
    if ((console == NULL) || (write == NULL) ||
        (execute == NULL) || (prompt == NULL))
    {
        return false;
    }

    memset(console, 0, sizeof(*console));
    console->context = context;
    console->write = write;
    console->execute = execute;
    console->prompt = prompt;
    return true;
}

void command_console_show_prompt(command_console_t *console)
{
    if (console != NULL)
    {
        console_write_text(console, console->prompt);
    }
}

void command_console_feed(command_console_t *console,
                          const uint8_t *data,
                          size_t length)
{
    size_t index;

    if ((console == NULL) || ((data == NULL) && (length > 0U)))
    {
        return;
    }

    for (index = 0U; index < length; ++index)
    {
        uint8_t value = data[index];

        if ((value == '\r') || (value == '\n'))
        {
            if ((value == '\n') && console->previous_was_cr)
            {
                console->previous_was_cr = false;
                continue;
            }
            console->previous_was_cr = (value == '\r');
            console_write_text(console, "\r\n");
            console_execute_line(console);
            console->line_length = 0U;
            console->overflowed = false;
            command_console_show_prompt(console);
        }
        else if ((value == 0x08U) || (value == 0x7FU))
        {
            console->previous_was_cr = false;
            if (console->line_length > 0U)
            {
                --console->line_length;
            }
        }
        else if ((value == '\t') ||
                 ((value >= 0x20U) && (value <= 0x7EU)))
        {
            console->previous_was_cr = false;
            if (console->line_length < (sizeof(console->line) - 1U))
            {
                console->line[console->line_length] = (char)value;
                ++console->line_length;
            }
            else
            {
                console->overflowed = true;
            }
        }
    }
}

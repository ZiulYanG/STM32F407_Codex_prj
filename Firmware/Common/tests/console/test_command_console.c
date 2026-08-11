#include "command_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                    \
                    __FILE__, __LINE__, #condition);                            \
            return EXIT_FAILURE;                                                \
        }                                                                       \
    } while (0)

typedef struct
{
    char output[512];
    size_t output_length;
    int execute_count;
    int argc;
    char args[COMMAND_CONSOLE_MAX_ARGS][32];
} fixture_t;

static int fixture_write(void *context, const uint8_t *data, size_t length)
{
    fixture_t *fixture = context;

    if ((fixture->output_length + length) >= sizeof(fixture->output))
    {
        return -1;
    }
    memcpy(&fixture->output[fixture->output_length], data, length);
    fixture->output_length += length;
    fixture->output[fixture->output_length] = '\0';
    return 0;
}

static void fixture_execute(void *context, int argc, char *argv[])
{
    fixture_t *fixture = context;
    int index;

    ++fixture->execute_count;
    fixture->argc = argc;
    for (index = 0; index < argc; ++index)
    {
        (void)snprintf(fixture->args[index],
                       sizeof(fixture->args[index]),
                       "%s",
                       argv[index]);
    }
}

int main(void)
{
    command_console_t console;
    fixture_t fixture = {0};
    static const uint8_t first[] = "  set\tlog_level  debug\r\n";
    static const uint8_t edited[] = "get log_levex\bl\r";
    uint8_t overflow[COMMAND_CONSOLE_LINE_SIZE + 4U];

    CHECK(command_console_init(&console,
                               &fixture,
                               fixture_write,
                               fixture_execute,
                               "app> "));
    command_console_show_prompt(&console);
    command_console_feed(&console, first, sizeof(first) - 1U);
    CHECK(fixture.execute_count == 1);
    CHECK(fixture.argc == 3);
    CHECK(strcmp(fixture.args[0], "set") == 0);
    CHECK(strcmp(fixture.args[1], "log_level") == 0);
    CHECK(strcmp(fixture.args[2], "debug") == 0);

    command_console_feed(&console, edited, sizeof(edited) - 1U);
    CHECK(fixture.execute_count == 2);
    CHECK(fixture.argc == 2);
    CHECK(strcmp(fixture.args[0], "get") == 0);
    CHECK(strcmp(fixture.args[1], "log_level") == 0);

    memset(overflow, 'A', sizeof(overflow));
    overflow[sizeof(overflow) - 1U] = '\n';
    command_console_feed(&console, overflow, sizeof(overflow));
    CHECK(strstr(fixture.output, "ERR line too long") != NULL);
    CHECK(strstr(fixture.output, "app> ") != NULL);

    puts("command console host tests: PASS");
    return EXIT_SUCCESS;
}

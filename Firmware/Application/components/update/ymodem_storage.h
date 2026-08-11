#ifndef YMODEM_STORAGE_H
#define YMODEM_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "storage.h"
#include "ymodem.h"

struct ymodem_storage_sink
{
    struct storage_device *storage;
    struct storage_info info;
    uint32_t file_size;
    uint32_t written_size;
    uint32_t erased_until;
    bool active;
};

struct ymodem_storage_source
{
    struct storage_device *storage;
    char file_name[YMODEM_MAX_FILE_NAME];
    uint32_t file_size;
    bool active;
};

bool ymodem_storage_sink_init(struct ymodem_storage_sink *sink,
                              struct storage_device *storage);
ymodem_file_sink_t ymodem_storage_sink_interface(
    struct ymodem_storage_sink *sink);

bool ymodem_storage_source_init(struct ymodem_storage_source *source,
                                struct storage_device *storage,
                                const char *file_name,
                                uint32_t file_size);
ymodem_file_source_t ymodem_storage_source_interface(
    struct ymodem_storage_source *source);

#endif /* YMODEM_STORAGE_H */

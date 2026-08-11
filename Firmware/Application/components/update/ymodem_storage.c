#include "ymodem_storage.h"

#include <string.h>

static bool ymodem_storage_range_valid(uint32_t capacity,
                                       uint32_t offset,
                                       size_t length)
{
    return (offset <= capacity) &&
           (length <= (size_t)(capacity - offset));
}

static int ymodem_storage_sink_begin(void *context,
                                     const ymodem_file_info_t *file)
{
    struct ymodem_storage_sink *sink = context;

    if ((sink == NULL) || (file == NULL) || sink->active ||
        (file->size == 0U) || (file->size > sink->info.capacity_bytes) ||
        ((sink->info.capabilities & STORAGE_CAP_WRITE) == 0U))
    {
        return -1;
    }
    if (((sink->info.capabilities & STORAGE_CAP_WRITE_REQUIRES_ERASE) != 0U) &&
        (((sink->info.capabilities & STORAGE_CAP_ERASE) == 0U) ||
         (sink->info.erase_size_bytes == 0U)))
    {
        return -1;
    }

    sink->file_size = file->size;
    sink->written_size = 0U;
    sink->erased_until = 0U;
    sink->active = true;
    return 0;
}

static int ymodem_storage_erase_to(struct ymodem_storage_sink *sink,
                                   uint32_t required_end)
{
    uint32_t erase_size = sink->info.erase_size_bytes;

    if ((sink->info.capabilities & STORAGE_CAP_WRITE_REQUIRES_ERASE) == 0U)
    {
        return 0;
    }

    while (sink->erased_until < required_end)
    {
        if (storage_erase(sink->storage,
                          sink->erased_until,
                          erase_size) != STORAGE_OK)
        {
            return -1;
        }
        sink->erased_until += erase_size;
    }
    return 0;
}

static int ymodem_storage_sink_write(void *context,
                                     uint32_t offset,
                                     const uint8_t *data,
                                     size_t length)
{
    struct ymodem_storage_sink *sink = context;
    uint32_t required_end;

    if ((sink == NULL) || !sink->active || (data == NULL) ||
        (length == 0U) || (offset != sink->written_size) ||
        !ymodem_storage_range_valid(sink->file_size, offset, length))
    {
        return -1;
    }
    required_end = offset + (uint32_t)length;
    if ((ymodem_storage_erase_to(sink, required_end) != 0) ||
        (storage_write(sink->storage, offset, data, length) != STORAGE_OK))
    {
        return -1;
    }

    sink->written_size = required_end;
    return 0;
}

static int ymodem_storage_sink_finish(void *context)
{
    struct ymodem_storage_sink *sink = context;

    if ((sink == NULL) || !sink->active ||
        (sink->written_size != sink->file_size))
    {
        return -1;
    }
    if (storage_sync(sink->storage) != STORAGE_OK)
    {
        return -1;
    }
    sink->active = false;
    return 0;
}

static void ymodem_storage_sink_abort(void *context)
{
    struct ymodem_storage_sink *sink = context;

    if (sink != NULL)
    {
        /* Partially written bytes intentionally remain invalid.  P6 metadata
           will decide whether a later session resumes or erases them. */
        (void)storage_sync(sink->storage);
        sink->active = false;
    }
}

bool ymodem_storage_sink_init(struct ymodem_storage_sink *sink,
                              struct storage_device *storage)
{
    if ((sink == NULL) || (storage == NULL) ||
        (storage_get_info(storage, &sink->info) != STORAGE_OK))
    {
        return false;
    }

    memset(sink, 0, sizeof(*sink));
    sink->storage = storage;
    return storage_get_info(storage, &sink->info) == STORAGE_OK;
}

ymodem_file_sink_t ymodem_storage_sink_interface(
    struct ymodem_storage_sink *sink)
{
    const ymodem_file_sink_t interface = {
        .context = sink,
        .begin = ymodem_storage_sink_begin,
        .write = ymodem_storage_sink_write,
        .finish = ymodem_storage_sink_finish,
        .abort = ymodem_storage_sink_abort,
    };

    return interface;
}

static int ymodem_storage_source_open(void *context,
                                      ymodem_file_info_t *file)
{
    struct ymodem_storage_source *source = context;

    if ((source == NULL) || (file == NULL) || source->active)
    {
        return -1;
    }
    (void)strncpy(file->name, source->file_name, sizeof(file->name));
    file->name[sizeof(file->name) - 1U] = '\0';
    file->size = source->file_size;
    source->active = true;
    return 0;
}

static int ymodem_storage_source_read(void *context,
                                      uint32_t offset,
                                      uint8_t *data,
                                      size_t capacity,
                                      size_t *read_length)
{
    struct ymodem_storage_source *source = context;

    if ((source == NULL) || !source->active || (data == NULL) ||
        (read_length == NULL) || (capacity == 0U) ||
        !ymodem_storage_range_valid(source->file_size, offset, capacity))
    {
        return -1;
    }
    if (storage_read(source->storage, offset, data, capacity) != STORAGE_OK)
    {
        return -1;
    }
    *read_length = capacity;
    return 0;
}

static void ymodem_storage_source_close(void *context)
{
    struct ymodem_storage_source *source = context;

    if (source != NULL)
    {
        source->active = false;
    }
}

bool ymodem_storage_source_init(struct ymodem_storage_source *source,
                                struct storage_device *storage,
                                const char *file_name,
                                uint32_t file_size)
{
    struct storage_info info;
    size_t name_length;

    if ((source == NULL) || (storage == NULL) || (file_name == NULL) ||
        (storage_get_info(storage, &info) != STORAGE_OK) ||
        ((info.capabilities & STORAGE_CAP_READ) == 0U) ||
        (file_size == 0U) || (file_size > info.capacity_bytes))
    {
        return false;
    }
    name_length = strlen(file_name);
    if ((name_length == 0U) || (name_length >= sizeof(source->file_name)))
    {
        return false;
    }

    memset(source, 0, sizeof(*source));
    source->storage = storage;
    source->file_size = file_size;
    memcpy(source->file_name, file_name, name_length + 1U);
    return true;
}

ymodem_file_source_t ymodem_storage_source_interface(
    struct ymodem_storage_source *source)
{
    const ymodem_file_source_t interface = {
        .context = source,
        .open = ymodem_storage_source_open,
        .read = ymodem_storage_source_read,
        .close = ymodem_storage_source_close,
    };

    return interface;
}

#include "storage_partition.h"

static struct storage_partition *storage_partition_context(
    struct storage_device *device)
{
    return (struct storage_partition *)device->private_data;
}

static int storage_partition_open(struct storage_device *device)
{
    return storage_open(storage_partition_context(device)->parent);
}

static int storage_partition_read(struct storage_device *device,
                                  uint32_t offset,
                                  void *buffer,
                                  size_t length)
{
    const struct storage_partition *partition =
        storage_partition_context(device);

    return storage_read(partition->parent,
                        partition->parent_offset + offset,
                        buffer,
                        length);
}

static int storage_partition_write(struct storage_device *device,
                                   uint32_t offset,
                                   const void *buffer,
                                   size_t length)
{
    const struct storage_partition *partition =
        storage_partition_context(device);

    return storage_write(partition->parent,
                         partition->parent_offset + offset,
                         buffer,
                         length);
}

static int storage_partition_erase(struct storage_device *device,
                                   uint32_t offset,
                                   size_t length)
{
    const struct storage_partition *partition =
        storage_partition_context(device);

    return storage_erase(partition->parent,
                         partition->parent_offset + offset,
                         length);
}

static int storage_partition_sync(struct storage_device *device)
{
    return storage_sync(storage_partition_context(device)->parent);
}

/* Closing a view must not close its shared parent device. */
static int storage_partition_close(struct storage_device *device)
{
    (void)device;
    return STORAGE_OK;
}

static const struct storage_ops storage_partition_ops = {
    .open = storage_partition_open,
    .read = storage_partition_read,
    .write = storage_partition_write,
    .erase = storage_partition_erase,
    .sync = storage_partition_sync,
    .close = storage_partition_close,
};

int storage_partition_bind(struct storage_device *device,
                           const char *name,
                           struct storage_partition *partition,
                           struct storage_device *parent,
                           uint32_t parent_offset,
                           uint32_t length)
{
    struct storage_info parent_info;
    struct storage_info partition_info;

    if ((partition == NULL) || (parent == NULL) || (length == 0U) ||
        (storage_get_info(parent, &parent_info) != STORAGE_OK) ||
        (parent_offset >= parent_info.capacity_bytes) ||
        (length > (parent_info.capacity_bytes - parent_offset)))
    {
        return STORAGE_ERR_INVALID;
    }

    if (((parent_info.capabilities & STORAGE_CAP_ERASE) != 0U) &&
        (((parent_offset % parent_info.erase_size_bytes) != 0U) ||
         ((length % parent_info.erase_size_bytes) != 0U)))
    {
        return STORAGE_ERR_INVALID;
    }

    partition_info = parent_info;
    partition_info.capacity_bytes = length;
    partition->parent = parent;
    partition->parent_offset = parent_offset;

    return storage_device_init(device,
                               name,
                               &storage_partition_ops,
                               partition,
                               &partition_info);
}

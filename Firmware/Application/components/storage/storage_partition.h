#ifndef STORAGE_PARTITION_H
#define STORAGE_PARTITION_H

#include "storage.h"

struct storage_partition
{
    struct storage_device *parent;
    uint32_t parent_offset;
};

int storage_partition_bind(struct storage_device *device,
                           const char *name,
                           struct storage_partition *partition,
                           struct storage_device *parent,
                           uint32_t parent_offset,
                           uint32_t length);

#endif /* STORAGE_PARTITION_H */

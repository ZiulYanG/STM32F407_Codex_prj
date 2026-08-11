#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum storage_status
{
    STORAGE_OK = 0,
    STORAGE_ERR_INVALID = -1,
    STORAGE_ERR_STATE = -2,
    STORAGE_ERR_RANGE = -3,
    STORAGE_ERR_UNSUPPORTED = -4,
    STORAGE_ERR_IO = -5,
    STORAGE_ERR_TIMEOUT = -6,
    STORAGE_ERR_BUSY = -7,
};

enum storage_capability
{
    STORAGE_CAP_READ = (1UL << 0),
    STORAGE_CAP_WRITE = (1UL << 1),
    STORAGE_CAP_ERASE = (1UL << 2),
    STORAGE_CAP_WRITE_REQUIRES_ERASE = (1UL << 3),
};

struct storage_device;

struct storage_info
{
    uint32_t capacity_bytes;
    uint32_t program_page_size_bytes;
    uint32_t erase_size_bytes;
    uint32_t capabilities;
    uint8_t erased_value;
};

struct storage_ops
{
    int (*open)(struct storage_device *device);
    int (*read)(struct storage_device *device,
                uint32_t offset,
                void *buffer,
                size_t length);
    int (*write)(struct storage_device *device,
                 uint32_t offset,
                 const void *buffer,
                 size_t length);
    int (*erase)(struct storage_device *device,
                 uint32_t offset,
                 size_t length);
    int (*sync)(struct storage_device *device);
    int (*close)(struct storage_device *device);
};

struct storage_device
{
    const char *name;
    const struct storage_ops *ops;
    void *private_data;
    struct storage_info info;
    bool is_open;
};

int storage_device_init(struct storage_device *device,
                        const char *name,
                        const struct storage_ops *ops,
                        void *private_data,
                        const struct storage_info *info);
int storage_open(struct storage_device *device);
int storage_read(struct storage_device *device,
                 uint32_t offset,
                 void *buffer,
                 size_t length);
int storage_write(struct storage_device *device,
                  uint32_t offset,
                  const void *buffer,
                  size_t length);
int storage_erase(struct storage_device *device,
                  uint32_t offset,
                  size_t length);
int storage_sync(struct storage_device *device);
int storage_close(struct storage_device *device);
int storage_get_info(const struct storage_device *device,
                     struct storage_info *info);

#endif /* STORAGE_H */

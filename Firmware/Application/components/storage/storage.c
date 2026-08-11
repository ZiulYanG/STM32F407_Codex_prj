#include "storage.h"

#include <string.h>

static bool storage_range_valid(const struct storage_device *device,
                                uint32_t offset,
                                size_t length)
{
    return (length != 0U) && (offset < device->info.capacity_bytes) &&
           (length <= (device->info.capacity_bytes - offset));
}

static int storage_validate_definition(const struct storage_ops *ops,
                                       const struct storage_info *info)
{
    if ((ops == NULL) || (info == NULL) || (info->capacity_bytes == 0U))
    {
        return STORAGE_ERR_INVALID;
    }
    if (((info->capabilities & STORAGE_CAP_READ) != 0U) &&
        (ops->read == NULL))
    {
        return STORAGE_ERR_INVALID;
    }
    if (((info->capabilities & STORAGE_CAP_WRITE) != 0U) &&
        (ops->write == NULL))
    {
        return STORAGE_ERR_INVALID;
    }
    if (((info->capabilities & STORAGE_CAP_ERASE) != 0U) &&
        ((ops->erase == NULL) || (info->erase_size_bytes == 0U)))
    {
        return STORAGE_ERR_INVALID;
    }
    if (((info->capabilities & STORAGE_CAP_WRITE_REQUIRES_ERASE) != 0U) &&
        ((info->capabilities & STORAGE_CAP_ERASE) == 0U))
    {
        return STORAGE_ERR_INVALID;
    }

    return STORAGE_OK;
}

int storage_device_init(struct storage_device *device,
                        const char *name,
                        const struct storage_ops *ops,
                        void *private_data,
                        const struct storage_info *info)
{
    int status;

    if ((device == NULL) || (name == NULL) || (name[0] == '\0') ||
        (private_data == NULL))
    {
        return STORAGE_ERR_INVALID;
    }

    status = storage_validate_definition(ops, info);
    if (status != STORAGE_OK)
    {
        return status;
    }

    memset(device, 0, sizeof(*device));
    device->name = name;
    device->ops = ops;
    device->private_data = private_data;
    device->info = *info;
    return STORAGE_OK;
}

int storage_open(struct storage_device *device)
{
    int status = STORAGE_OK;

    if ((device == NULL) || (device->ops == NULL))
    {
        return STORAGE_ERR_INVALID;
    }
    if (device->is_open)
    {
        return STORAGE_OK;
    }
    if (device->ops->open != NULL)
    {
        status = device->ops->open(device);
    }
    if (status == STORAGE_OK)
    {
        device->is_open = true;
    }
    return status;
}

int storage_read(struct storage_device *device,
                 uint32_t offset,
                 void *buffer,
                 size_t length)
{
    if ((device == NULL) || (buffer == NULL))
    {
        return STORAGE_ERR_INVALID;
    }
    if (!device->is_open)
    {
        return STORAGE_ERR_STATE;
    }
    if (((device->info.capabilities & STORAGE_CAP_READ) == 0U) ||
        (device->ops->read == NULL))
    {
        return STORAGE_ERR_UNSUPPORTED;
    }
    if (!storage_range_valid(device, offset, length))
    {
        return STORAGE_ERR_RANGE;
    }
    return device->ops->read(device, offset, buffer, length);
}

int storage_write(struct storage_device *device,
                  uint32_t offset,
                  const void *buffer,
                  size_t length)
{
    if ((device == NULL) || (buffer == NULL))
    {
        return STORAGE_ERR_INVALID;
    }
    if (!device->is_open)
    {
        return STORAGE_ERR_STATE;
    }
    if (((device->info.capabilities & STORAGE_CAP_WRITE) == 0U) ||
        (device->ops->write == NULL))
    {
        return STORAGE_ERR_UNSUPPORTED;
    }
    if (!storage_range_valid(device, offset, length))
    {
        return STORAGE_ERR_RANGE;
    }
    return device->ops->write(device, offset, buffer, length);
}

int storage_erase(struct storage_device *device,
                  uint32_t offset,
                  size_t length)
{
    uint32_t erase_size;

    if (device == NULL)
    {
        return STORAGE_ERR_INVALID;
    }
    if (!device->is_open)
    {
        return STORAGE_ERR_STATE;
    }
    if (((device->info.capabilities & STORAGE_CAP_ERASE) == 0U) ||
        (device->ops->erase == NULL))
    {
        return STORAGE_ERR_UNSUPPORTED;
    }
    if (!storage_range_valid(device, offset, length))
    {
        return STORAGE_ERR_RANGE;
    }

    erase_size = device->info.erase_size_bytes;
    if (((offset % erase_size) != 0U) || ((length % erase_size) != 0U))
    {
        return STORAGE_ERR_INVALID;
    }
    return device->ops->erase(device, offset, length);
}

int storage_sync(struct storage_device *device)
{
    if (device == NULL)
    {
        return STORAGE_ERR_INVALID;
    }
    if (!device->is_open)
    {
        return STORAGE_ERR_STATE;
    }
    if (device->ops->sync == NULL)
    {
        return STORAGE_OK;
    }
    return device->ops->sync(device);
}

int storage_close(struct storage_device *device)
{
    int status = STORAGE_OK;

    if (device == NULL)
    {
        return STORAGE_ERR_INVALID;
    }
    if (!device->is_open)
    {
        return STORAGE_ERR_STATE;
    }
    if (device->ops->close != NULL)
    {
        status = device->ops->close(device);
    }
    if (status == STORAGE_OK)
    {
        device->is_open = false;
    }
    return status;
}

int storage_get_info(const struct storage_device *device,
                     struct storage_info *info)
{
    if ((device == NULL) || (device->ops == NULL) || (info == NULL))
    {
        return STORAGE_ERR_INVALID;
    }

    *info = device->info;
    return STORAGE_OK;
}

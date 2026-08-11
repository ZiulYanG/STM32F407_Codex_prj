#include "storage_spi_nor.h"

static struct storage_spi_nor *storage_spi_nor_context(
    struct storage_device *device)
{
    return (struct storage_spi_nor *)device->private_data;
}

static int storage_spi_nor_open(struct storage_device *device)
{
    struct storage_spi_nor *adapter = storage_spi_nor_context(device);

    return spi_nor_wait_ready(adapter->driver,
                              adapter->program_timeout_ms)
               ? STORAGE_OK
               : STORAGE_ERR_IO;
}

static int storage_spi_nor_read(struct storage_device *device,
                                uint32_t offset,
                                void *buffer,
                                size_t length)
{
    struct storage_spi_nor *adapter = storage_spi_nor_context(device);

    return spi_nor_read(adapter->driver,
                        offset,
                        (uint8_t *)buffer,
                        length)
               ? STORAGE_OK
               : STORAGE_ERR_IO;
}

static int storage_spi_nor_write(struct storage_device *device,
                                 uint32_t offset,
                                 const void *buffer,
                                 size_t length)
{
    struct storage_spi_nor *adapter = storage_spi_nor_context(device);

    return spi_nor_write(adapter->driver,
                         offset,
                         (const uint8_t *)buffer,
                         length,
                         adapter->program_timeout_ms)
               ? STORAGE_OK
               : STORAGE_ERR_IO;
}

static int storage_spi_nor_erase(struct storage_device *device,
                                 uint32_t offset,
                                 size_t length)
{
    struct storage_spi_nor *adapter = storage_spi_nor_context(device);
    size_t bytes_remaining = length;
    uint32_t current_offset = offset;

    while (bytes_remaining != 0U)
    {
        if (!spi_nor_sector_erase_4k(adapter->driver,
                                     current_offset,
                                     adapter->erase_timeout_ms))
        {
            return STORAGE_ERR_IO;
        }
        current_offset += SPI_NOR_SECTOR_SIZE_BYTES;
        bytes_remaining -= SPI_NOR_SECTOR_SIZE_BYTES;
    }
    return STORAGE_OK;
}

static int storage_spi_nor_sync(struct storage_device *device)
{
    struct storage_spi_nor *adapter = storage_spi_nor_context(device);

    return spi_nor_wait_ready(adapter->driver,
                              adapter->erase_timeout_ms)
               ? STORAGE_OK
               : STORAGE_ERR_IO;
}

static const struct storage_ops storage_spi_nor_ops = {
    .open = storage_spi_nor_open,
    .read = storage_spi_nor_read,
    .write = storage_spi_nor_write,
    .erase = storage_spi_nor_erase,
    .sync = storage_spi_nor_sync,
    .close = NULL,
};

int storage_spi_nor_bind(struct storage_device *device,
                         const char *name,
                         struct storage_spi_nor *adapter,
                         spi_nor_t *driver,
                         uint32_t program_timeout_ms,
                         uint32_t erase_timeout_ms)
{
    struct storage_info info;

    if ((adapter == NULL) || (driver == NULL) || !driver->initialized ||
        (driver->capacity_bytes == 0U) || (program_timeout_ms == 0U) ||
        (erase_timeout_ms == 0U))
    {
        return STORAGE_ERR_INVALID;
    }

    info.capacity_bytes = driver->capacity_bytes;
    info.program_page_size_bytes = SPI_NOR_PAGE_SIZE_BYTES;
    info.erase_size_bytes = SPI_NOR_SECTOR_SIZE_BYTES;
    info.capabilities = STORAGE_CAP_READ | STORAGE_CAP_WRITE |
                        STORAGE_CAP_ERASE |
                        STORAGE_CAP_WRITE_REQUIRES_ERASE;
    info.erased_value = 0xFFU;

    adapter->driver = driver;
    adapter->program_timeout_ms = program_timeout_ms;
    adapter->erase_timeout_ms = erase_timeout_ms;
    return storage_device_init(device,
                               name,
                               &storage_spi_nor_ops,
                               adapter,
                               &info);
}

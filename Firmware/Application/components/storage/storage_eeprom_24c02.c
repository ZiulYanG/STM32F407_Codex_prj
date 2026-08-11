#include "storage_eeprom_24c02.h"

static struct storage_eeprom_24c02 *storage_eeprom_context(
    struct storage_device *device)
{
    return (struct storage_eeprom_24c02 *)device->private_data;
}

static int storage_eeprom_open(struct storage_device *device)
{
    struct storage_eeprom_24c02 *adapter = storage_eeprom_context(device);

    return eeprom_24c02_probe(adapter->driver, adapter->timeout_ms)
               ? STORAGE_OK
               : STORAGE_ERR_IO;
}

static int storage_eeprom_read(struct storage_device *device,
                               uint32_t offset,
                               void *buffer,
                               size_t length)
{
    struct storage_eeprom_24c02 *adapter = storage_eeprom_context(device);

    return eeprom_24c02_read(adapter->driver,
                             (uint16_t)offset,
                             (uint8_t *)buffer,
                             length,
                             adapter->timeout_ms)
               ? STORAGE_OK
               : STORAGE_ERR_IO;
}

static int storage_eeprom_write(struct storage_device *device,
                                uint32_t offset,
                                const void *buffer,
                                size_t length)
{
    struct storage_eeprom_24c02 *adapter = storage_eeprom_context(device);

    return eeprom_24c02_write(adapter->driver,
                              (uint16_t)offset,
                              (const uint8_t *)buffer,
                              length,
                              adapter->timeout_ms)
               ? STORAGE_OK
               : STORAGE_ERR_IO;
}

static int storage_eeprom_sync(struct storage_device *device)
{
    struct storage_eeprom_24c02 *adapter = storage_eeprom_context(device);

    return eeprom_24c02_probe(adapter->driver, adapter->timeout_ms)
               ? STORAGE_OK
               : STORAGE_ERR_IO;
}

static const struct storage_ops storage_eeprom_ops = {
    .open = storage_eeprom_open,
    .read = storage_eeprom_read,
    .write = storage_eeprom_write,
    .erase = NULL,
    .sync = storage_eeprom_sync,
    .close = NULL,
};

int storage_eeprom_24c02_bind(struct storage_device *device,
                              const char *name,
                              struct storage_eeprom_24c02 *adapter,
                              eeprom_24c02_t *driver,
                              uint32_t timeout_ms)
{
    const struct storage_info info = {
        .capacity_bytes = EEPROM_24C02_CAPACITY_BYTES,
        .program_page_size_bytes = EEPROM_24C02_PAGE_SIZE_BYTES,
        .erase_size_bytes = 0U,
        .capabilities = STORAGE_CAP_READ | STORAGE_CAP_WRITE,
        .erased_value = 0xFFU,
    };

    if ((adapter == NULL) || (driver == NULL) || !driver->initialized ||
        (timeout_ms == 0U))
    {
        return STORAGE_ERR_INVALID;
    }

    adapter->driver = driver;
    adapter->timeout_ms = timeout_ms;
    return storage_device_init(device,
                               name,
                               &storage_eeprom_ops,
                               adapter,
                               &info);
}

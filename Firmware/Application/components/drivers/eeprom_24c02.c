#include "eeprom_24c02.h"

#include <string.h>

#define EEPROM_24C02_READY_TRIALS 20U

static bool eeprom_24c02_range_valid(const eeprom_24c02_t *device,
                                     uint16_t address,
                                     size_t length)
{
    return (device != NULL) && device->initialized && (length != 0U) &&
           (address < EEPROM_24C02_CAPACITY_BYTES) &&
           (length <= (EEPROM_24C02_CAPACITY_BYTES - address));
}

bool eeprom_24c02_init(eeprom_24c02_t *device,
                       const eeprom_24c02_bus_t *bus)
{
    if ((device == NULL) || (bus == NULL) || (bus->is_ready == NULL) ||
        (bus->read == NULL) || (bus->write == NULL))
    {
        return false;
    }

    memset(device, 0, sizeof(*device));
    device->bus = *bus;
    device->initialized = true;
    return true;
}

bool eeprom_24c02_probe(eeprom_24c02_t *device, uint32_t timeout_ms)
{
    return (device != NULL) && device->initialized &&
           (device->bus.is_ready(device->bus.context,
                                 EEPROM_24C02_ADDRESS_7BIT,
                                 EEPROM_24C02_READY_TRIALS,
                                 timeout_ms) == 0);
}

bool eeprom_24c02_read(eeprom_24c02_t *device,
                       uint16_t address,
                       uint8_t *data,
                       size_t length,
                       uint32_t timeout_ms)
{
    return (data != NULL) && eeprom_24c02_range_valid(device, address, length) &&
           (device->bus.read(device->bus.context,
                             EEPROM_24C02_ADDRESS_7BIT,
                             (uint8_t)address,
                             data,
                             length,
                             timeout_ms) == 0);
}

bool eeprom_24c02_page_write(eeprom_24c02_t *device,
                             uint16_t address,
                             const uint8_t *data,
                             size_t length,
                             uint32_t timeout_ms)
{
    if ((data == NULL) || !eeprom_24c02_range_valid(device, address, length) ||
        (length > EEPROM_24C02_PAGE_SIZE_BYTES) ||
        ((address / EEPROM_24C02_PAGE_SIZE_BYTES) !=
         ((address + length - 1U) / EEPROM_24C02_PAGE_SIZE_BYTES)))
    {
        return false;
    }

    return (device->bus.write(device->bus.context,
                              EEPROM_24C02_ADDRESS_7BIT,
                              (uint8_t)address,
                              data,
                              length,
                              timeout_ms) == 0) &&
           eeprom_24c02_probe(device, timeout_ms);
}

bool eeprom_24c02_write(eeprom_24c02_t *device,
                        uint16_t address,
                        const uint8_t *data,
                        size_t length,
                        uint32_t timeout_ms)
{
    size_t bytes_remaining = length;
    size_t data_offset = 0U;
    uint16_t current_address = address;

    if ((data == NULL) || !eeprom_24c02_range_valid(device, address, length))
    {
        return false;
    }

    while (bytes_remaining != 0U)
    {
        const size_t page_offset =
            current_address % EEPROM_24C02_PAGE_SIZE_BYTES;
        const size_t page_space = EEPROM_24C02_PAGE_SIZE_BYTES - page_offset;
        const size_t chunk_length =
            (bytes_remaining < page_space) ? bytes_remaining : page_space;

        if (!eeprom_24c02_page_write(device,
                                     current_address,
                                     &data[data_offset],
                                     chunk_length,
                                     timeout_ms))
        {
            return false;
        }

        current_address = (uint16_t)(current_address + chunk_length);
        data_offset += chunk_length;
        bytes_remaining -= chunk_length;
    }

    return true;
}

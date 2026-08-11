#ifndef STORAGE_EEPROM_24C02_H
#define STORAGE_EEPROM_24C02_H

#include "eeprom_24c02.h"
#include "storage.h"

struct storage_eeprom_24c02
{
    eeprom_24c02_t *driver;
    uint32_t timeout_ms;
};

int storage_eeprom_24c02_bind(struct storage_device *device,
                              const char *name,
                              struct storage_eeprom_24c02 *adapter,
                              eeprom_24c02_t *driver,
                              uint32_t timeout_ms);

#endif /* STORAGE_EEPROM_24C02_H */

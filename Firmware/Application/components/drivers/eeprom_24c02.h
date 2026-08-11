#ifndef EEPROM_24C02_H
#define EEPROM_24C02_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EEPROM_24C02_ADDRESS_7BIT       0x50U
#define EEPROM_24C02_CAPACITY_BYTES     256U
#define EEPROM_24C02_PAGE_SIZE_BYTES    8U

typedef int (*eeprom_24c02_ready_fn)(void *context,
                                     uint8_t address_7bit,
                                     uint32_t trials,
                                     uint32_t timeout_ms);
typedef int (*eeprom_24c02_read_fn)(void *context,
                                    uint8_t address_7bit,
                                    uint8_t memory_address,
                                    uint8_t *data,
                                    size_t length,
                                    uint32_t timeout_ms);
typedef int (*eeprom_24c02_write_fn)(void *context,
                                     uint8_t address_7bit,
                                     uint8_t memory_address,
                                     const uint8_t *data,
                                     size_t length,
                                     uint32_t timeout_ms);

typedef struct
{
    void *context;
    eeprom_24c02_ready_fn is_ready;
    eeprom_24c02_read_fn read;
    eeprom_24c02_write_fn write;
} eeprom_24c02_bus_t;

typedef struct
{
    eeprom_24c02_bus_t bus;
    bool initialized;
} eeprom_24c02_t;

bool eeprom_24c02_init(eeprom_24c02_t *device,
                       const eeprom_24c02_bus_t *bus);
bool eeprom_24c02_probe(eeprom_24c02_t *device, uint32_t timeout_ms);
bool eeprom_24c02_read(eeprom_24c02_t *device,
                       uint16_t address,
                       uint8_t *data,
                       size_t length,
                       uint32_t timeout_ms);
bool eeprom_24c02_page_write(eeprom_24c02_t *device,
                             uint16_t address,
                             const uint8_t *data,
                             size_t length,
                             uint32_t timeout_ms);
bool eeprom_24c02_write(eeprom_24c02_t *device,
                        uint16_t address,
                        const uint8_t *data,
                        size_t length,
                        uint32_t timeout_ms);

#endif /* EEPROM_24C02_H */

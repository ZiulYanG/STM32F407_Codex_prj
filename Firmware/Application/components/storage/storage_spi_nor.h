#ifndef STORAGE_SPI_NOR_H
#define STORAGE_SPI_NOR_H

#include "spi_nor.h"
#include "storage.h"

struct storage_spi_nor
{
    spi_nor_t *driver;
    uint32_t program_timeout_ms;
    uint32_t erase_timeout_ms;
};

int storage_spi_nor_bind(struct storage_device *device,
                         const char *name,
                         struct storage_spi_nor *adapter,
                         spi_nor_t *driver,
                         uint32_t program_timeout_ms,
                         uint32_t erase_timeout_ms);

#endif /* STORAGE_SPI_NOR_H */

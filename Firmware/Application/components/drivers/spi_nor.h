#ifndef SPI_NOR_H
#define SPI_NOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPI_NOR_PAGE_SIZE_BYTES      256U
#define SPI_NOR_SECTOR_SIZE_BYTES    4096U
#define SPI_NOR_STATUS1_BUSY         0x01U
#define SPI_NOR_STATUS1_WRITE_ENABLE 0x02U

typedef int (*spi_nor_transfer_fn)(void *context,
                                   const uint8_t *tx_data,
                                   uint8_t *rx_data,
                                   size_t length,
                                   uint32_t timeout_ms);

typedef void (*spi_nor_chip_select_fn)(void *context, bool selected);
typedef void (*spi_nor_delay_fn)(void *context, uint32_t delay_ms);

typedef struct
{
    void *context;
    spi_nor_transfer_fn transfer;
    spi_nor_chip_select_fn chip_select;
    spi_nor_delay_fn delay;
} spi_nor_bus_t;

typedef struct
{
    uint8_t manufacturer;
    uint8_t memory_type;
    uint8_t capacity;
} spi_nor_jedec_id_t;

typedef struct
{
    const char *model;
    spi_nor_jedec_id_t jedec_id;
    uint32_t capacity_bytes;
} spi_nor_device_info_t;

typedef struct
{
    spi_nor_bus_t bus;
    uint32_t capacity_bytes;
    bool initialized;
} spi_nor_t;

bool spi_nor_init(spi_nor_t *device, const spi_nor_bus_t *bus);
bool spi_nor_release_power_down(spi_nor_t *device);
bool spi_nor_reset(spi_nor_t *device);
bool spi_nor_read_jedec_id(spi_nor_t *device, spi_nor_jedec_id_t *jedec_id);
const spi_nor_device_info_t *spi_nor_find_device(const spi_nor_jedec_id_t *jedec_id);
bool spi_nor_read_status1(spi_nor_t *device, uint8_t *status);
bool spi_nor_wait_ready(spi_nor_t *device, uint32_t timeout_ms);
bool spi_nor_read(spi_nor_t *device, uint32_t address, uint8_t *data, size_t length);
bool spi_nor_page_program(spi_nor_t *device,
                          uint32_t address,
                          const uint8_t *data,
                          size_t length,
                          uint32_t timeout_ms);
bool spi_nor_write(spi_nor_t *device,
                   uint32_t address,
                   const uint8_t *data,
                   size_t length,
                   uint32_t page_timeout_ms);
bool spi_nor_sector_erase_4k(spi_nor_t *device,
                             uint32_t address,
                             uint32_t timeout_ms);

#endif /* SPI_NOR_H */

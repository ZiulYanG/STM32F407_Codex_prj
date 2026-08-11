#include "spi_nor.h"

#define SPI_NOR_CMD_READ_JEDEC_ID        0x9FU
#define SPI_NOR_CMD_RELEASE_POWER_DOWN   0xABU
#define SPI_NOR_CMD_ENABLE_RESET         0x66U
#define SPI_NOR_CMD_RESET_DEVICE         0x99U
#define SPI_NOR_CMD_READ_STATUS1          0x05U
#define SPI_NOR_CMD_WRITE_ENABLE          0x06U
#define SPI_NOR_CMD_READ_DATA             0x03U
#define SPI_NOR_CMD_PAGE_PROGRAM          0x02U
#define SPI_NOR_CMD_SECTOR_ERASE_4K       0x20U
#define SPI_NOR_TRANSFER_TIMEOUT_MS      100U
#define SPI_NOR_JEDEC_TRANSACTION_LENGTH 4U
#define SPI_NOR_CAPACITY_128_MBIT_BYTES  (16UL * 1024UL * 1024UL)

static const spi_nor_device_info_t supported_devices[] = {
    {
        .model = "NM25Q128EVB",
        .jedec_id = {0x52U, 0x21U, 0x18U},
        .capacity_bytes = SPI_NOR_CAPACITY_128_MBIT_BYTES,
    },
    {
        .model = "W25Q128",
        .jedec_id = {0xEFU, 0x40U, 0x18U},
        .capacity_bytes = SPI_NOR_CAPACITY_128_MBIT_BYTES,
    },
};

static bool spi_nor_address_range_valid(const spi_nor_t *device,
                                        uint32_t address,
                                        size_t length)
{
    return (device != NULL) && device->initialized &&
           (device->capacity_bytes != 0U) && (length != 0U) &&
           (address < device->capacity_bytes) &&
           (length <= (size_t)(device->capacity_bytes - address));
}

static void spi_nor_build_address_command(uint8_t *header,
                                          uint8_t command,
                                          uint32_t address)
{
    header[0] = command;
    header[1] = (uint8_t)(address >> 16U);
    header[2] = (uint8_t)(address >> 8U);
    header[3] = (uint8_t)address;
}

static bool spi_nor_send_command(spi_nor_t *device, uint8_t command)
{
    bool success;

    device->bus.chip_select(device->bus.context, true);
    success = device->bus.transfer(device->bus.context,
                                   &command,
                                   NULL,
                                   1U,
                                   SPI_NOR_TRANSFER_TIMEOUT_MS) == 0;
    device->bus.chip_select(device->bus.context, false);
    return success;
}

bool spi_nor_init(spi_nor_t *device, const spi_nor_bus_t *bus)
{
    if ((device == NULL) || (bus == NULL) ||
        (bus->transfer == NULL) || (bus->chip_select == NULL) ||
        (bus->delay == NULL))
    {
        return false;
    }

    device->bus = *bus;
    device->capacity_bytes = 0U;
    device->initialized = true;
    device->bus.chip_select(device->bus.context, false);
    return true;
}

bool spi_nor_release_power_down(spi_nor_t *device)
{
    return (device != NULL) && device->initialized &&
           spi_nor_send_command(device, SPI_NOR_CMD_RELEASE_POWER_DOWN);
}

bool spi_nor_reset(spi_nor_t *device)
{
    if ((device == NULL) || !device->initialized)
    {
        return false;
    }

    return spi_nor_send_command(device, SPI_NOR_CMD_ENABLE_RESET) &&
           spi_nor_send_command(device, SPI_NOR_CMD_RESET_DEVICE);
}

bool spi_nor_read_jedec_id(spi_nor_t *device, spi_nor_jedec_id_t *jedec_id)
{
    const uint8_t request[SPI_NOR_JEDEC_TRANSACTION_LENGTH] = {
        SPI_NOR_CMD_READ_JEDEC_ID,
        0xFFU,
        0xFFU,
        0xFFU,
    };
    uint8_t response[SPI_NOR_JEDEC_TRANSACTION_LENGTH] = {0};
    bool success;

    if ((device == NULL) || (jedec_id == NULL) || !device->initialized)
    {
        return false;
    }

    device->bus.chip_select(device->bus.context, true);
    success = device->bus.transfer(device->bus.context,
                                   request,
                                   response,
                                   sizeof(request),
                                   SPI_NOR_TRANSFER_TIMEOUT_MS) == 0;
    device->bus.chip_select(device->bus.context, false);

    if (!success)
    {
        return false;
    }

    jedec_id->manufacturer = response[1];
    jedec_id->memory_type = response[2];
    jedec_id->capacity = response[3];
    {
        const spi_nor_device_info_t *device_info = spi_nor_find_device(jedec_id);

        device->capacity_bytes = (device_info != NULL) ? device_info->capacity_bytes : 0U;
    }
    return true;
}

const spi_nor_device_info_t *spi_nor_find_device(const spi_nor_jedec_id_t *jedec_id)
{
    size_t index;

    if (jedec_id == NULL)
    {
        return NULL;
    }

    for (index = 0U; index < (sizeof(supported_devices) / sizeof(supported_devices[0])); ++index)
    {
        const spi_nor_jedec_id_t *candidate = &supported_devices[index].jedec_id;

        if ((candidate->manufacturer == jedec_id->manufacturer) &&
            (candidate->memory_type == jedec_id->memory_type) &&
            (candidate->capacity == jedec_id->capacity))
        {
            return &supported_devices[index];
        }
    }

    return NULL;
}

bool spi_nor_read_status1(spi_nor_t *device, uint8_t *status)
{
    const uint8_t request[2] = {SPI_NOR_CMD_READ_STATUS1, 0xFFU};
    uint8_t response[2] = {0};
    bool success;

    if ((device == NULL) || (status == NULL) || !device->initialized)
    {
        return false;
    }

    device->bus.chip_select(device->bus.context, true);
    success = device->bus.transfer(device->bus.context,
                                   request,
                                   response,
                                   sizeof(request),
                                   SPI_NOR_TRANSFER_TIMEOUT_MS) == 0;
    device->bus.chip_select(device->bus.context, false);
    if (success)
    {
        *status = response[1];
    }
    return success;
}

bool spi_nor_wait_ready(spi_nor_t *device, uint32_t timeout_ms)
{
    uint32_t elapsed_ms = 0U;

    if ((device == NULL) || !device->initialized)
    {
        return false;
    }

    for (;;)
    {
        uint8_t status;

        if (!spi_nor_read_status1(device, &status))
        {
            return false;
        }
        if ((status & SPI_NOR_STATUS1_BUSY) == 0U)
        {
            return true;
        }
        if (elapsed_ms >= timeout_ms)
        {
            return false;
        }

        device->bus.delay(device->bus.context, 1U);
        ++elapsed_ms;
    }
}

static bool spi_nor_write_enable(spi_nor_t *device)
{
    uint8_t status;

    return spi_nor_send_command(device, SPI_NOR_CMD_WRITE_ENABLE) &&
           spi_nor_read_status1(device, &status) &&
           ((status & SPI_NOR_STATUS1_WRITE_ENABLE) != 0U);
}

bool spi_nor_read(spi_nor_t *device, uint32_t address, uint8_t *data, size_t length)
{
    uint8_t header[4];
    bool success;

    if ((data == NULL) || !spi_nor_address_range_valid(device, address, length) ||
        (length > UINT16_MAX))
    {
        return false;
    }

    spi_nor_build_address_command(header, SPI_NOR_CMD_READ_DATA, address);
    device->bus.chip_select(device->bus.context, true);
    success = device->bus.transfer(device->bus.context,
                                   header,
                                   NULL,
                                   sizeof(header),
                                   SPI_NOR_TRANSFER_TIMEOUT_MS) == 0;
    if (success)
    {
        success = device->bus.transfer(device->bus.context,
                                       NULL,
                                       data,
                                       length,
                                       SPI_NOR_TRANSFER_TIMEOUT_MS) == 0;
    }
    device->bus.chip_select(device->bus.context, false);
    return success;
}

bool spi_nor_page_program(spi_nor_t *device,
                          uint32_t address,
                          const uint8_t *data,
                          size_t length,
                          uint32_t timeout_ms)
{
    uint8_t header[4];
    bool success;

    if ((data == NULL) || !spi_nor_address_range_valid(device, address, length) ||
        (length > SPI_NOR_PAGE_SIZE_BYTES) ||
        ((address / SPI_NOR_PAGE_SIZE_BYTES) !=
         ((address + (uint32_t)length - 1U) / SPI_NOR_PAGE_SIZE_BYTES)))
    {
        return false;
    }
    if (!spi_nor_wait_ready(device, timeout_ms) || !spi_nor_write_enable(device))
    {
        return false;
    }

    spi_nor_build_address_command(header, SPI_NOR_CMD_PAGE_PROGRAM, address);
    device->bus.chip_select(device->bus.context, true);
    success = device->bus.transfer(device->bus.context,
                                   header,
                                   NULL,
                                   sizeof(header),
                                   SPI_NOR_TRANSFER_TIMEOUT_MS) == 0;
    if (success)
    {
        success = device->bus.transfer(device->bus.context,
                                       data,
                                       NULL,
                                       length,
                                       SPI_NOR_TRANSFER_TIMEOUT_MS) == 0;
    }
    device->bus.chip_select(device->bus.context, false);

    return success && spi_nor_wait_ready(device, timeout_ms);
}

bool spi_nor_write(spi_nor_t *device,
                   uint32_t address,
                   const uint8_t *data,
                   size_t length,
                   uint32_t page_timeout_ms)
{
    size_t bytes_remaining = length;
    size_t data_offset = 0U;
    uint32_t current_address = address;

    if ((data == NULL) || !spi_nor_address_range_valid(device, address, length))
    {
        return false;
    }

    while (bytes_remaining != 0U)
    {
        const size_t page_offset = current_address % SPI_NOR_PAGE_SIZE_BYTES;
        const size_t page_space = SPI_NOR_PAGE_SIZE_BYTES - page_offset;
        const size_t chunk_length = (bytes_remaining < page_space)
                                        ? bytes_remaining
                                        : page_space;

        if (!spi_nor_page_program(device,
                                  current_address,
                                  &data[data_offset],
                                  chunk_length,
                                  page_timeout_ms))
        {
            return false;
        }

        current_address += (uint32_t)chunk_length;
        data_offset += chunk_length;
        bytes_remaining -= chunk_length;
    }

    return true;
}

bool spi_nor_sector_erase_4k(spi_nor_t *device,
                             uint32_t address,
                             uint32_t timeout_ms)
{
    uint8_t header[4];
    bool success;

    if (!spi_nor_address_range_valid(device, address, SPI_NOR_SECTOR_SIZE_BYTES) ||
        ((address % SPI_NOR_SECTOR_SIZE_BYTES) != 0U))
    {
        return false;
    }
    if (!spi_nor_wait_ready(device, timeout_ms) || !spi_nor_write_enable(device))
    {
        return false;
    }

    spi_nor_build_address_command(header, SPI_NOR_CMD_SECTOR_ERASE_4K, address);
    device->bus.chip_select(device->bus.context, true);
    success = device->bus.transfer(device->bus.context,
                                   header,
                                   NULL,
                                   sizeof(header),
                                   SPI_NOR_TRANSFER_TIMEOUT_MS) == 0;
    device->bus.chip_select(device->bus.context, false);

    return success && spi_nor_wait_ready(device, timeout_ms);
}

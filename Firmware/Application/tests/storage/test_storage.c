#include "storage.h"
#include "storage_partition.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct fake_storage
{
    uint8_t bytes[64];
    unsigned int open_count;
    unsigned int read_count;
    unsigned int write_count;
    unsigned int erase_count;
    unsigned int sync_count;
    unsigned int close_count;
};

static struct fake_storage *fake_context(struct storage_device *device)
{
    return (struct fake_storage *)device->private_data;
}

static int fake_open(struct storage_device *device)
{
    fake_context(device)->open_count++;
    return STORAGE_OK;
}

static int fake_read(struct storage_device *device,
                     uint32_t offset,
                     void *buffer,
                     size_t length)
{
    struct fake_storage *fake = fake_context(device);

    fake->read_count++;
    memcpy(buffer, &fake->bytes[offset], length);
    return STORAGE_OK;
}

static int fake_write(struct storage_device *device,
                      uint32_t offset,
                      const void *buffer,
                      size_t length)
{
    struct fake_storage *fake = fake_context(device);

    fake->write_count++;
    memcpy(&fake->bytes[offset], buffer, length);
    return STORAGE_OK;
}

static int fake_erase(struct storage_device *device,
                      uint32_t offset,
                      size_t length)
{
    struct fake_storage *fake = fake_context(device);

    fake->erase_count++;
    memset(&fake->bytes[offset], device->info.erased_value, length);
    return STORAGE_OK;
}

static int fake_sync(struct storage_device *device)
{
    fake_context(device)->sync_count++;
    return STORAGE_OK;
}

static int fake_close(struct storage_device *device)
{
    fake_context(device)->close_count++;
    return STORAGE_OK;
}

static const struct storage_ops fake_ops = {
    .open = fake_open,
    .read = fake_read,
    .write = fake_write,
    .erase = fake_erase,
    .sync = fake_sync,
    .close = fake_close,
};

static void test_lifecycle_and_io(void)
{
    const struct storage_info definition = {
        .capacity_bytes = 64U,
        .program_page_size_bytes = 8U,
        .erase_size_bytes = 16U,
        .capabilities = STORAGE_CAP_READ | STORAGE_CAP_WRITE |
                        STORAGE_CAP_ERASE |
                        STORAGE_CAP_WRITE_REQUIRES_ERASE,
        .erased_value = 0xFFU,
    };
    struct fake_storage fake = {0};
    struct storage_device device;
    struct storage_info info;
    const uint8_t write_data[] = {1U, 2U, 3U, 4U};
    uint8_t read_data[sizeof(write_data)] = {0};

    assert(storage_device_init(&device,
                               "fake0",
                               &fake_ops,
                               &fake,
                               &definition) == STORAGE_OK);
    assert(storage_read(&device, 0U, read_data, sizeof(read_data)) ==
           STORAGE_ERR_STATE);
    assert(storage_open(&device) == STORAGE_OK);
    assert(storage_open(&device) == STORAGE_OK);
    assert(fake.open_count == 1U);
    assert(storage_get_info(&device, &info) == STORAGE_OK);
    assert(info.capacity_bytes == 64U);

    assert(storage_write(&device, 4U, write_data, sizeof(write_data)) ==
           STORAGE_OK);
    assert(storage_read(&device, 4U, read_data, sizeof(read_data)) ==
           STORAGE_OK);
    assert(memcmp(write_data, read_data, sizeof(write_data)) == 0);
    assert(storage_sync(&device) == STORAGE_OK);

    assert(storage_erase(&device, 0U, 16U) == STORAGE_OK);
    assert(fake.bytes[4] == 0xFFU);
    assert(storage_erase(&device, 1U, 16U) == STORAGE_ERR_INVALID);
    assert(storage_erase(&device, 0U, 15U) == STORAGE_ERR_INVALID);

    assert(storage_read(&device, 63U, read_data, 2U) == STORAGE_ERR_RANGE);
    assert(storage_write(&device, 64U, write_data, 1U) == STORAGE_ERR_RANGE);
    assert(fake.read_count == 1U);
    assert(fake.write_count == 1U);
    assert(fake.erase_count == 1U);
    assert(fake.sync_count == 1U);

    assert(storage_close(&device) == STORAGE_OK);
    assert(fake.close_count == 1U);
    assert(storage_close(&device) == STORAGE_ERR_STATE);
}

static void test_capability_rejection(void)
{
    const struct storage_info definition = {
        .capacity_bytes = 64U,
        .program_page_size_bytes = 1U,
        .erase_size_bytes = 0U,
        .capabilities = STORAGE_CAP_READ,
        .erased_value = 0xFFU,
    };
    const struct storage_ops read_only_ops = {
        .open = fake_open,
        .read = fake_read,
        .write = NULL,
        .erase = NULL,
        .sync = NULL,
        .close = NULL,
    };
    struct fake_storage fake = {0};
    struct storage_device device;
    uint8_t byte = 0U;

    assert(storage_device_init(&device,
                               "read_only",
                               &read_only_ops,
                               &fake,
                               &definition) == STORAGE_OK);
    assert(storage_open(&device) == STORAGE_OK);
    assert(storage_write(&device, 0U, &byte, 1U) ==
           STORAGE_ERR_UNSUPPORTED);
    assert(storage_erase(&device, 0U, 1U) == STORAGE_ERR_UNSUPPORTED);
    assert(storage_sync(&device) == STORAGE_OK);
}

static void test_partition_translation_and_isolation(void)
{
    const struct storage_info definition = {
        .capacity_bytes = 64U,
        .program_page_size_bytes = 8U,
        .erase_size_bytes = 16U,
        .capabilities = STORAGE_CAP_READ | STORAGE_CAP_WRITE |
                        STORAGE_CAP_ERASE |
                        STORAGE_CAP_WRITE_REQUIRES_ERASE,
        .erased_value = 0xFFU,
    };
    struct fake_storage fake = {0};
    struct storage_device parent;
    struct storage_device view;
    struct storage_device invalid_view;
    struct storage_partition partition;
    struct storage_partition invalid_partition;
    const uint8_t write_data[] = {0x12U, 0x34U};
    uint8_t read_data[sizeof(write_data)] = {0};

    assert(storage_device_init(&parent,
                               "parent",
                               &fake_ops,
                               &fake,
                               &definition) == STORAGE_OK);
    assert(storage_partition_bind(&view,
                                  "partition0",
                                  &partition,
                                  &parent,
                                  16U,
                                  32U) == STORAGE_OK);
    assert(storage_open(&view) == STORAGE_OK);
    assert(parent.is_open);
    assert(fake.open_count == 1U);

    assert(storage_write(&view, 2U, write_data, sizeof(write_data)) ==
           STORAGE_OK);
    assert(fake.bytes[18] == write_data[0]);
    assert(fake.bytes[19] == write_data[1]);
    assert(storage_read(&view, 2U, read_data, sizeof(read_data)) ==
           STORAGE_OK);
    assert(memcmp(write_data, read_data, sizeof(write_data)) == 0);

    assert(storage_write(&view, 31U, write_data, sizeof(write_data)) ==
           STORAGE_ERR_RANGE);
    assert(fake.write_count == 1U);
    assert(storage_erase(&view, 0U, 16U) == STORAGE_OK);
    assert(fake.bytes[16] == 0xFFU);
    assert(storage_close(&view) == STORAGE_OK);
    assert(parent.is_open);
    assert(fake.close_count == 0U);

    assert(storage_partition_bind(&invalid_view,
                                  "misaligned",
                                  &invalid_partition,
                                  &parent,
                                  1U,
                                  16U) == STORAGE_ERR_INVALID);
    assert(storage_partition_bind(&invalid_view,
                                  "outside",
                                  &invalid_partition,
                                  &parent,
                                  48U,
                                  32U) == STORAGE_ERR_INVALID);
}

int main(void)
{
    test_lifecycle_and_io();
    test_capability_rejection();
    test_partition_translation_and_isolation();
    puts("storage interface tests: PASS");
    return 0;
}

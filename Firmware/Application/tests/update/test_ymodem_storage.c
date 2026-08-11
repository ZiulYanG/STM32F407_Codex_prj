#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "storage.h"
#include "ymodem_storage.h"

#define FAKE_CAPACITY   8192U
#define FAKE_ERASE_SIZE 4096U

struct fake_storage
{
    uint8_t bytes[FAKE_CAPACITY];
    uint32_t erase_count;
    uint32_t write_count;
    uint32_t sync_count;
};

static int fake_open(struct storage_device *device)
{
    (void)device;
    return STORAGE_OK;
}

static int fake_read(struct storage_device *device,
                     uint32_t offset,
                     void *buffer,
                     size_t length)
{
    struct fake_storage *fake = device->private_data;

    memcpy(buffer, &fake->bytes[offset], length);
    return STORAGE_OK;
}

static int fake_write(struct storage_device *device,
                      uint32_t offset,
                      const void *buffer,
                      size_t length)
{
    struct fake_storage *fake = device->private_data;

    memcpy(&fake->bytes[offset], buffer, length);
    ++fake->write_count;
    return STORAGE_OK;
}

static int fake_erase(struct storage_device *device,
                      uint32_t offset,
                      size_t length)
{
    struct fake_storage *fake = device->private_data;

    memset(&fake->bytes[offset], 0xFF, length);
    ++fake->erase_count;
    return STORAGE_OK;
}

static int fake_sync(struct storage_device *device)
{
    struct fake_storage *fake = device->private_data;

    ++fake->sync_count;
    return STORAGE_OK;
}

static int fake_close(struct storage_device *device)
{
    (void)device;
    return STORAGE_OK;
}

static void bind_fake(struct storage_device *device,
                      struct fake_storage *fake)
{
    static const struct storage_ops operations = {
        .open = fake_open,
        .read = fake_read,
        .write = fake_write,
        .erase = fake_erase,
        .sync = fake_sync,
        .close = fake_close,
    };
    const struct storage_info info = {
        .capacity_bytes = FAKE_CAPACITY,
        .program_page_size_bytes = 256U,
        .erase_size_bytes = FAKE_ERASE_SIZE,
        .capabilities = STORAGE_CAP_READ | STORAGE_CAP_WRITE |
                        STORAGE_CAP_ERASE |
                        STORAGE_CAP_WRITE_REQUIRES_ERASE,
        .erased_value = 0xFFU,
    };

    memset(fake, 0, sizeof(*fake));
    memset(fake->bytes, 0x00, sizeof(fake->bytes));
    assert(storage_device_init(device,
                               "fake",
                               &operations,
                               fake,
                               &info) == STORAGE_OK);
    assert(storage_open(device) == STORAGE_OK);
}

static void fill_pattern(uint8_t *data, size_t length, uint8_t seed)
{
    size_t index;

    for (index = 0U; index < length; ++index)
    {
        data[index] = (uint8_t)(seed + (uint8_t)index);
    }
}

static void test_sink_erases_on_4k_boundary(void)
{
    struct storage_device device;
    struct fake_storage fake;
    struct ymodem_storage_sink sink;
    ymodem_file_sink_t interface;
    const ymodem_file_info_t file = {.name = "font.bin", .size = 5000U};
    uint8_t block[1024];

    bind_fake(&device, &fake);
    assert(ymodem_storage_sink_init(&sink, &device));
    interface = ymodem_storage_sink_interface(&sink);
    assert(interface.begin(interface.context, &file) == 0);

    fill_pattern(block, sizeof(block), 0x10U);
    assert(interface.write(interface.context, 0U, block, 1024U) == 0);
    assert(fake.erase_count == 1U);
    assert(interface.write(interface.context, 1024U, block, 1024U) == 0);
    assert(interface.write(interface.context, 2048U, block, 1024U) == 0);
    assert(interface.write(interface.context, 3072U, block, 1024U) == 0);
    assert(fake.erase_count == 1U);

    assert(interface.write(interface.context, 4096U, block, 904U) == 0);
    assert(fake.erase_count == 2U);
    assert(fake.write_count == 5U);
    assert(interface.finish(interface.context) == 0);
    assert(fake.sync_count == 1U);
    assert(memcmp(fake.bytes, block, sizeof(block)) == 0);
}

static void test_sink_rejects_out_of_order_data(void)
{
    struct storage_device device;
    struct fake_storage fake;
    struct ymodem_storage_sink sink;
    ymodem_file_sink_t interface;
    const ymodem_file_info_t file = {.name = "firmware.bin", .size = 1024U};
    uint8_t block[128] = {0};

    bind_fake(&device, &fake);
    assert(ymodem_storage_sink_init(&sink, &device));
    interface = ymodem_storage_sink_interface(&sink);
    assert(interface.begin(interface.context, &file) == 0);
    assert(interface.write(interface.context, 128U, block, sizeof(block)) != 0);
    interface.abort(interface.context);
    assert(!sink.active);
}

static void test_source_reads_selected_range(void)
{
    struct storage_device device;
    struct fake_storage fake;
    struct ymodem_storage_source source;
    ymodem_file_source_t interface;
    ymodem_file_info_t file = {0};
    uint8_t result[128];
    size_t read_length = 0U;

    bind_fake(&device, &fake);
    fill_pattern(fake.bytes, sizeof(fake.bytes), 0x42U);
    assert(ymodem_storage_source_init(&source,
                                      &device,
                                      "event.log",
                                      512U));
    interface = ymodem_storage_source_interface(&source);
    assert(interface.open(interface.context, &file) == 0);
    assert(strcmp(file.name, "event.log") == 0);
    assert(file.size == 512U);
    assert(interface.read(interface.context,
                          128U,
                          result,
                          sizeof(result),
                          &read_length) == 0);
    assert(read_length == sizeof(result));
    assert(memcmp(result, &fake.bytes[128], sizeof(result)) == 0);
    interface.close(interface.context);
    assert(!source.active);
}

int main(void)
{
    test_sink_erases_on_4k_boundary();
    test_sink_rejects_out_of_order_data();
    test_source_reads_selected_range();
    puts("YMODEM storage adapter host tests: PASS");
    return 0;
}

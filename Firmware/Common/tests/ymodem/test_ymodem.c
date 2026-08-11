#include "ymodem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FILE_SIZE 2500U
#define PIPE_SIZE      16384U

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                    \
                    __FILE__, __LINE__, #condition);                            \
            return false;                                                       \
        }                                                                       \
    } while (0)

typedef struct
{
    uint8_t data[PIPE_SIZE];
    size_t read_position;
    size_t write_position;
    unsigned int ack_count;
    unsigned int drop_ack_number;
    bool corrupt_first_data;
    bool corruption_done;
} test_pipe_t;

typedef struct
{
    uint8_t data[TEST_FILE_SIZE];
    unsigned int open_count;
    unsigned int close_count;
} test_source_t;

typedef struct
{
    uint8_t data[TEST_FILE_SIZE];
    uint32_t expected_offset;
    unsigned int begin_count;
    unsigned int write_count;
    unsigned int finish_count;
    unsigned int abort_count;
} test_sink_t;

static int pipe_send(void *context, const uint8_t *data, size_t length)
{
    test_pipe_t *pipe = context;
    size_t index;

    if ((length == 1U) && (data[0] == 0x06U))
    {
        ++pipe->ack_count;
        if (pipe->ack_count == pipe->drop_ack_number)
        {
            return 0;
        }
    }
    if ((pipe->write_position + length) > sizeof(pipe->data))
    {
        return -1;
    }

    memcpy(&pipe->data[pipe->write_position], data, length);
    if (pipe->corrupt_first_data && !pipe->corruption_done &&
        (length == YMODEM_MAX_FRAME_SIZE) && (data[0] == 0x02U))
    {
        index = pipe->write_position + 16U;
        pipe->data[index] ^= 0x01U;
        pipe->corruption_done = true;
    }
    pipe->write_position += length;
    return 0;
}

static int source_open(void *context, ymodem_file_info_t *file)
{
    test_source_t *source = context;

    ++source->open_count;
    (void)snprintf(file->name, sizeof(file->name), "font.bin");
    file->size = sizeof(source->data);
    return 0;
}

static int source_read(void *context,
                       uint32_t offset,
                       uint8_t *data,
                       size_t capacity,
                       size_t *read_length)
{
    test_source_t *source = context;

    if ((offset > sizeof(source->data)) ||
        (capacity > (sizeof(source->data) - offset)))
    {
        return -1;
    }
    memcpy(data, &source->data[offset], capacity);
    *read_length = capacity;
    return 0;
}

static void source_close(void *context)
{
    test_source_t *source = context;

    ++source->close_count;
}

static int sink_begin(void *context, const ymodem_file_info_t *file)
{
    test_sink_t *sink = context;

    ++sink->begin_count;
    sink->expected_offset = 0U;
    return ((strcmp(file->name, "font.bin") == 0) &&
            (file->size == sizeof(sink->data)))
               ? 0
               : -1;
}

static int sink_write(void *context,
                      uint32_t offset,
                      const uint8_t *data,
                      size_t length)
{
    test_sink_t *sink = context;

    if ((offset != sink->expected_offset) ||
        (length > (sizeof(sink->data) - offset)))
    {
        return -1;
    }
    memcpy(&sink->data[offset], data, length);
    sink->expected_offset += (uint32_t)length;
    ++sink->write_count;
    return 0;
}

static int sink_finish(void *context)
{
    test_sink_t *sink = context;

    ++sink->finish_count;
    return (sink->expected_offset == sizeof(sink->data)) ? 0 : -1;
}

static void sink_abort(void *context)
{
    test_sink_t *sink = context;

    ++sink->abort_count;
}

static bool pipe_deliver_to_tx(test_pipe_t *pipe,
                               ymodem_tx_t *sender,
                               uint32_t now_ms)
{
    if (pipe->read_position >= pipe->write_position)
    {
        return false;
    }
    (void)ymodem_tx_feed(sender,
                         &pipe->data[pipe->read_position],
                         1U,
                         now_ms);
    ++pipe->read_position;
    return true;
}

static bool pipe_deliver_to_rx(test_pipe_t *pipe,
                               ymodem_rx_t *receiver,
                               uint32_t now_ms)
{
    size_t available;
    size_t chunk;

    if (pipe->read_position >= pipe->write_position)
    {
        return false;
    }
    available = pipe->write_position - pipe->read_position;
    chunk = (available > 37U) ? 37U : available;
    (void)ymodem_rx_feed(receiver,
                         &pipe->data[pipe->read_position],
                         chunk,
                         now_ms);
    pipe->read_position += chunk;
    return true;
}

static bool run_transfer(bool corrupt_data, unsigned int drop_ack_number)
{
    ymodem_rx_t receiver;
    ymodem_tx_t sender;
    test_pipe_t receiver_to_sender = {0};
    test_pipe_t sender_to_receiver = {0};
    test_source_t source = {0};
    test_sink_t sink = {0};
    ymodem_config_t sender_config = {
        .timeout_ms = 100U,
        .maximum_file_size = 4096U,
        .maximum_retries = 10U,
    };
    ymodem_config_t receiver_config = {
        .timeout_ms = 300U,
        .maximum_file_size = 4096U,
        .maximum_retries = 10U,
    };
    ymodem_transport_t receiver_transport = {
        .context = &receiver_to_sender,
        .send = pipe_send,
    };
    ymodem_transport_t sender_transport = {
        .context = &sender_to_receiver,
        .send = pipe_send,
    };
    ymodem_file_source_t file_source = {
        .context = &source,
        .open = source_open,
        .read = source_read,
        .close = source_close,
    };
    ymodem_file_sink_t file_sink = {
        .context = &sink,
        .begin = sink_begin,
        .write = sink_write,
        .finish = sink_finish,
        .abort = sink_abort,
    };
    uint32_t now_ms = 0U;
    size_t index;
    unsigned int iterations;

    for (index = 0U; index < sizeof(source.data); ++index)
    {
        source.data[index] = (uint8_t)((index * 17U) ^ (index >> 3U));
    }
    sender_to_receiver.corrupt_first_data = corrupt_data;
    receiver_to_sender.drop_ack_number = drop_ack_number;

    CHECK(ymodem_tx_init(&sender,
                         &sender_config,
                         &sender_transport,
                         &file_source));
    CHECK(ymodem_rx_init(&receiver,
                         &receiver_config,
                         &receiver_transport,
                         &file_sink));
    CHECK(ymodem_tx_start(&sender, now_ms) == YMODEM_STATUS_ACTIVE);
    CHECK(ymodem_rx_start(&receiver, now_ms) == YMODEM_STATUS_ACTIVE);

    for (iterations = 0U; iterations < 200000U; ++iterations)
    {
        bool progressed = false;

        progressed |= pipe_deliver_to_tx(&receiver_to_sender,
                                         &sender,
                                         now_ms);
        progressed |= pipe_deliver_to_rx(&sender_to_receiver,
                                         &receiver,
                                         now_ms);
        if (!progressed)
        {
            now_ms += sender_config.timeout_ms;
            (void)ymodem_tx_poll(&sender, now_ms);
            (void)ymodem_rx_poll(&receiver, now_ms);
        }
        if ((sender.status == YMODEM_STATUS_COMPLETE) &&
            (receiver.status == YMODEM_STATUS_COMPLETE))
        {
            break;
        }
        if ((sender.status == YMODEM_STATUS_ERROR) ||
            (receiver.status == YMODEM_STATUS_ERROR))
        {
            fprintf(stderr,
                    "transfer failed: corrupt=%u drop_ack=%u tx_state=%u "
                    "tx_error=%u rx_state=%u rx_error=%u\n",
                    corrupt_data ? 1U : 0U,
                    drop_ack_number,
                    sender.state,
                    sender.error,
                    receiver.state,
                    receiver.error);
            return false;
        }
    }

    CHECK(sender.status == YMODEM_STATUS_COMPLETE);
    CHECK(receiver.status == YMODEM_STATUS_COMPLETE);
    CHECK(source.open_count == 1U);
    CHECK(source.close_count == 1U);
    CHECK(sink.begin_count == 1U);
    CHECK(sink.finish_count == 1U);
    CHECK(sink.abort_count == 0U);
    CHECK(sink.write_count == 3U);
    CHECK(memcmp(source.data, sink.data, sizeof(source.data)) == 0);
    return true;
}

static bool test_crc(void)
{
    static const uint8_t input[] = "123456789";

    CHECK(ymodem_crc16(input, sizeof(input) - 1U) == 0x31C3U);
    return true;
}

static bool test_cancel(void)
{
    ymodem_tx_t sender;
    test_pipe_t pipe = {0};
    test_source_t source = {0};
    ymodem_config_t config = {100U, 4096U, 3U};
    ymodem_transport_t transport = {&pipe, pipe_send};
    ymodem_file_source_t file_source = {
        &source, source_open, source_read, source_close
    };
    const uint8_t cancel[] = {0x18U, 0x18U};

    CHECK(ymodem_tx_init(&sender, &config, &transport, &file_source));
    CHECK(ymodem_tx_start(&sender, 0U) == YMODEM_STATUS_ACTIVE);
    CHECK(ymodem_tx_feed(&sender, cancel, sizeof(cancel), 1U) ==
          YMODEM_STATUS_CANCELLED);
    CHECK(source.close_count == 1U);
    return true;
}

int main(void)
{
    if (!test_crc() ||
        !run_transfer(false, 0U) ||
        !run_transfer(true, 0U) ||
        !run_transfer(false, 1U) ||
        !run_transfer(false, 2U) ||
        !run_transfer(false, 5U) ||
        !run_transfer(false, 6U) ||
        !test_cancel())
    {
        return EXIT_FAILURE;
    }

    puts("YMODEM RX/TX host tests: PASS");
    return EXIT_SUCCESS;
}

#include "ymodem.h"

#include <stdio.h>
#include <string.h>

#define YMODEM_SOH 0x01U
#define YMODEM_STX 0x02U
#define YMODEM_EOT 0x04U
#define YMODEM_ACK 0x06U
#define YMODEM_NAK 0x15U
#define YMODEM_CAN 0x18U
#define YMODEM_CRC_REQUEST 0x43U
#define YMODEM_PADDING 0x1AU

enum ymodem_rx_state
{
    YMODEM_RX_WAIT_HEADER = 0,
    YMODEM_RX_WAIT_DATA,
    YMODEM_RX_WAIT_SECOND_EOT,
    YMODEM_RX_WAIT_END_HEADER,
    YMODEM_RX_LINGER
};

enum ymodem_tx_state
{
    YMODEM_TX_WAIT_HEADER_REQUEST = 0,
    YMODEM_TX_WAIT_HEADER_ACK,
    YMODEM_TX_WAIT_DATA_REQUEST,
    YMODEM_TX_WAIT_DATA_ACK,
    YMODEM_TX_WAIT_FIRST_EOT_RESPONSE,
    YMODEM_TX_WAIT_SECOND_EOT_ACK,
    YMODEM_TX_WAIT_END_HEADER_REQUEST,
    YMODEM_TX_WAIT_END_HEADER_ACK
};

static bool ymodem_config_valid(const ymodem_config_t *config)
{
    return (config != NULL) && (config->timeout_ms > 0U) &&
           (config->maximum_file_size > 0U) &&
           (config->maximum_retries > 0U);
}

uint16_t ymodem_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0U;
    size_t index;
    uint8_t bit;

    if ((data == NULL) && (length > 0U))
    {
        return 0U;
    }

    for (index = 0U; index < length; ++index)
    {
        crc ^= (uint16_t)data[index] << 8U;
        for (bit = 0U; bit < 8U; ++bit)
        {
            crc = ((crc & 0x8000U) != 0U)
                      ? (uint16_t)((crc << 1U) ^ 0x1021U)
                      : (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

static int ymodem_send(const ymodem_transport_t *transport,
                       const uint8_t *data,
                       size_t length)
{
    return transport->send(transport->context, data, length);
}

static int ymodem_send_byte(const ymodem_transport_t *transport, uint8_t value)
{
    return ymodem_send(transport, &value, 1U);
}

static void ymodem_rx_fail(ymodem_rx_t *receiver, ymodem_error_t error)
{
    static const uint8_t cancel[] = {YMODEM_CAN, YMODEM_CAN};

    if (receiver->sink_open && (receiver->sink.abort != NULL))
    {
        receiver->sink.abort(receiver->sink.context);
    }
    receiver->sink_open = false;
    receiver->error = error;
    receiver->status = YMODEM_STATUS_ERROR;
    (void)ymodem_send(&receiver->transport, cancel, sizeof(cancel));
}

static bool ymodem_rx_retry(ymodem_rx_t *receiver, uint8_t response)
{
    ++receiver->retry_count;
    if (receiver->retry_count > receiver->config.maximum_retries)
    {
        ymodem_rx_fail(receiver, YMODEM_ERROR_RETRY_LIMIT);
        return false;
    }
    if (ymodem_send_byte(&receiver->transport, response) != 0)
    {
        ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
        return false;
    }
    return true;
}

static bool ymodem_parse_file_info(const uint8_t *payload,
                                   size_t payload_size,
                                   ymodem_file_info_t *file)
{
    size_t name_length = 0U;
    size_t position;
    uint32_t file_size = 0U;
    bool has_digit = false;

    while ((name_length < payload_size) &&
           (payload[name_length] != 0U))
    {
        ++name_length;
    }
    if ((name_length == 0U) || (name_length >= payload_size) ||
        (name_length >= sizeof(file->name)))
    {
        return false;
    }

    memcpy(file->name, payload, name_length);
    file->name[name_length] = '\0';
    position = name_length + 1U;
    while ((position < payload_size) && (payload[position] == ' '))
    {
        ++position;
    }
    while ((position < payload_size) &&
           (payload[position] >= '0') && (payload[position] <= '9'))
    {
        uint32_t digit = (uint32_t)(payload[position] - '0');

        if (file_size > ((UINT32_MAX - digit) / 10U))
        {
            return false;
        }
        file_size = (file_size * 10U) + digit;
        has_digit = true;
        ++position;
    }
    if (!has_digit)
    {
        return false;
    }
    file->size = file_size;
    return true;
}

static void ymodem_rx_handle_header(ymodem_rx_t *receiver,
                                    const uint8_t *payload,
                                    size_t payload_size)
{
    if (payload[0] == 0U)
    {
        if ((receiver->state != YMODEM_RX_WAIT_END_HEADER) &&
            (receiver->state != YMODEM_RX_LINGER))
        {
            (void)ymodem_rx_retry(receiver, YMODEM_NAK);
            return;
        }
        if (ymodem_send_byte(&receiver->transport, YMODEM_ACK) != 0)
        {
            ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
            return;
        }
        receiver->state = YMODEM_RX_LINGER;
        receiver->retry_count = 0U;
        return;
    }

    /* The header ACK or following 'C' may be lost.  A retransmitted block 0
       must not reopen or erase the file sink. */
    if ((receiver->state == YMODEM_RX_WAIT_DATA) && receiver->sink_open)
    {
        if ((ymodem_send_byte(&receiver->transport, YMODEM_ACK) != 0) ||
            (ymodem_send_byte(&receiver->transport, YMODEM_CRC_REQUEST) != 0))
        {
            ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
        }
        return;
    }

    if ((receiver->state != YMODEM_RX_WAIT_HEADER) ||
        !ymodem_parse_file_info(payload, payload_size, &receiver->file) ||
        (receiver->file.size > receiver->config.maximum_file_size))
    {
        ymodem_rx_fail(receiver, YMODEM_ERROR_FILE_SIZE);
        return;
    }
    if (receiver->sink.begin(receiver->sink.context, &receiver->file) != 0)
    {
        ymodem_rx_fail(receiver, YMODEM_ERROR_FILE_REJECTED);
        return;
    }

    receiver->sink_open = true;
    receiver->received_size = 0U;
    receiver->expected_block = 1U;
    receiver->state = YMODEM_RX_WAIT_DATA;
    receiver->retry_count = 0U;
    if ((ymodem_send_byte(&receiver->transport, YMODEM_ACK) != 0) ||
        (ymodem_send_byte(&receiver->transport, YMODEM_CRC_REQUEST) != 0))
    {
        ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
    }
}

static void ymodem_rx_handle_data(ymodem_rx_t *receiver,
                                  uint8_t block_number,
                                  const uint8_t *payload,
                                  size_t payload_size)
{
    uint8_t previous_block = (uint8_t)(receiver->expected_block - 1U);
    uint32_t remaining;
    size_t write_length;

    if (receiver->state != YMODEM_RX_WAIT_DATA)
    {
        (void)ymodem_rx_retry(receiver, YMODEM_NAK);
        return;
    }
    if (block_number == previous_block)
    {
        if (ymodem_send_byte(&receiver->transport, YMODEM_ACK) != 0)
        {
            ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
        }
        return;
    }
    if (block_number != receiver->expected_block)
    {
        (void)ymodem_rx_retry(receiver, YMODEM_NAK);
        return;
    }
    if (receiver->received_size >= receiver->file.size)
    {
        ymodem_rx_fail(receiver, YMODEM_ERROR_PROTOCOL);
        return;
    }

    remaining = receiver->file.size - receiver->received_size;
    write_length = (remaining < payload_size) ? (size_t)remaining : payload_size;
    if (receiver->sink.write(receiver->sink.context,
                             receiver->received_size,
                             payload,
                             write_length) != 0)
    {
        ymodem_rx_fail(receiver, YMODEM_ERROR_FILE_WRITE);
        return;
    }

    receiver->received_size += (uint32_t)write_length;
    ++receiver->expected_block;
    receiver->retry_count = 0U;
    if (ymodem_send_byte(&receiver->transport, YMODEM_ACK) != 0)
    {
        ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
    }
}

static void ymodem_rx_process_frame(ymodem_rx_t *receiver)
{
    uint8_t block_number = receiver->frame[0];
    uint8_t block_inverse = receiver->frame[1];
    const uint8_t *payload = &receiver->frame[2];
    uint16_t received_crc =
        (uint16_t)((uint16_t)receiver->frame[2U + receiver->packet_size] << 8U) |
        receiver->frame[3U + receiver->packet_size];

    if (((uint8_t)(block_number + block_inverse) != 0xFFU) ||
        (ymodem_crc16(payload, receiver->packet_size) != received_crc))
    {
        (void)ymodem_rx_retry(receiver, YMODEM_NAK);
        return;
    }

    if (block_number == 0U)
    {
        ymodem_rx_handle_header(receiver, payload, receiver->packet_size);
    }
    else
    {
        ymodem_rx_handle_data(receiver,
                              block_number,
                              payload,
                              receiver->packet_size);
    }
}

bool ymodem_rx_init(ymodem_rx_t *receiver,
                    const ymodem_config_t *config,
                    const ymodem_transport_t *transport,
                    const ymodem_file_sink_t *sink)
{
    if ((receiver == NULL) || !ymodem_config_valid(config) ||
        (transport == NULL) || (transport->send == NULL) ||
        (sink == NULL) || (sink->begin == NULL) ||
        (sink->write == NULL) || (sink->finish == NULL))
    {
        return false;
    }

    memset(receiver, 0, sizeof(*receiver));
    receiver->config = *config;
    receiver->transport = *transport;
    receiver->sink = *sink;
    return true;
}

ymodem_status_t ymodem_rx_start(ymodem_rx_t *receiver, uint32_t now_ms)
{
    if (receiver == NULL)
    {
        return YMODEM_STATUS_ERROR;
    }

    receiver->status = YMODEM_STATUS_ACTIVE;
    receiver->error = YMODEM_ERROR_NONE;
    receiver->state = YMODEM_RX_WAIT_HEADER;
    receiver->last_activity_ms = now_ms;
    receiver->retry_count = 0U;
    receiver->cancel_count = 0U;
    receiver->frame_active = false;
    if (ymodem_send_byte(&receiver->transport, YMODEM_CRC_REQUEST) != 0)
    {
        ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
    }
    return receiver->status;
}

static void ymodem_rx_handle_control(ymodem_rx_t *receiver, uint8_t value)
{
    if (value == YMODEM_CAN)
    {
        ++receiver->cancel_count;
        if (receiver->cancel_count >= 2U)
        {
            if (receiver->sink_open && (receiver->sink.abort != NULL))
            {
                receiver->sink.abort(receiver->sink.context);
            }
            receiver->sink_open = false;
            receiver->status = YMODEM_STATUS_CANCELLED;
        }
        return;
    }
    receiver->cancel_count = 0U;

    if (value != YMODEM_EOT)
    {
        return;
    }
    if ((receiver->state == YMODEM_RX_WAIT_DATA) &&
        (receiver->received_size == receiver->file.size))
    {
        receiver->state = YMODEM_RX_WAIT_SECOND_EOT;
        receiver->retry_count = 0U;
        if (ymodem_send_byte(&receiver->transport, YMODEM_NAK) != 0)
        {
            ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
        }
    }
    else if (receiver->state == YMODEM_RX_WAIT_SECOND_EOT)
    {
        if (receiver->sink.finish(receiver->sink.context) != 0)
        {
            ymodem_rx_fail(receiver, YMODEM_ERROR_FILE_WRITE);
            return;
        }
        receiver->sink_open = false;
        receiver->state = YMODEM_RX_WAIT_END_HEADER;
        receiver->retry_count = 0U;
        if ((ymodem_send_byte(&receiver->transport, YMODEM_ACK) != 0) ||
            (ymodem_send_byte(&receiver->transport, YMODEM_CRC_REQUEST) != 0))
        {
            ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
        }
    }
    else if (receiver->state == YMODEM_RX_WAIT_END_HEADER)
    {
        /* The ACK after the second EOT may have been lost. */
        if ((ymodem_send_byte(&receiver->transport, YMODEM_ACK) != 0) ||
            (ymodem_send_byte(&receiver->transport, YMODEM_CRC_REQUEST) != 0))
        {
            ymodem_rx_fail(receiver, YMODEM_ERROR_IO);
        }
    }
    else
    {
        (void)ymodem_rx_retry(receiver, YMODEM_NAK);
    }
}

ymodem_status_t ymodem_rx_feed(ymodem_rx_t *receiver,
                               const uint8_t *data,
                               size_t length,
                               uint32_t now_ms)
{
    size_t index;

    if ((receiver == NULL) || ((data == NULL) && (length > 0U)) ||
        (receiver->status != YMODEM_STATUS_ACTIVE))
    {
        return (receiver != NULL) ? receiver->status : YMODEM_STATUS_ERROR;
    }

    for (index = 0U; (index < length) &&
                     (receiver->status == YMODEM_STATUS_ACTIVE); ++index)
    {
        uint8_t value = data[index];

        receiver->last_activity_ms = now_ms;
        if (!receiver->frame_active)
        {
            if ((value == YMODEM_SOH) || (value == YMODEM_STX))
            {
                receiver->packet_size = (value == YMODEM_SOH)
                                            ? YMODEM_DATA_SIZE_128
                                            : YMODEM_DATA_SIZE_1K;
                receiver->frame_expected =
                    (uint16_t)(receiver->packet_size + 4U);
                receiver->frame_received = 0U;
                receiver->frame_active = true;
            }
            else
            {
                ymodem_rx_handle_control(receiver, value);
            }
            continue;
        }

        receiver->frame[receiver->frame_received] = value;
        ++receiver->frame_received;
        if (receiver->frame_received == receiver->frame_expected)
        {
            receiver->frame_active = false;
            ymodem_rx_process_frame(receiver);
        }
    }
    return receiver->status;
}

ymodem_status_t ymodem_rx_poll(ymodem_rx_t *receiver, uint32_t now_ms)
{
    uint8_t response;

    if ((receiver == NULL) ||
        (receiver->status != YMODEM_STATUS_ACTIVE))
    {
        return (receiver != NULL) ? receiver->status : YMODEM_STATUS_ERROR;
    }
    if ((uint32_t)(now_ms - receiver->last_activity_ms) <
        receiver->config.timeout_ms)
    {
        return receiver->status;
    }

    receiver->last_activity_ms = now_ms;
    if (receiver->state == YMODEM_RX_LINGER)
    {
        receiver->status = YMODEM_STATUS_COMPLETE;
        return receiver->status;
    }
    receiver->frame_active = false;
    response = ((receiver->state == YMODEM_RX_WAIT_HEADER) ||
                (receiver->state == YMODEM_RX_WAIT_END_HEADER) ||
                ((receiver->state == YMODEM_RX_WAIT_DATA) &&
                 (receiver->received_size == 0U) &&
                 (receiver->expected_block == 1U)))
                   ? YMODEM_CRC_REQUEST
                   : YMODEM_NAK;
    (void)ymodem_rx_retry(receiver, response);
    return receiver->status;
}

void ymodem_rx_cancel(ymodem_rx_t *receiver)
{
    static const uint8_t cancel[] = {YMODEM_CAN, YMODEM_CAN};

    if ((receiver == NULL) ||
        (receiver->status != YMODEM_STATUS_ACTIVE))
    {
        return;
    }
    if (receiver->sink_open && (receiver->sink.abort != NULL))
    {
        receiver->sink.abort(receiver->sink.context);
    }
    receiver->sink_open = false;
    receiver->status = YMODEM_STATUS_CANCELLED;
    (void)ymodem_send(&receiver->transport, cancel, sizeof(cancel));
}

static void ymodem_tx_fail(ymodem_tx_t *sender, ymodem_error_t error)
{
    static const uint8_t cancel[] = {YMODEM_CAN, YMODEM_CAN};

    if (sender->source_open && (sender->source.close != NULL))
    {
        sender->source.close(sender->source.context);
    }
    sender->source_open = false;
    sender->error = error;
    sender->status = YMODEM_STATUS_ERROR;
    (void)ymodem_send(&sender->transport, cancel, sizeof(cancel));
}

static bool ymodem_tx_send_last(ymodem_tx_t *sender)
{
    if ((sender->last_frame_length == 0U) ||
        (ymodem_send(&sender->transport,
                     sender->last_frame,
                     sender->last_frame_length) != 0))
    {
        ymodem_tx_fail(sender, YMODEM_ERROR_IO);
        return false;
    }
    return true;
}

static bool ymodem_tx_build_packet(ymodem_tx_t *sender,
                                   uint8_t start,
                                   uint8_t block_number,
                                   const uint8_t *payload,
                                   size_t payload_size)
{
    uint16_t crc;

    if ((payload == NULL) ||
        ((payload_size != YMODEM_DATA_SIZE_128) &&
         (payload_size != YMODEM_DATA_SIZE_1K)))
    {
        return false;
    }

    sender->last_frame[0] = start;
    sender->last_frame[1] = block_number;
    sender->last_frame[2] = (uint8_t)(0xFFU - block_number);
    memcpy(&sender->last_frame[3], payload, payload_size);
    crc = ymodem_crc16(payload, payload_size);
    sender->last_frame[3U + payload_size] = (uint8_t)(crc >> 8U);
    sender->last_frame[4U + payload_size] = (uint8_t)crc;
    sender->last_frame_length = (uint16_t)(payload_size + 5U);
    return ymodem_tx_send_last(sender);
}

static bool ymodem_tx_send_header(ymodem_tx_t *sender, bool empty)
{
    uint8_t payload[YMODEM_DATA_SIZE_128] = {0};

    if (!empty)
    {
        size_t name_length = strlen(sender->file.name);
        int result;

        if ((name_length == 0U) ||
            (name_length >= YMODEM_MAX_FILE_NAME) ||
            (name_length + 2U >= sizeof(payload)))
        {
            ymodem_tx_fail(sender, YMODEM_ERROR_FILE_READ);
            return false;
        }
        memcpy(payload, sender->file.name, name_length);
        result = snprintf((char *)&payload[name_length + 1U],
                          sizeof(payload) - name_length - 1U,
                          "%lu",
                          (unsigned long)sender->file.size);
        if ((result <= 0) ||
            ((size_t)result >= (sizeof(payload) - name_length - 1U)))
        {
            ymodem_tx_fail(sender, YMODEM_ERROR_FILE_READ);
            return false;
        }
    }
    return ymodem_tx_build_packet(sender,
                                  YMODEM_SOH,
                                  0U,
                                  payload,
                                  sizeof(payload));
}

static bool ymodem_tx_send_data(ymodem_tx_t *sender)
{
    uint8_t payload[YMODEM_DATA_SIZE_1K];
    size_t requested;
    size_t read_length = 0U;

    if (sender->sent_size >= sender->file.size)
    {
        sender->last_frame[0] = YMODEM_EOT;
        sender->last_frame_length = 1U;
        sender->state = YMODEM_TX_WAIT_FIRST_EOT_RESPONSE;
        return ymodem_tx_send_last(sender);
    }

    requested = (size_t)(sender->file.size - sender->sent_size);
    if (requested > sizeof(payload))
    {
        requested = sizeof(payload);
    }
    memset(payload, YMODEM_PADDING, sizeof(payload));
    if ((sender->source.read(sender->source.context,
                             sender->sent_size,
                             payload,
                             requested,
                             &read_length) != 0) ||
        (read_length != requested))
    {
        ymodem_tx_fail(sender, YMODEM_ERROR_FILE_READ);
        return false;
    }

    sender->pending_size = (uint32_t)read_length;
    sender->state = YMODEM_TX_WAIT_DATA_ACK;
    return ymodem_tx_build_packet(sender,
                                  YMODEM_STX,
                                  sender->block_number,
                                  payload,
                                  sizeof(payload));
}

bool ymodem_tx_init(ymodem_tx_t *sender,
                    const ymodem_config_t *config,
                    const ymodem_transport_t *transport,
                    const ymodem_file_source_t *source)
{
    if ((sender == NULL) || !ymodem_config_valid(config) ||
        (transport == NULL) || (transport->send == NULL) ||
        (source == NULL) || (source->open == NULL) ||
        (source->read == NULL))
    {
        return false;
    }

    memset(sender, 0, sizeof(*sender));
    sender->config = *config;
    sender->transport = *transport;
    sender->source = *source;
    return true;
}

ymodem_status_t ymodem_tx_start(ymodem_tx_t *sender, uint32_t now_ms)
{
    if (sender == NULL)
    {
        return YMODEM_STATUS_ERROR;
    }

    sender->status = YMODEM_STATUS_ACTIVE;
    sender->error = YMODEM_ERROR_NONE;
    sender->state = YMODEM_TX_WAIT_HEADER_REQUEST;
    sender->last_activity_ms = now_ms;
    sender->retry_count = 0U;
    sender->cancel_count = 0U;
    sender->block_number = 1U;
    sender->sent_size = 0U;
    sender->last_frame_length = 0U;
    if (sender->source.open(sender->source.context, &sender->file) != 0)
    {
        ymodem_tx_fail(sender, YMODEM_ERROR_FILE_READ);
    }
    else if (sender->file.name[0] == '\0')
    {
        sender->source_open = true;
        ymodem_tx_fail(sender, YMODEM_ERROR_FILE_READ);
    }
    else if (sender->file.size > sender->config.maximum_file_size)
    {
        sender->source_open = true;
        ymodem_tx_fail(sender, YMODEM_ERROR_FILE_SIZE);
    }
    else
    {
        sender->source_open = true;
    }
    return sender->status;
}

static void ymodem_tx_reset_retry(ymodem_tx_t *sender)
{
    sender->retry_count = 0U;
}

static void ymodem_tx_retry(ymodem_tx_t *sender)
{
    ++sender->retry_count;
    if (sender->retry_count > sender->config.maximum_retries)
    {
        ymodem_tx_fail(sender, YMODEM_ERROR_RETRY_LIMIT);
        return;
    }
    (void)ymodem_tx_send_last(sender);
}

static void ymodem_tx_handle_control(ymodem_tx_t *sender, uint8_t value)
{
    if (value == YMODEM_CAN)
    {
        ++sender->cancel_count;
        if (sender->cancel_count >= 2U)
        {
            if (sender->source_open && (sender->source.close != NULL))
            {
                sender->source.close(sender->source.context);
            }
            sender->source_open = false;
            sender->status = YMODEM_STATUS_CANCELLED;
        }
        return;
    }
    sender->cancel_count = 0U;

    switch ((enum ymodem_tx_state)sender->state)
    {
        case YMODEM_TX_WAIT_HEADER_REQUEST:
            if (value == YMODEM_CRC_REQUEST)
            {
                ymodem_tx_reset_retry(sender);
                if (ymodem_tx_send_header(sender, false))
                {
                    sender->state = YMODEM_TX_WAIT_HEADER_ACK;
                }
            }
            break;

        case YMODEM_TX_WAIT_HEADER_ACK:
            if (value == YMODEM_ACK)
            {
                ymodem_tx_reset_retry(sender);
                sender->state = YMODEM_TX_WAIT_DATA_REQUEST;
            }
            else if (value == YMODEM_NAK)
            {
                ymodem_tx_retry(sender);
            }
            break;

        case YMODEM_TX_WAIT_DATA_REQUEST:
            if (value == YMODEM_CRC_REQUEST)
            {
                ymodem_tx_reset_retry(sender);
                (void)ymodem_tx_send_data(sender);
            }
            break;

        case YMODEM_TX_WAIT_DATA_ACK:
            if (value == YMODEM_ACK)
            {
                sender->sent_size += sender->pending_size;
                ++sender->block_number;
                ymodem_tx_reset_retry(sender);
                (void)ymodem_tx_send_data(sender);
            }
            else if (value == YMODEM_NAK)
            {
                ymodem_tx_retry(sender);
            }
            break;

        case YMODEM_TX_WAIT_FIRST_EOT_RESPONSE:
            if (value == YMODEM_NAK)
            {
                sender->last_frame[0] = YMODEM_EOT;
                sender->last_frame_length = 1U;
                sender->state = YMODEM_TX_WAIT_SECOND_EOT_ACK;
                ymodem_tx_reset_retry(sender);
                (void)ymodem_tx_send_last(sender);
            }
            else if (value == YMODEM_ACK)
            {
                sender->state = YMODEM_TX_WAIT_END_HEADER_REQUEST;
                ymodem_tx_reset_retry(sender);
            }
            break;

        case YMODEM_TX_WAIT_SECOND_EOT_ACK:
            if (value == YMODEM_ACK)
            {
                sender->state = YMODEM_TX_WAIT_END_HEADER_REQUEST;
                ymodem_tx_reset_retry(sender);
            }
            else if (value == YMODEM_NAK)
            {
                ymodem_tx_retry(sender);
            }
            break;

        case YMODEM_TX_WAIT_END_HEADER_REQUEST:
            if (value == YMODEM_CRC_REQUEST)
            {
                ymodem_tx_reset_retry(sender);
                if (ymodem_tx_send_header(sender, true))
                {
                    sender->state = YMODEM_TX_WAIT_END_HEADER_ACK;
                }
            }
            break;

        case YMODEM_TX_WAIT_END_HEADER_ACK:
            if (value == YMODEM_ACK)
            {
                if (sender->source_open && (sender->source.close != NULL))
                {
                    sender->source.close(sender->source.context);
                }
                sender->source_open = false;
                sender->status = YMODEM_STATUS_COMPLETE;
                ymodem_tx_reset_retry(sender);
            }
            else if (value == YMODEM_NAK)
            {
                ymodem_tx_retry(sender);
            }
            break;

        default:
            ymodem_tx_fail(sender, YMODEM_ERROR_PROTOCOL);
            break;
    }
}

ymodem_status_t ymodem_tx_feed(ymodem_tx_t *sender,
                               const uint8_t *data,
                               size_t length,
                               uint32_t now_ms)
{
    size_t index;

    if ((sender == NULL) || ((data == NULL) && (length > 0U)) ||
        (sender->status != YMODEM_STATUS_ACTIVE))
    {
        return (sender != NULL) ? sender->status : YMODEM_STATUS_ERROR;
    }

    for (index = 0U; (index < length) &&
                     (sender->status == YMODEM_STATUS_ACTIVE); ++index)
    {
        sender->last_activity_ms = now_ms;
        ymodem_tx_handle_control(sender, data[index]);
    }
    return sender->status;
}

ymodem_status_t ymodem_tx_poll(ymodem_tx_t *sender, uint32_t now_ms)
{
    if ((sender == NULL) ||
        (sender->status != YMODEM_STATUS_ACTIVE))
    {
        return (sender != NULL) ? sender->status : YMODEM_STATUS_ERROR;
    }
    if ((uint32_t)(now_ms - sender->last_activity_ms) <
        sender->config.timeout_ms)
    {
        return sender->status;
    }

    sender->last_activity_ms = now_ms;
    switch ((enum ymodem_tx_state)sender->state)
    {
        case YMODEM_TX_WAIT_HEADER_ACK:
        case YMODEM_TX_WAIT_DATA_ACK:
        case YMODEM_TX_WAIT_FIRST_EOT_RESPONSE:
        case YMODEM_TX_WAIT_SECOND_EOT_ACK:
        case YMODEM_TX_WAIT_END_HEADER_ACK:
            ymodem_tx_retry(sender);
            break;

        case YMODEM_TX_WAIT_HEADER_REQUEST:
        case YMODEM_TX_WAIT_DATA_REQUEST:
        case YMODEM_TX_WAIT_END_HEADER_REQUEST:
            ++sender->retry_count;
            if (sender->retry_count > sender->config.maximum_retries)
            {
                ymodem_tx_fail(sender, YMODEM_ERROR_RETRY_LIMIT);
            }
            break;

        default:
            ymodem_tx_fail(sender, YMODEM_ERROR_PROTOCOL);
            break;
    }
    return sender->status;
}

void ymodem_tx_cancel(ymodem_tx_t *sender)
{
    static const uint8_t cancel[] = {YMODEM_CAN, YMODEM_CAN};

    if ((sender == NULL) ||
        (sender->status != YMODEM_STATUS_ACTIVE))
    {
        return;
    }
    if (sender->source_open && (sender->source.close != NULL))
    {
        sender->source.close(sender->source.context);
    }
    sender->source_open = false;
    sender->status = YMODEM_STATUS_CANCELLED;
    (void)ymodem_send(&sender->transport, cancel, sizeof(cancel));
}

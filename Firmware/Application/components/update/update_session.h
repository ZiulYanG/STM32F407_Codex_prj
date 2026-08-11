#ifndef UPDATE_SESSION_H
#define UPDATE_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "storage.h"
#include "ymodem.h"

typedef enum
{
    UPDATE_SESSION_IDLE = 0,
    UPDATE_SESSION_RECEIVING,
    UPDATE_SESSION_SENDING,
    UPDATE_SESSION_COMPLETE,
    UPDATE_SESSION_CANCELLED,
    UPDATE_SESSION_ERROR
} update_session_state_t;

typedef struct
{
    update_session_state_t state;
    ymodem_error_t error;
    uint32_t transferred_bytes;
    uint32_t file_size;
    uint32_t completed_receive_size;
} update_session_snapshot_t;

bool update_session_init(void);
bool update_session_bind_candidate(struct storage_device *candidate);
bool update_session_request_receive(void);
bool update_session_request_send(uint32_t file_size);
bool update_session_cancel(void);
void update_session_get_snapshot(update_session_snapshot_t *snapshot);
const char *update_session_state_name(update_session_state_t state);
uint32_t update_session_get_stack_high_water_mark_words(void);

#endif /* UPDATE_SESSION_H */

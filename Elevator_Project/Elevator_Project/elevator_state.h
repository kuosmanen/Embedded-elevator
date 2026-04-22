#ifndef ELEVATOR_STATE_H
#define ELEVATOR_STATE_H

#include <stdint.h>
#include "queue_utils.h"

extern uint8_t g_current_floor;
extern uint8_t g_target_floor;
extern uint8_t g_input_digits[2];
extern uint8_t g_input_len;
extern floor_queue_t g_queue;

#endif
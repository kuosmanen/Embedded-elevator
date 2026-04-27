#ifndef ELEVATOR_PROTOCOL_H
#define ELEVATOR_PROTOCOL_H

#include <stdint.h>

/*
 * Simple one-byte command protocol from MEGA (master) to UNO (slave).
 * The UNO only needs to know which output pattern it should currently show.
 */
typedef enum elevator_uno_command {
    UNO_CMD_IDLE = 0,
    UNO_CMD_MOVING = 1,
    UNO_CMD_DOOR_OPEN = 2,
    UNO_CMD_DOOR_CLOSING = 3,
    UNO_CMD_OBSTACLE_START = 4,
    UNO_CMD_OBSTACLE_STOP = 5,
    UNO_CMD_FAULT = 6,
    UNO_CMD_SLEEP = 7
} elevator_uno_command_t;

#define ELEVATOR_TWI_SLAVE_ADDRESS 0x57

#endif

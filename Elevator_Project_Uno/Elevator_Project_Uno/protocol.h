#ifndef ELEVATOR_PROTOCOL_H
#define ELEVATOR_PROTOCOL_H

#include <stdint.h>

/*
 * Simple one-byte command protocol from MEGA (master) to UNO (slave).
 * The UNO interprets these commands as actuator/output modes.
 */
typedef enum elevator_uno_command {
    UNO_CMD_IDLE = 0,
    UNO_CMD_MOVING = 1,
    UNO_CMD_DOOR_OPEN = 2,
    UNO_CMD_DOOR_CLOSING = 3,
    UNO_CMD_OBSTACLE_START = 4,
    UNO_CMD_OBSTACLE_STOP = 5,
    UNO_CMD_FAULT = 6,

    /*
     * System is awake/being used, but no elevator status LED needs to be on.
     * The UNO uses this to start/keep the background melody without starting
     * the idle low-power countdown.
     */
    UNO_CMD_BACKGROUND = 7
} elevator_uno_command_t;

#define ELEVATOR_TWI_SLAVE_ADDRESS 0x57

#endif

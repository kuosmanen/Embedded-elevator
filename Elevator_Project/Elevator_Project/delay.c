#include "delay.h"

/*
 * Delay for a specified number of seconds
 * Because there is no built in function for seconds
 */

void DELAY_sec(uint16_t seconds)
{
    while (seconds != 0u) {
        DELAY_ms(1000);
        seconds--;
    }
}

#include "delay.h"

void DELAY_sec(uint16_t seconds)
{
    while (seconds != 0u) {
        DELAY_ms(1000);
        seconds--;
    }
}

#ifndef _DELAY_H
#define _DELAY_H

// Clock frequency for Mega and Uno is 16MHz
#define F_CPU 16000000UL

#include <util/delay.h>
#include "stdutils.h"

// For consistent naming style to course provided utility files
#define DELAY_us(x) _delay_us(x)
#define DELAY_ms(x) _delay_ms(x)

// Additional delay function for delay in seconds
// Works by calling DELAY_ms(1000) in a loop as many times as needed
void DELAY_sec(uint16_t seconds);

#endif

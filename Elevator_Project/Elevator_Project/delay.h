#ifndef _DELAY_H
#define _DELAY_H

#define F_CPU 16000000UL

#include <util/delay.h>
#include "stdutils.h"

#define DELAY_us(x) _delay_us(x)
#define DELAY_ms(x) _delay_ms(x)
void DELAY_sec(uint16_t seconds);

#endif

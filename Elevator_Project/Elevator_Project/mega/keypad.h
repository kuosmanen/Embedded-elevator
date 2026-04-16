#ifndef _KEYPAD_H
#define _KEYPAD_H

#include <avr/io.h>
#include "stdutils.h"

#define M_RowColDirection DDRK
#define M_ROW PORTK
#define M_COL PINK
#define C_RowOutputColInput_U8 0xF0

void KEYPAD_Init(void);
void KEYPAD_WaitForKeyRelease(void);
void KEYPAD_WaitForKeyPress(void);
uint8_t KEYPAD_GetKey(void);
uint8_t KEYPAD_GetKeyNonBlocking(void);

#endif

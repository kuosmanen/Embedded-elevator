#ifndef _KEYPAD_H
#define _KEYPAD_H

#include <avr/io.h>
#include "stdutils.h"
#include "stdbool.h"

#define M_RowColDirection DDRK
#define M_ROW PORTK
#define M_COL PINK
#define C_RowOutputColInput_U8 0xF0

#define KEYSCAN_UPPER_CAP 4
#define KEYSCAN_INITIAL 0

/**
 * Initializing the keypad based on its physical
 * layout.
 */
void KEYPAD_Init(void);

/*
 * The core functionality for detecting what key has been pressed from keypad
 * 1) Ensuring a key has been pressed (and released)
 * 2) Precise scanning to detect the pressed key
 * 3) Decoding the key for the value
 * 
 * Returns the value of the key that has been pressed.
 */
uint8_t KEYPAD_GetKey(void);

#endif

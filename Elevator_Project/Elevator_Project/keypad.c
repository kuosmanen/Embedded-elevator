#include "keypad.h"
#include "delay.h"
#include <stdbool.h>

static uint8_t keypad_ScanKey(void);
static uint8_t keypad_DecodeKey(uint8_t scan_code);


void KEYPAD_Init(void)
{
    M_RowColDirection = C_RowOutputColInput_U8;
    M_ROW = 0x0F;
}


uint8_t KEYPAD_GetKey(void)
{
    static uint8_t key_is_held = 0;
    uint8_t raw_key_input;

    // Scanning the keypad for inputs
    M_ROW = 0x0F;
    raw_key_input = M_COL & 0x0F;
    
    // Checking whether a keypress has been pressed.
    // If not, exiting with static keypress check-up value set to 0
    if (raw_key_input == 0x0F) {
        key_is_held = 0;
        return 0;
    }

    // Ensuring no values are being held down
    if (key_is_held) {
        return 0;
    }

    DELAY_ms(15); /* debounce */

    // Reading and ensuring the input can be detected from the keypad
    raw_key_input = 0x0F;
    raw_key_input = M_COL & 0x0F;
    if (raw_key_input == 0x0F) {
        return 0;
    }

    key_is_held = 1;
    return keypad_DecodeKey(keypad_ScanKey());
}

/** 
 * Converting a received byte into a corresponding key input.
 * The decoded key is returned based on the received hexa 
 * value.
 */
static uint8_t keypad_DecodeKey(uint8_t key)
{
    switch (key) {
        case 0xE7: return '*';
        case 0xEB: return '7';
        case 0xED: return '4';
        case 0xEE: return '1';
        case 0xD7: return '0';
        case 0xDB: return '8';
        case 0xDD: return '5';
        case 0xDE: return '2';
        case 0xB7: return '#';
        case 0xBB: return '9';
        case 0xBD: return '6';
        case 0xBE: return '3';
        case 0x77: return 'D';
        case 0x7B: return 'C';
        case 0x7D: return 'B';
        case 0x7E: return 'A';
        default:   return 0;
    }
}


/**
 * Scans the keypad by traversing each row one at a time. The output
 * can then be passed to the decoder for the intended user input value.
 */
static uint8_t keypad_ScanKey(void)
{
    uint8_t scan_code = 0xEF;
    uint8_t row;
    uint8_t col_value = 0x0F;  

    for (row = KEYSCAN_INITIAL; row < KEYSCAN_UPPER_CAP; row++) { // Scan All the 4-Rows for key press
        // Select 1-Row at a time for Scanning the Key
        M_ROW = scan_code; 
        DELAY_ms(1); 

        // Reading the columns to see where keypress occurred
        col_value = M_COL & 0x0F;
        
        // Ending the loop upon keypress detection. If all columns in
        // the row are of value 0x0F, no keys have been pressed. 
        if (col_value != 0x0F) {
            break;
        }

        // Moving to the next row
        scan_code = (uint8_t)((scan_code << 1) | 0x01); 
    }

    return (uint8_t)(col_value + (scan_code & 0xF0));
}

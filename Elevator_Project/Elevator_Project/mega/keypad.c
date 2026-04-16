#include "keypad.h"
#include "delay.h"

static uint8_t keypad_ScanKey(void);
static uint8_t keypad_DecodeKey(uint8_t scan_code);

void KEYPAD_Init(void)
{
    M_RowColDirection = C_RowOutputColInput_U8;
    M_ROW = 0x0F;
}

void KEYPAD_WaitForKeyRelease(void)
{
    uint8_t key;
    do {
        do {
            M_ROW = 0x0F;
            key = M_COL & 0x0F;
        } while (key != 0x0F);

        DELAY_ms(1);
        M_ROW = 0x0F;
        key = M_COL & 0x0F;
    } while (key != 0x0F);
}

void KEYPAD_WaitForKeyPress(void)
{
    uint8_t key;
    do {
        do {
            M_ROW = 0x0F;
            key = M_COL & 0x0F;
        } while (key == 0x0F);

        DELAY_ms(1);
        M_ROW = 0x0F;
        key = M_COL & 0x0F;
    } while (key == 0x0F);
}

uint8_t KEYPAD_GetKey(void)
{
    KEYPAD_WaitForKeyRelease();
    DELAY_ms(1);
    KEYPAD_WaitForKeyPress();
    return keypad_DecodeKey(keypad_ScanKey());
}

/*
 * Non-blocking helper added for the project work.
 * Returns 0 when no new key is available.
 */
uint8_t KEYPAD_GetKeyNonBlocking(void)
{
    static uint8_t key_is_held = 0;
    uint8_t raw;

    M_ROW = 0x0F;
    raw = M_COL & 0x0F;

    if (raw == 0x0F) {
        key_is_held = 0;
        return 0;
    }

    if (key_is_held) {
        return 0;
    }

    DELAY_ms(15); /* debounce */
    M_ROW = 0x0F;
    raw = M_COL & 0x0F;
    if (raw == 0x0F) {
        return 0;
    }

    key_is_held = 1;
    return keypad_DecodeKey(keypad_ScanKey());
}

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

static uint8_t keypad_ScanKey(void)
{
    uint8_t scan_code = 0xEF;
    uint8_t row;
    uint8_t col_value = 0x0F;

    for (row = 0; row < 4; row++) {
        M_ROW = scan_code;
        DELAY_ms(1);
        col_value = M_COL & 0x0F;
        if (col_value != 0x0F) {
            break;
        }
        scan_code = (uint8_t)((scan_code << 1) | 0x01);
    }

    return (uint8_t)(col_value + (scan_code & 0xF0));
}

#ifndef LCD_PRINT_UTILS_H
#define LCD_PRINT_UTILS_H

#include <avr/io.h>
#include <avr/eeprom.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "lcd.h"
#include "mega_twi.h"
#include "protocol.h"
#include "queue_utils.h"
#include "elevator_state.h"

#define LCD_BUFFER_SIZE 17

void lcd_print_line(uint8_t row, const char *text);

void lcd_show_idle(void);
void lcd_show_current_floor(const char *state_text);
void lcd_show_fault(void);
void lcd_show_obstacle(void);
void lcd_show_queue_status(void);

#endif
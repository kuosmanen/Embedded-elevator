#include "lcd_print_utils.h"

void lcd_print_line(uint8_t row, const char *text)
{
    char buffer[LCD_BUFFER_SIZE];
    uint8_t i = 0;

    while ((text[i] != '\0') && (i < 16u)) {
        buffer[i] = text[i];
        i++;
    }
    while (i < 16u) {
        buffer[i++] = ' ';
    }
    buffer[16] = '\0';

    lcd_gotoxy(0, row);
    lcd_puts(buffer);
}


void lcd_show_idle(void)
{
    char line2[LCD_BUFFER_SIZE];
    lcd_print_line(0, "Choose floor:# ");

    if (g_input_len == 0u) {
        snprintf(line2, sizeof(line2), "Current:%02u Q:%u", g_current_floor, g_queue.count);
    } else if (g_input_len == 1u) {
        snprintf(line2, sizeof(line2), "Input:%u_ Q:%u   ", g_input_digits[0], g_queue.count);
    } else {
        snprintf(line2, sizeof(line2), "Input:%u%u Q:%u   ", g_input_digits[0], g_input_digits[1], g_queue.count);
    }

    lcd_print_line(1, line2);
}

void lcd_show_current_floor(const char *state_text)
{
    char line1[LCD_BUFFER_SIZE];
    char line2[LCD_BUFFER_SIZE];

    snprintf(line1, sizeof(line1), "%s        ", state_text);
    snprintf(line2, sizeof(line2), "Cur:%02u Tar:%02u Q:%u", g_current_floor, g_target_floor, g_queue.count);
    lcd_print_line(0, line1);
    lcd_print_line(1, line2);
}

void lcd_show_fault(void)
{
    lcd_print_line(0, "Same floor      ");
    lcd_print_line(1, "Choose another  ");
}

void lcd_show_obstacle(void)
{
    lcd_print_line(0, "Obstacle detect ");
    lcd_print_line(1, "Press any key   ");
}

void lcd_show_queue_status(void)
{
    char line[LCD_BUFFER_SIZE];
    snprintf(line, sizeof(line), "Queued requests:%u", g_queue.count);
    lcd_print_line(1, line);
}
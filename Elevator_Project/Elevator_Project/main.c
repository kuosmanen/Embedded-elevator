#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "lcd.h"
#include "keypad.h"
#include "mega_twi.h"
#include "protocol.h"
#include "queue_utils.h"
#include "lcd_print_utils.h"
#include "elevator_state.h"

#define MOVE_STEP_MS 1000UL
#define DOOR_OPEN_TIME_MS 3000UL
#define DOOR_CLOSE_TIME_MS 2000UL
#define FAULT_TIME_MS 1500UL
#define LOW_POWER_DELAY_MS 10000UL


typedef enum elevator_state {
    STATE_IDLE = 0,
    STATE_GOING_UP,
    STATE_GOING_DOWN,
    STATE_DOOR_OPENING,
    STATE_OBSTACLE_DETECTION,
    STATE_DOOR_CLOSING,
    STATE_FAULT
} elevator_state_t;

floor_queue_t g_queue;
elevator_state_t g_state = STATE_IDLE;
uint8_t g_current_floor = 0;
uint8_t g_target_floor = 0;
uint8_t g_input_digits[2];
uint8_t g_input_len = 0;

static uint32_t g_state_time_ms = 0;
static uint32_t g_move_time_ms = 0;
static uint32_t g_inactivity_time_ms = 0;
static uint8_t EEMEM saved_floor_eeprom;
static volatile bool g_keypad_wake_pending = false;

static void handle_idle_key(uint8_t key);
static void handle_background_queue_key(uint8_t key);
static void try_start_next_request(void);
static void set_state(elevator_state_t new_state);
static bool process_keypad(void);
static void update_state_machine(uint32_t elapsed_ms);
static uint8_t digits_to_floor(void);
static bool is_digit(uint8_t key);
static void keypad_wakeup_interrupt_init(void);
static bool can_enter_low_power(void);
static void enter_low_power_until_keypad(void);

ISR(PCINT2_vect)
{
    g_keypad_wake_pending = true;
}

static void keypad_wakeup_interrupt_init(void)
{
    /* PK0..PK3 are keypad column inputs and are PCINT16..PCINT19 on ATmega2560 */
    PCMSK2 |= (1 << PCINT16) | (1 << PCINT17) | (1 << PCINT18) | (1 << PCINT19);
    PCIFR |= (1 << PCIF2);
    PCICR |= (1 << PCIE2);
}

static bool can_enter_low_power(void)
{
    return (g_state == STATE_IDLE) && queue_is_empty(&g_queue) && (g_input_len == 0u);
}

static void enter_low_power_until_keypad(void)
{
    uint8_t adcsra_backup;
    uint8_t acsr_backup;
    uint8_t prr0_backup;
    uint8_t prr1_backup;

    /* If a key is already held, skip sleep and handle it immediately. */
    if ((M_COL & 0x0Fu) != 0x0Fu) {
        return;
    }

    lcd_command(LCD_DISP_OFF);

    g_keypad_wake_pending = false;

    adcsra_backup = ADCSRA;
    acsr_backup = ACSR;
    prr0_backup = PRR0;
    prr1_backup = PRR1;

    /* Disable analog blocks and clock-gate unused peripherals during sleep. */
    ADCSRA &= (uint8_t)~(1 << ADEN);
    ACSR |= (1 << ACD);
    PRR0 = 0xFFu;
    PRR1 = 0xFFu;

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();

    cli();
    if ((M_COL & 0x0Fu) == 0x0Fu) {
#if defined(BODS) && defined(BODSE)
        sleep_bod_disable();
#endif
        sei();
        sleep_cpu();
    } else {
        sei();
    }

    sleep_disable();

    PRR0 = prr0_backup;
    PRR1 = prr1_backup;
    ACSR = acsr_backup;
    ADCSRA = adcsra_backup;

    lcd_command(LCD_DISP_ON);
    lcd_show_idle();

    if (g_keypad_wake_pending) {
        _delay_ms(15);
    }

    PCIFR |= (1 << PCIF2);
}


static uint8_t digits_to_floor(void)
{
	return g_input_len == 1u
		? g_input_digits[0]
		: (uint8_t)(g_input_digits[0] * 10u + g_input_digits[1]);
}

static bool is_digit(uint8_t key)
{
    return (key >= '0' && key <= '9');
}

static void submit_floor_request(uint8_t floor)
{
    if (floor == g_current_floor && g_state == STATE_IDLE && queue_is_empty(&g_queue)) {
        set_state(STATE_FAULT);
        return;
    }

    if (!queue_push(&g_queue, floor)) {
        lcd_print_line(0, "Queue full      ");
        lcd_print_line(1, "Try again later ");
        _delay_ms(800);
    }
}

static void handle_idle_key(uint8_t key)
{
    if (is_digit(key)) {
        if (g_input_len < 2u) {
            g_input_digits[g_input_len++] = (uint8_t)(key - '0');
        }
    } else if (key == '*') {
        g_input_len = 0;
    } else if (key == '#') {
        if (g_input_len > 0u) {
            submit_floor_request(digits_to_floor());
            g_input_len = 0;
        }
    }
}

static void handle_background_queue_key(uint8_t key)
{
    static uint8_t buffered_digits[2];
    static uint8_t buffered_len = 0;

    if (is_digit(key)) {
        if (buffered_len < 2u) {
            buffered_digits[buffered_len++] = (uint8_t)(key - '0');
        }
    } else if (key == '*') {
        if (g_state == STATE_DOOR_OPENING) {
            set_state(STATE_OBSTACLE_DETECTION);
            return;
        }
        buffered_len = 0;
    } else if (key == '#') {
        uint8_t floor;
        if (buffered_len == 1u) {
            floor = buffered_digits[0];
            queue_push(&g_queue, floor);
        } else if (buffered_len == 2u) {
            floor = (uint8_t)(buffered_digits[0] * 10u + buffered_digits[1]);
            queue_push(&g_queue, floor);
        }
        buffered_len = 0;
    } else if (g_state == STATE_OBSTACLE_DETECTION) {
        set_state(STATE_DOOR_CLOSING);
    }
}

static void try_start_next_request(void)
{
    if (g_state != STATE_IDLE) {
        return;
    }

    if (!queue_pop(&g_queue, &g_target_floor)) {
        return;
    }

    if (g_target_floor == g_current_floor) {
        set_state(STATE_FAULT);
    } else if (g_target_floor > g_current_floor) {
        set_state(STATE_GOING_UP);
    } else {
        set_state(STATE_GOING_DOWN);
    }
}

static void set_state(elevator_state_t new_state)
{
    g_state = new_state;
    g_state_time_ms = 0;
    g_move_time_ms = 0;
    g_inactivity_time_ms = 0;

    switch (g_state) {
        case STATE_IDLE:
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_IDLE);
            lcd_show_idle();
            break;
        case STATE_GOING_UP:
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_MOVING);
            lcd_show_current_floor("Going up");
            break;
        case STATE_GOING_DOWN:
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_MOVING);
            lcd_show_current_floor("Going down");
            break;
        case STATE_DOOR_OPENING:
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_DOOR_OPEN);
            lcd_print_line(0, "Door open       ");
            lcd_print_line(1, "*: obstacle     ");
            break;
        case STATE_OBSTACLE_DETECTION:
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_OBSTACLE_START);
            lcd_show_obstacle();
            break;
        case STATE_DOOR_CLOSING:
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_DOOR_CLOSING);
            lcd_print_line(0, "Door closing    ");
            lcd_print_line(1, "Please wait     ");
            break;
        case STATE_FAULT:
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_FAULT);
            lcd_show_fault();
            break;
        default:
            break;
    }
}

/**
 * A function that main.c uses to utilize the keypad functionality,
 * including key input detection.
 *
 * Returns true when a key was decoded. The main loop uses this as
 * activity so that low power mode is not re-entered immediately.
 */
static bool process_keypad(void)
{
    uint8_t key = KEYPAD_GetKey();
    if (key == 0u) {
        return false;
    }

    if (g_state == STATE_IDLE) {
        handle_idle_key(key);
        if (g_state == STATE_IDLE) {
            lcd_show_idle();
        }
        return true;
    }

    if (g_state == STATE_OBSTACLE_DETECTION) {
        twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_OBSTACLE_STOP);
        set_state(STATE_DOOR_CLOSING);
        return true;
    }

    handle_background_queue_key(key);
    return true;
}

static void update_state_machine(uint32_t elapsed_ms)
{
    g_state_time_ms += elapsed_ms;

    switch (g_state) {
        case STATE_IDLE:
            try_start_next_request();
            break;

        case STATE_GOING_UP:
            g_move_time_ms += elapsed_ms;
            if (g_move_time_ms >= MOVE_STEP_MS) {
                g_move_time_ms = 0;
                if (g_current_floor < g_target_floor) {
                    g_current_floor++;
                    eeprom_update_byte(&saved_floor_eeprom, g_current_floor);
                    lcd_show_current_floor("Going up");
                }
                if (g_current_floor >= g_target_floor) {
                    set_state(STATE_DOOR_OPENING);
                }
            }
            break;

        case STATE_GOING_DOWN:
            g_move_time_ms += elapsed_ms;
            if (g_move_time_ms >= MOVE_STEP_MS) {
                g_move_time_ms = 0;
                if (g_current_floor > g_target_floor) {
                    g_current_floor--;
                    eeprom_update_byte(&saved_floor_eeprom, g_current_floor);
                    lcd_show_current_floor("Going down");
                }
                if (g_current_floor <= g_target_floor) {
                    set_state(STATE_DOOR_OPENING);
                }
            }
            break;

        case STATE_DOOR_OPENING:
            if (g_state_time_ms >= DOOR_OPEN_TIME_MS) {
                set_state(STATE_DOOR_CLOSING);
            }
            break;

        case STATE_OBSTACLE_DETECTION:
            /* waits here until any keypad key is pressed */
            break;

        case STATE_DOOR_CLOSING:
            if (g_state_time_ms >= DOOR_CLOSE_TIME_MS) {
                set_state(STATE_IDLE);
            }
            break;

        case STATE_FAULT:
            if (g_state_time_ms >= FAULT_TIME_MS) {
                set_state(STATE_IDLE);
            }
            break;

        default:
            break;
    }
}

int main(void)
{
    uint8_t restored_floor;

    KEYPAD_Init();
    keypad_wakeup_interrupt_init();
    lcd_init(LCD_DISP_ON);
    lcd_clrscr();
    twi_master_init();

    queue_init(&g_queue);

    restored_floor = eeprom_read_byte(&saved_floor_eeprom);
    if (restored_floor <= 99u) {
        g_current_floor = restored_floor;
    } else {
        g_current_floor = 0u;
        eeprom_update_byte(&saved_floor_eeprom, 0u);
    }

    lcd_print_line(0, "Elevator ready  ");
    lcd_print_line(1, "Floor restored  ");
    _delay_ms(1200);
    set_state(STATE_IDLE);
    sei();

    while (1) {
        bool key_activity;

        /*
         * Only enter low power after a real inactivity timeout.
         * The earlier version slept immediately whenever the system was idle,
         * the request queue was empty and no digits were currently typed.
         */
        if (can_enter_low_power() && (g_inactivity_time_ms >= LOW_POWER_DELAY_MS)) {
            enter_low_power_until_keypad();

            /* Waking up is activity even if the key is released too quickly
             * to be decoded. Without this reset the system can immediately
             * enter sleep again after the wake key is released.
             */
            g_inactivity_time_ms = 0u;

            process_keypad();
            update_state_machine(0u);
            continue;
        }

        key_activity = process_keypad();
        update_state_machine(50u);

        if (key_activity || !can_enter_low_power()) {
            g_inactivity_time_ms = 0u;
        } else if (g_inactivity_time_ms < LOW_POWER_DELAY_MS) {
            g_inactivity_time_ms += 50u;
        }

        _delay_ms(50);
    }
}

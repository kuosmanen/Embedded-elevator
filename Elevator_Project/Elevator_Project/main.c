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
#define POST_WAKE_LOCKOUT_MS 500UL
#define WAKE_RELEASE_TIMEOUT_MS 1200UL

/*
 * Elevator state machine states.

    * The Mega uses these states to decide what happens next
    * Whenever the state changes, the Mega also sends a command to the Uno so it can
    * match the background music / sound and LEDs to the current elevator state
 */

typedef enum elevator_state {
    STATE_IDLE = 0,             // Default state. Waiting for requests.
    STATE_GOING_UP,             // Moving towards target floor.
    STATE_GOING_DOWN,           // Moving towards target floor.
    STATE_DOOR_OPENING,         // Door is opening, can trigger obstacle.
    STATE_OBSTACLE_DETECTION,   // Obstacle detected. Can be overridden with input.
    STATE_DOOR_CLOSING,         // Door is closing.
    STATE_FAULT                 // Fault condition.
} elevator_state_t;

// Queue variable for storing floor requests. Implementation in queue_utils.c
floor_queue_t g_queue;
elevator_state_t g_state = STATE_IDLE;
uint8_t g_current_floor = 0;
uint8_t g_target_floor = 0;
uint8_t g_input_digits[2];
uint8_t g_input_len = 0;

static uint32_t g_state_time_ms = 0;
static uint32_t g_move_time_ms = 0;
static uint32_t g_inactivity_time_ms = 0;
static uint32_t g_sleep_lockout_ms = 0;
// EEPROM variable to store current floor even if powered off.
static uint8_t EEMEM saved_floor_eeprom;
/* 
 * Flag set by the keypad wake-up interrupt.
 * The interrupt is mainly used to wake the CPU from sleep.
 */
static volatile bool g_keypad_wake_pending = false;

/*
 * Function prototypes for local helper functions in this file.
 */
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
static void mark_activity(void);

/* 
 * Pin change interrupt for waking up from sleep when a keypad key is pressed.
 * The interrupt handler just sets a flag, and the main loop checks this flag
 * after waking up to know that the wake-up was caused by the keypad.
 */
ISR(PCINT2_vect)
{
    g_keypad_wake_pending = true;
}

/*
 * Initialize the keypad wake-up interrupt.
 */
static void keypad_wakeup_interrupt_init(void)
{
    /* PK0..PK3 are keypad column inputs and are PCINT16..PCINT19 on ATmega2560 */
    PCMSK2 |= (1 << PCINT16) | (1 << PCINT17) | (1 << PCINT18) | (1 << PCINT19);
    PCIFR |= (1 << PCIF2);
    PCICR |= (1 << PCIE2);
}

/* 
 * Check that we can enter Low power mode, which requires:
 * - elevator is idle
 * - queue is empty
 * - no floor digits are currently typed
 */
static bool can_enter_low_power(void)
{
    return (g_state == STATE_IDLE) && queue_is_empty(&g_queue) && (g_input_len == 0u);
}
// Simply resets the inactivity timer when something happens.
static void mark_activity(void)
{
    g_inactivity_time_ms = 0u;
}

/*
 * Enter low power mode until a keypad key is pressed.
 *
 * Before sleeping:
 * - prepare the keypad for wake-up
 * - LCD display is turned off
 * - unused peripherals are disabled
 * 
 * After waking up:
 * - restore peripherals
 * - re-init the keypad
 * - re-init the LCD
 * - wake key is ignored as normal input
 */
static void enter_low_power_until_keypad(void)
{
    uint8_t adcsra_backup;
    uint8_t acsr_backup;
    uint8_t prr0_backup;
    uint8_t prr1_backup;
    uint16_t release_wait_ms = 0u;

    // Prepare the keypad specifically for wake-up before checking/sleeping.
    KEYPAD_Init();
    M_ROW = 0x0Fu;
    _delay_ms(2);

    /*
     * If a key is already held, dont sleep.
     * Otherwise the system could sleep immediately during a key release or bounce.
     */
    if ((M_COL & 0x0Fu) != 0x0Fu) {
        mark_activity();
        return;
    }

    lcd_command(LCD_DISP_OFF);

    g_keypad_wake_pending = false;
    PCIFR |= (1 << PCIF2);

    // Save peripheral register states so we can restore them after waking up.
    adcsra_backup = ADCSRA;
    acsr_backup = ACSR;
    prr0_backup = PRR0;
    prr1_backup = PRR1;

    // Disable analog blocks and clock-gate unused peripherals during sleep.
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

    KEYPAD_Init();
    M_ROW = 0x0Fu;

    lcd_command(LCD_DISP_ON);
    lcd_show_idle();

    /*
     * The key used to wake the system should not also be interpreted as a
     * floor digit or command. Wait for release, with a timeout so a stuck key
     * cannot freeze the program forever.
     */
    while (((M_COL & 0x0Fu) != 0x0Fu) && (release_wait_ms < WAKE_RELEASE_TIMEOUT_MS)) {
        _delay_ms(10);
        release_wait_ms = (uint16_t)(release_wait_ms + 10u);
    }

    _delay_ms(30);
    PCIFR |= (1 << PCIF2);
    mark_activity();
}
// Converts input digits into a floor number.
static uint8_t digits_to_floor(void)
{
    return g_input_len == 1u
        ? g_input_digits[0]
        : (uint8_t)(g_input_digits[0] * 10u + g_input_digits[1]);
}

// Check that input digit is a number, not a command key.
static bool is_digit(uint8_t key)
{
    return (key >= '0' && key <= '9');
}
/*
 * Handling of floor requests to elevator queue.
 * If the request is the same floor as the current one enter FAULT state as per assignment.
 * Else if the queue is full display queue is full message on LCD. Otherwise add the request to the queue.
*/
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

/**
 * Handles key presses when the elevator is in idle state.
 * Digits are stored until user presses "#" to submit the request or "*" to clear the input.
 */
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

/*
 * Handles key presses when the elevator is already busy.
 * 
 * This allows for floor requests to be queued while the elevator is moving or is handling doors.
 * This function uses a separate small input buffer so as to not intervene with the idle input buffer.
 * 
 * Some special behaviours:
 * - "*" during DOOR_OPENING triggers obstacle detetcion
 * - "#" confirms a queued floor request
 * - "A" clears the queue
 * - any key during OBSTACLE_DETECTION ends the obstacle state and starts closing the door again.
 */
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
    } else if (key == 'A') {
        queue_reset(&g_queue);
    } else if (g_state == STATE_OBSTACLE_DETECTION) {
        set_state(STATE_DOOR_CLOSING);
    }
}

/*
 * Start the next queued floor request if elevator is idle
 *
 * If queue is empty, do nothing.
 * If the next target is above the current floor, start going up.
 * If next target is below the current floor, start going down.
 * If the target is the current floor, enter FAULT state as per assignment.
 */
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

/*
 * Handles changing of the elevator state.
 *
 * This function centralizes all state changes and actions that happen when a new state begins:
 * - update the current state variable
 * - reset state timing counters
 * - reset the inactivity timer
 * - send the matching (to state) command to the UNO
 * - update the LCD message
 */
static void set_state(elevator_state_t new_state)
{
    g_state = new_state;
    g_state_time_ms = 0;
    g_move_time_ms = 0;
    mark_activity();

    switch (g_state) {
        case STATE_IDLE:
            /*
             * Elevator waiting for input.
             * Tell UNO to enter idle output mode and show idle text on LCD
             */
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_IDLE);
            lcd_show_idle();
            break;
        case STATE_GOING_UP:
            /*
             * Elevator moving upwards
             * UNO turns on movement LED and keeps background memory playing
             */
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_MOVING);
            lcd_show_current_floor("Going up");
            break;
        case STATE_GOING_DOWN:
            /*
             * Elevator moving downwards
             * UNO turns on movement LED and keeps background memory playing
             */
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_MOVING);
            lcd_show_current_floor("Going down");
            break;
        case STATE_DOOR_OPENING:
            /*
             * Door is opening, "*" can trigger obstacle detection
             */
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_DOOR_OPEN);
            lcd_print_line(0, "Door open       ");
            lcd_print_line(1, "*: obstacle     ");
            break;
        case STATE_OBSTACLE_DETECTION:
            /*
             * Obstacle detected, waiting for user to clear it by pressing any key.
             * The UNO blinks the obstacle LED and plays alert sounds.
             */
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_OBSTACLE_START);
            lcd_show_obstacle();
            break;
        case STATE_DOOR_CLOSING:
            /*
             * Door is closing, returns to idle afterwards.
             */
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_DOOR_CLOSING);
            lcd_print_line(0, "Door closing    ");
            lcd_print_line(1, "Please wait     ");
            break;
        case STATE_FAULT:
            /* 
             * Fault state, used when the selected floor equals current floor.
             */
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_FAULT);
            lcd_show_fault();
            break;
        default:
            break;
    }
}

/*
 * Read and process one keypad input if available.
 *
 * Returns true if a key was received. The main loop uses this return value
 * as user activity, which resets the inactivity timer.
 *
 * Input handling depends on the current elevator state:
 * - IDLE: digits, clear, and submit floor requests
 * - OBSTACLE_DETECTION: any key stops obstacle alert and starts door closing
 * - other active states: keypad input is used for background queueing
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

/*
 * Advance the elevator state machine.
 *
 * This function is called repeatedly from the main loop with elapsed_ms = 50.
 * It updates timers and decides when to transition from one state to another.
 *
 * The actual state transition actions are handled by set_state().
 */
static void update_state_machine(uint32_t elapsed_ms)
{
    g_state_time_ms += elapsed_ms;

    switch (g_state) {
        case STATE_IDLE:
            /*
             * While idle, check whether a queued request exists.
             * If there is one, start moving toward it.
             */
            try_start_next_request();
            break;

        case STATE_GOING_UP:
            /*
             * Simulate elevator movement.
             * Every MOVE_STEP_MS milliseconds, increase current floor by one.
             * The floor is saved to EEPROM so it can be restored after reset.
             */
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
            /*
             * Same as GOING_UP, but the current floor decreases by one step.
             */
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
            /*
             * Door stays open for a fixed time unless obstacle is triggered.
             */
            if (g_state_time_ms >= DOOR_OPEN_TIME_MS) {
                set_state(STATE_DOOR_CLOSING);
            }
            break;

        case STATE_OBSTACLE_DETECTION:
            /*
             * Stay here until process_keypad() detects a key press.
             */
            break;

        case STATE_DOOR_CLOSING:
            /*
             * Door closing lasts for a fixed time, then elevator returns to idle.
             */
            if (g_state_time_ms >= DOOR_CLOSE_TIME_MS) {
                set_state(STATE_IDLE);
            }
            break;

        case STATE_FAULT:
            /*
             * Fault message is shown briefly, then system returns to idle.
             */
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

    /*
     * Initialize hardware modules controlled by the Mega.
     */
    KEYPAD_Init();
    keypad_wakeup_interrupt_init();
    lcd_init(LCD_DISP_ON);
    lcd_clrscr();
    twi_master_init();

    /*
     * Clear the floor request queue at startup.
     */
    queue_reset(&g_queue);

    /*
     * Restore the last known floor from EEPROM.
     * If EEPROM contains an invalid value, reset to floor 0.
     */
    restored_floor = eeprom_read_byte(&saved_floor_eeprom);
    if (restored_floor <= 99u) {
        g_current_floor = restored_floor;
    } else {
        g_current_floor = 0u;
        eeprom_update_byte(&saved_floor_eeprom, 0u);
    }
    /*
     * Show startup message briefly, then enter idle state.
     */
    lcd_print_line(0, "Elevator ready  ");
    lcd_print_line(1, "Floor restored  ");
    _delay_ms(1200);
    set_state(STATE_IDLE);

    /*
     * Enable global interrupts.
     * Needed for keypad wake-up interrupt.
     */
    sei();

    while (1) {
        bool key_activity;

        /*
         * Only enter low power after:
         * - elevator is idle
         * - queue is empty
         * - no floor digits are currently typed
         * - inactivity timeout has elapsed
         * - post-wake lockout has ended
         */
        if ((g_sleep_lockout_ms == 0u) &&
            can_enter_low_power() &&
            (g_inactivity_time_ms >= LOW_POWER_DELAY_MS)) {

            enter_low_power_until_keypad();

            /*
             * The wake key only wakes the MEGA. It should not also become a
             * floor input. The UNO still needs a command so it can resume the
             * background melody and restart its own idle sleep countdown.
             */
            mark_activity();
            twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_IDLE);

            /* Prevent immediate re-sleep after key release/bounce. */
            g_sleep_lockout_ms = POST_WAKE_LOCKOUT_MS;

            continue;
        }

        /*
         * Read keypad once and advance the state machine by one loop step.
         */
        key_activity = process_keypad();
        update_state_machine(50u);

        /*
         * If a key was pressed while the elevator is still idle, notify the Uno.
         *
         * If no digits are buffered, UNO_CMD_IDLE lets the Uno start its own
         * sleep countdown.
         *
         * If digits are buffered, UNO_CMD_BACKGROUND keeps the background music
         * running without starting the Uno sleep countdown.
         */
        if (key_activity && (g_state == STATE_IDLE)) {
            if (can_enter_low_power()) {
                twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_IDLE);
            } else {
                twi_master_send_byte(ELEVATOR_TWI_SLAVE_ADDRESS, UNO_CMD_BACKGROUND);
            }
        }
        /*
         * Count down the post-wake lockout timer.
         */
        if (g_sleep_lockout_ms >= 50u) {
            g_sleep_lockout_ms -= 50u;
        } else {
            g_sleep_lockout_ms = 0u;
        }
        /*
         * Update inactivity timer.
         *
         * Any key activity or active elevator state resets the inactivity timer.
         * Only fully idle time is allowed to increase the timer.
         */
        if (key_activity || !can_enter_low_power()) {
            mark_activity();
        } else if (g_inactivity_time_ms < LOW_POWER_DELAY_MS) {
            g_inactivity_time_ms += 50u;
        }

        /*
         * Main loop period.
         * Since this delay is 50 ms, update_state_machine(50u) uses the same
         * elapsed time value.
         */
        _delay_ms(50);
    }
}

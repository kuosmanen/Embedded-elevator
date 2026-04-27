#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <stdbool.h>

#include "uno_twi.h"
#include "buzzer.h"
#include "protocol.h"
#include "tune.h"

#define LOW_POWER_DELAY_MS 10000UL //10 seconds of idling before power saving mode

/* UNO output allocation
 * D4  -> movement LED
 * D5  -> door open LED
 * D6  -> door closing LED
 * D7  -> obstacle LED
 * D9  -> buzzer (OC1A hardware toggle)
 */
#define MOVING_LED_PORT PORTD
#define MOVING_LED_DDR  DDRD
#define MOVING_LED_PIN  PD3   /* Arduino D3 */

#define OPEN_LED_PORT   PORTD
#define OPEN_LED_DDR    DDRD
#define OPEN_LED_PIN    PD4   /* Arduino D4 */

#define CLOSE_LED_PORT  PORTD
#define CLOSE_LED_DDR   DDRD
#define CLOSE_LED_PIN   PD5   /* Arduino D5 */

#define OBST_LED_PORT   PORTD
#define OBST_LED_DDR    DDRD
#define OBST_LED_PIN    PD6   /* Arduino D6 */

static bool g_obstacle_blink_active = false;
static uint8_t g_obstacle_toggle_count = 0;
static uint32_t g_last_blink_ms = 0;
static bool g_low_power_mode = false;
static bool g_low_power_pending = false;
static uint32_t g_low_power_requested_ms = 0;

static void exit_low_power_mode(void)
{
    g_low_power_pending = false;

    timer0_tick_start();

    if (!g_low_power_mode) {
        return;
    }

    g_low_power_mode = false;
}

/// Switching all status LEDs off
static void leds_all_off(void)
{
    MOVING_LED_PORT &= (uint8_t)~(1 << MOVING_LED_PIN);
    OPEN_LED_PORT   &= (uint8_t)~(1 << OPEN_LED_PIN);
    CLOSE_LED_PORT  &= (uint8_t)~(1 << CLOSE_LED_PIN);
    OBST_LED_PORT   &= (uint8_t)~(1 << OBST_LED_PIN);
}

/// Configuring all output LEDs and reset them to OFF
static void outputs_init(void)
{
    MOVING_LED_DDR |= (1 << MOVING_LED_PIN);
    OPEN_LED_DDR   |= (1 << OPEN_LED_PIN);
    CLOSE_LED_DDR  |= (1 << CLOSE_LED_PIN);
    OBST_LED_DDR   |= (1 << OBST_LED_PIN);
    leds_all_off();
}

/// Starting obstacle blink pattern timing
static void obstacle_blink_start(void)
{
    g_obstacle_blink_active = true;
    g_obstacle_toggle_count = 0u;
    g_last_blink_ms = millis_get();
    OBST_LED_PORT &= (uint8_t)~(1 << OBST_LED_PIN);
}

/// Blinking obstacle LED
static void obstacle_blink_update(void)
{
    uint32_t now;

    if (!g_obstacle_blink_active) {
        return;
    }

    now = millis_get();
    if ((now - g_last_blink_ms) >= 250u) {
        OBST_LED_PORT ^= (1 << OBST_LED_PIN);
        g_last_blink_ms = now;
        g_obstacle_toggle_count++;

        if (g_obstacle_toggle_count >= 6u) {
            g_obstacle_blink_active = false;
            OBST_LED_PORT &= (uint8_t)~(1 << OBST_LED_PIN);
        }
    }
}

/// Applying commands received from the MEGA master controller by setting LEDs and buzzer state
static void apply_command(uint8_t command)
{
    //making sure timing is live and the CPU is out of low-power before processing commands
     
    exit_low_power_mode();

    switch (command) {
        case UNO_CMD_IDLE:
            g_obstacle_blink_active = false;
            leds_all_off();
            buzzer_stop();
            g_low_power_requested_ms = millis_get();
            g_low_power_pending = true;
            break;

        case UNO_CMD_MOVING:
            g_obstacle_blink_active = false;
            leds_all_off();
            MOVING_LED_PORT |= (1 << MOVING_LED_PIN);
            buzzer_start_background();
            break;

        case UNO_CMD_DOOR_OPEN:
            g_obstacle_blink_active = false;
            leds_all_off();
            OPEN_LED_PORT |= (1 << OPEN_LED_PIN);
            buzzer_start_background();
            break;

        case UNO_CMD_DOOR_CLOSING:
            g_obstacle_blink_active = false;
            leds_all_off();
            CLOSE_LED_PORT |= (1 << CLOSE_LED_PIN);
            buzzer_start_background();
            break;

        case UNO_CMD_OBSTACLE_START:
            leds_all_off();
            obstacle_blink_start();
            buzzer_start_alert();
            break;

        case UNO_CMD_OBSTACLE_STOP:
            g_obstacle_blink_active = false;
            OBST_LED_PORT &= (uint8_t)~(1 << OBST_LED_PIN);
            buzzer_start_background();
            break;

        case UNO_CMD_FAULT:
        default:
            g_obstacle_blink_active = false;
            leds_all_off();
            buzzer_stop();//silent if fault
            break;
    }
}

int main(void)
{
	uint8_t command;

	outputs_init();
	buzzer_init();
	timer0_tick_init();
	twi_slave_init(ELEVATOR_TWI_SLAVE_ADDRESS);
    set_sleep_mode(SLEEP_MODE_IDLE);
	sei();

	while (1) {
		if (twi_slave_receive_byte(&command)) {
			apply_command(command);
		}

        if (g_low_power_pending && ((millis_get() - g_low_power_requested_ms) >= LOW_POWER_DELAY_MS)) {
            g_low_power_pending = false;
            timer0_tick_stop();
            g_low_power_mode = true;
        }

        if (g_low_power_mode) {
            sleep_enable();
            sleep_cpu();
            sleep_disable();
            continue;
        }

		obstacle_blink_update();
		buzzer_update();
	}
}

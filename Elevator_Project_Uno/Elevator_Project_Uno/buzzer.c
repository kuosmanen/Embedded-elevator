#define F_CPU 16000000UL

#include "buzzer.h"
#include "tune.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define BUZZER_DDR DDRB /* Data Direction Register for buzzer pin (output) */
#define BUZZER_PORT PORTB /* Port Register for buzzer pin */
#define BUZZER_PIN PB1   /* Arduino UNO D9 / OC1A */

typedef enum buzzer_mode {
    BUZZER_MODE_OFF = 0,
    BUZZER_MODE_BACKGROUND,
    BUZZER_MODE_ALERT,
} buzzer_mode_t;

static volatile uint32_t g_millis = 0;
static buzzer_mode_t g_mode = BUZZER_MODE_OFF;
static const note_t *g_active_melody = 0;
static uint8_t g_active_melody_len = 0u;
static uint8_t g_note_index = 0;
static uint32_t g_note_started_ms = 0;

//increments the ms counter every 1ms (when Timer0 Compare Match A interrupt occurs)
ISR(TIMER0_COMPA_vect)
{
    g_millis++;
}

//1ms interrupt tick for counting time for note timings
void timer0_tick_init(void)
{
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00); /* prescaler 64 */
    OCR0A = 249;                        /* 1 ms tick */
    timer0_tick_start();
}

void timer0_tick_start(void)
{
    TIMSK0 = (1 << OCIE0A);
}

void timer0_tick_stop(void)
{
    TIMSK0 &= (uint8_t)~(1 << OCIE0A);
}

/// Reading elapsed milliseconds
uint32_t millis_get(void)
{
    uint32_t value;
    uint8_t sreg = SREG;
    cli();
    value = g_millis;
    SREG = sreg;
    return value;
}

/// Disable timer1 OC1A output to silence buzzer pin
static void timer1_disable_output(void)
{
    TCCR1A &= (uint8_t)~((1 << COM1A1) | (1 << COM1A0));
    BUZZER_PORT &= (uint8_t)~(1 << BUZZER_PIN);
}

/// Set timer1 toggle frequency used for square-wave buzzer output
static void timer1_set_frequency(uint16_t frequency)
{
    uint32_t top; //temporary variable for calculations

    //resetting timer1 state
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;

    if (frequency == 0u) {
        //silence if frequency = 0
        timer1_disable_output();
        return;
    }

    /* CTC mode, toggle OC1A on compare match, prescaler 8 */
    //flips the output pin on and off (square wave) at the desired frequency
    top = (F_CPU / (2UL * 8UL * frequency)) - 1UL;
    if (top > 65535UL) {
        top = 65535UL; //16-bit max value to prevent overflow
    }

    OCR1A = (uint16_t)top; //setting the register to the frequency
    TCCR1A = (1 << COM1A0); //toggling on compare match
    TCCR1B = (1 << WGM12) | (1 << CS11); //CTC mode, prescaler 8
}

void buzzer_init(void)
{
    BUZZER_DDR |= (1 << BUZZER_PIN);
    timer1_disable_output();
}


void buzzer_start_background(void)
{
    // If the background melody is already playing, do not restart it.
    if (g_mode == BUZZER_MODE_BACKGROUND) {
        return;
    }

    /* loading background tune */
    g_mode = BUZZER_MODE_BACKGROUND;
    g_active_melody = UNO_BACKGROUND_TUNE;
    g_active_melody_len = UNO_BACKGROUND_TUNE_LENGTH;
    g_note_index = 0u;
    g_note_started_ms = millis_get();
    timer1_set_frequency(g_active_melody[g_note_index].frequency_hz);
}

/// doing the same for the obstacle alert melody
void buzzer_start_alert(void)
{
    g_mode = BUZZER_MODE_ALERT;
    g_active_melody = UNO_ALERT_TUNE;
    g_active_melody_len = UNO_ALERT_TUNE_LENGTH;
    g_note_index = 0u;
    g_note_started_ms = millis_get();
    timer1_set_frequency(g_active_melody[g_note_index].frequency_hz);
}


/// Stopping buzzer and clearing active melody state
void buzzer_stop(void)
{
    g_mode = BUZZER_MODE_OFF;
    g_active_melody = 0;
    g_active_melody_len = 0u;
    timer1_disable_output();
}


void buzzer_update(void)
/* This must be done so that the buzzer can play two different tunes: alert and background (it uses the same timer)*/
{
    uint32_t now;

    if (g_mode == BUZZER_MODE_OFF || g_active_melody == 0 || g_active_melody_len == 0u) {
        return;// nothing to play
    }

    now = millis_get();
    if ((now - g_note_started_ms) >= g_active_melody[g_note_index].duration_ms) {
        g_note_index++; //move to the next note
        if (g_note_index >= g_active_melody_len) {
            g_note_index = 0u;
        }//start the next note
        g_note_started_ms = now;
        timer1_set_frequency(g_active_melody[g_note_index].frequency_hz);
    }
}

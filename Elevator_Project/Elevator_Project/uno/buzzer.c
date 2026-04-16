#define F_CPU 16000000UL

#include "buzzer.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define BUZZER_DDR DDRB
#define BUZZER_PORT PORTB
#define BUZZER_PIN PB1   /* Arduino UNO D9 / OC1A */

typedef struct note {
    uint16_t frequency;
    uint16_t duration_ms;
} note_t;

static const note_t alert_melody[] = {
    {523, 180},
    {659, 180},
    {784, 180},
    {659, 180},
    {988, 260},
    {0,   120}
};

static volatile uint32_t g_millis = 0;
static bool g_buzzer_enabled = false;
static uint8_t g_note_index = 0;
static uint32_t g_note_started_ms = 0;

ISR(TIMER0_COMPA_vect)
{
    g_millis++;
}

void timer0_tick_init(void)
{
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00); /* prescaler 64 */
    OCR0A = 249;                        /* 1 ms tick */
    TIMSK0 = (1 << OCIE0A);
}

uint32_t millis_get(void)
{
    uint32_t value;
    uint8_t sreg = SREG;
    cli();
    value = g_millis;
    SREG = sreg;
    return value;
}

static void timer1_disable_output(void)
{
    TCCR1A &= (uint8_t)~((1 << COM1A1) | (1 << COM1A0));
    BUZZER_PORT &= (uint8_t)~(1 << BUZZER_PIN);
}

static void timer1_set_frequency(uint16_t frequency)
{
    uint32_t top;

    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;

    if (frequency == 0u) {
        timer1_disable_output();
        return;
    }

    /* CTC mode, toggle OC1A on compare match, prescaler 8 */
    top = (F_CPU / (2UL * 8UL * frequency)) - 1UL;
    if (top > 65535UL) {
        top = 65535UL;
    }

    OCR1A = (uint16_t)top;
    TCCR1A = (1 << COM1A0);
    TCCR1B = (1 << WGM12) | (1 << CS11);
}

void buzzer_init(void)
{
    BUZZER_DDR |= (1 << BUZZER_PIN);
    timer1_disable_output();
}

void buzzer_start_alert(void)
{
    g_buzzer_enabled = true;
    g_note_index = 0u;
    g_note_started_ms = millis_get();
    timer1_set_frequency(alert_melody[g_note_index].frequency);
}

void buzzer_stop(void)
{
    g_buzzer_enabled = false;
    timer1_disable_output();
}

void buzzer_update(void)
{
    uint32_t now;

    if (!g_buzzer_enabled) {
        return;
    }

    now = millis_get();
    if ((now - g_note_started_ms) >= alert_melody[g_note_index].duration_ms) {
        g_note_index++;
        if (g_note_index >= (sizeof(alert_melody) / sizeof(alert_melody[0]))) {
            g_note_index = 0u;
        }
        g_note_started_ms = now;
        timer1_set_frequency(alert_melody[g_note_index].frequency);
    }
}

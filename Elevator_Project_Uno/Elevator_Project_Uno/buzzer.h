#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

/// Initializes timer0 as 1 ms system tick used by melody sequencing.
void timer0_tick_init(void);

/// Returns elapsed milliseconds from timer0 tick counter.
uint32_t millis_get(void);

/// Initializes buzzer output pin and timer1 output state.
void buzzer_init(void);

/* Buzzer control */
void buzzer_start_background(void);
void buzzer_start_alert(void);
void buzzer_stop(void);

/// Advances active melody when current note duration has elapsed.
void buzzer_update(void);

#endif

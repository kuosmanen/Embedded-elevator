#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void timer0_tick_init(void);
uint32_t millis_get(void);
void buzzer_init(void);
void buzzer_start_alert(void);
void buzzer_stop(void);
void buzzer_update(void);

#endif

#ifndef TUNE_H
#define TUNE_H

#include <stdint.h>

#define BPM (90)
#define FULL (60000/BPM)
#define HALF (FULL/2)
#define QUARTER (FULL/4)

#define C3 262
#define D3 293
#define E3 330
#define F3 349
#define G3 392
#define A3 440
#define B3 494
#define C4 523
#define A4 440
#define B4 494
#define C5 523
#define D5 587
#define E5 659

/// note representation shared across buzzer melodies
typedef struct note {
	uint16_t frequency_hz;
	uint16_t duration_ms;
} note_t;

/* Tetris "inspired" (Korobeiniki 1898) 8-bit style loop for background sound */
static const note_t UNO_BACKGROUND_TUNE[] = {
	{ E5, 160 },
	{ B4, 160 },
	{ C5, 160 },
	{ D5, 160 },
	{ C5, 160 },
	{ B4, 160 },
	{ A4, 160 },
	{ A4, 160 },
	{ C5, 160 },
	{ E5, 160 },
	{ D5, 160 },
	{ C5, 160 },
	{ B4, 160 },
	{ C5, 160 },
	{ D5, 160 },
	{ E5, 160 },
	{ C5, 160 },
	{ A4, 160 },
	{ A4, 320 },
	{ 0,  120 },
};

/// Number of notes in background melody table
#define UNO_BACKGROUND_TUNE_LENGTH ((uint8_t)(sizeof(UNO_BACKGROUND_TUNE) / sizeof(UNO_BACKGROUND_TUNE[0])))

/* Obstacle alert melody reused by buzzer_start_alert() */
static const note_t UNO_ALERT_TUNE[] = {
	{ 523, 180 },
	{ 659, 180 },
	{ 784, 180 },
	{ 659, 180 },
	{ 988, 260 },
	{ 0,   120 },
};

///Number of notes in obstacle alert melody table
#define UNO_ALERT_TUNE_LENGTH ((uint8_t)(sizeof(UNO_ALERT_TUNE) / sizeof(UNO_ALERT_TUNE[0])))
#endif

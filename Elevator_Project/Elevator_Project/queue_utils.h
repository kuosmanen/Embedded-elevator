#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FLOOR_QUEUE_SIZE 10

typedef struct floor_queue {
	uint8_t data[FLOOR_QUEUE_SIZE];
	uint8_t head;
	uint8_t tail;
	uint8_t count;
} floor_queue_t;

void queue_init(floor_queue_t *queue);
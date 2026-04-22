#ifndef QUEUE_UTILS_H
#define QUEUE_UTILS_H

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

bool queue_push(floor_queue_t *queue, uint8_t floor);
bool queue_pop(floor_queue_t *queue, uint8_t *floor);
bool queue_is_empty(const floor_queue_t *queue);

#endif
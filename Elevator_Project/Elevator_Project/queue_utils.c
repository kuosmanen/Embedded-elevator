#include "queue_utils.h"

void queue_init(floor_queue_t *queue)
{
	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
}

bool queue_push(floor_queue_t *queue, uint8_t floor)
{
    if (queue->count >= FLOOR_QUEUE_SIZE) {
        return false;
    }

    queue->data[queue->tail] = floor;
    queue->tail = (uint8_t)((queue->tail + 1u) % FLOOR_QUEUE_SIZE);
    queue->count++;
    return true;
}

bool queue_pop(floor_queue_t *queue, uint8_t *floor)
{
    if (queue->count == 0u) {
        return false;
    }

    *floor = queue->data[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % FLOOR_QUEUE_SIZE);
    queue->count--;
    return true;
}

bool queue_is_empty(const floor_queue_t *queue)
{
    return (queue->count == 0u);
}
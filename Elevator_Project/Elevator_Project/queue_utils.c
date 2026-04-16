#include "queue_utils.h"

void queue_init(floor_queue_t *queue)
{
	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
}
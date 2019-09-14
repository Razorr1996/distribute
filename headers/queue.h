#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdio.h>
#include "ipc.h"

typedef struct Queue {
	struct Queue *front, *back, *next;
	local_id pid;
	timestamp_t time;
} Queue;

void queue_init( Queue **queue );
void queue_push( Queue *queue, local_id pid, timestamp_t time );
void queue_pop( Queue *queue );
void queue_free( Queue *queue );

#endif

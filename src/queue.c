#include <stdlib.h>
#include <assert.h>
#include "queue.h"

#define TRY_ALLOC(node) {               \
    node = malloc(sizeof *node );    \
}

void queue_init(Queue **queue) {
    *queue = calloc(1, sizeof **queue);
}

void queue_push(Queue *queue, local_id pid, timestamp_t time) {
    assert(queue != NULL);

    Queue *node;
    TRY_ALLOC(node);
    node->pid = pid;
    node->time = time;
    node->next = NULL;

    if (queue->front == NULL && queue->back == NULL) {
        queue->front = queue->back = node;
        return;
    }

    Queue *current = queue->front;
    Queue *prev = NULL;

    while (current != NULL) {
        if (current->time > time || (current->time == time && pid < current->pid)) {

            node->next = current;
            if (prev) prev->next = node;

            if (current == queue->front) queue->front = node;

            node = NULL;
            break;
        } else {
            prev = current;
            current = current->next;
        }
    }

    if (node) {
        queue->back->next = node;
        queue->back = node;
        node = NULL;
    }
}

void queue_pop(Queue *queue) {
    assert(queue != NULL);
    if (queue->front == NULL) return;
    Queue *del = queue->front;
    if (queue->front == queue->back) {
        queue->front = NULL;
        queue->back = NULL;
    } else {
        queue->front = queue->front->next;
    }
    free(del);
}

void queue_free(Queue *queue) {
    assert(queue != NULL);
    Queue *node = queue->front;
    while (node != NULL) {
        Queue *next = node->next;
        free(node);
        node = next;
    }
    queue->front = NULL;
    queue->back = NULL;
    free(queue);
}

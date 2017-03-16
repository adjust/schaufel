#ifndef _SCHAUFEL_QUEUE_H_
#define _SCHAUFEL_QUEUE_H_

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct Message *Message;

Message message_init();
void * message_data(Message msg);
void message_free(Message *msg);

typedef struct Queue *Queue;

Queue queue_init();
int queue_add(Queue queue, void *data, long msgtype);
int queue_get(Queue queue, Message msg);
long queue_length(Queue queue);
int queue_free(Queue *queue);

#endif
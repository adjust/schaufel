#ifndef _SCHAUFEL_QUEUE_H_
#define _SCHAUFEL_QUEUE_H_

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_QUEUE_SIZE 100000

typedef struct Message *Message;

Message message_init();
void   *message_get_data(Message msg);
void    message_set_data(Message msg, void *data);
void    message_free(Message *msg);

typedef struct Queue *Queue;

Queue queue_init();
int  queue_add(Queue q, void *data, long msgtype);
int  queue_get(Queue q, Message msg);
long queue_length(Queue q);
int  queue_free(Queue *q);

#endif

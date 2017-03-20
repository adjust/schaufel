#ifndef _SCHAUFEL_PRODUCER_H_
#define _SCHAUFEL_PRODUCER_H_

#include <dummy.h>
#include <producer.h>
#include <queue.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct Producer *Producer;

typedef struct Producer {
    void *meta;
    void (*produce) (Producer p, Message msg);
    void (*producer_free)(Producer *p);
} *Producer;

Producer producer_init(char kind);

void producer_free(Producer *p);

void producer_produce(Producer p, Message msg);

#endif

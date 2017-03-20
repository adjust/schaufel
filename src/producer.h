#ifndef _SCHAUFEL_PRODUCER_H_
#define _SCHAUFEL_PRODUCER_H_

#include <dummy.h>
#include <queue.h>

typedef struct Producer *Producer;

typedef struct Producer {
    void (*produce) (Producer p, Message msg);
    void (*producer_free)(Producer *p);
    void *meta;
} *Producer;

Producer producer_init(char kind, void *opt);

void producer_free(Producer *p);

void producer_produce(Producer p, Message msg);

#endif

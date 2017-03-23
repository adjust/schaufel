#ifndef _SCHAUFEL_CONSUMER_H_
#define _SCHAUFEL_CONSUMER_H_

#include <dummy.h>
#include <file.h>
#include <kafka.h>
#include <queue.h>
#include <redis.h>
#include <schaufel.h>

typedef struct Consumer *Consumer;

typedef struct Consumer{
    void (*consume) (Consumer c, Message msg);
    void (*consumer_free) (Consumer *c);
    void *meta;
}*Consumer;

Consumer consumer_init(char kind, void *opt);

void consumer_free(Consumer *c);

void consumer_consume(Consumer c, Message msg);

#endif

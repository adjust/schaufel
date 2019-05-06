#ifndef _SCHAUFEL_CONSUMER_H_
#define _SCHAUFEL_CONSUMER_H_

#include <dummy.h>
#include <file.h>
#include <kafka.h>
#include <queue.h>
#include <redis.h>
#include <utils/helper.h>
#include <utils/scalloc.h>

typedef struct Consumer *Consumer;

typedef struct Consumer{
    int  (*consume) (Consumer c, Message msg);
    void (*consumer_free) (Consumer *c);
    void *meta;
}*Consumer;

Consumer consumer_init(char kind, config_setting_t *config);

void consumer_free(Consumer *c);

int consumer_consume(Consumer c, Message msg);

#endif

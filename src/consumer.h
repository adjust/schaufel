#ifndef _SCHAUFEL_CONSUMER_H_
#define _SCHAUFEL_CONSUMER_H_

#include <libconfig.h>

#include "queue.h"
#include "hooks.h"


typedef struct Consumer *Consumer;

typedef struct Consumer{
    int  (*consume) (Consumer c, Message msg);
    void (*consumer_free) (Consumer *c);
    void *meta;
    Hooklist preadd;
}*Consumer;

Consumer consumer_init(char kind, config_setting_t *config);

void consumer_free(Consumer *c);

int consumer_consume(Consumer c, Message msg);

#endif

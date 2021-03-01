#ifndef _SCHAUFEL_PRODUCER_H_
#define _SCHAUFEL_PRODUCER_H_

#include <libconfig.h>

#include "queue.h"
#include "hooks.h"


typedef struct Producer *Producer;

struct Producer {
    void (*produce) (Producer p, Message msg);
    void (*producer_free)(Producer *p);
    void *meta;
    Hooklist postget;
};

Producer producer_init(char kind, config_setting_t *config);

void producer_free(Producer *p);

void producer_produce(Producer p, Message msg);

#endif

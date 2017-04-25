#ifndef _SCHAUFEL_REDIS_H_
#define _SCHAUFEL_REDIS_H_

#include <consumer.h>
#include <hiredis/hiredis.h>
#include <utils/logger.h>
#include <producer.h>
#include <stdlib.h>

typedef struct Producer *Producer;

Producer redis_producer_init(char *host, char *topic);

void redis_producer_free(Producer *p);

void redis_producer_produce(Producer p, Message msg);

typedef struct Consumer *Consumer;

Consumer redis_consumer_init(char *host, char *topic);

void redis_consumer_free(Consumer *c);

int redis_consumer_consume(Consumer c, Message msg);

#endif

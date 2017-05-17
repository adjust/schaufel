#ifndef _SCHAUFEL_POSTGRES_H_
#define _SCHAUFEL_POSTGRES_H_

#include <consumer.h>
#include <utils/logger.h>
#include <producer.h>
#include <stdlib.h>
#include <libpq-fe.h>

typedef struct Producer *Producer;

Producer postgres_producer_init(char *host, char *host_replica);

void postgres_producer_free(Producer *p);

void postgres_producer_produce(Producer p, Message msg);

typedef struct Consumer *Consumer;

Consumer postgres_consumer_init(char *host);

void postgres_consumer_free(Consumer *c);

int postgres_consumer_consume(Consumer c, Message msg);

#endif

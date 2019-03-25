#ifndef _SCHAUFEL_POSTGRES_H_
#define _SCHAUFEL_POSTGRES_H_

#include <consumer.h>
#include <queue.h>
#include <utils/logger.h>
#include <producer.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <queue.h>
#include <validator.h>

typedef struct Producer *Producer;

Producer postgres_producer_init(config_setting_t *config);

void postgres_producer_free(Producer *p);

void postgres_producer_produce(Producer p, Message msg);

typedef struct Consumer *Consumer;

Consumer postgres_consumer_init(char *host);

void postgres_consumer_free(Consumer *c);

int postgres_consumer_consume(Consumer c, Message msg);

typedef struct Validator *Validator;

Validator postgres_validator_init();

#endif

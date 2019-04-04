#ifndef _SCHAUFEL_EXPORTS_H_
#define _SCHAUFEL_EXPORTS_H_

#include <consumer.h>
#include <utils/logger.h>
#include <producer.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <json-c/json.h>
#include <queue.h>
#include <sys/prctl.h>

typedef struct Producer *Producer;

Producer exports_producer_init(config_setting_t *config);

void exports_producer_free(Producer *p);

void exports_producer_produce(Producer p, Message msg);

typedef struct Consumer *Consumer;

Consumer exports_consumer_init(config_setting_t *config);

void exports_consumer_free(Consumer *c);

int exports_consumer_consume(Consumer c, Message msg);

typedef struct Validator *Validator;

Validator exports_validator_init();

#endif

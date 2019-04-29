#ifndef _SCHAUFEL_POSTGRES_H_
#define _SCHAUFEL_POSTGRES_H_

#include "consumer.h"
#include "producer.h"
#include "queue.h"
#include "validator.h"


Producer postgres_producer_init(config_setting_t *config);

void postgres_producer_free(Producer *p);

void postgres_producer_produce(Producer p, Message msg);

Consumer postgres_consumer_init(char *host);

void postgres_consumer_free(Consumer *c);

int postgres_consumer_consume(Consumer c, Message msg);

Validator postgres_validator_init();

#endif
